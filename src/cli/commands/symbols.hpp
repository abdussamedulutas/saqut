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
    std::string filePath = inputFilePath(args);
    std::string source   = readSource(args);
    if (source.empty()) return saqut::exit_code::kUsageError;

    Tokenizer tokenizer;
    auto tokens = tokenizer.scan(source, filePath);

    // RG-7 (#157): #134/ast ile aynı sınıf düzeltme — Parser'a gerçek
    // DiagnosticEngine verilmezse syntax hatası panic-mode kurtarma ile
    // yutulur, `!ast` hiç true olmaz.
    DiagnosticEngine diag;
    Parser           parser(&diag);
    ASTNode*         ast = parser.parse(tokens);

    SymbolTable table;
    if (ast) {
        SymbolCollector(table, diag, args.allowedCaps).collect(ast);
    } else if (!diag.hasErrors()) {
        diag.report("E000", SourceLocation{}, "failed to build AST");
    }

    if (args.jsonOutput) {
        // ── JSON çıktı ──────────────────────────────────────────────────────
        nlohmann::json out;
        out["file"] = filePath;

        nlohmann::json symArray = nlohmann::json::array();
        for (Symbol* s : table.allSymbols()) {
            if (s->isBuiltin) continue;

            nlohmann::json refs = nlohmann::json::array();
            for (const SourceLocation& r : s->references)
                refs.push_back(r.toJsonObj());

            symArray.push_back({
                {"name",           s->name},
                {"kind",           symbolKindName(s->kind)},
                {"type",           s->type.toString()},
                {"typeDetail",     s->type.toJsonObj()},
                {"sourceModule",   s->moduleId == 0 ? "__builtin__"
                                                    : s->moduleId < 0  ? "<main>"
                                                    : "<module:" + std::to_string(s->moduleId) + ">"},
                {"definition",     s->definitionLoc.toJsonObj()},
                {"referenceCount", static_cast<int>(s->references.size())},
                {"references",     refs},
                {"isBuiltin",      s->isBuiltin}
            });
        }
        out["symbols"]     = symArray;
        out["diagnostics"] = diag.toJsonObj();

        std::cout << (args.compact ? out.dump() : out.dump(2)) << "\n";
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
