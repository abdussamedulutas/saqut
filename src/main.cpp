// ============================================================================
// saQut Compiler — Giriş Noktası (main)
// ============================================================================
//
// DİZİN:   src/main.cpp
// KATMAN:  En üst — tüm alt katmanları birleştirir
// BAĞIMLI: Tokenizer, Parser, IR (ve dolaylı olarak Lexer, AST, Token)
//
// AMAÇ:
//   Derleyici pipeline'ını başlatır:
//   1. source.sqt dosyasını oku
//   2. Lexing + Tokenizing
//   3. Parsing (AST üretimi)
//   4. IR üretimi
//   5. Sonuçları konsola yazdır (debug modu)
//
// KULLANIM:
//   ./saqut              → source.sqt dosyasını derler
//   echo "1+2" > source.sqt && ./saqut  → hızlı test
//
// GELECEK:
//   - Komut satırı argümanları: ./saqut file.sqt -o output
//   - Mod seçimi: ./saqut --mode=parse|ir|compile|run
//   - Birden fazla dosya: ./saqut file1.sqt file2.sqt
//
// ============================================================================

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "tokenizer/tokenizer.hpp"
#include "parser/parser.hpp"
#include "ir/ir.hpp"

int main() {
    // ------------------------------------------------------------------
    // 1. Kaynak dosyayı oku
    // ------------------------------------------------------------------
    // Şimdilik sabit dosya adı: source.sqt.
    // TODO: argc/argv ile dosya adı al.
    // ------------------------------------------------------------------
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

    // ------------------------------------------------------------------
    // 2. Lexing → Tokenizing
    // ------------------------------------------------------------------
    // Tokenizer, Lexer'ı içerir. scan() tüm pipeline'ı çalıştırır.
    // Token'lar heap'te new ile oluşturulur, iş bitince silinmeli.
    // ------------------------------------------------------------------
    Tokenizer tokenizer;
    auto tokens = tokenizer.scan(source);

    std::cout << "Tokenler (" << tokens.size() << " adet):\n";
    for (auto* t : tokens) {
        std::cout << "  [" << t->gettype() << "] \"" << t->token << "\"\n";
    }
    std::cout << "\n";

    // ------------------------------------------------------------------
    // 3. Parsing → AST
    // ------------------------------------------------------------------
    // Parser, token listesini alır, AST üretir.
    // parse() artık parseProgram()'ı çağırır — birden fazla deklarasyon
    // veya statement içeren tam programları ayrıştırabilir.
    // ------------------------------------------------------------------
    Parser parser;
    ASTNode* ast = parser.parse(tokens);

    if (ast) {
        std::cout << "AST:\n";
        ast->log(0);  // Ağacı girintili olarak yazdır
        std::cout << "\n";

        // ------------------------------------------------------------------
        // 4. IR Üretimi
        // ------------------------------------------------------------------
        // CodeGenerator AST'yi dolaşır, sanal register makine komutları üretir.
        // Şu anda sadece matematik işlemleri ve literal'lar destekleniyor.
        // ------------------------------------------------------------------
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
            // arg1.value.index(): 0=int, 1=float
            std::cout << " (" << op.arg1.value.index() << ")\n";
        }
    }

    // ------------------------------------------------------------------
    // 5. Temizlik
    // ------------------------------------------------------------------
    // Token'lar heap'te oluşturuldu, manuel silinmeli.
    // TODO: std::unique_ptr ile otomatik bellek yönetimi.
    // ------------------------------------------------------------------
    for (auto* t : tokens) delete t;

    return 0;
}
