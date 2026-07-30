// ============================================================================
// saQut IR — Control Flow Graph (CFG) + BasicBlock
// ============================================================================
//
// DİZİN:   src/ir/ir_cfg.hpp
// KATMAN:  IR — CFG, optimizasyon pass'leri ve backend linearizasyonu
//
// AMAÇ:
//   Flat instruction list → CFG → optimizasyon → flat (linearize).
//   VM ve backend'ler CFG'yi GÖRMEZ — yalnızca linearize edilmiş flat
//   instruction list'ini alır. (#218, ADR-039)
//
// ============================================================================

#ifndef SAQUT_IR_CFG
#define SAQUT_IR_CFG

#include <vector>
#include <string>
#include <sstream>
#include "ir/instruction.hpp"

struct BasicBlock {
    int id = -1;
    int startIndex = 0;       // orijinal flat listedeki başlangıç
    int endIndex = 0;          // orijinal flat listedeki bitiş (exclusive)
    std::vector<Instruction> instructions;
    Opcode terminator = Opcode::RETURN;
    int jumpTarget = -1;       // block hedefi (block ID)
    std::vector<int> predecessors;
    std::vector<int> successors;

    std::string dump() const {
        std::ostringstream os;
        os << "BB_" << id << " [" << startIndex << ".." << endIndex << "]"
           << " preds:{";
        for (size_t i = 0; i < predecessors.size(); ++i) {
            if (i) os << ",";
            os << "BB_" << predecessors[i];
        }
        os << "} succs:{";
        for (size_t i = 0; i < successors.size(); ++i) {
            if (i) os << ",";
            os << "BB_" << successors[i];
        }
        os << "} term=" << opcodeName(terminator);
        if (jumpTarget >= 0) os << " ->BB_" << jumpTarget;
        os << "\n";
        for (const auto& ins : instructions) {
            os << "    ";
            if (&ins == &instructions.back()) os << "* ";
            else os << "  ";
            os << opcodeName(ins.opcode);
            if (ins.dest >= 0) os << " s" << ins.dest;
            os << "\n";
        }
        return os.str();
    }
};

struct CFG {
    std::vector<BasicBlock> blocks;

    bool isValid() const {
        if (blocks.empty()) return false;
        for (const auto& b : blocks) {
            if (b.id < 0 || b.id >= (int)blocks.size()) return false;
        }
        return true;
    }

    // CFG → flat instruction list
    // VM bu listeyi alır, CFG'yi görmez.
    std::vector<Instruction> linearize() const {
        std::vector<Instruction> result;
        // Blokları sırayla dolaş, instruction'ları ekle
        // Jump target'ları block ID → instruction index'e çevir
        for (const auto& block : blocks) {
            for (const auto& ins : block.instructions)
                result.push_back(ins);

            // Blok sonundaki jump'ın target'ını block ID'den index'e çevir
            if (!result.empty()) {
                Instruction& last = result.back();
                if ((last.opcode == Opcode::JMP || 
                     last.opcode == Opcode::JIF_FALSE ||
                     last.opcode == Opcode::JIF_TRUE) && last.jumpTarget >= 0) {
                    // last.jumpTarget şu an block ID
                    int targetBlock = last.jumpTarget;
                    if (targetBlock >= 0 && targetBlock < (int)blocks.size()) {
                        int targetIndex = 0;
                        for (int b = 0; b < targetBlock; ++b)
                            targetIndex += (int)blocks[b].instructions.size();
                        last.jumpTarget = targetIndex;
                    }
                }
            }
        }
        return result;
    }

    std::string dump() const {
        std::ostringstream os;
        os << "CFG: " << blocks.size() << " blocks\n";
        for (const auto& b : blocks)
            os << b.dump();
        return os.str();
    }
};

// Forward: CFG builder
// buildCFG(instructions) → CFG
// implementasyon ir_cfg.cpp'de
CFG buildCFG(const std::vector<Instruction>& instructions);

#endif
