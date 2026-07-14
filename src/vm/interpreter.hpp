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
#include <functional>
#include <set>
#include <unordered_map>
#include <utility>
#include "ir/ir_program.hpp"
#include "core/module_registry.hpp"
#include "core/capability.hpp"
#include "vm/call_frame.hpp"
#include "vm/object.hpp"

// Forward-declare: BenchVMTrace tam tanımı bench/profile.hpp'de.
// Yalnızca pointer tutulur — normal modda sıfır bağımlılık.
struct BenchVMTrace;

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

    // DAP: VM'i çalıştırmadan ilklendir (callStack, globaller, vmInitialized_)
    void initForDebug();

    // Profil hook — bench komutu tarafından set edilir (nullptr = kapalı).
    // Normal run/check/ir komutlarında çağrılmaz, sıfır maliyet.
    void setVMTrace(BenchVMTrace* t) { vmTrace_ = t; }
    int  heapAllocCount() const { return heap_.allocCount; }

    // Faz 7 (#105): program çıktısı kancası. DAP modunda print çıktısı
    // protokol stdout'unu kirletmesin diye DapHandler output event'ine
    // yönlendirilir. Varsayılan (boş) std::cout — CLI run/exec DEĞİŞMEZ.
    using OutputSink = std::function<void(const std::string&)>;
    void setOutputSink(OutputSink sink) { outputSink_ = std::move(sink); }

    // ── GC (#77, ADR-022) ────────────────────────────────────────────────────
    // Eşik tabanlı tetikleme: canlı nesne sayısı eşiği aşınca instruction
    // sınırında (safepoint) mark-sweep koşar. n <= 0 → otomatik GC kapalı
    // (yalnızca ~Heap temizler — eski arena davranışı).
    void      setGCThreshold(int n) { gcInitialThreshold_ = n; gcThreshold_ = n; }
    int       gcRuns() const       { return heap_.gcRuns; }
    long long gcFreedTotal() const { return heap_.freedTotal; }

    // ADR-035 (#76): --allow-fs/--allow-net/--allow-sys — CLI'dan doldurulur.
    void setCapabilities(std::set<Capability> caps) { caps_ = std::move(caps); }
    // #90: `--` sonrası argümanlar — sys::args() ile programa geçirilir.
    void setProgramArgs(std::vector<std::string> a) { programArgs_ = std::move(a); }
    const std::vector<std::string>& programArgs() const { return programArgs_; }
    // #91: caps::drop/caps::has runtime erişimi.
    bool hasCapability(Capability c) const { return caps_.find(c) != caps_.end(); }
    void dropCapability(Capability c) { caps_.erase(c); }

    // ── DAP API ───────────────────────────────────────────────────────────────
    enum class RunState { Running, Paused, Finished };
    // Faz 5: runUntilEvent dönüş nedeni
    enum class RunReason { StepDone, Breakpoint, Finished, BudgetExhausted, Error };

    void setBreakpoint(const std::string& file, int line);
    void clearBreakpoint(const std::string& file, int line);
    void clearAllBreakpoints();
    // Faz 7 (#105): (dosya, satır) çalıştırılabilir bir satıra denk geliyor mu?
    // setBreakpoints.verified için Faz 5'in lineToFirstIP indeksinde arar.
    bool isExecutableLine(const std::string& file, int line) const;

    RunState    state() const { return state_; }
    void        resume();
    void        stepInstruction();
    void        stepOver();
    // Faz 5: satır bazlı adımlar
    void        stepLine();   // sourceLine değişene kadar ilerle
    void        stepOut();    // callDepth azalana kadar ilerle

    // Faz 5: instruction budget ile koş — mevcut durumdan devam eder, başlatma yapmaz.
    RunReason   runUntilEvent(int maxInstructions, int startCallDepth = -1);

    int         currentSourceLine() const;
    std::string currentSourceFile() const;
    int         callDepth() const;
    std::string frameSourceFile(int depth) const;
    int         frameSlotCount(int depth) const;
    std::string frameFunctionName(int depth) const;
    int         frameSourceLine(int depth) const;

    Value       readSlotInFrame(int frameDepth, int slotIndex) const;
    // Faz 5: IRFunction::slotNames kullanarak gerçek değişken adını döndürür
    std::string slotName(int frameDepth, int slotIndex) const;

private:
    IRProgram&             program_;
    std::vector<CallFrame> callStack_;
    std::unordered_map<int, std::vector<Value>> moduleSlots_;
    Heap                   heap_;
    std::vector<TryFrame>  tryStack_;
    std::optional<Value>   pendingThrow_;
    BenchVMTrace*          vmTrace_ = nullptr;  // profil hook (bench modunda non-null)
    OutputSink             outputSink_;         // Faz 7 (#105): boş = std::cout

    // DAP durumu
    RunState state_ = RunState::Running;
    std::set<std::pair<std::string,int>> breakpoints_;  // {file, line}

    // Faz 5: debug koşu kontrolü
    bool vmInitialized_  = false;   // run() başlatmayı bir kez yapar
    int  runBudget_      = 0;       // kalan talimat bütçesi (0 = sınırsız, run() tarafından kullanılmaz)
    int  stepStartDepth_ = -1;      // stepOver/Out için başlangıç derinliği
    int  stepStartLine_  = 0;       // stepOver/Line için başlangıç satırı
    int  lastReturnValue_ = 0;      // main'in dönüş değeri (pause/resume sonrası için)

    bool isBreakpoint() const;
    void checkBreakpoint();

    // GC (#77): eşik aşıldıysa kökleri (moduleSlots_ + callStack_ +
    // pendingThrow_) işaretleyip sweep koşar. YALNIZCA instruction sınırında
    // çağrılmalı — opcode ortasında slot'a bağlanmamış nesne toplanabilir.
    void maybeCollect();

    static constexpr int kGCDefaultThreshold = 1024;
    int gcInitialThreshold_ = kGCDefaultThreshold;
    int gcThreshold_        = kGCDefaultThreshold; // bir sonraki tetikleme eşiği

    std::set<Capability>     caps_;       // ADR-035 (#76): açık capability'ler
    std::vector<std::string> programArgs_; // #90: `--` sonrası argümanlar

    // Faz 5: bütçe/step kısıtlarını kontrol eder, true = durmalı
    bool shouldStop();

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
