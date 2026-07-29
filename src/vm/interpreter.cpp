// ============================================================================
// saQut VM — Interpreter (Bytecode Yorumlayıcı) Gerçeklemesi
// ============================================================================
//
// DİZİN:   src/vm/interpreter.cpp
// KATMAN:  VM — IRProgram içindeki instruction'ları yorumlar
//
// AMAÇ:
//   Tüm Opcode'ların işlenmesi (~1303 satır). DAP breakpoint/adım API'leri,
//   built-in metod dispatch, hata yönetimi (TRY/THROW), GC tetikleme.
//
// ============================================================================

#include "vm/interpreter.hpp"
#include "vm/object.hpp"
#include "builtin/builtin_methods.hpp"
#include "bench/profile.hpp"
#include "ffi/host_functions.hpp"
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <climits>
#include <cstdint>

// int32 aritmetiği: taşma TANIMLI 2's-complement wrap (#113/ADR-040). saQut `int`
// 32-bit; iki backend (VM/JIT) birebir aynı sonucu vermek zorunda (ADR-032/038).
// C++ signed overflow UB olduğundan toplama/çıkarma/çarpma uint32 üzerinden yapılır;
// INT_MIN/-1 bölme/mod donanımda tuzak (x86 #DE) → elle 2's-complement sonucu verilir.
namespace {
inline int wrapAddI32(int a, int b) { return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b)); }
inline int wrapSubI32(int a, int b) { return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b)); }
inline int wrapMulI32(int a, int b) { return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b)); }
inline int wrapDivI32(int a, int b) { return (a == INT_MIN && b == -1) ? INT_MIN : a / b; }
inline int wrapModI32(int a, int b) { return (a == INT_MIN && b == -1) ? 0 : a % b; }
// Kaydırma miktarı 5-bit maskelenir (x86/MIR native davranışı; b&31), sonuç 32-bit.
inline int wrapShlI32(int a, int b) { return static_cast<int32_t>(static_cast<uint32_t>(a) << (b & 31)); }
inline int wrapShrI32(int a, int b) { return a >> (b & 31); }

// longint (64-bit) aritmetiği: aynı gerekçeyle tanımlı 2's-complement wrap
// (ADR-040) — MIR backend'in native 64-bit MIR_ADD/SUB/MUL/LSH/RSH'siyle
// birebir (mir_backend.cpp). INT64_MIN/-1 x86'da yine tuzak, elle ele alınır.
inline long long wrapAddI64(long long a, long long b) { return static_cast<int64_t>(static_cast<uint64_t>(a) + static_cast<uint64_t>(b)); }
inline long long wrapSubI64(long long a, long long b) { return static_cast<int64_t>(static_cast<uint64_t>(a) - static_cast<uint64_t>(b)); }
inline long long wrapMulI64(long long a, long long b) { return static_cast<int64_t>(static_cast<uint64_t>(a) * static_cast<uint64_t>(b)); }
inline long long wrapDivI64(long long a, long long b) { return (a == INT64_MIN && b == -1) ? INT64_MIN : a / b; }
inline long long wrapModI64(long long a, long long b) { return (a == INT64_MIN && b == -1) ? 0 : a % b; }
inline long long wrapShlI64(long long a, long long b) { return static_cast<int64_t>(static_cast<uint64_t>(a) << (b & 63)); }
inline long long wrapShrI64(long long a, long long b) { return a >> (b & 63); }
inline long long wrapNegI64(long long a) { return static_cast<int64_t>(0ULL - static_cast<uint64_t>(a)); }

bool dispatchStringBuiltinNoCopy(int runtimeId,
                                 const std::vector<const Value*>& args,
                                 Heap& heap,
                                 Value& out) {
    switch (runtimeId) {
    case 11: {
        if (args[0]->kind != ValueKind::String)
            throw std::runtime_error("string::length — expected string");
        out = Value::fromInt((int)args[0]->stringValue.size());
        return true;
    }
    case 12: {
        if (args[0]->kind != ValueKind::String)
            throw std::runtime_error("string::upper — expected string");
        std::string s = args[0]->stringValue;
        for (char& c : s) c = (char)std::toupper((unsigned char)c);
        out = Value::fromString(std::move(s));
        return true;
    }
    case 13: {
        if (args[0]->kind != ValueKind::String)
            throw std::runtime_error("string::lower — expected string");
        std::string s = args[0]->stringValue;
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        out = Value::fromString(std::move(s));
        return true;
    }
    case 14: {
        if (args[0]->kind != ValueKind::String)
            throw std::runtime_error("string::trim — expected string");
        const std::string& src = args[0]->stringValue;
        size_t start = src.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) out = Value::fromString("");
        else {
            size_t end = src.find_last_not_of(" \t\n\r");
            out = Value::fromString(src.substr(start, end - start + 1));
        }
        return true;
    }
    case 15: {
        if (args[0]->kind != ValueKind::String || args[1]->kind != ValueKind::String)
            throw std::runtime_error("string::split — expected string, string");
        const std::string& src = args[0]->stringValue;
        const std::string& sep = args[1]->stringValue;
        auto* arr = heap.allocArray();
        if (sep.empty()) {
            for (char c : src)
                arr->elements.push_back(Value::fromString(std::string(1, c)));
        } else {
            size_t pos = 0, found;
            while ((found = src.find(sep, pos)) != std::string::npos) {
                arr->elements.push_back(Value::fromString(src.substr(pos, found - pos)));
                pos = found + sep.size();
            }
            arr->elements.push_back(Value::fromString(src.substr(pos)));
        }
        out = Value::fromRef(arr);
        return true;
    }
    case 16: {
        if (args[0]->kind != ValueKind::String)
            throw std::runtime_error("string::substring — expected string");
        const std::string& s = args[0]->stringValue;
        int from = args[1]->intValue;
        int len  = args[2]->intValue;
        if (from < 0 || from > (int)s.size())
            throw std::runtime_error("string::substring — index out of bounds");
        if (len < 0) len = 0;
        out = Value::fromString(s.substr(from, len));
        return true;
    }
    case 17: {
        if (args[0]->kind != ValueKind::String || args[1]->kind != ValueKind::String || args[2]->kind != ValueKind::String)
            throw std::runtime_error("string::replace — expected string, string, string");
        std::string s = args[0]->stringValue;
        const std::string& from = args[1]->stringValue;
        const std::string& to = args[2]->stringValue;
        if (!from.empty()) {
            size_t pos = 0;
            while ((pos = s.find(from, pos)) != std::string::npos) {
                s.replace(pos, from.size(), to);
                pos += to.size();
            }
        }
        out = Value::fromString(std::move(s));
        return true;
    }
    case 18: {
        if (args[0]->kind != ValueKind::String)
            throw std::runtime_error("string::repeat — expected string");
        int n = args[1]->intValue;
        if (n < 0) n = 0;
        std::string result;
        result.reserve(args[0]->stringValue.size() * (size_t)n);
        for (int i = 0; i < n; ++i) result += args[0]->stringValue;
        out = Value::fromString(std::move(result));
        return true;
    }
    case 19: {
        if (args[0]->kind != ValueKind::String)
            throw std::runtime_error("string::charAt — expected string");
        const std::string& s = args[0]->stringValue;
        int idx = args[1]->intValue;
        if (idx < 0 || idx >= (int)s.size())
            throw std::runtime_error("string::charAt — index out of bounds");
        out = Value::fromString(std::string(1, s[idx]));
        return true;
    }
    case 20: {
        if (args[0]->kind != ValueKind::String || args[1]->kind != ValueKind::String)
            throw std::runtime_error("string::indexOf — expected string, string");
        size_t pos = args[0]->stringValue.find(args[1]->stringValue);
        out = (pos == std::string::npos) ? Value::null() : Value::fromInt((int)pos);
        return true;
    }
    case 21: {
        if (args[0]->kind != ValueKind::String || args[1]->kind != ValueKind::String)
            throw std::runtime_error("string::contains — expected string, string");
        bool found = args[0]->stringValue.find(args[1]->stringValue) != std::string::npos;
        out = Value::fromInt(found ? 1 : 0);
        return true;
    }
    case 22: {
        if (args[0]->kind != ValueKind::String || args[1]->kind != ValueKind::String)
            throw std::runtime_error("string::startsWith — expected string, string");
        const std::string& s = args[0]->stringValue;
        const std::string& p = args[1]->stringValue;
        bool ok = s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
        out = Value::fromInt(ok ? 1 : 0);
        return true;
    }
    case 23: {
        if (args[0]->kind != ValueKind::String || args[1]->kind != ValueKind::String)
            throw std::runtime_error("string::endsWith — expected string, string");
        const std::string& s = args[0]->stringValue;
        const std::string& p = args[1]->stringValue;
        bool ok = s.size() >= p.size() &&
                  s.compare(s.size() - p.size(), p.size(), p) == 0;
        out = Value::fromInt(ok ? 1 : 0);
        return true;
    }
    default:
        return false;
    }
}
} // namespace

// ── buildTrace ─────────────────────────────────────────────────────────────────
// Mevcut callStack_'i en içten dışa gezerek stacktrace string'i üretir.
// pendingThrow_ set edilmeden (unwind olmadan) önce çağrılmalıdır.
std::string Interpreter::buildTrace() const {
    std::string result;
    for (int i = (int)callStack_.size() - 1; i >= 0; --i) {
        const CallFrame& f = callStack_[i];
        if (!f.function) continue;
        // ip zaten artırılmış olduğundan şu an çalışan instruction = ip - 1
        int ip = f.instructionPointer - 1;
        if (ip >= 0 && ip < (int)f.function->instructions.size()) {
            const Instruction& ins = f.function->instructions[ip];
            if (ins.sourceLine > 0) {
                const std::string& file =
                    program_.moduleRegistry.filePath(f.function->moduleId);
                result += f.function->name + " (" + file + ":" +
                          std::to_string(ins.sourceLine) + ":" +
                          std::to_string(ins.sourceCol)  + ")\n";
            } else {
                result += f.function->name + " (?)\n";
            }
        } else {
            result += f.function->name + " (?)\n";
        }
    }
    return result;
}

// ── makeErrorValue ─────────────────────────────────────────────────────────────
// ADR-025: Error struct oluşturur — alan sırası: [line, col, message, trace, code]
Value Interpreter::makeErrorValue(const std::string& message,
                                   const std::string& code,
                                   int line, int col) {
    StructObject* obj = heap_.allocStruct(5);
    obj->fields[0] = Value::fromInt(line);
    obj->fields[1] = Value::fromInt(col);
    obj->fields[2] = Value::fromString(message);
    obj->fields[3] = Value::fromString(buildTrace());
    obj->fields[4] = Value::fromString(code);
    return Value::fromRef(obj);
}

