#ifndef SAQUT_SYMBOL_SYMBOL
#define SAQUT_SYMBOL_SYMBOL

#include <string>
#include <vector>
#include "core/type.hpp"
#include "core/location.hpp"

enum class SymbolKind { Variable, Function, Parameter, Struct, Field };

inline const char* symbolKindName(SymbolKind k) {
    switch (k) {
        case SymbolKind::Variable:  return "variable";
        case SymbolKind::Function:  return "function";
        case SymbolKind::Parameter: return "parameter";
        case SymbolKind::Struct:    return "struct";
        case SymbolKind::Field:     return "field";
    }
    return "?";
}

class Scope;

struct Symbol {
    std::string                 name;
    SymbolKind                  kind = SymbolKind::Variable;
    Type                        type;
    SourceLocation              definitionLoc;
    std::vector<SourceLocation> references;
    Scope*                      scope    = nullptr;
    bool                        isBuiltin = false;
};

#endif // SAQUT_SYMBOL_SYMBOL
