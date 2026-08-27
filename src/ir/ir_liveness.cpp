// ============================================================================
// saQut IR — Slot Liveness Gerçeklemesi
// ============================================================================
//
// Blok gen/kill + geriye fixpoint, ardından blok içi geriye yürüyüş.
// def/use türetimi OPCODE_LIST semantiğinden (instruction.hpp başlık
// yorumundaki operand haritası) tek yerde türetilir — başka yerde elle
// tablo tutulmaz (#132 tek-kaynak ilkesi).
// ============================================================================

#include "ir/ir_liveness.hpp"
#include <cstdint>

namespace {

// Talimatın hangi slotu YAZDIĞI (-1 = yok). ENTER_TRY'nin dest'i (error
// slotu) talimat anında yazılmaz — yalnızca unwinding'de dolar; def sayılmaz
// (analiz zaten ENTER_TRY görünce muhafazakârdır). FIELD_SET/ARRAY_SET'te
// dest bir OKUMADIR (struct/array referansı), yazılan heap'tedir.
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

// Talimatın OKUDUĞU slot'lar. dest'i okuma olan opcode'lar (FIELD_SET/
// ARRAY_SET) burada eklenir. CALLHOST yalnızca argSlots okur (sonuç yok);
// CALL dest'e yazar, argSlots + arg diye okur… CALL'un arg'ları argSlots'ta.
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

SlotLiveness computeSlotLiveness(const IRFunction& fn) {
    SlotLiveness result;
    result.slotCount = fn.slotCount;

    // Güvenlik sınırı: ENTER_TRY → muhafazakâr (bkz. başlık yorumu).
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

    // Blok başına gen (kill'den önce okunan) / kill (tanımlanan).
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
    // Parametreler giriş bloğunun gen'indedir: çağıran yazdı, frame'de durur.
    for (int p = 0; p < fn.paramCount && p < nSlots; ++p)
        gen[0][(size_t)p] = 1;

    // Geriye fixpoint: liveIn = gen ∪ (liveOut \ kill); liveOut = ∪ succ liveIn.
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

    // Talimat bazına indirgeme: blok sonunda liveOut'tan geriye yürü.
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
