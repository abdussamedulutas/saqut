// ============================================================================
// saQut Compiler — Kaynak Kod Konum Yapısı
// ============================================================================
//
// DİZİN:   src/core/location.hpp
// KATMAN:  Katman 0 — Tüm katmanlar tarafından kullanılır
// BAĞIMLI: Yok (sadece <string>)
//
// AMAÇ:
//   Her token ve AST düğümünün kaynak koddaki tam konumunu tutar.
//   "Hata nerede?" ve "Kullanıcı imleci nerede?" sorularına cevap verir.
//
// ALANLAR:
//   filePath : Kaynak dosyanın yolu (bilinmiyorsa boş string)
//   line     : 1-tabanlı satır numarası
//   column   : 1-tabanlı sütun numarası
//   offset   : 0-tabanlı karakter offset'i (dosya başından itibaren)
//
// ============================================================================

#ifndef SAQUT_CORE_LOCATION
#define SAQUT_CORE_LOCATION

#include <string>
#include "vendor/nlohmann/json.hpp"

// ============================================================================
// SourceLocation — Kaynak Koddaki Bir Nokta
// ============================================================================
//
// KULLANIM:
//   SourceLocation loc{"test.sqt", 5, 10, 134};
//   std::cout << loc.toString();  // "test.sqt:5:10"
//   std::cout << loc.shortString(); // "5:10"
//
//   Varsayılan kurucu: geçersiz bir konum üretir (line=0, column=0, offset=-1).
//   isValid() ile kontrol edilebilir.
//
// ============================================================================

struct LspPosition {
    int line;      // 0-tabanlı
    int character; // 0-tabanlı
};

struct LspRange {
    LspPosition start;
    LspPosition end;
};

struct SourceLocation {
    std::string filePath;
    int line   = 0;    // 1-tabanlı, 0 = geçersiz
    int column = 0;    // 1-tabanlı, 0 = geçersiz
    int offset = -1;   // 0-tabanlı, -1 = geçersiz

    SourceLocation() = default;

    SourceLocation(std::string file, int line, int col, int off)
        : filePath(std::move(file)), line(line), column(col), offset(off) {}

    // Geçerli bir konum mu?
    bool isValid() const {
        return line > 0 && column > 0 && offset >= 0;
    }

    // Tam konum: "dosya.sqt:5:10"
    std::string toString() const {
        if (!isValid()) return "<invalid>";
        return filePath + ":" + std::to_string(line) + ":" + std::to_string(column);
    }

    // Kısa konum: "5:10"
    std::string shortString() const {
        if (!isValid()) return "?:?";
        return std::to_string(line) + ":" + std::to_string(column);
    }

    // JSON formatı: {"file":"...","line":5,"column":10,"offset":134}
    nlohmann::json toJsonObj() const {
        if (!isValid()) return nullptr;
        return {
            {"file",   filePath},
            {"line",   line},
            {"column", column},
            {"offset", offset}
        };
    }

    std::string toJson() const { return toJsonObj().dump(); }

    LspPosition toLspPosition() const {
        return { line > 0 ? line - 1 : 0,
                 column > 0 ? column - 1 : 0 };
    }
};

#endif // SAQUT_CORE_LOCATION
