#ifndef SAQUT_SEMANTIC_TYPE_CHECKER
#define SAQUT_SEMANTIC_TYPE_CHECKER

#include "symbol/symbol_table.hpp"
#include "diagnostic/diagnostic_engine.hpp"
#include "parser/ast_node.hpp"
#include "core/type.hpp"

class TypeChecker {
public:
    TypeChecker(SymbolTable& table, DiagnosticEngine& diag)
        : table_(table), diag_(diag) {}

    void check(ASTNode* program);

private:
    // İfadeyi gez, resolvedType ata, tipi döndür.
    // expected: bağlam tipi — literal genişletme kararı için.
    Type checkExpr(ASTNode* node, const Type& expected = Type::error());

    void checkStmt(ASTNode* node);
    void checkFunction(ASTNode* fnNode);

    // Atama / parametre uyumu: true = geçerli (uyarı dahil).
    // srcIsLiteral: RHS doğrudan bir Literal node'u mu?
    bool checkAssign(const Type& target, const Type& src,
                     bool srcIsLiteral,
                     const SourceLocation& loc,
                     const std::string& context);

    // İki sayısal tipin genişlik sırası: int=0, float=1, double=2; -1 = sayısal değil.
    static int numericRank(const Type& t);

    SymbolTable&      table_;
    DiagnosticEngine& diag_;

    Type currentReturnType_;   // aktif fonksiyonun beklenen dönüş tipi
    bool inFunction_ = false;
};

#endif // SAQUT_SEMANTIC_TYPE_CHECKER
