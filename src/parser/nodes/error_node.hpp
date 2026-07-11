#ifndef SAQUT_AST_ERROR_NODE
#define SAQUT_AST_ERROR_NODE

#include "parser/ast_node.hpp"

// ErrorNode — panic-mode kurtarma yer tutucusu (Faz 2).
// Parser sözdizimsel bir hatayla karşılaşınca konumlu bir tanı üretir, bilinen
// bir sınıra kadar token atlar ve bu düğümü ağaca bırakıp devam eder. Analiz
// katmanları (SymbolCollector/TypeChecker/StructuralValidator) bunu switch
// ifadelerindeki default dalıyla sessizce atlar.
class ErrorNode : public ASTNode {
public:
    std::string message; // hangi tanının üretildiği (debug/log amaçlı)
    std::string code;     // ör. "E901"

    ErrorNode();
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

#endif // SAQUT_AST_ERROR_NODE
