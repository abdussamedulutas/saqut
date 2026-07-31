// ============================================================================
// saQut IR — CFG Builder Gerçeklemesi
// ============================================================================
//
// #218, ADR-039
// Flat instruction list → CFG dönüşümü.
//
// Algoritma:
//   1. Blok liderlerini bul (instruction 0, jump hedefleri, terminator sonrası)
//   2. Liderler arasını BasicBlock'lara böl
//   3. Block'lar arası edge'leri kur (predecessor/successor)
//
// ============================================================================

#include "ir/ir_cfg.hpp"
#include <algorithm>
#include <set>

static bool isTerminator(Opcode op) {
    return op == Opcode::RETURN || op == Opcode::THROW;
}

static bool isJump(Opcode op) {
    return op == Opcode::JMP || op == Opcode::JIF_FALSE || op == Opcode::JIF_TRUE;
}

// Blok liderlerini bul
static std::vector<int> findLeaders(const std::vector<Instruction>& insns) {
    std::set<int> leaderSet;
    if (insns.empty()) return {};

    leaderSet.insert(0);

    for (int i = 0; i < (int)insns.size(); ++i) {
        const auto& ins = insns[i];

        if (isTerminator(ins.opcode)) {
            if (i + 1 < (int)insns.size())
                leaderSet.insert(i + 1);
        }

        if (isJump(ins.opcode) && ins.jumpTarget >= 0) {
            leaderSet.insert(ins.jumpTarget);
        }

        if (ins.opcode == Opcode::JIF_FALSE || ins.opcode == Opcode::JIF_TRUE) {
            if (i + 1 < (int)insns.size())
                leaderSet.insert(i + 1);
        }
    }

    return std::vector<int>(leaderSet.begin(), leaderSet.end());
}

CFG buildCFG(const std::vector<Instruction>& instructions) {
    CFG cfg;
    if (instructions.empty()) return cfg;

    auto leaders = findLeaders(instructions);

    // Bloklara böl
    for (size_t li = 0; li < leaders.size(); ++li) {
        BasicBlock block;
        block.id = (int)cfg.blocks.size();
        block.startIndex = leaders[li];
        block.endIndex = (li + 1 < leaders.size()) ? leaders[li + 1] : (int)instructions.size();

        for (int i = block.startIndex; i < block.endIndex; ++i)
            block.instructions.push_back(instructions[i]);

        if (!block.instructions.empty()) {
            const auto& last = block.instructions.back();
            block.terminator = last.opcode;
            block.jumpTarget = last.jumpTarget;
        }

        cfg.blocks.push_back(std::move(block));
    }

    // Edge'leri kur. jumpTarget anlamı: buildCFG girdisinde TALİMAT İNDEKSİ,
    // çözümlendikten sonra BLOCK ID (block.jumpTarget). linearize() blok ID'yi
    // talimat indeksine geri çevirir; dump() "->BB_N" doğru blok ID basar.
    // (#218; düzeltme: --cfg görüntüleyici öncesi anlam karışıklığı kapatıldı)
    auto resolveTarget = [&cfg](BasicBlock& block, int instrIdx) {
        for (auto& target : cfg.blocks) {
            if (target.startIndex == instrIdx) {
                block.successors.push_back(target.id);
                target.predecessors.push_back(block.id);
                block.jumpTarget = target.id;
                return;
            }
        }
    };
    for (auto& block : cfg.blocks) {
        if (block.terminator == Opcode::JMP) {
            resolveTarget(block, block.jumpTarget);
        } else if (block.terminator == Opcode::JIF_FALSE ||
                   block.terminator == Opcode::JIF_TRUE) {
            // Fall-through successor
            if (block.id + 1 < (int)cfg.blocks.size()) {
                block.successors.push_back(block.id + 1);
                cfg.blocks[block.id + 1].predecessors.push_back(block.id);
            }
            // Jump successor
            resolveTarget(block, block.jumpTarget);
        } else if (block.terminator != Opcode::RETURN &&
                   block.terminator != Opcode::THROW) {
            // Implicit fall-through: sıradan talimatla biten blok (ör. init
            // bloğu) sonraki bloğa akar — eksik kenar hatası kapatıldı.
            if (block.id + 1 < (int)cfg.blocks.size()) {
                block.successors.push_back(block.id + 1);
                cfg.blocks[block.id + 1].predecessors.push_back(block.id);
            }
        }
    }

    return cfg;
}
