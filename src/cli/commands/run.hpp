// ============================================================================
// saQut CLI — run komutu (pipeline: token → AST → IR debug)
// ============================================================================
//
// TODO: İleride `saqut -` ile stdin'den okuyup anında çalıştıracak
//       interpreter modu bu komutun altına gelecek.
//
// ============================================================================

#ifndef SAQUT_CLI_RUN
#define SAQUT_CLI_RUN

#include <iostream>
#include "cli/args.hpp"
#include "tokenizer/tokenizer.hpp"
#include "parser/parser.hpp"
#include "ir/ir.hpp"

inline int cmdRun(const CliArgs& args) {
    std::string source = readSource(args);
    if (source.empty()) return 1;

    Tokenizer tokenizer;
    auto tokens = tokenizer.scan(source);

    std::cout << "=== saQut Compiler ===\n";
    std::cout << "Kaynak kod:\n" << source << "\n\n";

    std::cout << "Tokenler (" << tokens.size() << " adet):\n";
    for (auto* t : tokens) {
        std::cout << "  [" << t->gettype() << "] \"" << t->token << "\"\n";
    }
    std::cout << "\n";

    Parser parser;
    ASTNode* ast = parser.parse(tokens);

    if (ast) {
        std::cout << "AST:\n";
        ast->log(0);
        std::cout << "\n";

        CodeGenerator cg;
        cg.parse(ast);
        std::cout << "IR (" << cg.IROpDatas.size() << " komut):\n";
        for (size_t i = 0; i < cg.IROpDatas.size(); i++) {
            auto& op = cg.IROpDatas[i];
            std::cout << "  [" << i << "] reg" << op.targetReg << " = ";
            switch (op.op) {
                case OPCode::mathadd: std::cout << "add"; break;
                case OPCode::mathsub: std::cout << "sub"; break;
                case OPCode::mathmul: std::cout << "mul"; break;
                case OPCode::mathdiv: std::cout << "div"; break;
                case OPCode::declare: std::cout << "literal"; break;
            }
            std::cout << " (" << op.arg1.value.index() << ")\n";
        }

        delete ast;
    }

    for (auto* t : tokens) delete t;
    return 0;
}

#endif // SAQUT_CLI_RUN
