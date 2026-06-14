// ============================================================================
// saQut CLI — symbols komutu (sembol tablosu)
// ============================================================================

#ifndef SAQUT_CLI_SYMBOLS
#define SAQUT_CLI_SYMBOLS

#include <iostream>
#include "cli/args.hpp"
#include "tokenizer/tokenizer.hpp"
#include "parser/parser.hpp"
#include "json.hpp"

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

    auto symbols = collectSymbols(ast);

    std::cout << "Sembol Tablosu (" << symbols.size() << " sembol):\n";
    std::cout << "────────────────────────────────────────────\n";

    if (symbols.empty()) {
        std::cout << "  (sembol bulunamadı)\n";
    }

    for (auto& s : symbols) {
        std::cout << "  [" << s.kind << "] " << s.type << " " << s.name << "\n";
    }

    std::cout << "────────────────────────────────────────────\n";

    int fnCount = 0, varCount = 0;
    for (auto& s : symbols) {
        if (s.kind == "function") fnCount++;
        else if (s.kind == "variable") varCount++;
    }
    std::cout << "Fonksiyon: " << fnCount
              << "  |  Değişken: " << varCount << "\n";

    delete ast;
    for (auto* t : tokens) delete t;
    return 0;
}

#endif // SAQUT_CLI_SYMBOLS
