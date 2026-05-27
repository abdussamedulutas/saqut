// ============================================================================
// saQut Compiler — Parser Deklarasyonlar
// ============================================================================
//
// DİZİN:   src/parser/parser_decl.hpp
// İÇERİK:  parseFunctionDecl(), parseStructDecl(), parseVariableDecl()
//
// ============================================================================

#ifndef SAQUT_PARSER_DECL
#define SAQUT_PARSER_DECL

#include <iostream>
#include "parser/parser_base.hpp"
inline ASTNode* Parser::parseFunctionDecl() {
    FunctionDeclNode* fn = new FunctionDeclNode();
    fn->loc = currentToken().token->loc;
    fn->returnType = currentToken().token->token;  // "int", "void", ...
    nextToken();  // Dönüş tipini tüket

    fn->name = currentToken().token->token;  // "main", "calculate", ...
    nextToken();  // İsmi tüket

    // Parametre listesi: ( ... )
    if (currentToken().type == TokenType::LPAREN) {
        nextToken();  // '(' tüket
        // TODO: Parametreleri ayrıştır
        // Şimdilik ')' gelene kadar atla
        while (currentToken().type != TokenType::RPAREN &&
               currentToken().type != TokenType::SVR_VOID)
            nextToken();
        if (currentToken().type == TokenType::RPAREN)
            nextToken();  // ')' tüket
    }

    // Gövde: { ... }
    if (currentToken().type == TokenType::LBRACE) {
        ASTNode* body = parseBlock();
        fn->addChild(body);
    }

    return fn;
}
// --------------------------------------------------------------------------
// parseStructDecl: struct tanimi.
// --------------------------------------------------------------------------
inline ASTNode* Parser::parseStructDecl() {
    StructDeclNode* st = new StructDeclNode();
    st->loc = currentToken().token->loc;
    nextToken();
    if (currentToken().type == TokenType::IDENTIFIER) {
        st->name = currentToken().token->token;
        nextToken();
    }
    if (currentToken().type == TokenType::LBRACE) {
        nextToken();
        while (currentToken().type != TokenType::RBRACE && currentToken().type != TokenType::SVR_VOID) {
            ASTNode* field = parseDeclaration();
            if (field) st->addChild(field);
            else break;
        }
        if (currentToken().type == TokenType::RBRACE) nextToken();
    }
    if (currentToken().type == TokenType::SEMICOLON) nextToken();
    return st;
}


// --------------------------------------------------------------------------
// parseVariableDecl: Değişken tanımı.
//
// Sözdizimi: Type Identifier [= Expression] {, Identifier [= Expression]} ;
// Örnek:     int x = 10;
//            float y;              (initExpr = nullptr)
//            int first = 0, second = 1, next;
//
// Çoklu değişken:
//   İlk değişken ana düğüm olur. Virgülle ayrılmış ek değişkenler
//   ana düğümün children vektörüne eklenir. JSON çıktısında "declarators"
//   dizisi olarak görünür.
// --------------------------------------------------------------------------
inline ASTNode* Parser::parseVariableDecl() {
    // --- Tip ve ilk değişken adı ---
    VariableDeclNode* vd = new VariableDeclNode();
    vd->loc = currentToken().token->loc;
    vd->varType = currentToken().token->token;  // "int", "float", ...
    nextToken();  // Tipi tüket

    if (currentToken().type != TokenType::IDENTIFIER) {
        std::cerr << "Parser hatası: değişken ismi bekleniyor\n";
        return vd;
    }

    vd->name = currentToken().token->token;
    nextToken();  // İsmi tüket

    // Opsiyonel array boyutu: [expr]
    if (currentToken().type == TokenType::LBRACKET) {
        nextToken();  // '['
        while (currentToken().type != TokenType::RBRACKET &&
               currentToken().type != TokenType::SEMICOLON &&
               currentToken().type != TokenType::SVR_VOID)
            nextToken();
        if (currentToken().type == TokenType::RBRACKET)
            nextToken();  // ']'
    }

    // İlk değişkenin başlangıç değeri
    if (currentToken().type == TokenType::EQUAL) {
        nextToken();  // '=' tüket
        vd->initExpr = parseExpression();
    }

    // --- Çoklu değişken: , identifier [= expr] ---
    while (currentToken().type == TokenType::COMMA) {
        nextToken();  // ',' tüket

        if (currentToken().type != TokenType::IDENTIFIER) {
            std::cerr << "Parser hatası: virgülden sonra değişken ismi bekleniyor\n";
            break;
        }

        VariableDeclNode* sibling = new VariableDeclNode();
        sibling->loc = currentToken().token->loc;
        sibling->varType = vd->varType;  // Aynı tip
        sibling->name = currentToken().token->token;
        nextToken();  // İsmi tüket

        // Opsiyonel array boyutu: [expr]
        if (currentToken().type == TokenType::LBRACKET) {
            nextToken();  // '['
            while (currentToken().type != TokenType::RBRACKET &&
                   currentToken().type != TokenType::SEMICOLON &&
                   currentToken().type != TokenType::SVR_VOID)
                nextToken();
            if (currentToken().type == TokenType::RBRACKET)
                nextToken();  // ']'
        }

        // Başlangıç değeri
        if (currentToken().type == TokenType::EQUAL) {
            nextToken();  // '=' tüket
            sibling->initExpr = parseExpression();
        }

        // Kardeş düğümü ana düğüme ekle
        vd->addChild(sibling);
    }

    // Noktalı virgül (opsiyonel — parser hoşgörülü)
    if (currentToken().type == TokenType::SEMICOLON)
        nextToken();

    return vd;
}

// ============================================================================

#endif // SAQUT_PARSER_DECL
