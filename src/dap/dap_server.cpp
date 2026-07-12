// ============================================================================
// saQut DAP — DapServer Gerçeklemesi
// ============================================================================

#include "dap/dap_server.hpp"
#include "lsp/json_rpc.hpp"

void DapServer::run() {
    while (std::cin.good()) {
        auto msg = JsonRpc::readMessage(std::cin);
        if (msg.is_null() || msg.is_discarded()) continue;

        auto response = handler_.dispatch(msg);

        if (!response.is_null())
            JsonRpc::writeMessage(std::cout, response);
    }
}
