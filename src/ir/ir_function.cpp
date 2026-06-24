#include "ir/ir_function.hpp"
#include "builtin/builtin_methods.hpp"
#include "tools.hpp"
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
        case Opcode::BXOR:          return "^";
        case Opcode::SHL:           return "<<";
        case Opcode::SHR:           return ">>";
        case Opcode::LESS:          return "<";
        case Opcode::LESS_EQUAL:    return "<=";
        case Opcode::GREATER:       return ">";
        case Opcode::GREATER_EQUAL: return ">=";
        case Opcode::EQUAL_EQUAL:   return "==";
        case Opcode::NOT_EQUAL:     return "!=";
        case Opcode::STRING_CONCAT: return "++";
        case Opcode::DADD:          return "+d";
        case Opcode::DSUB:          return "-d";
        case Opcode::DMUL:          return "*d";
        case Opcode::DDIV:          return "/d";
        case Opcode::DMOD:          return "%d";
        default:                    return "?";
    }
}

static bool isBinaryOp(Opcode op) {
    switch (op) {
        case Opcode::ADD: case Opcode::SUB: case Opcode::MUL:
        case Opcode::DIV: case Opcode::MOD:
        case Opcode::FADD: case Opcode::FSUB: case Opcode::FMUL: case Opcode::FDIV:
        case Opcode::BAND: case Opcode::BOR: case Opcode::BXOR:
        case Opcode::SHL: case Opcode::SHR:
        case Opcode::LESS: case Opcode::LESS_EQUAL:
        case Opcode::GREATER: case Opcode::GREATER_EQUAL:
        case Opcode::EQUAL_EQUAL: case Opcode::NOT_EQUAL:
        case Opcode::STRING_CONCAT:
        case Opcode::DADD: case Opcode::DSUB: case Opcode::DMUL:
        case Opcode::DDIV: case Opcode::DMOD:
            return true;
        default: return false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// IRFunction::dump
// ─────────────────────────────────────────────────────────────────────────────

// Yardımcı: slot'u SoftTurkuaz renkle sar
static std::string cs(int s) {
    return std::string(Color::SoftTurkuaz) + slot(s) + Color::Reset;
}

// Yardımcı: int değeri SoftTuruncu renkle sar
static std::string ci(int v) {
    return std::string(Color::SoftTuruncu) + std::to_string(v) + Color::Reset;
}

void IRFunction::dump() const {
    // Başlık: NAME=fibonacci PARAMS=1 SLOTS=10
    std::cout << Color::SoftGri << "NAME=" << Color::Reset
              << Color::SoftMor << name << Color::Reset
              << Color::SoftGri << " PARAMS=" << Color::Reset
              << Color::SoftTuruncu << paramCount << Color::Reset
              << Color::SoftGri << " SLOTS=" << Color::Reset
              << Color::SoftTuruncu << slotCount << Color::Reset
              << "\n";

    // Talimatlar
    for (int i = 0; i < (int)instructions.size(); i++) {
        const Instruction& ins = instructions[i];

        // Satır numarası
        std::cout << "  " << Color::SoftGri << std::setw(3) << std::right << i << Color::Reset << "  ";

        // Opcode sütunu
        std::cout << Color::SoftMor << std::left << std::setw(16) << opcodeName(ins.opcode) << Color::Reset;

        // Operandlar — opcode'a göre farklı format
        if (ins.opcode == Opcode::LOAD_CONST) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset << " " << ci(ins.intValue);

        } else if (ins.opcode == Opcode::LOAD_STRING) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset
                      << " \"" << Color::SoftPembe << ins.stringValue << Color::Reset << "\"";

        } else if (ins.opcode == Opcode::LOAD_SLOT) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset << " " << cs(ins.src);

        } else if (isBinaryOp(ins.opcode)) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset << " "
                      << cs(ins.left) << " " << Color::SoftMor << opSymbol(ins.opcode) << Color::Reset
                      << " " << cs(ins.right);

        } else if (ins.opcode == Opcode::JMP) {
            std::cout << Color::SoftGri << "→ " << Color::Reset << ci(ins.jumpTarget);

        } else if (ins.opcode == Opcode::JIF_FALSE) {
            std::cout << Color::SoftGri << "!" << Color::Reset << cs(ins.cond)
                      << " " << Color::SoftGri << "→" << Color::Reset << " " << ci(ins.jumpTarget);

        } else if (ins.opcode == Opcode::JIF_TRUE) {
            std::cout << cs(ins.cond) << " " << Color::SoftGri << "→" << Color::Reset << " " << ci(ins.jumpTarget);

        } else if (ins.opcode == Opcode::CALL) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset << " "
                      << Color::SoftYesil << ins.functionName << Color::Reset
                      << Color::SoftGri << "(" << Color::Reset;
            for (int j = 0; j < (int)ins.argSlots.size(); j++) {
                if (j) std::cout << Color::SoftGri << ", " << Color::Reset;
                std::cout << cs(ins.argSlots[j]);
            }
            std::cout << Color::SoftGri << ")" << Color::Reset;

        } else if (ins.opcode == Opcode::CALLHOST) {
            if (ins.functionName == "__builtin_method__") {
                const auto* bm = BuiltinMethodRegistry::instance().byId(ins.intValue);
                std::string methodLabel = bm ? bm->name : ("id" + std::to_string(ins.intValue));
                if (ins.dest >= 0)
                    std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset << " ";
                std::cout << Color::SoftGri << "builtin::" << Color::Reset
                          << Color::SoftYesil << methodLabel << Color::Reset
                          << Color::SoftGri << "(" << Color::Reset;
                for (int j = 0; j < (int)ins.argSlots.size(); j++) {
                    if (j) std::cout << Color::SoftGri << ", " << Color::Reset;
                    std::cout << cs(ins.argSlots[j]);
                }
                std::cout << Color::SoftGri << ")" << Color::Reset;
            } else {
                std::cout << Color::SoftYesil << ins.functionName << Color::Reset
                          << Color::SoftGri << "(" << Color::Reset;
                for (int j = 0; j < (int)ins.argSlots.size(); j++) {
                    if (j) std::cout << Color::SoftGri << ", " << Color::Reset;
                    std::cout << cs(ins.argSlots[j]);
                }
                std::cout << Color::SoftGri << ")" << Color::Reset;
            }

        } else if (ins.opcode == Opcode::BNOT) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset
                      << " " << Color::SoftMor << "~" << Color::Reset << cs(ins.src);

        } else if (ins.opcode == Opcode::LOAD_FLOAT) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset << " "
                      << Color::SoftTuruncu << ins.floatValue << Color::Reset;

        } else if (ins.opcode == Opcode::INT_TO_FLOAT) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset
                      << " " << Color::SoftGri << "(float)" << Color::Reset << cs(ins.src);

        } else if (ins.opcode == Opcode::FLOAT_TO_INT) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset
                      << " " << Color::SoftGri << "(int)" << Color::Reset << cs(ins.src);

        } else if (ins.opcode == Opcode::CAST_INT_TO_STR) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset
                      << " " << Color::SoftGri << "str(" << Color::Reset << cs(ins.src) << Color::SoftGri << ")" << Color::Reset;
        } else if (ins.opcode == Opcode::CAST_FLOAT_TO_STR) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset
                      << " " << Color::SoftGri << "str(" << Color::Reset << cs(ins.src) << Color::SoftGri << ")" << Color::Reset;
        } else if (ins.opcode == Opcode::CAST_BOOL_TO_STR) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset
                      << " " << Color::SoftGri << "str(" << Color::Reset << cs(ins.src) << Color::SoftGri << ")" << Color::Reset;
        } else if (ins.opcode == Opcode::CAST_STR_TO_INT) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset
                      << " " << Color::SoftGri << "int?(" << Color::Reset << cs(ins.src) << Color::SoftGri << ")" << Color::Reset
                      << (ins.left ? (std::string(" ") + Color::SoftTurkuaz + "[null]" + Color::Reset) : (std::string(" ") + Color::SoftTurkuaz + "[throw]" + Color::Reset));
        } else if (ins.opcode == Opcode::CAST_STR_TO_FLOAT) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset
                      << " " << Color::SoftGri << "float?(" << Color::Reset << cs(ins.src) << Color::SoftGri << ")" << Color::Reset
                      << (ins.left ? (std::string(" ") + Color::SoftTurkuaz + "[null]" + Color::Reset) : (std::string(" ") + Color::SoftTurkuaz + "[throw]" + Color::Reset));
        } else if (ins.opcode == Opcode::CAST_FLOAT_TO_INT_CHECKED) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset
                      << " " << Color::SoftGri << "int(" << Color::Reset << cs(ins.src) << Color::SoftGri << ")" << Color::Reset
                      << (ins.left ? (std::string(" ") + Color::SoftTurkuaz + "[null]" + Color::Reset) : (std::string(" ") + Color::SoftTurkuaz + "[throw]" + Color::Reset));

        } else if (ins.opcode == Opcode::FNEG) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset
                      << " " << Color::SoftMor << "-" << Color::Reset << cs(ins.src);

        } else if (ins.opcode == Opcode::STRUCT_NEW) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset
                      << " " << Color::SoftGri << "struct<" << Color::Reset
                      << Color::SoftYesil << ins.functionName << Color::Reset
                      << Color::SoftGri << ">[" << Color::Reset << ci(ins.intValue)
                      << Color::SoftGri << " alan]" << Color::Reset;

        } else if (ins.opcode == Opcode::FIELD_GET) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset
                      << " " << cs(ins.src) << Color::SoftGri << "." << Color::Reset << ci(ins.intValue);

        } else if (ins.opcode == Opcode::FIELD_SET) {
            std::cout << cs(ins.dest) << Color::SoftGri << "." << Color::Reset << ci(ins.intValue)
                      << " " << Color::SoftGri << "=" << Color::Reset << " " << cs(ins.right);

        } else if (ins.opcode == Opcode::ARRAY_NEW) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset
                      << " " << Color::SoftGri << "array[" << Color::Reset << ci(ins.intValue) << Color::SoftGri << "]" << Color::Reset;

        } else if (ins.opcode == Opcode::ARRAY_GET) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset
                      << " " << cs(ins.left) << Color::SoftGri << "[" << Color::Reset
                      << cs(ins.right) << Color::SoftGri << "]" << Color::Reset;

        } else if (ins.opcode == Opcode::ARRAY_SET) {
            std::cout << cs(ins.dest) << Color::SoftGri << "[" << Color::Reset
                      << cs(ins.left) << Color::SoftGri << "] =" << Color::Reset << " " << cs(ins.right);

        } else if (ins.opcode == Opcode::ARRAY_LEN) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset
                      << " " << Color::SoftGri << "len(" << Color::Reset << cs(ins.src) << Color::SoftGri << ")" << Color::Reset;

        } else if (ins.opcode == Opcode::LOAD_GLOBAL) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset
                      << " " << Color::SoftGri << "global[" << Color::Reset << ci(ins.intValue) << Color::SoftGri << "]" << Color::Reset;

        } else if (ins.opcode == Opcode::STORE_GLOBAL) {
            std::cout << Color::SoftGri << "global[" << Color::Reset << ci(ins.intValue)
                      << Color::SoftGri << "] =" << Color::Reset << " " << cs(ins.src);

        } else if (ins.opcode == Opcode::LOAD_NULL) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset
                      << " " << Color::SoftTurkuaz << "null" << Color::Reset;

        } else if (ins.opcode == Opcode::LOAD_DECIMAL) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset << " "
                      << Color::SoftTuruncu << ins.decimalValue.toString() << Color::Reset
                      << Color::SoftGri << "d" << Color::Reset;

        } else if (ins.opcode == Opcode::INT_TO_DECIMAL) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset
                      << " " << Color::SoftGri << "(decimal)" << Color::Reset << cs(ins.src);

        } else if (ins.opcode == Opcode::FLOAT_TO_DECIMAL) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset
                      << " " << Color::SoftGri << "(decimal)" << Color::Reset << cs(ins.src);

        } else if (ins.opcode == Opcode::DNEG) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset
                      << " " << Color::SoftGri << "-d" << Color::Reset << " " << cs(ins.src);

        } else if (ins.opcode == Opcode::CAST_DECIMAL_TO_STR) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset
                      << " " << Color::SoftGri << "str(" << Color::Reset << cs(ins.src) << Color::SoftGri << ")" << Color::Reset;

        } else if (ins.opcode == Opcode::CAST_DECIMAL_TO_FLOAT) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset
                      << " " << Color::SoftGri << "float(" << Color::Reset << cs(ins.src) << Color::SoftGri << ")" << Color::Reset;

        } else if (ins.opcode == Opcode::CAST_DECIMAL_TO_INT) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset
                      << " " << Color::SoftGri << "int(" << Color::Reset << cs(ins.src) << Color::SoftGri << ")" << Color::Reset
                      << (ins.left ? (std::string(" ") + Color::SoftTurkuaz + "[null]" + Color::Reset) : (std::string(" ") + Color::SoftTurkuaz + "[throw]" + Color::Reset));

        } else if (ins.opcode == Opcode::CAST_STR_TO_DECIMAL) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset
                      << " " << Color::SoftGri << "decimal?(" << Color::Reset << cs(ins.src) << Color::SoftGri << ")" << Color::Reset
                      << (ins.left ? (std::string(" ") + Color::SoftTurkuaz + "[null]" + Color::Reset) : (std::string(" ") + Color::SoftTurkuaz + "[throw]" + Color::Reset));

        } else if (ins.opcode == Opcode::ENTER_TRY) {
            std::cout << Color::SoftGri << "err→" << Color::Reset << cs(ins.dest)
                      << "  " << Color::SoftGri << "catch→" << Color::Reset << ci(ins.jumpTarget);

        } else if (ins.opcode == Opcode::LEAVE_TRY) {
            // operand yok

        } else if (ins.opcode == Opcode::THROW) {
            std::cout << cs(ins.src);

        } else if (ins.opcode == Opcode::RETURN) {
            std::cout << Color::SoftGri << "return" << Color::Reset << " " << cs(ins.src);
        }

        std::cout << "\n";
    }
    std::cout << "\n";
}
