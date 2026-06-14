#include "parser/nodes/identifier.hpp"
#include <iostream>
#include <sstream>
#include "parser/ast_json.hpp"

IdentifierNode::IdentifierNode() { kind = ASTKind::Identifier; }

void IdentifierNode::log(int indent) {
    std::cout << padRight("", indent)
              << "Identifier {" << (parserToken.token ? parserToken.token->token : "?") << "}\n";
}

std::string IdentifierNode::toJson(int depth) {
    std::string in = jsonIndent(depth);
    std::string name = parserToken.token ? parserToken.token->token : "?";
    std::ostringstream ss;
    ss << "{\n"
       << in << "  \"kind\": \"Identifier\",\n"
       << in << "  \"name\": \"" << jsonEscape(name) << "\",\n"
       << in << "  \"location\": " << loc.toJson() << "\n"
       << in << "}";
    return ss.str();
}