// ── DAP API implementasyonu ────────────────────────────────────────────────────

void Interpreter::setBreakpoint(const std::string& file, int line) {
    breakpoints_.insert({file, line});
}

void Interpreter::clearBreakpoint(const std::string& file, int line) {
    breakpoints_.erase({file, line});
}

void Interpreter::clearAllBreakpoints() {
    breakpoints_.clear();
}

// Faz 7 (#105): (dosya, satır) çalıştırılabilir mi? — setBreakpoints.verified.
// Yorum/boş satıra konan breakpoint'e verified:false dönmek için Faz 5'in
// lineToFirstIP indeksinde arar; dosya eşleşmesi ModuleRegistry üzerinden.
bool Interpreter::isExecutableLine(const std::string& file, int line) const {
    for (const auto& [name, fn] : program_.functions) {
        if (fn.lineToFirstIP.count(line) == 0) continue;
        if (program_.moduleRegistry.filePath(fn.moduleId) == file) return true;
    }
    return false;
}

bool Interpreter::isBreakpoint() const {
    if (callStack_.empty()) return false;
    const CallFrame& frame = callStack_.back();
    if (!frame.function) return false;
    int ip = frame.instructionPointer;
    if (ip < 0 || ip >= (int)frame.function->instructions.size()) return false;
    const Instruction& ins = frame.function->instructions[ip];
    if (ins.sourceLine <= 0) return false;
    const std::string& file = ins.sourceFile.empty()
        ? program_.moduleRegistry.filePath(frame.function->moduleId)
        : ins.sourceFile;
    return breakpoints_.count({file, ins.sourceLine}) > 0;
}

void Interpreter::checkBreakpoint() {
    if (state_ == RunState::Paused) return;
    if (isBreakpoint())
        state_ = RunState::Paused;
}

void Interpreter::resume() {
    // Faz 5: run() değil, mevcut durumdan devam (bütçe -1 = sınırsız)
    runUntilEvent(-1, -1);
}

void Interpreter::stepInstruction() {
    if (callStack_.empty()) { state_ = RunState::Finished; return; }
    // Faz 5: tek talimat çalıştır
    runUntilEvent(1, -1);
}

void Interpreter::stepOver() {
    if (callStack_.empty()) { state_ = RunState::Finished; return; }
    // Faz 5: aynı çağrı derinliğinde satır değişene kadar ilerle
    int depth = (int)callStack_.size();
    runUntilEvent(-1, depth);
}

// Faz 5: sourceLine değişene kadar ilerle
void Interpreter::stepLine() {
    if (callStack_.empty()) { state_ = RunState::Finished; return; }
    int depth = (int)callStack_.size();
    runUntilEvent(-1, depth); // stepOver ile aynı mantık
}

// Faz 5: callDepth azalana kadar ilerle
void Interpreter::stepOut() {
    if (callStack_.empty()) { state_ = RunState::Finished; return; }
    int depth = (int)callStack_.size() - 1; // şu anki fonksiyondan çıkış
    runUntilEvent(-1, depth);
}

int Interpreter::currentSourceLine() const {
    if (callStack_.empty()) return 0;
    const CallFrame& f = callStack_.back();
    int ip = f.instructionPointer;
    // Faz 9 (#105): duraklama semantiği — SIRADAKİ (henüz çalışmamış)
    // instruction'ın satırı raporlanır; adım kontrolü fetch ÖNCESİNE
    // alındığından durulan nokta ip'nin kendisidir. Fonksiyon sonundaysa
    // son çalışan instruction'ın satırına düşülür.
    if (f.function && ip >= 0 && ip < (int)f.function->instructions.size())
        return f.function->instructions[ip].sourceLine;
    if (f.function && ip > 0 && ip - 1 < (int)f.function->instructions.size())
        return f.function->instructions[ip - 1].sourceLine;
    return 0;
}

std::string Interpreter::currentSourceFile() const {
    if (callStack_.empty()) return "";
    const CallFrame& f = callStack_.back();
    int ip = f.instructionPointer;
    if (f.function && ip > 0 && ip - 1 < (int)f.function->instructions.size()) {
        const Instruction& ins = f.function->instructions[ip - 1];
        return ins.sourceFile.empty()
            ? program_.moduleRegistry.filePath(f.function->moduleId)
            : ins.sourceFile;
    }
    return "";
}

int Interpreter::callDepth() const {
    return (int)callStack_.size();
}

std::string Interpreter::frameSourceFile(int depth) const {
    if (depth < 0 || depth >= (int)callStack_.size()) return "";
    const CallFrame& f = callStack_[(int)callStack_.size() - 1 - depth];
    int ip = f.instructionPointer - 1;
    if (f.function && ip >= 0 && ip < (int)f.function->instructions.size()) {
        const Instruction& ins = f.function->instructions[ip];
        return ins.sourceFile.empty()
            ? program_.moduleRegistry.filePath(f.function->moduleId)
            : ins.sourceFile;
    }
    return program_.moduleRegistry.filePath(f.function ? f.function->moduleId : 0);
}

int Interpreter::frameSlotCount(int depth) const {
    if (depth < 0 || depth >= (int)callStack_.size()) return 0;
    const CallFrame& f = callStack_[(int)callStack_.size() - 1 - depth];
    return f.function ? f.function->slotCount : 0;
}

std::string Interpreter::frameFunctionName(int depth) const {
    if (depth < 0 || depth >= (int)callStack_.size()) return "";
    const CallFrame& f = callStack_[(int)callStack_.size() - 1 - depth];
    return f.function ? f.function->name : "";
}

int Interpreter::frameSourceLine(int depth) const {
    if (depth < 0 || depth >= (int)callStack_.size()) return 0;
    const CallFrame& f = callStack_[(int)callStack_.size() - 1 - depth];
    // Faz 9 (#105): aktif frame'de (depth 0) durulan nokta = SIRADAKİ
    // instruction; üst frame'lerde ip dönüş adresidir — çağrıyı yapan
    // satır (ip-1'deki CALL) raporlanır.
    int ip = (depth == 0) ? f.instructionPointer : f.instructionPointer - 1;
    if (f.function && ip >= 0 && ip < (int)f.function->instructions.size())
        return f.function->instructions[ip].sourceLine;
    if (f.function && ip - 1 >= 0 && ip - 1 < (int)f.function->instructions.size())
        return f.function->instructions[ip - 1].sourceLine;
    return 0;
}

Value Interpreter::readSlotInFrame(int frameDepth, int slotIndex) const {
    if (frameDepth < 0 || frameDepth >= (int)callStack_.size())
        return Value::fromInt(0);
    const CallFrame& f = callStack_[(int)callStack_.size() - 1 - frameDepth];
    if (slotIndex < 0 || slotIndex >= (int)f.slots.size())
        return Value::fromInt(0);
    return f.slots[slotIndex];
}

// Faz 5: IRFunction::slotNames kullanarak gerçek değişken adını döndürür.
std::string Interpreter::slotName(int frameDepth, int slotIndex) const {
    if (frameDepth < 0 || frameDepth >= (int)callStack_.size()) return "";
    const CallFrame& f = callStack_[(int)callStack_.size() - 1 - frameDepth];
    if (!f.function) return "";
    if (slotIndex < 0 || slotIndex >= (int)f.function->slotNames.size()) return "";
    return f.function->slotNames[slotIndex];
}

// Faz 5: bütçe/step kısıtı kontrolü — döngü başında çağrılır.
bool Interpreter::shouldStop() {
    if (state_ == RunState::Paused) return true;
    if (runBudget_ <= 0 && runBudget_ != 0) return true; // bütçe tükendi (0 = sınırsız değil)
    // stepOver/stepLine: satır değişti mi?
    if (stepStartDepth_ >= 0 && stepStartLine_ > 0) {
        int curLine = currentSourceLine();
        int curDepth = (int)callStack_.size();
        if (curLine > 0 && curLine != stepStartLine_ && curDepth <= stepStartDepth_) {
            state_ = RunState::Paused;
            return true;
        }
    }
    // stepOut: derinlik azaldı mı?
    if (stepStartDepth_ >= 0 && stepStartLine_ == 0) {
        if ((int)callStack_.size() <= stepStartDepth_) {
            state_ = RunState::Paused;
            return true;
        }
    }
    return false;
}

// Faz 5: mevcut durumdan bütçeli/step'li devam et.
// ─────────────────────────────────────────────────────────────────────────────
// maybeCollect — eşik tabanlı mark-sweep tetikleme (#77, ADR-022)
// ─────────────────────────────────────────────────────────────────────────────
//
// Kökler: globalSlots_ (modül-düzeyi değişkenler), callStack_ (her frame'in
// slot'ları) ve pendingThrow_ (unwind sırasındaki Error nesnesi). Eşik
// adaptif: toplama sonrası canlı kümenin 2 katına çıkar (küçülünce başlangıç
// eşiğine iner) — canlı nesnesi çok programda her instruction'da sweep
// koşulmasını önler, tetikleme sayısı deterministik kalır.

void Interpreter::maybeCollect() {
    if (gcThreshold_ <= 0 || heap_.allocCount < gcThreshold_) return;

    heap_.markSlots(globalSlots_);
    for (const CallFrame& frame : callStack_)
        heap_.markSlots(frame.slots);
    if (pendingThrow_)
        heap_.markValue(*pendingThrow_);

    heap_.sweep();
    gcThreshold_ = std::max(gcInitialThreshold_, heap_.allocCount * 2);
}

