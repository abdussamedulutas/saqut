// ============================================================================
// saQut MIR JIT — Fast-mode Codegen Gövdesi (Dilim 1.5, #80, MIRPLAN.md §3/§4/§10)
//
// TEK dosya bu projede <mir.h>/<mir-gen.h> include eder (MIRPLAN.md §1).
// Programın TAMAMI (her fonksiyon) desteklenen opcode kümesinde değilse
// hiçbir şey derlenmez/çalıştırılmaz — kısmi JIT / sessiz VM'e düşme YOK
// (bkz. mir_backend.hpp başlık yorumu).
//
// KAPSAM (Dilim 1.5): int-skaler (Dilim 1) + FLOAT skaler. Register tipi
// IRFunction::slotTypes'tan seçilir (Float → MIR_T_D, diğerleri → MIR_T_I64,
// MIRPLAN §3). Slot türü Int/Float DIŞINDA bir şeyse (Ref/Str/Decimal/Date)
// program reddedilir — bu türler sonraki dilimlerde (kutulama + shadow stack).
// ============================================================================

#include "mir/mir_backend.hpp"

#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "mir/vendor/mir-gen.h"
#include "mir/vendor/mir.h"
#include "vm/value.hpp"   // Value tam tanımı — object.hpp'nin vector<Value> üyeleri için
#include "vm/object.hpp"  // StringObject — JIT string kutulama (ADR-037)

