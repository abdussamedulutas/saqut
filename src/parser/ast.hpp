// ============================================================================
// saQut Compiler — Soyut Sözdizim Ağacı (AST)
// ============================================================================
//
// DİZİN:   src/parser/ast.hpp
// KATMAN:  Katman 3 — Parser'ın ürettiği, IR'nin tükettiği
// BAĞIMLI: Token (src/parser/token.hpp), Tools (src/tools.hpp)
// KULLANAN: Parser (src/parser/parser.hpp), IR (src/ir/ir.hpp), JSON (src/json.hpp)
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
//   1. ASTKind enum + log() + toJson(): Her düğüm kendi tipini bilir,
//      kendini konsola yazdırabilir ve JSON olarak serileştirebilir.
//      Yeni bir düğüm eklendiğinde tüm davranışlar tek yerde tanımlanır.
//
//   2. parent pointer: Her düğüm ebeveynini bilir.
//
//   3. children vektörü (protected): Liste tipi düğümler için.
//
// BİLİNEN SINIRLAMALAR (TODO):
//   TODO: Bellek yönetimi (unique_ptr veya arena allocator)
//   TODO: Visitor pattern ile log/toJson/IR üretimi ayrıştırılabilir
//
// ============================================================================

#ifndef SAQUT_AST
#define SAQUT_AST

#include <iostream>
#include <sstream>
#include <vector>
#include "parser/token.hpp"
#include "tools.hpp"

// ============================================================================
// ASTKind — AST Düğüm Tipi Enum'u
// ============================================================================

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

class ASTNode {
public:
    ASTKind kind;
    ASTNode* parent = nullptr;

    virtual void log(int indent = 0) {
        (void)indent;
        std::cout << "<Unknown>\n";
    }

    // JSON serileştirme — her alt sınıf kendi implemente eder
    virtual std::string toJson(int indent = 0) {
        (void)indent;
        return "{\"kind\":\"Unknown\"}";
    }

    void addChild(ASTNode* child) {
        children.push_back(child);
        child->parent = this;
    }

    std::vector<ASTNode*>& getChildren() { return children; }
    virtual ~ASTNode() = default;

protected:
    std::vector<ASTNode*> children;
};

// ============================================================================
// JSON yardımcısı: alt düğüm listesini JSON array olarak yaz
// ============================================================================
inline std::string childrenToJson(ASTNode* node, int depth) {
    std::ostringstream ss;
    std::string in = jsonIndent(depth);
    auto& ch = node->getChildren();
    for (size_t i = 0; i < ch.size(); i++) {
        ss << ch[i]->toJson(depth);
        if (i + 1 < ch.size()) ss << ",";
        ss << "\n";
    }
    return ss.str();
}

// ============================================================================
// ProgramNode — Kök Düğüm
// ============================================================================

class ProgramNode : public ASTNode {
public:
    ProgramNode() { kind = ASTKind::Program; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "Program\n";
        for (auto* c : getChildren()) c->log(indent + 2);
    }

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        std::ostringstream ss;
        ss << in << "{\n"
           << in << "  \"kind\": \"Program\",\n"
           << in << "  \"children\": [\n"
           << childrenToJson(this, depth + 3)
           << in << "  ]\n"
           << in << "}";
        return ss.str();
    }
};

// ============================================================================
// FunctionDeclNode — Fonksiyon Tanımı
// ============================================================================

class FunctionDeclNode : public ASTNode {
public:
    std::string name;
    std::string returnType;

    FunctionDeclNode() { kind = ASTKind::FunctionDecl; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent)
                  << "FunctionDecl " << returnType << " " << name << "()\n";
        for (auto* c : getChildren()) c->log(indent + 2);
    }

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        std::ostringstream ss;
        ss << in << "{\n"
           << in << "  \"kind\": \"FunctionDecl\",\n"
           << in << "  \"name\": \"" << jsonEscape(name) << "\",\n"
           << in << "  \"returnType\": \"" << jsonEscape(returnType) << "\",\n"
           << in << "  \"children\": [\n"
           << childrenToJson(this, depth + 3)
           << in << "  ]\n"
           << in << "}";
        return ss.str();
    }
};

// ============================================================================
// BlockNode — Blok { ... }
// ============================================================================

class BlockNode : public ASTNode {
public:
    BlockNode() { kind = ASTKind::Block; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "Block\n";
        for (auto* c : getChildren()) c->log(indent + 2);
    }

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        std::ostringstream ss;
        ss << in << "{\n"
           << in << "  \"kind\": \"Block\",\n"
           << in << "  \"children\": [\n"
           << childrenToJson(this, depth + 3)
           << in << "  ]\n"
           << in << "}";
        return ss.str();
    }
};

// ============================================================================
// VariableDeclNode — Değişken Tanımı
// ============================================================================

