// ============================================================================
// saQut Profiling — StageTimer (Aşama Başına Süre Ölçümü)
// ============================================================================
//
// DİZİN:   src/profiling/stage_timer.hpp
// KATMAN:  Çapraz kesit — CLI'nin (`--profile`) pipeline aşamalarını
//          (token/parser/ir-gen/vm-veya-jit) ölçmesi için kullandığı
//          minimal araç.
//
// AMAÇ:
//   Bu dizin ileride büyümeye aday (opcode-seviyesi profil, bellek
//   izleme, JIT derleme-süresi kırılımı vb. — bkz. mevcut `src/bench/`
//   ile karışmasın: bench.hpp N-tekrarlı istatistiksel ölçüm + opcode
//   trace analiz eder, StageTimer TEK bir koşuda "hangi aşama ne kadar
//   sürdü" sorusuna cevap veren, `saqut run --profile` için düz bir
//   araçtır). Bugün yalnızca ana pipeline aşamalarını ölçüyor.
//
// KULLANIM:
//   profiling::StageTimer timer;
//   {
//       profiling::StageTimer::ScopedStage _(&timer, "token");
//       // ... tokenize ...
//   }
//   timer.printReport(std::cerr);
//
//   `timer` işaretçisi nullptr ise ScopedStage HİÇBİR ŞEY yapmaz (ölçüm
//   istenmediğinde saat çağrısı maliyeti bile yok) — çağıran taraf
//   `args.profile ? &timer : nullptr` deseniyle koşulsuz enjekte edebilir.
// ============================================================================

#ifndef SAQUT_PROFILING_STAGE_TIMER
#define SAQUT_PROFILING_STAGE_TIMER

#include <chrono>
#include <iomanip>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace profiling {

class StageTimer {
public:
    using Clock = std::chrono::steady_clock;

    // Aynı isimle birden çok kez çağrılabilir (ör. çok-modüllü derlemede
    // her dosya için ayrı ayrı tokenize/parse) — süreler isim başına toplanır.
    void add(const std::string& stage, long long microseconds) {
        auto it = totals_.find(stage);
        if (it == totals_.end()) {
            order_.push_back(stage);
            totals_.emplace(stage, microseconds);
        } else {
            it->second += microseconds;
        }
    }

    long long microsecondsFor(const std::string& stage) const {
        auto it = totals_.find(stage);
        return it == totals_.end() ? 0 : it->second;
    }

    void printReport(std::ostream& out) const {
        out << "[profil] Asama sureleri:\n";
        long long total = 0;
        for (const auto& name : order_) {
            long long us = totals_.at(name);
            total += us;
            out << "  " << std::left << std::setw(14) << name
                << std::right << std::setw(10) << (us / 1000.0) << " ms\n";
        }
        out << "  " << std::left << std::setw(14) << "toplam"
            << std::right << std::setw(10) << (total / 1000.0) << " ms\n";
    }

    // RAII: yapıcıda saat başlar, yıkıcıda timer->add(stage, gecen_sure)
    // çağrılır. timer == nullptr ise tamamen no-op (saat bile okunmaz).
    class ScopedStage {
    public:
        ScopedStage(StageTimer* timer, std::string stage)
            : timer_(timer), stage_(std::move(stage)),
              start_(timer_ ? Clock::now() : Clock::time_point{}) {}

        ScopedStage(const ScopedStage&)            = delete;
        ScopedStage& operator=(const ScopedStage&) = delete;

        ~ScopedStage() {
            if (!timer_) return;
            auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                           Clock::now() - start_)
                           .count();
            timer_->add(stage_, us);
        }

    private:
        StageTimer*        timer_;
        std::string         stage_;
        Clock::time_point   start_;
    };

private:
    std::vector<std::string>                  order_;   // ilk görülme sırası (rapor sırası)
    std::unordered_map<std::string, long long> totals_;
};

}  // namespace profiling

#endif  // SAQUT_PROFILING_STAGE_TIMER
