// ============================================================================
// saQut IR — IRFunction Gerçeklemesi
// ============================================================================
//
// DİZİN:   src/ir/ir_function.cpp
// KATMAN:  IR — Fonksiyonun IR karşılığı (instruction listesi + slotlar)
//
// AMAÇ:
//   IRFunction::dump() ile debug çıktısı ve slot isim çözümlemesi.
//
// ============================================================================

#include "ir/ir_function.hpp"
#include "builtin/builtin_methods.hpp"
#include "ir/ir_color.hpp"
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
    return std::string(IrColor::SoftTurkuaz()) + slot(s) + IrColor::Reset();
}

// Yardımcı: int değeri SoftTuruncu renkle sar
static std::string ci(int v) {
    return std::string(IrColor::SoftTuruncu()) + std::to_string(v) + IrColor::Reset();
}

void IRFunction::dump() const {
    // Başlık: NAME=fibonacci PARAMS=1 SLOTS=10
    std::cout << IrColor::SoftGri() << "NAME=" << IrColor::Reset()
              << IrColor::SoftMor() << name << IrColor::Reset()
              << IrColor::SoftGri() << " PARAMS=" << IrColor::Reset()
              << IrColor::SoftTuruncu() << paramCount << IrColor::Reset()
              << IrColor::SoftGri() << " SLOTS=" << IrColor::Reset()
              << IrColor::SoftTuruncu() << slotCount << IrColor::Reset()
              << "\n";

    // Talimatlar
    for (int i = 0; i < (int)instructions.size(); i++) {
        const Instruction& ins = instructions[i];

        // Satır numarası
        std::cout << "  " << IrColor::SoftGri() << std::setw(3) << std::right << i << IrColor::Reset() << "  ";

        // Opcode sütunu
        std::cout << IrColor::SoftMor() << std::left << std::setw(16) << opcodeName(ins.opcode) << IrColor::Reset();

        // Operandlar — opcode'a göre farklı format
        if (ins.opcode == Opcode::LOAD_CONST) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset() << " " << ci(ins.intValue);

        } else if (ins.opcode == Opcode::LOAD_STRING) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " \"" << IrColor::SoftPembe() << ins.stringValue << IrColor::Reset() << "\"";

        } else if (ins.opcode == Opcode::LOAD_SLOT) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset() << " " << cs(ins.src);

        } else if (isBinaryOp(ins.opcode)) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset() << " "
                      << cs(ins.left) << " " << IrColor::SoftMor() << opSymbol(ins.opcode) << IrColor::Reset()
                      << " " << cs(ins.right);

        } else if (ins.opcode == Opcode::JMP) {
            std::cout << IrColor::SoftGri() << "→ " << IrColor::Reset() << ci(ins.jumpTarget);

        } else if (ins.opcode == Opcode::JIF_FALSE) {
            std::cout << IrColor::SoftGri() << "!" << IrColor::Reset() << cs(ins.cond)
                      << " " << IrColor::SoftGri() << "→" << IrColor::Reset() << " " << ci(ins.jumpTarget);

        } else if (ins.opcode == Opcode::JIF_TRUE) {
            std::cout << cs(ins.cond) << " " << IrColor::SoftGri() << "→" << IrColor::Reset() << " " << ci(ins.jumpTarget);

        } else if (ins.opcode == Opcode::CALL) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset() << " "
                      << IrColor::SoftYesil() << ins.functionName << IrColor::Reset()
                      << IrColor::SoftGri() << "(" << IrColor::Reset();
            for (int j = 0; j < (int)ins.argSlots.size(); j++) {
                if (j) std::cout << IrColor::SoftGri() << ", " << IrColor::Reset();
                std::cout << cs(ins.argSlots[j]);
            }
            std::cout << IrColor::SoftGri() << ")" << IrColor::Reset();

        } else if (ins.opcode == Opcode::CALLHOST) {
            if (ins.functionName == "__builtin_method__") {
                const auto* bm = BuiltinMethodRegistry::instance().byId(ins.intValue);
                std::string methodLabel = bm ? bm->name : ("id" + std::to_string(ins.intValue));
                if (ins.dest >= 0)
                    std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset() << " ";
                std::cout << IrColor::SoftGri() << "builtin::" << IrColor::Reset()
                          << IrColor::SoftYesil() << methodLabel << IrColor::Reset()
                          << IrColor::SoftGri() << "(" << IrColor::Reset();
                for (int j = 0; j < (int)ins.argSlots.size(); j++) {
                    if (j) std::cout << IrColor::SoftGri() << ", " << IrColor::Reset();
                    std::cout << cs(ins.argSlots[j]);
                }
                std::cout << IrColor::SoftGri() << ")" << IrColor::Reset();
            } else {
                std::cout << IrColor::SoftYesil() << ins.functionName << IrColor::Reset()
                          << IrColor::SoftGri() << "(" << IrColor::Reset();
                for (int j = 0; j < (int)ins.argSlots.size(); j++) {
                    if (j) std::cout << IrColor::SoftGri() << ", " << IrColor::Reset();
                    std::cout << cs(ins.argSlots[j]);
                }
                std::cout << IrColor::SoftGri() << ")" << IrColor::Reset();
            }

        } else if (ins.opcode == Opcode::BNOT) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftMor() << "~" << IrColor::Reset() << cs(ins.src);

        } else if (ins.opcode == Opcode::LOAD_FLOAT) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset() << " "
                      << IrColor::SoftTuruncu() << ins.floatValue << IrColor::Reset();

        } else if (ins.opcode == Opcode::INT_TO_FLOAT) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "(float)" << IrColor::Reset() << cs(ins.src);

        } else if (ins.opcode == Opcode::FLOAT_TO_INT) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "(int)" << IrColor::Reset() << cs(ins.src);

        } else if (ins.opcode == Opcode::CAST_INT_TO_STR) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "str(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset();
        } else if (ins.opcode == Opcode::CAST_FLOAT_TO_STR) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "str(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset();
        } else if (ins.opcode == Opcode::CAST_BOOL_TO_STR) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "str(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset();
        } else if (ins.opcode == Opcode::CAST_STR_TO_INT) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "int?(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset()
                      << (ins.left ? (std::string(" ") + IrColor::SoftTurkuaz() + "[null]" + IrColor::Reset()) : (std::string(" ") + IrColor::SoftTurkuaz() + "[throw]" + IrColor::Reset()));
        } else if (ins.opcode == Opcode::CAST_STR_TO_FLOAT) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "float?(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset()
                      << (ins.left ? (std::string(" ") + IrColor::SoftTurkuaz() + "[null]" + IrColor::Reset()) : (std::string(" ") + IrColor::SoftTurkuaz() + "[throw]" + IrColor::Reset()));
        } else if (ins.opcode == Opcode::CAST_FLOAT_TO_INT_CHECKED) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "int(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset()
                      << (ins.left ? (std::string(" ") + IrColor::SoftTurkuaz() + "[null]" + IrColor::Reset()) : (std::string(" ") + IrColor::SoftTurkuaz() + "[throw]" + IrColor::Reset()));

        } else if (ins.opcode == Opcode::FNEG) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftMor() << "-" << IrColor::Reset() << cs(ins.src);

        } else if (ins.opcode == Opcode::STRUCT_NEW) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "struct<" << IrColor::Reset()
                      << IrColor::SoftYesil() << ins.functionName << IrColor::Reset()
                      << IrColor::SoftGri() << ">[" << IrColor::Reset() << ci(ins.intValue)
                      << IrColor::SoftGri() << " alan]" << IrColor::Reset();

        } else if (ins.opcode == Opcode::FIELD_GET) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << cs(ins.src) << IrColor::SoftGri() << "." << IrColor::Reset() << ci(ins.intValue);

        } else if (ins.opcode == Opcode::FIELD_SET) {
            std::cout << cs(ins.dest) << IrColor::SoftGri() << "." << IrColor::Reset() << ci(ins.intValue)
                      << " " << IrColor::SoftGri() << "=" << IrColor::Reset() << " " << cs(ins.right);

        } else if (ins.opcode == Opcode::ARRAY_NEW) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "array<" << IrColor::Reset();
            switch (ins.arrayElemKind) {
                case ArrayElemKind::Ref:     std::cout << "ref";     break;
                case ArrayElemKind::Byte:    std::cout << "byte";    break;
                case ArrayElemKind::Int:     std::cout << "int";     break;
                case ArrayElemKind::LongInt: std::cout << "long";    break;
                case ArrayElemKind::Float32: std::cout << "f32";     break;
                case ArrayElemKind::Float64: std::cout << "f64";     break;
                case ArrayElemKind::Decimal: std::cout << "dec";     break;
            }
            std::cout << IrColor::SoftGri() << ">[" << IrColor::Reset() << ci(ins.intValue) << IrColor::SoftGri() << "]" << IrColor::Reset();

        } else if (ins.opcode == Opcode::ARRAY_GET) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << cs(ins.left) << IrColor::SoftGri() << "[" << IrColor::Reset()
                      << cs(ins.right) << IrColor::SoftGri() << "]" << IrColor::Reset();

        } else if (ins.opcode == Opcode::ARRAY_SET) {
            std::cout << cs(ins.dest) << IrColor::SoftGri() << "[" << IrColor::Reset()
                      << cs(ins.left) << IrColor::SoftGri() << "] =" << IrColor::Reset() << " " << cs(ins.right);

        } else if (ins.opcode == Opcode::ARRAY_LEN) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "len(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset();

        } else if (ins.opcode == Opcode::LOAD_GLOBAL) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "global[" << IrColor::Reset() << ci(ins.intValue) << IrColor::SoftGri() << "]" << IrColor::Reset();

        } else if (ins.opcode == Opcode::STORE_GLOBAL) {
            std::cout << IrColor::SoftGri() << "global[" << IrColor::Reset() << ci(ins.intValue)
                      << IrColor::SoftGri() << "] =" << IrColor::Reset() << " " << cs(ins.src);

        } else if (ins.opcode == Opcode::LOAD_NULL) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftTurkuaz() << "null" << IrColor::Reset();

        } else if (ins.opcode == Opcode::LOAD_DECIMAL) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset() << " "
                      << IrColor::SoftTuruncu() << ins.decimalValue.toString() << IrColor::Reset()
                      << IrColor::SoftGri() << "d" << IrColor::Reset();

        } else if (ins.opcode == Opcode::INT_TO_DECIMAL) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "(decimal)" << IrColor::Reset() << cs(ins.src);

        } else if (ins.opcode == Opcode::FLOAT_TO_DECIMAL) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "(decimal)" << IrColor::Reset() << cs(ins.src);

        } else if (ins.opcode == Opcode::DNEG) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "-d" << IrColor::Reset() << " " << cs(ins.src);

        } else if (ins.opcode == Opcode::CAST_DECIMAL_TO_STR) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "str(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset();

        } else if (ins.opcode == Opcode::CAST_DECIMAL_TO_FLOAT) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "float(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset();

        } else if (ins.opcode == Opcode::CAST_DECIMAL_TO_INT) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "int(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset()
                      << (ins.left ? (std::string(" ") + IrColor::SoftTurkuaz() + "[null]" + IrColor::Reset()) : (std::string(" ") + IrColor::SoftTurkuaz() + "[throw]" + IrColor::Reset()));

        } else if (ins.opcode == Opcode::CAST_STR_TO_DECIMAL) {
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "decimal?(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset()
                      << (ins.left ? (std::string(" ") + IrColor::SoftTurkuaz() + "[null]" + IrColor::Reset()) : (std::string(" ") + IrColor::SoftTurkuaz() + "[throw]" + IrColor::Reset()));

        } else if (ins.opcode == Opcode::ENTER_TRY) {
            std::cout << IrColor::SoftGri() << "err→" << IrColor::Reset() << cs(ins.dest)
                      << "  " << IrColor::SoftGri() << "catch→" << IrColor::Reset() << ci(ins.jumpTarget);

        } else if (ins.opcode == Opcode::LEAVE_TRY) {
            // operand yok

        } else if (ins.opcode == Opcode::THROW) {
            std::cout << cs(ins.src);

        } else if (ins.opcode == Opcode::RETURN) {
            std::cout << IrColor::SoftGri() << "return" << IrColor::Reset() << " " << cs(ins.src);
        }

        std::cout << "\n";
    }
    std::cout << "\n";
}
