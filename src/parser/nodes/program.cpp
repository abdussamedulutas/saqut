#include "parser/nodes/program.hpp"
#include "parser/ast_json.hpp"

ProgramNode::ProgramNode() {
    kind = ASTKind::Program;
}

void ProgramNode::log(int indent) {
    std::string in = jsonIndent(indent);
    std::cout << in << Color::SoftMavi << "Program" << Color::Reset << "\n";
    for (auto* child : children) {
        child->log(indent + 1);
    }
}

std::string ProgramNode::toJson(int depth) {
    JsonObject obj(depth);
    obj.add("kind", "Program");
    obj.addArray("children", [&]() {
        for (auto* child : children) {
            obj.addItem(child->toJson(depth + 2));
        }
    });
    obj.addRaw("location", loc.toJson());
    return obj.str();
}
