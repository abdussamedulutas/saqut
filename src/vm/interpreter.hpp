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
#include "ffi/host_bridge.hpp"   // #222: HostCallScratch / HostRetOwner
#include "profiling/stage_timer.hpp"

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

    // src/profiling/ (--profile): nullptr = kapalı, sıfır maliyet. Set
    // edilirse "vm-warmup" (initForDebug — frame/global kurulumu) ve
    // "vm-exec" (runUntilEvent'in ANA döngüsü, yani VM'in gerçekten
    // instruction çalıştırdığı kısım) ayrı ayrı raporlanır.
    void setStageProfiler(profiling::StageTimer* p) { stageProfiler_ = p; }

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
    // #3 (2026-07-16): tek DÜZ global slot dizisi — LOAD_GLOBAL/STORE_GLOBAL
    // yürütülen fonksiyonun DEĞİL, IRGenerator'ın tüm programa yaydığı flat
    // indekse göre çalışıyor (nameToGlobal_ ADR-034 import-gated stdlib'den
    // önce de tek program-çapında sayaçtı). Önceki "moduleId → vector" haritası
    // çapraz-modül global okuma/yazmada frame.function->moduleId'yi kullanıyordu
    // — bildiren fonksiyonun DEĞİL çağıran fonksiyonun modülüne göre yanlış
    // diziye erişiyordu (export edilmiş global başka modülden okunduğunda
    // sessizce 0 dönüyordu).
    std::vector<Value>     globalSlots_;
    Heap                   heap_;
    // #206: struct alan adları tip başına bir kez, paylaşımlı metadata
    std::unordered_map<std::string, std::shared_ptr<std::vector<std::string>>>
                           structFieldNamesRegistry_;
    std::vector<TryFrame>  tryStack_;
    std::optional<Value>   pendingThrow_;
    BenchVMTrace*          vmTrace_ = nullptr;  // profil hook (bench modunda non-null)
    profiling::StageTimer* stageProfiler_ = nullptr;  // --profile hook
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

    // GC (#77): eşik aşıldıysa kökleri (globalSlots_ + callStack_ +
    // pendingThrow_) işaretleyip sweep koşar. YALNIZCA instruction sınırında
    // çağrılmalı — opcode ortasında slot'a bağlanmamış nesne toplanabilir.
    void maybeCollect();

    static constexpr int kGCDefaultThreshold = 1024;
    static constexpr int kGCBudgetPerStep = 128;  // #217: incremental step'te işlenecek max nesne
    int gcInitialThreshold_ = kGCDefaultThreshold;
    bool gcCycleActive_ = false;  // #217: incremental cycle devam ediyor mu?
    int gcThreshold_        = kGCDefaultThreshold; // bir sonraki tetikleme eşiği

    std::set<Capability>     caps_;       // ADR-035 (#76): açık capability'ler
    std::vector<std::string> programArgs_; // #90: `--` sonrası argümanlar

    // #222: host çağrı ABI'si — çağrılar arasında YENİDEN KULLANILIR.
    // Çağrı başına heap tahsisi yapmamanın yolu budur: scratch argüman
    // dönüşümünün, owner dönüş değerinin ömrünü taşır; ikisi de her
    // CALLHOST'ta reset edilir, yeniden tahsis edilmez.
    HostCallScratch hostScratch_;
    HostRetOwner    hostRetOwner_;
    // Frame de yeniden kullanılır: içinde HostError'ın iki std::string'i var
    // ve her CALLHOST'ta yeniden kurmak sıcak yolda ölçülebilir maliyetti.
    HostCallFrame   hostFrame_;

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

    // Mevcut callStack_'i gezerek stacktrace string'i üretir.
    // pendingThrow_ set edilmeden ÖNCE çağrılmalı (unwind olmadan).
    std::string buildTrace() const;
};

#endif // SAQUT_VM_INTERPRETER
