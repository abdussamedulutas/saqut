// ============================================================================
// saQut Compiler — Parser Deyimler
// ============================================================================
//
// DİZİN:   src/parser/parser_stmt.hpp
// İÇERİK:  parseStatement(), parseBlock(), parseIf/While/For/DoWhile,
//          parseReturn/Break/Continue/ExpressionStatement
//
// ============================================================================

#ifndef SAQUT_PARSER_STMT
#define SAQUT_PARSER_STMT

#include <iostream>
#include "parser/parser_base.hpp"
inline ASTNode* Parser::parseStatement() {
    auto ct = currentToken();

    if (ct.type == TokenType::LBRACE)
        return parseBlock();

    if (ct.type == TokenType::KW_IF)
        return parseIfStatement();

    if (ct.type == TokenType::KW_WHILE)
        return parseWhileStatement();

    if (ct.type == TokenType::KW_FOR)
        return parseForStatement();

    if (ct.type == TokenType::KW_DO)
        return parseDoWhileStatement();

    if (ct.type == TokenType::KW_RETURN)
        return parseReturnStatement();

    if (ct.type == TokenType::KW_BREAK)
        return parseBreakStatement();

    if (ct.type == TokenType::KW_CONTINUE)
        return parseContinueStatement();

    // Değişken tanımı? (tip keyword'ü ile başlayan)
    if (ct.is({
        TokenType::KW_VOID, TokenType::KW_INT, TokenType::KW_FLOAT_TYPE,
        TokenType::KW_DOUBLE, TokenType::KW_BOOL, TokenType::KW_CHAR,
        TokenType::KW_STRING_TYPE
    })) {
        return parseVariableDecl();
    }

    // struct tanımı: struct Name { ... }
    if (ct.type == TokenType::KW_STRUCT)
        return parseStructDecl();

    // Hiçbiri değilse → ifade statement'ı (atama, fonksiyon çağrısı, ...)
    return parseExpressionStatement();
}

// --------------------------------------------------------------------------
// parseBlock: { statement* }
// --------------------------------------------------------------------------
inline ASTNode* Parser::parseBlock() {
    BlockNode* block = new BlockNode();
    block->loc = currentToken().token ? currentToken().token->loc : SourceLocation{};

    if (currentToken().type == TokenType::LBRACE)
        nextToken();  // '{' tüket

    while (currentToken().type != TokenType::RBRACE &&
           currentToken().type != TokenType::SVR_VOID) {
        ASTNode* stmt = parseStatement();
        if (stmt)
            block->addChild(stmt);
        else
            break;  // Hata durumunda döngüden çık
    }

    if (currentToken().type == TokenType::RBRACE)
        nextToken();  // '}' tüket

    return block;
}

// --------------------------------------------------------------------------
// parseIfStatement: if (expression) statement [else statement]
//
// Süslü parantez zorunlu DEĞİL — tek statement de olabilir.
//   if (x > 5) return x;  ← geçerli
//   if (x > 5) { ... }    ← geçerli
//
// TODO: Sallantılı else (dangling else) sorunu:
//   if (a) if (b) x; else y;  ← else hangi if'e ait?
//   Mevcut implementasyon doğru: else en yakın if'e bağlanır.
// --------------------------------------------------------------------------
inline ASTNode* Parser::parseIfStatement() {
    IfStatementNode* ifNode = new IfStatementNode();
    ifNode->loc = currentToken().token->loc;
    nextToken();  // 'if' tüket

    // Koşul: ( expression )
    if (currentToken().type == TokenType::LPAREN) {
        nextToken();  // '(' tüket
        ifNode->condition = parseExpression();
        if (currentToken().type == TokenType::RPAREN)
            nextToken();  // ')' tüket
    }

    // Then gövdesi
    ifNode->thenBranch = parseStatement();

    // Opsiyonel else
    if (currentToken().type == TokenType::KW_ELSE) {
        nextToken();  // 'else' tüket
        ifNode->elseBranch = parseStatement();
    }

    return ifNode;
}

