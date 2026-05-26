// ============================================================================
// saQut Compiler — AST Analizi ve Sembol Toplayıcı
// ============================================================================
//
// DİZİN:   src/json.hpp
// KATMAN:  Tüm katmanlardan sonra — AST'yi tüketir
// BAĞIMLI: AST (src/parser/ast.hpp)
// KULLANAN: main.cpp
//
// AMAÇ:
//   1. AST üzerinde analiz yap (düğüm sayısı, derinlik, tip dağılımı)
//   2. Sembolleri topla (fonksiyon isimleri, dönüş tipleri, değişkenler)
//
// NOT: JSON serileştirme ASTNode::toJson() tarafından yapılır.
//      Bu dosya sadece analiz ve sembol toplama işlemlerini içerir.
//      Yeni bir AST düğümü eklendiğinde buraya dokunmak gerekmez.
//
// ============================================================================

#ifndef SAQUT_JSON
#define SAQUT_JSON

#include <string>
#include <sstream>
#include <map>
#include <vector>
#include "parser/ast.hpp"

// ============================================================================
// astKindName — ASTKind enum'unu string'e çevir (analiz çıktısı için)
// ============================================================================

inline const char* astKindName(ASTKind k) {
    switch (k) {
        case ASTKind::Program:             return "Program";
        case ASTKind::FunctionDecl:        return "FunctionDecl";
        case ASTKind::Block:               return "Block";
        case ASTKind::VariableDecl:        return "VariableDecl";
        case ASTKind::BinaryExpression:    return "BinaryExpression";
        case ASTKind::UnaryExpression:     return "UnaryExpression";
        case ASTKind::Literal:             return "Literal";
        case ASTKind::Identifier:          return "Identifier";
        case ASTKind::Postfix:             return "Postfix";
        case ASTKind::IfStatement:         return "IfStatement";
        case ASTKind::ForStatement:        return "ForStatement";
        case ASTKind::WhileStatement:      return "WhileStatement";
        case ASTKind::DoWhileStatement:    return "DoWhileStatement";
        case ASTKind::ReturnStatement:     return "ReturnStatement";
        case ASTKind::BreakStatement:      return "BreakStatement";
        case ASTKind::ContinueStatement:   return "ContinueStatement";
        case ASTKind::ExpressionStatement: return "ExpressionStatement";
        case ASTKind::Call:                return "Call";
        case ASTKind::MemberAccess:        return "MemberAccess";
        case ASTKind::IndexExpression:     return "IndexExpression";
        case ASTKind::StructDecl:          return "StructDecl";
        default: return "Unknown";
    }
}

// ============================================================================
// AST → JSON (ince sarmalayıcı — asıl iş ASTNode::toJson()'da)
// ============================================================================

inline std::string astToJson(ASTNode* node, int depth = 0) {
    if (!node) return "null";
    return node->toJson(depth);
}

// ============================================================================
// AST Analizi
// ============================================================================

struct AstAnalysis {
    int totalNodes = 0;
    int maxDepth = 0;
    int functionCount = 0;
    int variableCount = 0;
    int ifCount = 0;
    int loopCount = 0;  // for + while + do-while
    std::map<std::string, int> nodeTypeCounts;
};

