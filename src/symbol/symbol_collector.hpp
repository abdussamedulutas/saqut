#ifndef SAQUT_SYMBOL_COLLECTOR
#define SAQUT_SYMBOL_COLLECTOR

#include <string>
#include <unordered_map>
#include <vector>
#include "symbol/symbol_table.hpp"
#include "diagnostic/diagnostic_engine.hpp"
#include "core/type.hpp"
#include "core/location.hpp"
#include "parser/ast_node.hpp"

class SymbolCollector {
public:
    SymbolCollector(SymbolTable& t, DiagnosticEngine& d) : table_(t), diag_(d) {}

    // seedBuiltins → pass1 → structCycles(E010) → pass2
    void collect(ASTNode* program);

private:
    void seedBuiltins();
    void pass1Globals(ASTNode* program);
    void checkStructCycles();
    void pass2Bodies(ASTNode* program);
    void walkStmt(ASTNode* node);
    void walkExpr(ASTNode* node);

    Type typeFromName(const std::string& n, const SourceLocation& loc);

    SymbolTable&     table_;
    DiagnosticEngine& diag_;

    // struct adı → içerdiği struct-tip alan adları (cycle check için)
    std::unordered_map<std::string, std::vector<std::string>> structFields_;
};

#endif // SAQUT_SYMBOL_COLLECTOR
