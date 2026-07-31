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

#include <cstdio>
#include <cstdlib>
#include <ostream>
#include <string>
#include <unistd.h>

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
//
// TTY FARKINDALIĞI:
//   Renkler yalnız stdout gerçek bir terminal ise üretilir. Pipe, redirect
//   veya dosyaya giden çıktıda ANSI CSI byte'ı bırakılmaz — `saqut ast > f`
//   ya da `saqut symbols | grep` çıktısı temiz metindir. Semantic içerik
//   (metin, hizalama, boşluk) TTY durumundan etkilenmez; yalnız renk kod
//   noktaları eklenir veya çıkarılır.
//
//   `NO_COLOR` ortam değişkeni (https://no-color.org) set edilmişse — değeri
//   ne olursa olsun — renk üretilmez. Terminalde de zorla kapatmak için:
//       NO_COLOR=1 saqut ast file:x.sqt
//
//   Bu kontrol #141'de yalnız `saqut ir` için IrColor:: altında yapılmıştı;
//   ast/symbols/parser log() çıktıları TTY'den bağımsız renk basıyordu.
//   Kontrol artık paylaşılan Color:: katmanında — bütün komutlar aynı
//   sözleşmeye tabi. IrColor:: geriye dönük uyumluluk için duruyor ve aynı
//   kararı devralır.
// --------------------------------------------------------------------------
namespace Color {

// stdout TTY mi ve NO_COLOR set değil mi? Bir kez hesaplanır (static).
inline bool enabled() {
    static const bool v = (std::getenv("NO_COLOR") == nullptr)
                       && (isatty(fileno(stdout)) != 0);
    return v;
}

namespace raw {
    inline const char* Reset       = "\033[0m";
    inline const char* Bold        = "\033[1m";
    inline const char* SoftMavi    = "\033[38;2;140;185;225m";   // node/ad tipleri
    inline const char* SoftYesil   = "\033[38;2;165;205;165m";   // değişken/isimler
    inline const char* SoftTuruncu = "\033[38;2;225;185;145m";   // sayısal değerler
    inline const char* SoftMor     = "\033[38;2;185;165;220m";   // opcode/operator
    inline const char* SoftPembe   = "\033[38;2;225;175;195m";   // tip isimleri/string
    inline const char* SoftTurkuaz = "\033[38;2;155;205;205m";   // konum/referans
    inline const char* SoftGri     = "\033[38;2;145;145;155m";   // parantez/etiket
    inline const char* KoyuSari    = "\033[38;2;200;180;80m";    // CFG metadata (blok aralığı, preds/succs/term etiketleri)
    inline const char* Kirmizi     = "\033[38;2;235;120;120m";   // CALLHOST/FFI (dış dünya çağrıları)
}

// Çağrı yerlerinin `Color::SoftMavi` yazımını koruyan TTY-farkında sarmalayıcı.
// const char*'a örtük dönüşür; `<<` ile doğrudan kullanılabilir.
struct Ink {
    const char* code;
    operator const char*() const { return enabled() ? code : ""; }
};

inline const Ink Reset       { raw::Reset       };
inline const Ink Bold        { raw::Bold        };
inline const Ink SoftMavi    { raw::SoftMavi    };
inline const Ink SoftYesil   { raw::SoftYesil   };
inline const Ink SoftTuruncu { raw::SoftTuruncu };
inline const Ink SoftMor     { raw::SoftMor     };
inline const Ink SoftPembe   { raw::SoftPembe   };
inline const Ink SoftTurkuaz { raw::SoftTurkuaz };
inline const Ink SoftGri     { raw::SoftGri     };
inline const Ink KoyuSari    { raw::KoyuSari    };
inline const Ink Kirmizi     { raw::Kirmizi     };

}  // namespace Color

// std::ostream << Ink ve std::string + Ink — örtük const char* dönüşümü bu iki
// bağlamda aşırı yükleme çözümlemesine takıldığı için açık operatörler.
inline std::ostream& operator<<(std::ostream& os, const Color::Ink& ink) {
    return os << static_cast<const char*>(ink);
}
inline std::string operator+(const std::string& s, const Color::Ink& ink) {
    return s + static_cast<const char*>(ink);
}
inline std::string operator+(const Color::Ink& ink, const std::string& s) {
    return static_cast<const char*>(ink) + s;
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
