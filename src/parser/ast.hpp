// ============================================================================
// saQut Compiler — Soyut Sözdizim Ağacı (AST)
// ============================================================================
//
// DİZİN:   src/parser/ast.hpp
// KATMAN:  Katman 3 — Parser'ın ürettiği, IR'nin tükettiği
// BAĞIMLI: Token (src/parser/token.hpp), Tools (src/tools.hpp)
// KULLANAN: Parser (src/parser/parser.hpp), IR (src/ir/ir.hpp)
//
// AMAÇ:
//   Kaynak kodun hiyerarşik, anlamsal gösterimi. Her dil yapısı (ifade,
//   deyim, fonksiyon) bir AST düğümü ile temsil edilir.
//
// AST DÜĞÜM HİYERARŞİSİ:
//   ASTNode (soyut taban)
//   ├── ProgramNode            : Kök düğüm, tüm üst seviye deklarasyonları tutar
//   ├── FunctionDeclNode       : Fonksiyon tanımı (int main() { ... })
//   ├── BlockNode              : { ... } bloğu, statement listesi
//   ├── VariableDeclNode       : Değişken tanımı (int x = 10;)
//   ├── IfStatementNode        : if/else
//   ├── WhileStatementNode     : while döngüsü
//   ├── ForStatementNode       : for döngüsü
//   ├── DoWhileStatementNode   : do-while döngüsü
//   ├── ReturnStatementNode    : return [ifade]
//   ├── BreakStatementNode     : break
//   ├── ContinueStatementNode  : continue
//   ├── ExpressionStatementNode: ifade + ; (bir statement olarak)
//   ├── BinaryExpressionNode   : İkili işlem (a + b, a * b)
//   ├── LiteralNode            : Sabit değer (42, "hello", true)
//   ├── IdentifierNode         : Değişken/fonksiyon ismi
//   └── PostfixNode            : Son ek işlem (a++, a--)
//
// TASARIM KARARLARI:
//   1. ASTKind enum: Her düğüm tipi için bir enum değeri.
//      RTTI (dynamic_cast) yerine manuel tip kontrolü sağlar.
//      Daha hızlı ve hata ayıklaması kolay.
//
//   2. parent pointer: Her düğüm ebeveynini bilir.
//      Yukarı doğru gezinme (ör: bir döngü içinde break'in hedefini bulma).
//
//   3. children vektörü (protected): Sadece addChild() ile ekleme.
//      ProgramNode, FunctionDeclNode, BlockNode gibi liste tutan düğümler
//      bu vektörü kullanır. İkili işlem gibi sabit sayıda çocuğu olan
//      düğümler kendi üye değişkenlerini kullanır (Left, Right).
//
//   4. log() metodu: Her düğüm kendi alt ağacını girintili olarak yazdırır.
//      Debug ve test için. Gerçek kod üretimi için kullanılmaz.
//
// BİLİNEN SINIRLAMALAR (TODO):
//   TODO: Bellek yönetimi: AST düğümleri heap'te new ile oluşturuluyor,
//         silme sorumluluğu yok (sızıntı). unique_ptr veya arena allocator.
//   TODO: Ziyaretçi deseni (Visitor pattern) eklenerek log() ve IR
//         üretimi ayrı sınıflara taşınabilir.
//
// ============================================================================

#ifndef SAQUT_AST
#define SAQUT_AST

#include <iostream>
#include <vector>
#include "parser/token.hpp"
#include "tools.hpp"

// ============================================================================
// ASTKind — AST Düğüm Tipi Enum'u
// ============================================================================
//
// Her AST düğüm sınıfı, constructor'ında kendi kind değerini atar.
// CodeGenerator (IR) ve diğer AST işlemcileri, düğümün tipini bu enum
// üzerinden belirler.
//
// İsimlendirme: Düğüm sınıf adları "Node" ile biter, enum değerleri bitmez.
//   Örn: sınıf=IfStatementNode, enum=IfStatement
//
enum class ASTKind {
    Program,              // Kök düğüm
    FunctionDecl,         // Fonksiyon tanımı
    Block,                // { } bloğu
    VariableDecl,         // Değişken tanımı
    BinaryExpression,     // İkili işlem (a + b)
    UnaryExpression,      // Tekli işlem (-a, !a) — ileride kullanılacak
    Literal,              // Sabit değer
    Identifier,           // İsim referansı
    Postfix,              // Son ek (a++)
    IfStatement,          // if/else
    ForStatement,         // for
    WhileStatement,       // while
    DoWhileStatement,     // do-while
    ReturnStatement,      // return
    BreakStatement,       // break
    ContinueStatement,    // continue
    ExpressionStatement,  // ifade + ;
};

