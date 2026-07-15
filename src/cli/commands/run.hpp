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

inline int cmdRun(const CliArgs& args) {
    std::string filePath = inputFilePath(args);
    if (filePath.empty()) { std::cerr << "error: no input file\n"; return 1; }

    // ── Aşama 1: Tüm modülleri yükle (BFS parse) ─────────────────────────
    ModuleRegistry   registry;
    DiagnosticEngine diag;
    ModuleLoader     loader(registry, diag);
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
    IRProgram   program = irGenerator.generateModuleGraph(graph, symbolTable);

    // ── Aşama 6: Çalıştırma backend'i ────────────────────────────────────
    // #80/MIRPLAN.md: --jit istenirse önce MIR Dilim 0'ı dene (yalnızca
    // LOAD_CONST/ADD/SUB/MUL/RETURN, parametresiz main). Desteklenmeyen bir
    // şey görülürse (bugün hemen hemen her program) VM'e düşülür — bu, tek
    // dispatch noktası ilkesinin (MIRPLAN.md §1) ilk hâli: JIT/VM seçimi
    // burada, TEK bir yerde yapılır.
    if (args.useJit) {
        IRFunction* mainForJit = program.findFunction("main");
        if (mainForJit) {
            int         jitResult = 0;
            std::string jitError;
            if (mir_backend::tryCompileAndRun(*mainForJit, jitResult, jitError)) {
                if (args.verbose) std::cerr << "[jit] Dilim 0 ile calistirildi\n";
                return jitResult;
            }
            if (args.verbose) std::cerr << "[jit] VM'e dusuldu: " << jitError << "\n";
        }
    }

    // ── Aşama 6b: VM çalıştır (varsayılan yol) ───────────────────────────
    int exitCode = 0;
    try {
        Interpreter vm(program);
        // GC (#77): --gc-threshold=N eşiği ezer (negatif = otomatik GC kapalı)
        if (args.gcThreshold != 0) vm.setGCThreshold(args.gcThreshold);
        vm.setCapabilities(args.allowedCaps);
        vm.setProgramArgs(args.programArgs);
        exitCode = vm.run();
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

    return exitCode;
}

#endif // SAQUT_CLI_RUN
