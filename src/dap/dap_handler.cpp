#include "dap/dap_handler.hpp"
#include "module/module_loader.hpp"
#include "module/module_graph.hpp"
#include "core/module_registry.hpp"
#include "symbol/symbol_table.hpp"
#include "symbol/symbol_collector.hpp"
#include "semantic/type_checker.hpp"
#include "semantic/structural_validator.hpp"
#include "diagnostic/diagnostic_engine.hpp"
#include "ir/ir_generator.hpp"
#include "vm/value.hpp"
#include "vm/object.hpp"
#include <sstream>

// ─────────────────────────────────────────────────────────────────────────────

void DapHandler::sendEvent(const std::string& event,
                            const nlohmann::json& body) {
    nlohmann::json msg = {
        {"jsonrpc", "2.0"},
        {"method",  "event"},
        {"params",  {{"event", event}, {"body", body}}}
    };
    // DAP eventi: seq alanı gerekli; basit incrementing seq
    static int seq = 1;
    msg["seq"] = seq++;
    JsonRpc::writeMessage(out_, msg);
}

std::string DapHandler::valueToString(const Value& v) const {
    switch (v.kind) {
        case ValueKind::Int:     return std::to_string(v.intValue);
        case ValueKind::Float:   {
            std::ostringstream ss;
            ss << v.floatValue;
            return ss.str();
        }
        case ValueKind::Decimal: return v.decimalValue.toString();
        case ValueKind::String:  return "\"" + v.stringValue + "\"";
        case ValueKind::Ref:     return v.ref ? "<object>" : "<null-ref>";
        case ValueKind::Null:    return "null";
    }
    return "?";
}

// ─────────────────────────────────────────────────────────────────────────────

nlohmann::json DapHandler::dispatch(const nlohmann::json& msg) {
    if (msg.is_discarded() || !msg.contains("command")) return nullptr;

    std::string command = msg["command"].get<std::string>();
    nlohmann::json id   = msg.value("seq", nlohmann::json(nullptr));
    nlohmann::json args = msg.value("arguments", nlohmann::json::object());

    if (command == "initialize")   return handleInitialize(id, args);
    if (command == "launch")       return handleLaunch(id, args);
    if (command == "setBreakpoints") return handleSetBreakpoints(id, args);
    if (command == "continue")     return handleContinue(id, args);
    if (command == "next")         return handleStepOver(id, args);
    if (command == "stepIn")       return handleStepIn(id, args);
    if (command == "threads")      return handleThreads(id, args);
    if (command == "stackTrace")   return handleStackTrace(id, args);
    if (command == "scopes")       return handleScopes(id, args);
    if (command == "variables")    return handleVariables(id, args);
    if (command == "disconnect")   return handleDisconnect(id, args);

    if (command == "configurationDone") {
        return {{"seq", 0}, {"type","response"}, {"request_seq", id},
                {"success", true}, {"command", command}, {"body", {}}};
    }

    return nullptr;
}

