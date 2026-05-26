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
