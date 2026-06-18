// ============================================================================
// saQut CLI — symbols komutu (sembol tablosu — JSON çıktı, Faz 2)
// ============================================================================

#ifndef SAQUT_CLI_SYMBOLS
#define SAQUT_CLI_SYMBOLS

#include <iostream>
#include <sstream>
#include "cli/args.hpp"
#include "tokenizer/tokenizer.hpp"
#include "parser/parser.hpp"
#include "symbol/symbol_table.hpp"
#include "symbol/symbol_collector.hpp"
#include "diagnostic/diagnostic_engine.hpp"
#include "tools.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// symbolsToJson — sembol tablosunu JSON olarak serileştir
// ─────────────────────────────────────────────────────────────────────────────

inline std::string symbolsToJson(const std::string& filePath,
                                  const SymbolTable& table,
                                  const DiagnosticEngine& diag) {
    auto symbols = table.allSymbols();
    std::ostringstream ss;

    ss << "{\n";
    ss << "  \"file\": \"" << jsonEscape(filePath) << "\",\n";
    ss << "  \"symbols\": [";

    bool firstSym = true;
    for (Symbol* s : symbols) {
        if (s->isBuiltin) continue;

        if (!firstSym) ss << ",";
        firstSym = false;

        ss << "\n    {\n";
        ss << "      \"name\": \""    << jsonEscape(s->name)              << "\",\n";
        ss << "      \"kind\": \""    << symbolKindName(s->kind)           << "\",\n";
        ss << "      \"type\": \""    << jsonEscape(s->type.toString())    << "\",\n";
        ss << "      \"typeDetail\": " << s->type.toJson()                 << ",\n";
        ss << "      \"definition\": " << s->definitionLoc.toJson()        << ",\n";
        ss << "      \"isBuiltin\": "  << (s->isBuiltin ? "true" : "false") << ",\n";

        // referanslar
        ss << "      \"references\": [";
        bool firstRef = true;
        for (const SourceLocation& ref : s->references) {
            if (!firstRef) ss << ", ";
            firstRef = false;
            ss << ref.toJson();
        }
        ss << "]\n";

        ss << "    }";
    }

    if (!firstSym) ss << "\n  ";
    ss << "],\n";

    // tanılar (diagnostic engine'den hazır JSON al, iç kısmını sar)
    ss << "  \"diagnostics\": " << diag.toJson() << "\n";
    ss << "}\n";

    return ss.str();
}

// ─────────────────────────────────────────────────────────────────────────────
// cmdSymbols — giriş noktası
// ─────────────────────────────────────────────────────────────────────────────

inline int cmdSymbols(const CliArgs& args) {
    std::string filePath = inputFilePath(args);
    std::string source   = readSource(args);
    if (source.empty()) return 1;

    Tokenizer tokenizer;
    auto tokens = tokenizer.scan(source, filePath);

    Parser parser;
    ASTNode* ast = parser.parse(tokens);

    if (!ast) {
        // AST null olursa boş ama geçerli bir JSON çıktısı üret
        DiagnosticEngine diag;
        diag.report("E000", SourceLocation{}, "AST üretilemedi");
        SymbolTable empty;
        std::cout << symbolsToJson(filePath, empty, diag);
        for (auto* t : tokens) delete t;
        return 1;
    }

    SymbolTable table;
    DiagnosticEngine diag;
    SymbolCollector(table, diag).collect(ast);

    std::cout << symbolsToJson(filePath, table, diag);

    delete ast;
    for (auto* t : tokens) delete t;
    return diag.hasErrors() ? 1 : 0;
}

#endif // SAQUT_CLI_SYMBOLS
