#include "parser/nodes/literal.hpp"
#include <iostream>
#include <sstream>
#include "parser/ast_json.hpp"

LiteralNode::LiteralNode() { kind = ASTKind::Literal; }

void LiteralNode::log(int indent) {
    std::string val = hasDirectValue ? std::to_string(directIntValue)
                                     : (parserToken.token ? parserToken.token->token : "?");
    std::cout << padRight("", indent)
              << "Literal {" << val << "} "
              << literalTypeToString(literalType);
    if (isConstant) std::cout << " [folded]";
    if (literalType == LiteralType::INTEGER && literalBase != 10)
        std::cout << " (base " << literalBase << ")";
    std::cout << "\n";
}

std::string LiteralNode::toJson(int depth) {
    std::string in  = jsonIndent(depth);
    std::string val = hasDirectValue ? std::to_string(directIntValue)
                                     : (parserToken.token ? parserToken.token->token : "?");
    std::ostringstream ss;
    ss << "{\n"
       << in << "  \"kind\": \"Literal\",\n"
       << in << "  \"literalType\": \"" << literalTypeToString(literalType) << "\",\n"
       << in << "  \"value\": \"" << jsonEscape(val) << "\"";
    if (literalType == LiteralType::INTEGER && literalBase != 10) {
        ss << ",\n" << in << "  \"base\": " << literalBase;
    }
    if (literalType == LiteralType::FLOAT) {
        ss << ",\n" << in << "  \"isFloat\": true";
    }
    ss << ",\n" << in << "  \"resolvedType\": " << resolvedTypeJson();
    ss << ",\n" << in << "  \"location\": " << loc.toJson() << "\n"
       << in << "}";
    return ss.str();
}
