#include "lsp/lsp_server.hpp"
#include "lsp/json_rpc.hpp"

void LspServer::run() {
    while (std::cin.good()) {
        auto msg = JsonRpc::readMessage(std::cin);
        if (msg.is_null() || msg.is_discarded()) continue;

        // Faz 6 (#84): handler'da beklenmedik istisna (eksik params alanı,
        // tip uyuşmazlığı) sunucuyu düşürmemeli — istek ise InternalError
        // yanıtı dön, notification ise yut ve sonraki mesaja geç.
        nlohmann::json response;
        try {
            response = handler_.dispatch(msg);
        } catch (const std::exception& e) {
            nlohmann::json id = msg.value("id", nlohmann::json(nullptr));
            if (id.is_null()) continue;
            response = JsonRpc::makeError(id, -32603,
                std::string("Internal error: ") + e.what());
        }

        if (!response.is_null())
            JsonRpc::writeMessage(std::cout, response);
    }
}