// ============================================================================
// ASTNode — Soyut Temel Sınıf
// ============================================================================
//
// Tüm AST düğümlerinin ortak atası. Minimum arayüz:
//   - kind: Düğüm tipi (ASTKind enum)
//   - parent: Ebeveyn düğüm (kök için nullptr)
//   - addChild() / getChildren(): Çocuk yönetimi
//   - log(): Debug çıktısı (virtual, her alt sınıf override eder)
//
class ASTNode {
public:
    ASTKind kind;              // Düğüm tipi (alt sınıf constructor'ında atanır)
    ASTNode* parent = nullptr; // Ebeveyn düğüm (kök = nullptr)

    virtual void log(int indent = 0) {
        (void)indent;          // Kullanılmayan parametre uyarısını sustur
        std::cout << "<Unknown>\n";
    }

    // Çocuk ekleme. Otomatik olarak parent pointer'ı ayarlar.
    void addChild(ASTNode* child) {
        children.push_back(child);
        child->parent = this;
    }

    std::vector<ASTNode*>& getChildren() { return children; }

    virtual ~ASTNode() = default;

protected:
    std::vector<ASTNode*> children;  // Alt düğümler (liste tipi düğümler için)
};

// ============================================================================
// ProgramNode — Kök Düğüm
// ============================================================================
//
// Her saQut programı tek bir ProgramNode ile başlar.
// Çocukları: FunctionDeclNode, VariableDeclNode (global), ExpressionStatement.
//
class ProgramNode : public ASTNode {
public:
    ProgramNode() { kind = ASTKind::Program; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "Program\n";
        for (auto* c : getChildren())
            c->log(indent + 2);
    }
};

// ============================================================================
// FunctionDeclNode — Fonksiyon Tanımı
// ============================================================================
//
// Örnek: int main() { ... }
//   returnType: "int", "void", "float", ...
//   name:       "main", "calculate", ...
//   children:   gövde (genellikle tek bir BlockNode)
//
// TODO: Parametre listesi (şu anda boş)
//
class FunctionDeclNode : public ASTNode {
public:
    std::string name;        // Fonksiyon adı
    std::string returnType;  // Dönüş tipi (string olarak, ileride tip sistemi)

    FunctionDeclNode() { kind = ASTKind::FunctionDecl; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent)
                  << "FunctionDecl " << returnType << " " << name << "()\n";
        for (auto* c : getChildren())
            c->log(indent + 2);
    }
};

// ============================================================================
// BlockNode — Blok { ... }
// ============================================================================
//
// Bir dizi statement'i gruplar. Kendi scope (kapsam) alanı oluşturur.
// Örnek: { int x = 1; x = x + 2; }
//
class BlockNode : public ASTNode {
public:
    BlockNode() { kind = ASTKind::Block; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "Block\n";
        for (auto* c : getChildren())
            c->log(indent + 2);
    }
};

// ============================================================================
// VariableDeclNode — Değişken Tanımı
// ============================================================================
//
// Örnek: int x = 10;
//   varType:  "int", "float", "bool", ...
//   name:     "x", "counter", ...
//   initExpr: Başlangıç değeri (nullptr = tanımsız, örn: int x;)
//
class VariableDeclNode : public ASTNode {
public:
    std::string varType;         // Değişken tipi
    std::string name;            // Değişken adı
    ASTNode*   initExpr = nullptr; // Başlangıç ifadesi (opsiyonel)

    VariableDeclNode() { kind = ASTKind::VariableDecl; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent)
                  << "VariableDecl " << varType << " " << name;
        if (initExpr) {
            std::cout << " =\n";
            initExpr->log(indent + 4);
        } else {
            std::cout << "\n";
        }
    }
};

// ============================================================================
// BinaryExpressionNode — İkili İşlem (a OP b)
// ============================================================================
//
// İki operandlı tüm işlemler: a + b, a * b, a == b, a && b, ...
// Unary prefix operatörler de burada temsil edilir (Left = nullptr).
//
//   Operator: İşlem tipi (PLUS, MINUS, STAR, EQUAL_EQUAL, ...)
//   Left:     Sol operand (unary prefix'te nullptr)
//   Right:    Sağ operand (her zaman dolu)
//
// NEDEN AYRI BİR UnaryExpressionNode YOK?
//   Pratt parser'da unary ve binary operatörler aynı akışta işlenir.
//   Left'in null olması unary olduğunu belirtir. Bu, kod tekrarını önler.
//   İleride AST işlemcisi Left'e bakarak unary/binary ayrımı yapabilir.
//
class BinaryExpressionNode : public ASTNode {
public:
    TokenType Operator;           // İşlem tipi
    ASTNode*  Left  = nullptr;   // Sol operand
    ASTNode*  Right = nullptr;   // Sağ operand

