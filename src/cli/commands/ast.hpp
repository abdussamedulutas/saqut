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
#include "cli/exit_codes.hpp"
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
    if (source.empty()) return saqut::exit_code::kUsageError;

    Tokenizer tokenizer;
    auto tokens = tokenizer.scan(source, inputFilePath(args));

    // RG-7 (#157): ast, run/check/ir ile AYNI diagnostic kapısından
    // geçmeli. Önceki kod Parser'ı DiagnosticEngine vermeden çağırıyordu
    // (diag_ == nullptr) — bu modda syntax hatası yalnız stderr'e basılıp
    // panic-mode kurtarma ile devam ediyor, `!ast` hiç true olmuyordu (aynı
    // sınıf hata #134'te exec için tespit edildi). Üstüne semantic
    // hatalar (SymbolCollector/TypeChecker/StructuralValidator) HİÇ
    // kontrol edilmiyordu — `ast` her zaman 0 dönüyordu, `--optimized`
    // verilmediği sürece diagnostic hiç basılmıyordu.
    DiagnosticEngine diag;
    Parser           parser(&diag);
    ASTNode*         ast = parser.parse(tokens);

    if (!ast || diag.hasErrors()) {
        diag.printAll(std::cerr);
        delete ast;
        for (auto* t : tokens) delete t;
        return saqut::exit_code::kDataError;
    }

    // ── Symbol + type analysis (required for --optimized; optional for plain ast) ──
    SymbolTable symbolTable;
    SymbolCollector(symbolTable, diag, args.allowedCaps).collect(ast);
    TypeChecker(symbolTable, diag).check(ast);
    StructuralValidator(diag).validate(ast);

    if (diag.hasErrors()) {
        diag.printAll(std::cerr);
        delete ast;
        for (auto* t : tokens) delete t;
        return saqut::exit_code::kDataError;
    }

    ASTNode* displayAst = ast; // gösterilecek ağaç (orijinal veya klon)
    ASTNode* clonedAst  = nullptr;

    if (args.optimized) {
        CompilerConfig   cfg;
        OptimizationManager mgr(cfg, diag);
        clonedAst  = mgr.optimize(ast, &symbolTable);
        displayAst = clonedAst;
    }

    if (diag.hasErrors()) {
        // --optimized aşaması yeni hata ekledi (önceki geçitte yoktu).
        diag.printAll(std::cerr);
        if (clonedAst) delete clonedAst;
        delete ast;
        for (auto* t : tokens) delete t;
        return saqut::exit_code::kDataError;
    }

    AstAnalysis analysis = analyzeAst(displayAst);

    std::ostream* out = &std::cout;
    std::ofstream outFile;
    if (!args.outputFile.empty()) {
        outFile.open(args.outputFile);
        if (outFile.is_open()) out = &outFile;
    }

    if (args.jsonOutput) {
        *out << "{\n"
             << "  \"ast\":\n"
             << jsonIndent(2) << astToJson(displayAst, 2) << ",\n"
             << "  \"analysis\": {\n"
             << analysisToJson(analysis) << "\n"
             << "  }\n"
             << "}\n";
    } else {
        displayAst->log(0);
    }

    // Kalan uyarılar (W002 optimizasyon uyarıları dahil, hata yok) her
    // zaman basılır — yalnız --optimized modunda değil.
    if (diag.warningCount() > 0) diag.printAll(std::cerr);

    if (clonedAst) delete clonedAst;
    delete ast;
    for (auto* t : tokens) delete t;
    return saqut::exit_code::kSuccess;
}

#endif // SAQUT_CLI_AST
