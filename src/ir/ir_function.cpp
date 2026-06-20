#include "ir/ir_function.hpp"
#include <iomanip>
#include <iostream>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// Yardımcılar
// ─────────────────────────────────────────────────────────────────────────────

// Slot adını kısa göster: s0, s1, ...
static std::string slot(int s) {
    if (s == -1) return "?";
    return "s" + std::to_string(s);
}

// İkili op sembolü: ADD → "+"
static const char* opSymbol(Opcode op) {
    switch (op) {
        case Opcode::ADD:           return "+";
        case Opcode::SUB:           return "-";
        case Opcode::MUL:           return "*";
        case Opcode::DIV:           return "/";
        case Opcode::FADD:          return "+.";
        case Opcode::FSUB:          return "-.";
        case Opcode::FMUL:          return "*.";
        case Opcode::FDIV:          return "/.";
        case Opcode::MOD:           return "%";
        case Opcode::BAND:          return "&";
        case Opcode::BOR:           return "|";
        case Opcode::SHL:           return "<<";
        case Opcode::SHR:           return ">>";
        case Opcode::LESS:          return "<";
        case Opcode::LESS_EQUAL:    return "<=";
        case Opcode::GREATER:       return ">";
        case Opcode::GREATER_EQUAL: return ">=";
        case Opcode::EQUAL_EQUAL:   return "==";
        case Opcode::NOT_EQUAL:     return "!=";
        default:                    return "?";
    }
}

static bool isBinaryOp(Opcode op) {
    switch (op) {
        case Opcode::ADD: case Opcode::SUB: case Opcode::MUL:
        case Opcode::DIV: case Opcode::MOD:
        case Opcode::FADD: case Opcode::FSUB: case Opcode::FMUL: case Opcode::FDIV:
        case Opcode::BAND: case Opcode::BOR: case Opcode::SHL: case Opcode::SHR:
        case Opcode::LESS: case Opcode::LESS_EQUAL:
        case Opcode::GREATER: case Opcode::GREATER_EQUAL:
        case Opcode::EQUAL_EQUAL: case Opcode::NOT_EQUAL:
            return true;
        default: return false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// IRFunction::dump
// ─────────────────────────────────────────────────────────────────────────────

void IRFunction::dump() const {
    // Başlık: fonksiyon adı + slot bilgisi
    std::string header = "  " + name + "()";
    if (paramCount > 0) {
        header = "  " + name + "(";
        for (int i = 0; i < paramCount; i++) {
            if (i) header += ", ";
            header += "s" + std::to_string(i);
        }
        header += ")";
    }
    header += "  [" + std::to_string(slotCount) + " slot]";

    // Başlık: NAME=fibonacci PARAMS=1 SLOTS=10
    std::cout << "NAME=" << name
              << " PARAMS=" << paramCount
              << " SLOTS=" << slotCount
              << "\n";

    // Talimatlar
    for (int i = 0; i < (int)instructions.size(); i++) {
        const Instruction& ins = instructions[i];

        // Satır numarası
        std::cout << "  " << std::setw(3) << std::right << i << "  ";

        // Opcode sütunu (12 karakter genişlik)
        std::cout << std::left << std::setw(12) << opcodeName(ins.opcode);

        // Operandlar — opcode'a göre farklı format
        if (ins.opcode == Opcode::LOAD_CONST) {
            std::cout << slot(ins.dest) << " = " << ins.intValue;

        } else if (ins.opcode == Opcode::LOAD_STRING) {
            std::cout << slot(ins.dest) << " = \"" << ins.stringValue << "\"";

        } else if (ins.opcode == Opcode::LOAD_SLOT) {
            std::cout << slot(ins.dest) << " = " << slot(ins.src);

        } else if (isBinaryOp(ins.opcode)) {
            std::cout << slot(ins.dest) << " = "
                      << slot(ins.left) << " " << opSymbol(ins.opcode)
                      << " " << slot(ins.right);

        } else if (ins.opcode == Opcode::JMP) {
            std::cout << "→ " << ins.jumpTarget;

        } else if (ins.opcode == Opcode::JIF_FALSE) {
            std::cout << "!" << slot(ins.cond) << " → " << ins.jumpTarget;

        } else if (ins.opcode == Opcode::CALL) {
            std::cout << slot(ins.dest) << " = " << ins.functionName << "(";
            for (int j = 0; j < (int)ins.argSlots.size(); j++) {
                if (j) std::cout << ", ";
                std::cout << slot(ins.argSlots[j]);
            }
            std::cout << ")";

        } else if (ins.opcode == Opcode::CALLHOST) {
            std::cout << ins.functionName << "(";
            for (int j = 0; j < (int)ins.argSlots.size(); j++) {
                if (j) std::cout << ", ";
                std::cout << slot(ins.argSlots[j]);
            }
            std::cout << ")";

        } else if (ins.opcode == Opcode::BNOT) {
            std::cout << slot(ins.dest) << " = ~" << slot(ins.src);

        } else if (ins.opcode == Opcode::LOAD_FLOAT) {
            std::cout << slot(ins.dest) << " = " << ins.floatValue;

        } else if (ins.opcode == Opcode::INT_TO_FLOAT) {
            std::cout << slot(ins.dest) << " = (float)" << slot(ins.src);

        } else if (ins.opcode == Opcode::FLOAT_TO_INT) {
            std::cout << slot(ins.dest) << " = (int)" << slot(ins.src);

        } else if (ins.opcode == Opcode::FNEG) {
            std::cout << slot(ins.dest) << " = -" << slot(ins.src);

        } else if (ins.opcode == Opcode::STRUCT_NEW) {
            std::cout << slot(ins.dest) << " = struct<" << ins.functionName << ">[" << ins.intValue << " alan]";

        } else if (ins.opcode == Opcode::FIELD_GET) {
            std::cout << slot(ins.dest) << " = " << slot(ins.src) << "." << ins.intValue;

        } else if (ins.opcode == Opcode::FIELD_SET) {
            std::cout << slot(ins.dest) << "." << ins.intValue << " = " << slot(ins.right);

        } else if (ins.opcode == Opcode::ARRAY_NEW) {
            std::cout << slot(ins.dest) << " = array[" << ins.intValue << "]";

        } else if (ins.opcode == Opcode::ARRAY_GET) {
            std::cout << slot(ins.dest) << " = " << slot(ins.left) << "[" << slot(ins.right) << "]";

        } else if (ins.opcode == Opcode::ARRAY_SET) {
            std::cout << slot(ins.dest) << "[" << slot(ins.left) << "] = " << slot(ins.right);

        } else if (ins.opcode == Opcode::ARRAY_LEN) {
            std::cout << slot(ins.dest) << " = len(" << slot(ins.src) << ")";

        } else if (ins.opcode == Opcode::LOAD_GLOBAL) {
            std::cout << slot(ins.dest) << " = global[" << ins.intValue << "]";

        } else if (ins.opcode == Opcode::STORE_GLOBAL) {
            std::cout << "global[" << ins.intValue << "] = " << slot(ins.src);

        } else if (ins.opcode == Opcode::RETURN) {
            std::cout << slot(ins.src);
        }

        std::cout << "\n";
    }
    std::cout << "\n";
}