Interpreter::RunReason Interpreter::runUntilEvent(int maxInstructions,
                                                    int startCallDepth) {
    if (callStack_.empty() || !vmInitialized_) {
        state_ = RunState::Finished;
        return RunReason::Finished;
    }
    state_          = RunState::Running;
    runBudget_      = maxInstructions;
    stepStartDepth_ = startCallDepth;
    stepStartLine_  = (startCallDepth >= 0) ? currentSourceLine() : 0;

    // Faz 7 (#105): breakpoint ÜSTÜNDE dururken devam edilirse aynı satıra
    // yeniden takılma — bir kaynak satırı birden çok instruction ürettiğinden
    // "üzerinde durduğumuz satır"dan çıkana kadar bp kontrolü atlanır.
    // TODO(faz8): satır içi çağrıdan aynı satıra dönüşte bp yeniden vurur
    // (GDB "her varışta bir kez" semantiği için hit-noktası takibi gerekir).
    int         resumeSkipLine = 0;
    std::string resumeSkipFile;
    if (isBreakpoint() && !callStack_.empty()) {
        const CallFrame& f = callStack_.back();
        const Instruction& ins = f.function->instructions[f.instructionPointer];
        resumeSkipLine = ins.sourceLine;
        resumeSkipFile = ins.sourceFile.empty()
            ? program_.moduleRegistry.filePath(f.function->moduleId)
            : ins.sourceFile;
    }

    // run() ile aynı döngü — ortak kod yolu
    // src/profiling/ (--profile): "vm-exec" TAM OLARAK bu döngünün süresi —
    // VM'in gerçekten instruction çalıştırdığı kısım (kapanış: while'ın
    // kendi kapanış parantezinden hemen sonra).
    { profiling::StageTimer::ScopedStage _profExec(stageProfiler_, "vm-exec");
    while (!callStack_.empty()) {
        // Bütçe kontrolü: < 0 = sınırsız, == 0 = tükendi, > 0 = kalan hak
        // runUntilEvent(-1, ...) → sınırsız
        // runUntilEvent(1, ...)  → 1 instruction, sonra dur
        if (runBudget_ < 0) {
            // Sınırsız bütçe — devam
        } else if (runBudget_ == 0) {
            state_ = RunState::Paused;
            return RunReason::BudgetExhausted;
        }

        // Breakpoint kontrolü (resume satırı atlanır, bkz. yukarı)
        if (isBreakpoint()) {
            bool onResumeLine = false;
            if (resumeSkipLine > 0) {
                int curLine = 0;
                std::string curFile;
                const CallFrame& f = callStack_.back();
                if (f.function &&
                    f.instructionPointer < (int)f.function->instructions.size()) {
                    const Instruction& ins =
                        f.function->instructions[f.instructionPointer];
                    curLine = ins.sourceLine;
                    curFile = ins.sourceFile.empty()
                        ? program_.moduleRegistry.filePath(f.function->moduleId)
                        : ins.sourceFile;
                }
                onResumeLine = (curLine == resumeSkipLine &&
                                curFile == resumeSkipFile);
            }
            if (!onResumeLine) {
                state_ = RunState::Paused;
                return RunReason::Breakpoint;
            }
        } else {
            // Resume satırından çıkıldı — bundan sonra normal kontrol
            resumeSkipLine = 0;
        }

        // GC safepoint (#77): instruction sınırı — tüm canlı nesneler bu
        // noktada bir slot'a (frame/modül) ya da pendingThrow_'a bağlıdır.
        maybeCollect();

        CallFrame& frame = callStack_.back();

        if (frame.instructionPointer >= (int)frame.function->instructions.size()) {
            int destSlot = frame.returnDestSlot;
            callStack_.pop_back();
            if (!callStack_.empty() && destSlot != -1)
                callStack_.back().slots[destSlot] = Value::fromInt(0);
            // stepOut: derinlik azaldı → tamam
            if (stepStartDepth_ >= 0 && stepStartLine_ == 0 &&
                (int)callStack_.size() <= stepStartDepth_) {
                state_ = RunState::Paused;
                return RunReason::StepDone;
            }
            if (callStack_.empty()) {
                state_ = RunState::Finished;
                return RunReason::Finished;
            }
            continue;
        }

        // Adım kontrolü (stepOver/stepLine): SIRADAKİ instruction yeni bir
        // satıra aitse o instruction ÇALIŞMADAN dur. Faz 9 (#105) düzeltmesi:
        // eski konum (fetch SONRASI) satır sınırındaki ilk instruction'ı
        // yutuyordu — print gibi tek-instruction'lık satırlar adımlamada
        // hiç çalışmıyordu.
        if (stepStartLine_ > 0 && stepStartDepth_ >= 0) {
            int nextLine = frame.function->instructions[frame.instructionPointer].sourceLine;
            int curDepth = (int)callStack_.size();
            if (nextLine > 0 && nextLine != stepStartLine_ && curDepth <= stepStartDepth_) {
                state_ = RunState::Paused;
                return RunReason::StepDone;
            }
        }

        const Instruction& instr = frame.function->instructions[frame.instructionPointer];
        frame.instructionPointer++;
        if (runBudget_ > 0) runBudget_--;

        // Profil hook
        if (vmTrace_) [[unlikely]]
            vmTrace_->pushDispatch(static_cast<uint8_t>(instr.opcode));

        switch (instr.opcode) {

        case Opcode::LOAD_CONST:
            frame.slots[instr.dest] = Value::fromInt(instr.intValue);
            break;

        case Opcode::LOAD_STRING:
            frame.slots[instr.dest] = Value::fromString(instr.stringValue);
            break;

        case Opcode::LOAD_NULL:
            frame.slots[instr.dest] = Value::null();
            break;

        case Opcode::LOAD_SLOT:
            frame.slots[instr.dest] = frame.slots[instr.src];
            break;

        // ── Aritmetik ─────────────────────────────────────────────────────
        // TypeChecker derleme zamanında tipleri doğruladı — burada sadece hesap yapılır.
        // İstisna: sıfıra bölme gerçek bir çalışma zamanı koşuludur, kontrol edilir.
        case Opcode::ADD:
            frame.slots[instr.dest] = Value::fromInt(
                wrapAddI32(frame.slots[instr.left].intValue, frame.slots[instr.right].intValue));
            break;
        case Opcode::SUB:
            frame.slots[instr.dest] = Value::fromInt(
                wrapSubI32(frame.slots[instr.left].intValue, frame.slots[instr.right].intValue));
            break;
        case Opcode::MUL:
            frame.slots[instr.dest] = Value::fromInt(
                wrapMulI32(frame.slots[instr.left].intValue, frame.slots[instr.right].intValue));
            break;
        case Opcode::DIV: {
            int d = frame.slots[instr.right].intValue;
            if (d == 0) { pendingThrow_ = makeErrorValue("division by zero", "E_DIVZERO", instr.sourceLine, instr.sourceCol); break; }
            frame.slots[instr.dest] = Value::fromInt(wrapDivI32(frame.slots[instr.left].intValue, d));
            break;
        }
        case Opcode::MOD: {
            int d = frame.slots[instr.right].intValue;
            if (d == 0) { pendingThrow_ = makeErrorValue("sıfıra bölme (mod)", "E_DIVZERO", instr.sourceLine, instr.sourceCol); break; }
            frame.slots[instr.dest] = Value::fromInt(wrapModI32(frame.slots[instr.left].intValue, d));
            break;
        }

        // ── Bitsel ────────────────────────────────────────────────────────
        case Opcode::BAND:
            frame.slots[instr.dest] = Value::fromInt(
                frame.slots[instr.left].intValue & frame.slots[instr.right].intValue);
            break;
        case Opcode::BOR:
            frame.slots[instr.dest] = Value::fromInt(
                frame.slots[instr.left].intValue | frame.slots[instr.right].intValue);
            break;
        case Opcode::BXOR:
            frame.slots[instr.dest] = Value::fromInt(
                frame.slots[instr.left].intValue ^ frame.slots[instr.right].intValue);
            break;
        case Opcode::SHL:
            frame.slots[instr.dest] = Value::fromInt(
                wrapShlI32(frame.slots[instr.left].intValue, frame.slots[instr.right].intValue));
            break;
        case Opcode::SHR:
            frame.slots[instr.dest] = Value::fromInt(
                wrapShrI32(frame.slots[instr.left].intValue, frame.slots[instr.right].intValue));
            break;
        case Opcode::BNOT:
            frame.slots[instr.dest] = Value::fromInt(~frame.slots[instr.src].intValue);
            break;

        // ── Global değişken erişimi ────────────────────────────────────────
        // #3: instr.intValue IRGenerator'ın program-çapında (modüller arası)
        // tek flat indeksi — yürüten fonksiyonun moduleId'siyle KARIŞTIRILMAZ,
        // aksi halde başka modülden import edilmiş bir global yanlış (ya da
        // sınır dışı) diziye erişirdi.
        case Opcode::LOAD_GLOBAL:
            frame.slots[instr.dest] = globalSlots_[instr.intValue];
            break;
        case Opcode::STORE_GLOBAL:
            globalSlots_[instr.intValue] = frame.slots[instr.src];
            break;

        // ── Karşılaştırma ─────────────────────────────────────────────────
        case Opcode::LESS: {
            auto& lv = frame.slots[instr.left]; auto& rv = frame.slots[instr.right];
            int r;
            if (lv.kind == ValueKind::Date && rv.kind == ValueKind::Date)
                r = (lv.int64Value < rv.int64Value ? 1 : 0);
            else if (lv.kind == ValueKind::Decimal || rv.kind == ValueKind::Decimal)
                r = DecimalValue::compare(lv.decimalValue, rv.decimalValue) < 0 ? 1 : 0;
            else if (lv.isFloaty() || rv.isFloaty())
                r = (lv.asDouble() < rv.asDouble() ? 1 : 0);
            else r = (lv.asI64() < rv.asI64() ? 1 : 0);
            frame.slots[instr.dest] = Value::fromInt(r);
            break;
        }
        case Opcode::LESS_EQUAL: {
            auto& lv = frame.slots[instr.left]; auto& rv = frame.slots[instr.right];
            int r;
            if (lv.kind == ValueKind::Date && rv.kind == ValueKind::Date)
                r = (lv.int64Value <= rv.int64Value ? 1 : 0);
            else if (lv.kind == ValueKind::Decimal || rv.kind == ValueKind::Decimal)
                r = DecimalValue::compare(lv.decimalValue, rv.decimalValue) <= 0 ? 1 : 0;
            else if (lv.isFloaty() || rv.isFloaty())
                r = (lv.asDouble() <= rv.asDouble() ? 1 : 0);
            else r = (lv.asI64() <= rv.asI64() ? 1 : 0);
            frame.slots[instr.dest] = Value::fromInt(r);
            break;
        }
        case Opcode::GREATER: {
            auto& lv = frame.slots[instr.left]; auto& rv = frame.slots[instr.right];
            int r;
            if (lv.kind == ValueKind::Date && rv.kind == ValueKind::Date)
                r = (lv.int64Value > rv.int64Value ? 1 : 0);
            else if (lv.kind == ValueKind::Decimal || rv.kind == ValueKind::Decimal)
                r = DecimalValue::compare(lv.decimalValue, rv.decimalValue) > 0 ? 1 : 0;
            else if (lv.isFloaty() || rv.isFloaty())
                r = (lv.asDouble() > rv.asDouble() ? 1 : 0);
            else r = (lv.asI64() > rv.asI64() ? 1 : 0);
            frame.slots[instr.dest] = Value::fromInt(r);
            break;
        }
        case Opcode::GREATER_EQUAL: {
            auto& lv = frame.slots[instr.left]; auto& rv = frame.slots[instr.right];
            int r;
            if (lv.kind == ValueKind::Date && rv.kind == ValueKind::Date)
                r = (lv.int64Value >= rv.int64Value ? 1 : 0);
            else if (lv.kind == ValueKind::Decimal || rv.kind == ValueKind::Decimal)
                r = DecimalValue::compare(lv.decimalValue, rv.decimalValue) >= 0 ? 1 : 0;
            else if (lv.isFloaty() || rv.isFloaty())
                r = (lv.asDouble() >= rv.asDouble() ? 1 : 0);
            else r = (lv.asI64() >= rv.asI64() ? 1 : 0);
            frame.slots[instr.dest] = Value::fromInt(r);
            break;
        }
        case Opcode::EQUAL_EQUAL: {
            auto& lv = frame.slots[instr.left]; auto& rv = frame.slots[instr.right];
            int r;
            // ADR-021/027: null kind ayrı işlenir — null yalnızca null'a eşittir
            if (lv.kind == ValueKind::Null && rv.kind == ValueKind::Null)
                r = 1;
            else if (lv.kind == ValueKind::Null || rv.kind == ValueKind::Null)
                r = 0;
            else if (lv.kind == ValueKind::Ref || rv.kind == ValueKind::Ref)
                r = (lv.ref == rv.ref ? 1 : 0); // ADR-023: array/struct kimlik
            else if (lv.kind == ValueKind::Date && rv.kind == ValueKind::Date)
                r = (lv.int64Value == rv.int64Value ? 1 : 0);
            else if (lv.kind == ValueKind::String)
                r = (lv.stringValue == rv.stringValue ? 1 : 0);
            else if (lv.kind == ValueKind::Decimal || rv.kind == ValueKind::Decimal)
                r = (lv.decimalValue == rv.decimalValue ? 1 : 0);
            else if (lv.isFloaty() || rv.isFloaty())
                r = (lv.asDouble() == rv.asDouble() ? 1 : 0);
            else
                r = (lv.asI64() == rv.asI64() ? 1 : 0);
            frame.slots[instr.dest] = Value::fromInt(r);
            break;
        }
        case Opcode::NOT_EQUAL: {
            auto& lv = frame.slots[instr.left]; auto& rv = frame.slots[instr.right];
            int r;
            if (lv.kind == ValueKind::Null && rv.kind == ValueKind::Null)
                r = 0;
            else if (lv.kind == ValueKind::Null || rv.kind == ValueKind::Null)
                r = 1;
            else if (lv.kind == ValueKind::Ref || rv.kind == ValueKind::Ref)
                r = (lv.ref != rv.ref ? 1 : 0);
            else if (lv.kind == ValueKind::Date && rv.kind == ValueKind::Date)
                r = (lv.int64Value != rv.int64Value ? 1 : 0);
            else if (lv.kind == ValueKind::String)
                r = (lv.stringValue != rv.stringValue ? 1 : 0);
            else if (lv.kind == ValueKind::Decimal || rv.kind == ValueKind::Decimal)
                r = (lv.decimalValue != rv.decimalValue ? 1 : 0);
            else if (lv.isFloaty() || rv.isFloaty())
                r = (lv.asDouble() != rv.asDouble() ? 1 : 0);
            else
                r = (lv.asI64() != rv.asI64() ? 1 : 0);
            frame.slots[instr.dest] = Value::fromInt(r);
            break;
        }

        // ── Kontrol akışı ─────────────────────────────────────────────────
        case Opcode::JMP:
            frame.instructionPointer = instr.jumpTarget;
            break;
        case Opcode::JIF_FALSE:
            if (!frame.slots[instr.cond].isTruthy())
                frame.instructionPointer = instr.jumpTarget;
            break;
        case Opcode::JIF_TRUE:
            if (frame.slots[instr.cond].isTruthy())
                frame.instructionPointer = instr.jumpTarget;
            break;

        // ── Fonksiyon çağrısı ─────────────────────────────────────────────
        case Opcode::CALL: {
            if (vmTrace_) [[unlikely]] ++vmTrace_->vmSaqutCalls;
            IRFunction* callee = program_.findFunction(instr.functionName);
            if (!callee)
                throw std::runtime_error(
                    "'" + instr.functionName + "' function not found");

            CallFrame newFrame;
            newFrame.function           = callee;
            newFrame.instructionPointer = 0;
            newFrame.slots.resize(callee->slotCount, Value::fromInt(0));
            newFrame.returnDestSlot     = instr.dest;

            for (int i = 0; i < (int)instr.argSlots.size(); i++)
                newFrame.slots[i] = frame.slots[instr.argSlots[i]];

            callStack_.push_back(std::move(newFrame));
            continue;
        }

        // ── Dönüş ─────────────────────────────────────────────────────────
        //
        // GC (#77): eski "her RETURN'de koşulsuz collect" kaldırıldı — hem
        // her dönüşte tüm modül slotlarını geçici vektöre kopyalıyordu hem de
        // döngü İÇİNDE tahsis yapan program hiç toplanmıyordu (#67 zayıflığı).
        // Toplama artık döngü başındaki eşik tabanlı maybeCollect() safepoint'i
        // (dönüş değeri o noktada caller slot'una yazılmış olur — kök kararlı).
        case Opcode::RETURN: {
            Value returnValue    = std::move(frame.slots[instr.src]);
            int   returnDestSlot = frame.returnDestSlot;
            callStack_.pop_back();

            if (!callStack_.empty() && returnDestSlot != -1)
                callStack_.back().slots[returnDestSlot] = std::move(returnValue);

            if (callStack_.empty()) {
                lastReturnValue_ = returnValue.intValue;
                state_ = RunState::Finished;
                return RunReason::Finished;
            }

            // stepOut: derinlik azaldı → tamam
            if (stepStartDepth_ >= 0 && stepStartLine_ == 0 &&
                (int)callStack_.size() <= stepStartDepth_) {
                state_ = RunState::Paused;
                return RunReason::StepDone;
            }
            continue;
        }

        // ── Float aritmetik (#44) ─────────────────────────────────────────
        case Opcode::LOAD_FLOAT:
            frame.slots[instr.dest] = Value::fromFloat(instr.floatValue);
            break;
        case Opcode::FADD:
            frame.slots[instr.dest] = Value::fromFloat(
                frame.slots[instr.left].floatValue + frame.slots[instr.right].floatValue);
            break;
        case Opcode::FSUB:
            frame.slots[instr.dest] = Value::fromFloat(
                frame.slots[instr.left].floatValue - frame.slots[instr.right].floatValue);
            break;
        case Opcode::FMUL:
            frame.slots[instr.dest] = Value::fromFloat(
                frame.slots[instr.left].floatValue * frame.slots[instr.right].floatValue);
            break;
        case Opcode::FDIV: {
            double r = frame.slots[instr.right].floatValue;
            if (r == 0.0) { pendingThrow_ = makeErrorValue("float division by zero", "E_DIVZERO", instr.sourceLine, instr.sourceCol); break; }
            frame.slots[instr.dest] = Value::fromFloat(frame.slots[instr.left].floatValue / r);
            break;
        }
        case Opcode::FNEG:
            frame.slots[instr.dest] = Value::fromFloat(-frame.slots[instr.src].floatValue);
            break;
        case Opcode::INT_TO_FLOAT:
            frame.slots[instr.dest] = Value::fromFloat((double)frame.slots[instr.src].intValue);
            break;
        case Opcode::FLOAT_TO_INT:
            frame.slots[instr.dest] = Value::fromInt((int)frame.slots[instr.src].floatValue);
            break;

        // ── float32 aritmetiği (ADR-040) — gerçek `float` hassasiyetiyle
        // hesaplanır (double'a genişletip yuvarlamak değil), MIR'in native
        // MIR_T_F FADD/FSUB/FMUL/FDIV'iyle bit-birebir aynı sonucu vermek için
        // (çift-yuvarlama riskinden kaçınılır, VM≡JIT diferansiyel sözleşme).
        case Opcode::LOAD_FLOAT32:
            frame.slots[instr.dest] = Value::fromFloat32(instr.floatValue);
            break;
        case Opcode::F32ADD:
            frame.slots[instr.dest] = Value::fromFloat32((double)(
                (float)frame.slots[instr.left].floatValue + (float)frame.slots[instr.right].floatValue));
            break;
        case Opcode::F32SUB:
            frame.slots[instr.dest] = Value::fromFloat32((double)(
                (float)frame.slots[instr.left].floatValue - (float)frame.slots[instr.right].floatValue));
            break;
        case Opcode::F32MUL:
            frame.slots[instr.dest] = Value::fromFloat32((double)(
                (float)frame.slots[instr.left].floatValue * (float)frame.slots[instr.right].floatValue));
            break;
        case Opcode::F32DIV: {
            float r = (float)frame.slots[instr.right].floatValue;
            if (r == 0.0f) { pendingThrow_ = makeErrorValue("float division by zero", "E_DIVZERO", instr.sourceLine, instr.sourceCol); break; }
            frame.slots[instr.dest] = Value::fromFloat32((double)((float)frame.slots[instr.left].floatValue / r));
            break;
        }
        case Opcode::F32NEG:
            frame.slots[instr.dest] = Value::fromFloat32((double)(-(float)frame.slots[instr.src].floatValue));
            break;
        case Opcode::INT_TO_FLOAT32:
            frame.slots[instr.dest] = Value::fromFloat32((double)frame.slots[instr.src].intValue);
            break;
        case Opcode::FLOAT32_TO_INT: {
            float fv = (float)frame.slots[instr.src].floatValue;
            if (!std::isfinite(fv) || fv < (float)INT_MIN || fv > (float)INT_MAX) {
                if (instr.left == 1) frame.slots[instr.dest] = Value::null();
                else pendingThrow_ = makeErrorValue(
                    "float value out of int range or NaN/Inf", "E_CAST",
                    instr.sourceLine, instr.sourceCol);
            } else {
                frame.slots[instr.dest] = Value::fromInt((int)fv);
            }
            break;
        }
        case Opcode::FLOAT_TO_FLOAT32:
            // double → float: veri kaybı gerçekleşir (E003 derleme zamanında uyardı).
            frame.slots[instr.dest] = Value::fromFloat32(frame.slots[instr.src].floatValue);
            break;
        case Opcode::FLOAT32_TO_FLOAT:
            // float → double: kayıpsız genişletme, kind değişir (Float32 → Float).
            frame.slots[instr.dest] = Value::fromFloat(frame.slots[instr.src].floatValue);
            break;

        // ── longint aritmetiği (ADR-040) — 64-bit, rank kulesi dışında izole ──
        case Opcode::LOAD_LONG:
            frame.slots[instr.dest] = Value::fromLongInt(instr.int64Value);
            break;
        case Opcode::LADD:
            frame.slots[instr.dest] = Value::fromLongInt(
                wrapAddI64(frame.slots[instr.left].int64Value, frame.slots[instr.right].int64Value));
            break;
        case Opcode::LSUB:
            frame.slots[instr.dest] = Value::fromLongInt(
                wrapSubI64(frame.slots[instr.left].int64Value, frame.slots[instr.right].int64Value));
            break;
        case Opcode::LMUL:
            frame.slots[instr.dest] = Value::fromLongInt(
                wrapMulI64(frame.slots[instr.left].int64Value, frame.slots[instr.right].int64Value));
            break;
        case Opcode::LDIV: {
            long long d = frame.slots[instr.right].int64Value;
            if (d == 0) { pendingThrow_ = makeErrorValue("division by zero", "E_DIVZERO", instr.sourceLine, instr.sourceCol); break; }
            frame.slots[instr.dest] = Value::fromLongInt(
                wrapDivI64(frame.slots[instr.left].int64Value, d));
            break;
        }
        case Opcode::LMOD: {
            long long d = frame.slots[instr.right].int64Value;
            if (d == 0) { pendingThrow_ = makeErrorValue("sıfıra bölme (mod)", "E_DIVZERO", instr.sourceLine, instr.sourceCol); break; }
            frame.slots[instr.dest] = Value::fromLongInt(
                wrapModI64(frame.slots[instr.left].int64Value, d));
            break;
        }
        case Opcode::LNEG:
            frame.slots[instr.dest] = Value::fromLongInt(wrapNegI64(frame.slots[instr.src].int64Value));
            break;
        case Opcode::LBAND:
            frame.slots[instr.dest] = Value::fromLongInt(
                frame.slots[instr.left].int64Value & frame.slots[instr.right].int64Value);
            break;
        case Opcode::LBOR:
            frame.slots[instr.dest] = Value::fromLongInt(
                frame.slots[instr.left].int64Value | frame.slots[instr.right].int64Value);
            break;
        case Opcode::LBXOR:
            frame.slots[instr.dest] = Value::fromLongInt(
                frame.slots[instr.left].int64Value ^ frame.slots[instr.right].int64Value);
            break;
        case Opcode::LSHL:
            frame.slots[instr.dest] = Value::fromLongInt(
                wrapShlI64(frame.slots[instr.left].int64Value, frame.slots[instr.right].int64Value));
            break;
        case Opcode::LSHR:
            frame.slots[instr.dest] = Value::fromLongInt(
                wrapShrI64(frame.slots[instr.left].int64Value, frame.slots[instr.right].int64Value));
            break;
        case Opcode::LBNOT:
            frame.slots[instr.dest] = Value::fromLongInt(~frame.slots[instr.src].int64Value);
            break;
        case Opcode::INT_TO_LONG:
            // int → longint: kayıpsız genişletme, işaret uzatılır (32→64 bit).
            frame.slots[instr.dest] = Value::fromLongInt((long long)frame.slots[instr.src].intValue);
            break;
        case Opcode::LONG_TO_INT_CHECKED: {
            long long lv = frame.slots[instr.src].int64Value;
            if (lv < INT_MIN || lv > INT_MAX) {
                if (instr.left == 1) frame.slots[instr.dest] = Value::null();
                else pendingThrow_ = makeErrorValue(
                    "longint value " + std::to_string(lv) + " out of int range", "E_CAST",
                    instr.sourceLine, instr.sourceCol);
            } else {
                frame.slots[instr.dest] = Value::fromInt((int)lv);
            }
            break;
        }

        // ── Struct (ADR-020: referans semantiği) ──────────────────────────
        case Opcode::STRUCT_NEW: {
            StructObject* obj = heap_.allocStruct(instr.intValue);
            obj->fieldNames   = instr.fieldNames;
            callStack_.back().slots[instr.dest] = Value::fromRef(obj);
            break;
        }
        case Opcode::FIELD_GET: {
            Value& objVal = frame.slots[instr.src];
            if (objVal.kind != ValueKind::Ref || !objVal.ref)
                throw std::runtime_error("not a struct");
            auto* obj = (StructObject*)objVal.ref;
            int idx = instr.intValue;
            if (idx < 0 || idx >= (int)obj->fields.size())
                throw std::runtime_error("invalid struct field index " + std::to_string(idx));
            frame.slots[instr.dest] = obj->fields[idx];
            break;
        }
        case Opcode::FIELD_SET: {
            Value& objVal = frame.slots[instr.dest];
            if (objVal.kind != ValueKind::Ref || !objVal.ref)
                throw std::runtime_error("not a struct");
            auto* obj = (StructObject*)objVal.ref;
            int idx = instr.intValue;
            if (idx < 0 || idx >= (int)obj->fields.size())
                throw std::runtime_error("invalid struct field index " + std::to_string(idx));
            obj->fields[idx] = frame.slots[instr.right];
            break;
        }

        // ── Array (ADR-020: referans semantiği) ───────────────────────────
        case Opcode::ARRAY_NEW: {
            ArrayObject* arr = heap_.allocArray(instr.intValue);
            arr->elements.resize(instr.intValue, Value::fromInt(0));
            frame.slots[instr.dest] = Value::fromRef(arr);
            break;
        }
        case Opcode::ARRAY_GET: {
            Value& arrVal = frame.slots[instr.left];
            if (arrVal.kind != ValueKind::Ref || !arrVal.ref) {
                pendingThrow_ = makeErrorValue("expected array, got different type", "E_TYPE", instr.sourceLine, instr.sourceCol); break;
            }
            auto* arr = (ArrayObject*)arrVal.ref;
            int idx = frame.slots[instr.right].intValue;
            if (idx < 0 || idx >= (int)arr->elements.size()) {
                pendingThrow_ = makeErrorValue(
                    "array index out of bounds (index=" + std::to_string(idx) +
                    ", length=" + std::to_string(arr->elements.size()) + ")", "E_OOB",
                    instr.sourceLine, instr.sourceCol);
                break;
            }
            frame.slots[instr.dest] = arr->elements[idx];
            break;
        }
        case Opcode::ARRAY_SET: {
            Value& arrVal = frame.slots[instr.dest];
            if (arrVal.kind != ValueKind::Ref || !arrVal.ref) {
                pendingThrow_ = makeErrorValue("expected array, got different type", "E_TYPE", instr.sourceLine, instr.sourceCol); break;
            }
            auto* arr = (ArrayObject*)arrVal.ref;
            int idx = frame.slots[instr.left].intValue;
            if (idx < 0 || idx >= (int)arr->elements.size()) {
                pendingThrow_ = makeErrorValue(
                    "array index out of bounds (index=" + std::to_string(idx) +
                    ", length=" + std::to_string(arr->elements.size()) + ")", "E_OOB",
                    instr.sourceLine, instr.sourceCol);
                break;
            }
            arr->elements[idx] = frame.slots[instr.right];
            break;
        }
        case Opcode::ARRAY_LEN: {
            Value& arrVal = frame.slots[instr.src];
            if (arrVal.kind != ValueKind::Ref || !arrVal.ref)
                throw std::runtime_error("not an array");
            auto* arr = (ArrayObject*)arrVal.ref;
            frame.slots[instr.dest] = Value::fromInt((int)arr->elements.size());
            break;
        }

        // ── Tip dönüşümleri (ADR-026: as operatörü) ─────────────────────
        case Opcode::CAST_INT_TO_STR: {
            frame.slots[instr.dest] = Value::fromString(
                std::to_string(frame.slots[instr.src].intValue));
            break;
        }
        case Opcode::CAST_FLOAT_TO_STR: {
            std::ostringstream oss;
            double fv = frame.slots[instr.src].floatValue;
            oss << fv;
            frame.slots[instr.dest] = Value::fromString(oss.str());
            break;
        }
        case Opcode::CAST_BOOL_TO_STR:
            frame.slots[instr.dest] = Value::fromString(
                frame.slots[instr.src].intValue ? "true" : "false");
            break;

        case Opcode::CAST_STR_TO_INT: {
            const std::string& s = frame.slots[instr.src].stringValue;
            try {
                size_t pos;
                long long v = std::stoll(s, &pos);
                if (pos != s.size()) throw std::invalid_argument("incomplete parse");
                if (v < INT_MIN || v > INT_MAX) throw std::out_of_range("overflow");
                frame.slots[instr.dest] = Value::fromInt((int)v);
            } catch (...) {
                if (instr.left == 1) frame.slots[instr.dest] = Value::null();
                else pendingThrow_ = makeErrorValue(
                    "'" + s + "' cannot convert to int", "E_CAST",
                    instr.sourceLine, instr.sourceCol);
            }
            break;
        }
        case Opcode::CAST_STR_TO_FLOAT: {
            const std::string& s = frame.slots[instr.src].stringValue;
            try {
                size_t pos;
                double v = std::stod(s, &pos);
                if (pos != s.size()) throw std::invalid_argument("incomplete parse");
                frame.slots[instr.dest] = Value::fromFloat(v);
            } catch (...) {
                if (instr.left == 1) frame.slots[instr.dest] = Value::null();
                else pendingThrow_ = makeErrorValue(
                    "'" + s + "' cannot convert to float", "E_CAST",
                    instr.sourceLine, instr.sourceCol);
            }
            break;
        }
        case Opcode::CAST_FLOAT_TO_INT_CHECKED: {
            double fv = frame.slots[instr.src].floatValue;
            if (!std::isfinite(fv) || fv < (double)INT_MIN || fv > (double)INT_MAX) {
                if (instr.left == 1) frame.slots[instr.dest] = Value::null();
                else pendingThrow_ = makeErrorValue(
                    "float value out of int range or NaN/Inf", "E_CAST",
                    instr.sourceLine, instr.sourceCol);
            } else {
                frame.slots[instr.dest] = Value::fromInt((int)fv); // truncate to zero
            }
            break;
        }
        case Opcode::CAST_LONG_TO_STR: {
            frame.slots[instr.dest] = Value::fromString(
                std::to_string(frame.slots[instr.src].int64Value));
            break;
        }
        case Opcode::CAST_STR_TO_LONG: {
            const std::string& s = frame.slots[instr.src].stringValue;
            try {
                size_t pos;
                long long v = std::stoll(s, &pos);
                if (pos != s.size()) throw std::invalid_argument("incomplete parse");
                frame.slots[instr.dest] = Value::fromLongInt(v);
            } catch (...) {
                if (instr.left == 1) frame.slots[instr.dest] = Value::null();
                else pendingThrow_ = makeErrorValue(
                    "'" + s + "' cannot convert to longint", "E_CAST",
                    instr.sourceLine, instr.sourceCol);
            }
            break;
        }
        case Opcode::CAST_FLOAT32_TO_STR: {
            // MIR rt_jit_float32_to_str ile birebir (setprecision(9), gerçek float).
            std::ostringstream oss;
            oss << std::setprecision(9) << (float)frame.slots[instr.src].floatValue;
            frame.slots[instr.dest] = Value::fromString(oss.str());
            break;
        }
        case Opcode::CAST_STR_TO_FLOAT32: {
            const std::string& s = frame.slots[instr.src].stringValue;
            try {
                size_t pos;
                float v = std::stof(s, &pos);
                if (pos != s.size()) throw std::invalid_argument("incomplete parse");
                frame.slots[instr.dest] = Value::fromFloat32((double)v);
            } catch (...) {
                if (instr.left == 1) frame.slots[instr.dest] = Value::null();
                else pendingThrow_ = makeErrorValue(
                    "'" + s + "' cannot convert to float", "E_CAST",
                    instr.sourceLine, instr.sourceCol);
            }
            break;
        }
        case Opcode::CAST_FLOAT_TO_LONG_CHECKED: {
            double fv = frame.slots[instr.src].floatValue;
            if (!std::isfinite(fv) || fv < -9223372036854775808.0 || fv >= 9223372036854775808.0) {
                if (instr.left == 1) frame.slots[instr.dest] = Value::null();
                else pendingThrow_ = makeErrorValue(
                    "float value out of longint range or NaN/Inf", "E_CAST",
                    instr.sourceLine, instr.sourceCol);
            } else {
                frame.slots[instr.dest] = Value::fromLongInt((long long)fv); // sıfıra kırp
            }
            break;
        }
        case Opcode::CAST_INT_TO_BYTE_CHECKED: {
            // #86: int → byte, 0-255 dışı sessiz kırpılmaz — fallible
            int iv = frame.slots[instr.src].intValue;
            if (iv < 0 || iv > 255) {
                if (instr.left == 1) frame.slots[instr.dest] = Value::null();
                else pendingThrow_ = makeErrorValue(
                    "integer value " + std::to_string(iv) + " out of byte range (0-255)",
                    "E_CAST", instr.sourceLine, instr.sourceCol);
            } else {
                frame.slots[instr.dest] = Value::fromInt(iv); // byte int olarak taşınır
            }
            break;
        }

        // ── Decimal aritmetik (ADR-028) ──────────────────────────────────
        case Opcode::LOAD_DECIMAL:
            frame.slots[instr.dest] = Value::fromDecimal(instr.decimalValue);
            break;
        case Opcode::DADD: {
            auto r = DecimalValue::add(frame.slots[instr.left].decimalValue,
                                       frame.slots[instr.right].decimalValue);
            if (r.isOverflow()) {
                pendingThrow_ = makeErrorValue("decimal overflow", "E_DECIMAL_OVERFLOW",
                                               instr.sourceLine, instr.sourceCol); break;
            }
            frame.slots[instr.dest] = Value::fromDecimal(r);
            break;
        }
        case Opcode::DSUB: {
            auto r = DecimalValue::sub(frame.slots[instr.left].decimalValue,
                                       frame.slots[instr.right].decimalValue);
            if (r.isOverflow()) {
                pendingThrow_ = makeErrorValue("decimal overflow", "E_DECIMAL_OVERFLOW",
                                               instr.sourceLine, instr.sourceCol); break;
            }
            frame.slots[instr.dest] = Value::fromDecimal(r);
            break;
        }
        case Opcode::DMUL: {
            auto r = DecimalValue::mul(frame.slots[instr.left].decimalValue,
                                       frame.slots[instr.right].decimalValue);
            if (r.isOverflow()) {
                pendingThrow_ = makeErrorValue("decimal overflow", "E_DECIMAL_OVERFLOW",
                                               instr.sourceLine, instr.sourceCol); break;
            }
            frame.slots[instr.dest] = Value::fromDecimal(r);
            break;
        }
        case Opcode::DDIV: {
            const DecimalValue& divisor = frame.slots[instr.right].decimalValue;
            if (divisor.coeff == 0) {
                pendingThrow_ = makeErrorValue("decimal division by zero", "E_DECIMAL_DIVZERO",
                                               instr.sourceLine, instr.sourceCol); break;
            }
            auto r = DecimalValue::div(frame.slots[instr.left].decimalValue, divisor);
            if (r.isOverflow()) {
                pendingThrow_ = makeErrorValue("decimal overflow", "E_DECIMAL_OVERFLOW",
                                               instr.sourceLine, instr.sourceCol); break;
            }
            frame.slots[instr.dest] = Value::fromDecimal(r);
            break;
        }
        case Opcode::DMOD: {
            const DecimalValue& divisor = frame.slots[instr.right].decimalValue;
            if (divisor.coeff == 0) {
                pendingThrow_ = makeErrorValue("decimal modulo by zero", "E_DECIMAL_DIVZERO",
                                               instr.sourceLine, instr.sourceCol); break;
            }
            auto r = DecimalValue::mod(frame.slots[instr.left].decimalValue, divisor);
            if (r.isOverflow()) {
                pendingThrow_ = makeErrorValue("decimal overflow", "E_DECIMAL_OVERFLOW",
                                               instr.sourceLine, instr.sourceCol); break;
            }
            frame.slots[instr.dest] = Value::fromDecimal(r);
            break;
        }
        case Opcode::DNEG:
            frame.slots[instr.dest] = Value::fromDecimal(
                DecimalValue::neg(frame.slots[instr.src].decimalValue));
            break;
        case Opcode::INT_TO_DECIMAL:
            frame.slots[instr.dest] = Value::fromDecimal(
                DecimalValue::fromInt(frame.slots[instr.src].intValue));
            break;
        case Opcode::FLOAT_TO_DECIMAL:
            frame.slots[instr.dest] = Value::fromDecimal(
                DecimalValue::fromDouble(frame.slots[instr.src].floatValue));
            break;
        case Opcode::CAST_DECIMAL_TO_STR:
            frame.slots[instr.dest] = Value::fromString(
                frame.slots[instr.src].decimalValue.toString());
            break;
        case Opcode::CAST_DECIMAL_TO_FLOAT:
            frame.slots[instr.dest] = Value::fromFloat(
                frame.slots[instr.src].decimalValue.toDouble());
            break;
        case Opcode::CAST_DECIMAL_TO_INT: {
            const DecimalValue& dv = frame.slots[instr.src].decimalValue;
            DecimalValue trunc = DecimalValue::truncate(dv);
            if (trunc.coeff < INT_MIN || trunc.coeff > INT_MAX) {
                if (instr.left == 1) frame.slots[instr.dest] = Value::null();
                else pendingThrow_ = makeErrorValue(
                    "decimal value out of int range", "E_CAST",
                    instr.sourceLine, instr.sourceCol);
            } else {
                frame.slots[instr.dest] = Value::fromInt((int)trunc.coeff);
            }
            break;
        }
        case Opcode::CAST_STR_TO_DECIMAL: {
            const std::string& s = frame.slots[instr.src].stringValue;
            try {
                DecimalValue dv = DecimalValue::fromString(s);
                frame.slots[instr.dest] = Value::fromDecimal(dv);
            } catch (...) {
                if (instr.left == 1) frame.slots[instr.dest] = Value::null();
                else pendingThrow_ = makeErrorValue(
                    "'" + s + "' cannot convert to decimal", "E_CAST",
                    instr.sourceLine, instr.sourceCol);
            }
            break;
        }

        // ── String (ADR-024: immutable değer-tipi, içerik ==) ────────────
        case Opcode::STRING_CONCAT: {
            const std::string& left = frame.slots[instr.left].stringValue;
            const std::string& right = frame.slots[instr.right].stringValue;
            std::string joined;
            joined.reserve(left.size() + right.size());
            joined.append(left);
            joined.append(right);
            frame.slots[instr.dest] = Value::fromString(std::move(joined));
            break;
        }

        // ── Hata yönetimi (ADR-025) ──────────────────────────────────────
        case Opcode::ENTER_TRY:
            tryStack_.push_back({callStack_.size(), instr.jumpTarget, instr.dest});
            break;

        case Opcode::LEAVE_TRY:
            if (!tryStack_.empty()) tryStack_.pop_back();
            break;

        case Opcode::THROW: {
            Value errVal = frame.slots[instr.src];
            // If user throws Error struct, fill trace field (fields[3])
            if (errVal.kind == ValueKind::Ref && errVal.ref &&
                errVal.ref->type == ObjectType::Struct) {
                auto* errObj = static_cast<StructObject*>(errVal.ref);
                if ((int)errObj->fields.size() >= 4)
                    errObj->fields[3] = Value::fromString(buildTrace());
            } else {
                // #4 — mimari karar: struct-olmayan (düz string vb.) throw
                // değeri otomatik Error{message=<değerin string temsili>,
                // code="", line, col, trace}'a sarmalanır. Böylece her
                // `catch (Error e)` güvenle e.code/e.message okuyabilir.
                errVal = makeErrorValue(errVal.toString(), "",
                                         instr.sourceLine, instr.sourceCol);
            }
            pendingThrow_ = std::move(errVal);
            break;
        }

        // ── FFI ───────────────────────────────────────────────────────────
        case Opcode::CALLHOST: {
            if (vmTrace_) [[unlikely]] ++vmTrace_->vmFfiCalls;
            if (instr.functionName == "__builtin_method__") {
                if (vmTrace_) [[unlikely]] ++vmTrace_->vmBuiltinCalls;
                try {
                    std::vector<const Value*> argRefs;
                    argRefs.reserve(instr.argSlots.size());
                    for (int s : instr.argSlots)
                        argRefs.push_back(&frame.slots[s]);

                    Value ret;
                    if (dispatchStringBuiltinNoCopy(instr.intValue, argRefs, heap_, ret)) {
                        if (instr.dest >= 0)
                            callStack_.back().slots[instr.dest] = std::move(ret);
                        break;
                    }

                    // Built-in metod: sabit id ile dispatch, O(1) tablo lookup
                    std::vector<Value> argVals;
                    argVals.reserve(instr.argSlots.size());
                    for (const Value* v : argRefs)
                        argVals.push_back(*v);
                    ret = dispatchBuiltinMethod(instr.intValue, argVals, heap_);
                    if (instr.dest >= 0)
                        callStack_.back().slots[instr.dest] = std::move(ret);
                } catch (const std::runtime_error& e) {
                    pendingThrow_ = makeErrorValue(e.what(), "E_BUILTIN",
                                                   instr.sourceLine, instr.sourceCol);
                }
            } else if (instr.functionName == "__ffi__") {
                // ADR-034 (#107): sayısal host id ile FFI dispatch
                // ADR-035 (#76): runtime capability backstop (A+B modelinin B'si)
                if (instr.requiredCap && caps_.find(*instr.requiredCap) == caps_.end()) {
                    pendingThrow_ = makeErrorValue(
                        std::string("requires --allow-") + capabilityName(*instr.requiredCap) +
                            " capability",
                        "E_CAP_MISSING", instr.sourceLine, instr.sourceCol);
                    break;
                }
                std::vector<Value> argVals;
                argVals.reserve(instr.argSlots.size());
                for (int s : instr.argSlots)
                    argVals.push_back(frame.slots[s]);
                try {
                    HostContext ctx{&caps_, &programArgs_, &heap_};
                    Value ret = callHostFn(instr.intValue, argVals, ctx);
                    if (instr.dest >= 0)
                        callStack_.back().slots[instr.dest] = std::move(ret);
                } catch (const std::runtime_error& e) {
                    pendingThrow_ = makeErrorValue(e.what(), "E_FFI",
                                                   instr.sourceLine, instr.sourceCol);
                }
            } else {
                executeHostFunction(instr.functionName, frame.slots, instr.argSlots);
            }
            break;
        }
        }

        // ── pendingThrow_ işle: try varsa catch'e unwind, yoksa fırlat ───
        if (pendingThrow_.has_value()) {
            Value errVal = std::move(*pendingThrow_);
            pendingThrow_.reset();

            if (!tryStack_.empty()) {
                TryFrame tf = tryStack_.back();
                tryStack_.pop_back();
                // catch bloğunun bulunduğu frame'e unwind
                while (callStack_.size() > tf.callStackDepth)
                    callStack_.pop_back();
                // Error'ı catch değişkenine bağla ve catch etiketine atla
                callStack_.back().slots[tf.errorSlot] = std::move(errVal);
                callStack_.back().instructionPointer  = tf.catchTarget;
            } else {
                // Uncaught error — extract message and raise as C++ exception
                std::string msg = "uncaught error";
                if (errVal.kind == ValueKind::Ref && errVal.ref) {
                    auto* s = static_cast<StructObject*>(errVal.ref);
                    if ((int)s->fields.size() > 2 &&
                        s->fields[2].kind == ValueKind::String)
                        msg = s->fields[2].stringValue;
                } else if (errVal.kind == ValueKind::String) {
                    msg = errVal.stringValue;
                }
                throw std::runtime_error(msg);
            }
            continue;
        }
    }
    }  // _profExec kapsamı — "vm-exec" burada biter

    // Döngü bitti — callStack boş
    state_ = RunState::Finished;
    return RunReason::Finished;
}

