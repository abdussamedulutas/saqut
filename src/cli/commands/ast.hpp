// ============================================================================
// saQut CLI — ast komutu (JSON formatında AST hiyerarşisi + analiz)
// ============================================================================

#ifndef SAQUT_CLI_AST
#define SAQUT_CLI_AST

#include <iostream>
#include <fstream>
#include "cli/args.hpp"
#include "tokenizer/tokenizer.hpp"
#include "parser/parser.hpp"
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

    AstAnalysis analysis = analyzeAst(ast);

    // Çıktı hedefi
    std::ostream* out = &std::cout;
    std::ofstream outFile;
    if (!args.outputFile.empty()) {
        outFile.open(args.outputFile);
        if (outFile.is_open()) out = &outFile;
    }

    *out << "{\n"
         << "  \"ast\":\n"
         << jsonIndent(2) << astToJson(ast, 2) << ",\n"
         << "  \"analysis\": {\n"
         << analysisToJson(analysis) << "\n"
         << "  }\n"
         << "}\n";

    delete ast;
    for (auto* t : tokens) delete t;
    return 0;
}

#endif // SAQUT_CLI_AST
