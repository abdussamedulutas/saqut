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
    void setText(const std::string& path, const std::string& source) {
        filePath = path;
        text = source;
        computeLineStarts();
    }

    // Kaynak kodun toplam satır sayısı
    int lineCount() const {
        return static_cast<int>(lineStarts.size());
    }

    // Belirtilen offset'teki satırın tam metnini döndür
    std::string getLine(int line) const {
        if (line < 1 || line > lineCount()) return "";
        int start = lineStarts[line - 1];
        int end;
        if (line < lineCount()) {
            end = lineStarts[line] - 1;  // Satır sonu (\n) hariç
            // \r\n varsa bir karakter daha geri
            if (end > start && text[end - 1] == '\r') end--;
        } else {
            end = static_cast<int>(text.length());
        }
        return text.substr(start, end - start);
    }

    // Offset'ten (line, column) dönüşümü
    // Binary search ile O(log n)
    SourceLocation offsetToLocation(int offset) const {
        // Geçersiz offset kontrolü
        if (offset < 0 || offset > static_cast<int>(text.length())) {
            return SourceLocation{filePath, 0, 0, -1};
        }

        // Binary search: offset'in hangi satıra ait olduğunu bul
        // lineStarts içinde offset'ten büyük ilk elemanı bul
        auto it = std::upper_bound(lineStarts.begin(), lineStarts.end(), offset);
        int lineIndex = static_cast<int>(it - lineStarts.begin()) - 1;

        // lineIndex geçerli değilse
        if (lineIndex < 0) {
            lineIndex = 0;
        } else if (lineIndex >= static_cast<int>(lineStarts.size())) {
            lineIndex = static_cast<int>(lineStarts.size()) - 1;
        }

        int lineStart = lineStarts[lineIndex];
        int line = lineIndex + 1;       // 1-tabanlı
        int column = offset - lineStart + 1;  // 1-tabanlı

        return SourceLocation{filePath, line, column, offset};
    }

    // Bir aralığın başlangıç ve bitiş konumlarını döndür
    struct LocationRange {
        SourceLocation start;
        SourceLocation end;
    };

    LocationRange rangeFromOffsets(int startOffset, int endOffset) const {
        return {offsetToLocation(startOffset), offsetToLocation(endOffset)};
    }

private:
    // lineStarts vektörünü hesapla
    // Her \n karakterinden sonraki offset bir sonraki satırın başlangıcıdır
    // İlk satır her zaman offset 0'dan başlar
    void computeLineStarts() {
        lineStarts.clear();
        lineStarts.push_back(0);  // 1. satır offset 0

        for (int i = 0; i < static_cast<int>(text.length()); i++) {
            if (text[i] == '\n') {
                // \r\n kontrolü: \r'yi atla, \n'den sonraki karakter yeni satır
                int nextStart = i + 1;
                if (nextStart < static_cast<int>(text.length())) {
                    lineStarts.push_back(nextStart);
                }
            }
        }
    }
};

#endif // SAQUT_CORE_SOURCEFILE
