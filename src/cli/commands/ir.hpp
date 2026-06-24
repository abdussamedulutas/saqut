#ifndef SAQUT_CLI_IR
#define SAQUT_CLI_IR

#include <iostream>
#include "cli/args.hpp"
#include "module/module_loader.hpp"
#include "symbol/symbol_table.hpp"
#include "symbol/symbol_collector.hpp"
#include "semantic/type_checker.hpp"
#include "semantic/structural_validator.hpp"
#include "diagnostic/diagnostic_engine.hpp"
#include "core/module_registry.hpp"
#include "core/config.hpp"
#include "opt/optimization_manager.hpp"
#include "ir/ir_generator.hpp"

inline int cmdIr(const CliArgs& args) {
    std::string filePath = inputFilePath(args);
    if (filePath.empty()) return 1;

    ModuleRegistry   registry;
    DiagnosticEngine diag;
    ModuleGraph      graph = ModuleLoader(registry, diag).load(filePath);

    SymbolTable symbolTable;
    SymbolCollector(symbolTable, diag).collectModuleGraph(graph);
    if (!diag.hasErrors()) {
        for (auto& unit : graph.units) TypeChecker(symbolTable, diag).check(unit.ast);
        for (auto& unit : graph.units) StructuralValidator(diag).validate(unit.ast);
    }

    if (diag.hasErrors()) {
        diag.printAll(std::cerr);
        return 1;
    }

    if (args.optimized) {
        CompilerConfig   cfg;
        DiagnosticEngine optDiag;
        for (auto& unit : graph.units)
            OptimizationManager(cfg, optDiag).runPassesInPlace(unit.ast, &symbolTable);
        if (optDiag.errorCount() + optDiag.warningCount() > 0)
            optDiag.printAll(std::cerr);
    }

    IRGenerator irGenerator;
    IRProgram   program = irGenerator.generateModuleGraph(graph, symbolTable);
    program.dump();

    return 0;
}

#endif // SAQUT_CLI_IR
