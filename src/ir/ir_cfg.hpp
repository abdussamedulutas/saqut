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
#include <iomanip>
#include "ir/instruction.hpp"
#include "ir/ir_color.hpp"   // TTY-aware renk — redirect'te ANSI yok (#141 deseni)
#include "ir/ir_dump.hpp"    // ortak operand renderer'ı (literal değerler, #218)

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
        // TTY-aware renk: gerçek terminalde renkli, redirect/pipe'ta düz.
        // Semantik metin (blok aralığı, kenar listesi, terminator) renkten
        // bağımsız aynıdır — yalnız renk kod noktaları eklenir/çıkarılır.
        std::ostringstream os;
        // Renk şeması (kullanıcı): blok adları gri, metadata koyu sarı,
        // opcode turuncu, CALLHOST/FFI kırmızı, slot değerleri açık mavi.
        os << IrColor::SoftGri() << "BB_" << id << IrColor::Reset()
           << IrColor::KoyuSari() << " [" << startIndex << ".." << endIndex << "]"
           << " preds:{" << IrColor::Reset();
        for (size_t i = 0; i < predecessors.size(); ++i) {
            if (i) os << ",";
            os << IrColor::SoftGri() << "BB_" << predecessors[i] << IrColor::Reset();
        }
        os << IrColor::KoyuSari() << "} succs:{" << IrColor::Reset();
        for (size_t i = 0; i < successors.size(); ++i) {
            if (i) os << ",";
            os << IrColor::SoftGri() << "BB_" << successors[i] << IrColor::Reset();
        }
        os << IrColor::KoyuSari() << "} term=" << IrColor::Reset()
           << IrColor::KoyuSari() << opcodeName(terminator) << IrColor::Reset();
        if (jumpTarget >= 0)
            os << IrColor::KoyuSari() << " ->" << IrColor::Reset()
               << IrColor::SoftGri() << "BB_" << jumpTarget << IrColor::Reset();
        os << "\n";
        for (const auto& ins : instructions) {
            os << "    ";
            if (&ins == &instructions.back()) os << IrColor::SoftGri() << "* " << IrColor::Reset();
            else os << "  ";
            // CALLHOST (builtin metod + __ffi__) dış dünya çağrısı → kırmızı;
            // diğer opcode'lar turuncu. Operandlar (literal değerler dahil)
            // ortak renderer'dan — CFG paleti (ir_dump.hpp, #218).
            const char* opColor = (ins.opcode == Opcode::CALLHOST)
                ? IrColor::Kirmizi() : IrColor::SoftTuruncu();
            os << opColor << std::left << std::setw(16) << opcodeName(ins.opcode) << IrColor::Reset();
            os << IrDump::operands(ins, IrDump::kCfgPalette);
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
        // Jump target'ları BLOCK ID → instruction index'e çevir.
        // KAYNAK block.jumpTarget'tır (buildCFG'de çözümlenmiş blok ID);
        // instruction'ın kendi jumpTarget'ı orijinal TALİMAT İNDEKSİNİ taşır
        // ve flat listedeki sıra korunduğundan DOKUNULMADAN kalmalıdır.
        for (const auto& block : blocks) {
            for (const auto& ins : block.instructions)
                result.push_back(ins);

            if (!result.empty()) {
                Instruction& last = result.back();
                if ((last.opcode == Opcode::JMP ||
                     last.opcode == Opcode::JIF_FALSE ||
                     last.opcode == Opcode::JIF_TRUE) && block.jumpTarget >= 0) {
                    int targetBlock = block.jumpTarget;
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
        os << IrColor::SoftGri() << "CFG: " << IrColor::Reset()
           << IrColor::SoftTuruncu() << blocks.size() << IrColor::Reset()
           << " blocks\n";
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
