#include "parser/nodes/statements.hpp"
#include "parser/ast_json.hpp"

// BlockNode
BlockNode::BlockNode() { kind = ASTKind::Block; }
void BlockNode::log(int indent) {
    std::cout << jsonIndent(indent) << "Block\n";
    for (auto* child : children) child->log(indent + 1);
}
std::string BlockNode::toJson(int depth) {
    JsonObject obj(depth);
    obj.add("kind", "Block");
    obj.addArray("children", [&]() {
        for (auto* child : children) obj.addItem(child->toJson(depth + 2));
    });
    obj.add("isReachable", isReachable);
    obj.addRaw("location", loc.toJson());
    return obj.str();
}

// IfStatementNode
IfStatementNode::IfStatementNode() { kind = ASTKind::IfStatement; }
void IfStatementNode::log(int indent) {
    std::cout << jsonIndent(indent) << "IfStatement\n";
    if (condition) condition->log(indent + 1);
    if (thenBranch) thenBranch->log(indent + 1);
    if (elseBranch) elseBranch->log(indent + 1);
}
std::string IfStatementNode::toJson(int depth) {
    JsonObject obj(depth);
    obj.add("kind", "IfStatement");
    if (condition) obj.addRaw("condition", condition->toJson(depth + 1));
    if (thenBranch) obj.addRaw("then", thenBranch->toJson(depth + 1));
    if (elseBranch) obj.addRaw("else", elseBranch->toJson(depth + 1));
    obj.add("isReachable", isReachable);
    obj.addRaw("location", loc.toJson());
    return obj.str();
}

// WhileStatementNode
WhileStatementNode::WhileStatementNode() { kind = ASTKind::WhileStatement; }
void WhileStatementNode::log(int indent) {
    std::cout << jsonIndent(indent) << "WhileStatement\n";
    if (condition) condition->log(indent + 1);
    if (body) body->log(indent + 1);
}
std::string WhileStatementNode::toJson(int depth) {
    JsonObject obj(depth);
    obj.add("kind", "WhileStatement");
    if (condition) obj.addRaw("condition", condition->toJson(depth + 1));
    if (body) obj.addRaw("body", body->toJson(depth + 1));
    obj.add("isReachable", isReachable);
    obj.addRaw("location", loc.toJson());
    return obj.str();
}

// ForStatementNode
ForStatementNode::ForStatementNode() { kind = ASTKind::ForStatement; }
void ForStatementNode::log(int indent) {
    std::cout << jsonIndent(indent) << "ForStatement\n";
    if (init) init->log(indent + 1);
    if (condition) condition->log(indent + 1);
    if (update) update->log(indent + 1);
    if (body) body->log(indent + 1);
}
std::string ForStatementNode::toJson(int depth) {
    JsonObject obj(depth);
    obj.add("kind", "ForStatement");
    if (init) obj.addRaw("init", init->toJson(depth + 1));
    if (condition) obj.addRaw("condition", condition->toJson(depth + 1));
    if (update) obj.addRaw("update", update->toJson(depth + 1));
    if (body) obj.addRaw("body", body->toJson(depth + 1));
    obj.add("isReachable", isReachable);
    obj.addRaw("location", loc.toJson());
    return obj.str();
}

// DoWhileStatementNode
DoWhileStatementNode::DoWhileStatementNode() { kind = ASTKind::DoWhileStatement; }
void DoWhileStatementNode::log(int indent) {
    std::cout << jsonIndent(indent) << "DoWhileStatement\n";
    if (body) body->log(indent + 1);
    if (condition) condition->log(indent + 1);
}
std::string DoWhileStatementNode::toJson(int depth) {
    JsonObject obj(depth);
    obj.add("kind", "DoWhileStatement");
    if (condition) obj.addRaw("condition", condition->toJson(depth + 1));
    if (body) obj.addRaw("body", body->toJson(depth + 1));
    obj.add("isReachable", isReachable);
    obj.addRaw("location", loc.toJson());
    return obj.str();
}

// ReturnStatementNode
ReturnStatementNode::ReturnStatementNode() { kind = ASTKind::ReturnStatement; }
void ReturnStatementNode::log(int indent) {
    std::cout << jsonIndent(indent) << "ReturnStatement\n";
    if (value) value->log(indent + 1);
}
std::string ReturnStatementNode::toJson(int depth) {
    JsonObject obj(depth);
    obj.add("kind", "ReturnStatement");
    if (value) obj.addRaw("value", value->toJson(depth + 1));
    obj.add("isReachable", isReachable);
    obj.addRaw("location", loc.toJson());
    return obj.str();
}

// BreakStatementNode
BreakStatementNode::BreakStatementNode() { kind = ASTKind::BreakStatement; }
void BreakStatementNode::log(int indent) {
    std::cout << jsonIndent(indent) << "BreakStatement\n";
}
std::string BreakStatementNode::toJson(int depth) {
    JsonObject obj(depth);
    obj.add("kind", "BreakStatement");
    obj.add("isReachable", isReachable);
    obj.addRaw("location", loc.toJson());
    return obj.str();
}

// ContinueStatementNode
ContinueStatementNode::ContinueStatementNode() { kind = ASTKind::ContinueStatement; }
void ContinueStatementNode::log(int indent) {
    std::cout << jsonIndent(indent) << "ContinueStatement\n";
}
std::string ContinueStatementNode::toJson(int depth) {
    JsonObject obj(depth);
    obj.add("kind", "ContinueStatement");
    obj.add("isReachable", isReachable);
    obj.addRaw("location", loc.toJson());
    return obj.str();
}

// ExpressionStatementNode
ExpressionStatementNode::ExpressionStatementNode() { kind = ASTKind::ExpressionStatement; }
void ExpressionStatementNode::log(int indent) {
    std::cout << jsonIndent(indent) << "ExpressionStatement\n";
    if (expression) expression->log(indent + 1);
}
std::string ExpressionStatementNode::toJson(int depth) {
    JsonObject obj(depth);
    obj.add("kind", "ExpressionStatement");
    if (expression) obj.addRaw("expression", expression->toJson(depth + 1));
    obj.add("isReachable", isReachable);
    obj.addRaw("location", loc.toJson());
    return obj.str();
}
