// ============================================================================
// saQut Compiler — Yardımcı Fonksiyonlar
// ============================================================================
//
// DİZİN:   src/tools.hpp
// KATMAN:  Tüm katmanlar tarafından kullanılabilir
// BAĞIMLI: Yok (sadece <string>)
//
// AMAÇ:
//   Tüm derleyici modüllerinin ihtiyaç duyduğu ortak yardımcı fonksiyonlar.
//
// ============================================================================

#ifndef SAQUT_TOOLS
#define SAQUT_TOOLS

#include <string>

// --------------------------------------------------------------------------
// padRight: String'i sağdan boşluk ile belirtilen uzunluğa tamamla.
// --------------------------------------------------------------------------
inline std::string padRight(std::string str, size_t totalLen) {
    if (str.size() < totalLen) {
        str.append(totalLen - str.size(), ' ');
    }
    return str;
}

// --------------------------------------------------------------------------
// jsonIndent: JSON çıktısı için girinti (her seviye 2 boşluk)
// --------------------------------------------------------------------------
inline std::string jsonIndent(int n) {
    return std::string(static_cast<size_t>(n) * 2, ' ');
}

// --------------------------------------------------------------------------
// jsonEscape: JSON string değerleri için kaçış karakterleri
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
// Pastel renk sabitleri — ANSI true color (göze batmayan, pastel tonları)
// Kullanım: std::cout << Color::SoftMavi << "metin" << Color::Reset;
// --------------------------------------------------------------------------
namespace Color {
    inline const char* Reset       = "\033[0m";
    inline const char* Bold        = "\033[1m";
    inline const char* SoftMavi    = "\033[38;2;140;185;225m";   // node/ad tipleri
    inline const char* SoftYesil   = "\033[38;2;165;205;165m";   // değişken/isimler
    inline const char* SoftTuruncu = "\033[38;2;225;185;145m";   // sayısal değerler
    inline const char* SoftMor     = "\033[38;2;185;165;220m";   // opcode/operator
    inline const char* SoftPembe   = "\033[38;2;225;175;195m";   // tip isimleri/string
    inline const char* SoftTurkuaz = "\033[38;2;155;205;205m";   // konum/referans
    inline const char* SoftGri     = "\033[38;2;145;145;155m";   // parantez/etiket
}

inline std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;
        }
    }
    return out;
}

#endif // SAQUT_TOOLS
