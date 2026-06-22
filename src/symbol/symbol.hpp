#ifndef SAQUT_SYMBOL_SYMBOL
#define SAQUT_SYMBOL_SYMBOL

#include <string>
#include <vector>
#include "core/type.hpp"
#include "core/location.hpp"

enum class SymbolKind { Variable, Function, Parameter, Struct, Field, Enum, EnumValue };

inline const char* symbolKindName(SymbolKind k) {
    switch (k) {
        case SymbolKind::Variable:   return "variable";
        case SymbolKind::Function:   return "function";
        case SymbolKind::Parameter:  return "parameter";
        case SymbolKind::Struct:     return "struct";
        case SymbolKind::Field:      return "field";
        case SymbolKind::Enum:       return "enum";
        case SymbolKind::EnumValue:  return "enum_value";
    }
    return "?";
}

class Scope;

struct Symbol {
    std::string                 name;
    SymbolKind                  kind = SymbolKind::Variable;
    Type                        type;
    int                         moduleId = -1;   // ModuleRegistry ID; -1 = "<main>", 0 = "__builtin__"
    SourceLocation              definitionLoc;
    std::vector<SourceLocation> references;
    Scope*                      scope    = nullptr;
    bool                        isBuiltin = false;
};

#endif // SAQUT_SYMBOL_SYMBOL
