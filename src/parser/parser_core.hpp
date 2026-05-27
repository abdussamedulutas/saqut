// ============================================================================
// saQut Compiler — Parser Çekirdek (Program + İfadeler)
// ============================================================================
//
// DİZİN:   src/parser/parser_core.hpp
// İÇERİK:  Token navigasyonu, parse(), parseProgram(),
//          parseDeclaration(), parseExpression() [Pratt]
//
// ============================================================================

#ifndef SAQUT_PARSER_CORE
#define SAQUT_PARSER_CORE

#include <iostream>
#include "parser/parser_base.hpp"
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

    // struct
    if (ct.type == TokenType::KW_STRUCT)
        return parseStructDecl();

    // Tip keyword'ü değil → statement
    return parseStatement();
}

// --------------------------------------------------------------------------
// parseFunctionDecl: Fonksiyon tanımı.

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
        bin->loc    = ct.token ? ct.token->loc : SourceLocation{};
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
        lit->loc       = ct.token ? ct.token->loc : SourceLocation{};
        lit->lexerToken  = ct.token;
        lit->parserToken = ct;
        // NumberToken'a cast edip base/isFloat bilgisini al
        if (auto* nt = dynamic_cast<NumberToken*>(ct.token)) {
            lit->literalBase  = nt->base;
            lit->isFloatValue = nt->isFloat;
            lit->literalType  = nt->isFloat ? LiteralType::FLOAT : LiteralType::INTEGER;
        }
        return lit;
    }

    // --- String literal: "hello" ---
    if (ct.type == TokenType::STRING) {
        nextToken();
        LiteralNode* lit = new LiteralNode();
        lit->literalType = LiteralType::STRING;
        lit->loc       = ct.token ? ct.token->loc : SourceLocation{};
        lit->lexerToken  = ct.token;
        lit->parserToken = ct;
        return lit;
    }

    // --- Boolean/null literal: true, false, null ---
    if (ct.is({TokenType::KW_TRUE, TokenType::KW_FALSE, TokenType::KW_NULL})) {
        nextToken();
        LiteralNode* lit = new LiteralNode();
        // Token içeriğine göre boolean/null ayrımı
        if (ct.is({TokenType::KW_TRUE, TokenType::KW_FALSE}))
            lit->literalType = LiteralType::BOOLEAN;
        else
            lit->literalType = LiteralType::BOŞ;
        lit->loc       = ct.token ? ct.token->loc : SourceLocation{};
        lit->lexerToken  = ct.token;
        lit->parserToken = ct;
        return lit;
    }

    // --- Identifier: x, myVar ---
    if (ct.type == TokenType::IDENTIFIER) {
        nextToken();
        IdentifierNode* id = new IdentifierNode();
        id->loc          = ct.token ? ct.token->loc : SourceLocation{};
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
    if (ct.is({TokenType::PLUS_PLUS, TokenType::MINUS_MINUS})) {
        nextToken();
        PostfixNode* pf = new PostfixNode();
        pf->loc     = ct.token ? ct.token->loc : SourceLocation{};
        pf->operand  = left;
        pf->Operator = ct.type;
        left->parent = pf;
        return pf;
    }

    // --- Fonksiyon cagrisi: expr(args) ---
    if (ct.type == TokenType::LPAREN) {
        nextToken();
        CallExpressionNode* call = new CallExpressionNode();
        call->loc    = ct.token ? ct.token->loc : SourceLocation{};
        call->callee = left;
        left->parent = call;

        if (currentToken().type != TokenType::RPAREN) {
            call->arguments.push_back(parseExpression(0));
            while (currentToken().type == TokenType::COMMA) {
                nextToken();
                call->arguments.push_back(parseExpression(0));
            }
        }
        if (currentToken().type == TokenType::RPAREN)
            nextToken();
        return call;
    }

    // --- Dizi erisimi: expr[index] ---
    if (ct.type == TokenType::LBRACKET) {
        nextToken();
        IndexExpressionNode* idx = new IndexExpressionNode();
        idx->loc     = ct.token ? ct.token->loc : SourceLocation{};
        idx->object = left;
        left->parent = idx;
        idx->index = parseExpression(0);
        if (currentToken().type == TokenType::RBRACKET)
            nextToken();
        return idx;
    }

    // --- Uye erisimi: expr.member / expr->member ---
    if (ct.type == TokenType::DOT || ct.type == TokenType::ARROW) {
        bool arrow = (ct.type == TokenType::ARROW);
        nextToken();

        if (currentToken().type != TokenType::IDENTIFIER) {
            std::cerr << "Parser hatasi: uye ismi bekleniyor\n";
            return left;
        }

        MemberAccessNode* ma = new MemberAccessNode();
        ma->loc     = ct.token ? ct.token->loc : SourceLocation{};
        ma->object = left;
        ma->member = currentToken().token->token;
        ma->arrow  = arrow;
        left->parent = ma;
        nextToken();
        return ma;
    }

    // --- Binary infix: expr OP expr ---
    uint16_t prec = ct.getPowerOperator();
    nextToken();

    ASTNode* right = parseExpression(prec);

    BinaryExpressionNode* bin = new BinaryExpressionNode();
    bin->loc      = ct.token ? ct.token->loc : SourceLocation{};
    bin->Left     = left;
    bin->Right    = right;
    bin->Operator = ct.type;
    if (left)  left->parent  = bin;
    if (right) right->parent = bin;
    return bin;
}


#endif // SAQUT_PARSER_CORE
