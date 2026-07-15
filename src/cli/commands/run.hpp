// ============================================================================
// saQut CLI — run komutu
//
// Pipeline: ModuleLoader → 3-geçiş SymbolCollect → TypeCheck → Opt → IRGen → VM
// ============================================================================

#ifndef SAQUT_CLI_RUN
#define SAQUT_CLI_RUN

#include <iostream>
#include "cli/args.hpp"
#include "module/module_loader.hpp"
#include "symbol/symbol_table.hpp"
#include "symbol/symbol_collector.hpp"
#include "semantic/type_checker.hpp"
#include "semantic/structural_validator.hpp"
#include "diagnostic/diagnostic_engine.hpp"
#include "core/config.hpp"
#include "core/module_registry.hpp"
#include "opt/optimization_manager.hpp"
#include "ir/ir_generator.hpp"
#include "vm/interpreter.hpp"
#include "mir/mir_backend.hpp"
#include "profiling/stage_timer.hpp"

inline int cmdRun(const CliArgs& args) {
    std::string filePath = inputFilePath(args);
    if (filePath.empty()) { std::cerr << "error: no input file\n"; return 1; }

    // src/profiling/ (--profile): args.profile false ise timer kullanılmaz,
    // ScopedStage'ler no-op kalır (StageTimer::ScopedStage tasarımı gereği).
    profiling::StageTimer  stageTimer;
    profiling::StageTimer* profilerPtr = args.profile ? &stageTimer : nullptr;

    // ── Aşama 1: Tüm modülleri yükle (BFS parse) ─────────────────────────
    ModuleRegistry   registry;
    DiagnosticEngine diag;
    ModuleLoader     loader(registry, diag);
    loader.setProfiler(profilerPtr);
    ModuleGraph      graph = loader.load(filePath);

    if (diag.hasErrors()) {
        diag.printAll(std::cerr);
        return 1;
    }

    // ── Aşama 2: 3-geçiş sembol toplama + import doğrulama ───────────────
    SymbolTable symbolTable;
    SymbolCollector collector(symbolTable, diag, args.allowedCaps);
    collector.collectModuleGraph(graph);

    if (diag.hasErrors()) {
        std::cerr << "compilation errors, cannot run program:\n";
        diag.printAll(std::cerr);
        return 1;
    }

    // ── Aşama 3: Tip denetimi + yapısal doğrulama ─────────────────────────
    for (auto& unit : graph.units)
        TypeChecker(symbolTable, diag).check(unit.ast);
    for (auto& unit : graph.units)
        StructuralValidator(diag).validate(unit.ast);

    if (diag.hasErrors()) {
        std::cerr << "compilation errors, cannot run program:\n";
        diag.printAll(std::cerr);
        return 1;
    }

    // ── Aşama 4 (opsiyonel): Optimizasyon ────────────────────────────────
    if (args.optimized) {
        CompilerConfig   cfg;
        DiagnosticEngine optDiag;
        for (auto& unit : graph.units)
            OptimizationManager(cfg, optDiag).runPassesInPlace(unit.ast, &symbolTable);
        if (optDiag.errorCount() + optDiag.warningCount() > 0)
            optDiag.printAll(std::cerr);
    }

    // ── Aşama 5: IR üretimi ───────────────────────────────────────────────
    IRGenerator irGenerator;
    IRProgram   program;
    {
        profiling::StageTimer::ScopedStage _prof(profilerPtr, "ir-gen");
        program = irGenerator.generateModuleGraph(graph, symbolTable);
    }

    // ── Aşama 6: Çalıştırma backend'i ────────────────────────────────────
    // #80/MIRPLAN.md: --jit istenirse KISMİ/sessiz VM'e düşme YOK —
    // kullanıcı talimatı: "JIT diyorsam baştan sona JIT derlemesi
    // gerekiyor". Program.functions'daki HER fonksiyon Dilim 1'in
    // desteklediği opcode kümesinde değilse, HİÇBİR ŞEY çalıştırılmadan
    // açık bir hatayla çıkılır — VM devreye asla girmez.
    if (args.useJit) {
        int                             jitResult = 0;
        mir_backend::UnsupportedReason  reason;
        bool                            jitOk;
        {
            profiling::StageTimer::ScopedStage _prof(profilerPtr, "vm/jit");
            jitOk = mir_backend::tryCompileAndRunProgram(program, jitResult, reason);
        }
        if (jitOk) {
            if (args.verbose) std::cerr << "[jit] program bastan sona JIT'lendi (VM calismadi)\n";
            if (args.profile) stageTimer.printReport(std::cerr);
            return jitResult;
        }
        std::cerr << "error: --jit bu programi tam olarak derleyemiyor "
                   << "(fonksiyon '" << reason.functionName << "', desteklenmeyen opcode: "
                   << reason.opcodeName << ") — VM'e sessizce dusulmuyor, "
                   << "bkz. MIRPLAN.md.\n";
        return 1;
    }

    // ── Aşama 6b: VM çalıştır (varsayılan yol) ───────────────────────────
    int exitCode = 0;
    try {
        Interpreter vm(program);
        // GC (#77): --gc-threshold=N eşiği ezer (negatif = otomatik GC kapalı)
        if (args.gcThreshold != 0) vm.setGCThreshold(args.gcThreshold);
        vm.setCapabilities(args.allowedCaps);
        vm.setProgramArgs(args.programArgs);
        {
            profiling::StageTimer::ScopedStage _prof(profilerPtr, "vm/jit");
            exitCode = vm.run();
        }
        // --gc-stats: golden testlerin stdout karşılaştırmasını bozmamak
        // için stderr'e yazılır
        if (args.gcStats)
            std::cerr << "gc: runs=" << vm.gcRuns()
                      << " freed=" << vm.gcFreedTotal()
                      << " live=" << vm.heapAllocCount() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "runtime error: " << e.what() << "\n";
        exitCode = 1;
    }

    if (args.profile) stageTimer.printReport(std::cerr);

    return exitCode;
}

#endif // SAQUT_CLI_RUN