static nlohmann::json makeDapResponse(const nlohmann::json& reqSeq,
                                       const std::string& command,
                                       const nlohmann::json& body,
                                       bool success = true) {
    static int seq = 100;
    return {
        {"seq",         seq++},
        {"type",        "response"},
        {"request_seq", reqSeq},
        {"success",     success},
        {"command",     command},
        {"body",        body}
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// Tier 0 — Zorunlu metodlar
// ─────────────────────────────────────────────────────────────────────────────

nlohmann::json DapHandler::handleInitialize(const nlohmann::json& id,
                                             const nlohmann::json& /*args*/) {
    nlohmann::json caps = {
        {"supportsConfigurationDoneRequest", true},
        {"supportsFunctionBreakpoints",      false},
        {"supportsConditionalBreakpoints",   false},
        {"supportsSetVariable",              false},
        {"supportsStepBack",                 false},
    };
    sendEvent("initialized", {});
    return makeDapResponse(id, "initialize", caps);
}

nlohmann::json DapHandler::handleLaunch(const nlohmann::json& id,
                                         const nlohmann::json& args) {
    std::string program = args.value("program", "");
    if (program.empty()) {
        sendEvent("terminated", {});
        return makeDapResponse(id, "launch", {}, false);
    }

    // Derle
    ModuleRegistry   registry;
    DiagnosticEngine diag;
    SymbolTable      table;
    ModuleGraph      graph = ModuleLoader(registry, diag).load(program);

    if (!diag.hasErrors()) {
        SymbolCollector(table, diag).collectModuleGraph(graph);
        if (!diag.hasErrors()) {
            for (auto& u : graph.units) TypeChecker(table, diag).check(u.ast);
            for (auto& u : graph.units) StructuralValidator(diag).validate(u.ast);
        }
    }

    if (diag.hasErrors()) {
        sendEvent("output", {{"category","stderr"}, {"output","Build failed\n"}});
        sendEvent("terminated", {});
        return makeDapResponse(id, "launch", {});
    }

    IRGenerator irgen;
    irProgram_ = std::make_unique<IRProgram>(
        irgen.generateModuleGraph(graph, table));

    vm_ = std::make_unique<Interpreter>(*irProgram_);

    bool stopOnEntry = args.value("stopOnEntry", true);
    if (stopOnEntry) {
        vm_->stepInstruction();
        sendEvent("stopped", {{"reason","entry"}, {"threadId",1}});
    } else {
        vm_->resume();
        sendEvent("terminated", {});
    }

    return makeDapResponse(id, "launch", {});
}

nlohmann::json DapHandler::handleSetBreakpoints(const nlohmann::json& id,
                                                  const nlohmann::json& args) {
    if (vm_) vm_->clearAllBreakpoints();

    std::string sourceFile;
    if (args.contains("source") && args["source"].contains("path"))
        sourceFile = args["source"]["path"].get<std::string>();

    nlohmann::json bps = nlohmann::json::array();
    if (args.contains("breakpoints")) {
        for (const auto& bp : args["breakpoints"]) {
            int line = bp.value("line", 0);
            if (vm_) vm_->setBreakpoint(sourceFile, line);
            bps.push_back({
                {"id",       nextBpId_++},
                {"verified", true},
                {"line",     line}
            });
        }
    }

    return makeDapResponse(id, "setBreakpoints", {{"breakpoints", bps}});
}

nlohmann::json DapHandler::handleContinue(const nlohmann::json& id,
                                           const nlohmann::json& /*args*/) {
    if (vm_) {
        vm_->resume();
        if (vm_->state() == Interpreter::RunState::Paused) {
            sendEvent("stopped", {{"reason","breakpoint"}, {"threadId",1}});
        } else {
            sendEvent("terminated", {});
        }
    }
    return makeDapResponse(id, "continue", {{"allThreadsContinued", true}});
}

nlohmann::json DapHandler::handleStepOver(const nlohmann::json& id,
                                           const nlohmann::json& /*args*/) {
    if (vm_) {
        vm_->stepOver();
        sendEvent("stopped", {{"reason","step"}, {"threadId",1}});
    }
    return makeDapResponse(id, "next", {});
}

nlohmann::json DapHandler::handleStepIn(const nlohmann::json& id,
                                         const nlohmann::json& /*args*/) {
    if (vm_) {
        vm_->stepInstruction();
        sendEvent("stopped", {{"reason","step"}, {"threadId",1}});
    }
    return makeDapResponse(id, "stepIn", {});
}

nlohmann::json DapHandler::handleThreads(const nlohmann::json& id,
                                          const nlohmann::json& /*args*/) {
    nlohmann::json threads = nlohmann::json::array();
    threads.push_back({{"id", 1}, {"name", "main"}});
    return makeDapResponse(id, "threads", {{"threads", threads}});
}

// ─────────────────────────────────────────────────────────────────────────────
// Tier 1 — Yığın ve değişken okuma
// ─────────────────────────────────────────────────────────────────────────────

nlohmann::json DapHandler::handleStackTrace(const nlohmann::json& id,
                                             const nlohmann::json& /*args*/) {
    nlohmann::json frames = nlohmann::json::array();
    if (vm_) {
        int depth = vm_->callDepth();
        for (int i = 0; i < depth; ++i) {
            frames.push_back({
                {"id",     i},
                {"name",   vm_->frameFunctionName(i)},
                {"line",   vm_->frameSourceLine(i)},
                {"column", 0},
                {"source", {{"path", vm_->currentSourceFile()}}}
            });
        }
    }
    return makeDapResponse(id, "stackTrace",
        {{"stackFrames", frames}, {"totalFrames", (int)frames.size()}});
}

nlohmann::json DapHandler::handleScopes(const nlohmann::json& id,
                                         const nlohmann::json& args) {
    int frameId = args.value("frameId", 0);
    nlohmann::json scopes = nlohmann::json::array();
    scopes.push_back({
        {"name",               "Locals"},
        {"variablesReference", 1000 + frameId},
        {"expensive",          false}
    });
    return makeDapResponse(id, "scopes", {{"scopes", scopes}});
}

nlohmann::json DapHandler::handleVariables(const nlohmann::json& id,
                                            const nlohmann::json& args) {
    int ref     = args.value("variablesReference", 0);
    int frameId = (ref >= 1000) ? ref - 1000 : 0;

    nlohmann::json vars = nlohmann::json::array();
    if (vm_) {
        // Her frame'de slotları listele (slot sayısı bilinmiyorsa 16 dene)
        for (int slot = 0; slot < 16; ++slot) {
            Value v = vm_->readSlotInFrame(frameId, slot);
            if (v.kind == ValueKind::Int && v.intValue == 0 && slot > 4) break;
            vars.push_back({
                {"name",               "slot[" + std::to_string(slot) + "]"},
                {"value",              valueToString(v)},
                {"type",               ""},
                {"variablesReference", 0}
            });
        }
    }
    return makeDapResponse(id, "variables", {{"variables", vars}});
}

nlohmann::json DapHandler::handleDisconnect(const nlohmann::json& id,
                                             const nlohmann::json& /*args*/) {
    vm_.reset();
    irProgram_.reset();
    return makeDapResponse(id, "disconnect", {});
}
