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
//   fileId   : Kaynak dosya kimliği (FileRegistry sıra numarası; 0 = bilinmiyor)
//   line     : 1-tabanlı satır numarası
//   column   : 1-tabanlı sütun numarası
//   offset   : 0-tabanlı karakter offset'i (dosya başından itibaren)
//
// Dosya YOLU burada saklanmaz — filePath() ile FileRegistry'den okunur.
// Gerekçe ve ölçüm: core/file_registry.hpp.
//
// ============================================================================

#ifndef SAQUT_CORE_LOCATION
#define SAQUT_CORE_LOCATION

#include <string>
#include "core/file_registry.hpp"
#include "vendor/nlohmann/json.hpp"

// ============================================================================
// SourceLocation — Kaynak Koddaki Bir Nokta
// ============================================================================
//
// KULLANIM:
//   SourceLocation loc{"test.sqt", 5, 10, 134};
//   std::cout << loc.toString();   // "test.sqt:5:10"
//   std::cout << loc.filePath();   // "test.sqt"  (FileRegistry'den)
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
    // Kaynak dosya kimliği — FileRegistry'deki sıra numarası (1'den başlar;
    // 0 = bilinmiyor). Dosya YOLU burada saklanmaz: SourceLocation her token'da
    // ve her AST düğümünde kopyalanır, yolu string olarak taşımak token başına
    // bir heap tahsisi demekti (bkz. core/file_registry.hpp'deki ölçüm).
    // Yola erişmek için filePath() kullanın — davranış aynıdır.
    int fileId = FileRegistry::UNKNOWN_ID;
    int line   = 0;    // 1-tabanlı, 0 = geçersiz
    int column = 0;    // 1-tabanlı, 0 = geçersiz
    int offset = -1;   // 0-tabanlı, -1 = geçersiz

    SourceLocation() = default;

    // Yol ile kurulum — yolu kaydeder ve kimliğini saklar.
    SourceLocation(const std::string& file, int line, int col, int off)
        : fileId(FileRegistry::instance().intern(file)),
          line(line), column(col), offset(off) {}

    // Kimlik ile kurulum — yol zaten kayıtlıysa kopyalama/arama olmaz.
    SourceLocation(int fileId, int line, int col, int off)
        : fileId(fileId), line(line), column(col), offset(off) {}

    // Kaynak dosya yolu. Kayıtlı değilse (veya hiç atanmadıysa) boş string —
    // filePath'in düz bir alan olduğu zamanki davranışla aynı.
    const std::string& filePath() const {
        return FileRegistry::instance().path(fileId);
    }

    // Yolu doğrudan atamak için (fileId'yi kaydeder). Eski `loc.filePath = p;`
    // yazımının karşılığı.
    void setFilePath(const std::string& p) {
        fileId = FileRegistry::instance().intern(p);
    }

    // Geçerli bir konum mu?
    bool isValid() const {
        return line > 0 && column > 0 && offset >= 0;
    }

    // Tam konum: "dosya.sqt:5:10"
    std::string toString() const {
        if (!isValid()) return "<invalid>";
        return filePath() + ":" + std::to_string(line) + ":" + std::to_string(column);
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
            {"file",   filePath()},
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
