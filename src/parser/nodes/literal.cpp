#include "parser/nodes/literal.hpp"
#include <iostream>
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
    std::string val = hasDirectValue ? std::to_string(directIntValue)
                                     : (parserToken.token ? parserToken.token->token : "?");
    JsonObject obj(depth);
    obj.add("kind", "Literal");
    obj.add("literalType", literalTypeToString(literalType));
    obj.add("value", val);
    if (literalType == LiteralType::INTEGER && literalBase != 10)
        obj.add("base", literalBase);
    if (literalType == LiteralType::FLOAT)
        obj.add("isFloat", true);
    obj.addRaw("resolvedType", resolvedTypeJson());
    obj.addRaw("location", loc.toJson());
    return obj.str();
}
