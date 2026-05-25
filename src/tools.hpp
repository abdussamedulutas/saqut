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
//   Şu anda sadece padRight() içerir.
//
// ============================================================================

#ifndef SAQUT_TOOLS
#define SAQUT_TOOLS

#include <string>

// --------------------------------------------------------------------------
// padRight: String'i sağdan boşluk ile belirtilen uzunluğa tamamla.
//
// KULLANIM: AST ağacını konsola yazdırırken girintileme (indent) için.
//   padRight("", indent) → indent adet boşluk döndürür.
//
// ÖRNEK:
//   padRight("", 4) → "    "
//   padRight("abc", 6) → "abc   "
//
// NOT: std::setw + std::left ile de yapılabilirdi, ancak bu daha basit.
// --------------------------------------------------------------------------
inline std::string padRight(std::string str, size_t totalLen) {
    if (str.size() < totalLen) {
        str.append(totalLen - str.size(), ' ');
    }
    return str;
}

#endif // SAQUT_TOOLS
