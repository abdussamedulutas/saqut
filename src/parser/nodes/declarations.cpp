#include "parser/nodes/declarations.hpp"
#include "parser/ast_json.hpp"

// FunctionDeclNode
FunctionDeclNode::FunctionDeclNode() { kind = ASTKind::FunctionDecl; }
FunctionDeclNode::~FunctionDeclNode() { for (auto* p : params) delete p; }
void FunctionDeclNode::log(int indent) {
    std::cout << jsonIndent(indent) << "FunctionDecl (" << name << " : " << returnType << ")\n";
    for (auto* child : children) child->log(indent + 1);
}
std::string FunctionDeclNode::toJson(int depth) {
    JsonObject obj(depth);
    obj.add("kind", "FunctionDecl");
    obj.add("name", name);
    obj.add("returnType", returnType);
    obj.addArray("params", [&]() {
        for (auto* p : params) obj.addItem(p->toJson(depth + 2));
    });
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
    obj.add("isReachable", isReachable);
    if (initExpr) obj.addRaw("init", initExpr->toJson(depth + 1));
    obj.addRaw("location", loc.toJson());
    return obj.str();
}

// EnumDeclNode
EnumDeclNode::EnumDeclNode() { kind = ASTKind::EnumDecl; }
void EnumDeclNode::log(int indent) {
    std::cout << jsonIndent(indent) << "EnumDecl (" << name << ")\n";
    for (auto& m : members)
        std::cout << jsonIndent(indent + 1) << m.name << " = " << m.value << "\n";
}
std::string EnumDeclNode::toJson(int depth) {
    JsonObject obj(depth);
    obj.add("kind", "EnumDecl");
    obj.add("name", name);
    obj.addArray("members", [&]() {
        for (auto& m : members) {
            std::string entry = "{\"name\":\"" + m.name + "\",\"value\":" + std::to_string(m.value) + "}";
            obj.addItem(entry);
        }
    });
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
