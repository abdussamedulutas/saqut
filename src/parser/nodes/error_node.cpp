#include "parser/nodes/error_node.hpp"
#include "parser/ast_json.hpp"

ErrorNode::ErrorNode() { kind = ASTKind::Error; }

void ErrorNode::log(int indent) {
    std::cout << jsonIndent(indent) << Color::SoftGri << "Error" << Color::Reset
               << " {" << code << ": " << message << "}\n";
}

std::string ErrorNode::toJson(int depth) {
    JsonObject obj(depth);
    obj.add("kind", "Error");
    obj.add("code", code);
    obj.add("message", message);
    obj.addRaw("location", loc.toJson());
    return obj.str();
}
