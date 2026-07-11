// ============================================================================
// saQut Compiler — Parser Sınıf Tanımı
// ============================================================================
//
// DİZİN:   src/parser/parser_base.hpp
// İÇERİK:  Parser sınıf tanımı + include'lar. Metot gövdeleri yok.
//
// ============================================================================

#ifndef SAQUT_PARSER_BASE
#define SAQUT_PARSER_BASE

#include <iostream>
#include <cstdint>
#include <string>
#include "parser/token.hpp"
#include "parser/ast.hpp"
#include "tools.hpp"
#include "diagnostic/diagnostic_engine.hpp"

class Parser {
public:
    Parser() = default;
    // diag: opsiyonel — verilmezse (nullptr) eski davranış korunur: sözdizimi
    // hataları std::cerr'e yazılır (CLI komutlarının bir kısmı hâlâ diag
    // vermeden Parser kurar; bkz. cli/commands/ast.hpp, symbols.hpp, exec.hpp).
    // ModuleLoader gibi konumlu tanı isteyen çağıranlar diag verir (Faz 2).
    explicit Parser(DiagnosticEngine* diag) : diag_(diag) {}

    ASTNode* parse(TokenList tokens);

private:
    TokenList tokens;      // Tokenizer'dan gelen token listesi
    int current = 0;       // Şu anki token indeksi

    DiagnosticEngine* diag_ = nullptr; // Faz 2: konumlu sözdizimi tanıları (E9xx)
    SourceLocation lastLoc_;           // en son tüketilen gerçek token'ın konumu
                                        // (EOF'ta "nerede beklendiği"ni raporlamak için)

    // --- Token navigasyonu ---
    ParserToken currentToken();
    void        nextToken();
    ParserToken lookahead(uint32_t forward);
    ParserToken parseToken(Token* token);
    ParserToken getToken(int offset);

    // --- Faz 2: sözdizimi hata raporlama + panic-mode kurtarma ---
    // reportError: diag_ varsa konumlu Diagnostic üretir; yoksa eski cerr davranışı.
    void reportError(const SourceLocation& loc, const std::string& code,
                      const std::string& message);
    // synchronizeAndMakeError: konumlu tanı üretir, ';'/'}'/statement-başlangıcı
    // token'ına kadar (en az bir token ilerleyerek) atlar ve bir ErrorNode döner.
    ASTNode* synchronizeAndMakeError(const SourceLocation& loc, const std::string& code,
                                       const std::string& message);

    // --- Üst seviye ---
    ASTNode* parseProgram();

    // --- Deklarasyonlar ---
    ASTNode* parseDeclaration();
    ASTNode* parseFunctionDecl();
    ASTNode* parseStructDecl();
    ASTNode* parseEnumDecl();
    ASTNode* parseVariableDecl();
    ASTNode* parseImportDecl();   // import {name, ...} from "file.sqt";
    ASTNode* parseExportDecl();   // export struct/enum/function ...

    // --- Statement'lar ---
    ASTNode* parseStatement();
    ASTNode* parseBlock();
    ASTNode* parseIfStatement();
    ASTNode* parseWhileStatement();
    ASTNode* parseForStatement();
    ASTNode* parseDoWhileStatement();
    ASTNode* parseReturnStatement();
    ASTNode* parseBreakStatement();
    ASTNode* parseContinueStatement();
    ASTNode* parseExpressionStatement();
    ASTNode* parseTryStatement();
    ASTNode* parseThrowStatement();
    ASTNode* parseSwitchStatement();

    // --- İfadeler (Pratt parser) ---
    ASTNode* parseExpression();
    ASTNode* parseExpression(uint16_t precedence);
    ASTNode* parseNullDenotation();
    ASTNode* parseLeftDenotation(ASTNode* left);
};

#endif // SAQUT_PARSER_BASE
