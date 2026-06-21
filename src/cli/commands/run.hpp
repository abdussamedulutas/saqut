// ============================================================================
// saQut CLI — run komutu
//
// Tam derleme + çalıştırma pipeline'ı:
//   tokenize → parse → sembol topla → [opsiyonel: optimize] → IR üret → VM çalıştır
//
// --optimized bayrağı: AST yerinde optimize edilir (klon yok — sadece tek versiyon
// gerekiyor). ast komutu orijinali saklaması gerektiği için klon kullanır; run/ir
// kullanmaz. Aynı pattern ir.hpp'de de var — paralel değişikliklerde ikisine bak.
// ============================================================================

#ifndef SAQUT_CLI_RUN
#define SAQUT_CLI_RUN

#include <iostream>
#include "cli/args.hpp"
#include "tokenizer/tokenizer.hpp"
#include "parser/parser.hpp"
#include "symbol/symbol_table.hpp"
#include "symbol/symbol_collector.hpp"
#include "semantic/type_checker.hpp"
#include "semantic/structural_validator.hpp"
#include "diagnostic/diagnostic_engine.hpp"
#include "core/config.hpp"
#include "opt/optimization_manager.hpp"
#include "ir/ir_generator.hpp"
#include "vm/interpreter.hpp"

inline int cmdRun(const CliArgs& args) {
    std::string filePath = inputFilePath(args);
    std::string source   = readSource(args);
    if (source.empty()) return 1;

    // ── Aşama 1: Tokenize ────────────────────────────────────────────────
    Tokenizer tokenizer;
    auto tokens = tokenizer.scan(source, filePath);

    // ── Aşama 2: Parse ───────────────────────────────────────────────────
    Parser parser;
    ASTNode* ast = parser.parse(tokens);
    if (!ast) {
        std::cerr << "error: failed to build AST\n";
        for (auto* t : tokens) delete t;
        return 1;
    }

    // ── Phase 3: Symbol collection + semantic analysis ─────────────────────────
    // Identifier's resolvedSymbol is filled — IR generator needs this.
    SymbolTable      symbolTable;
    DiagnosticEngine diag;
    SymbolCollector(symbolTable, diag).collect(ast);
    TypeChecker(symbolTable, diag).check(ast);
    StructuralValidator(diag).validate(ast);

    if (diag.hasErrors()) {
        std::cerr << "compilation errors, cannot run program:\n";
        diag.printAll(std::cerr);
        delete ast;
        for (auto* t : tokens) delete t;
        return 1;
    }

    // ── Phase 4 (optional): Optimization ────────────────────────────────
    // --optimized: constant folding + DCE applied in-place, no clone.
    // Single version (optimized) is sufficient — no comparison like ast command.
    if (args.optimized) {
        CompilerConfig   cfg;
        DiagnosticEngine optDiag;
        OptimizationManager(cfg, optDiag).runPassesInPlace(ast, &symbolTable);
        if (optDiag.errorCount() + optDiag.warningCount() > 0)
            optDiag.printAll(std::cerr); // W002 (compile-time division by zero) etc.
    }

    // ── Phase 5: IR generation ───────────────────────────────────────────────
    IRGenerator irGenerator;
    IRProgram   program = irGenerator.generate(ast, symbolTable, filePath);

    // ── Phase 6: Run on VM ────────────────────────────────────────────
    int exitCode = 0;
    try {
        Interpreter vm(program);
        exitCode = vm.run();
    } catch (const std::exception& e) {
        std::cerr << "runtime error: " << e.what() << "\n";
        exitCode = 1;
    }

    delete ast;
    for (auto* t : tokens) delete t;
    return exitCode;
}

#endif // SAQUT_CLI_RUN
