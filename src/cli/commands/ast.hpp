// ============================================================================
// saQut CLI — ast komutu (JSON formatında AST hiyerarşisi + analiz)
//
// --optimized: sabit katlama + ölü kod eleme uygulandıktan sonra AST göster.
//              Orijinal AST dokunulmaz; optimize edilmiş klon gösterilir.
// ============================================================================

#ifndef SAQUT_CLI_AST
#define SAQUT_CLI_AST

#include <iostream>
#include <fstream>
#include "cli/args.hpp"
#include "tokenizer/tokenizer.hpp"
#include "parser/parser.hpp"
#include "symbol/symbol_table.hpp"
#include "symbol/symbol_collector.hpp"
#include "semantic/type_checker.hpp"
#include "semantic/structural_validator.hpp"
#include "diagnostic/diagnostic_engine.hpp"
#include "core/config.hpp"
#include "opt/optimization_manager.hpp"
#include "json.hpp"

inline int cmdAst(const CliArgs& args) {
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

    // ── Sembol + tip analizi (--optimized için gerekli; yalın ast'te opsiyonel) ──
    SymbolTable      symbolTable;
    DiagnosticEngine diag;
    SymbolCollector(symbolTable, diag).collect(ast);
    TypeChecker(symbolTable, diag).check(ast);
    StructuralValidator(diag).validate(ast);

    ASTNode* displayAst = ast; // gösterilecek ağaç (orijinal veya klon)
    ASTNode* clonedAst  = nullptr;

    if (args.optimized) {
        CompilerConfig   cfg;
        OptimizationManager mgr(cfg, diag);
        clonedAst  = mgr.optimize(ast, &symbolTable);
        displayAst = clonedAst;
    }

    AstAnalysis analysis = analyzeAst(displayAst);

    std::ostream* out = &std::cout;
    std::ofstream outFile;
    if (!args.outputFile.empty()) {
        outFile.open(args.outputFile);
        if (outFile.is_open()) out = &outFile;
    }

    *out << "{\n"
         << "  \"ast\":\n"
         << jsonIndent(2) << astToJson(displayAst, 2) << ",\n"
         << "  \"analysis\": {\n"
         << analysisToJson(analysis) << "\n"
         << "  }\n"
         << "}\n";

    // Optimizasyon uyarılarını (W002 vb.) stderr'e yazdır
    if (args.optimized) diag.printAll(std::cerr);

    if (clonedAst) delete clonedAst;
    delete ast;
    for (auto* t : tokens) delete t;
    return 0;
}

#endif // SAQUT_CLI_AST
