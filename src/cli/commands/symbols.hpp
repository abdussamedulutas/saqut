// ============================================================================
// saQut CLI — symbols komutu (sembol tablosu — JSON çıktı, Faz 2)
// ============================================================================

#ifndef SAQUT_CLI_SYMBOLS
#define SAQUT_CLI_SYMBOLS

#include <iostream>
#include "tools.hpp"
#include "cli/args.hpp"
#include "cli/exit_codes.hpp"
#include "tokenizer/tokenizer.hpp"
#include "parser/parser.hpp"
#include "symbol/symbol_table.hpp"
#include "symbol/symbol_collector.hpp"
#include "diagnostic/diagnostic_engine.hpp"
#include "vendor/nlohmann/json.hpp"

inline int cmdSymbols(const CliArgs& args) {
    // #145: eski tek-doküman --json preview yüzeyi kaldırıldı; symbols
    // artık yalnız düz metin (varsayılan) veya --jsonl üretir. --json
    // text/JSONL'e sessizce düşmez, açık usage error verir.
    if (args.jsonOutput) {
        std::cerr << "error: --json is removed for symbols; use --jsonl\n";
        return saqut::exit_code::kUsageError;
    }
    if (args.jsonlOutput && args.compact) {
        std::cerr << "error: --compact is not valid with --jsonl\n";
        return saqut::exit_code::kUsageError;
    }
    std::string filePath = inputFilePath(args);
    std::string source   = readSource(args);
    if (source.empty()) return saqut::exit_code::kUsageError;

    Tokenizer tokenizer;
    auto tokens = tokenizer.scan(source, filePath);

    Parser parser;
    ASTNode* ast = parser.parse(tokens);

    SymbolTable table;
    DiagnosticEngine diag;

    if (ast) {
        SymbolCollector(table, diag, args.allowedCaps).collect(ast);
    } else {
        diag.report("E000", SourceLocation{}, "failed to build AST");
    }

    if (args.jsonlOutput) {
        nlohmann::json header = {{"kind", "symbols.header"}, {"schemaVersion", 1}, {"file", filePath}};
        std::cout << header.dump() << "\n";
        int symbolCount = 0;
        for (Symbol* s : table.allSymbols()) {
            if (s->isBuiltin) continue;
            nlohmann::json refs = nlohmann::json::array();
            for (const SourceLocation& r : s->references) refs.push_back(r.toJsonObj());
            std::cout << nlohmann::json({{"kind", "symbol"}, {"name", s->name},
                {"symbolKind", symbolKindName(s->kind)}, {"type", s->type.toString()},
                {"definition", s->definitionLoc.toJsonObj()}, {"referenceCount", (int)s->references.size()},
                {"references", refs}}).dump() << "\n";
            ++symbolCount;
        }
        for (const auto& d : diag.all())
            std::cout << nlohmann::json({{"kind", "diagnostic"}, {"code", d.code}, {"message", d.message},
                {"severity", d.level == DiagLevel::Error ? "error" : "warning"}}).dump() << "\n";
        std::cout << nlohmann::json({{"kind", "symbols.end"}, {"symbolCount", symbolCount},
            {"diagnosticCount", (int)diag.all().size()}}).dump() << "\n";
    } else {
        // ── Düz metin çıktı ─────────────────────────────────────────────────
        for (Symbol* s : table.allSymbols()) {
            if (s->isBuiltin) continue;

            // "<def_loc>  <tip>  <isim>" — fonksiyonlarda kind parantezde
            std::string defLoc = s->definitionLoc.isValid()
                ? s->definitionLoc.toString()
                : "<unknown>";
            std::string kindSuffix = (s->kind != SymbolKind::Variable &&
                                      s->kind != SymbolKind::Parameter)
                ? std::string(" (") + symbolKindName(s->kind) + ")"
                : "";
            std::cout << Color::SoftGri << defLoc << Color::Reset << "  "
                      << Color::SoftPembe << s->type.toString() << Color::Reset << "  "
                      << Color::SoftYesil << s->name << Color::Reset
                      << Color::SoftMor << kindSuffix << Color::Reset << "\n";

            if (!s->references.empty()) {
                std::cout << "\t" << Color::SoftTurkuaz << "refs" << Color::Reset;
                for (const SourceLocation& r : s->references)
                    std::cout << "  " << Color::SoftGri << r.toString() << Color::Reset;
                std::cout << "\n";
            }
        }
        if (diag.hasErrors() || diag.warningCount() > 0)
            diag.printAll(std::cerr);
    }

    delete ast;
    for (auto* t : tokens) delete t;
    return diag.hasErrors() ? saqut::exit_code::kDataError : saqut::exit_code::kSuccess;
}

#endif // SAQUT_CLI_SYMBOLS
