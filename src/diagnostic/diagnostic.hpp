// ============================================================================
// saQut Compiler — Tanılama (Diagnostic) Veri Yapıları + Hata Kataloğu
// ============================================================================
//
// DİZİN:   src/diagnostic/diagnostic.hpp
// KATMAN:  Katman 0 — Tüm analiz katmanları tarafından kullanılır
// BAĞIMLI: src/core/location.hpp
// KULLANAN: DiagnosticEngine, sembol toplayıcı (Faz 2), tip denetleyici (Faz 3)
//
// AMAÇ:
//   Derleme sırasında bulunan hata/uyarıları YAPISAL veri olarak temsil eder.
//   "Veri birincil, insan-okur metin bir görünümdür" (readme → Tasarım felsefesi):
//   bir Diagnostic; seviye + kod + konum + mesaj taşır; ekrana basılan satır
//   bunun yalnızca bir render'ıdır. Bu sayede aynı tanı LSP, AI veya `saqut
//   explain` tarafından da tüketilebilir.
//
// HATA KATALOĞU (baştan sabitlenir — yeni kodlar buraya eklenir):
//   E001  Tanımsız değişken/isim (declare-before-use ihlali dâhil)        Faz 2/3
//   E002  Aynı scope'ta çift tanım                                         Faz 2
//   E003  Tip uyuşmazlığı (gizli dönüşüm yok, ADR-010)                     Faz 3
//   E004  Döngü/switch dışı break/continue                                 Faz 3
//   E005  Fonksiyon dışı return                                           Faz 3
//   E006  Return tipi imzaya uymuyor                                       Faz 3
//   E007  Tanımsız tip (bilinmeyen tip adı)                               Faz 2/3
//   E008  Fonksiyon çağrısı argüman sayısı/tipi uyuşmuyor                  Faz 3
//   E009  Array boyutu sabit değil / geçersiz                             Faz 3
//   E010  Özyinelemeli/döngüsel struct (by-value çevrim → sonsuz boyut)   Faz 2/3
//   W001  Kullanılmayan değişken                                          Faz 4
//   W002  Sıfıra bölme (sabit folding)                                    Faz 4
//   W003  Erişilemez (ölü) kod                                            Faz 4
//
// ============================================================================

#ifndef SAQUT_DIAGNOSTIC_DIAGNOSTIC
#define SAQUT_DIAGNOSTIC_DIAGNOSTIC

#include <string>
#include <vector>
#include "core/location.hpp"
#include "tools.hpp"   // jsonEscape — TEK tanım (tools.hpp); çakışmayı önler

// ============================================================================
// DiagLevel — Tanı seviyesi
// ============================================================================

enum class DiagLevel { Error, Warning, Note, Hint };

inline const char* diagLevelName(DiagLevel l) {
    switch (l) {
        case DiagLevel::Error:   return "error";
        case DiagLevel::Warning: return "warning";
        case DiagLevel::Note:    return "note";
        case DiagLevel::Hint:    return "hint";
    }
    return "?";
}

// İnsan-okur çıktı için Türkçe karşılık
inline const char* diagLevelNameTr(DiagLevel l) {
    switch (l) {
        case DiagLevel::Error:   return "hata";
        case DiagLevel::Warning: return "uyarı";
        case DiagLevel::Note:    return "not";
        case DiagLevel::Hint:    return "ipucu";
    }
    return "?";
}

// NOT: jsonEscape() tools.hpp'de tanımlıdır (tek tanım — ODR çakışması olmaz).

// ============================================================================
// Diagnostic — Tek bir tanı (hata/uyarı/not/ipucu)
// ============================================================================
//
// KULLANIM:
//   Diagnostic d{DiagLevel::Error, "E003", loc, "int'e string atanamaz"};
//   d.hint = "açık dönüşüm gerekiyor";
//   std::cout << d.toJson();
// ============================================================================

struct Diagnostic {
    DiagLevel      level = DiagLevel::Error;
    std::string    code;     // "E003" (katalog kodu; boş olabilir)
    SourceLocation loc;      // hatanın kaynak koddaki yeri
    std::string    message;  // bağlama özel açıklama
    std::string    hint;     // opsiyonel "şunu dene" önerisi

    nlohmann::json toJsonObj() const {
        nlohmann::json j;
        j["level"]    = diagLevelName(level);
        j["code"]     = code;
        j["location"] = loc.toJsonObj();
        j["message"]  = message;
        if (!hint.empty()) j["hint"] = hint;
        return j;
    }

    std::string toJson() const { return toJsonObj().dump(); }
};

// ============================================================================
// Hata Kataloğu — kod → (seviye, kanonik başlık)
// ============================================================================
//
// Bağlama özel mesaj report sırasında verilir; buradaki başlık, kodun GENEL
// anlamıdır (ileride `saqut explain E003` bunu kullanabilir, #107/#98).
// ============================================================================

struct DiagInfo {
    const char* code;
    DiagLevel   level;
    const char* title;
};

inline const std::vector<DiagInfo>& diagnosticCatalog() {
    static const std::vector<DiagInfo> catalog = {
        {"E001", DiagLevel::Error,   "Tanımsız değişken/isim"},
        {"E002", DiagLevel::Error,   "Aynı scope'ta çift tanım"},
        {"E003", DiagLevel::Error,   "Tip uyuşmazlığı"},
        {"E004", DiagLevel::Error,   "Döngü/switch dışı break/continue"},
        {"E005", DiagLevel::Error,   "Fonksiyon dışı return"},
        {"E006", DiagLevel::Error,   "Return tipi imzaya uymuyor"},
        {"E007", DiagLevel::Error,   "Tanımsız tip"},
        {"E008", DiagLevel::Error,   "Fonksiyon çağrısı argümanı uyuşmuyor"},
        {"E009", DiagLevel::Error,   "Array boyutu sabit değil / geçersiz"},
        {"E010", DiagLevel::Error,   "Özyinelemeli/döngüsel struct tanımı"},
        {"W001", DiagLevel::Warning, "Kullanılmayan değişken"},
        {"W002", DiagLevel::Warning, "Sıfıra bölme (sabit ifade)"},
        {"W003", DiagLevel::Warning, "Erişilemez (ölü) kod"},
        {"W004", DiagLevel::Warning, "Örtük sayısal genişletme (widening)"},
    };
    return catalog;
}

// Kod kataloğda var mı? (yoksa nullptr)
inline const DiagInfo* findDiag(const std::string& code) {
    for (const auto& d : diagnosticCatalog())
        if (code == d.code) return &d;
    return nullptr;
}

// Bir koddan Diagnostic üretir; seviye kataloğdan çözülür (yoksa: E→Error,
// W→Warning, diğer→Note). Bağlama özel mesajı çağıran verir.
inline Diagnostic makeDiagnostic(const std::string& code,
                                 const SourceLocation& loc,
                                 const std::string& message,
                                 const std::string& hint = "") {
    DiagLevel level = DiagLevel::Note;
    if (const DiagInfo* info = findDiag(code)) {
        level = info->level;
    } else if (!code.empty()) {
        if (code[0] == 'E') level = DiagLevel::Error;
        else if (code[0] == 'W') level = DiagLevel::Warning;
    }
    Diagnostic d;
    d.level   = level;
    d.code    = code;
    d.loc     = loc;
    d.message = message;
    d.hint    = hint;
    return d;
}

#endif // SAQUT_DIAGNOSTIC_DIAGNOSTIC
