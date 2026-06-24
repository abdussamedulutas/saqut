#ifndef SAQUT_CLI_LSP
#define SAQUT_CLI_LSP

#include "cli/args.hpp"
#include "lsp/lsp_server.hpp"

inline int cmdLsp(const CliArgs&) {
    LspServer server;
    server.run();
    return 0;
}

#endif // SAQUT_CLI_LSP