inline void analyzeRecursive(ASTNode* node, int currentDepth, AstAnalysis& a) {
    if (!node) return;

    a.totalNodes++;
    if (currentDepth > a.maxDepth) a.maxDepth = currentDepth;

    const char* name = astKindName(node->kind);
    a.nodeTypeCounts[name]++;

    switch (node->kind) {
        case ASTKind::FunctionDecl: a.functionCount++; break;
        case ASTKind::VariableDecl: a.variableCount++; break;
        case ASTKind::IfStatement:  a.ifCount++; break;
        case ASTKind::WhileStatement:
        case ASTKind::ForStatement:
        case ASTKind::DoWhileStatement:
            a.loopCount++; break;
        default: break;
    }

    // Tüm çocukları gez
    switch (node->kind) {
        case ASTKind::Program:
        case ASTKind::FunctionDecl:
        case ASTKind::Block: {
            for (auto* child : node->getChildren())
                analyzeRecursive(child, currentDepth + 1, a);
            break;
        }
        case ASTKind::VariableDecl: {
            auto* vd = (VariableDeclNode*)node;
            if (vd->initExpr) analyzeRecursive(vd->initExpr, currentDepth + 1, a);
            break;
        }
        case ASTKind::BinaryExpression: {
            auto* bin = (BinaryExpressionNode*)node;
            if (bin->Left)  analyzeRecursive(bin->Left,  currentDepth + 1, a);
            if (bin->Right) analyzeRecursive(bin->Right, currentDepth + 1, a);
            break;
        }
        case ASTKind::Postfix: {
            auto* pf = (PostfixNode*)node;
            if (pf->operand) analyzeRecursive(pf->operand, currentDepth + 1, a);
            break;
        }
        case ASTKind::IfStatement: {
            auto* ifn = (IfStatementNode*)node;
            if (ifn->condition)  analyzeRecursive(ifn->condition,  currentDepth + 1, a);
            if (ifn->thenBranch) analyzeRecursive(ifn->thenBranch, currentDepth + 1, a);
            if (ifn->elseBranch) analyzeRecursive(ifn->elseBranch, currentDepth + 1, a);
            break;
        }
        case ASTKind::WhileStatement: {
            auto* ws = (WhileStatementNode*)node;
            if (ws->condition) analyzeRecursive(ws->condition, currentDepth + 1, a);
            if (ws->body)      analyzeRecursive(ws->body,      currentDepth + 1, a);
            break;
        }
        case ASTKind::ForStatement: {
            auto* fs = (ForStatementNode*)node;
            if (fs->init)      analyzeRecursive(fs->init,      currentDepth + 1, a);
            if (fs->condition) analyzeRecursive(fs->condition, currentDepth + 1, a);
            if (fs->update)    analyzeRecursive(fs->update,    currentDepth + 1, a);
            if (fs->body)      analyzeRecursive(fs->body,      currentDepth + 1, a);
            break;
        }
        case ASTKind::DoWhileStatement: {
            auto* dw = (DoWhileStatementNode*)node;
            if (dw->body)      analyzeRecursive(dw->body,      currentDepth + 1, a);
            if (dw->condition) analyzeRecursive(dw->condition, currentDepth + 1, a);
            break;
        }
        case ASTKind::ReturnStatement: {
            auto* rs = (ReturnStatementNode*)node;
            if (rs->value) analyzeRecursive(rs->value, currentDepth + 1, a);
            break;
        }
        case ASTKind::ExpressionStatement: {
            auto* es = (ExpressionStatementNode*)node;
            if (es->expression) analyzeRecursive(es->expression, currentDepth + 1, a);
            break;
        }
        case ASTKind::Call: {
            auto* call = (CallExpressionNode*)node;
            if (call->callee) analyzeRecursive(call->callee, currentDepth + 1, a);
            for (auto* arg : call->arguments) analyzeRecursive(arg, currentDepth + 1, a);
            break;
        }
        case ASTKind::MemberAccess: {
            auto* ma = (MemberAccessNode*)node;
            if (ma->object) analyzeRecursive(ma->object, currentDepth + 1, a);
            break;
        }
        case ASTKind::IndexExpression: {
            auto* ie = (IndexExpressionNode*)node;
            if (ie->object) analyzeRecursive(ie->object, currentDepth + 1, a);
            if (ie->index)  analyzeRecursive(ie->index,  currentDepth + 1, a);
            break;
        }
        case ASTKind::StructDecl: {
            for (auto* child : node->getChildren()) analyzeRecursive(child, currentDepth + 1, a);
            break;
        }
        default: break; // Literal, Identifier, Break, Continue — yaprak düğüm
    }
}

inline AstAnalysis analyzeAst(ASTNode* root) {
    AstAnalysis a;
    analyzeRecursive(root, 0, a);
    return a;
}

inline std::string analysisToJson(const AstAnalysis& a) {
    std::ostringstream ss;
    ss << "  \"totalNodes\": " << a.totalNodes << ",\n"
       << "  \"maxDepth\": " << a.maxDepth << ",\n"
       << "  \"functionCount\": " << a.functionCount << ",\n"
       << "  \"variableCount\": " << a.variableCount << ",\n"
       << "  \"ifCount\": " << a.ifCount << ",\n"
       << "  \"loopCount\": " << a.loopCount << ",\n"
       << "  \"nodeTypes\": {\n";
    bool first = true;
    for (auto& [name, count] : a.nodeTypeCounts) {
        if (!first) ss << ",\n";
        ss << "    \"" << name << "\": " << count;
        first = false;
    }
    ss << "\n  }";
    return ss.str();
}

// ============================================================================
// Sembol Toplayıcı
// ============================================================================

struct SymbolEntry {
    std::string name;
    std::string kind;  // "function" veya "variable"
    std::string type;  // Dönüş tipi veya değişken tipi
};

