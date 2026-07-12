// ============================================================================
// saQut Compiler — LiteralNode Gerçeklemesi
// ============================================================================

#include "parser/nodes/literal.hpp"
#include <iostream>
#include "parser/ast_json.hpp"

LiteralNode::LiteralNode() { kind = ASTKind::Literal; }

void LiteralNode::log(int indent) {
    std::string val = hasDirectValue ? std::to_string(directIntValue)
                                     : (parserToken.token ? parserToken.token->token : "?");
    std::cout << padRight("", indent)
              << Color::SoftMavi << "Literal" << Color::Reset
              << " {" << Color::SoftTuruncu << val << Color::Reset << "} "
              << Color::SoftGri << literalTypeToString(literalType) << Color::Reset;
    if (isConstant) std::cout << " " << Color::SoftTurkuaz << "[folded]" << Color::Reset;
    if (literalType == LiteralType::INTEGER && literalBase != 10)
        std::cout << " " << Color::SoftGri << "(base " << literalBase << ")" << Color::Reset;
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