namespace mir_backend {

namespace {

// ── print(int) trampoline'i — VM'in Value::toString()'iyle birebir (ADR-024).
extern "C" void rt_jit_print_int(int64_t v) {
    std::cout << v << "\n";
}

// ── print(float) trampoline'i — VM'in Value::toString() Float dalıyla BİREBİR
// aynı biçim (setprecision(10) + nokta yoksa ".0"). Diferansiyel testin stdout
// eşleşmesi buna bağlı (value.hpp:94-102). ──────────────────────────────────
extern "C" void rt_jit_print_float(double v) {
    std::ostringstream oss;
    oss << std::setprecision(10) << v;
    std::string s = oss.str();
    if (s.find('.') == std::string::npos && s.find('e') == std::string::npos)
        s += ".0";
    std::cout << s << "\n";
}

// ── print(float32) trampoline'i — VM'in Value::toString() Float32 dalıyla BİREBİR
// (setprecision(9) + nokta yoksa ".0"). Argüman JIT'te MIR_T_F register olduğundan
// çağrı öncesi F2D ile double'a genişletilip buraya double gelir. ─────────────
extern "C" void rt_jit_print_float32(double v) {
    std::ostringstream oss;
    oss << std::setprecision(9) << (float)v;
    std::string s = oss.str();
    if (s.find('.') == std::string::npos && s.find('e') == std::string::npos)
        s += ".0";
    std::cout << s << "\n";
}

// ── print(string) trampoline'i (Dilim 3, ADR-037). Argüman, JIT register'ında
// pointer olarak taşınan bir StringObject*'tir (kutulanmış string). VM'in
// Value::toString() String dalı ham içeriği döndürür (value.hpp:104), print
// host'u "\n" ekler → burada data + "\n". Diferansiyel test buna bağlı. ──────
extern "C" void rt_jit_print_str(void* strObj) {
    std::cout << static_cast<StringObject*>(strObj)->data << "\n";
}

// ── Runtime string havuzu (Dilim 3). LOAD_STRING sabitleri derleme zamanı
// intern edilir (fonksiyon-local stringPool); CONCAT gibi ÇALIŞMA zamanı üretilen
// stringler burada tutulur. GC henüz JIT tarafını taramadığından (Dilim 2/§8)
// bunlar program-ömrü boyunca birikir ve tryCompileAndRunProgram sonunda toplu
// silinir (leak değil, ama döngüde çok concat = çok nesne — GC gelince çözülür).
// Tek-iş-parçacıklı varsayım (MIRPLAN §9: ileride thread-local). ──────────────
namespace {
std::vector<std::unique_ptr<StringObject>> g_jitRuntimeStrings;
}

// ── STRING_CONCAT trampolini — yeni (immutable, ADR-024) string üretir. ──────
extern "C" void* rt_jit_string_concat(void* a, void* b) {
    const std::string& sa = static_cast<StringObject*>(a)->data;
    const std::string& sb = static_cast<StringObject*>(b)->data;
    g_jitRuntimeStrings.push_back(std::make_unique<StringObject>(sa + sb));
    return g_jitRuntimeStrings.back().get();
}

// ── string ==/!= trampolini — ADR-023 istisnası: string eşitliği İÇERİK.
// VM'in EQUAL_EQUAL String dalıyla birebir (interpreter.cpp: stringValue==). ──
extern "C" int64_t rt_jit_string_eq(void* a, void* b) {
    return static_cast<StringObject*>(a)->data == static_cast<StringObject*>(b)->data ? 1 : 0;
}

// ── Cast trampolinleri (Dilim 3). Hepsi non-nullable hedef; başarısızlık
// uncaught (try/catch JIT'te yok) → rt_jit_cast_error, VM'in uncaught-throw
// mesaj gövdesiyle birebir (interpreter.cpp CAST_* dalları). ──────────────────
extern "C" void rt_jit_cast_error(const char* what) {
    std::cerr << "runtime error: " << what << "\n";  // div_zero deseniyle tutarlı
    std::exit(1);
}
extern "C" void* rt_jit_int_to_str(int64_t v) {
    g_jitRuntimeStrings.push_back(std::make_unique<StringObject>(std::to_string(v)));
    return g_jitRuntimeStrings.back().get();
}
extern "C" void* rt_jit_float_to_str(double v) {
    std::ostringstream oss;
    oss << v;  // VM CAST_FLOAT_TO_STR default precision (print_float'tan FARKLI — birebir)
    g_jitRuntimeStrings.push_back(std::make_unique<StringObject>(oss.str()));
    return g_jitRuntimeStrings.back().get();
}
extern "C" void* rt_jit_bool_to_str(int64_t v) {
    g_jitRuntimeStrings.push_back(std::make_unique<StringObject>(v ? "true" : "false"));
    return g_jitRuntimeStrings.back().get();
}
// ADR-040: longint (int64) → string. VM CAST_LONG_TO_STR ile birebir (to_string).
extern "C" void* rt_jit_long_to_str(int64_t v) {
    g_jitRuntimeStrings.push_back(std::make_unique<StringObject>(std::to_string(v)));
    return g_jitRuntimeStrings.back().get();
}
// ADR-040: float32 → string. VM CAST_FLOAT32_TO_STR ile birebir (setprecision 9).
// Argüman gerçek single (MIR_T_F) — F2D genişletmesi olmadan doğrudan.
extern "C" void* rt_jit_float32_to_str(float v) {
    std::ostringstream oss;
    oss << std::setprecision(9) << v;
    g_jitRuntimeStrings.push_back(std::make_unique<StringObject>(oss.str()));
    return g_jitRuntimeStrings.back().get();
}
extern "C" int64_t rt_jit_str_to_int(void* s) {
    const std::string& str = static_cast<StringObject*>(s)->data;
    try {
        size_t pos;
        long long v = std::stoll(str, &pos);
        if (pos != str.size()) throw std::invalid_argument("incomplete");
        if (v < INT_MIN || v > INT_MAX) throw std::out_of_range("overflow");
        return static_cast<int64_t>(static_cast<int>(v));
    } catch (...) {
        rt_jit_cast_error(("'" + str + "' cannot convert to int").c_str());
        return 0;  // ulaşılmaz (exit)
    }
}
extern "C" double rt_jit_str_to_float(void* s) {
    const std::string& str = static_cast<StringObject*>(s)->data;
    try {
        size_t pos;
        double v = std::stod(str, &pos);
        if (pos != str.size()) throw std::invalid_argument("incomplete");
        return v;
    } catch (...) {
        rt_jit_cast_error(("'" + str + "' cannot convert to float").c_str());
        return 0.0;  // ulaşılmaz
    }
}
extern "C" int64_t rt_jit_float_to_int_checked(double fv) {
    if (!std::isfinite(fv) || fv < static_cast<double>(INT_MIN) || fv > static_cast<double>(INT_MAX))
        rt_jit_cast_error("float value out of int range or NaN/Inf");
    return static_cast<int64_t>(static_cast<int>(fv));  // sıfıra kırp
}
extern "C" int64_t rt_jit_int_to_byte_checked(int64_t iv) {
    if (iv < 0 || iv > 255)
        rt_jit_cast_error(("integer value " + std::to_string(iv) +
                           " out of byte range (0-255)").c_str());
    return iv;
}

// ── Decimal trampolinleri (Dilim 3, ADR-037: decimal her zaman kutulu). Değerler
// g_jitDecimals havuzunda (program sonunda toplu silinir). Hata mesajları VM'in
// D* / CAST_*_DECIMAL dallarıyla birebir. ────────────────────────────────────
namespace {
std::vector<std::unique_ptr<DecimalObject>> g_jitDecimals;
DecimalValue& jitDV(void* p) { return static_cast<DecimalObject*>(p)->val; }
DecimalObject* jitBoxDecimal(const DecimalValue& v) {
    g_jitDecimals.push_back(std::make_unique<DecimalObject>(v));
    return g_jitDecimals.back().get();
}
}
extern "C" void* rt_jit_decimal_add(void* a, void* b) {
    auto r = DecimalValue::add(jitDV(a), jitDV(b));
    if (r.isOverflow()) rt_jit_cast_error("decimal overflow");
    return jitBoxDecimal(r);
}
extern "C" void* rt_jit_decimal_sub(void* a, void* b) {
    auto r = DecimalValue::sub(jitDV(a), jitDV(b));
    if (r.isOverflow()) rt_jit_cast_error("decimal overflow");
    return jitBoxDecimal(r);
}
extern "C" void* rt_jit_decimal_mul(void* a, void* b) {
    auto r = DecimalValue::mul(jitDV(a), jitDV(b));
    if (r.isOverflow()) rt_jit_cast_error("decimal overflow");
    return jitBoxDecimal(r);
}
extern "C" void* rt_jit_decimal_div(void* a, void* b) {
    if (jitDV(b).coeff == 0) rt_jit_cast_error("decimal division by zero");
    return jitBoxDecimal(DecimalValue::div(jitDV(a), jitDV(b)));
}
extern "C" void* rt_jit_decimal_mod(void* a, void* b) {
    if (jitDV(b).coeff == 0) rt_jit_cast_error("decimal modulo by zero");
    return jitBoxDecimal(DecimalValue::mod(jitDV(a), jitDV(b)));
}
extern "C" void* rt_jit_decimal_neg(void* a) { return jitBoxDecimal(DecimalValue::neg(jitDV(a))); }
extern "C" void* rt_jit_int_to_decimal(int64_t v)   { return jitBoxDecimal(DecimalValue::fromInt(v)); }
extern "C" void* rt_jit_float_to_decimal(double v)  { return jitBoxDecimal(DecimalValue::fromDouble(v)); }
extern "C" void* rt_jit_decimal_to_str(void* d) {
    g_jitRuntimeStrings.push_back(std::make_unique<StringObject>(jitDV(d).toString()));
    return g_jitRuntimeStrings.back().get();
}
extern "C" int64_t rt_jit_decimal_to_int(void* d) {
    DecimalValue t = DecimalValue::truncate(jitDV(d));
    if (t.coeff < INT_MIN || t.coeff > INT_MAX)
        rt_jit_cast_error("decimal value out of int range");
    return static_cast<int64_t>(static_cast<int>(t.coeff));
}
extern "C" double rt_jit_decimal_to_float(void* d) { return jitDV(d).toDouble(); }
extern "C" void* rt_jit_str_to_decimal(void* s) {
    const std::string& str = static_cast<StringObject*>(s)->data;
    try {
        return jitBoxDecimal(DecimalValue::fromString(str));
    } catch (...) {
        rt_jit_cast_error(("'" + str + "' cannot convert to decimal").c_str());
        return nullptr;  // ulaşılmaz
    }
}
extern "C" void rt_jit_print_decimal(void* d) { std::cout << jitDV(d).toString() << "\n"; }

// Sıfıra bölme — bu Dilim'de try/catch (ENTER_TRY/THROW) reddedildiğinden
// yakalanamaz; VM'de de aynı program uncaught throw ile sonlanırdı. Mesaj/çıkış
// VM davranışıyla eşleşir (interpreter.cpp E_DIVZERO).
extern "C" void rt_jit_div_zero() {
    std::cerr << "runtime error: division by zero\n";
    std::exit(1);
}

extern "C" void rt_jit_mod_zero() {
    std::cerr << "runtime error: sifira bolme (mod)\n";
    std::exit(1);
}

extern "C" void rt_jit_fdiv_zero() {
    std::cerr << "runtime error: float division by zero\n";
    std::exit(1);
}

// SlotType → MIR register tipi (MIRPLAN §3; ADR-040 genişletmesi).
//   Float   → MIR_T_D (64-bit double)
//   Float32 → MIR_T_F (32-bit single — gerçek precision, VM ile birebir)
//   LongInt → MIR_T_I64 (int gibi ama EXT32 yok, tam 64-bit)
//   diğer (Int/Str/Decimal/…) → MIR_T_I64
MIR_type_t mirType(SlotType t) {
    if (t == SlotType::Float)   return MIR_T_D;
    if (t == SlotType::Float32) return MIR_T_F;
    return MIR_T_I64;
}

// Bir fonksiyonun dönüş türü — ilk RETURN'ün src slot türünden (tip denetleyici
// tüm RETURN'lerin aynı türde olduğunu garanti eder; Dilim 1 void RETURN'ü
// zaten reddediyor).
SlotType retKind(const IRFunction& fn) {
    for (const auto& ins : fn.instructions)
        if (ins.opcode == Opcode::RETURN && ins.src >= 0 &&
            ins.src < static_cast<int>(fn.slotTypes.size()))
            return fn.slotTypes[static_cast<size_t>(ins.src)];
    return SlotType::Int;
}

// Slot türünü güvenli oku (tablo eksikse/aralık dışıysa Int).
SlotType slotKindOf(const IRFunction& fn, int slot) {
    if (slot >= 0 && slot < static_cast<int>(fn.slotTypes.size()))
        return fn.slotTypes[static_cast<size_t>(slot)];
    return SlotType::Int;
}

bool isSupportedCallhost(const Instruction& instr) {
    return instr.functionName == "print" && instr.argSlots.size() == 1;
}

bool opcodeSupported(const Instruction& instr) {
    switch (instr.opcode) {
        case Opcode::LOAD_CONST:
        case Opcode::LOAD_SLOT:
        // Dilim 3: string skaler (kutulu — pointer register'da taşınır, ADR-037)
        case Opcode::LOAD_STRING:
        case Opcode::STRING_CONCAT:
        // Dilim 3: cast (skaler). Nullable hedef (instr.left==1 → başarısızlıkta null)
        // JIT'te desteklenmez — null'un register temsili ayrı tasarım turu. Yalnızca
        // non-nullable hedef (başarısızlık = uncaught throw, VM ile aynı).
        case Opcode::CAST_INT_TO_STR:
        case Opcode::CAST_FLOAT_TO_STR:
        case Opcode::CAST_BOOL_TO_STR:
            return true;
        case Opcode::CAST_STR_TO_INT:
        case Opcode::CAST_STR_TO_FLOAT:
        case Opcode::CAST_FLOAT_TO_INT_CHECKED:
        case Opcode::CAST_INT_TO_BYTE_CHECKED:
        // ADR-040 fallible cast'ler — nullable hedef JIT'te desteklenmez
        case Opcode::CAST_STR_TO_LONG:
        case Opcode::CAST_STR_TO_FLOAT32:
        case Opcode::CAST_FLOAT_TO_LONG_CHECKED:
        case Opcode::LONG_TO_INT_CHECKED:
            return instr.left != 1;  // nullable hedef → reddet
        // Dilim 3: decimal (kutulu — pointer register'da, aritmetik runtime call)
        case Opcode::LOAD_DECIMAL:
        case Opcode::DADD: case Opcode::DSUB: case Opcode::DMUL:
        case Opcode::DDIV: case Opcode::DMOD: case Opcode::DNEG:
        case Opcode::INT_TO_DECIMAL: case Opcode::FLOAT_TO_DECIMAL:
        case Opcode::CAST_DECIMAL_TO_STR: case Opcode::CAST_DECIMAL_TO_FLOAT:
            return true;
        case Opcode::CAST_DECIMAL_TO_INT:
        case Opcode::CAST_STR_TO_DECIMAL:
            return instr.left != 1;  // nullable hedef → reddet
        case Opcode::ADD:
        case Opcode::SUB:
        case Opcode::MUL:
        case Opcode::DIV:
        case Opcode::MOD:
        case Opcode::BAND:
        case Opcode::BOR:
        case Opcode::BXOR:
        case Opcode::SHL:
        case Opcode::SHR:
        case Opcode::BNOT:
        // Dilim 1.5: float skaler
        case Opcode::LOAD_FLOAT:
        case Opcode::FADD:
        case Opcode::FSUB:
        case Opcode::FMUL:
        case Opcode::FDIV:
        case Opcode::FNEG:
        case Opcode::INT_TO_FLOAT:
        case Opcode::FLOAT_TO_INT:
        // ADR-040: longint (64-bit) aritmetik — EXT32'siz native MIR op
        case Opcode::LOAD_LONG:
        case Opcode::LADD: case Opcode::LSUB: case Opcode::LMUL:
        case Opcode::LDIV: case Opcode::LMOD: case Opcode::LNEG:
        case Opcode::LBAND: case Opcode::LBOR: case Opcode::LBXOR:
        case Opcode::LSHL: case Opcode::LSHR: case Opcode::LBNOT:
        case Opcode::INT_TO_LONG:
        // ADR-040: float32 (32-bit single) aritmetik — native MIR_T_F op
        case Opcode::LOAD_FLOAT32:
        case Opcode::F32ADD: case Opcode::F32SUB: case Opcode::F32MUL:
        case Opcode::F32DIV: case Opcode::F32NEG:
        case Opcode::INT_TO_FLOAT32: case Opcode::FLOAT32_TO_INT:
        case Opcode::FLOAT_TO_FLOAT32: case Opcode::FLOAT32_TO_FLOAT:
        // ADR-040: hatasız longint/float32 string cast'leri
        case Opcode::CAST_LONG_TO_STR:
        case Opcode::CAST_FLOAT32_TO_STR:
        // Karşılaştırmalar — operand türüne göre int/float varyantı codegen'de seçilir
        case Opcode::LESS:
        case Opcode::LESS_EQUAL:
        case Opcode::GREATER:
        case Opcode::GREATER_EQUAL:
        case Opcode::EQUAL_EQUAL:
        case Opcode::NOT_EQUAL:
        case Opcode::JMP:
        case Opcode::JIF_FALSE:
        case Opcode::JIF_TRUE:
        case Opcode::CALL:
            return true;
        case Opcode::RETURN:
            return instr.src >= 0;  // void RETURN (src=-1) bu dilimde yok
        case Opcode::CALLHOST:
            return isSupportedCallhost(instr);
        default:
            return false;
    }
}

// Programın TAMAMINI tarar: (a) her opcode desteklenmeli, (b) her slot türü
// Int/Float olmalı (Ref/Str/Decimal/Date register'a sığmaz — sonraki dilimler).
bool wholeProgramSupported(IRProgram& program, UnsupportedReason& outReason) {
    for (auto& name : program.functionOrder) {
        IRFunction& fn = program.functions.at(name);
        for (auto& instr : fn.instructions) {
            if (!opcodeSupported(instr)) {
                outReason.functionName = name;
                outReason.opcodeName   = opcodeName(instr.opcode);
                return false;
            }
            // String operandlı SIRALAMA (</<=/>/>=) JIT'te desteklenmez —
            // zaten frontend'de reddedilir (E003: "for string use only == and
            // !="), bu yalnızca savunmacı bir kalkan. Eşitlik (==/!=) İÇERİK
            // karşılaştırmasıdır (ADR-023 istisnası) ve codegen'de rt_jit_string_eq
            // runtime call'a çevrilir (native MIR_EQ pointer eşitliği YANLIŞ olurdu).
            switch (instr.opcode) {
                case Opcode::LESS:    case Opcode::LESS_EQUAL:
                case Opcode::GREATER: case Opcode::GREATER_EQUAL:
                    if (slotKindOf(fn, instr.left) == SlotType::Str ||
                        slotKindOf(fn, instr.right) == SlotType::Str) {
                        outReason.functionName = name;
                        outReason.opcodeName =
                            std::string(opcodeName(instr.opcode)) + " <string operand>";
                        return false;
                    }
                    break;
                default:
                    break;
            }
        }
        for (SlotType st : fn.slotTypes) {
            // Int/LongInt/Float/Float32 register-skaler; Str/Decimal kutulu
            // pointer (I64, ADR-037). Ref hâlâ sonraki dilimde (shadow stack).
            if (st != SlotType::Int && st != SlotType::LongInt &&
                st != SlotType::Float && st != SlotType::Float32 &&
                st != SlotType::Str && st != SlotType::Decimal) {
                outReason.functionName = name;
                outReason.opcodeName =
                    std::string("<desteklenmeyen slot turu: ") + slotTypeName(st) + ">";
                return false;
            }
        }
    }
    return true;
}

struct FuncEntry {
    MIR_item_t callRef;    // CALL hedefi (önce forward, gövde açılınca gerçek func)
    MIR_item_t protoItem;
    int        paramCount;
};

}  // namespace

bool tryCompileAndRunProgram(IRProgram& program, int& outExitCode,
                              UnsupportedReason& outReason,
                              profiling::StageTimer* profiler) {
    if (!wholeProgramSupported(program, outReason)) return false;
    if (program.findFunction("main") == nullptr) {
        outReason.functionName = "main";
        outReason.opcodeName   = "(fonksiyon bulunamadi)";
        return false;
    }

    // "jit-warmup" — IR->MIR çeviri + gerçek native derleme (MIR_gen dahil).
    // compiled() çağrısı bu kapsamın DIŞINDA ("jit-exec"); RAII kapsamı
    // compiled()'dan hemen önce reset() ile kapatılır.
    std::optional<profiling::StageTimer::ScopedStage> profWarmup;
    profWarmup.emplace(profiler, "jit-warmup");

    MIR_context_t ctx = MIR_init();
    MIR_module_t  mod = MIR_new_module(ctx, "saqut_jit_dilim1");

    // ── print/fatal-hata trampolinleri (dış C fonksiyonları) ────────────
    MIR_item_t printProto      = MIR_new_proto(ctx, "print_proto", 0, nullptr, 1, MIR_T_I64, "v");
    MIR_item_t printImport     = MIR_new_import(ctx, "rt_jit_print_int");
    MIR_item_t printFProto     = MIR_new_proto(ctx, "print_f_proto", 0, nullptr, 1, MIR_T_D, "v");
    MIR_item_t printFImport    = MIR_new_import(ctx, "rt_jit_print_float");
    // ADR-040: float32 print (arg F2D ile double'a genişletilir → MIR_T_D)
    MIR_item_t printF32Proto   = MIR_new_proto(ctx, "print_f32_proto", 0, nullptr, 1, MIR_T_D, "v");
    MIR_item_t printF32Import  = MIR_new_import(ctx, "rt_jit_print_float32");
    MIR_item_t printSProto     = MIR_new_proto(ctx, "print_s_proto", 0, nullptr, 1, MIR_T_I64, "v");
    MIR_item_t printSImport    = MIR_new_import(ctx, "rt_jit_print_str");
    // STRING_CONCAT / string ==,!= runtime call'ları (ret I64 pointer/bool, 2×I64 arg)
    MIR_type_t i64Ret          = MIR_T_I64;
    MIR_var_t  strConcatArgs[2] = {{MIR_T_I64, "a", 0}, {MIR_T_I64, "b", 0}};
    MIR_item_t concatProto     = MIR_new_proto_arr(ctx, "str_concat_proto", 1, &i64Ret, 2, strConcatArgs);
    MIR_item_t concatImport    = MIR_new_import(ctx, "rt_jit_string_concat");
    MIR_var_t  strEqArgs[2]     = {{MIR_T_I64, "a", 0}, {MIR_T_I64, "b", 0}};
    MIR_item_t strEqProto      = MIR_new_proto_arr(ctx, "str_eq_proto", 1, &i64Ret, 2, strEqArgs);
    MIR_item_t strEqImport     = MIR_new_import(ctx, "rt_jit_string_eq");
    // Cast trampolinleri (Dilim 3). ret I64 (pointer/int) veya D; arg I64/D.
    MIR_type_t dRet            = MIR_T_D;
    MIR_item_t castI2SProto    = MIR_new_proto(ctx, "cast_i2s_proto", 1, &i64Ret, 1, MIR_T_I64, "v");
    MIR_item_t castI2SImport   = MIR_new_import(ctx, "rt_jit_int_to_str");
    MIR_item_t castF2SProto    = MIR_new_proto(ctx, "cast_f2s_proto", 1, &i64Ret, 1, MIR_T_D, "v");
    MIR_item_t castF2SImport   = MIR_new_import(ctx, "rt_jit_float_to_str");
    MIR_item_t castB2SProto    = MIR_new_proto(ctx, "cast_b2s_proto", 1, &i64Ret, 1, MIR_T_I64, "v");
    MIR_item_t castB2SImport   = MIR_new_import(ctx, "rt_jit_bool_to_str");
    // ADR-040: longint→str (arg I64), float32→str (arg MIR_T_F). ret I64 pointer.
    MIR_item_t castL2SProto    = MIR_new_proto(ctx, "cast_l2s_proto", 1, &i64Ret, 1, MIR_T_I64, "v");
    MIR_item_t castL2SImport   = MIR_new_import(ctx, "rt_jit_long_to_str");
    MIR_item_t castF322SProto  = MIR_new_proto(ctx, "cast_f322s_proto", 1, &i64Ret, 1, MIR_T_F, "v");
    MIR_item_t castF322SImport = MIR_new_import(ctx, "rt_jit_float32_to_str");
    MIR_item_t castS2IProto    = MIR_new_proto(ctx, "cast_s2i_proto", 1, &i64Ret, 1, MIR_T_I64, "v");
    MIR_item_t castS2IImport   = MIR_new_import(ctx, "rt_jit_str_to_int");
    MIR_item_t castS2FProto    = MIR_new_proto(ctx, "cast_s2f_proto", 1, &dRet, 1, MIR_T_I64, "v");
    MIR_item_t castS2FImport   = MIR_new_import(ctx, "rt_jit_str_to_float");
    MIR_item_t castF2IProto    = MIR_new_proto(ctx, "cast_f2i_proto", 1, &i64Ret, 1, MIR_T_D, "v");
    MIR_item_t castF2IImport   = MIR_new_import(ctx, "rt_jit_float_to_int_checked");
    MIR_item_t castI2BProto    = MIR_new_proto(ctx, "cast_i2b_proto", 1, &i64Ret, 1, MIR_T_I64, "v");
    MIR_item_t castI2BImport   = MIR_new_import(ctx, "rt_jit_int_to_byte_checked");
    // Decimal trampolinleri (Dilim 3). Kutulu → I64 pointer. Binary I64,I64→I64;
    // unary I64→I64; float→dec D→I64; dec→float I64→D.
    MIR_var_t  decBinArgs[2]   = {{MIR_T_I64, "a", 0}, {MIR_T_I64, "b", 0}};
    MIR_item_t decBinProto     = MIR_new_proto_arr(ctx, "dec_bin_proto", 1, &i64Ret, 2, decBinArgs);
    MIR_item_t decAddImport    = MIR_new_import(ctx, "rt_jit_decimal_add");
    MIR_item_t decSubImport    = MIR_new_import(ctx, "rt_jit_decimal_sub");
    MIR_item_t decMulImport    = MIR_new_import(ctx, "rt_jit_decimal_mul");
    MIR_item_t decDivImport    = MIR_new_import(ctx, "rt_jit_decimal_div");
    MIR_item_t decModImport    = MIR_new_import(ctx, "rt_jit_decimal_mod");
    MIR_item_t decUnIProto     = MIR_new_proto(ctx, "dec_uni_proto", 1, &i64Ret, 1, MIR_T_I64, "v");
    MIR_item_t decNegImport    = MIR_new_import(ctx, "rt_jit_decimal_neg");
    MIR_item_t decI2DImport    = MIR_new_import(ctx, "rt_jit_int_to_decimal");
    MIR_item_t decToStrImport  = MIR_new_import(ctx, "rt_jit_decimal_to_str");
    MIR_item_t decToIntImport  = MIR_new_import(ctx, "rt_jit_decimal_to_int");
    MIR_item_t decS2DImport    = MIR_new_import(ctx, "rt_jit_str_to_decimal");
    MIR_item_t decFromFProto   = MIR_new_proto(ctx, "dec_fromf_proto", 1, &i64Ret, 1, MIR_T_D, "v");
    MIR_item_t decF2DImport    = MIR_new_import(ctx, "rt_jit_float_to_decimal");
    MIR_item_t decToFProto     = MIR_new_proto(ctx, "dec_tof_proto", 1, &dRet, 1, MIR_T_I64, "v");
    MIR_item_t decToFImport    = MIR_new_import(ctx, "rt_jit_decimal_to_float");
    MIR_item_t printDProto     = MIR_new_proto(ctx, "print_d_proto", 0, nullptr, 1, MIR_T_I64, "v");
    MIR_item_t printDImport    = MIR_new_import(ctx, "rt_jit_print_decimal");
    MIR_item_t divZeroProto    = MIR_new_proto(ctx, "divzero_proto", 0, nullptr, 0);
    MIR_item_t divZeroImport   = MIR_new_import(ctx, "rt_jit_div_zero");
    MIR_item_t modZeroProto    = MIR_new_proto(ctx, "modzero_proto", 0, nullptr, 0);
    MIR_item_t modZeroImport   = MIR_new_import(ctx, "rt_jit_mod_zero");
    MIR_item_t fdivZeroProto   = MIR_new_proto(ctx, "fdivzero_proto", 0, nullptr, 0);
    MIR_item_t fdivZeroImport  = MIR_new_import(ctx, "rt_jit_fdiv_zero");

    // ── String sabit havuzu (Dilim 3, ADR-037). LOAD_STRING derleme zamanında
    // string'i kutular; StringObject* pointer'ı native koda int sabiti olarak
    // gömülür (JIT in-process, pointer geçerli). intern tablosu aynı içeriği
    // tek nesneye indirger → döngüde tekrar kutulama/leak yok. Nesneler bu
    // fonksiyon kapsamı boyunca (native compiled() çağrısı dahil) yaşar.
    // NOT: AOT (#81) bu yolu runtime call'a (rt_intern_string + string_data)
    // çevirmeli — farklı process'te derleme-zamanı host pointer'ı gömülemez.
    std::vector<std::unique_ptr<StringObject>>     stringPool;
    std::unordered_map<std::string, StringObject*> internTable;
    auto internString = [&](const std::string& s) -> StringObject* {
        auto it = internTable.find(s);
        if (it != internTable.end()) return it->second;
        stringPool.push_back(std::make_unique<StringObject>(s));
        StringObject* obj = stringPool.back().get();
        internTable.emplace(s, obj);
        return obj;
    };

    // ── Aşama 1: TÜM fonksiyonlar için proto + forward (ileri-referanslı
    // CALL çözümü, MIRPLAN §2). Tip-imzalar slotTypes'tan (MIRPLAN §3). ──
    std::unordered_map<std::string, FuncEntry> funcMap;
    for (auto& name : program.functionOrder) {
        IRFunction& fn = program.functions.at(name);

        std::vector<MIR_var_t>   argVars(static_cast<size_t>(fn.paramCount));
        std::vector<std::string> argNames(static_cast<size_t>(fn.paramCount));
        for (int i = 0; i < fn.paramCount; i++) {
            argNames[static_cast<size_t>(i)] = "arg" + std::to_string(i);
            argVars[static_cast<size_t>(i)]  =
                {mirType(slotKindOf(fn, i)), argNames[static_cast<size_t>(i)].c_str(), 0};
        }

        MIR_type_t ret = mirType(retKind(fn));
        MIR_item_t proto = MIR_new_proto_arr(ctx, (name + "_proto").c_str(), 1, &ret,
                                              static_cast<size_t>(fn.paramCount), argVars.data());
        MIR_item_t forward = MIR_new_forward(ctx, name.c_str());

        funcMap[name] = FuncEntry{forward, proto, fn.paramCount};
    }

    // ── Aşama 2: her fonksiyonun gerçek gövdesini aç/doldur/kapat ────────
    for (auto& name : program.functionOrder) {
        IRFunction& fn = program.functions.at(name);

        std::vector<MIR_var_t>   argVars(static_cast<size_t>(fn.paramCount));
        std::vector<std::string> argNames(static_cast<size_t>(fn.paramCount));
        for (int i = 0; i < fn.paramCount; i++) {
            argNames[static_cast<size_t>(i)] = "arg" + std::to_string(i);
            argVars[static_cast<size_t>(i)]  =
                {mirType(slotKindOf(fn, i)), argNames[static_cast<size_t>(i)].c_str(), 0};
        }
        MIR_type_t ret = mirType(retKind(fn));
        MIR_item_t func = MIR_new_func_arr(ctx, name.c_str(), 1, &ret,
                                            static_cast<size_t>(fn.paramCount), argVars.data());
        funcMap.at(name).callRef = func;  // gövde açıldı — forward yerine gerçek item

        size_t instrN = fn.instructions.size();

        // saQut slot'u -> MIR register'ı. Parametreler biçimsel argümanlar
        // (MIR_reg); geri kalan yerel register. Tip slotTypes'tan (MIRPLAN §3).
        std::vector<MIR_reg_t> regs(static_cast<size_t>(fn.slotCount));
        for (int i = 0; i < fn.paramCount; i++) {
            std::string argName = "arg" + std::to_string(i);
            regs[static_cast<size_t>(i)] = MIR_reg(ctx, argName.c_str(), func->u.func);
        }
        for (int i = fn.paramCount; i < fn.slotCount; i++) {
            std::string regName = "slot" + std::to_string(i);
            regs[static_cast<size_t>(i)] =
                MIR_new_func_reg(ctx, func->u.func, mirType(slotKindOf(fn, i)), regName.c_str());
        }

        std::vector<MIR_label_t> labelAt(instrN);
        for (size_t i = 0; i < instrN; i++) labelAt[i] = MIR_new_label(ctx);

        auto R = [&](int slot) { return MIR_new_reg_op(ctx, regs[static_cast<size_t>(slot)]); };
        // Bir karşılaştırma/aritmetik talimatının FLOAT operand mı aldığını
        // (varyant seçimi için) statik slot türünden anla.
        auto floatOperands = [&](const Instruction& in) {
            return slotKindOf(fn, in.left) == SlotType::Float ||
                   slotKindOf(fn, in.right) == SlotType::Float;
        };
        // ADR-040: float32 operand mı (MIR single karşılaştırma varyantı için).
        auto float32Operands = [&](const Instruction& in) {
            return slotKindOf(fn, in.left) == SlotType::Float32 ||
                   slotKindOf(fn, in.right) == SlotType::Float32;
        };
        // Karşılaştırma MIR op'unu operand türüne göre seç: int/longint (I),
        // double (D), float32 (F). i=int, d=double, f=single opu.
        auto cmpOp = [&](const Instruction& in, MIR_insn_code_t iOp,
                         MIR_insn_code_t dOp, MIR_insn_code_t fOp) {
            if (floatOperands(in))   return dOp;
            if (float32Operands(in)) return fOp;
            return iOp;
        };
        // Eşitlik karşılaştırması string operand mı alıyor (içerik karşılaştırması
        // → rt_jit_string_eq runtime call, ADR-023). Tip denetleyici iki operandın
        // da string olmasını garanti eder (karışık yasak).
        auto stringOperands = [&](const Instruction& in) {
            return slotKindOf(fn, in.left) == SlotType::Str ||
                   slotKindOf(fn, in.right) == SlotType::Str;
        };

        for (size_t i = 0; i < instrN; i++) {
            MIR_append_insn(ctx, func, labelAt[i]);
            const Instruction& instr = fn.instructions[i];

            switch (instr.opcode) {
                case Opcode::LOAD_CONST:
                    MIR_append_insn(ctx, func,
                        MIR_new_insn(ctx, MIR_MOV, R(instr.dest), MIR_new_int_op(ctx, instr.intValue)));
                    break;
                case Opcode::LOAD_FLOAT:
                    MIR_append_insn(ctx, func,
                        MIR_new_insn(ctx, MIR_DMOV, R(instr.dest), MIR_new_double_op(ctx, instr.floatValue)));
                    break;
                case Opcode::LOAD_STRING: {
                    // Sabit string'i derleme zamanı kutula, pointer'ını int
                    // sabiti olarak register'a taşı (ADR-037: Str = I64 pointer).
                    StringObject* obj = internString(instr.stringValue);
                    MIR_append_insn(ctx, func,
                        MIR_new_insn(ctx, MIR_MOV, R(instr.dest),
                            MIR_new_int_op(ctx, reinterpret_cast<int64_t>(obj))));
                    break;
                }
                case Opcode::STRING_CONCAT:
                    // dest = rt_jit_string_concat(left, right) — yeni string kutusu.
                    MIR_append_insn(ctx, func,
                        MIR_new_call_insn(ctx, 5, MIR_new_ref_op(ctx, concatProto),
                            MIR_new_ref_op(ctx, concatImport),
                            R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                // ── Cast (Dilim 3) — hepsi dest = rt_jit_<cast>(src) runtime call ──
                case Opcode::CAST_INT_TO_STR:
                    MIR_append_insn(ctx, func, MIR_new_call_insn(ctx, 4, MIR_new_ref_op(ctx, castI2SProto), MIR_new_ref_op(ctx, castI2SImport), R(instr.dest), R(instr.src)));
                    break;
                case Opcode::CAST_FLOAT_TO_STR:
                    MIR_append_insn(ctx, func, MIR_new_call_insn(ctx, 4, MIR_new_ref_op(ctx, castF2SProto), MIR_new_ref_op(ctx, castF2SImport), R(instr.dest), R(instr.src)));
                    break;
                case Opcode::CAST_BOOL_TO_STR:
                    MIR_append_insn(ctx, func, MIR_new_call_insn(ctx, 4, MIR_new_ref_op(ctx, castB2SProto), MIR_new_ref_op(ctx, castB2SImport), R(instr.dest), R(instr.src)));
                    break;
                case Opcode::CAST_LONG_TO_STR:
                    MIR_append_insn(ctx, func, MIR_new_call_insn(ctx, 4, MIR_new_ref_op(ctx, castL2SProto), MIR_new_ref_op(ctx, castL2SImport), R(instr.dest), R(instr.src)));
                    break;
                case Opcode::CAST_FLOAT32_TO_STR:
                    MIR_append_insn(ctx, func, MIR_new_call_insn(ctx, 4, MIR_new_ref_op(ctx, castF322SProto), MIR_new_ref_op(ctx, castF322SImport), R(instr.dest), R(instr.src)));
                    break;
                case Opcode::CAST_STR_TO_INT:
                    MIR_append_insn(ctx, func, MIR_new_call_insn(ctx, 4, MIR_new_ref_op(ctx, castS2IProto), MIR_new_ref_op(ctx, castS2IImport), R(instr.dest), R(instr.src)));
                    break;
                case Opcode::CAST_STR_TO_FLOAT:
                    MIR_append_insn(ctx, func, MIR_new_call_insn(ctx, 4, MIR_new_ref_op(ctx, castS2FProto), MIR_new_ref_op(ctx, castS2FImport), R(instr.dest), R(instr.src)));
                    break;
                case Opcode::CAST_FLOAT_TO_INT_CHECKED:
                    MIR_append_insn(ctx, func, MIR_new_call_insn(ctx, 4, MIR_new_ref_op(ctx, castF2IProto), MIR_new_ref_op(ctx, castF2IImport), R(instr.dest), R(instr.src)));
                    break;
                case Opcode::CAST_INT_TO_BYTE_CHECKED:
                    MIR_append_insn(ctx, func, MIR_new_call_insn(ctx, 4, MIR_new_ref_op(ctx, castI2BProto), MIR_new_ref_op(ctx, castI2BImport), R(instr.dest), R(instr.src)));
                    break;
                // ── Decimal (Dilim 3) — kutulu; sabit derleme zamanı, aritmetik call ──
                case Opcode::LOAD_DECIMAL: {
                    DecimalObject* obj = jitBoxDecimal(instr.decimalValue);
                    MIR_append_insn(ctx, func,
                        MIR_new_insn(ctx, MIR_MOV, R(instr.dest),
                            MIR_new_int_op(ctx, reinterpret_cast<int64_t>(obj))));
                    break;
                }
                case Opcode::DADD:
                    MIR_append_insn(ctx, func, MIR_new_call_insn(ctx, 5, MIR_new_ref_op(ctx, decBinProto), MIR_new_ref_op(ctx, decAddImport), R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::DSUB:
                    MIR_append_insn(ctx, func, MIR_new_call_insn(ctx, 5, MIR_new_ref_op(ctx, decBinProto), MIR_new_ref_op(ctx, decSubImport), R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::DMUL:
                    MIR_append_insn(ctx, func, MIR_new_call_insn(ctx, 5, MIR_new_ref_op(ctx, decBinProto), MIR_new_ref_op(ctx, decMulImport), R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::DDIV:
                    MIR_append_insn(ctx, func, MIR_new_call_insn(ctx, 5, MIR_new_ref_op(ctx, decBinProto), MIR_new_ref_op(ctx, decDivImport), R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::DMOD:
                    MIR_append_insn(ctx, func, MIR_new_call_insn(ctx, 5, MIR_new_ref_op(ctx, decBinProto), MIR_new_ref_op(ctx, decModImport), R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::DNEG:
                    MIR_append_insn(ctx, func, MIR_new_call_insn(ctx, 4, MIR_new_ref_op(ctx, decUnIProto), MIR_new_ref_op(ctx, decNegImport), R(instr.dest), R(instr.src)));
                    break;
                case Opcode::INT_TO_DECIMAL:
                    MIR_append_insn(ctx, func, MIR_new_call_insn(ctx, 4, MIR_new_ref_op(ctx, decUnIProto), MIR_new_ref_op(ctx, decI2DImport), R(instr.dest), R(instr.src)));
                    break;
                case Opcode::FLOAT_TO_DECIMAL:
                    MIR_append_insn(ctx, func, MIR_new_call_insn(ctx, 4, MIR_new_ref_op(ctx, decFromFProto), MIR_new_ref_op(ctx, decF2DImport), R(instr.dest), R(instr.src)));
                    break;
                case Opcode::CAST_DECIMAL_TO_STR:
                    MIR_append_insn(ctx, func, MIR_new_call_insn(ctx, 4, MIR_new_ref_op(ctx, decUnIProto), MIR_new_ref_op(ctx, decToStrImport), R(instr.dest), R(instr.src)));
                    break;
                case Opcode::CAST_DECIMAL_TO_INT:
                    MIR_append_insn(ctx, func, MIR_new_call_insn(ctx, 4, MIR_new_ref_op(ctx, decUnIProto), MIR_new_ref_op(ctx, decToIntImport), R(instr.dest), R(instr.src)));
                    break;
                case Opcode::CAST_DECIMAL_TO_FLOAT:
                    MIR_append_insn(ctx, func, MIR_new_call_insn(ctx, 4, MIR_new_ref_op(ctx, decToFProto), MIR_new_ref_op(ctx, decToFImport), R(instr.dest), R(instr.src)));
                    break;
                case Opcode::CAST_STR_TO_DECIMAL:
                    MIR_append_insn(ctx, func, MIR_new_call_insn(ctx, 4, MIR_new_ref_op(ctx, decUnIProto), MIR_new_ref_op(ctx, decS2DImport), R(instr.dest), R(instr.src)));
                    break;
                case Opcode::LOAD_SLOT: {
                    // Float→DMOV, Float32→FMOV, diğerleri MOV (pointer/int/longint I64).
                    SlotType dk = slotKindOf(fn, instr.dest);
                    MIR_insn_code_t mv = dk == SlotType::Float   ? MIR_DMOV
                                       : dk == SlotType::Float32 ? MIR_FMOV
                                       : MIR_MOV;
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, mv, R(instr.dest), R(instr.src)));
                    break;
                }
                // ── int32 aritmetiği (#113/ADR-040) ──────────────────────
                // saQut `int` 32-bit; MIR "S"-op'ları alt 32-bit'te çalışır ama
                // sonucun üst yarısı TANIMSIZ (MIR.md §insns) → her sonucu EXT32
                // ile sign-extend edip register'ı normalize tutuyoruz. Böylece
                // taşma VM'in int32 wrap'iyle birebir eşleşir (ADR-032/038).
                case Opcode::ADD:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_ADDS, R(instr.dest), R(instr.left), R(instr.right)));
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_EXT32, R(instr.dest), R(instr.dest)));
                    break;
                case Opcode::SUB:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_SUBS, R(instr.dest), R(instr.left), R(instr.right)));
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_EXT32, R(instr.dest), R(instr.dest)));
                    break;
                case Opcode::MUL:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_MULS, R(instr.dest), R(instr.left), R(instr.right)));
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_EXT32, R(instr.dest), R(instr.dest)));
                    break;
                case Opcode::DIV: {
                    MIR_label_t okLabel   = MIR_new_label(ctx);
                    MIR_label_t doDiv     = MIR_new_label(ctx);
                    MIR_label_t doneLabel = MIR_new_label(ctx);
                    // /0 → yakalanabilir hata (VM ile aynı)
                    MIR_append_insn(ctx, func,
                        MIR_new_insn(ctx, MIR_BNE, MIR_new_label_op(ctx, okLabel), R(instr.right), MIR_new_int_op(ctx, 0)));
                    MIR_append_insn(ctx, func,
                        MIR_new_call_insn(ctx, 2, MIR_new_ref_op(ctx, divZeroProto), MIR_new_ref_op(ctx, divZeroImport)));
                    MIR_append_insn(ctx, func, okLabel);
                    // INT_MIN / -1 donanımda tuzak (#DE) → 2's-complement sonucu INT_MIN
                    MIR_append_insn(ctx, func,
                        MIR_new_insn(ctx, MIR_BNE, MIR_new_label_op(ctx, doDiv), R(instr.right), MIR_new_int_op(ctx, -1)));
                    MIR_append_insn(ctx, func,
                        MIR_new_insn(ctx, MIR_BNE, MIR_new_label_op(ctx, doDiv), R(instr.left), MIR_new_int_op(ctx, INT_MIN)));
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_MOV, R(instr.dest), MIR_new_int_op(ctx, INT_MIN)));
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_JMP, MIR_new_label_op(ctx, doneLabel)));
                    MIR_append_insn(ctx, func, doDiv);
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_DIVS, R(instr.dest), R(instr.left), R(instr.right)));
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_EXT32, R(instr.dest), R(instr.dest)));
                    MIR_append_insn(ctx, func, doneLabel);
                    break;
                }
                case Opcode::MOD: {
                    MIR_label_t okLabel   = MIR_new_label(ctx);
                    MIR_label_t doMod     = MIR_new_label(ctx);
                    MIR_label_t doneLabel = MIR_new_label(ctx);
                    MIR_append_insn(ctx, func,
                        MIR_new_insn(ctx, MIR_BNE, MIR_new_label_op(ctx, okLabel), R(instr.right), MIR_new_int_op(ctx, 0)));
                    MIR_append_insn(ctx, func,
                        MIR_new_call_insn(ctx, 2, MIR_new_ref_op(ctx, modZeroProto), MIR_new_ref_op(ctx, modZeroImport)));
                    MIR_append_insn(ctx, func, okLabel);
                    // INT_MIN % -1 → tuzak → 2's-complement sonucu 0
                    MIR_append_insn(ctx, func,
                        MIR_new_insn(ctx, MIR_BNE, MIR_new_label_op(ctx, doMod), R(instr.right), MIR_new_int_op(ctx, -1)));
                    MIR_append_insn(ctx, func,
                        MIR_new_insn(ctx, MIR_BNE, MIR_new_label_op(ctx, doMod), R(instr.left), MIR_new_int_op(ctx, INT_MIN)));
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_MOV, R(instr.dest), MIR_new_int_op(ctx, 0)));
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_JMP, MIR_new_label_op(ctx, doneLabel)));
                    MIR_append_insn(ctx, func, doMod);
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_MODS, R(instr.dest), R(instr.left), R(instr.right)));
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_EXT32, R(instr.dest), R(instr.dest)));
                    MIR_append_insn(ctx, func, doneLabel);
                    break;
                }
                // ── Float aritmetiği (Dilim 1.5) ────────────────────────
                case Opcode::FADD:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_DADD, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::FSUB:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_DSUB, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::FMUL:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_DMUL, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::FDIV: {
                    // VM float /0 → yakalanabilir hata; try/catch bu dilimde
                    // reddedildiğinden uncaught = fatal (VM'de de aynı).
                    MIR_label_t okLabel = MIR_new_label(ctx);
                    MIR_append_insn(ctx, func,
                        MIR_new_insn(ctx, MIR_DBNE, MIR_new_label_op(ctx, okLabel), R(instr.right), MIR_new_double_op(ctx, 0.0)));
                    MIR_append_insn(ctx, func,
                        MIR_new_call_insn(ctx, 2, MIR_new_ref_op(ctx, fdivZeroProto), MIR_new_ref_op(ctx, fdivZeroImport)));
                    MIR_append_insn(ctx, func, okLabel);
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_DDIV, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                }
                case Opcode::FNEG:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_DNEG, R(instr.dest), R(instr.src)));
                    break;
                case Opcode::INT_TO_FLOAT:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_I2D, R(instr.dest), R(instr.src)));
                    break;
                case Opcode::FLOAT_TO_INT:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_D2I, R(instr.dest), R(instr.src)));
                    break;
                // ── LongInt aritmetiği (ADR-040) — native 64-bit, EXT32 YOK ──
                case Opcode::LOAD_LONG:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_MOV, R(instr.dest), MIR_new_int_op(ctx, instr.int64Value)));
                    break;
                case Opcode::LADD:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_ADD, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::LSUB:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_SUB, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::LMUL:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_MUL, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::LDIV: {
                    MIR_label_t okLabel = MIR_new_label(ctx);
                    MIR_label_t doDiv = MIR_new_label(ctx);
                    MIR_label_t doneLabel = MIR_new_label(ctx);
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_BNE, MIR_new_label_op(ctx, okLabel), R(instr.right), MIR_new_int_op(ctx, 0)));
                    MIR_append_insn(ctx, func, MIR_new_call_insn(ctx, 2, MIR_new_ref_op(ctx, divZeroProto), MIR_new_ref_op(ctx, divZeroImport)));
                    MIR_append_insn(ctx, func, okLabel);
                    // INT64_MIN / -1 → tuzak → 2's-complement sonucu INT64_MIN
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_BNE, MIR_new_label_op(ctx, doDiv), R(instr.right), MIR_new_int_op(ctx, -1)));
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_BNE, MIR_new_label_op(ctx, doDiv), R(instr.left), MIR_new_int_op(ctx, INT64_MIN)));
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_MOV, R(instr.dest), MIR_new_int_op(ctx, INT64_MIN)));
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_JMP, MIR_new_label_op(ctx, doneLabel)));
                    MIR_append_insn(ctx, func, doDiv);
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_DIV, R(instr.dest), R(instr.left), R(instr.right)));
                    MIR_append_insn(ctx, func, doneLabel);
                    break;
                }
                case Opcode::LMOD: {
                    MIR_label_t okLabel = MIR_new_label(ctx);
                    MIR_label_t doMod = MIR_new_label(ctx);
                    MIR_label_t doneLabel = MIR_new_label(ctx);
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_BNE, MIR_new_label_op(ctx, okLabel), R(instr.right), MIR_new_int_op(ctx, 0)));
                    MIR_append_insn(ctx, func, MIR_new_call_insn(ctx, 2, MIR_new_ref_op(ctx, modZeroProto), MIR_new_ref_op(ctx, modZeroImport)));
                    MIR_append_insn(ctx, func, okLabel);
                    // INT64_MIN % -1 → tuzak → 0
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_BNE, MIR_new_label_op(ctx, doMod), R(instr.right), MIR_new_int_op(ctx, -1)));
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_BNE, MIR_new_label_op(ctx, doMod), R(instr.left), MIR_new_int_op(ctx, INT64_MIN)));
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_MOV, R(instr.dest), MIR_new_int_op(ctx, 0)));
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_JMP, MIR_new_label_op(ctx, doneLabel)));
                    MIR_append_insn(ctx, func, doMod);
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_MOD, R(instr.dest), R(instr.left), R(instr.right)));
                    MIR_append_insn(ctx, func, doneLabel);
                    break;
                }
                case Opcode::LNEG:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_NEG, R(instr.dest), R(instr.src)));
                    break;
                case Opcode::LBAND:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_AND, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::LBOR:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_OR, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::LBXOR:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_XOR, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::LSHL:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_LSH, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::LSHR:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_RSH, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::LBNOT:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_XOR, R(instr.dest), R(instr.src), MIR_new_int_op(ctx, -1)));
                    break;
                case Opcode::INT_TO_LONG:
                    // int (I64 register, sign-extended) → longint: kimlik kopya.
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_MOV, R(instr.dest), R(instr.src)));
                    break;
                // ── Float32 aritmetiği (ADR-040) — native single MIR_T_F op ──
                case Opcode::LOAD_FLOAT32:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_FMOV, R(instr.dest), MIR_new_float_op(ctx, (float)instr.floatValue)));
                    break;
                case Opcode::F32ADD:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_FADD, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::F32SUB:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_FSUB, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::F32MUL:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_FMUL, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::F32DIV: {
                    MIR_label_t okLabel = MIR_new_label(ctx);
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_FBNE, MIR_new_label_op(ctx, okLabel), R(instr.right), MIR_new_float_op(ctx, 0.0f)));
                    MIR_append_insn(ctx, func, MIR_new_call_insn(ctx, 2, MIR_new_ref_op(ctx, fdivZeroProto), MIR_new_ref_op(ctx, fdivZeroImport)));
                    MIR_append_insn(ctx, func, okLabel);
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_FDIV, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                }
                case Opcode::F32NEG:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_FNEG, R(instr.dest), R(instr.src)));
                    break;
                case Opcode::INT_TO_FLOAT32:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_I2F, R(instr.dest), R(instr.src)));
                    break;
                case Opcode::FLOAT32_TO_INT:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_F2I, R(instr.dest), R(instr.src)));
                    break;
                case Opcode::FLOAT_TO_FLOAT32:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_D2F, R(instr.dest), R(instr.src)));
                    break;
                case Opcode::FLOAT32_TO_FLOAT:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_F2D, R(instr.dest), R(instr.src)));
                    break;
                case Opcode::BAND:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_AND, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::BOR:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_OR, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::BXOR:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_XOR, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::SHL:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_LSHS, R(instr.dest), R(instr.left), R(instr.right)));
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_EXT32, R(instr.dest), R(instr.dest)));
                    break;
                case Opcode::SHR:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_RSHS, R(instr.dest), R(instr.left), R(instr.right)));
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_EXT32, R(instr.dest), R(instr.dest)));
                    break;
                case Opcode::BNOT:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_XOR, R(instr.dest), R(instr.src), MIR_new_int_op(ctx, -1)));
                    break;
                // ── Karşılaştırmalar — float operand ise D-varyantı ──────
                case Opcode::LESS:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, cmpOp(instr, MIR_LT, MIR_DLT, MIR_FLT), R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::LESS_EQUAL:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, cmpOp(instr, MIR_LE, MIR_DLE, MIR_FLE), R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::GREATER:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, cmpOp(instr, MIR_GT, MIR_DGT, MIR_FGT), R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::GREATER_EQUAL:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, cmpOp(instr, MIR_GE, MIR_DGE, MIR_FGE), R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::EQUAL_EQUAL:
                    if (stringOperands(instr)) {
                        // dest = rt_jit_string_eq(left, right)  (içerik, ADR-023)
                        MIR_append_insn(ctx, func,
                            MIR_new_call_insn(ctx, 5, MIR_new_ref_op(ctx, strEqProto),
                                MIR_new_ref_op(ctx, strEqImport),
                                R(instr.dest), R(instr.left), R(instr.right)));
                    } else {
                        MIR_append_insn(ctx, func, MIR_new_insn(ctx, cmpOp(instr, MIR_EQ, MIR_DEQ, MIR_FEQ), R(instr.dest), R(instr.left), R(instr.right)));
                    }
                    break;
                case Opcode::NOT_EQUAL:
                    if (stringOperands(instr)) {
                        // dest = !rt_jit_string_eq(left, right) → eq sonra XOR 1
                        MIR_append_insn(ctx, func,
                            MIR_new_call_insn(ctx, 5, MIR_new_ref_op(ctx, strEqProto),
                                MIR_new_ref_op(ctx, strEqImport),
                                R(instr.dest), R(instr.left), R(instr.right)));
                        MIR_append_insn(ctx, func,
                            MIR_new_insn(ctx, MIR_XOR, R(instr.dest), R(instr.dest), MIR_new_int_op(ctx, 1)));
                    } else {
                        MIR_append_insn(ctx, func, MIR_new_insn(ctx, cmpOp(instr, MIR_NE, MIR_DNE, MIR_FNE), R(instr.dest), R(instr.left), R(instr.right)));
                    }
                    break;
                case Opcode::JMP:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_JMP, MIR_new_label_op(ctx, labelAt[static_cast<size_t>(instr.jumpTarget)])));
                    break;
                case Opcode::JIF_FALSE:
                    MIR_append_insn(ctx, func,
                        MIR_new_insn(ctx, MIR_BF, MIR_new_label_op(ctx, labelAt[static_cast<size_t>(instr.jumpTarget)]), R(instr.cond)));
                    break;
                case Opcode::JIF_TRUE:
                    MIR_append_insn(ctx, func,
                        MIR_new_insn(ctx, MIR_BT, MIR_new_label_op(ctx, labelAt[static_cast<size_t>(instr.jumpTarget)]), R(instr.cond)));
                    break;
                case Opcode::CALL: {
                    const FuncEntry& callee = funcMap.at(instr.functionName);
                    std::vector<MIR_op_t> ops;
                    ops.push_back(MIR_new_ref_op(ctx, callee.protoItem));
                    ops.push_back(MIR_new_ref_op(ctx, callee.callRef));
                    ops.push_back(R(instr.dest));
                    for (int argSlot : instr.argSlots) ops.push_back(R(argSlot));
                    MIR_append_insn(ctx, func,
                        MIR_new_insn_arr(ctx, MIR_CALL, ops.size(), ops.data()));
                    break;
                }
                case Opcode::CALLHOST: {
                    // isSupportedCallhost() yalnizca tek-argumanli print'i gecirdi.
                    // Argüman türüne göre int/float/string trampolinini seç.
                    int      a  = instr.argSlots[0];
                    SlotType at = slotKindOf(fn, a);
                    if (at == SlotType::Float)
                        MIR_append_insn(ctx, func,
                            MIR_new_call_insn(ctx, 3, MIR_new_ref_op(ctx, printFProto), MIR_new_ref_op(ctx, printFImport), R(a)));
                    else if (at == SlotType::Float32) {
                        // float32 argümanı F2D ile double'a genişletilip print_f32'ye
                        // geçilir (VM float32 toString biçimi trampolinde uygulanır).
                        static int f32TmpCounter = 0;
                        std::string tmpName = "f32print" + std::to_string(f32TmpCounter++);
                        MIR_reg_t tmp = MIR_new_func_reg(ctx, func->u.func, MIR_T_D, tmpName.c_str());
                        MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_F2D, MIR_new_reg_op(ctx, tmp), R(a)));
                        MIR_append_insn(ctx, func,
                            MIR_new_call_insn(ctx, 3, MIR_new_ref_op(ctx, printF32Proto), MIR_new_ref_op(ctx, printF32Import), MIR_new_reg_op(ctx, tmp)));
                    }
                    else if (at == SlotType::Str)
                        MIR_append_insn(ctx, func,
                            MIR_new_call_insn(ctx, 3, MIR_new_ref_op(ctx, printSProto), MIR_new_ref_op(ctx, printSImport), R(a)));
                    else if (at == SlotType::Decimal)
                        MIR_append_insn(ctx, func,
                            MIR_new_call_insn(ctx, 3, MIR_new_ref_op(ctx, printDProto), MIR_new_ref_op(ctx, printDImport), R(a)));
                    else
                        MIR_append_insn(ctx, func,
                            MIR_new_call_insn(ctx, 3, MIR_new_ref_op(ctx, printProto), MIR_new_ref_op(ctx, printImport), R(a)));
                    break;
                }
                case Opcode::RETURN:
                    MIR_append_insn(ctx, func, MIR_new_ret_insn(ctx, 1, R(instr.src)));
                    break;
                default:
                    // opcodeSupported() yukarida zaten eledi.
                    break;
            }
        }

        MIR_finish_func(ctx);
    }

    MIR_finish_module(ctx);
    MIR_load_module(ctx, mod);
    MIR_load_external(ctx, "rt_jit_print_int",   reinterpret_cast<void*>(rt_jit_print_int));
    MIR_load_external(ctx, "rt_jit_print_float", reinterpret_cast<void*>(rt_jit_print_float));
    MIR_load_external(ctx, "rt_jit_print_float32", reinterpret_cast<void*>(rt_jit_print_float32));
    MIR_load_external(ctx, "rt_jit_print_str",   reinterpret_cast<void*>(rt_jit_print_str));
    MIR_load_external(ctx, "rt_jit_long_to_str",    reinterpret_cast<void*>(rt_jit_long_to_str));
    MIR_load_external(ctx, "rt_jit_float32_to_str", reinterpret_cast<void*>(rt_jit_float32_to_str));
    MIR_load_external(ctx, "rt_jit_string_concat", reinterpret_cast<void*>(rt_jit_string_concat));
    MIR_load_external(ctx, "rt_jit_string_eq",     reinterpret_cast<void*>(rt_jit_string_eq));
    MIR_load_external(ctx, "rt_jit_int_to_str",           reinterpret_cast<void*>(rt_jit_int_to_str));
    MIR_load_external(ctx, "rt_jit_float_to_str",         reinterpret_cast<void*>(rt_jit_float_to_str));
    MIR_load_external(ctx, "rt_jit_bool_to_str",          reinterpret_cast<void*>(rt_jit_bool_to_str));
    MIR_load_external(ctx, "rt_jit_str_to_int",           reinterpret_cast<void*>(rt_jit_str_to_int));
    MIR_load_external(ctx, "rt_jit_str_to_float",         reinterpret_cast<void*>(rt_jit_str_to_float));
    MIR_load_external(ctx, "rt_jit_float_to_int_checked", reinterpret_cast<void*>(rt_jit_float_to_int_checked));
    MIR_load_external(ctx, "rt_jit_int_to_byte_checked",  reinterpret_cast<void*>(rt_jit_int_to_byte_checked));
    MIR_load_external(ctx, "rt_jit_decimal_add", reinterpret_cast<void*>(rt_jit_decimal_add));
    MIR_load_external(ctx, "rt_jit_decimal_sub", reinterpret_cast<void*>(rt_jit_decimal_sub));
    MIR_load_external(ctx, "rt_jit_decimal_mul", reinterpret_cast<void*>(rt_jit_decimal_mul));
    MIR_load_external(ctx, "rt_jit_decimal_div", reinterpret_cast<void*>(rt_jit_decimal_div));
    MIR_load_external(ctx, "rt_jit_decimal_mod", reinterpret_cast<void*>(rt_jit_decimal_mod));
    MIR_load_external(ctx, "rt_jit_decimal_neg", reinterpret_cast<void*>(rt_jit_decimal_neg));
    MIR_load_external(ctx, "rt_jit_int_to_decimal",   reinterpret_cast<void*>(rt_jit_int_to_decimal));
    MIR_load_external(ctx, "rt_jit_float_to_decimal", reinterpret_cast<void*>(rt_jit_float_to_decimal));
    MIR_load_external(ctx, "rt_jit_decimal_to_str",   reinterpret_cast<void*>(rt_jit_decimal_to_str));
    MIR_load_external(ctx, "rt_jit_decimal_to_int",   reinterpret_cast<void*>(rt_jit_decimal_to_int));
    MIR_load_external(ctx, "rt_jit_decimal_to_float", reinterpret_cast<void*>(rt_jit_decimal_to_float));
    MIR_load_external(ctx, "rt_jit_str_to_decimal",   reinterpret_cast<void*>(rt_jit_str_to_decimal));
    MIR_load_external(ctx, "rt_jit_print_decimal",    reinterpret_cast<void*>(rt_jit_print_decimal));
    MIR_load_external(ctx, "rt_jit_div_zero",    reinterpret_cast<void*>(rt_jit_div_zero));
    MIR_load_external(ctx, "rt_jit_mod_zero",    reinterpret_cast<void*>(rt_jit_mod_zero));
    MIR_load_external(ctx, "rt_jit_fdiv_zero",   reinterpret_cast<void*>(rt_jit_fdiv_zero));

    MIR_gen_init(ctx);
    // MIRPLAN.md §0: optimizasyon seviyesi determinizm gerekcesiyle
    // kisitlanmaz — MIR'in gcc -O2'yle kiyaslandigi seviye varsayilan.
    MIR_gen_set_optimize_level(ctx, 2);
    MIR_link(ctx, MIR_set_gen_interface, nullptr);

    // Tum fonksiyonlari onceden JIT'le (lazy-gen'e guvenmiyoruz — cam kutu
    // ilkesi: derleme zamani tumuyle burada belirlenmis olsun).
    void* mainPtr = nullptr;
    for (auto& name : program.functionOrder) {
        void* p = MIR_gen(ctx, funcMap.at(name).callRef);
        if (name == "main") mainPtr = p;
    }

    profWarmup.reset();  // "jit-warmup" burada biter

    using SaqutMainFn = int64_t (*)(void);
    auto    compiled = reinterpret_cast<SaqutMainFn>(mainPtr);
    int64_t nativeResult;
    {
        profiling::StageTimer::ScopedStage profExec(profiler, "jit-exec");
        nativeResult = compiled();
    }

    MIR_gen_finish(ctx);
    MIR_finish(ctx);

    // Çalışma-zamanı üretilen string/decimal nesnelerini topla — native kod bitti,
    // pointer'lara artık erişilmiyor (GC Dilim 2/§8'e kadar elle temizlik).
    g_jitRuntimeStrings.clear();
    g_jitDecimals.clear();

    outExitCode = static_cast<int>(nativeResult);
    return true;
}

}  // namespace mir_backend