// --------------------------------------------------------------------------
// parseWhileStatement: while (expression) statement
// --------------------------------------------------------------------------
inline ASTNode* Parser::parseWhileStatement() {
    WhileStatementNode* ws = new WhileStatementNode();
    ws->loc = currentToken().token->loc;
    nextToken();  // 'while' tüket

    if (currentToken().type == TokenType::LPAREN) {
        nextToken();  // '(' tüket
        ws->condition = parseExpression();
        if (currentToken().type == TokenType::RPAREN)
            nextToken();  // ')' tüket
    }

    ws->body = parseStatement();
    return ws;
}

// --------------------------------------------------------------------------
// parseForStatement: for (init; condition; update) statement
//
// for'un 3 parçası da isteğe bağlıdır:
//   for (;;) { ... }  ← sonsuz döngü (geçerli)
//
// init: VariableDeclNode veya ExpressionStatementNode
//   for (int i = 0; ...) → VariableDecl
//   for (i = 0; ...)     → ExpressionStatement
// condition: ifade (nullptr = yok)
// update: ifade (nullptr = yok)
// --------------------------------------------------------------------------
inline ASTNode* Parser::parseForStatement() {
    ForStatementNode* fs = new ForStatementNode();
    fs->loc = currentToken().token->loc;
    nextToken();  // 'for' tüket

    if (currentToken().type == TokenType::LPAREN)
        nextToken();  // '(' tüket

    // Init (opsiyonel)
    if (currentToken().type != TokenType::SEMICOLON)
        fs->init = parseStatement();
    if (currentToken().type == TokenType::SEMICOLON)
        nextToken();  // ';' tüket

    // Condition (opsiyonel)
    if (currentToken().type != TokenType::SEMICOLON)
        fs->condition = parseExpression();
    if (currentToken().type == TokenType::SEMICOLON)
        nextToken();  // ';' tüket

    // Update (opsiyonel)
    if (currentToken().type != TokenType::RPAREN)
        fs->update = parseExpression();
    if (currentToken().type == TokenType::RPAREN)
        nextToken();  // ')' tüket

    // Body
    fs->body = parseStatement();

    return fs;
}

// --------------------------------------------------------------------------
// parseDoWhileStatement: do statement while (expression) ;
// --------------------------------------------------------------------------
inline ASTNode* Parser::parseDoWhileStatement() {
    DoWhileStatementNode* dw = new DoWhileStatementNode();
    dw->loc = currentToken().token->loc;
    nextToken();  // 'do' tüket

    // Gövde
    dw->body = parseStatement();

    // while (expression) ;
    if (currentToken().type == TokenType::KW_WHILE) {
        nextToken();  // 'while' tüket
        if (currentToken().type == TokenType::LPAREN) {
            nextToken();  // '(' tüket
            dw->condition = parseExpression();
            if (currentToken().type == TokenType::RPAREN)
                nextToken();  // ')' tüket
        }
        if (currentToken().type == TokenType::SEMICOLON)
            nextToken();  // ';' tüket
    }

    return dw;
}

// --------------------------------------------------------------------------
// parseReturnStatement: return [expression] ;
//
// return;        ← value = nullptr (void fonksiyon)
// return x + 1;  ← value = BinaryExpression
// --------------------------------------------------------------------------
inline ASTNode* Parser::parseReturnStatement() {
    ReturnStatementNode* rs = new ReturnStatementNode();
    rs->loc = currentToken().token->loc;
    nextToken();  // 'return' tüket

    // Opsiyonel dönüş değeri
    // Eğer sıradaki token ; veya } ise → return;
    if (currentToken().type != TokenType::SEMICOLON &&
        currentToken().type != TokenType::RBRACE) {
        rs->value = parseExpression();
    }

    if (currentToken().type == TokenType::SEMICOLON)
        nextToken();  // ';' tüket

    return rs;
}

