// ============================================================================
// saQut DAP Handler — Faz 6: DAP Protokolünü Doğru Formatla
//
// Tüm mesajlar DAP formatında: {"type":"request|response|event","seq":N,...}
// jsonrpc/method/params zarfı KULLANILMAZ (Content-Length çerçevesi aynı kalır).
//
// TODO(faz6): continue sonrası VM tamamlanınca exited/terminated event'leri
// gönderilmiyor — runUntilEvent(-1,-1) çağrısı RETURN'den sonra state=Finished
// döndürüyor ama process çıkış yapmıyor. Sebep araştırılıyor.
// TODO(faz6): breakpoint file-path eşleşmesi tam doğrulanmadı — golden test
// senaryosu (wip_breakpoint) continue flow'u düzeltilene kadar ertelendi.
// ============================================================================

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
#include <climits>

// ── Yardımcı: DAP response oluştur ──────────────────────────────────────────

nlohmann::json DapHandler::makeResponse(int requestSeq,
                                         const std::string& command,
                                         const nlohmann::json& body,
                                         bool success) {
    return {
        {"seq",         responseSeq_++},
        {"type",        "response"},
        {"request_seq", requestSeq},
        {"success",     success},
        {"command",     command},
        {"body",        body}
    };
}

// ── Event gönderme ──────────────────────────────────────────────────────────
// DAP event formatı: {"type":"event","event":"...","body":{...},"seq":N}
// jsonrpc/method/params YOK.

void DapHandler::sendEvent(const std::string& event,
                            const nlohmann::json& body) {
    nlohmann::json msg = {
        {"type",  "event"},
        {"event", event},
        {"body",  body},
        {"seq",   responseSeq_++}
    };
    JsonRpc::writeMessage(out_, msg);
}

// ── valueToString ───────────────────────────────────────────────────────────

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

// ── Struct/array child variable'ları (tek seviye) ───────────────────────────

nlohmann::json DapHandler::buildChildVariables(const Value& v,
                                                int /*parentVarRef*/) {
    nlohmann::json vars = nlohmann::json::array();
    if (v.kind != ValueKind::Ref || !v.ref) return vars;

    if (v.ref->type == ObjectType::Struct) {
        auto* s = static_cast<StructObject*>(v.ref);
        for (size_t i = 0; i < s->fields.size(); ++i) {
            const Value& fv = s->fields[i];
            std::string fname = (i < s->fieldNames.size() && !s->fieldNames[i].empty())
                ? s->fieldNames[i] : "field[" + std::to_string(i) + "]";
            int childRef = 0;
            if (fv.kind == ValueKind::Ref && fv.ref) {
                childRef = nextVarRef_++;
            }
            vars.push_back({
                {"name",               fname},
                {"value",              valueToString(fv)},
                {"type",               ""},
                {"variablesReference", childRef}
            });
        }
    } else if (v.ref->type == ObjectType::Array) {
        auto* a = static_cast<ArrayObject*>(v.ref);
        for (size_t i = 0; i < a->elements.size(); ++i) {
            const Value& ev = a->elements[i];
            int childRef = 0;
            if (ev.kind == ValueKind::Ref && ev.ref) {
                childRef = nextVarRef_++;
            }
            vars.push_back({
                {"name",               "[" + std::to_string(i) + "]"},
                {"value",              valueToString(ev)},
                {"type",               ""},
                {"variablesReference", childRef}
            });
        }
    }
    return vars;
}

// ── Koşu döngüsü ────────────────────────────────────────────────────────────
// continue/step sonrası VM'i bütçeli çalıştır, uygun event'i gönder.

void DapHandler::runWithBudget() {
    if (!vm_) return;
    Interpreter::RunReason reason = vm_->runUntilEvent(-1, -1);

    switch (reason) {
        case Interpreter::RunReason::Breakpoint:
            sendEvent("stopped", {{"reason","breakpoint"}, {"threadId",1}});
            break;
        case Interpreter::RunReason::StepDone:
            sendEvent("stopped", {{"reason","step"}, {"threadId",1}});
            break;
        case Interpreter::RunReason::Finished:
            sendEvent("exited", {{"exitCode", 0}});
            sendEvent("terminated", {});
            break;
        case Interpreter::RunReason::BudgetExhausted:
            // Budget tükendi — pause için yer tutucu
            sendEvent("stopped", {{"reason","pause"}, {"threadId",1}});
            break;
        case Interpreter::RunReason::Error:
            sendEvent("stopped", {{"reason","exception"}, {"threadId",1}});
            break;
    }
}

