#include "core/sourcefile.hpp"

void SourceFile::setText(const std::string& path, const std::string& source) {
    filePath = path;
    text = source;
    computeLineStarts();
}

int SourceFile::lineCount() const {
    return static_cast<int>(lineStarts.size());
}

std::string SourceFile::getLine(int line) const {
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

SourceLocation SourceFile::offsetToLocation(int offset) const {
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

SourceFile::LocationRange SourceFile::rangeFromOffsets(int startOffset, int endOffset) const {
    return {offsetToLocation(startOffset), offsetToLocation(endOffset)};
}

void SourceFile::computeLineStarts() {
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
