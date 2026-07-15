// ============================================================================
// saQut MIR JIT — Fast-mode Codegen Gövdesi (Dilim 0, #80, MIRPLAN.md §4/§10)
//
// TEK dosya bu projede <mir.h>/<mir-gen.h> include eder (MIRPLAN.md §1).
// Opcode -> MIR eşlemesi MIRPLAN.md §4'teki tabloya uyar; Dilim 0 yalnızca
// LOAD_CONST/ADD/SUB/MUL/RETURN satırlarını uygular.
// ============================================================================

#include "mir/mir_backend.hpp"

#include <cstdint>
#include <vector>

#include "mir/vendor/mir-gen.h"
#include "mir/vendor/mir.h"

namespace mir_backend {

namespace {

bool opcodeSupported(Opcode op) {
    switch (op) {
        case Opcode::LOAD_CONST:
        case Opcode::ADD:
        case Opcode::SUB:
        case Opcode::MUL:
        case Opcode::RETURN:
            return true;
        default:
            return false;
    }
}

}  // namespace

bool tryCompileAndRun(const IRFunction& fn, int& outResult, std::string& errorOut) {
    if (fn.paramCount != 0) {
        errorOut = "MIR Dilim 0: yalnızca parametresiz fonksiyonlar destekleniyor";
        return false;
    }
    for (const auto& instr : fn.instructions) {
        if (!opcodeSupported(instr.opcode)) {
            errorOut = std::string("MIR Dilim 0: desteklenmeyen opcode: ") +
                       opcodeName(instr.opcode);
            return false;
        }
    }

    MIR_context_t ctx = MIR_init();

    MIR_module_t mod = MIR_new_module(ctx, "saqut_jit_dilim0");

    MIR_type_t resType = MIR_T_I64;
    MIR_item_t  func    = MIR_new_func(ctx, "saqut_fn", 1, &resType, 0);

    // saQut slot'ları -> MIR local register'ları. Dilim 0'da hepsi I64
    // (MIRPLAN.md §3 — skaler int/bool/byte/date tek register'da taşınır;
    // string/decimal/ref kutulama bu dilimde yok, o opcode'lar zaten elendi).
    std::vector<MIR_reg_t> regs(static_cast<size_t>(fn.slotCount));
    for (int i = 0; i < fn.slotCount; i++) {
        std::string regName = "slot" + std::to_string(i);
        regs[static_cast<size_t>(i)] =
            MIR_new_func_reg(ctx, func->u.func, MIR_T_I64, regName.c_str());
    }

    for (const auto& instr : fn.instructions) {
        switch (instr.opcode) {
            case Opcode::LOAD_CONST:
                MIR_append_insn(
                    ctx, func,
                    MIR_new_insn(ctx, MIR_MOV, MIR_new_reg_op(ctx, regs[static_cast<size_t>(instr.dest)]),
                                 MIR_new_int_op(ctx, instr.intValue)));
                break;
            case Opcode::ADD:
                MIR_append_insn(
                    ctx, func,
                    MIR_new_insn(ctx, MIR_ADD, MIR_new_reg_op(ctx, regs[static_cast<size_t>(instr.dest)]),
                                 MIR_new_reg_op(ctx, regs[static_cast<size_t>(instr.left)]),
                                 MIR_new_reg_op(ctx, regs[static_cast<size_t>(instr.right)])));
                break;
            case Opcode::SUB:
                MIR_append_insn(
                    ctx, func,
                    MIR_new_insn(ctx, MIR_SUB, MIR_new_reg_op(ctx, regs[static_cast<size_t>(instr.dest)]),
                                 MIR_new_reg_op(ctx, regs[static_cast<size_t>(instr.left)]),
                                 MIR_new_reg_op(ctx, regs[static_cast<size_t>(instr.right)])));
                break;
            case Opcode::MUL:
                MIR_append_insn(
                    ctx, func,
                    MIR_new_insn(ctx, MIR_MUL, MIR_new_reg_op(ctx, regs[static_cast<size_t>(instr.dest)]),
                                 MIR_new_reg_op(ctx, regs[static_cast<size_t>(instr.left)]),
                                 MIR_new_reg_op(ctx, regs[static_cast<size_t>(instr.right)])));
                break;
            case Opcode::RETURN:
                MIR_append_insn(
                    ctx, func,
                    MIR_new_ret_insn(ctx, 1, MIR_new_reg_op(ctx, regs[static_cast<size_t>(instr.src)])));
                break;
            default:
                // opcodeSupported() yukarıda zaten eledi — buraya düşülmez.
                break;
        }
    }

    MIR_finish_func(ctx);
    MIR_finish_module(ctx);

    MIR_load_module(ctx, mod);

    MIR_gen_init(ctx);
    // MIRPLAN.md §0: optimizasyon seviyesi determinizm gerekçesiyle
    // kısıtlanmaz — MIR'in kendi benchmark'larında gcc -O2'yle
    // kıyaslandığı seviye varsayılan.
    MIR_gen_set_optimize_level(ctx, 2);
    MIR_link(ctx, MIR_set_gen_interface, nullptr);

    using SaqutFn        = int64_t (*)(void);
    auto    compiled     = reinterpret_cast<SaqutFn>(MIR_gen(ctx, func));
    int64_t nativeResult = compiled();

    MIR_gen_finish(ctx);
    MIR_finish(ctx);

    outResult = static_cast<int>(nativeResult);
    return true;
}

}  // namespace mir_backend
