#include "parser/nodes/expressions.hpp"
#include "parser/ast_json.hpp"

// ScopeCallNode — built-in metod çağrısı: E::method(args)
ScopeCallNode::ScopeCallNode() { kind = ASTKind::ScopeCall; }
void ScopeCallNode::log(int indent) {
    std::cout << jsonIndent(indent) << "ScopeCall " << leftTypeName << "::" << methodName << "\n";
    for (auto* arg : arguments) arg->log(indent + 1);
}
std::string ScopeCallNode::toJson(int depth) {
    JsonObject obj(depth);
    obj.add("kind",       "ScopeCall");
    obj.add("leftType",   leftTypeName);
    obj.add("method",     methodName);
    obj.add("builtinId",  builtinId);
    obj.addArray("arguments", [&]() {
        for (auto* arg : arguments) obj.addItem(arg->toJson(depth + 2));
    });
    obj.addRaw("resolvedType", resolvedTypeJson());
    obj.addRaw("location", loc.toJson());
    return obj.str();
}

// PostfixNode
PostfixNode::PostfixNode() { kind = ASTKind::Postfix; }
void PostfixNode::log(int indent) {
    std::cout << jsonIndent(indent) << "Postfix (" << (OPERATOR_MAP_REV.count(Operator) ? OPERATOR_MAP_REV.at(Operator) : "?") << ")\n";
    if (operand) operand->log(indent + 1);
}
std::string PostfixNode::toJson(int depth) {
    JsonObject obj(depth);
    obj.add("kind", "Postfix");
    obj.add("operator", std::string(OPERATOR_MAP_REV.count(Operator) ? OPERATOR_MAP_REV.at(Operator) : "?"));
    if (operand) obj.addRaw("operand", operand->toJson(depth + 1));
    obj.addRaw("resolvedType", resolvedTypeJson());
    obj.addRaw("location", loc.toJson());
    return obj.str();
}

// CallExpressionNode
CallExpressionNode::CallExpressionNode() { kind = ASTKind::Call; }
void CallExpressionNode::log(int indent) {
    std::cout << jsonIndent(indent) << "Call\n";
    if (callee) callee->log(indent + 1);
    for (auto* arg : arguments) arg->log(indent + 1);
}
std::string CallExpressionNode::toJson(int depth) {
    JsonObject obj(depth);
    obj.add("kind", "Call");
    if (callee) obj.addRaw("callee", callee->toJson(depth + 1));
    obj.addArray("arguments", [&]() {
        for (auto* arg : arguments) obj.addItem(arg->toJson(depth + 2));
    });
    obj.addRaw("resolvedType", resolvedTypeJson());
    obj.addRaw("location", loc.toJson());
    return obj.str();
}

// MemberAccessNode
MemberAccessNode::MemberAccessNode() { kind = ASTKind::MemberAccess; }
void MemberAccessNode::log(int indent) {
    std::cout << jsonIndent(indent) << "MemberAccess (" << (arrow ? "->" : ".") << member << ")\n";
    if (object) object->log(indent + 1);
}
std::string MemberAccessNode::toJson(int depth) {
    JsonObject obj(depth);
    obj.add("kind", "MemberAccess");
    obj.add("member", member);
    obj.add("arrow", arrow);
    if (object) obj.addRaw("object", object->toJson(depth + 1));
    obj.addRaw("resolvedType", resolvedTypeJson());
    obj.addRaw("location", loc.toJson());
    return obj.str();
}

// ArrayLiteralNode
ArrayLiteralNode::ArrayLiteralNode() { kind = ASTKind::ArrayLiteral; }
void ArrayLiteralNode::log(int indent) {
    std::cout << jsonIndent(indent) << "ArrayLiteral [" << elements.size() << " eleman]\n";
    for (auto* e : elements) e->log(indent + 1);
}
std::string ArrayLiteralNode::toJson(int depth) {
    JsonObject obj(depth);
    obj.add("kind", "ArrayLiteral");
    obj.addArray("elements", [&]() {
        for (auto* e : elements) obj.addItem(e->toJson(depth + 2));
    });
    obj.addRaw("resolvedType", resolvedTypeJson());
    obj.addRaw("location", loc.toJson());
    return obj.str();
}

// CastExpressionNode (ADR-026)
CastExpressionNode::CastExpressionNode() { kind = ASTKind::CastExpression; }
void CastExpressionNode::log(int indent) {
    std::cout << jsonIndent(indent) << "CastExpression as " << targetTypeName
              << (targetNullable ? "?" : "") << "\n";
    if (operand) operand->log(indent + 1);
}
std::string CastExpressionNode::toJson(int depth) {
    JsonObject obj(depth);
    obj.add("kind", "CastExpression");
    obj.add("targetType", targetTypeName + (targetNullable ? "?" : ""));
    if (operand) obj.addRaw("operand", operand->toJson(depth + 1));
    obj.addRaw("resolvedType", resolvedTypeJson());
    obj.addRaw("location", loc.toJson());
    return obj.str();
}

// IndexExpressionNode
IndexExpressionNode::IndexExpressionNode() { kind = ASTKind::IndexExpression; }
void IndexExpressionNode::log(int indent) {
    std::cout << jsonIndent(indent) << "IndexExpression\n";
    if (object) object->log(indent + 1);
    if (index) index->log(indent + 1);
}
std::string IndexExpressionNode::toJson(int depth) {
    JsonObject obj(depth);
    obj.add("kind", "IndexExpression");
    if (object) obj.addRaw("object", object->toJson(depth + 1));
    if (index) obj.addRaw("index", index->toJson(depth + 1));
    obj.addRaw("resolvedType", resolvedTypeJson());
    obj.addRaw("location", loc.toJson());
    return obj.str();
}
