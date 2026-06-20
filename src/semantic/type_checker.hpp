#ifndef SAQUT_SEMANTIC_TYPE_CHECKER
#define SAQUT_SEMANTIC_TYPE_CHECKER

#include <unordered_set>
#include <string>
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

    // ADR-021: if-narrowing — null kontrolü kalıbını ayrıştır
    // Dönüş: {varName, isNotNull} — "a != null" → {a, true}; "a == null" → {a, false}; {"", _} = kalıp yok
    static std::pair<std::string, bool> extractNullCheck(ASTNode* cond);
    // Bir statement her zaman çıkış yapıyor mu? (return/throw/break/continue)
    static bool alwaysExits(ASTNode* stmt);

    SymbolTable&      table_;
    DiagnosticEngine& diag_;

    Type currentReturnType_;   // aktif fonksiyonun beklenen dönüş tipi
    bool inFunction_ = false;

    // ADR-021: akış-duyarlı null daraltma — bu kapsamda non-null olduğu bilinen değişkenler
    std::unordered_set<std::string> narrowedNonNull_;
};

#endif // SAQUT_SEMANTIC_TYPE_CHECKER
