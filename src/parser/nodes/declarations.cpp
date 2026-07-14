// ============================================================================
// saQut Compiler — Bildirim Düğümleri Gerçeklemesi
// ============================================================================

#include "parser/nodes/declarations.hpp"
#include "parser/ast_json.hpp"

// FunctionDeclNode
FunctionDeclNode::FunctionDeclNode() { kind = ASTKind::FunctionDecl; }
FunctionDeclNode::~FunctionDeclNode() { for (auto* p : params) delete p; }
void FunctionDeclNode::log(int indent) {
    std::cout << jsonIndent(indent) << Color::SoftMavi << "FunctionDecl" << Color::Reset
              << " (" << Color::SoftYesil << name << Color::Reset
              << " : " << Color::SoftPembe << returnType << Color::Reset << ")\n";
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
    std::cout << jsonIndent(indent) << Color::SoftMavi << "VariableDecl" << Color::Reset
              << " (" << Color::SoftYesil << name << Color::Reset
              << " : " << Color::SoftPembe << varType << Color::Reset << ")\n";
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
    std::cout << jsonIndent(indent) << Color::SoftMavi << "EnumDecl" << Color::Reset
              << " (" << Color::SoftYesil << name << Color::Reset << ")\n";
    for (auto& m : members)
        std::cout << jsonIndent(indent + 1) << Color::SoftYesil << m.name << Color::Reset
                  << " " << Color::SoftGri << "=" << Color::Reset << " "
                  << Color::SoftTuruncu << m.value << Color::Reset << "\n";
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
    std::cout << jsonIndent(indent) << Color::SoftMavi << "StructDecl" << Color::Reset
              << " (" << Color::SoftYesil << name << Color::Reset << ")\n";
    for (auto* child : children) child->log(indent + 1);
}
std::string StructDeclNode::toJson(int depth) {
    JsonObject obj(depth);
    obj.add("kind", "StructDecl");
    obj.add("name", name);
    obj.add("isExported", isExported);
    obj.addArray("children", [&]() {
        for (auto* child : children) obj.addItem(child->toJson(depth + 2));
    });
    obj.addRaw("location", loc.toJson());
    return obj.str();
}

// ImportDeclNode
ImportDeclNode::ImportDeclNode() { kind = ASTKind::ImportDecl; }
void ImportDeclNode::log(int indent) {
    std::cout << jsonIndent(indent) << Color::SoftMavi << "ImportDecl" << Color::Reset
              << " " << Color::SoftGri << "from" << Color::Reset << " \""
              << Color::SoftPembe << sourcePath << Color::Reset << "\": {";
    for (size_t i = 0; i < importedNames.size(); i++) {
        if (i) std::cout << Color::SoftGri << ", " << Color::Reset;
        std::cout << Color::SoftYesil << importedNames[i] << Color::Reset;
    }
    std::cout << "}\n";
}
std::string ImportDeclNode::toJson(int depth) {
    JsonObject obj(depth);
    obj.add("kind", "ImportDecl");
    obj.add("sourcePath", sourcePath);
    obj.add("isModuleName", isModuleName);
    obj.addArray("importedNames", [&]() {
        for (auto& n : importedNames)
            obj.addItem("\"" + n + "\"");
    });
    obj.addRaw("location", loc.toJson());
    return obj.str();
}

// FfiDeclNode (ADR-034, #107)
FfiDeclNode::FfiDeclNode() { kind = ASTKind::FfiDecl; }
FfiDeclNode::~FfiDeclNode() { for (auto* p : params) delete p; }
void FfiDeclNode::log(int indent) {
    std::cout << jsonIndent(indent) << Color::SoftMavi << "FfiDecl" << Color::Reset
              << " (" << Color::SoftYesil << name << Color::Reset
              << " : " << Color::SoftPembe << returnType << Color::Reset << ") "
              << Color::SoftGri << ": " << Color::Reset << hostId
              << Color::SoftGri << " from " << Color::Reset << moduleName;
    if (!requiresCap.empty())
        std::cout << Color::SoftGri << " requires " << Color::Reset << requiresCap;
    std::cout << "\n";
    for (auto* child : children) child->log(indent + 1);
}
std::string FfiDeclNode::toJson(int depth) {
    JsonObject obj(depth);
    obj.add("kind", "FfiDecl");
    obj.add("name", name);
    obj.add("returnType", returnType);
    obj.add("hostId", hostId);
    obj.add("module", moduleName);
    obj.add("requiresCap", requiresCap);
    obj.addArray("params", [&]() {
        for (auto* p : params) obj.addItem(((ASTNode*)p)->toJson(depth + 2));
    });
    obj.addRaw("location", loc.toJson());
    return obj.str();
}
