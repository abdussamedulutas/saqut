// ============================================================================
// saQut Compiler — Kaynak Kod Yöneticisi
// ============================================================================
//
// DİZİN:   src/core/sourcefile.hpp
// KATMAN:  Katman 0 — Tüm katmanlar tarafından kullanılır
// BAĞIMLI: core/location.hpp
//
// AMAÇ:
//   Kaynak kodun tamamını ve satır başı offset'lerini tutar.
//   offset → (line, column) dönüşümü yapar.
//
// TASARIM:
//   lineStarts vektörü, her satırın ilk karakterinin offset'ini tutar:
//     lineStarts[0] = 0   (1. satır, offset 0)
//     lineStarts[1] = 15  (2. satır, offset 15)
//     lineStarts[2] = 32  (3. satır, offset 32)
//
//   offsetToLocation() bu dizide binary search yaparak O(log n)'de line/column
//   bulur. Line-start dizisi bir kere setText()'te O(n)'de hesaplanır.
//
// PERFORMANS:
//   setText()  : O(n) — line-start dizisi bir kere kurulur
//   offsetToLocation() : O(log n) — binary search
//   Bellek     : O(n) — lineStarts (en fazla n eleman, her satır için bir int)
//
// ============================================================================

#ifndef SAQUT_CORE_SOURCEFILE
#define SAQUT_CORE_SOURCEFILE

#include <algorithm>
#include <string>
#include <vector>
#include "core/location.hpp"

// ============================================================================
// SourceFile — Kaynak Kod Yöneticisi
// ============================================================================
//
// KULLANIM:
//   SourceFile sf;
//   sf.setText("deneme.sqt", "int x = 5;\nreturn x;\n");
//   SourceLocation loc = sf.offsetToLocation(10);  // 1:10 (2. satır)
//
// ============================================================================

class SourceFile {
public:
    std::string filePath;         // Kaynak dosyanın yolu
    std::string text;             // Kaynak kodun tamamı
    std::vector<int> lineStarts;  // Her satırın başlangıç offset'i

    SourceFile() = default;

    // text verisini yeni satır dizisini de hesapla
    void setText(const std::string& path, const std::string& source);

    // Kaynak kodun toplam satır sayısı
    int lineCount() const;

    // Belirtilen offset'teki satırın tam metnini döndür
    std::string getLine(int line) const;

    // Offset'ten (line, column) dönüşümü
    // Binary search ile O(log n)
    SourceLocation offsetToLocation(int offset) const;

    // Bir aralığın başlangıç ve bitiş konumlarını döndür
    struct LocationRange {
        SourceLocation start;
        SourceLocation end;
    };

    LocationRange rangeFromOffsets(int startOffset, int endOffset) const;

private:
    // lineStarts vektörünü hesapla
    // Her \n karakterinden sonraki offset bir sonraki satırın başlangıcıdır
    // İlk satır her zaman offset 0'dan başlar
    void computeLineStarts();
};

#endif // SAQUT_CORE_SOURCEFILE
