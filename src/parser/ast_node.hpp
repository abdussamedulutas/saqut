// ============================================================================
// saQut Compiler — AST Düğüm Tabanı (ASTNode, ASTKind, LiteralType)
// ============================================================================
//
// DİZİN:   src/parser/ast_node.hpp
// KATMAN:  Katman 3 — Parser
// BAĞIMLI: core/location.hpp, parser/token.hpp, tools.hpp
//
// Bu dosya: ASTNode taban sınıfını, ASTKind enum'unu, LiteralType enum'unu
// ve çocuk düğümleri JSON olarak yazdırmak için childrenToJson yardımcısını
// içerir. Diğer tüm düğüm sınıfları bu dosyayı include eder.
//
// ============================================================================

#ifndef SAQUT_AST_NODE
#define SAQUT_AST_NODE

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "core/location.hpp"
#include "parser/token.hpp"
#include "tools.hpp"

// ============================================================================
// ASTKind — Düğüm Tipi Enum
// ============================================================================

enum class ASTKind {
    Program,              // Kök düğüm
    FunctionDecl,         // Fonksiyon tanımı
    Block,                // { } bloğu
    VariableDecl,         // Değişken tanımı
    BinaryExpression,     // İkili işlem (a + b)
    UnaryExpression,      // Tekli işlem (-a, !a)
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
    Call,                 // Fonksiyon çağrısı f(args)
    MemberAccess,         // Üye erişimi a.b, a->b
    IndexExpression,      // Dizi erişimi a[i]
    StructDecl,           // struct tanımı
};

// ============================================================================
// LiteralType — Sabit Değer Alt Tipleri
// ============================================================================

enum class LiteralType : uint8_t {
    INTEGER,   // Tamsayı (decimal, hex, octal, binary)
    FLOAT,     // Ondalıklı sayı (3.14, 1e-5)
    STRING,    // Metin ("hello")
    BOOLEAN,   // true / false
    BOŞ        // null
};

inline const char* literalTypeToString(LiteralType t) {
    switch (t) {
        case LiteralType::INTEGER: return "integer";
        case LiteralType::FLOAT:   return "float";
        case LiteralType::STRING:  return "string";
        case LiteralType::BOOLEAN: return "boolean";
        case LiteralType::BOŞ:     return "null";
    }
    return "?";
}

// ============================================================================
// ASTNode — Soyut Taban Sınıf
// ============================================================================
//
// Tüm AST düğümleri bu sınıftan türetilir. Her düğüm:
//   - kind:  Tipini bilir (ASTKind enum)
//   - parent: Ebeveynine işaret eder
//   - loc:    Kaynak koddaki konumunu bilir
//   - log():  Konsola yazdırılabilir
//   - toJson: JSON olarak serileştirilebilir
//
// ============================================================================

class ASTNode {
public:
    ASTKind kind;
    ASTNode* parent = nullptr;
    SourceLocation loc;

    virtual void log(int indent = 0) {
        (void)indent;
        std::cout << "<Unknown>\n";
    }

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
// childrenToJson — Düğümün çocuklarını JSON array olarak yaz
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

#endif // SAQUT_AST_NODE