// DAP: run()'ın ilklendirme kısmı. VM'i çalıştırmadan hazırlar.
void Interpreter::initForDebug() {
    if (vmInitialized_) return;

    // Globalleri sıfırla — tek flat dizi (bkz. globalSlots_ yorum notu, #3)
    globalSlots_.assign(program_.globalCount, Value::fromInt(0));

    IRFunction* mainFunction = program_.findFunction("main");
    if (!mainFunction)
        throw std::runtime_error("'main' function not found");

    CallFrame mainFrame;
    mainFrame.function           = mainFunction;
    mainFrame.instructionPointer = 0;
    mainFrame.slots.resize(mainFunction->slotCount, Value::fromInt(0));
    mainFrame.returnDestSlot     = -1;
    callStack_.push_back(std::move(mainFrame));
    vmInitialized_ = true;
}

// Faz 5: run() artık başlatma + runUntilEvent çağrısı.
int Interpreter::run() {
    // Eğer VM zaten başlatıldıysa (DAP resume) — sadece devam et
    if (vmInitialized_ && !callStack_.empty()) {
        runUntilEvent(-1, -1);
        return lastReturnValue_;
    }

    {
        profiling::StageTimer::ScopedStage _prof(stageProfiler_, "vm-warmup");
        initForDebug();
    }

    runUntilEvent(-1, -1);
    return lastReturnValue_;
}