    BinaryExpressionNode() { kind = ASTKind::BinaryExpression; }

    void log(int indent = 0) override {
        // Operatörün enum ismini ve sembolünü göster
        auto it = OPERATOR_MAP_STRREV.find(Operator);
        std::string sym = (it != OPERATOR_MAP_STRREV.end()) ? std::string(it->second) : "?";
        std::string val;
        auto it2 = OPERATOR_MAP_REV.find(Operator);
        if (it2 != OPERATOR_MAP_REV.end()) val = std::string(it2->second);

        std::cout << padRight("", indent) << "BinaryExpr " << sym
                  << " (" << val << ")\n";
        // Önce sağ, sonra sol yazdır — ağaç görselleştirmesi için
        if (Right) Right->log(indent + 2);
        if (Left)  Left->log(indent + 2);
    }
};

// ============================================================================
// LiteralNode — Sabit Değer
// ============================================================================
//
// Kaynak kodda doğrudan yazılan değerler: 42, "hello", true, false, null.
// lexerToken:  Orijinal Token (NumberToken ise isFloat/base bilgisi)
// parserToken: Parser'ın atadığı tip bilgisi
//
class LiteralNode : public ASTNode {
public:
    Token*       lexerToken  = nullptr;  // Tokenizer'dan gelen orijinal token
    ParserToken  parserToken;            // Parser tarafından zenginleştirilmiş token

    LiteralNode() { kind = ASTKind::Literal; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent)
                  << "Literal {" << parserToken.token->token << "}\n";
    }
};

// ============================================================================
// IdentifierNode — Tanımlayıcı Referansı
// ============================================================================
//
// Değişken, fonksiyon, veya tip ismi. Örn: x, myVar, calculate.
// İleride symbol table ile çözümlenecek (bu değişken nerede tanımlı?).
//
class IdentifierNode : public ASTNode {
public:
    Token*       lexerToken  = nullptr;
    ParserToken  parserToken;

    IdentifierNode() { kind = ASTKind::Identifier; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent)
                  << "Identifier {" << parserToken.token->token << "}\n";
    }
};

// ============================================================================
// PostfixNode — Son Ek İşlem (a++, a--)
// ============================================================================
//
// Operand'dan SONRA gelen operatör. Şu anda sadece ++ ve --.
// operand: İşlem yapılan ifade (genellikle IdentifierNode)
// Operator: PLUS_PLUS veya MINUS_MINUS
//
class PostfixNode : public ASTNode {
public:
    ASTNode*  operand  = nullptr;  // İşlem yapılan ifade
    TokenType Operator;            // PLUS_PLUS veya MINUS_MINUS

    PostfixNode() { kind = ASTKind::Postfix; }

    void log(int indent = 0) override {
        auto it = OPERATOR_MAP_STRREV.find(Operator);
        std::string sym = (it != OPERATOR_MAP_STRREV.end()) ? std::string(it->second) : "?";

        std::cout << padRight("", indent) << "Postfix " << sym;
        auto it2 = OPERATOR_MAP_REV.find(Operator);
        if (it2 != OPERATOR_MAP_REV.end())
            std::cout << " (" << it2->second << ")";
        std::cout << "\n";
        if (operand) operand->log(indent + 2);
    }
};

// ============================================================================
// IfStatementNode — if / else
// ============================================================================
//
// condition:  Koşul ifadesi (parantez içindeki)
// thenBranch: if gövdesi (BlockNode veya tek statement)
// elseBranch: else gövdesi (opsiyonel, nullptr = else yok)
//
class IfStatementNode : public ASTNode {
public:
    ASTNode* condition  = nullptr;  // Koşul
    ASTNode* thenBranch = nullptr;  // if gövdesi
    ASTNode* elseBranch = nullptr;  // else gövdesi (opsiyonel)

    IfStatementNode() { kind = ASTKind::IfStatement; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "IfStatement\n";
        std::cout << padRight("", indent + 2) << "Condition:\n";
        if (condition) condition->log(indent + 4);
        std::cout << padRight("", indent + 2) << "Then:\n";
        if (thenBranch) thenBranch->log(indent + 4);
        if (elseBranch) {
            std::cout << padRight("", indent + 2) << "Else:\n";
            elseBranch->log(indent + 4);
        }
    }
};

