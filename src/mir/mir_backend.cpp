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

#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "mir/vendor/mir-gen.h"
#include "mir/vendor/mir.h"

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

// SlotType → MIR register tipi (MIRPLAN §3). Dilim 1.5'te yalnızca Int/Float
// buraya ulaşır (diğerleri wholeProgramSupported'ta reddedilir).
MIR_type_t mirType(SlotType t) {
    return t == SlotType::Float ? MIR_T_D : MIR_T_I64;
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
        }
        for (SlotType st : fn.slotTypes) {
            if (st != SlotType::Int && st != SlotType::Float) {
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
    MIR_item_t divZeroProto    = MIR_new_proto(ctx, "divzero_proto", 0, nullptr, 0);
    MIR_item_t divZeroImport   = MIR_new_import(ctx, "rt_jit_div_zero");
    MIR_item_t modZeroProto    = MIR_new_proto(ctx, "modzero_proto", 0, nullptr, 0);
    MIR_item_t modZeroImport   = MIR_new_import(ctx, "rt_jit_mod_zero");
    MIR_item_t fdivZeroProto   = MIR_new_proto(ctx, "fdivzero_proto", 0, nullptr, 0);
    MIR_item_t fdivZeroImport  = MIR_new_import(ctx, "rt_jit_fdiv_zero");

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
                case Opcode::LOAD_SLOT:
                    // Float slot kopyası DMOV, diğerleri MOV (pointer/int I64).
                    MIR_append_insn(ctx, func,
                        MIR_new_insn(ctx,
                            slotKindOf(fn, instr.dest) == SlotType::Float ? MIR_DMOV : MIR_MOV,
                            R(instr.dest), R(instr.src)));
                    break;
                case Opcode::ADD:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_ADD, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::SUB:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_SUB, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::MUL:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_MUL, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::DIV: {
                    MIR_label_t okLabel = MIR_new_label(ctx);
                    MIR_append_insn(ctx, func,
                        MIR_new_insn(ctx, MIR_BNE, MIR_new_label_op(ctx, okLabel), R(instr.right), MIR_new_int_op(ctx, 0)));
                    MIR_append_insn(ctx, func,
                        MIR_new_call_insn(ctx, 2, MIR_new_ref_op(ctx, divZeroProto), MIR_new_ref_op(ctx, divZeroImport)));
                    MIR_append_insn(ctx, func, okLabel);
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_DIV, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                }
                case Opcode::MOD: {
                    MIR_label_t okLabel = MIR_new_label(ctx);
                    MIR_append_insn(ctx, func,
                        MIR_new_insn(ctx, MIR_BNE, MIR_new_label_op(ctx, okLabel), R(instr.right), MIR_new_int_op(ctx, 0)));
                    MIR_append_insn(ctx, func,
                        MIR_new_call_insn(ctx, 2, MIR_new_ref_op(ctx, modZeroProto), MIR_new_ref_op(ctx, modZeroImport)));
                    MIR_append_insn(ctx, func, okLabel);
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_MOD, R(instr.dest), R(instr.left), R(instr.right)));
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
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_LSH, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::SHR:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_RSH, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::BNOT:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_XOR, R(instr.dest), R(instr.src), MIR_new_int_op(ctx, -1)));
                    break;
                // ── Karşılaştırmalar — float operand ise D-varyantı ──────
                case Opcode::LESS:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, floatOperands(instr) ? MIR_DLT : MIR_LT, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::LESS_EQUAL:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, floatOperands(instr) ? MIR_DLE : MIR_LE, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::GREATER:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, floatOperands(instr) ? MIR_DGT : MIR_GT, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::GREATER_EQUAL:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, floatOperands(instr) ? MIR_DGE : MIR_GE, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::EQUAL_EQUAL:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, floatOperands(instr) ? MIR_DEQ : MIR_EQ, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::NOT_EQUAL:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, floatOperands(instr) ? MIR_DNE : MIR_NE, R(instr.dest), R(instr.left), R(instr.right)));
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
                    // Argüman türüne göre int/float trampolinini seç.
                    int a = instr.argSlots[0];
                    if (slotKindOf(fn, a) == SlotType::Float)
                        MIR_append_insn(ctx, func,
                            MIR_new_call_insn(ctx, 3, MIR_new_ref_op(ctx, printFProto), MIR_new_ref_op(ctx, printFImport), R(a)));
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

    outExitCode = static_cast<int>(nativeResult);
    return true;
}

}  // namespace mir_backend