void Interpreter::executeHostFunction(const std::string&       name,
                                       const std::vector<Value>& slots,
                                       const std::vector<int>&   argSlots) {
    if (name == "print") {
        if (!argSlots.empty()) {
            const Value& val = slots[argSlots[0]];
            // Faz 7 (#105): DAP modunda çıktı sink üzerinden output event'ine
            // gider — protokol stdout'una çıplak bayt sızmaz.
            std::string text = val.toString();
            if (outputSink_) outputSink_(text);
            else             std::cout << text << std::flush;
        }
        return;
    }
    throw std::runtime_error("unknown host function '" + name + "'");
}

// ── Built-in Method Dispatch ─────────────────────────────────────────────────
//
// runtimeId, BuiltinMethodRegistry::init() içinde metodların tanımlanma sırasıyla
// örtüşmeli. Sıra: length(0), push(1), pop(2), insert(3), remove(4), slice(5),
// reverse(6), concat(7), contains(8), indexOf(9), clear(10),
// sv:length(11), sv:upper(12), sv:lower(13), sv:trim(14), sv:split(15),
// sv:substring(16), sv:replace(17), sv:repeat(18), sv:charAt(19),
// sv:indexOf(20), sv:contains(21), sv:startsWith(22), sv:endsWith(23),
// st:toJson(24), st:dump(25)
//
// Her handler: args[0] = receiver, args[1..] = diğer argümanlar.

