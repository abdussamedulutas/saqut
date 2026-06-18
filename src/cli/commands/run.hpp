// ============================================================================
// saQut CLI — run komutu
//
// Tam derleme + çalıştırma pipeline'ı:
//   tokenize → parse → sembol topla → IR üret → VM çalıştır
//
// Başarı kriteri:
//   build/saqut run file:examples/fibonacci.sqt
//   → 55
//   → 55
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

    // ── Aşama 3: Sembol toplama ───────────────────────────────────────────
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

    // ── Aşama 4: IR üretimi ───────────────────────────────────────────────
    IRGenerator irGenerator;
    IRProgram   program = irGenerator.generate(ast, symbolTable);

    // ── Aşama 5: VM çalıştırma ────────────────────────────────────────────
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
