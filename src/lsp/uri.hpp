#ifndef SAQUT_LSP_URI
#define SAQUT_LSP_URI

#include <string>
#include <cctype>

// LSP `file://` URI'leri ile dosya sistemi yolları arasında dönüşüm.
// Faz 1: uriToPath tek yardımcıya çıkarıldı (önceden document_store.cpp
// içinde static'ti). pathToUri tersi — Faz 3'te çok-dosya URI üretimi için
// kullanılacak (definition/references sonucu sorgulanan URI'yi değil,
// tanımın bulunduğu dosyanın URI'sini döndürmeli).

inline std::string uriToPath(const std::string& uri) {
    std::string s = uri;
    if (s.rfind("file://", 0) == 0)
        s = s.substr(7);
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int hi = std::isdigit(static_cast<unsigned char>(s[i+1])) ? s[i+1]-'0' : std::tolower(s[i+1])-'a'+10;
            int lo = std::isdigit(static_cast<unsigned char>(s[i+2])) ? s[i+2]-'0' : std::tolower(s[i+2])-'a'+10;
            out += static_cast<char>(hi * 16 + lo);
            i += 2;
        } else {
            out += s[i];
        }
    }
    return out;
}

inline std::string pathToUri(const std::string& path) {
    static const char* hex = "0123456789ABCDEF";
    std::string out = "file://";
    for (unsigned char c : path) {
        bool safe = std::isalnum(c) || c == '/' || c == '.' || c == '-' ||
                    c == '_' || c == '~';
        if (safe) {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0xF];
        }
    }
    return out;
}

#endif // SAQUT_LSP_URI
