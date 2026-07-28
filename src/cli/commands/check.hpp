// ============================================================================
// saQut CLI — check komutu
//
// Yalnızca semantik analiz: ModuleLoader → SymbolCollector → TypeChecker
// ============================================================================

#ifndef SAQUT_CLI_CHECK
#define SAQUT_CLI_CHECK

#include <iostream>
#include "cli/args.hpp"
#include "cli/exit_codes.hpp"
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
    if (filePath.empty()) return saqut::exit_code::kUsageError;

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

    nlohmann::json out;
    out["file"]        = filePath;
    out["diagnostics"] = diag.toJsonObj();
    std::cout << (args.compact ? out.dump() : out.dump(2)) << "\n";

    return diag.hasErrors() ? saqut::exit_code::kDataError : saqut::exit_code::kSuccess;
}

#endif // SAQUT_CLI_CHECK
