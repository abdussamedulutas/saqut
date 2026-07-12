// ============================================================================
// saQut LSP — Pozisyon Dönüştürücüleri (UTF-16 ↔ byte)
// ============================================================================

#ifndef SAQUT_LSP_POSITION
#define SAQUT_LSP_POSITION

#include <string>
#include <vector>

// LSP pozisyonları öntanımlı olarak UTF-16 code unit sayar; saQut derleyicisi
// içeride byte (UTF-8 code unit) offset/column kullanır (bkz.
// core/sourcefile.hpp — SourceLocation.column = offset - lineStart + 1, ham
// byte sayımı). İstemci "utf-8" positionEncoding'i desteklemiyorsa (LSP 3.17
// varsayılanı budur), bu iki birim arasında satır bazlı dönüşüm gerekir.
// Faz 3 (docs/prompt-lsp-dap-kurtarma.md), kök neden #4.
//
// Tüm handler'lar konum çevirisini BU dosyadaki yardımcılardan geçirmeli —
// elle +1/-1 hesabı kalmamalı.

// İçerikteki her satırın başlangıç byte offset'ini bir kez çıkarır (satır
// indeksi). lspLineText her çağrıda dosya başından tarar (O(dosya boyutu));
// sembol/tanı başına çağrılan döngülerde bu kuadratik patlar (90K satırlık
// dosyada documentSymbol dakikalarca CPU yakıyordu). Belge başına bir kez
// kur (DocumentState.lineStarts), döngülerde *At varyantlarını kullan.
inline std::vector<int> buildLineStarts(const std::string& content) {
    std::vector<int> starts;
    starts.push_back(0);
    for (size_t i = 0; i < content.size(); ++i)
        if (content[i] == '\n') starts.push_back(static_cast<int>(i) + 1);
    return starts;
}

// buildLineStarts indeksiyle O(satır uzunluğu) satır metni (\n / \r\n HARİÇ).
inline std::string lspLineTextAt(const std::string& content,
                                 const std::vector<int>& starts, int line) {
    if (line < 0 || line >= static_cast<int>(starts.size())) return "";
    size_t s = static_cast<size_t>(starts[line]);
    size_t e = (line + 1 < static_cast<int>(starts.size()))
                   ? static_cast<size_t>(starts[line + 1]) - 1  // '\n' hariç
                   : content.size();
    if (e > s && content[e - 1] == '\r') --e;
    return content.substr(s, e - s);
}

// content içindeki 0-tabanlı `line`'a karşılık gelen satırın metnini
// döndürür (satır sonu \n / \r\n HARİÇ).
inline std::string lspLineText(const std::string& content, int line) {
    size_t i = 0;
    int cur = 0;
    while (cur < line && i < content.size()) {
        if (content[i] == '\n') ++cur;
        ++i;
    }
    size_t start = i;
    size_t end = content.find('\n', start);
    if (end == std::string::npos) end = content.size();
    if (end > start && content[end - 1] == '\r') --end;
    return content.substr(start, end - start);
}

// content içinde 0-tabanlı `line`'ın byte offset'ini (satır başı) döndürür.
inline int lspLineStartOffset(const std::string& content, int line) {
    size_t i = 0;
    int cur = 0;
    while (cur < line && i < content.size()) {
        if (content[i] == '\n') ++cur;
        ++i;
    }
    return static_cast<int>(i);
}

// UTF-8 öncü baytından code point uzunluğunu (1-4) çıkarır.
inline int utf8SeqLen(unsigned char c) {
    if ((c & 0x80) == 0x00) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1; // geçersiz/devam baytı — tek baytmış gibi ilerle (bozuk girdiye dayanıklı)
}

// lineText içinde verilen UTF-16 code unit sayısına (0-tabanlı `utf16Char`)
// karşılık gelen byte offset'ini (0-tabanlı) döndürür.
inline int utf16ToByteOffset(const std::string& lineText, int utf16Char) {
    int units = 0;
    size_t i = 0;
    while (i < lineText.size() && units < utf16Char) {
        unsigned char c = static_cast<unsigned char>(lineText[i]);
        int len = utf8SeqLen(c);
        if (i + static_cast<size_t>(len) > lineText.size()) len = 1;
        int codepoint = (len == 1) ? c : (c & (0xFF >> (len + 1)));
        for (int k = 1; k < len && i + static_cast<size_t>(k) < lineText.size(); ++k)
            codepoint = (codepoint << 6) | (static_cast<unsigned char>(lineText[i + k]) & 0x3F);
        units += (codepoint > 0xFFFF) ? 2 : 1; // surrogate çift mi (BMP dışı)
        i += static_cast<size_t>(len);
    }
    return static_cast<int>(i);
}

// lineText içinde verilen byte offset'ine (0-tabanlı `byteOffset`) karşılık
// gelen UTF-16 code unit sayısını (0-tabanlı) döndürür.
inline int byteOffsetToUtf16(const std::string& lineText, int byteOffset) {
    if (byteOffset < 0) byteOffset = 0;
    if (byteOffset > static_cast<int>(lineText.size()))
        byteOffset = static_cast<int>(lineText.size());
    int units = 0;
    size_t i = 0;
    while (i < static_cast<size_t>(byteOffset)) {
        unsigned char c = static_cast<unsigned char>(lineText[i]);
        int len = utf8SeqLen(c);
        if (i + static_cast<size_t>(len) > lineText.size()) len = 1;
        int codepoint = (len == 1) ? c : (c & (0xFF >> (len + 1)));
        for (int k = 1; k < len && i + static_cast<size_t>(k) < lineText.size(); ++k)
            codepoint = (codepoint << 6) | (static_cast<unsigned char>(lineText[i + k]) & 0x3F);
        units += (codepoint > 0xFFFF) ? 2 : 1;
        i += static_cast<size_t>(len);
    }
    return units;
}

// content + 0-tabanlı LSP satırı + 0-tabanlı UTF-16 sütunu → 1-tabanlı byte
// sütunu (SourceLocation.column ile aynı birim).
inline int lspToByteCol(const std::string& content, int line, int utf16Col) {
    return utf16ToByteOffset(lspLineText(content, line), utf16Col) + 1;
}

// content + 0-tabanlı LSP satırı + 1-tabanlı byte sütunu → 0-tabanlı UTF-16
// sütunu (LSP `character` alanı ile aynı birim).
inline int byteColToLsp(const std::string& content, int line, int byteCol) {
    return byteOffsetToUtf16(lspLineText(content, line), byteCol - 1);
}

// byteColToLsp'nin satır-indeksli varyantı — döngü içinde çağıranlar için.
inline int byteColToLspAt(const std::string& content,
                          const std::vector<int>& starts, int line, int byteCol) {
    return byteOffsetToUtf16(lspLineTextAt(content, starts, line), byteCol - 1);
}

#endif // SAQUT_LSP_POSITION
