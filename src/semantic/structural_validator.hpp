#ifndef SAQUT_SEMANTIC_STRUCTURAL_VALIDATOR
#define SAQUT_SEMANTIC_STRUCTURAL_VALIDATOR

#include "diagnostic/diagnostic_engine.hpp"
#include "parser/ast_node.hpp"

class StructuralValidator {
public:
    explicit StructuralValidator(DiagnosticEngine& diag) : diag_(diag) {}

    void validate(ASTNode* program);

private:
    void walkDecl(ASTNode* node);
    void walkStmt(ASTNode* node);

    DiagnosticEngine& diag_;
    int  loopDepth_  = 0;
    bool inFunction_ = false;
};

#endif // SAQUT_SEMANTIC_STRUCTURAL_VALIDATOR
