#include "parser/nodes/identifier.hpp"
#include <iostream>
#include "parser/ast_json.hpp"

IdentifierNode::IdentifierNode() { kind = ASTKind::Identifier; }

void IdentifierNode::log(int indent) {
    std::cout << padRight("", indent)
              << Color::SoftMavi << "Identifier" << Color::Reset
              << " {" << Color::SoftYesil
              << (parserToken.token ? parserToken.token->token : "?")
              << Color::Reset << "}\n";
}

std::string IdentifierNode::toJson(int depth) {
    JsonObject obj(depth);
    obj.add("kind", "Identifier");
    obj.add("name", parserToken.token ? parserToken.token->token : "?");
    obj.addRaw("resolvedType", resolvedTypeJson());
    obj.addRaw("location", loc.toJson());
    return obj.str();
}
