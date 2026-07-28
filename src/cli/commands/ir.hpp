// ============================================================================
// saQut CLI — ir komutu (IR instruction dump)
// ============================================================================

#ifndef SAQUT_CLI_IR
#define SAQUT_CLI_IR

#include <iostream>
#include <set>
#include "cli/args.hpp"
#include "cli/exit_codes.hpp"
#include "core/capability.hpp"
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
    if (filePath.empty()) return saqut::exit_code::kUsageError;

    ModuleRegistry   registry;
    DiagnosticEngine diag;
    ModuleGraph      graph = ModuleLoader(registry, diag).load(filePath);

    SymbolTable symbolTable;
    SymbolCollector(symbolTable, diag, args.allowedCaps).collectModuleGraph(graph);
    if (!diag.hasErrors()) {
        for (auto& unit : graph.units) TypeChecker(symbolTable, diag).check(unit.ast);
        for (auto& unit : graph.units) StructuralValidator(diag).validate(unit.ast);
    }

    if (diag.hasErrors()) {
        diag.printAll(std::cerr);
        return saqut::exit_code::kDataError;
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

    // ADR-035 (#76): statik capability analizi — programın hangi cap'lere
    // ihtiyaç duyduğunu raporlar. caps::drop RUNTIME davranışıdır, bu üst
    // sınır raporundan ETKİLENMEZ (ayrım kasıtlı — #91).
    if (args.showCapabilities) {
        std::set<Capability> used;
        for (auto& [name, fn] : program.functions)
            for (auto& instr : fn.instructions)
                if (instr.requiredCap) used.insert(*instr.requiredCap);
        std::cout << "capabilities:";
        if (used.empty()) std::cout << " (none)";
        for (auto c : used) std::cout << " " << capabilityName(c);
        std::cout << "\n";
        return saqut::exit_code::kSuccess;
    }

    program.dump();

    return saqut::exit_code::kSuccess;
}

#endif // SAQUT_CLI_IR
