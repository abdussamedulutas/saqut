#ifndef SAQUT_DAP_TYPES
#define SAQUT_DAP_TYPES

#include <string>

struct DapBreakpoint {
    int         id;
    bool        verified;
    int         line;
    std::string source;
};

struct DapStackFrame {
    int         id;
    std::string name;
    std::string sourceFile;
    int         line;
};

struct DapVariable {
    std::string name;
    std::string value;
    std::string type;
    int         variablesReference;  // 0 = yaprak, >0 = genişletilebilir
};

#endif // SAQUT_DAP_TYPES
