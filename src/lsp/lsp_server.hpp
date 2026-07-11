// ============================================================================
// saQut LSP — LspServer (stdio JSON-RPC Sunucusu)
// ============================================================================

#ifndef SAQUT_LSP_SERVER
#define SAQUT_LSP_SERVER

#include "lsp/lsp_handler.hpp"
#include <iostream>

class LspServer {
public:
    void run();
private:
    LspHandler handler_{std::cout};
};

#endif // SAQUT_LSP_SERVER
