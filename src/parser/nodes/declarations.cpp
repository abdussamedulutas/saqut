#include "parser/nodes/declarations.hpp"
#include "parser/ast_json.hpp"

// FunctionDeclNode
FunctionDeclNode::FunctionDeclNode() { kind = ASTKind::FunctionDecl; }
void FunctionDeclNode::log(int indent) {
    std::cout << jsonIndent(indent) << "FunctionDecl (" << name << " : " << returnType << ")\n";
    for (auto* child : children) child->log(indent + 1);
}
std::string FunctionDeclNode::toJson(int depth) {
    JsonObject obj(depth);
    obj.add("kind", "FunctionDecl");
    obj.add("name", name);
    obj.add("returnType", returnType);
    obj.addArray("children", [&]() {
        for (auto* child : children) obj.addItem(child->toJson(depth + 2));
    });
    obj.addRaw("location", loc.toJson());
    return obj.str();
}

// VariableDeclNode
VariableDeclNode::VariableDeclNode() { kind = ASTKind::VariableDecl; }
void VariableDeclNode::log(int indent) {
    std::cout << jsonIndent(indent) << "VariableDecl (" << name << " : " << varType << ")\n";
    if (initExpr) initExpr->log(indent + 1);
}
std::string VariableDeclNode::toJson(int depth) {
    JsonObject obj(depth);
    obj.add("kind", "VariableDecl");
    obj.add("name", name);
    obj.add("varType", varType);
    if (initExpr) obj.addRaw("init", initExpr->toJson(depth + 1));
    obj.addRaw("location", loc.toJson());
    return obj.str();
}

// StructDeclNode
StructDeclNode::StructDeclNode() { kind = ASTKind::StructDecl; }
void StructDeclNode::log(int indent) {
    std::cout << jsonIndent(indent) << "StructDecl (" << name << ")\n";
    for (auto* child : children) child->log(indent + 1);
}
std::string StructDeclNode::toJson(int depth) {
    JsonObject obj(depth);
    obj.add("kind", "StructDecl");
    obj.add("name", name);
    obj.addArray("children", [&]() {
        for (auto* child : children) obj.addItem(child->toJson(depth + 2));
    });
    obj.addRaw("location", loc.toJson());
    return obj.str();
}
