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
        std::cerr << "Hata: AST üretilemedi\n";
        for (auto* t : tokens) delete t;
        return 1;
    }

    // ── Aşama 3: Sembol toplama + semantik analiz ─────────────────────────
    // Identifier'ların resolvedSymbol'ü doldurulur — IR generator buna ihtiyaç duyar.
    SymbolTable      symbolTable;
    DiagnosticEngine diag;
    SymbolCollector(symbolTable, diag).collect(ast);
    TypeChecker(symbolTable, diag).check(ast);
    StructuralValidator(diag).validate(ast);

    if (diag.hasErrors()) {
        std::cerr << "Derleme hataları var, program çalıştırılamaz:\n";
        diag.printAll(std::cerr);
        delete ast;
        for (auto* t : tokens) delete t;
        return 1;
    }

    // ── Aşama 4 (opsiyonel): Optimizasyon ────────────────────────────────
    // --optimized: constant folding + DCE yerinde uygulanır, klon yok.
    // Tek versiyon (optimize edilmiş) yeterli — ast komutu gibi karşılaştırma yok.
    if (args.optimized) {
        CompilerConfig   cfg;
        DiagnosticEngine optDiag;
        OptimizationManager(cfg, optDiag).runPassesInPlace(ast, &symbolTable);
        if (optDiag.errorCount() + optDiag.warningCount() > 0)
            optDiag.printAll(std::cerr); // W002 (derleme zamanı sıfıra bölme) vb.
    }

    // ── Aşama 5: IR üretimi ───────────────────────────────────────────────
    IRGenerator irGenerator;
    IRProgram   program = irGenerator.generate(ast, symbolTable);

    // ── Aşama 6: VM çalıştırma ────────────────────────────────────────────
    int exitCode = 0;
    try {
        Interpreter vm(program);
        exitCode = vm.run();
    } catch (const std::exception& e) {
        std::cerr << "Çalışma zamanı hatası: " << e.what() << "\n";
        exitCode = 1;
    }

    delete ast;
    for (auto* t : tokens) delete t;
    return exitCode;
}

#endif // SAQUT_CLI_RUN
