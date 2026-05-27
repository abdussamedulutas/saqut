// ============================================================================
// saQut Compiler — AST İfade Düğümleri
// ============================================================================
//
// DİZİN:   src/parser/ast_expr.hpp
// İÇERİK:  BinaryExpr, Literal, Identifier, Postfix,
//          CallExpression, MemberAccess, IndexExpression
//
// ============================================================================

#ifndef SAQUT_AST_EXPR
#define SAQUT_AST_EXPR

#include <iostream>
#include <sstream>
#include <string>
#include "parser/ast_node.hpp"
#include "parser/ast_json.hpp"
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
           << in << "  \"operator\": \"" << jsonEscape(opSym) << "\",\n"
           << in << "  \"location\": " << loc.toJson() << "";
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

// LiteralType enum'u ve literalTypeToString ast_node.hpp'de tanımlıdır.

class LiteralNode : public ASTNode {
public:
    Token*       lexerToken  = nullptr;
    ParserToken  parserToken;

    LiteralType literalType  = LiteralType::INTEGER;
    int         literalBase  = 10;     // 10, 16, 8, 2 (sadece INTEGER/FLOAT için)
    bool        isFloatValue = false;  // Ondalıklı mı? (sadece INTEGER/FLOAT için)

    LiteralNode() { kind = ASTKind::Literal; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent)
                  << "Literal {" << parserToken.token->token << "} "
                  << literalTypeToString(literalType);
        if (literalType == LiteralType::INTEGER && literalBase != 10)
            std::cout << " (base " << literalBase << ")";
        std::cout << "\n";
    }

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        std::string val = parserToken.token ? parserToken.token->token : "?";
        std::ostringstream ss;
        ss << in << "{\n"
           << in << "  \"kind\": \"Literal\",\n"
           << in << "  \"literalType\": \"" << literalTypeToString(literalType) << "\",\n"
           << in << "  \"value\": \"" << jsonEscape(val) << "\"";
        if (literalType == LiteralType::INTEGER && literalBase != 10) {
            ss << ",\n" << in << "  \"base\": " << literalBase;
        }
        if (literalType == LiteralType::FLOAT) {
            ss << ",\n" << in << "  \"isFloat\": true";
        }
        ss << ",\n" << in << "  \"location\": " << loc.toJson() << "\n"
           << in << "}";
        return ss.str();
    }
};

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
           << in << "  \"name\": \"" << jsonEscape(name) << "\",\n"
           << in << "  \"location\": " << loc.toJson() << "\n"
           << in << "}";
        return ss.str();
    }
};

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

class CallExpressionNode : public ASTNode {
public:
    ASTNode* callee = nullptr;
    std::vector<ASTNode*> arguments;

    CallExpressionNode() { kind = ASTKind::Call; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "Call\n";
        if (callee) {
            std::cout << padRight("", indent + 2) << "Callee:\n";
            callee->log(indent + 4);
        }
        std::cout << padRight("", indent + 2) << "Args (" << arguments.size() << "):\n";
        for (auto* a : arguments) a->log(indent + 4);
    }

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        std::ostringstream ss;
        ss << in << "{\n"
           << in << "  \"kind\": \"Call\"";
        if (callee) {
            ss << ",\n" << in << "  \"callee\":\n"
               << callee->toJson(depth + 2);
        }
        ss << ",\n" << in << "  \"arguments\": [\n";
        for (size_t i = 0; i < arguments.size(); i++) {
            ss << arguments[i]->toJson(depth + 3);
            if (i + 1 < arguments.size()) ss << ",";
            ss << "\n";
        }
        ss << in << "  ]\n" << in << "}";
        return ss.str();
    }
};

// ============================================================================
// MemberAccessNode — Üye Erişimi a.b veya a->b
// ============================================================================


class MemberAccessNode : public ASTNode {
public:
    ASTNode*   object = nullptr;
    std::string member;
    bool       arrow = false;

    MemberAccessNode() { kind = ASTKind::MemberAccess; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "MemberAccess "
                  << (arrow ? "->" : ".") << " " << member << "\n";
        if (object) object->log(indent + 2);
    }

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        std::ostringstream ss;
        ss << in << "{\n"
           << in << "  \"kind\": \"MemberAccess\",\n"
           << in << "  \"member\": \"" << jsonEscape(member) << "\",\n"
           << in << "  \"arrow\": " << (arrow ? "true" : "false");
        if (object) {
            ss << ",\n" << in << "  \"object\":\n"
               << object->toJson(depth + 2);
        }
        ss << "\n" << in << "}";
        return ss.str();
    }
};

// ============================================================================
// IndexExpressionNode — Dizi Erişimi a[i]
// ============================================================================


class IndexExpressionNode : public ASTNode {
public:
    ASTNode* object = nullptr;
    ASTNode* index  = nullptr;

    IndexExpressionNode() { kind = ASTKind::IndexExpression; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "IndexExpression\n";
        if (object) {
            std::cout << padRight("", indent + 2) << "Object:\n";
            object->log(indent + 4);
        }
        if (index) {
            std::cout << padRight("", indent + 2) << "Index:\n";
            index->log(indent + 4);
        }
    }

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        std::ostringstream ss;
        ss << in << "{\n"
           << in << "  \"kind\": \"IndexExpression\"";
        if (object) {
            ss << ",\n" << in << "  \"object\":\n"
               << object->toJson(depth + 2);
        }
        if (index) {
            ss << ",\n" << in << "  \"index\":\n"
               << index->toJson(depth + 2);
        }
        ss << "\n" << in << "}";
        return ss.str();
    }
};

// ============================================================================
// StructDeclNode — struct Tanımı
// ============================================================================


#endif // SAQUT_AST_EXPR
