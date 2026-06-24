#ifndef SAQUT_CLI_DAP
#define SAQUT_CLI_DAP

#include "cli/args.hpp"
#include "dap/dap_server.hpp"

inline int cmdDap(const CliArgs&) {
    DapServer server;
    server.run();
    return 0;
}

#endif // SAQUT_CLI_DAP
