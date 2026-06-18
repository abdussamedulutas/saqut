// ============================================================================
// saQut CLI — symbols komutu (sembol tablosu — JSON çıktı, Faz 2)
// ============================================================================

#ifndef SAQUT_CLI_SYMBOLS
#define SAQUT_CLI_SYMBOLS

#include <iostream>
#include "cli/args.hpp"
#include "tokenizer/tokenizer.hpp"
#include "parser/parser.hpp"
#include "symbol/symbol_table.hpp"
#include "symbol/symbol_collector.hpp"
#include "diagnostic/diagnostic_engine.hpp"
#include "vendor/nlohmann/json.hpp"

inline int cmdSymbols(const CliArgs& args) {
    std::string filePath = inputFilePath(args);
    std::string source   = readSource(args);
    if (source.empty()) return 1;

    Tokenizer tokenizer;
    auto tokens = tokenizer.scan(source, filePath);

    Parser parser;
    ASTNode* ast = parser.parse(tokens);

    SymbolTable table;
    DiagnosticEngine diag;

    if (ast) {
        SymbolCollector(table, diag).collect(ast);
    } else {
        diag.report("E000", SourceLocation{}, "AST üretilemedi");
    }

    // ── JSON çıktı ──────────────────────────────────────────────────────────
    nlohmann::json out;
    out["file"] = filePath;

    nlohmann::json symArray = nlohmann::json::array();
    for (Symbol* s : table.allSymbols()) {
        if (s->isBuiltin) continue;

        nlohmann::json refs = nlohmann::json::array();
        for (const SourceLocation& r : s->references)
            refs.push_back(r.toJsonObj());

        symArray.push_back({
            {"name",       s->name},
            {"kind",       symbolKindName(s->kind)},
            {"type",       s->type.toString()},
            {"typeDetail", s->type.toJsonObj()},
            {"definition", s->definitionLoc.toJsonObj()},
            {"references", refs},
            {"isBuiltin",  s->isBuiltin}
        });
    }
    out["symbols"]     = symArray;
    out["diagnostics"] = diag.toJsonObj();

    std::cout << out.dump(2) << "\n";

    delete ast;
    for (auto* t : tokens) delete t;
    return diag.hasErrors() ? 1 : 0;
}

#endif // SAQUT_CLI_SYMBOLS