class VariableDeclNode : public ASTNode {
public:
    std::string varType;
    std::string name;
    ASTNode*   initExpr = nullptr;

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

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        std::ostringstream ss;
        ss << in << "{\n"
           << in << "  \"kind\": \"VariableDecl\",\n"
           << in << "  \"name\": \"" << jsonEscape(name) << "\",\n"
           << in << "  \"varType\": \"" << jsonEscape(varType) << "\"";
        if (initExpr) {
            ss << ",\n" << in << "  \"initExpr\":\n"
               << initExpr->toJson(depth + 2);
        }
        ss << "\n" << in << "}";
        return ss.str();
    }
};

// ============================================================================
// BinaryExpressionNode — İkili İşlem (a OP b)
// ============================================================================

class BinaryExpressionNode : public ASTNode {
public:
    TokenType Operator;
    ASTNode*  Left  = nullptr;
    ASTNode*  Right = nullptr;

    BinaryExpressionNode() { kind = ASTKind::BinaryExpression; }

    void log(int indent = 0) override {
        auto it = OPERATOR_MAP_STRREV.find(Operator);
        std::string sym = (it != OPERATOR_MAP_STRREV.end()) ? std::string(it->second) : "?";
        std::string val;
        auto it2 = OPERATOR_MAP_REV.find(Operator);
        if (it2 != OPERATOR_MAP_REV.end()) val = std::string(it2->second);

        std::cout << padRight("", indent) << "BinaryExpr " << sym
                  << " (" << val << ")\n";
        if (Right) Right->log(indent + 2);
        if (Left)  Left->log(indent + 2);
    }

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        std::string opSym = "?";
        auto it = OPERATOR_MAP_REV.find(Operator);
        if (it != OPERATOR_MAP_REV.end()) opSym = std::string(it->second);

        std::ostringstream ss;
        ss << in << "{\n"
           << in << "  \"kind\": \"BinaryExpression\",\n"
           << in << "  \"operator\": \"" << jsonEscape(opSym) << "\"";
        if (Left) {
            ss << ",\n" << in << "  \"left\":\n"
               << Left->toJson(depth + 2);
        }
        if (Right) {
            ss << ",\n" << in << "  \"right\":\n"
               << Right->toJson(depth + 2);
        }
        ss << "\n" << in << "}";
        return ss.str();
    }
};

// ============================================================================
// LiteralNode — Sabit Değer
// ============================================================================

class LiteralNode : public ASTNode {
public:
    Token*       lexerToken  = nullptr;
    ParserToken  parserToken;

    LiteralNode() { kind = ASTKind::Literal; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent)
                  << "Literal {" << parserToken.token->token << "}\n";
    }

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        std::string val = parserToken.token ? parserToken.token->token : "?";
        std::ostringstream ss;
        ss << in << "{\n"
           << in << "  \"kind\": \"Literal\",\n"
           << in << "  \"value\": \"" << jsonEscape(val) << "\"\n"
           << in << "}";
        return ss.str();
    }
};

// ============================================================================
// IdentifierNode — Tanımlayıcı Referansı
// ============================================================================

class IdentifierNode : public ASTNode {
public:
    Token*       lexerToken  = nullptr;
    ParserToken  parserToken;

    IdentifierNode() { kind = ASTKind::Identifier; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent)
                  << "Identifier {" << parserToken.token->token << "}\n";
    }

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        std::string name = parserToken.token ? parserToken.token->token : "?";
        std::ostringstream ss;
        ss << in << "{\n"
           << in << "  \"kind\": \"Identifier\",\n"
           << in << "  \"name\": \"" << jsonEscape(name) << "\"\n"
           << in << "}";
        return ss.str();
    }
};

// ============================================================================
// PostfixNode — Son Ek İşlem (a++, a--)
// ============================================================================

class PostfixNode : public ASTNode {
public:
    ASTNode*  operand  = nullptr;
    TokenType Operator;

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

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        std::string opSym = "?";
        auto it = OPERATOR_MAP_REV.find(Operator);
        if (it != OPERATOR_MAP_REV.end()) opSym = std::string(it->second);

        std::ostringstream ss;
        ss << in << "{\n"
           << in << "  \"kind\": \"Postfix\",\n"
           << in << "  \"operator\": \"" << jsonEscape(opSym) << "\"";
        if (operand) {
            ss << ",\n" << in << "  \"operand\":\n"
               << operand->toJson(depth + 2);
        }
        ss << "\n" << in << "}";
        return ss.str();
    }
};

// ============================================================================
// IfStatementNode — if / else
// ============================================================================

