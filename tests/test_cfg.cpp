// #218 — CFG birim testleri (çerçevesiz; assert + çıktı).
// Koşmak için: tests/run.sh (ir_cfg.cpp ile birlikte derlenir)
//
// Hata SINIFI: buildCFG kenar kurma + linearize round-trip doğruluğu.
//   - Sıradan talimatlarla biten blokların implicit fall-through kenarı olmalı
//   - block.jumpTarget = BLOCK ID (talimat indeksi değil)
//   - linearize(buildCFG(flat)) orijinal flat listeyi birebir üretmeli
//     (JMP geri kenarları dahil — bozulma #218 regresyonuydu)
//   - JMP sonrası ölü kod AYRI bloğa düşmeli ve removeUnreachableBlocks
//     tarafından silinmeli
//   - ENTER_TRY exception kenarı catch bloğunu erişilebilir yapmalı
//   - dominance + natural loop tespiti (CHK) döngüde doğru yapı vermeli
//   - slot liveness: son kullanımı geçen slot ölü; döngüde kullanılan slot
//     geri kenar boyunca canlı; ENTER_TRY → muhafazakâr (exact=false)
#include "ir/ir_cfg.hpp"
#include "ir/ir_liveness.hpp"
#include <cassert>
#include <iostream>

static Instruction jump(Opcode op, int target) {
    Instruction ins(op);
    ins.jumpTarget = target;
    return ins;
}

// fibonacciIterative desenli döngü:
//   0..2  init (düz talimatlar)
//   3     LESS
//   4     JIF_FALSE -> 12
//   5..10 gövde
//   11    JMP -> 3   (geri kenar)
//   12    RETURN
static std::vector<Instruction> makeLoop() {
    std::vector<Instruction> insns;
    for (int i = 0; i < 3; ++i) insns.emplace_back(Opcode::LOAD_CONST);
    insns.push_back(Instruction(Opcode::LESS));
    insns.push_back(jump(Opcode::JIF_FALSE, 12));
    for (int i = 0; i < 6; ++i) insns.push_back(Instruction(Opcode::LOAD_SLOT));
    insns.push_back(jump(Opcode::JMP, 3));
    insns.push_back(Instruction(Opcode::RETURN));
    return insns;
}

// İki flat listenin opcode+dest+jumpTarget açısından birebir eşitliği
static bool sameIR(const std::vector<Instruction>& a, const std::vector<Instruction>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].opcode != b[i].opcode) return false;
        if (a[i].dest != b[i].dest) return false;
        if (a[i].jumpTarget != b[i].jumpTarget) return false;
    }
    return true;
}

