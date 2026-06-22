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
class Parser {
public:
    ASTNode* parse(TokenList tokens);

private:
    TokenList tokens;      // Tokenizer'dan gelen token listesi
    int current = 0;       // Şu anki token indeksi

    // --- Token navigasyonu ---
    ParserToken currentToken();
    void        nextToken();
    ParserToken lookahead(uint32_t forward);
    ParserToken parseToken(Token* token);
    ParserToken getToken(int offset);

    // --- Üst seviye ---
    ASTNode* parseProgram();

    // --- Deklarasyonlar ---
    ASTNode* parseDeclaration();
    ASTNode* parseFunctionDecl();
    ASTNode* parseStructDecl();
    ASTNode* parseEnumDecl();
    ASTNode* parseVariableDecl();

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
