#ifndef SAQUT_CLI_IR
#define SAQUT_CLI_IR

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
#include "core/config.hpp"
#include "opt/optimization_manager.hpp"

inline int cmdIr(const CliArgs& args) {
    std::string filePath = inputFilePath(args);
    std::string source   = readSource(args);
    if (source.empty()) return 1;

    Tokenizer tokenizer;
    auto tokens = tokenizer.scan(source, filePath);

    Parser parser;
    ASTNode* ast = parser.parse(tokens);
    if (!ast) {
        std::cerr << "Hata: AST üretilemedi\n";
        for (auto* t : tokens) delete t;
        return 1;
    }

    SymbolTable      symbolTable;
    DiagnosticEngine diag;
    SymbolCollector(symbolTable, diag).collect(ast);
    TypeChecker(symbolTable, diag).check(ast);
    StructuralValidator(diag).validate(ast);

    if (diag.hasErrors()) {
        diag.printAll(std::cerr);
        delete ast;
        for (auto* t : tokens) delete t;
        return 1;
    }

    // --optimized: optimize edilmiş AST klonu üzerinden IR üret
    ASTNode* activeAst   = ast;
    ASTNode* optimizedAst = nullptr;
    if (args.optimized) {
        CompilerConfig   cfg;
        DiagnosticEngine optDiag;
        OptimizationManager mgr(cfg, optDiag);
        optimizedAst = mgr.optimize(ast, &symbolTable);
        activeAst    = optimizedAst;
        if (optDiag.errorCount() + optDiag.warningCount() > 0)
            optDiag.printAll(std::cerr); // W002 vb. uyarılar stderr'e
    }

    IRGenerator irGenerator;
    IRProgram   program = irGenerator.generate(activeAst, symbolTable);
    program.dump();

    delete optimizedAst; // nullptr ise no-op
    delete ast;
    for (auto* t : tokens) delete t;
    return 0;
}

#endif // SAQUT_CLI_IR
