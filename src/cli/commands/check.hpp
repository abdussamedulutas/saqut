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

    ModuleRegistry   registry;
    DiagnosticEngine diag;
    ModuleGraph      graph = ModuleLoader(registry, diag).load(filePath);

    if (!diag.hasErrors()) {
        SymbolTable table;
        SymbolCollector(table, diag).collectModuleGraph(graph);

        if (!diag.hasErrors()) {
            for (auto& unit : graph.units) TypeChecker(table, diag).check(unit.ast);
            for (auto& unit : graph.units) StructuralValidator(diag).validate(unit.ast);
        }
    }

    nlohmann::json out;
    out["file"]        = filePath;
    out["diagnostics"] = diag.toJsonObj();
    std::cout << (args.compact ? out.dump() : out.dump(2)) << "\n";

    return diag.hasErrors() ? 1 : 0;
}

#endif // SAQUT_CLI_CHECK