// --------------------------------------------------------------------------
// parseBreakStatement / parseContinueStatement
// --------------------------------------------------------------------------
inline ASTNode* Parser::parseBreakStatement() {
    BreakStatementNode* bs = new BreakStatementNode();
    bs->loc = currentToken().token->loc;
    nextToken();  // 'break' tüket
    if (currentToken().type == TokenType::SEMICOLON)
        nextToken();
    return bs;
}

inline ASTNode* Parser::parseContinueStatement() {
    ContinueStatementNode* cs = new ContinueStatementNode();
    cs->loc = currentToken().token->loc;
    nextToken();  // 'continue' tüket
    if (currentToken().type == TokenType::SEMICOLON)
        nextToken();
    return cs;
}

// --------------------------------------------------------------------------
// parseExpressionStatement: expression ;
//
// Bir ifadeyi statement olarak kullanır. Örn: x = 5;  foo();
//
// HATA KURTARMA:
//   Eğer parseExpression() başarısız olursa (nullptr), sonraki ; veya }
//   veya EOF'a kadar token'ları atlayarak senkronize olur. Bu, tek bir
//   hatalı ifadenin tüm parser'ı kilitlemesini önler.
//
//   BUG FIX (commit 438bc0e): Eskiden hatalı ifade durumunda sonsuz
//   döngüye giriyordu (parseProgram her seferinde aynı ifadeyi okuyordu).
// --------------------------------------------------------------------------
inline ASTNode* Parser::parseExpressionStatement() {
    ExpressionStatementNode* es = new ExpressionStatementNode();
    es->loc = currentToken().token ? currentToken().token->loc : SourceLocation{};
    es->expression = parseExpression();
    if (!es->expression) {
        // Hata kurtarma: sonraki güvenli noktaya atla
        while (currentToken().type != TokenType::SEMICOLON &&
               currentToken().type != TokenType::RBRACE &&
               currentToken().type != TokenType::SVR_VOID)
            nextToken();
        if (currentToken().type == TokenType::SEMICOLON)
            nextToken();
    }
    if (currentToken().type == TokenType::SEMICOLON)
        nextToken();

    return es;
}

// ============================================================================
// İfadeler — Pratt Parser (Top-Down Operator Precedence)
// ============================================================================
//
// Pratt parser'ın temel fikri: Her operatörün bir "bağlanma gücü" (precedence)
// vardır. Parser, bu güce göre operatörleri doğru sırada gruplar.
//
// NUD (Null Denotation): Prefix ifadeleri (sayılar, -, !, parantez)
// LED (Left Denotation): Infix/Postfix ifadeler (+, *, ++)
//
// ÖRNEK: 1 + 2 * 3
//   1. NUD: 1 → Literal(1)
//   2. LED(+): prec=13, right'i parseExpression(13) ile ayrıştır
//      2a. NUD: 2 → Literal(2)
//      2b. LED(*): prec=14 > 13 → parseExpression(14)
//          3a. NUD: 3 → Literal(3)
//          3b. LED yok → dön
//      2c. BinaryExpr(*, 2, 3) dön
//   3. BinaryExpr(+, 1, BinaryExpr(*, 2, 3))
//   Sonuç: 1 + (2 * 3) ✓
//
// BUG FIX (commit 40579ca): Ana döngü lookahead(1) yerine currentToken()
//   kullanıyor. NUD artık token'ı tüketip ilerliyor, bu sayede currentToken()
//   her zaman bir sonraki operatörü gösterir.
//
// BUG FIX (commit 438bc0e): Atom'lar (sayı, string, identifier) NUD'da
//   nextToken() ile tüketiliyor. Eskiden tüketilmediği için sonsuz döngü
//   oluyordu.
//
// ============================================================================

// --------------------------------------------------------------------------
// parseExpression(): Öncelik 0'dan başla (en düşük bağlanma)

#endif // SAQUT_PARSER_STMT
