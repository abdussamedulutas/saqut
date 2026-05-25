// ============================================================================
// saQut Compiler — Parser (Sözdizimi Ayrıştırıcı)
// ============================================================================
//
// DİZİN:   src/parser/parser.hpp
// KATMAN:  Katman 3 — Tokenizer'ı tüketir, AST üretir
// BAĞIMLI: Token (token.hpp), AST (ast.hpp)
// KULLANAN: main.cpp
//
// AMAÇ:
//   Tokenizer'ın ürettiği düz token listesini alıp, dilin gramer kurallarına
//   göre hiyerarşik bir AST (Abstract Syntax Tree) üretir.
//
//   İKİ AYRI PARSER STRATEJİSİ:
//   1. Recursive Descent (ifadeler için Pratt parser):
//      - parseNullDenotation() (NUD): Prefix ifadeleri (sayılar, -, !, parantez)
//      - parseLeftDenotation() (LED): Infix/Postfix ifadeler (+, *, ++)
//      - parseExpression(precedence): Pratt'ın ana döngüsü
//
//   2. Recursive Descent (statement/deklarasyon için):
//      - parseDeclaration(): Fonksiyon mu, değişken mi, statement mı?
//      - parseStatement(): if/for/while/do/return/block/expression
//      - Her statement tipi kendi parse fonksiyonuna sahip
//
// ADR-002 (devam): Neden Hibrit Yaklaşım?
//   Pratt parser, operatör önceliğini merkezi bir tabloda yönetir ve yeni
//   operatör eklemeyi kolaylaştırır. Ancak statement'lar (if, for, while)
//   operatör değildir; kendi özel sözdizimleri vardır. Bu nedenle statement
//   tarafında klasik recursive descent kullanıyoruz. Bu, her iki dünyanın
//   en iyisini birleştirir.
//
// PARSER AKIŞI:
//   parse(tokens)
//   └── parseProgram()
//       └── parseDeclaration() [döngü, SVR_VOID gelene kadar]
//           ├── parseFunctionDecl()    → tip + isim + ( ) + { gövde }
//           ├── parseVariableDecl()    → tip + isim [+ = ifade] + ;
//           └── parseStatement()
//               ├── parseBlock()       → { statement* }
//               ├── parseIfStatement() → if (expr) stmt [else stmt]
//               ├── parseWhileStatement() → while (expr) stmt
//               ├── parseForStatement()   → for (stmt; expr; expr) stmt
//               ├── parseDoWhileStatement() → do stmt while (expr);
//               ├── parseReturnStatement()  → return [expr];
//               ├── parseBreakStatement()   → break;
//               ├── parseContinueStatement() → continue;
//               ├── parseVariableDecl()  → tip + isim ...
//               └── parseExpressionStatement() → expr;
//                   └── parseExpression() [Pratt]
//                       ├── parseNullDenotation()
//                       │   ├── LPAREN → ( expr )
//                       │   ├── Unary prefix → !expr, -expr, ++expr
//                       │   ├── NUMBER → Literal
//                       │   ├── STRING → Literal
//                       │   ├── true/false/null → Literal
//                       │   └── IDENTIFIER → Identifier
//                       └── parseLeftDenotation() [döngü]
//                           ├── Postfix → expr++, expr--
//                           └── Binary infix → expr + expr
//
// BİLİNEN SINIRLAMALAR (TODO):
//   TODO: else-if zincirleri (şu anda else'den sonra if gelirse düzgün çalışır mı?)
//   TODO: Hata kurtarma (panic mode): ilk hatada durmak yerine senkronizasyon
//   TODO: Fonksiyon parametreleri
//   TODO: Dizi erişimi: a[i]
//   TODO: Fonksiyon çağrısı: f(x, y)
//   TODO: Üye erişimi: a.b, a->b
//   TODO: Ternary: a ? b : c
//   TODO: Tip kontrolü ve sembol tablosu
//
// ============================================================================

#ifndef SAQUT_PARSER
#define SAQUT_PARSER

