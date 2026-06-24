// ============================================================================
// saQut VM — Interpreter (Bytecode Yorumlayıcı)
//
// IRProgram içindeki talimatları çalıştırır.
// "main" fonksiyonundan başlar, RETURN ile biten frame'leri kapatır.
//
// DÖNGÜ GÜVENLİĞİ (referans invalidation):
//   Her iterasyonun başında callStack.back() tazeden alınır.
//   CALL ve RETURN'den sonra `continue` ile döngü başına dönülür;
//   böylece vector büyümesinden kaynaklanan dangling pointer sorunu olmaz.
// ============================================================================

#ifndef SAQUT_VM_INTERPRETER
#define SAQUT_VM_INTERPRETER

#include <vector>
#include <optional>
#include <set>
#include <unordered_map>
#include <utility>
#include "ir/ir_program.hpp"
#include "core/module_registry.hpp"
#include "vm/call_frame.hpp"
#include "vm/object.hpp"

// ADR-025: try bloğu girişinde yığına eklenen kayıt
struct TryFrame {
    size_t callStackDepth; // ENTER_TRY anındaki callStack_.size() — unwind için
    int    catchTarget;    // catch bloğunun IR instruction indeksi
    int    errorSlot;      // catch değişkeninin slot numarası (catch frame'inde)
};

class Interpreter {
public:
    explicit Interpreter(IRProgram& program) : program_(program) {}

    // "main" fonksiyonunu bul ve çalıştır.
    // Tamamlandığında main'in dönüş değerini (int) döndürür.
    int run();

    // ── DAP API ───────────────────────────────────────────────────────────────
    enum class RunState { Running, Paused, Finished };

    void setBreakpoint(const std::string& file, int line);
    void clearBreakpoint(const std::string& file, int line);
    void clearAllBreakpoints();

    RunState    state() const { return state_; }
    void        resume();
    void        stepInstruction();
    void        stepOver();

    int         currentSourceLine() const;
    std::string currentSourceFile() const;
    int         callDepth() const;
    std::string frameFunctionName(int depth) const;
    int         frameSourceLine(int depth) const;

    Value       readSlotInFrame(int frameDepth, int slotIndex) const;
    std::string slotName(int /*frameDepth*/, int /*slotIndex*/) const { return ""; }

private:
    IRProgram&             program_;
    std::vector<CallFrame> callStack_;
    std::unordered_map<int, std::vector<Value>> moduleSlots_;
    Heap                   heap_;
    std::vector<TryFrame>  tryStack_;
    std::optional<Value>   pendingThrow_;

    // DAP durumu
    RunState state_ = RunState::Running;
    std::set<std::pair<std::string,int>> breakpoints_;  // {file, line}

    bool isBreakpoint() const;
    void checkBreakpoint();

    // Error StructObject oluştur (ADR-025): [line, col, message, trace, code]
    Value makeErrorValue(const std::string& message,
                         const std::string& code = "",
                         int line = 0, int col = 0);

    // Host (C++) fonksiyon çağrısı — şu an sadece "print" destekli
    void executeHostFunction(const std::string& name,
                             const std::vector<Value>& slots,
                             const std::vector<int>&   argSlots);

    // Built-in metod dispatch — sabit runtimeId ile O(1) tablo lookup
    Value dispatchBuiltinMethod(int runtimeId,
                                const std::vector<Value>& args,
                                Heap& heap);

    // Mevcut callStack_'i gezerek stacktrace string'i üretir.
    // pendingThrow_ set edilmeden ÖNCE çağrılmalı (unwind olmadan).
    std::string buildTrace() const;
};

#endif // SAQUT_VM_INTERPRETER
