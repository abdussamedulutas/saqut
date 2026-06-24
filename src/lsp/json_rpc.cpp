#include "lsp/json_rpc.hpp"

nlohmann::json JsonRpc::readMessage(std::istream& in) {
    int contentLength = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;
        if (line.rfind("Content-Length:", 0) == 0)
            contentLength = std::stoi(line.substr(16));
    }
    if (contentLength <= 0) return nullptr;
    std::string body(contentLength, '\0');
    in.read(body.data(), contentLength);
    return nlohmann::json::parse(body, nullptr, false);
}

void JsonRpc::writeMessage(std::ostream& out, const nlohmann::json& msg) {
    std::string body = msg.dump();
    out << "Content-Length: " << body.size() << "\r\n\r\n" << body;
    out.flush();
}

nlohmann::json JsonRpc::makeResponse(const nlohmann::json& id,
                                      const nlohmann::json& result) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
}

nlohmann::json JsonRpc::makeNotification(const std::string& method,
                                          const nlohmann::json& params) {
    return {{"jsonrpc", "2.0"}, {"method", method}, {"params", params}};
}

nlohmann::json JsonRpc::makeError(const nlohmann::json& id,
                                   int code, const std::string& msg) {
    return {{"jsonrpc", "2.0"}, {"id", id},
            {"error", {{"code", code}, {"message", msg}}}};
}
