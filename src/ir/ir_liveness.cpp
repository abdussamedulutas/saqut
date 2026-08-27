// ============================================================================
// saQut IR — Slot Liveness Gerçeklemesi
// ============================================================================
//
// Her blok için "yazılmadan okunanlar / yazılanlar" özeti, sonra son
// talimattan ilkine geri yürüyüşle talimat bazlı canlılık (ayrıntı:
// computeSlotLiveness üstündeki yorum). Hangi talimat hangi slotu okur/
// yazar bilgisi talimat tanımından (instruction.hpp) tek yerde türetilir —
// başka yerde elle tablo tutulmaz (#132 tek-kaynak ilkesi).
// ============================================================================

#include "ir/ir_liveness.hpp"
#include <cstdint>

namespace {

// Talimatın hangi slotu YAZDIĞI (-1 = yok). Yazma kuralı talimat türüne
// göre değişir ve bu fonksiyon tek kural kaynağıdır:
//   - Çoğu talimat "dest" slotuna yazar (LOAD_CONST, ADD, CALL, ...).
//   - İSTİSNALAR:
//     * FIELD_SET / ARRAY_SET: dest burada YAZILAN değil OKUNAN yerdir
//       (ör. "liste[0] = x" talimatı liste NESNESİNİ değiştirir, slotun
//       kendisindeki referansı değil).
//     * STORE_GLOBAL: slot'a değil global alana yazar (okuduğu src'dir).
//     * ENTER_TRY: dest hata slotudur; yalnızca hata fırlarsa dolar —
//       normal akışta yazmaz (bu fonksiyonda ENTER_TRY görünce analiz
//       zaten muhafazakâr moda geçer, aşağıya bakın).
//     * JMP/JIF/RETURN/THROW/CALLHOST/LEAVE_TRY: hiçbir slota yazmaz.
int defSlot(const Instruction& ins) {
    switch (ins.opcode) {
        case Opcode::FIELD_SET:
        case Opcode::ARRAY_SET:
        case Opcode::STORE_GLOBAL:
        case Opcode::ENTER_TRY:
        case Opcode::LEAVE_TRY:
        case Opcode::JMP:
        case Opcode::JIF_FALSE:
        case Opcode::JIF_TRUE:
        case Opcode::RETURN:
        case Opcode::THROW:
        case Opcode::CALLHOST:
            return -1;
        default:
            return ins.dest;
    }
}

// Talimatın OKUDUĞU slot'lar (varsa). src/left/right/cond alanları ve çağrı
// argümanları (argSlots) okumadır; ayrıca FIELD_SET/ARRAY_SET'in dest'i de
// okumadır (yukarıdaki istisna açıklaması).
void useSlots(const Instruction& ins, std::vector<int>& out) {
    auto add = [&](int s) { if (s >= 0) out.push_back(s); };
    add(ins.src);
    add(ins.left);
    add(ins.right);
    add(ins.cond);
    switch (ins.opcode) {
        case Opcode::FIELD_SET:  // slots[dest].fields[i] = slots[right]
        case Opcode::ARRAY_SET:  // slots[dest][slots[left]] = slots[right]
            add(ins.dest);
            break;
        default:
            break;
    }
    for (int a : ins.argSlots) add(a);
}

} // namespace

