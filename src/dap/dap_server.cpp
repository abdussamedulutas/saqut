// ============================================================================
// saQut DAP — DapServer Gerçeklemesi
// ============================================================================

#include "dap/dap_server.hpp"
#include "lsp/json_rpc.hpp"

void DapServer::run() {
    // Faz 8 (#105): std::cin yerine FrameReader — runWithBudget'ın tur-arası
    // bekleyen-mesaj kontrolüyle aynı tampon paylaşılır (bkz. frame_reader.hpp).
    FrameReader& reader = handler_.reader();
    while (!reader.eof()) {
        auto msg = reader.readMessage();
        if (msg.is_null() || msg.is_discarded()) continue;

        auto response = handler_.dispatch(msg);

        if (!response.is_null())
            JsonRpc::writeMessage(std::cout, response);
    }
}