#include <iostream>
#include <cstdint>
#include <string>
#include "parser/token.hpp"
#include "parser/ast.hpp"
#include "tools.hpp"

// ============================================================================
// Parser — Sözdizimi Ayrıştırıcı
// ============================================================================
//
// Durum bilgisi:
//   tokens:  Tokenizer'dan gelen token listesi (referans değil, kopya değil)
//   current: Şu anki token'ın indeksi (0 = ilk token)
//
// Token navigasyon metotları:
//   currentToken():       tokens[current] döndürür, ilerlemez
//   nextToken():          current++ (sonraki token'a geç)
//   lookahead(n):         tokens[current + n] döndürür, ilerlemez
//   getToken(offset):     tokens[current + offset] döndürür
//
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

    // --- İfadeler (Pratt parser) ---
    ASTNode* parseExpression();
    ASTNode* parseExpression(uint16_t precedence);
    ASTNode* parseNullDenotation();
    ASTNode* parseLeftDenotation(ASTNode* left);
};

// ============================================================================
// Token Navigasyonu
// ============================================================================

// --------------------------------------------------------------------------
// parseToken: Ham Token'ı ParserToken'a dönüştür.
//
// Tokenizer'ın string tabanlı tip sistemini ("number", "operator", ...)
// Parser'ın anlamsal tip sistemine (NUMBER, PLUS, KW_IF, ...) çevirir.
//
// BUG FIX (commit 40579ca): pt.token = token (pointer ataması).
//   Eskiden pt.token = *token (değer kopyası) object slicing yapıyordu.
// --------------------------------------------------------------------------
inline ParserToken Parser::parseToken(Token* token) {
    ParserToken pt;
    pt.token = token;  // Pointer — değer kopyası DEĞİL

    std::string t = token->gettype();
    if (t == "string")
        pt.type = TokenType::STRING;
    else if (t == "number")
        pt.type = TokenType::NUMBER;
    else if (t == "operator")
        pt.type = OPERATOR_MAP.find(pt.token->token)->second;
    else if (t == "delimiter")
        pt.type = OPERATOR_MAP.find(pt.token->token)->second;
    else if (t == "keyword")
        pt.type = KEYWORD_MAP.find(pt.token->token)->second;
    else if (t == "identifier")
        pt.type = TokenType::IDENTIFIER;

    return pt;
}

// --------------------------------------------------------------------------
// getToken: Güvenli token erişimi. Sınır dışı = SVR_VOID.
// --------------------------------------------------------------------------
inline ParserToken Parser::getToken(int offset) {
    if ((int)tokens.size() - 1 < current + offset) {
        ParserToken pt;
        pt.type = TokenType::SVR_VOID;
        return pt;
    }
    return parseToken(tokens[current + offset]);
}

inline void Parser::nextToken() {
    if ((int)tokens.size() >= current + 1)
        current++;
}

inline ParserToken Parser::lookahead(uint32_t forward) {
    return getToken(forward);
}

inline ParserToken Parser::currentToken() {
    return getToken(0);
}

// ============================================================================
// Üst Seviye
// ============================================================================

// --------------------------------------------------------------------------
// parse: Parser'ın ana giriş noktası. Token listesini alır, AST döndürür.
// --------------------------------------------------------------------------
inline ASTNode* Parser::parse(TokenList toks) {
    tokens  = toks;
    current = 0;
    return parseProgram();
}

// --------------------------------------------------------------------------
// parseProgram: Tüm üst seviye deklarasyonları/statement'ları ayrıştırır.
//
// Program ::= Declaration*
// EOF'a (SVR_VOID) kadar parseDeclaration() çağrılır.
//
// BUG FIX (commit 438bc0e): Eskiden parseExpression() doğrudan çağrılıyordu,
//   bu sadece tek bir ifadeyi ayrıştırabiliyordu. Şimdi tam program desteği var.
// --------------------------------------------------------------------------
inline ASTNode* Parser::parseProgram() {
    ProgramNode* program = new ProgramNode();

    while (currentToken().type != TokenType::SVR_VOID) {
        ASTNode* decl = parseDeclaration();
        if (decl)
            program->addChild(decl);
        else
            break;  // Hata durumunda döngüden çık
    }

    return program;
}