int main() {
    // 1) Düz hat (tek blok, kenar yok)
    {
        std::vector<Instruction> insns;
        insns.emplace_back(Opcode::LOAD_CONST);
        insns.emplace_back(Opcode::RETURN);
        CFG cfg = buildCFG(insns);
        assert(cfg.blocks.size() == 1);
        assert(cfg.blocks[0].successors.empty());
        assert(cfg.blocks[0].predecessors.empty());
        assert(cfg.blocks[0].terminator == Opcode::RETURN);
        assert(sameIR(cfg.linearize(), insns));
    }

    // 2) Döngü — blok yapısı, kenarlar, blok ID semantiği
    {
        auto flat = makeLoop();
        CFG cfg = buildCFG(flat);
        assert(cfg.blocks.size() == 4);
        // init bloğu düz talimatla biter → fall-through kenar OLMALI
        assert(cfg.blocks[0].successors.size() == 1);
        assert(cfg.blocks[0].successors[0] == 1);
        assert(cfg.blocks[0].terminator == Opcode::LOAD_CONST);
        // koşul bloğu: fall-through + jump
        assert(cfg.blocks[1].successors.size() == 2);  // BB_2 (fall) + BB_3 (jump)
        assert(cfg.blocks[1].successors[0] == 2);
        assert(cfg.blocks[1].successors[1] == 3);
        assert(cfg.blocks[1].jumpTarget == 3);         // BLOCK ID, talimat indeksi değil!
        // gövde: JMP geri kenar → koşul bloğu (BB_1)
        assert(cfg.blocks[2].terminator == Opcode::JMP);
        assert(cfg.blocks[2].successors.size() == 1);
        assert(cfg.blocks[2].successors[0] == 1);
        assert(cfg.blocks[2].jumpTarget == 1);         // BLOCK ID
        assert(cfg.blocks[2].predecessors.size() == 1);
        assert(cfg.blocks[2].predecessors[0] == 1);
        // return: kenar yok
        assert(cfg.blocks[3].successors.empty());
        assert(cfg.blocks[3].predecessors.size() == 1);
        assert(cfg.blocks[3].predecessors[0] == 1);

        // 3) linearize round-trip — JMP geri kenarı dahil birebir
        assert(sameIR(cfg.linearize(), flat));
        // spot: JMP hâlâ orijinal talimat indeksine (3) gidiyor
        auto lin = cfg.linearize();
        assert(lin[11].opcode == Opcode::JMP);
        assert(lin[11].jumpTarget == 3);
        assert(lin[4].opcode == Opcode::JIF_FALSE);
        assert(lin[4].jumpTarget == 12);
    }

    // 4) JIF_TRUE de aynı kenar kuralına tabi
    {
        std::vector<Instruction> insns;
        insns.emplace_back(Opcode::LOAD_CONST);
        insns.push_back(jump(Opcode::JIF_TRUE, 3));
        insns.emplace_back(Opcode::LOAD_CONST);
        insns.emplace_back(Opcode::RETURN);
        CFG cfg = buildCFG(insns);
        assert(cfg.blocks.size() == 3);
        assert(cfg.blocks[0].successors.size() == 2);
        assert(cfg.blocks[0].jumpTarget == 2);
        assert(sameIR(cfg.linearize(), insns));
    }

    // 5) JMP sonrası ölü kod: ayrı blok + removeUnreachableBlocks silmeli
    {
        std::vector<Instruction> insns;
        insns.emplace_back(Opcode::LOAD_CONST);          // 0
        insns.emplace_back(Opcode::RETURN);              // 1
        insns.push_back(jump(Opcode::JMP, 2));           // 2 (ölü: RETURN sonrası)
        insns.emplace_back(Opcode::LOAD_CONST);          // 3 (ölü)
        insns.emplace_back(Opcode::RETURN);              // 4 (ölü)
        CFG cfg = buildCFG(insns);
        // RETURN sonrası lider → ölü bölge kendi bloklarında
        int removed = cfg.removeUnreachableBlocks();
        assert(removed == 2);                            // JMP bloğu + ölü RETURN
        auto lin = cfg.linearize();
        assert(lin.size() == 2);                         // yalnızca ilk ikisi
        assert(lin[0].opcode == Opcode::LOAD_CONST);
        assert(lin[1].opcode == Opcode::RETURN);
    }

    // 6) ENTER_TRY exception kenarı: catch bloğu erişilebilir, silinmez;
    //    linearize ENTER_TRY.jumpTarget'ı yeni flat indekse yazar
    {
        std::vector<Instruction> insns;
        Instruction et(Opcode::ENTER_TRY);               // 0: dest=err, jump->3
        et.dest = 1; et.jumpTarget = 3;
        insns.push_back(et);
        insns.emplace_back(Opcode::LEAVE_TRY);           // 1
        insns.emplace_back(Opcode::RETURN);              // 2 (normal çıkış)
        insns.emplace_back(Opcode::LOAD_CONST);          // 3 (catch girişi)
        insns.emplace_back(Opcode::RETURN);              // 4
        CFG cfg = buildCFG(insns);
        assert(cfg.blocks.size() == 2);                 // normal blok + catch bloğu
        assert(cfg.blocks[0].exceptionTargets.size() == 1); // exception kenarı kayıtlı
        assert(cfg.blocks[0].successors.size() == 1);    // RETURN kapatır; tek kenar catch'e
        int removed = cfg.removeUnreachableBlocks();
        assert(removed == 0);                            // catch ERİŞİLEBİLİR
        auto lin = cfg.linearize();
        // ENTER_TRY hedefi, catch bloğunun YENİ flat başlangıcına işaret etmeli
        int etTarget = -1, catchFlatStart = -1;
        for (size_t i = 0; i < lin.size(); ++i)
            if (lin[i].opcode == Opcode::ENTER_TRY) etTarget = lin[i].jumpTarget;
        // catch bloğu: LOAD_CONST ile başlayan blok (tek blok garanti değil;
        // blok sınırlarından hesapla)
        for (size_t b = 0; b < cfg.blocks.size(); ++b) {
            if (!cfg.blocks[b].instructions.empty() &&
                cfg.blocks[b].instructions[0].opcode == Opcode::LOAD_CONST) {
                catchFlatStart = 0;
                for (size_t k = 0; k < b; ++k)
                    catchFlatStart += (int)cfg.blocks[k].instructions.size();
            }
        }
        assert(etTarget == catchFlatStart);
    }

    // 7) Dominance + natural loop: makeLoop üstünde
    {
        CFG cfg = buildCFG(makeLoop());
        cfg.removeUnreachableBlocks();
        cfg.computeDominance();
        // BB_0 init → BB_1 koşul; ikisinin de dominatör zinciri kökten
        assert(cfg.idom[0] == -1);                       // kök
        assert(cfg.idom[1] == 0);                        // koşulu init domine eder
        assert(cfg.idom[3] == 1);                        // return'u koşul domine eder
        assert(cfgDominates(cfg, 1, 3));
        assert(!cfgDominates(cfg, 3, 1));                // tersi değil
        cfg.computeLoops();
        assert(cfg.loops.size() == 1);                   // tek geri kenar (gövde→koşul)
        assert(cfg.loops[0].header == 1);
        // gövde = {BB_1 koşul, BB_2 gövde}
        assert(cfg.loops[0].body.size() == 2);
    }

    // 8) Liveness — düz hat: son kullanımı geçen slot ölür
    {
        IRFunction fn("main", 0);
        // slot0=1; slot1=2; slot2=slot0+slot1; slot3=slot0; return slot3
        std::vector<Instruction> insns;
        Instruction a(Opcode::LOAD_CONST); a.dest = 0; a.intValue = 1;
        insns.push_back(a);
        Instruction b(Opcode::LOAD_CONST); b.dest = 1; b.intValue = 2;
        insns.push_back(b);
        Instruction c(Opcode::ADD); c.dest = 2; c.left = 0; c.right = 1;
        insns.push_back(c);
        Instruction d(Opcode::LOAD_SLOT); d.dest = 3; d.src = 0;
        insns.push_back(d);
        Instruction e(Opcode::RETURN); e.src = 3;
        insns.push_back(e);
        fn.instructions = insns;
        fn.slotCount = 4;
        SlotLiveness lv = computeSlotLiveness(fn);
        assert(lv.exact);
        assert(lv.isLiveBefore(2, 0) && lv.isLiveBefore(2, 1));  // ADD okuyor
        assert(lv.isLiveBefore(3, 0));                            // LOAD_SLOT henüz okuyacak
        assert(!lv.isLiveBefore(4, 0));                           // slot0 artık ölü
        assert(!lv.isLiveBefore(3, 1) && !lv.isLiveBefore(4, 1)); // slot1 ADD'te öldü
        assert(lv.isLiveBefore(4, 3));                            // RETURN okuyor
    }

    // 9) Liveness — döngü: gövdede okunan slot geri kenar boyunca canlı
    {
        auto flat = makeLoop();                    // 0..2 init, 3 LESS, 4 JIF, 5..10 gövde, 11 JMP->3, 12 RETURN
        // slot0 ilk talimatta tanımlanır, gövdede (5. talimat) okunur —
        // makeLoop'un dest/src'siz LOAD_SLOT'ları yerine gerçek operand.
        flat[0].dest = 0;
        flat[5] = Instruction(Opcode::LOAD_SLOT);
        flat[5].dest = 1; flat[5].src = 0;
        IRFunction fn("main", 0);
        fn.instructions = flat;
        fn.slotCount = 2;
        SlotLiveness lv = computeSlotLiveness(fn);
        assert(lv.exact);
        assert(lv.isLiveBefore(3, 0));   // döngü koşulu bloğunda: gövde okuyacak
        assert(lv.isLiveBefore(11, 0));  // JMP'te bile: geri kenar → tekrar okunacak
        assert(!lv.isLiveBefore(0, 0));  // kendi tanımından önce canlı değil
    }

    // 10) Liveness — ENTER_TRY: muhafazakâr (exact=false), her slot canlı
    {
        IRFunction fn("main", 0);
        Instruction et(Opcode::ENTER_TRY);
        et.dest = 1; et.jumpTarget = 2;
        fn.instructions.push_back(et);
        fn.instructions.emplace_back(Opcode::LEAVE_TRY);
        fn.instructions.emplace_back(Opcode::RETURN);
        fn.slotCount = 2;
        SlotLiveness lv = computeSlotLiveness(fn);
        assert(!lv.exact);
        assert(lv.isLiveBefore(0, 0) && lv.isLiveBefore(2, 1));  // hepsi canlı
    }

    std::cout << "test_cfg: TUM TESTLER GECTI\n";
    return 0;
}