// NE YAPIYOR: "Bir slot, programın herhangi bir anında CANLI mı?" sorusunu
// cevaplar. Canlı = değer ileride en az bir kez daha OKUNACAK. Son okunmasının
// ardından slot ölür: değeri ne olursa olsun programın davranışı değişmez.
// Bu bilgi çöp toplayıcının köklerini daraltmak için kullanılır: ölü slot'a
// bağlı nesne, kimse kullanmayacağı için güvenle toplanabilir.
//
// YÖNTEM — üç aşama:
//   1. BLOK ÖZETİ: her blok için tek geçişle "blok içinde yazılmadan önce
//      okunanlar" (gen) ve "blok içinde yazılanlar" (kill) listelenir.
//   2. SABİT NOKTA: blokların SONUNDAN BAŞINA doğru bilgi akıtılır.
//      Kural: bloğun çıkışında canlı olanlar = gittiği blokların girişinde
//      canlı olanların birleşimi; bloğun girişinde canlı olanlar = bloğun
//      okuyacakları + (çıkışta canlı olup blokta yeniden yazılmayanlar).
//      Bloklar tersten gezilir; hiçbir sonuç değişmeyince hesap biter.
//      (Döngülerde bilgi döngü başına geri taşıdığı için birkaç tur gerekebilir.)
//   3. TALIMAT BAZINA İNDİRGEME: artık her bloğun çıkışındaki canlılar
//      kesindir; blok içinde SON talimattan İLKİNE doğru geri yürünür:
//      her talimat önce yazdığı slotu öldürür, sonra okuduğunu canlandırır.
//      Her adımda eldeki küme = "bu talimat çalışmadan önce canlı olanlar".
//
// GÜVENLİK SINIRI: ENTER_TRY içeren fonksiyonlar için analiz muhafazakârdır
// (her slot her yerde canlı kabul edilir). Neden: try bölgesindeki herhangi
// bir talimat hata fırlatıp catch bloğuna atlayabilir; catch'in hangi
// slotlara ihtiyaç duyduğunu bilmek için try-bölgesi analizi gerekir (henüz
// yok). Yanlış "ölü" işareti canlı nesnenin toplanması demek olduğundan,
// emin olunamayan yerde HER ŞEY canlı kabul edilir — güvenli taraf budur.
SlotLiveness computeSlotLiveness(const IRFunction& fn) {
    SlotLiveness result;
    result.slotCount = fn.slotCount;

    // Güvenlik sınırı: ENTER_TRY → muhafazakâr mod (bkz. üstteki açıklama).
    for (const auto& ins : fn.instructions) {
        if (ins.opcode == Opcode::ENTER_TRY) {
            result.exact = false;
            return result;
        }
    }

    CFG cfg = fn.cfg.blocks.empty() ? buildCFG(fn.instructions) : fn.cfg;

    const size_t nBlocks = cfg.blocks.size();
    const int nSlots = fn.slotCount;
    if (nBlocks == 0) { result.exact = false; return result; }

    // Aşama 1 — blok özeti: "yazılmadan okunanlar" (gen) ve "yazılanlar" (kill).
    std::vector<std::vector<char>> gen(nBlocks, std::vector<char>((size_t)nSlots, 0));
    std::vector<std::vector<char>> kill(nBlocks, std::vector<char>((size_t)nSlots, 0));
    for (size_t b = 0; b < nBlocks; ++b) {
        for (const auto& ins : cfg.blocks[b].instructions) {
            std::vector<int> uses;
            useSlots(ins, uses);
            int d = defSlot(ins);
            for (int u : uses)
                if (!kill[b][(size_t)u]) gen[b][(size_t)u] = 1;
            if (d >= 0) kill[b][(size_t)d] = 1;
        }
    }
    // Parametreler fonksiyon girişinde "okunmayı bekleyen" değerlerdir —
    // onları çağıran yazmıştır, frame'de dururlar. Bu yüzden giriş bloğunun
    // gen kümesine eklenirler.
    for (int p = 0; p < fn.paramCount && p < nSlots; ++p)
        gen[0][(size_t)p] = 1;

    // Aşama 2 — sabit nokta: blokları sondan başa gez, "girişte canlılar =
    // okuyacaklarım + (çıkışta canlı olup yeniden yazmadıklarım)" kuralını
    // hiçbir şey değişmeyene dek uygula. liveOut, gittiği blokların
    // liveIn'lerinin birleşimi olarak birikir.
    std::vector<std::vector<char>> liveIn(nBlocks, std::vector<char>((size_t)nSlots, 0));
    std::vector<std::vector<char>> liveOut(nBlocks, std::vector<char>((size_t)nSlots, 0));
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t bi = nBlocks; bi-- > 0;) {
            for (int s : cfg.blocks[bi].successors)
                for (int k = 0; k < nSlots; ++k)
                    if (liveIn[(size_t)s][(size_t)k]) liveOut[bi][(size_t)k] = 1;
            for (int k = 0; k < nSlots; ++k) {
                char v = gen[bi][(size_t)k] ||
                         (liveOut[bi][(size_t)k] && !kill[bi][(size_t)k]);
                if (v && !liveIn[bi][(size_t)k]) {
                    liveIn[bi][(size_t)k] = 1;
                    changed = true;
                }
            }
        }
    }

    // Aşama 3 — talimat bazına indirgeme: her bloğun çıkışındaki canlı
    // kümesinden başla, SON talimattan ilkine geri yürü: talimat önce
    // yazdığı slotu öldürür, sonra okuduklarını canlandırır.
    result.liveBefore.assign(fn.instructions.size(),
                             std::vector<char>((size_t)nSlots, 0));
    std::vector<int> flatStart(nBlocks + 1, 0);
    for (size_t b = 0; b < nBlocks; ++b)
        flatStart[b + 1] = flatStart[b] + (int)cfg.blocks[b].instructions.size();

    for (size_t b = 0; b < nBlocks; ++b) {
        std::vector<char> live = liveOut[b];
        const auto& insns = cfg.blocks[b].instructions;
        for (size_t i = insns.size(); i-- > 0;) {
            int d = defSlot(insns[i]);
            if (d >= 0) live[(size_t)d] = 0;
            std::vector<int> uses;
            useSlots(insns[i], uses);
            for (int u : uses) live[(size_t)u] = 1;
            result.liveBefore[(size_t)(flatStart[b] + (int)i)] = live;
        }
    }

    return result;
}
