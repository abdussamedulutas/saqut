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
        case Opcode::F32ADD:        return "+f32";
        case Opcode::F32SUB:        return "-f32";
        case Opcode::F32MUL:        return "*f32";
        case Opcode::F32DIV:        return "/f32";
        case Opcode::LADD:          return "+l";
        case Opcode::LSUB:          return "-l";
        case Opcode::LMUL:          return "*l";
        case Opcode::LDIV:          return "/l";
        case Opcode::LMOD:          return "%l";
        case Opcode::LBAND:         return "&l";
        case Opcode::LBOR:          return "|l";
        case Opcode::LBXOR:         return "^l";
        case Opcode::LSHL:          return "<<l";
        case Opcode::LSHR:          return ">>l";
        default:                    return "?";
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

        // Operandlar — opcode'a göre farklı format.
        //
        // Bilinçli olarak switch/default'suz: yeni bir Opcode enum değeri
        // eklenip buraya bir case eklenmezse -Wswitch (CMakeLists.txt'te bu
        // dosya için -Werror'a yükseltilir) derlemeyi kırar. Böylece yeni
        // opcode hiçbir zaman sessizce operandsız satıra düşemez (IR-K6).
        switch (ins.opcode) {
        case Opcode::LOAD_CONST:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset() << " " << ci(ins.intValue);
            break;

        case Opcode::LOAD_STRING:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " \"" << IrColor::SoftPembe() << ins.stringValue << IrColor::Reset() << "\"";
            break;

        case Opcode::LOAD_SLOT:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset() << " " << cs(ins.src);
            break;

        // İkili operatörler (dest = left OP right) — tek gövde paylaşılır.
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
        case Opcode::F32ADD: case Opcode::F32SUB: case Opcode::F32MUL: case Opcode::F32DIV:
        case Opcode::LADD: case Opcode::LSUB: case Opcode::LMUL: case Opcode::LDIV: case Opcode::LMOD:
        case Opcode::LBAND: case Opcode::LBOR: case Opcode::LBXOR:
        case Opcode::LSHL: case Opcode::LSHR:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset() << " "
                      << cs(ins.left) << " " << IrColor::SoftMor() << opSymbol(ins.opcode) << IrColor::Reset()
                      << " " << cs(ins.right);
            break;

        case Opcode::JMP:
            std::cout << IrColor::SoftGri() << "→ " << IrColor::Reset() << ci(ins.jumpTarget);
            break;

        case Opcode::JIF_FALSE:
            std::cout << IrColor::SoftGri() << "!" << IrColor::Reset() << cs(ins.cond)
                      << " " << IrColor::SoftGri() << "→" << IrColor::Reset() << " " << ci(ins.jumpTarget);
            break;

        case Opcode::JIF_TRUE:
            std::cout << cs(ins.cond) << " " << IrColor::SoftGri() << "→" << IrColor::Reset() << " " << ci(ins.jumpTarget);
            break;

        case Opcode::CALL:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset() << " "
                      << IrColor::SoftYesil() << ins.functionName << IrColor::Reset()
                      << IrColor::SoftGri() << "(" << IrColor::Reset();
            for (int j = 0; j < (int)ins.argSlots.size(); j++) {
                if (j) std::cout << IrColor::SoftGri() << ", " << IrColor::Reset();
                std::cout << cs(ins.argSlots[j]);
            }
            std::cout << IrColor::SoftGri() << ")" << IrColor::Reset();
            break;

        case Opcode::CALLHOST:
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
            break;

        case Opcode::BNOT:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftMor() << "~" << IrColor::Reset() << cs(ins.src);
            break;

        case Opcode::LBNOT:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftMor() << "~l" << IrColor::Reset() << cs(ins.src);
            break;

        case Opcode::LOAD_FLOAT:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset() << " "
                      << IrColor::SoftTuruncu() << ins.floatValue << IrColor::Reset();
            break;

        case Opcode::LOAD_FLOAT32:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset() << " "
                      << IrColor::SoftTuruncu() << ins.floatValue << IrColor::Reset() << "f32";
            break;

        case Opcode::LOAD_LONG:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset() << " "
                      << IrColor::SoftTuruncu() << ins.int64Value << IrColor::Reset() << "l";
            break;

        case Opcode::INT_TO_FLOAT:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "(float)" << IrColor::Reset() << cs(ins.src);
            break;

        case Opcode::FLOAT_TO_INT:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "(int)" << IrColor::Reset() << cs(ins.src);
            break;

        case Opcode::INT_TO_FLOAT32:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "(float32)" << IrColor::Reset() << cs(ins.src);
            break;

        case Opcode::FLOAT_TO_FLOAT32:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "(float32)" << IrColor::Reset() << cs(ins.src);
            break;

        } else if (ins.opcode == Opcode::ARRAY_NEW) {
            std::cout << cs(ins.dest) << " " << Color::SoftGri << "=" << Color::Reset
                      << " " << Color::SoftGri << "array<" << Color::Reset;
            switch (ins.arrayElemKind) {
                case ArrayElemKind::Ref:     std::cout << "ref";     break;
                case ArrayElemKind::Byte:    std::cout << "byte";    break;
                case ArrayElemKind::Int:     std::cout << "int";     break;
                case ArrayElemKind::LongInt: std::cout << "long";    break;
                case ArrayElemKind::Float32: std::cout << "f32";     break;
                case ArrayElemKind::Float64: std::cout << "f64";     break;
                case ArrayElemKind::Decimal: std::cout << "dec";     break;
            }
            std::cout << Color::SoftGri << ">[" << Color::Reset << ci(ins.intValue) << Color::SoftGri << "]" << Color::Reset;

        case Opcode::FLOAT32_TO_INT:
            // Fallible daralma (bkz. CAST_FLOAT_TO_INT_CHECKED); left: 0=throw, 1=null.
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "int(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset()
                      << (ins.left ? (std::string(" ") + IrColor::SoftTurkuaz() + "[null]" + IrColor::Reset()) : (std::string(" ") + IrColor::SoftTurkuaz() + "[throw]" + IrColor::Reset()));
            break;

        case Opcode::INT_TO_LONG:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "(longint)" << IrColor::Reset() << cs(ins.src);
            break;

        case Opcode::LONG_TO_INT_CHECKED:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "int(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset()
                      << (ins.left ? (std::string(" ") + IrColor::SoftTurkuaz() + "[null]" + IrColor::Reset()) : (std::string(" ") + IrColor::SoftTurkuaz() + "[throw]" + IrColor::Reset()));
            break;

        case Opcode::CAST_INT_TO_BYTE_CHECKED:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "byte(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset()
                      << (ins.left ? (std::string(" ") + IrColor::SoftTurkuaz() + "[null]" + IrColor::Reset()) : (std::string(" ") + IrColor::SoftTurkuaz() + "[throw]" + IrColor::Reset()));
            break;

        case Opcode::CAST_LONG_TO_STR:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "str(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset();
            break;

        case Opcode::CAST_STR_TO_LONG:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "long?(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset()
                      << (ins.left ? (std::string(" ") + IrColor::SoftTurkuaz() + "[null]" + IrColor::Reset()) : (std::string(" ") + IrColor::SoftTurkuaz() + "[throw]" + IrColor::Reset()));
            break;

        case Opcode::CAST_FLOAT32_TO_STR:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "str(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset();
            break;

        case Opcode::CAST_STR_TO_FLOAT32:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "float32?(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset()
                      << (ins.left ? (std::string(" ") + IrColor::SoftTurkuaz() + "[null]" + IrColor::Reset()) : (std::string(" ") + IrColor::SoftTurkuaz() + "[throw]" + IrColor::Reset()));
            break;

        case Opcode::CAST_FLOAT_TO_LONG_CHECKED:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "long(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset()
                      << (ins.left ? (std::string(" ") + IrColor::SoftTurkuaz() + "[null]" + IrColor::Reset()) : (std::string(" ") + IrColor::SoftTurkuaz() + "[throw]" + IrColor::Reset()));
            break;

        case Opcode::CAST_INT_TO_STR:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "str(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset();
            break;
        case Opcode::CAST_FLOAT_TO_STR:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "str(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset();
            break;
        case Opcode::CAST_BOOL_TO_STR:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "str(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset();
            break;
        case Opcode::CAST_STR_TO_INT:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "int?(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset()
                      << (ins.left ? (std::string(" ") + IrColor::SoftTurkuaz() + "[null]" + IrColor::Reset()) : (std::string(" ") + IrColor::SoftTurkuaz() + "[throw]" + IrColor::Reset()));
            break;
        case Opcode::CAST_STR_TO_FLOAT:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "float?(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset()
                      << (ins.left ? (std::string(" ") + IrColor::SoftTurkuaz() + "[null]" + IrColor::Reset()) : (std::string(" ") + IrColor::SoftTurkuaz() + "[throw]" + IrColor::Reset()));
            break;
        case Opcode::CAST_FLOAT_TO_INT_CHECKED:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "int(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset()
                      << (ins.left ? (std::string(" ") + IrColor::SoftTurkuaz() + "[null]" + IrColor::Reset()) : (std::string(" ") + IrColor::SoftTurkuaz() + "[throw]" + IrColor::Reset()));
            break;

        case Opcode::FNEG:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftMor() << "-" << IrColor::Reset() << cs(ins.src);
            break;

        case Opcode::F32NEG:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftMor() << "-f32" << IrColor::Reset() << cs(ins.src);
            break;

        case Opcode::LNEG:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftMor() << "-l" << IrColor::Reset() << cs(ins.src);
            break;

        case Opcode::STRUCT_NEW:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "struct<" << IrColor::Reset()
                      << IrColor::SoftYesil() << ins.functionName << IrColor::Reset()
                      << IrColor::SoftGri() << ">[" << IrColor::Reset() << ci(ins.intValue)
                      << IrColor::SoftGri() << " alan]" << IrColor::Reset();
            break;

        case Opcode::FIELD_GET:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << cs(ins.src) << IrColor::SoftGri() << "." << IrColor::Reset() << ci(ins.intValue);
            break;

        case Opcode::FIELD_SET:
            std::cout << cs(ins.dest) << IrColor::SoftGri() << "." << IrColor::Reset() << ci(ins.intValue)
                      << " " << IrColor::SoftGri() << "=" << IrColor::Reset() << " " << cs(ins.right);
            break;

        case Opcode::ARRAY_NEW:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "array[" << IrColor::Reset() << ci(ins.intValue) << IrColor::SoftGri() << "]" << IrColor::Reset();
            break;

        case Opcode::ARRAY_GET:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << cs(ins.left) << IrColor::SoftGri() << "[" << IrColor::Reset()
                      << cs(ins.right) << IrColor::SoftGri() << "]" << IrColor::Reset();
            break;

        case Opcode::ARRAY_SET:
            std::cout << cs(ins.dest) << IrColor::SoftGri() << "[" << IrColor::Reset()
                      << cs(ins.left) << IrColor::SoftGri() << "] =" << IrColor::Reset() << " " << cs(ins.right);
            break;

        case Opcode::ARRAY_LEN:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "len(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset();
            break;

        case Opcode::LOAD_GLOBAL:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "global[" << IrColor::Reset() << ci(ins.intValue) << IrColor::SoftGri() << "]" << IrColor::Reset();
            break;

        case Opcode::STORE_GLOBAL:
            std::cout << IrColor::SoftGri() << "global[" << IrColor::Reset() << ci(ins.intValue)
                      << IrColor::SoftGri() << "] =" << IrColor::Reset() << " " << cs(ins.src);
            break;

        case Opcode::LOAD_NULL:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftTurkuaz() << "null" << IrColor::Reset();
            break;

        case Opcode::LOAD_DECIMAL:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset() << " "
                      << IrColor::SoftTuruncu() << ins.decimalValue.toString() << IrColor::Reset()
                      << IrColor::SoftGri() << "d" << IrColor::Reset();
            break;

        case Opcode::INT_TO_DECIMAL:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "(decimal)" << IrColor::Reset() << cs(ins.src);
            break;

        case Opcode::FLOAT_TO_DECIMAL:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "(decimal)" << IrColor::Reset() << cs(ins.src);
            break;

        case Opcode::DNEG:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "-d" << IrColor::Reset() << " " << cs(ins.src);
            break;

        case Opcode::CAST_DECIMAL_TO_STR:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "str(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset();
            break;

        case Opcode::CAST_DECIMAL_TO_FLOAT:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "float(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset();
            break;

        case Opcode::CAST_DECIMAL_TO_INT:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "int(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset()
                      << (ins.left ? (std::string(" ") + IrColor::SoftTurkuaz() + "[null]" + IrColor::Reset()) : (std::string(" ") + IrColor::SoftTurkuaz() + "[throw]" + IrColor::Reset()));
            break;

        case Opcode::CAST_STR_TO_DECIMAL:
            std::cout << cs(ins.dest) << " " << IrColor::SoftGri() << "=" << IrColor::Reset()
                      << " " << IrColor::SoftGri() << "decimal?(" << IrColor::Reset() << cs(ins.src) << IrColor::SoftGri() << ")" << IrColor::Reset()
                      << (ins.left ? (std::string(" ") + IrColor::SoftTurkuaz() + "[null]" + IrColor::Reset()) : (std::string(" ") + IrColor::SoftTurkuaz() + "[throw]" + IrColor::Reset()));
            break;

        case Opcode::ENTER_TRY:
            std::cout << IrColor::SoftGri() << "err→" << IrColor::Reset() << cs(ins.dest)
                      << "  " << IrColor::SoftGri() << "catch→" << IrColor::Reset() << ci(ins.jumpTarget);
            break;

        case Opcode::LEAVE_TRY:
            // operand yok — bilinçli olarak boş case (N2)
            break;

        case Opcode::THROW:
            std::cout << cs(ins.src);
            break;

        case Opcode::RETURN:
            std::cout << IrColor::SoftGri() << "return" << IrColor::Reset() << " " << cs(ins.src);
            break;
        }

        if (ins.requiredCap)
            std::cout << " " << IrColor::SoftTurkuaz() << "[cap:" << capabilityName(*ins.requiredCap)
                      << "]" << IrColor::Reset();
        std::cout << "\n";
    }
    std::cout << "\n";
}
