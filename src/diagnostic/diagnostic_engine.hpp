// ============================================================================
// saQut Compiler — Tanılama Motoru (DiagnosticEngine)
// ============================================================================
//
// DİZİN:   src/diagnostic/diagnostic_engine.hpp
// KATMAN:  Katman 0 — Tüm analiz katmanları tarafından kullanılır
// BAĞIMLI: src/diagnostic/diagnostic.hpp
// KULLANAN: sembol toplayıcı (Faz 2), tip denetleyici (Faz 3), pipeline (main)
//
// AMAÇ:
//   Derleme boyunca üretilen tüm Diagnostic'leri EKLENME SIRASIYLA biriktirir.
//   İlk hatada DURMAZ (ADR-013): bütün hatalar toplanır, faz sonunda topluca
//   raporlanır; durdurma kararını pipeline verir (hasErrors()).
//
//   İki çıktı yüzü vardır — aynı veriden:
//     printAll() → insan-okur (terminal)
//     toJson()   → makine-okur (LSP / AI / araçlar)
//
// ============================================================================

#ifndef SAQUT_DIAGNOSTIC_ENGINE
#define SAQUT_DIAGNOSTIC_ENGINE

#include <string>
#include <vector>
#include <ostream>
#include "diagnostic/diagnostic.hpp"

// ============================================================================
// DiagnosticEngine
// ============================================================================
//
// KULLANIM:
//   DiagnosticEngine diag;
//   diag.report(makeDiagnostic("E001", loc, "x tanımsız"));
//   diag.report(DiagLevel::Warning, "W001", loc2, "y kullanılmıyor");
//   if (diag.hasErrors()) diag.printAll(std::cerr);
// ============================================================================

class DiagnosticEngine {
public:
    // --- Ekleme ---
    void report(const Diagnostic& d) {
        diagnostics_.push_back(d);
    }

    // Kolaylık: koddan üret + ekle (seviye kataloğdan çözülür)
    void report(const std::string& code,
                const SourceLocation& loc,
                const std::string& message,
                const std::string& hint = "") {
        diagnostics_.push_back(makeDiagnostic(code, loc, message, hint));
    }

    // Kolaylık: seviyeyi açıkça vererek
    void report(DiagLevel level,
                const std::string& code,
                const SourceLocation& loc,
                const std::string& message,
                const std::string& hint = "") {
        Diagnostic d;
        d.level = level; d.code = code; d.loc = loc; d.message = message; d.hint = hint;
        diagnostics_.push_back(d);
    }

    // --- Sorgu ---
    bool hasErrors() const { return errorCount() > 0; }

    int errorCount() const { return countLevel(DiagLevel::Error); }
    int warningCount() const { return countLevel(DiagLevel::Warning); }
    int count() const { return static_cast<int>(diagnostics_.size()); }
    bool empty() const { return diagnostics_.empty(); }

    const std::vector<Diagnostic>& all() const { return diagnostics_; }

    void clear() { diagnostics_.clear(); }

    // --- İnsan-okur çıktı (ekleme sırasıyla) ---
    void printAll(std::ostream& os) const {
        for (const auto& d : diagnostics_) {
            os << d.loc.toString() << ": "
               << diagLevelNameTr(d.level) << " [" << d.code << "]: "
               << d.message << "\n";
            if (!d.hint.empty())
                os << "    ipucu: " << d.hint << "\n";
        }
        os << "— " << errorCount() << " hata, " << warningCount() << " uyarı\n";
    }

    // --- Makine-okur çıktı ---
    std::string toJson() const {
        std::string s = "{\"diagnostics\":[";
        for (size_t i = 0; i < diagnostics_.size(); ++i) {
            if (i) s += ",";
            s += diagnostics_[i].toJson();
        }
        s += "],\"errorCount\":" + std::to_string(errorCount());
        s += ",\"warningCount\":" + std::to_string(warningCount());
        s += "}";
        return s;
    }

private:
    std::vector<Diagnostic> diagnostics_;

    int countLevel(DiagLevel level) const {
        int n = 0;
        for (const auto& d : diagnostics_)
            if (d.level == level) ++n;
        return n;
    }
};

#endif // SAQUT_DIAGNOSTIC_ENGINE
