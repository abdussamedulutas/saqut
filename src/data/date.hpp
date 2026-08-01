// ============================================================================
// saQut — date veri tipi (arayüz)
// ============================================================================
//
// DİZİN:   src/data/date.hpp
// KATMAN:  data — date'in çalışma zamanındaki TEK sahibi
//
// BELLEK TEMSİLİ (#88, ADR-036):
//   date = UTC epoch-ms, int64. Value::int64Value'da (VM), HostKind::Date'te
//   (sınır), MIR_T_I64 register'ında (JIT) — üç katmanda da AYNI sayı.
//   Kutulama yok, heap yok, GC yok.
//
// KAPSAM: buradaki fonksiyonlar SAF hesaptır (takvim, aritmetik, ayrıştırma).
//   Sistem saatini okuyan now() BURAYA AİT DEĞİLDİR — o gerçekten dış dünyadır
//   ve FFI'da kalır (src/ffi/host_functions.cpp, --allow-sys capability'si).
//
// ============================================================================

#ifndef SAQUT_DATA_DATE
#define SAQUT_DATA_DATE

#include <vector>

#include "ffi/host_abi.hpp"

// Bir date host fonksiyonu: sembolik id + gövde.
// (Built-in metod DEĞİL — root.sqt'teki `ffi` bildirimleriyle eşleşir.)
struct DateFn {
    const char* symbolicId;
    HostThunk   thunk;
};

// date'in saf fonksiyonları: fromEpochMillis, toEpochMillis, addDays/Hours/
// Minutes/Seconds, year, month, day, hour, minute, second, diffMillis,
// parse, format.
//
// now() burada YOK — o FFI'da kalır (sistem saati = gerçek dış dünya).
const std::vector<DateFn>& dataDateFunctions();

#endif // SAQUT_DATA_DATE
