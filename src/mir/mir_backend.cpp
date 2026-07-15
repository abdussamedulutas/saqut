// ============================================================================
// saQut MIR JIT — Fast-mode Codegen Gövdesi (Dilim 1, #80, MIRPLAN.md §4/§10)
//
// TEK dosya bu projede <mir.h>/<mir-gen.h> include eder (MIRPLAN.md §1).
// Programın TAMAMI (her fonksiyon) desteklenen opcode kümesinde değilse
// hiçbir şey derlenmez/çalıştırılmaz — kısmi JIT / sessiz VM'e düşme YOK
// (bkz. mir_backend.hpp başlık yorumu).
// ============================================================================

#include "mir/mir_backend.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <unordered_map>
#include <vector>

#include "mir/vendor/mir-gen.h"
#include "mir/vendor/mir.h"

namespace mir_backend {

namespace {

// ── print(int) trampoline'i — VM'in Interpreter::executeHostFunction'daki
// "print" özel-durumuyla AYNI davranışı üretir (Value::toString() bir Int
// için std::to_string(intValue) ile birebir aynı, ADR-024). ──────────────
extern "C" void rt_jit_print_int(int64_t v) {
    std::cout << v << "\n";
}

// Sıfıra bölme — VM'in pendingThrow_/catch mekanizması bu Dilim'de yok
// (ENTER_TRY/THROW opcode'ları zaten reddediliyor); bu yüzden yakalanamaz
// bir durum olarak ele alınır. Mesaj metni VM'deki E_DIVZERO mesajlarıyla
// aynı (interpreter.cpp) — gözlemlenen davranış (stderr + exit 1) eşleşir.
extern "C" void rt_jit_div_zero() {
    std::cerr << "runtime error: division by zero\n";
    std::exit(1);
}

extern "C" void rt_jit_mod_zero() {
    std::cerr << "runtime error: sifira bolme (mod)\n";
    std::exit(1);
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

// Programın TAMAMINI tarar; ilk desteklenmeyen opcode'da sebebiyle döner.
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
    }
    return true;
}

struct FuncEntry {
    // CALL hedefi olarak referanslanan item. MIR yalnızca TEK func'ı aynı
    // anda "açık" tutmaya izin verir (MIR_new_func -> ... -> MIR_finish_func
    // zorunlu, iç içe/erken açma hata verir) — bu yüzden ileri-referanslı
    // çağrılar (henüz gövdesi doldurulmamış bir fonksiyona çağrı) gerçek
    // func item'ı yerine bir MIR_new_forward ile çözülür; MIR aynı isimli
    // gerçek fonksiyon tanımlandığında bunu otomatik eşler (C'deki forward
    // declaration ile aynı fikir).
    MIR_item_t callRef;
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

    // src/profiling/ (--profile): "jit-warmup" — IR->MIR ceviri + gercek
    // native koda derleme (asagidaki MIR_gen dongusu dahil). Yalnizca
    // derlenmis main()'in CALISTIRILMASI (compiled() cagrisi) bu kapsamin
    // DISINDA — o "jit-exec" olarak ayri olculur. std::optional::reset()
    // ile RAII kapsamini compiled()'dan HEMEN once kapatiyoruz.
    std::optional<profiling::StageTimer::ScopedStage> profWarmup;
    profWarmup.emplace(profiler, "jit-warmup");

    MIR_context_t ctx = MIR_init();
    MIR_module_t  mod = MIR_new_module(ctx, "saqut_jit_dilim1");

    MIR_type_t resType = MIR_T_I64;

    // ── print/fatal-hata trampolinleri (dış C fonksiyonları) ────────────
    MIR_item_t printProto = MIR_new_proto(ctx, "print_proto", 0, nullptr, 1, MIR_T_I64, "v");
    MIR_item_t printImport = MIR_new_import(ctx, "rt_jit_print_int");
    MIR_item_t divZeroProto = MIR_new_proto(ctx, "divzero_proto", 0, nullptr, 0);
    MIR_item_t divZeroImport = MIR_new_import(ctx, "rt_jit_div_zero");
    MIR_item_t modZeroProto = MIR_new_proto(ctx, "modzero_proto", 0, nullptr, 0);
    MIR_item_t modZeroImport = MIR_new_import(ctx, "rt_jit_mod_zero");

    // ── Aşama 1: TÜM fonksiyonlar için proto + forward item önceden
    // oluştur (ileri referanslı CALL'ların çözülebilmesi için, MIRPLAN §2).
    // Protolar ve forward'lar "açık func" durumuna girmez — hepsi burada
    // güvenle önceden kurulabilir; gerçek func gövdeleri Aşama 2'de
    // TEK TEK açılıp kapatılır (MIR kısıtı: aynı anda yalnızca bir func açık
    // olabilir). ─────────────────────────────────────────────────────────
    std::unordered_map<std::string, FuncEntry> funcMap;
    for (auto& name : program.functionOrder) {
        IRFunction& fn = program.functions.at(name);

        std::vector<MIR_var_t>   argVars(static_cast<size_t>(fn.paramCount));
        std::vector<std::string> argNames(static_cast<size_t>(fn.paramCount));
        for (int i = 0; i < fn.paramCount; i++) {
            argNames[static_cast<size_t>(i)] = "arg" + std::to_string(i);
            argVars[static_cast<size_t>(i)]  = {MIR_T_I64, argNames[static_cast<size_t>(i)].c_str(), 0};
        }

        MIR_item_t proto = MIR_new_proto_arr(ctx, (name + "_proto").c_str(), 1, &resType,
                                              static_cast<size_t>(fn.paramCount), argVars.data());
        MIR_item_t forward = MIR_new_forward(ctx, name.c_str());

        funcMap[name] = FuncEntry{forward, proto, fn.paramCount};
    }

