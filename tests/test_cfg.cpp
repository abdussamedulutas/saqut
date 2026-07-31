// #218 — CFG birim testleri (çerçevesiz; assert + çıktı).
// Koşmak için: tests/run.sh (ir_cfg.cpp ile birlikte derlenir)
//
// Hata SINIFI: buildCFG kenar kurma + linearize round-trip doğruluğu.
//   - Sıradan talimatla biten blokların implicit fall-through kenarı olmalı
//   - block.jumpTarget = BLOCK ID (talimat indeksi değil)
//   - linearize(buildCFG(flat)) orijinal flat listeyi birebir üretmeli
//     (JMP geri kenarları dahil — bozulma #218 regresyonuydu)
#include "ir/ir_cfg.hpp"
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

    std::cout << "test_cfg: TUM TESTLER GECTI\n";
    return 0;
}
