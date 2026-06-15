#include "parser/nodes/expressions.hpp"
#include "parser/ast_json.hpp"

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
