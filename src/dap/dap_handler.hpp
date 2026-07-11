#ifndef SAQUT_DAP_HANDLER
#define SAQUT_DAP_HANDLER

#include "vendor/nlohmann/json.hpp"
#include "lsp/json_rpc.hpp"
#include "dap/dap_types.hpp"
#include "vm/interpreter.hpp"
#include "ir/ir_program.hpp"
#include "vm/value.hpp"
#include <ostream>
#include <memory>

class DapHandler {
public:
    explicit DapHandler(std::ostream& out) : out_(out) {}

    // Ana giriş: DAP-format mesajı işle, response döndür (null = response yok).
    // Event'ler doğrudan out_'a yazılır.
    nlohmann::json dispatch(const nlohmann::json& msg);

private:
    std::ostream&               out_;
    std::unique_ptr<IRProgram>  irProgram_;
    std::unique_ptr<Interpreter> vm_;
    int                         nextBpId_   = 1;
    int                         nextVarRef_ = 1;

    // Yeni yaşam döngüsü durumu
    bool  initialized_    = false;
    int   responseSeq_    = 100;     // cevap/event seq numaraları

    // ── Handler'lar ──────────────────────────────────────────────────────────
    nlohmann::json handleInitialize(const nlohmann::json& req);
    nlohmann::json handleLaunch(const nlohmann::json& req);
    nlohmann::json handleSetBreakpoints(const nlohmann::json& req);
    nlohmann::json handleConfigurationDone(const nlohmann::json& req);
    nlohmann::json handleContinue(const nlohmann::json& req);
    nlohmann::json handleNext(const nlohmann::json& req);
    nlohmann::json handleStepIn(const nlohmann::json& req);
    nlohmann::json handleStepOut(const nlohmann::json& req);
    nlohmann::json handlePause(const nlohmann::json& req);
    nlohmann::json handleThreads(const nlohmann::json& req);
    nlohmann::json handleStackTrace(const nlohmann::json& req);
    nlohmann::json handleScopes(const nlohmann::json& req);
    nlohmann::json handleVariables(const nlohmann::json& req);
    nlohmann::json handleDisconnect(const nlohmann::json& req);

    // ── Yardımcılar ──────────────────────────────────────────────────────────
    nlohmann::json makeResponse(int requestSeq, const std::string& command,
                                const nlohmann::json& body,
                                bool success = true);
    void sendEvent(const std::string& event, const nlohmann::json& body);

    // Değişken değerini DAP string'ine çevir
    std::string valueToString(const Value& v) const;

    // Struct/array child variable'ları oluştur (tek seviye)
    nlohmann::json buildChildVariables(const Value& v, int parentVarRef);

    // Koşu: budget döngüsüyle VM çalıştır, event'leri yönet
    void runWithBudget();
};

#endif // SAQUT_DAP_HANDLER