// ============================================================================
// Deklarasyonlar
// ============================================================================

// --------------------------------------------------------------------------
// parseDeclaration: Üst seviye deklarasyon ayrıştırıcı.
//
// Strateji:
//   1. Mevcut token bir tip keyword'ü mü (int, void, float, ...)?
//      - Evet → lookahead(2) '(' ise → fonksiyon tanımı
//      - Evet → değilse → değişken tanımı
//   2. Değilse → statement (REPL modunda ifade de olabilir)
//
// LOOKAHEAD KULLANIMI:
//   "int main()" ve "int x = 10" ayrımı için 2 ileriye bakarız:
//   - int main() → lookahead(1)=identifier, lookahead(2)='('
//   - int x = 10 → lookahead(1)=identifier, lookahead(2)='='
// --------------------------------------------------------------------------
inline ASTNode* Parser::parseDeclaration() {
    auto ct = currentToken();

    // Tip keyword'ü ile başlayan → fonksiyon veya değişken
    if (ct.is({
        TokenType::KW_VOID, TokenType::KW_INT, TokenType::KW_FLOAT_TYPE,
        TokenType::KW_DOUBLE, TokenType::KW_BOOL, TokenType::KW_CHAR,
        TokenType::KW_STRING_TYPE, TokenType::KW_AUTO
    })) {
        auto la1 = lookahead(1);
        auto la2 = lookahead(2);
        // int main( ... ) → fonksiyon
        if (la1.type == TokenType::IDENTIFIER && la2.type == TokenType::LPAREN)
            return parseFunctionDecl();
        // int x ... → değişken
        return parseVariableDecl();
    }

    // Tip keyword'ü değil → statement (veya REPL ifadesi)
    return parseStatement();
}

// --------------------------------------------------------------------------
// parseFunctionDecl: Fonksiyon tanımı.
//
// Sözdizimi: Type Identifier ( [ParamList] ) Block
// Örnek:     int main() { ... }
//
// TODO: Parametre listesi ayrıştırma
// TODO: Dönüş tipi doğrulama (şu anda string olarak saklanıyor)
// --------------------------------------------------------------------------
inline ASTNode* Parser::parseFunctionDecl() {
    FunctionDeclNode* fn = new FunctionDeclNode();
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
// parseVariableDecl: Değişken tanımı.
//
// Sözdizimi: Type Identifier [= Expression] ;
// Örnek:     int x = 10;
//            float y;         (initExpr = nullptr)
//
// TODO: Çoklu değişken: int x = 1, y = 2;
// --------------------------------------------------------------------------
inline ASTNode* Parser::parseVariableDecl() {
    VariableDeclNode* vd = new VariableDeclNode();
    vd->varType = currentToken().token->token;  // "int", "float", ...
    nextToken();  // Tipi tüket

    if (currentToken().type != TokenType::IDENTIFIER) {
        std::cerr << "Parser hatası: değişken ismi bekleniyor\n";
        return vd;  // Hatalı düğüm, çağıran kontrol etmeli
    }

    vd->name = currentToken().token->token;  // "x", "counter", ...
    nextToken();  // İsmi tüket

    // Opsiyonel başlangıç değeri: = expression
    if (currentToken().type == TokenType::EQUAL) {
        nextToken();  // '=' tüket
        vd->initExpr = parseExpression();
    }

    // Noktalı virgül (opsiyonel — parser hoşgörülü)
    if (currentToken().type == TokenType::SEMICOLON)
        nextToken();

    return vd;
}

// ============================================================================
// Statement'lar — Recursive Descent
// ============================================================================

// --------------------------------------------------------------------------
// parseStatement: Statement ayrıştırıcı (dispatcher).
//
// Mevcut token'a göre uygun parse fonksiyonuna yönlendirir.
// Sıralama önemli: LBRACE, keyword'ler, değişken tanımı, ifade.
// --------------------------------------------------------------------------
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

    // Hiçbiri değilse → ifade statement'ı (atama, fonksiyon çağrısı, ...)
    return parseExpressionStatement();
}