inline void collectSymbolsRecursive(ASTNode* node, std::vector<SymbolEntry>& symbols) {
    if (!node) return;

    // Sadece FunctionDecl ve VariableDecl toplanır — diğerleri gezilir
    if (node->kind == ASTKind::FunctionDecl) {
        auto* fn = (FunctionDeclNode*)node;
        symbols.push_back({fn->name, "function", fn->returnType});
    } else if (node->kind == ASTKind::StructDecl) {
        auto* st = (StructDeclNode*)node;
        symbols.push_back({st->name, "struct", ""});
    } else if (node->kind == ASTKind::VariableDecl) {
        auto* vd = (VariableDeclNode*)node;
        symbols.push_back({vd->name, "variable", vd->varType});
    }

    // Çocukları gez
    switch (node->kind) {
        case ASTKind::Program:
        case ASTKind::FunctionDecl:
        case ASTKind::Block:
            for (auto* child : node->getChildren())
                collectSymbolsRecursive(child, symbols);
            break;
        case ASTKind::VariableDecl: {
            auto* vd = (VariableDeclNode*)node;
            if (vd->initExpr) collectSymbolsRecursive(vd->initExpr, symbols);
            break;
        }
        case ASTKind::BinaryExpression: {
            auto* bin = (BinaryExpressionNode*)node;
            if (bin->Left)  collectSymbolsRecursive(bin->Left,  symbols);
            if (bin->Right) collectSymbolsRecursive(bin->Right, symbols);
            break;
        }
        case ASTKind::Postfix: {
            auto* pf = (PostfixNode*)node;
            if (pf->operand) collectSymbolsRecursive(pf->operand, symbols);
            break;
        }
        case ASTKind::IfStatement: {
            auto* ifn = (IfStatementNode*)node;
            if (ifn->condition)  collectSymbolsRecursive(ifn->condition,  symbols);
            if (ifn->thenBranch) collectSymbolsRecursive(ifn->thenBranch, symbols);
            if (ifn->elseBranch) collectSymbolsRecursive(ifn->elseBranch, symbols);
            break;
        }
        case ASTKind::WhileStatement: {
            auto* ws = (WhileStatementNode*)node;
            if (ws->condition) collectSymbolsRecursive(ws->condition, symbols);
            if (ws->body)      collectSymbolsRecursive(ws->body,      symbols);
            break;
        }
        case ASTKind::ForStatement: {
            auto* fs = (ForStatementNode*)node;
            if (fs->init)      collectSymbolsRecursive(fs->init,      symbols);
            if (fs->condition) collectSymbolsRecursive(fs->condition, symbols);
            if (fs->update)    collectSymbolsRecursive(fs->update,    symbols);
            if (fs->body)      collectSymbolsRecursive(fs->body,      symbols);
            break;
        }
        case ASTKind::DoWhileStatement: {
            auto* dw = (DoWhileStatementNode*)node;
            if (dw->body)      collectSymbolsRecursive(dw->body,      symbols);
            if (dw->condition) collectSymbolsRecursive(dw->condition, symbols);
            break;
        }
        case ASTKind::ReturnStatement: {
            auto* rs = (ReturnStatementNode*)node;
            if (rs->value) collectSymbolsRecursive(rs->value, symbols);
            break;
        }
        case ASTKind::ExpressionStatement: {
            auto* es = (ExpressionStatementNode*)node;
            if (es->expression) collectSymbolsRecursive(es->expression, symbols);
            break;
        }
        case ASTKind::Call: {
            auto* call = (CallExpressionNode*)node;
            if (call->callee) collectSymbolsRecursive(call->callee, symbols);
            for (auto* arg : call->arguments) collectSymbolsRecursive(arg, symbols);
            break;
        }
        case ASTKind::MemberAccess: {
            auto* ma = (MemberAccessNode*)node;
            if (ma->object) collectSymbolsRecursive(ma->object, symbols);
            break;
        }
        case ASTKind::IndexExpression: {
            auto* ie = (IndexExpressionNode*)node;
            if (ie->object) collectSymbolsRecursive(ie->object, symbols);
            if (ie->index)  collectSymbolsRecursive(ie->index,  symbols);
            break;
        }
        case ASTKind::StructDecl: {
            for (auto* child : node->getChildren()) collectSymbolsRecursive(child, symbols);
            break;
        }
        default: break;
    }
}

inline std::vector<SymbolEntry> collectSymbols(ASTNode* root) {
    std::vector<SymbolEntry> symbols;
    collectSymbolsRecursive(root, symbols);
    return symbols;
}

#endif // SAQUT_JSON
