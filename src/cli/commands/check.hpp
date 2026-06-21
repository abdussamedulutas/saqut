#ifndef SAQUT_CLI_CHECK
#define SAQUT_CLI_CHECK

#include <iostream>
#include "cli/args.hpp"
#include "tokenizer/tokenizer.hpp"
#include "parser/parser.hpp"
#include "symbol/symbol_table.hpp"
#include "symbol/symbol_collector.hpp"
#include "semantic/type_checker.hpp"
#include "semantic/structural_validator.hpp"
#include "diagnostic/diagnostic_engine.hpp"
#include "vendor/nlohmann/json.hpp"

inline int cmdCheck(const CliArgs& args) {
    std::string filePath = inputFilePath(args);
    std::string source   = readSource(args);
    if (source.empty()) return 1;

    Tokenizer tokenizer;
    auto tokens = tokenizer.scan(source, filePath);

    Parser parser;
    ASTNode* ast = parser.parse(tokens);

    DiagnosticEngine diag;

    if (!ast) {
        diag.report("E000", SourceLocation{}, "failed to build AST");
        nlohmann::json out;
        out["file"]        = filePath;
        out["diagnostics"] = diag.toJsonObj();
        std::cout << (args.compact ? out.dump() : out.dump(2)) << "\n";
        for (auto* t : tokens) delete t;
        return 1;
    }

    SymbolTable table;
    SymbolCollector(table, diag).collect(ast);

    // If symbol collection errors exist, skip type checking
    // (unresolved symbols produce spurious E003 in type checking)
    if (!diag.hasErrors()) {
        TypeChecker(table, diag).check(ast);
        StructuralValidator(diag).validate(ast);
    }

    nlohmann::json out;
    out["file"]        = filePath;
    out["diagnostics"] = diag.toJsonObj();

    std::cout << (args.compact ? out.dump() : out.dump(2)) << "\n";

    delete ast;
    for (auto* t : tokens) delete t;
    return diag.hasErrors() ? 1 : 0;
}

#endif // SAQUT_CLI_CHECK
