#ifndef SAQUT_LSP_JSON_RPC
#define SAQUT_LSP_JSON_RPC

#include "vendor/nlohmann/json.hpp"
#include <iostream>
#include <string>

class JsonRpc {
public:
    static nlohmann::json readMessage(std::istream& in);
    static void           writeMessage(std::ostream& out, const nlohmann::json& msg);
    static nlohmann::json makeResponse(const nlohmann::json& id,
                                       const nlohmann::json& result);
    static nlohmann::json makeNotification(const std::string& method,
                                           const nlohmann::json& params);
    static nlohmann::json makeError(const nlohmann::json& id,
                                    int code, const std::string& msg);
};

#endif // SAQUT_LSP_JSON_RPC