// Yardımcı: Value'dan ArrayObject* al
static ArrayObject* asArray(const Value& v, const char* ctx) {
    if (v.kind != ValueKind::Ref || !v.ref || v.ref->type != ObjectType::Array)
        throw std::runtime_error(std::string(ctx) + " — expected array");
    return static_cast<ArrayObject*>(v.ref);
}

// Yardımcı: iki Value'un saQut eşitliği (== semantiği, ADR-023)
// Primitive: değer karşılaştırma; referans: kimlik; string: içerik.
static bool valueEqual(const Value& a, const Value& b) {
    if (a.kind != b.kind) return false;
    switch (a.kind) {
        case ValueKind::Int:     return a.intValue  == b.intValue;
        case ValueKind::LongInt: return a.int64Value == b.int64Value;
        case ValueKind::Float:
        case ValueKind::Float32: return a.floatValue == b.floatValue;
        case ValueKind::Decimal: return a.decimalValue.toString() == b.decimalValue.toString();
        case ValueKind::String:  return a.stringValue == b.stringValue;
        case ValueKind::Ref:     return a.ref == b.ref;
        case ValueKind::Null:    return true;
        case ValueKind::Date:    return a.int64Value == b.int64Value;
    }
    return false;
}

// Yardımcı: struct alanını JSON string'e çevir (toJson için)
static std::string valueToJsonStr(const Value& v);
static std::string structToJson(StructObject* obj) {
    std::string s = "{";
    for (size_t i = 0; i < obj->fields.size(); ++i) {
        if (i) s += ",";
        std::string key = (i < obj->fieldNames.size())
                          ? obj->fieldNames[i]
                          : ("field" + std::to_string(i));
        s += "\"" + key + "\":" + valueToJsonStr(obj->fields[i]);
    }
    s += "}";
    return s;
}
static std::string valueToJsonStr(const Value& v) {
    switch (v.kind) {
        case ValueKind::Int:     return std::to_string(v.intValue);
        case ValueKind::LongInt: return std::to_string(v.int64Value);
        case ValueKind::Float:
        case ValueKind::Float32: {
            std::ostringstream os; os << v.floatValue; return os.str();
        }
        case ValueKind::Decimal: return v.decimalValue.toString();
        case ValueKind::String:  {
            // JSON string escaping (minimal)
            std::string r = "\"";
            for (char c : v.stringValue) {
                if      (c == '"')  r += "\\\"";
                else if (c == '\\') r += "\\\\";
                else if (c == '\n') r += "\\n";
                else if (c == '\t') r += "\\t";
                else r += c;
            }
            r += "\"";
            return r;
        }
        case ValueKind::Ref: {
            if (!v.ref) return "null";
            if (v.ref->type == ObjectType::Struct)
                return structToJson(static_cast<StructObject*>(v.ref));
            // Array içi JSON
            auto* arr = static_cast<ArrayObject*>(v.ref);
            std::string s = "[";
            for (size_t i = 0; i < arr->elements.size(); ++i) {
                if (i) s += ",";
                s += valueToJsonStr(arr->elements[i]);
            }
            s += "]";
            return s;
        }
        case ValueKind::Null: return "null";
        case ValueKind::Date: return std::to_string(v.int64Value);
    }
    return "null";
}

