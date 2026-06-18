#include "ir/ir_function.hpp"
#include <iomanip>
#include <iostream>

// Her instruction'ı "indeks: OPCODE  operandlar" formatında yazdır.
// Bu çıktı hem insanın okuduğu hem de birim testlerin karşılaştırdığı formattır.
void IRFunction::dump() const {
    std::cout << "=== " << name
              << " (paramCount=" << paramCount
              << ", slotCount=" << slotCount << ") ===\n";

    for (int i = 0; i < (int)instructions.size(); i++) {
        const Instruction& ins = instructions[i];
        std::cout << "  " << std::setw(3) << i << ": "
                  << std::left << std::setw(14) << opcodeName(ins.opcode);

        switch (ins.opcode) {
            case Opcode::LOAD_CONST:
                std::cout << "slot[" << ins.dest << "] = " << ins.intValue;
                break;
            case Opcode::LOAD_SLOT:
                std::cout << "slot[" << ins.dest << "] = slot[" << ins.src << "]";
                break;
            case Opcode::ADD: case Opcode::SUB: case Opcode::MUL:
            case Opcode::DIV: case Opcode::MOD:
            case Opcode::LESS: case Opcode::LESS_EQUAL:
            case Opcode::GREATER: case Opcode::GREATER_EQUAL:
            case Opcode::EQUAL_EQUAL: case Opcode::NOT_EQUAL:
                std::cout << "slot[" << ins.dest << "] = "
                          << "slot[" << ins.left << "] op slot[" << ins.right << "]";
                break;
            case Opcode::JMP:
                std::cout << "→ " << ins.jumpTarget;
                break;
            case Opcode::JIF_FALSE:
                std::cout << "if !slot[" << ins.cond << "] → " << ins.jumpTarget;
                break;
            case Opcode::CALL: {
                std::cout << "slot[" << ins.dest << "] = " << ins.functionName << "(";
                for (int j = 0; j < (int)ins.argSlots.size(); j++) {
                    if (j) std::cout << ", ";
                    std::cout << "slot[" << ins.argSlots[j] << "]";
                }
                std::cout << ")";
                break;
            }
            case Opcode::RETURN:
                std::cout << "slot[" << ins.src << "]";
                break;
            case Opcode::CALLHOST: {
                std::cout << ins.functionName << "(";
                for (int j = 0; j < (int)ins.argSlots.size(); j++) {
                    if (j) std::cout << ", ";
                    std::cout << "slot[" << ins.argSlots[j] << "]";
                }
                std::cout << ")";
                break;
            }
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}
