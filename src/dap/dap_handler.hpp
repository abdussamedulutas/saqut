#ifndef SAQUT_DAP_HANDLER
#define SAQUT_DAP_HANDLER

#include "vendor/nlohmann/json.hpp"
#include "lsp/json_rpc.hpp"
#include "dap/dap_types.hpp"
#include "vm/interpreter.hpp"
#include "ir/ir_program.hpp"
#include <ostream>
#include <memory>

class DapHandler {
public:
    explicit DapHandler(std::ostream& out) : out_(out) {}

    nlohmann::json dispatch(const nlohmann::json& msg);

private:
    std::ostream&              out_;
    std::unique_ptr<IRProgram> irProgram_;
    std::unique_ptr<Interpreter> vm_;
    int                        nextBpId_ = 1;

    nlohmann::json handleInitialize(const nlohmann::json& id,
                                    const nlohmann::json& args);
    nlohmann::json handleLaunch(const nlohmann::json& id,
                                const nlohmann::json& args);
    nlohmann::json handleSetBreakpoints(const nlohmann::json& id,
                                        const nlohmann::json& args);
    nlohmann::json handleContinue(const nlohmann::json& id,
                                  const nlohmann::json& args);
    nlohmann::json handleStepOver(const nlohmann::json& id,
                                  const nlohmann::json& args);
    nlohmann::json handleStepIn(const nlohmann::json& id,
                                const nlohmann::json& args);
    nlohmann::json handleThreads(const nlohmann::json& id,
                                 const nlohmann::json& args);
    nlohmann::json handleStackTrace(const nlohmann::json& id,
                                    const nlohmann::json& args);
    nlohmann::json handleScopes(const nlohmann::json& id,
                                const nlohmann::json& args);
    nlohmann::json handleVariables(const nlohmann::json& id,
                                   const nlohmann::json& args);
    nlohmann::json handleDisconnect(const nlohmann::json& id,
                                    const nlohmann::json& args);

    void sendEvent(const std::string& event, const nlohmann::json& body);
    std::string valueToString(const Value& v) const;
};

#endif // SAQUT_DAP_HANDLER