    // ── Aşama 2: her fonksiyonun gerçek gövdesini aç/doldur/kapat ────────
    for (auto& name : program.functionOrder) {
        IRFunction& fn      = program.functions.at(name);

        std::vector<MIR_var_t>   argVars(static_cast<size_t>(fn.paramCount));
        std::vector<std::string> argNames(static_cast<size_t>(fn.paramCount));
        for (int i = 0; i < fn.paramCount; i++) {
            argNames[static_cast<size_t>(i)] = "arg" + std::to_string(i);
            argVars[static_cast<size_t>(i)]  = {MIR_T_I64, argNames[static_cast<size_t>(i)].c_str(), 0};
        }
        MIR_item_t func = MIR_new_func_arr(ctx, name.c_str(), 1, &resType,
                                            static_cast<size_t>(fn.paramCount), argVars.data());
        funcMap.at(name).callRef = func;  // artık gerçek gövde var — forward yerine gerçek item

        size_t instrN = fn.instructions.size();

        // saQut slot'u -> MIR register'ı. Parametre slotları fonksiyonun
        // kendi biçimsel argümanları (MIR_reg ile bulunur); geri kalanı
        // yerel register (MIRPLAN §3: bu dilimde hepsi I64).
        std::vector<MIR_reg_t> regs(static_cast<size_t>(fn.slotCount));
        for (int i = 0; i < fn.paramCount; i++) {
            std::string argName = "arg" + std::to_string(i);
            regs[static_cast<size_t>(i)] = MIR_reg(ctx, argName.c_str(), func->u.func);
        }
        for (int i = fn.paramCount; i < fn.slotCount; i++) {
            std::string regName = "slot" + std::to_string(i);
            regs[static_cast<size_t>(i)] =
                MIR_new_func_reg(ctx, func->u.func, MIR_T_I64, regName.c_str());
        }

        // Her instruction indeksine bir label — JMP/JIF_* hedefleri bunlara
        // atlar (MIRPLAN §5, backpatch gerekmez: hedefler IR'de zaten kesin).
        std::vector<MIR_label_t> labelAt(instrN);
        for (size_t i = 0; i < instrN; i++) labelAt[i] = MIR_new_label(ctx);

        auto R = [&](int slot) { return MIR_new_reg_op(ctx, regs[static_cast<size_t>(slot)]); };

        for (size_t i = 0; i < instrN; i++) {
            MIR_append_insn(ctx, func, labelAt[i]);
            const Instruction& instr = fn.instructions[i];

            switch (instr.opcode) {
                case Opcode::LOAD_CONST:
                    MIR_append_insn(ctx, func,
                        MIR_new_insn(ctx, MIR_MOV, R(instr.dest), MIR_new_int_op(ctx, instr.intValue)));
                    break;
                case Opcode::LOAD_SLOT:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_MOV, R(instr.dest), R(instr.src)));
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
                case Opcode::LESS:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_LT, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::LESS_EQUAL:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_LE, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::GREATER:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_GT, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::GREATER_EQUAL:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_GE, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::EQUAL_EQUAL:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_EQ, R(instr.dest), R(instr.left), R(instr.right)));
                    break;
                case Opcode::NOT_EQUAL:
                    MIR_append_insn(ctx, func, MIR_new_insn(ctx, MIR_NE, R(instr.dest), R(instr.left), R(instr.right)));
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
                case Opcode::CALLHOST:
                    // isSupportedCallhost() zaten yalnizca tek-argumanli print'i
                    // gecirdi.
                    MIR_append_insn(ctx, func,
                        MIR_new_call_insn(ctx, 3, MIR_new_ref_op(ctx, printProto), MIR_new_ref_op(ctx, printImport),
                                          R(instr.argSlots[0])));
                    break;
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
    MIR_load_external(ctx, "rt_jit_print_int", reinterpret_cast<void*>(rt_jit_print_int));
    MIR_load_external(ctx, "rt_jit_div_zero", reinterpret_cast<void*>(rt_jit_div_zero));
    MIR_load_external(ctx, "rt_jit_mod_zero", reinterpret_cast<void*>(rt_jit_mod_zero));

    MIR_gen_init(ctx);
    // MIRPLAN.md §0: optimizasyon seviyesi determinizm gerekcesiyle
    // kisitlanmaz — MIR'in kendi benchmark'larinda gcc -O2'yle
    // kiyaslandigi seviye varsayilan.
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