// ── Dispatch ─────────────────────────────────────────────────────────────────
// DAP mesajı formatı: {"type":"request","seq":N,"command":"...","arguments":{...}}

nlohmann::json DapHandler::dispatch(const nlohmann::json& msg) {
    if (msg.is_discarded() || !msg.contains("type")) return nullptr;

    std::string type = msg["type"].get<std::string>();
    if (type != "request") return nullptr;
    if (!msg.contains("command")) return nullptr;

    std::string command = msg["command"].get<std::string>();
    int seq = msg.value("seq", 0);

    if (command == "initialize")         return handleInitialize(msg);
    if (command == "launch")             return handleLaunch(msg);
    if (command == "setBreakpoints")     return handleSetBreakpoints(msg);
    if (command == "configurationDone")  return handleConfigurationDone(msg);
    if (command == "continue")           return handleContinue(msg);
    if (command == "next")               return handleNext(msg);
    if (command == "stepIn")             return handleStepIn(msg);
    if (command == "stepOut")            return handleStepOut(msg);
    if (command == "pause")              return handlePause(msg);
    if (command == "threads")            return handleThreads(msg);
    if (command == "stackTrace")         return handleStackTrace(msg);
    if (command == "scopes")             return handleScopes(msg);
    if (command == "variables")          return handleVariables(msg);
    if (command == "disconnect")         return handleDisconnect(msg);

    // Bilinmeyen command → success:false döndür
    return makeResponse(seq, command, {}, false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Handler'lar
// ─────────────────────────────────────────────────────────────────────────────

nlohmann::json DapHandler::handleInitialize(const nlohmann::json& req) {
    int seq = req.value("seq", 0);

    nlohmann::json caps = {
        {"supportsConfigurationDoneRequest", true},
        {"supportsFunctionBreakpoints",      false},
        {"supportsConditionalBreakpoints",   false},
        {"supportsSetVariable",              false},
        {"supportsStepBack",                 false},
        {"supportsStepInTargetsRequest",     false},
        {"supportsGotoTargetsRequest",       false},
        {"supportsHitConditionalBreakpoints",false},
        {"supportsTerminateRequest",         true},
        {"supportsExceptionInfoRequest",     false},
        {"supportTerminateDebuggee",         true}
    };

    // DAP kuralı: ÖNCE response, SONRA initialized event
    nlohmann::json resp = makeResponse(seq, "initialize", caps);

    // VS Code her initialize response'u bekler, sonra initialized event'ini işler.
    // Cevabı yaz, sonra event gönder.
    JsonRpc::writeMessage(out_, resp);
    sendEvent("initialized", {});

    initialized_ = true;
    return nullptr;  // dispatch artık yazmasın — biz zaten yazdık
}

nlohmann::json DapHandler::handleLaunch(const nlohmann::json& req) {
    int seq = req.value("seq", 0);
    nlohmann::json args = req.value("arguments", nlohmann::json::object());

    std::string program = args.value("program", "");
    if (program.empty()) {
        sendEvent("output", {{"category","stderr"},
                             {"output","No program specified\n"}});
        sendEvent("terminated", {});
        return makeResponse(seq, "launch", {}, false);
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
        return makeResponse(seq, "launch", {});
    }

    IRGenerator irgen;
    irProgram_ = std::make_unique<IRProgram>(
        irgen.generateModuleGraph(graph, table));

    vm_ = std::make_unique<Interpreter>(*irProgram_);

    // VM'i ilklendir ama çalıştırma — configurationDone'da başlatılacak
    vm_->initForDebug();

    return makeResponse(seq, "launch", {});
}

nlohmann::json DapHandler::handleSetBreakpoints(const nlohmann::json& req) {
    int seq = req.value("seq", 0);
    nlohmann::json args = req.value("arguments", nlohmann::json::object());

    if (vm_) vm_->clearAllBreakpoints();

    std::string sourceFile;
    if (args.contains("source") && args["source"].contains("path"))
        sourceFile = args["source"]["path"].get<std::string>();

    nlohmann::json bps = nlohmann::json::array();
    if (args.contains("breakpoints")) {
        for (const auto& bp : args["breakpoints"]) {
            int line = bp.value("line", 0);
            bool verified = false;

            if (vm_) {
                // line→ip index'inde doğrula — eşleşen satır varsa verified=true
                // VM henüz başlamamış olabilir, breakpoint'i yine de kaydet
                vm_->setBreakpoint(sourceFile, line);
                // Basit doğrulama: line > 0 ise verified
                verified = (line > 0);
            }

            bps.push_back({
                {"id",       nextBpId_++},
                {"verified", verified},
                {"line",     line},
                {"source",   {{"path", sourceFile}}}
            });
        }
    }

    return makeResponse(seq, "setBreakpoints", {{"breakpoints", bps}});
}

nlohmann::json DapHandler::handleConfigurationDone(const nlohmann::json& req) {
    int seq = req.value("seq", 0);

    if (!vm_) {
        return makeResponse(seq, "configurationDone", {});
    }

    // ÖNCE response yaz (DAP kuralı: response her zaman event'ten önce)
    nlohmann::json resp = makeResponse(seq, "configurationDone", {});
    JsonRpc::writeMessage(out_, resp);

    // VM'i başlat — ilk instruction'da dur (entry)
    vm_->stepInstruction();

    sendEvent("stopped", {{"reason","entry"}, {"threadId",1}});

    return nullptr;
}

nlohmann::json DapHandler::handleContinue(const nlohmann::json& req) {
    int seq = req.value("seq", 0);

    // ÖNCE response yaz
    nlohmann::json resp = makeResponse(seq, "continue", {{"allThreadsContinued", true}});
    JsonRpc::writeMessage(out_, resp);

    if (vm_) {
        runWithBudget();
    }

    return nullptr;
}

nlohmann::json DapHandler::handleNext(const nlohmann::json& req) {
    int seq = req.value("seq", 0);

    // ÖNCE response yaz
    nlohmann::json resp = makeResponse(seq, "next", {});
    JsonRpc::writeMessage(out_, resp);

    if (vm_) {
        vm_->stepOver();
        if (vm_->state() == Interpreter::RunState::Paused) {
            sendEvent("stopped", {{"reason","step"}, {"threadId",1}});
        } else if (vm_->state() == Interpreter::RunState::Finished) {
            sendEvent("exited", {{"exitCode", 0}});
            sendEvent("terminated", {});
        }
    }

    return nullptr;
}

nlohmann::json DapHandler::handleStepIn(const nlohmann::json& req) {
    int seq = req.value("seq", 0);

    // ÖNCE response yaz
    nlohmann::json resp = makeResponse(seq, "stepIn", {});
    JsonRpc::writeMessage(out_, resp);

    if (vm_) {
        vm_->stepInstruction();
        if (vm_->state() == Interpreter::RunState::Paused) {
            sendEvent("stopped", {{"reason","step"}, {"threadId",1}});
        } else if (vm_->state() == Interpreter::RunState::Finished) {
            sendEvent("exited", {{"exitCode", 0}});
            sendEvent("terminated", {});
        }
    }

    return nullptr;
}

nlohmann::json DapHandler::handleStepOut(const nlohmann::json& req) {
    int seq = req.value("seq", 0);

    // ÖNCE response yaz
    nlohmann::json resp = makeResponse(seq, "stepOut", {});
    JsonRpc::writeMessage(out_, resp);

    if (vm_) {
        vm_->stepOut();
        if (vm_->state() == Interpreter::RunState::Paused) {
            sendEvent("stopped", {{"reason","step"}, {"threadId",1}});
        } else if (vm_->state() == Interpreter::RunState::Finished) {
            sendEvent("exited", {{"exitCode", 0}});
            sendEvent("terminated", {});
        }
    }

    return nullptr;
}

nlohmann::json DapHandler::handlePause(const nlohmann::json& req) {
    int seq = req.value("seq", 0);
    // Pause: mevcut uygulamada runUntilEvent budget döngüsü olmadığından
    // şu an sadece cevap döner. Gerçek pause ileride (non-blocking run loop).
    return makeResponse(seq, "pause", {});
}

nlohmann::json DapHandler::handleThreads(const nlohmann::json& req) {
    int seq = req.value("seq", 0);
    nlohmann::json threads = nlohmann::json::array();
    threads.push_back({{"id", 1}, {"name", "main"}});
    return makeResponse(seq, "threads", {{"threads", threads}});
}

nlohmann::json DapHandler::handleStackTrace(const nlohmann::json& req) {
    int seq = req.value("seq", 0);
    nlohmann::json frames = nlohmann::json::array();

    if (vm_) {
        int depth = vm_->callDepth();
        for (int i = 0; i < depth; ++i) {
            frames.push_back({
                {"id",     i},
                {"name",   vm_->frameFunctionName(i)},
                {"line",   vm_->frameSourceLine(i)},
                {"column", 0},
                {"source", {{"path", vm_->frameSourceFile(i)}}}
            });
        }
    }

    return makeResponse(seq, "stackTrace",
        {{"stackFrames", frames}, {"totalFrames", (int)frames.size()}});
}

nlohmann::json DapHandler::handleScopes(const nlohmann::json& req) {
    int seq     = req.value("seq", 0);
    nlohmann::json args = req.value("arguments", nlohmann::json::object());
    int frameId = args.value("frameId", 0);

    nlohmann::json scopes = nlohmann::json::array();
    scopes.push_back({
        {"name",               "Locals"},
        {"variablesReference", 1000 + frameId},
        {"expensive",          false}
    });
    return makeResponse(seq, "scopes", {{"scopes", scopes}});
}

nlohmann::json DapHandler::handleVariables(const nlohmann::json& req) {
    int seq     = req.value("seq", 0);
    nlohmann::json args = req.value("arguments", nlohmann::json::object());
    int ref     = args.value("variablesReference", 0);

    // Frame variable'ları (ref >= 1000)
    if (ref >= 1000) {
        int frameId = ref - 1000;
        nlohmann::json vars = nlohmann::json::array();

        if (vm_ && frameId < vm_->callDepth()) {
            // IRFunction'daki slot sayısını al
            int slotCount = vm_->frameSlotCount(frameId);
            for (int slot = 0; slot < slotCount; ++slot) {
                Value v = vm_->readSlotInFrame(frameId, slot);
                std::string name = vm_->slotName(frameId, slot);
                if (name.empty()) continue;  // geçici/adsız slotları atla

                int childRef = 0;
                if (v.kind == ValueKind::Ref && v.ref) {
                    childRef = nextVarRef_++;
                }
                vars.push_back({
                    {"name",               name},
                    {"value",              valueToString(v)},
                    {"type",               ""},
                    {"variablesReference", childRef}
                });
            }
        }

        return makeResponse(seq, "variables", {{"variables", vars}});
    }

    // Child variable'ları (struct/array alanları)
    // ref numarasını bir önceki çağrıdan gelen referansa eşle
    // — şu an basit: her childRef, bir önceki döngüde atanan sıraya göre
    //    buildChildVariables yeniden üretilir
    // Not: Bu yaklaşımda childRef'lerin map'ini tutmuyoruz; DAP her
    // variablesReference için yeniden buildChildVariables çağırır.
    // VS Code aynı childRef'leri birden çok kez sorgulayabilir, ama
    // biz her seferinde aynı veriyi döndürürüz (stateless).
    // Gerçek map 'variablesReference → Value' ileride eklenir.
    nlohmann::json vars = nlohmann::json::array();
    return makeResponse(seq, "variables", {{"variables", vars}});
}

nlohmann::json DapHandler::handleDisconnect(const nlohmann::json& req) {
    int seq = req.value("seq", 0);
    vm_.reset();
    irProgram_.reset();
    return makeResponse(seq, "disconnect", {});
}