class IfStatementNode : public ASTNode {
public:
    ASTNode* condition  = nullptr;
    ASTNode* thenBranch = nullptr;
    ASTNode* elseBranch = nullptr;

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

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        std::ostringstream ss;
        ss << in << "{\n"
           << in << "  \"kind\": \"IfStatement\"";
        if (condition) {
            ss << ",\n" << in << "  \"condition\":\n"
               << condition->toJson(depth + 2);
        }
        if (thenBranch) {
            ss << ",\n" << in << "  \"then\":\n"
               << thenBranch->toJson(depth + 2);
        }
        if (elseBranch) {
            ss << ",\n" << in << "  \"else\":\n"
               << elseBranch->toJson(depth + 2);
        }
        ss << "\n" << in << "}";
        return ss.str();
    }
};

// ============================================================================
// WhileStatementNode — while Döngüsü
// ============================================================================

class WhileStatementNode : public ASTNode {
public:
    ASTNode* condition = nullptr;
    ASTNode* body      = nullptr;

    WhileStatementNode() { kind = ASTKind::WhileStatement; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "WhileStatement\n";
        std::cout << padRight("", indent + 2) << "Condition:\n";
        if (condition) condition->log(indent + 4);
        std::cout << padRight("", indent + 2) << "Body:\n";
        if (body) body->log(indent + 4);
    }

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        std::ostringstream ss;
        ss << in << "{\n"
           << in << "  \"kind\": \"WhileStatement\"";
        if (condition) {
            ss << ",\n" << in << "  \"condition\":\n"
               << condition->toJson(depth + 2);
        }
        if (body) {
            ss << ",\n" << in << "  \"body\":\n"
               << body->toJson(depth + 2);
        }
        ss << "\n" << in << "}";
        return ss.str();
    }
};

// ============================================================================
// ForStatementNode — for Döngüsü
// ============================================================================

class ForStatementNode : public ASTNode {
public:
    ASTNode* init      = nullptr;
    ASTNode* condition = nullptr;
    ASTNode* update    = nullptr;
    ASTNode* body      = nullptr;

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

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        std::ostringstream ss;
        ss << in << "{\n"
           << in << "  \"kind\": \"ForStatement\"";
        if (init) {
            ss << ",\n" << in << "  \"init\":\n"
               << init->toJson(depth + 2);
        }
        if (condition) {
            ss << ",\n" << in << "  \"condition\":\n"
               << condition->toJson(depth + 2);
        }
        if (update) {
            ss << ",\n" << in << "  \"update\":\n"
               << update->toJson(depth + 2);
        }
        if (body) {
            ss << ",\n" << in << "  \"body\":\n"
               << body->toJson(depth + 2);
        }
        ss << "\n" << in << "}";
        return ss.str();
    }
};

// ============================================================================
// DoWhileStatementNode — do-while Döngüsü
// ============================================================================

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

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        std::ostringstream ss;
        ss << in << "{\n"
           << in << "  \"kind\": \"DoWhileStatement\"";
        if (body) {
            ss << ",\n" << in << "  \"body\":\n"
               << body->toJson(depth + 2);
        }
        if (condition) {
            ss << ",\n" << in << "  \"condition\":\n"
               << condition->toJson(depth + 2);
        }
        ss << "\n" << in << "}";
        return ss.str();
    }
};

// ============================================================================
// ReturnStatementNode — return [ifade]
// ============================================================================

class ReturnStatementNode : public ASTNode {
public:
    ASTNode* value = nullptr;

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

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        std::ostringstream ss;
        ss << in << "{\n"
           << in << "  \"kind\": \"ReturnStatement\"";
        if (value) {
            ss << ",\n" << in << "  \"value\":\n"
               << value->toJson(depth + 2);
        }
        ss << "\n" << in << "}";
        return ss.str();
    }
};

// ============================================================================
// BreakStatementNode — break
// ============================================================================

class BreakStatementNode : public ASTNode {
public:
    BreakStatementNode() { kind = ASTKind::BreakStatement; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "BreakStatement\n";
    }

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        return in + "{\n" + in + "  \"kind\": \"BreakStatement\"\n" + in + "}";
    }
};

// ============================================================================
// ContinueStatementNode — continue
// ============================================================================

class ContinueStatementNode : public ASTNode {
public:
    ContinueStatementNode() { kind = ASTKind::ContinueStatement; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "ContinueStatement\n";
    }

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        return in + "{\n" + in + "  \"kind\": \"ContinueStatement\"\n" + in + "}";
    }
};

// ============================================================================
// ExpressionStatementNode — İfadeyi Statement Olarak Sarma
// ============================================================================

class ExpressionStatementNode : public ASTNode {
public:
    ASTNode* expression = nullptr;

    ExpressionStatementNode() { kind = ASTKind::ExpressionStatement; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "ExpressionStatement\n";
        if (expression) expression->log(indent + 2);
    }

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        std::ostringstream ss;
        ss << in << "{\n"
           << in << "  \"kind\": \"ExpressionStatement\"";
        if (expression) {
            ss << ",\n" << in << "  \"expression\":\n"
               << expression->toJson(depth + 2);
        }
        ss << "\n" << in << "}";
        return ss.str();
    }
};

#endif // SAQUT_AST