// ============================================================================
// WhileStatementNode — while Döngüsü
// ============================================================================
//
// while (condition) body
//
class WhileStatementNode : public ASTNode {
public:
    ASTNode* condition = nullptr;  // Döngü koşulu
    ASTNode* body      = nullptr;  // Döngü gövdesi

    WhileStatementNode() { kind = ASTKind::WhileStatement; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "WhileStatement\n";
        std::cout << padRight("", indent + 2) << "Condition:\n";
        if (condition) condition->log(indent + 4);
        std::cout << padRight("", indent + 2) << "Body:\n";
        if (body) body->log(indent + 4);
    }
};

// ============================================================================
// ForStatementNode — for Döngüsü
// ============================================================================
//
// for (init; condition; update) body
//
// init:      Başlangıç (VariableDeclNode veya ExpressionStatementNode)
// condition: Devam koşulu (nullptr = sonsuz döngü)
// update:    Her adımda çalışan ifade
// body:      Döngü gövdesi
//
class ForStatementNode : public ASTNode {
public:
    ASTNode* init      = nullptr;  // Başlangıç
    ASTNode* condition = nullptr;  // Koşul
    ASTNode* update    = nullptr;  // Güncelleme
    ASTNode* body      = nullptr;  // Gövde

    ForStatementNode() { kind = ASTKind::ForStatement; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "ForStatement\n";
        if (init) {
            std::cout << padRight("", indent + 2) << "Init:\n";
            init->log(indent + 4);
        }
        if (condition) {
            std::cout << padRight("", indent + 2) << "Condition:\n";
            condition->log(indent + 4);
        }
        if (update) {
            std::cout << padRight("", indent + 2) << "Update:\n";
            update->log(indent + 4);
        }
        std::cout << padRight("", indent + 2) << "Body:\n";
        if (body) body->log(indent + 4);
    }
};

// ============================================================================
// DoWhileStatementNode — do-while Döngüsü
// ============================================================================
//
// do body while (condition);
//
class DoWhileStatementNode : public ASTNode {
public:
    ASTNode* condition = nullptr;
    ASTNode* body      = nullptr;

    DoWhileStatementNode() { kind = ASTKind::DoWhileStatement; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "DoWhileStatement\n";
        std::cout << padRight("", indent + 2) << "Body:\n";
        if (body) body->log(indent + 4);
        std::cout << padRight("", indent + 2) << "Condition:\n";
        if (condition) condition->log(indent + 4);
    }
};

// ============================================================================
// ReturnStatementNode — return [ifade]
// ============================================================================
//
// value = nullptr ise "return;" (void fonksiyonda)
// value dolu ise "return expr;"
//
class ReturnStatementNode : public ASTNode {
public:
    ASTNode* value = nullptr;  // Dönüş değeri (opsiyonel)

    ReturnStatementNode() { kind = ASTKind::ReturnStatement; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "ReturnStatement";
        if (value) {
            std::cout << "\n";
            value->log(indent + 2);
        } else {
            std::cout << " (void)\n";
        }
    }
};

// ============================================================================
// BreakStatementNode — break
// ============================================================================
//
// En yakın döngüden veya switch'ten çıkar.
//
class BreakStatementNode : public ASTNode {
public:
    BreakStatementNode() { kind = ASTKind::BreakStatement; }
    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "BreakStatement\n";
    }
};

// ============================================================================
// ContinueStatementNode — continue
// ============================================================================
//
// En yakın döngünün başına atlar.
//
class ContinueStatementNode : public ASTNode {
public:
    ContinueStatementNode() { kind = ASTKind::ContinueStatement; }
    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "ContinueStatement\n";
    }
};

// ============================================================================
// ExpressionStatementNode — İfadeyi Statement Olarak Sarma
// ============================================================================
//
// Bir ifadeyi (expression) statement bağlamında kullanmak için sarar.
// Örn: x = 5;  → ExpressionStatementNode( BinaryExpressionNode(x, =, 5) )
//
class ExpressionStatementNode : public ASTNode {
public:
    ASTNode* expression = nullptr;  // İç ifade

    ExpressionStatementNode() { kind = ASTKind::ExpressionStatement; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "ExpressionStatement\n";
        if (expression) expression->log(indent + 2);
    }
};

#endif // SAQUT_AST
