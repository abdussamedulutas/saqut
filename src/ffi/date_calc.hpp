// ============================================================================
// saQut FFI — Gregoryen Takvim Matematiği (#88, ADR-036)
// ============================================================================
//
// DİZİN:   src/ffi/date_calc.hpp
// BAĞIMLI: Yok (sadece standart kütüphane)
//
// AMAÇ:
//   date modülünün year/month/day/addDays gibi fonksiyonları için Gregoryen
//   takvim ↔ gün-sayısı dönüşümü. Howard Hinnant'ın kamu malı (public domain)
//   "chrono-compatible low-level date algorithms" makalesindeki
//   civil_from_days/days_from_civil algoritmalarının doğrudan uyarlaması —
//   harici bağımlılık yok (sıfır-toolchain kısıtı, ADR-032), tzdata gerekmez
//   (v1 yalnızca UTC, ADR-036).
//   http://howardhinnant.github.io/date_algorithms.html
//
// ============================================================================

#ifndef SAQUT_FFI_DATE_CALC
#define SAQUT_FFI_DATE_CALC

#include <cstdint>

namespace date_calc {

// 1970-01-01'den beri geçen gün sayısı → (yıl, ay 1-12, gün 1-31).
inline void civilFromDays(long long z, int& y, unsigned& m, unsigned& d) {
    z += 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);          // [0, 146096]
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; // [0, 399]
    const long long yr = static_cast<long long>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);          // [0, 365]
    const unsigned mp = (5 * doy + 2) / 153;                              // [0, 11]
    d = doy - (153 * mp + 2) / 5 + 1;                                     // [1, 31]
    m = mp < 10 ? mp + 3 : mp - 9;                                        // [1, 12]
    y = static_cast<int>(m <= 2 ? yr + 1 : yr);
}

// (yıl, ay 1-12, gün 1-31) → 1970-01-01'den beri geçen gün sayısı.
inline long long daysFromCivil(int y, unsigned m, unsigned d) {
    const long long yy = y - (m <= 2 ? 1 : 0);
    const long long era = (yy >= 0 ? yy : yy - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(yy - era * 400);           // [0, 399]
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;  // [0, 365]
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;           // [0, 146096]
    return era * 146097 + static_cast<long long>(doe) - 719468;
}

inline bool isLeapYear(int y) {
    return (y % 4 == 0) && (y % 100 != 0 || y % 400 == 0);
}

struct Broken {
    int y; unsigned mo, d, h, mi, s;
};

inline Broken breakDown(long long epochMs) {
    long long totalSec = epochMs / 1000;
    long long ms       = epochMs % 1000;
    if (ms < 0) { totalSec -= 1; }
    long long days   = totalSec >= 0 ? totalSec / 86400 : -((-totalSec + 86399) / 86400);
    long long secOfDay = totalSec - days * 86400;
    Broken b;
    civilFromDays(days, b.y, b.mo, b.d);
    b.h  = static_cast<unsigned>(secOfDay / 3600);
    b.mi = static_cast<unsigned>((secOfDay % 3600) / 60);
    b.s  = static_cast<unsigned>(secOfDay % 60);
    return b;
}

inline long long assemble(int y, unsigned mo, unsigned d, unsigned h, unsigned mi, unsigned s, unsigned ms = 0) {
    long long days = daysFromCivil(y, mo, d);
    return days * 86400000LL + (long long)h * 3600000LL + (long long)mi * 60000LL +
           (long long)s * 1000LL + ms;
}

} // namespace date_calc

#endif // SAQUT_FFI_DATE_CALC
