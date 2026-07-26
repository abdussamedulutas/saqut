// ============================================================================
// saQut CLI — check komutu
//
// Yalnızca semantik analiz: ModuleLoader → SymbolCollector → TypeChecker
// ============================================================================

#ifndef SAQUT_CLI_CHECK
#define SAQUT_CLI_CHECK

#include <iostream>
#include "cli/args.hpp"
#include "module/module_loader.hpp"
#include "symbol/symbol_table.hpp"
#include "symbol/symbol_collector.hpp"
#include "semantic/type_checker.hpp"
#include "semantic/structural_validator.hpp"
#include "diagnostic/diagnostic_engine.hpp"
#include "core/module_registry.hpp"
#include "vendor/nlohmann/json.hpp"

inline int cmdCheck(const CliArgs& args) {
    std::string filePath = inputFilePath(args);
    if (filePath.empty()) return 1;
    if (args.compact) {
        std::cerr << "error: check --compact is invalid for JSONL output\n";
        return 64;
    }

    ModuleRegistry   registry;
    DiagnosticEngine diag;
    ModuleGraph      graph = ModuleLoader(registry, diag).load(filePath);

    if (!diag.hasErrors()) {
        SymbolTable table;
        SymbolCollector(table, diag, args.allowedCaps).collectModuleGraph(graph);

        if (!diag.hasErrors()) {
            for (auto& unit : graph.units) TypeChecker(table, diag).check(unit.ast);
            for (auto& unit : graph.units) StructuralValidator(diag).validate(unit.ast);
        }
    }

    nlohmann::json header = {
        {"kind", "check.header"}, {"schemaVersion", 1}, {"file", filePath}
    };
    std::cout << header.dump() << "\n";
    for (const auto& d : diag.all()) {
        nlohmann::json record = d.toJsonObj();
        record["kind"] = "check.diagnostic";
        std::cout << record.dump() << "\n";
    }
    nlohmann::json end = {
        {"kind", "check.end"},
        {"errorCount", diag.errorCount()},
        {"warningCount", diag.warningCount()}
    };
    std::cout << end.dump() << "\n";

    return diag.hasErrors() ? 65 : 0;
}

#endif // SAQUT_CLI_CHECK
