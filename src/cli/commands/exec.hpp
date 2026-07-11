// ============================================================================
// saQut CLI — exec komutu (IR göster + çalıştır)
// ============================================================================

#ifndef SAQUT_CLI_EXEC
#define SAQUT_CLI_EXEC

// ============================================================================
// saQut CLI — exec komutu
//
// Kullanım: saqut exec "1 + 2"
//
// İfadeyi int main() { print(<expr>); return 0; } olarak sarmalar,
// tam derleme pipeline'ından geçirir ve sonucu stdout'a yazar.
// ============================================================================

#include <iostream>
#include <string>
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

inline int cmdExec(const CliArgs& args) {
    if (args.positional.empty()) {
        std::cerr << "usage: saqut exec \"<expression>\"\n";
        std::cerr << "example: saqut exec \"1 + 2\"\n";
        return 1;
    }

    // Kullanıcının girdiği ifadeyi minimal programa sar
    const std::string& expr = args.positional[0];
    std::string source = "int main() {\n    print(" + expr + ");\n    return 0;\n}\n";
    const std::string syntheticPath = "<exec>";

    Tokenizer        tokenizer;
    auto             tokens = tokenizer.scan(source, syntheticPath);

    Parser   parser;
    ASTNode* ast = parser.parse(tokens);
    if (!ast) {
        std::cerr << "exec: parse error in expression: " << expr << "\n";
        for (auto* t : tokens) delete t;
        return 1;
    }

    SymbolTable      symbolTable;
    DiagnosticEngine diag;
    SymbolCollector(symbolTable, diag).collect(ast);

    if (!diag.hasErrors()) {
        TypeChecker(symbolTable, diag).check(ast);
        StructuralValidator(diag).validate(ast);
    }

    if (diag.hasErrors()) {
        diag.printAll(std::cerr);
        delete ast;
        for (auto* t : tokens) delete t;
        return 1;
    }

    IRGenerator irGenerator;
    IRProgram   program = irGenerator.generate(ast, symbolTable, syntheticPath);

    int exitCode = 0;
    try {
        Interpreter vm(program);
        exitCode = vm.run();
    } catch (const std::exception& e) {
        std::cerr << "exec: runtime error: " << e.what() << "\n";
        exitCode = 1;
    }

    delete ast;
    for (auto* t : tokens) delete t;
    return exitCode;
}

#endif // SAQUT_CLI_EXEC
