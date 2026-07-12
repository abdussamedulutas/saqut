// ============================================================================
// saQut LSP — LSP Tip Tanımları
// ============================================================================

#ifndef SAQUT_LSP_TYPES
#define SAQUT_LSP_TYPES

#include "vendor/nlohmann/json.hpp"
#include <string>

struct JsonRpcMessage {
    std::string    jsonrpc = "2.0";
    nlohmann::json id;       // null → notification
    std::string    method;
    nlohmann::json params;
    nlohmann::json result;
    nlohmann::json error;
    bool isNotification() const { return id.is_null(); }
};

#endif // SAQUT_LSP_TYPES
