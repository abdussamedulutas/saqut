#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "tokenizer/tokenizer.hpp"
#include "parser/parser.hpp"
#include "ir/ir.hpp"

int main() {
    // Read source file
    std::ifstream file("source.sqt", std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Hata: source.sqt dosyası açılamadı\n";
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    file.close();

    std::cout << "=== saQut Compiler ===\n";
    std::cout << "Kaynak kod:\n" << source << "\n\n";

    // Lexing → Tokenizing
    Tokenizer tokenizer;
    auto tokens = tokenizer.scan(source);

    std::cout << "Tokenler (" << tokens.size() << " adet):\n";
    for (auto* t : tokens) {
        std::cout << "  [" << t->gettype() << "] \"" << t->token << "\"\n";
    }
    std::cout << "\n";

    // Parsing → AST
    Parser parser;
    ASTNode* ast = parser.parse(tokens);

    if (ast) {
        std::cout << "AST:\n";
        ast->log(0);
        std::cout << "\n";

        // IR generation
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
    }

    // Cleanup
    for (auto* t : tokens) delete t;

    return 0;
}