// --------------------------------------------------------------------------
// parseBlock: { statement* }
// --------------------------------------------------------------------------
inline ASTNode* Parser::parseBlock() {
    BlockNode* block = new BlockNode();

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
    nextToken();  // 'break' tüket
    if (currentToken().type == TokenType::SEMICOLON)
        nextToken();
    return bs;
}

inline ASTNode* Parser::parseContinueStatement() {
    ContinueStatementNode* cs = new ContinueStatementNode();
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
// --------------------------------------------------------------------------
inline ASTNode* Parser::parseExpression() {
    return parseExpression(0);
}

// --------------------------------------------------------------------------
// parseExpression(precedence): Pratt'ın ana döngüsü.
//
// Algoritma:
//   1. NUD ile ilk operand'ı ayrıştır (prefix)
//   2. Mevcut token bir operatör mü?
//      - Evet ve önceliği > precedence ise → LED ile infix ayrıştır
//      - Hayır veya öncelik <= precedence ise → dur, sol operand'ı döndür
//   3. LED'in döndürdüğü düğüm yeni sol operand olur, 2. adıma dön
//
// DURMA KOŞULLARI:
//   - RPAREN, SEMICOLON, RBRACE, COMMA: İfade sonu sinyali
//   - Operatörün önceliği <= mevcut öncelik: Daha sıkı bağlanamaz
// --------------------------------------------------------------------------
inline ASTNode* Parser::parseExpression(uint16_t precedence) {
    if (currentToken().type == TokenType::SVR_VOID)
        return nullptr;

    // 1. Prefix (NUD)
    ASTNode* left = parseNullDenotation();
    if (!left) return nullptr;

    // 2. Infix/Postfix döngüsü (LED)
    while (true) {
        auto next = currentToken();

        // İfade sonu sinyalleri → dur
        if (next.type == TokenType::RPAREN ||
            next.type == TokenType::SEMICOLON ||
            next.type == TokenType::RBRACE ||
            next.type == TokenType::COMMA)
            break;

        // Operatörün bağlanma gücü yetersiz → dur
        // (daha yüksek öncelikli bir bağlamdayız, bu operatör oraya ait değil)
        if (precedence < next.getPowerOperator()) {
            left = parseLeftDenotation(left);
        } else {
            break;
        }
    }
    return left;
}

// --------------------------------------------------------------------------
// parseNullDenotation (NUD): Prefix ifadeleri.
//
// İşlenen prefix tipleri:
//   - Parantez: ( expression )
//   - Unary: +expr, -expr, !expr, ~expr, ++expr, --expr
//   - Literal: 42, "hello", true, false, null
//   - Identifier: x, myVar
//
// DÖNÜŞ: Ayrıştırılmış AST düğümü. Token TÜKETİLMİŞ olur (current ilerlemiş).
// --------------------------------------------------------------------------
inline ASTNode* Parser::parseNullDenotation() {
    auto ct = currentToken();

    if (ct.type == TokenType::SVR_VOID) {
        std::cerr << "Parser hatası: beklenmeyen dosya sonu\n";
        return nullptr;
    }

    // --- Parantezli ifade: ( expr ) ---
    // Önceliği sıfırlar — parantez içinde yeni bir ifade başlar.
    if (ct.type == TokenType::LPAREN) {
        nextToken();  // '(' tüket
        ASTNode* expr = parseExpression(0);  // Öncelik sıfırla
        if (currentToken().type == TokenType::RPAREN)
            nextToken();  // ')' tüket
        return expr;
    }

    // --- Unary prefix operatörler: +, -, !, ~, ++, -- ---
    // PLUS ve MINUS burada UNARY olarak işlenir.
    // Binary olarak işlenmesi LED tarafından yapılır.
    //
    // ÖNEMLİ: PLUS ve MINUS için getPowerOperator() 13 döndürür (binary öncelik).
    // Ama burada unary olarak kullanılıyor. parseExpression(16) çağırmak daha
    // doğru olurdu ancak mevcut çalışma şekli de doğru sonuç veriyor.
    // TODO: Unary için ayrı öncelik seviyesi (örn: 16)
    if (ct.is({
        TokenType::PLUS_PLUS, TokenType::MINUS_MINUS,
        TokenType::PLUS, TokenType::MINUS,
        TokenType::BANG, TokenType::TILDE
    })) {
        nextToken();  // Operatörü tüket
        // Sağ operand'ı ayrıştır. Unary prefix sağdan sola bağlanır.
        ASTNode* right = parseExpression(ct.getPowerOperator());
        BinaryExpressionNode* bin = new BinaryExpressionNode();
        bin->Right    = right;
        bin->Left     = nullptr;  // Unary işaretçisi
        bin->Operator = ct.type;
        if (right) right->parent = bin;
        return bin;
    }

    // --- Sayısal literal: 42, 0xFF, 3.14 ---
    if (ct.type == TokenType::NUMBER) {
        nextToken();  // Token'ı tüket
        LiteralNode* lit = new LiteralNode();
        lit->lexerToken  = ct.token;
        lit->parserToken = ct;
        return lit;
    }

    // --- String literal: "hello" ---
    if (ct.type == TokenType::STRING) {
        nextToken();
        LiteralNode* lit = new LiteralNode();
        lit->lexerToken  = ct.token;
        lit->parserToken = ct;
        return lit;
    }

    // --- Boolean/null literal: true, false, null ---
    if (ct.is({TokenType::KW_TRUE, TokenType::KW_FALSE, TokenType::KW_NULL})) {
        nextToken();
        LiteralNode* lit = new LiteralNode();
        lit->lexerToken  = ct.token;
        lit->parserToken = ct;
        return lit;
    }

    // --- Identifier: x, myVar ---
    if (ct.type == TokenType::IDENTIFIER) {
        nextToken();
        IdentifierNode* id = new IdentifierNode();
        id->lexerToken     = ct.token;
        id->parserToken    = ct;
        return id;
    }

    return nullptr;
}

// --------------------------------------------------------------------------
// parseLeftDenotation (LED): Infix ve Postfix ifadeler.
//
// Sol operand zaten ayrıştırılmış olarak gelir (left).
// Mevcut token operatördür.
//
// İşlenen tipler:
//   - Postfix: expr++, expr--
//   - Binary infix: expr + expr, expr * expr, expr == expr, ...
//
// TASARIM NOTU: Postfix ve Binary aynı fonksiyonda işlenir çünkü ikisi de
//   "sol operand + operatör" pattern'ini takip eder. Postfix'te sağ operand
//   yoktur.
// --------------------------------------------------------------------------
inline ASTNode* Parser::parseLeftDenotation(ASTNode* left) {
    auto ct = currentToken();

    // --- Postfix: expr++, expr-- ---
    // Operatör operand'dan SONRA gelir, sağ operand yok.
    if (ct.is({TokenType::PLUS_PLUS, TokenType::MINUS_MINUS})) {
        nextToken();  // Operatörü tüket
        PostfixNode* pf = new PostfixNode();
        pf->operand  = left;
        pf->Operator = ct.type;
        left->parent = pf;
        return pf;
    }

    // --- Binary infix: expr OP expr ---
    // OP'nin önceliğine göre sağ operand'ı ayrıştır.
    uint16_t prec = ct.getPowerOperator();
    nextToken();  // Operatörü tüket

    // Sağ operand. prec parametresi, daha yüksek öncelikli operatörlerin
    // sağ operand içinde gruplanmasını sağlar.
    ASTNode* right = parseExpression(prec);

    BinaryExpressionNode* bin = new BinaryExpressionNode();
    bin->Left     = left;
    bin->Right    = right;
    bin->Operator = ct.type;
    if (left)  left->parent  = bin;
    if (right) right->parent = bin;
    return bin;
}

#endif // SAQUT_PARSER