Value Interpreter::dispatchBuiltinMethod(int                       runtimeId,
                                          const std::vector<Value>& args,
                                          Heap&                     heap)
{
    // ── Array metodları (id 0–10) ─────────────────────────────────────────────

    // 0: E::length(E[]) -> int
    switch (runtimeId) {
    case 0: {
        auto* arr = asArray(args[0], "length");
        return Value::fromInt((int)arr->elements.size());
    }
    // 1: E::push(E[], E) -> int   (indeks döner)
    case 1: {
        auto* arr = asArray(args[0], "push");
        arr->elements.push_back(args[1]);
        return Value::fromInt((int)arr->elements.size() - 1);
    }
    // 2: E::pop(E[]) -> E
    case 2: {
        auto* arr = asArray(args[0], "pop");
        if (arr->elements.empty())
            throw std::runtime_error("pop on empty array");
        Value v = std::move(arr->elements.back());
        arr->elements.pop_back();
        return v;
    }
    // 3: E::insert(E[], int, E) -> int
    case 3: {
        auto* arr = asArray(args[0], "insert");
        if (args[1].kind != ValueKind::Int)
            throw std::runtime_error("insert — index must be int");
        int idx = args[1].intValue;
        if (idx < 0 || idx > (int)arr->elements.size())
            throw std::runtime_error("insert — index out of bounds");
        arr->elements.insert(arr->elements.begin() + idx, args[2]);
        return Value::fromInt(idx);
    }
    // 4: E::remove(E[], int) -> E
    case 4: {
        auto* arr = asArray(args[0], "remove");
        if (args[1].kind != ValueKind::Int)
            throw std::runtime_error("remove — index must be int");
        int idx = args[1].intValue;
        if (idx < 0 || idx >= (int)arr->elements.size())
            throw std::runtime_error("remove — index out of bounds");
        Value v = std::move(arr->elements[idx]);
        arr->elements.erase(arr->elements.begin() + idx);
        return v;
    }
    // 5: E::slice(E[], int, int) -> E[]   (yeni array)
    case 5: {
        auto* arr = asArray(args[0], "slice");
        int from = (args[1].kind == ValueKind::Int) ? args[1].intValue : 0;
        int to   = (args[2].kind == ValueKind::Int) ? args[2].intValue : (int)arr->elements.size();
        if (from < 0) from = 0;
        if (to > (int)arr->elements.size()) to = (int)arr->elements.size();
        auto* dst = heap.allocArray(to - from);
        for (int i = from; i < to; ++i)
            dst->elements.push_back(arr->elements[i]);
        return Value::fromRef(dst);
    }
    // 6: E::reverse(E[]) -> E[]   (yerinde; aynı referansı döner)
    case 6: {
        auto* arr = asArray(args[0], "reverse");
        std::reverse(arr->elements.begin(), arr->elements.end());
        return args[0]; // aynı referans
    }
    // 7: E::concat(E[], E[]) -> E[]   (yeni array)
    case 7: {
        auto* a = asArray(args[0], "concat");
        auto* b = asArray(args[1], "concat");
        auto* dst = heap.allocArray((int)(a->elements.size() + b->elements.size()));
        for (auto& v : a->elements) dst->elements.push_back(v);
        for (auto& v : b->elements) dst->elements.push_back(v);
        return Value::fromRef(dst);
    }
    // 8: E::contains(E[], E) -> bool
    case 8: {
        auto* arr = asArray(args[0], "contains");
        for (auto& v : arr->elements)
            if (valueEqual(v, args[1])) return Value::fromInt(1);
        return Value::fromInt(0);
    }
    // 9: E::indexOf(E[], E) -> int?
    case 9: {
        auto* arr = asArray(args[0], "indexOf");
        for (int i = 0; i < (int)arr->elements.size(); ++i)
            if (valueEqual(arr->elements[i], args[1])) return Value::fromInt(i);
        return Value::null();
    }
    // 10: E::clear(E[]) -> void
    case 10: {
        auto* arr = asArray(args[0], "clear");
        arr->elements.clear();
        return Value::fromInt(0); // void — caller dest=-1 olduğundan kullanılmaz
    }

    // ── String value metodları (id 11–23) ─────────────────────────────────────

    // 11: string::length(string) -> int
    case 11: {
        if (args[0].kind != ValueKind::String)
            throw std::runtime_error("string::length — expected string");
        return Value::fromInt((int)args[0].stringValue.size());
    }
    // 12: string::upper(string) -> string
    case 12: {
        if (args[0].kind != ValueKind::String)
            throw std::runtime_error("string::upper — expected string");
        std::string s = args[0].stringValue;
        for (char& c : s) c = (char)std::toupper((unsigned char)c);
        return Value::fromString(std::move(s));
    }
    // 13: string::lower(string) -> string
    case 13: {
        if (args[0].kind != ValueKind::String)
            throw std::runtime_error("string::lower — expected string");
        std::string s = args[0].stringValue;
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return Value::fromString(std::move(s));
    }
    // 14: string::trim(string) -> string
    case 14: {
        if (args[0].kind != ValueKind::String)
            throw std::runtime_error("string::trim — expected string");
        const std::string& src = args[0].stringValue;
        size_t start = src.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return Value::fromString("");
        size_t end = src.find_last_not_of(" \t\n\r");
        return Value::fromString(src.substr(start, end - start + 1));
    }
    // 15: string::split(string, string) -> string[]
    case 15: {
        if (args[0].kind != ValueKind::String || args[1].kind != ValueKind::String)
            throw std::runtime_error("string::split — expected string, string");
        const std::string& src = args[0].stringValue;
        const std::string& sep = args[1].stringValue;
        auto* arr = heap.allocArray();
        if (sep.empty()) {
            for (char c : src)
                arr->elements.push_back(Value::fromString(std::string(1, c)));
        } else {
            size_t pos = 0, found;
            while ((found = src.find(sep, pos)) != std::string::npos) {
                arr->elements.push_back(Value::fromString(src.substr(pos, found - pos)));
                pos = found + sep.size();
            }
            arr->elements.push_back(Value::fromString(src.substr(pos)));
        }
        return Value::fromRef(arr);
    }
    // 16: string::substring(string, int, int) -> string
    case 16: {
        if (args[0].kind != ValueKind::String)
            throw std::runtime_error("string::substring — expected string");
        const std::string& s = args[0].stringValue;
        int from = args[1].intValue;
        int len  = args[2].intValue;
        if (from < 0 || from > (int)s.size())
            throw std::runtime_error("string::substring — index out of bounds");
        if (len < 0) len = 0;
        return Value::fromString(s.substr(from, len));
    }
    // 17: string::replace(string, string, string) -> string
    case 17: {
        if (args[0].kind != ValueKind::String || args[1].kind != ValueKind::String || args[2].kind != ValueKind::String)
            throw std::runtime_error("string::replace — expected string, string, string");
        std::string s   = args[0].stringValue;
        const std::string& from = args[1].stringValue;
        const std::string& to   = args[2].stringValue;
        if (!from.empty()) {
            size_t pos = 0;
            while ((pos = s.find(from, pos)) != std::string::npos) {
                s.replace(pos, from.size(), to);
                pos += to.size();
            }
        }
        return Value::fromString(std::move(s));
    }
    // 18: string::repeat(string, int) -> string
    case 18: {
        if (args[0].kind != ValueKind::String)
            throw std::runtime_error("string::repeat — expected string");
        int n = args[1].intValue;
        if (n < 0) n = 0;
        std::string result;
        result.reserve(args[0].stringValue.size() * (size_t)n);
        for (int i = 0; i < n; ++i) result += args[0].stringValue;
        return Value::fromString(std::move(result));
    }
    // 19: string::charAt(string, int) -> string
    case 19: {
        if (args[0].kind != ValueKind::String)
            throw std::runtime_error("string::charAt — expected string");
        const std::string& s = args[0].stringValue;
        int idx = args[1].intValue;
        if (idx < 0 || idx >= (int)s.size())
            throw std::runtime_error("string::charAt — index out of bounds");
        return Value::fromString(std::string(1, s[idx]));
    }
    // 20: string::indexOf(string, string) -> int?
    case 20: {
        if (args[0].kind != ValueKind::String || args[1].kind != ValueKind::String)
            throw std::runtime_error("string::indexOf — expected string, string");
        size_t pos = args[0].stringValue.find(args[1].stringValue);
        if (pos == std::string::npos) return Value::null();
        return Value::fromInt((int)pos);
    }
    // 21: string::contains(string, string) -> bool
    case 21: {
        if (args[0].kind != ValueKind::String || args[1].kind != ValueKind::String)
            throw std::runtime_error("string::contains — expected string, string");
        bool found = args[0].stringValue.find(args[1].stringValue) != std::string::npos;
        return Value::fromInt(found ? 1 : 0);
    }
    // 22: string::startsWith(string, string) -> bool
    case 22: {
        if (args[0].kind != ValueKind::String || args[1].kind != ValueKind::String)
            throw std::runtime_error("string::startsWith — expected string, string");
        const std::string& s = args[0].stringValue;
        const std::string& p = args[1].stringValue;
        bool ok = s.size() >= p.size() && s.substr(0, p.size()) == p;
        return Value::fromInt(ok ? 1 : 0);
    }
    // 23: string::endsWith(string, string) -> bool
    case 23: {
        if (args[0].kind != ValueKind::String || args[1].kind != ValueKind::String)
            throw std::runtime_error("string::endsWith — expected string, string");
        const std::string& s = args[0].stringValue;
        const std::string& p = args[1].stringValue;
        bool ok = s.size() >= p.size() && s.substr(s.size() - p.size()) == p;
        return Value::fromInt(ok ? 1 : 0);
    }

    // ── Struct value metodları (id 24–25) ─────────────────────────────────────

    // 24: S::toJson(S) -> string
    case 24: {
        if (args[0].kind == ValueKind::Ref && args[0].ref &&
            args[0].ref->type == ObjectType::Struct) {
            return Value::fromString(structToJson(static_cast<StructObject*>(args[0].ref)));
        }
        return Value::fromString("null");
    }
    // 25: S::dump(S) -> string
    case 25: {
        if (args[0].kind == ValueKind::Ref && args[0].ref &&
            args[0].ref->type == ObjectType::Struct) {
            auto* obj = static_cast<StructObject*>(args[0].ref);
            std::string s = "struct{";
            for (size_t i = 0; i < obj->fields.size(); ++i) {
                if (i) s += ", ";
                std::string key = (i < obj->fieldNames.size())
                                  ? obj->fieldNames[i]
                                  : ("field" + std::to_string(i));
                s += key + "=" + obj->fields[i].toString();
            }
            s += "}";
            return Value::fromString(s);
        }
        return Value::fromString("null");
    }

    default:
        throw std::runtime_error("unknown builtin method id " + std::to_string(runtimeId));
    }
}
