#include "parser/nodes/binary_expr.hpp"
#include "parser/ast_json.hpp"

BinaryExpressionNode::BinaryExpressionNode() {
    kind = ASTKind::BinaryExpression;
}

void BinaryExpressionNode::log(int indent) {
    std::string in = jsonIndent(indent);
    std::cout << in << "BinaryExpression (" << (OPERATOR_MAP_REV.count(Operator) ? OPERATOR_MAP_REV.at(Operator) : "?") << ")\n";
    if (Left)  Left->log(indent + 1);
    if (Right) Right->log(indent + 1);
}

std::string BinaryExpressionNode::toJson(int depth) {
    JsonObject obj(depth);
    obj.add("kind", "BinaryExpression");
    obj.add("operator", std::string(OPERATOR_MAP_REV.count(Operator) ? OPERATOR_MAP_REV.at(Operator) : "?"));
    if (Left)  obj.addRaw("left", Left->toJson(depth + 1));
    if (Right) obj.addRaw("right", Right->toJson(depth + 1));
    obj.addRaw("location", loc.toJson());
    return obj.str();
}
