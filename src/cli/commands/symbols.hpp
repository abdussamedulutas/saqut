// ============================================================================
// saQut CLI — symbols komutu (sembol tablosu — Faz 2)
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

inline int cmdSymbols(const CliArgs& args) {
    std::string source = readSource(args);
    if (source.empty()) return 1;

    Tokenizer tokenizer;
    auto tokens = tokenizer.scan(source, inputFilePath(args));

    Parser parser;
    ASTNode* ast = parser.parse(tokens);

    if (!ast) {
        std::cerr << "Hata: AST üretilemedi\n";
        for (auto* t : tokens) delete t;
        return 1;
    }

    SymbolTable table;
    DiagnosticEngine diag;
    SymbolCollector(table, diag).collect(ast);

    auto symbols = table.allSymbols();

    std::cout << "Sembol Tablosu (" << symbols.size() << " sembol):\n";
    std::cout << "────────────────────────────────────────────\n";

    if (symbols.empty()) {
        std::cout << "  (sembol bulunamadı)\n";
    }

    for (Symbol* s : symbols) {
        if (s->isBuiltin) continue; // builtinleri çıktıda gösterme
        std::cout << "  [" << symbolKindName(s->kind) << "] "
                  << s->type.toString() << " " << s->name
                  << " @" << s->definitionLoc.shortString()
                  << "  refs(" << s->references.size() << "):";
        for (auto& r : s->references)
            std::cout << " " << r.shortString();
        std::cout << "\n";
    }

    std::cout << "────────────────────────────────────────────\n";

    if (diag.hasErrors()) {
        std::cerr << "\n";
        diag.printAll(std::cerr);
    }

    delete ast;
    for (auto* t : tokens) delete t;
    return diag.hasErrors() ? 1 : 0;
}

#endif // SAQUT_CLI_SYMBOLS
