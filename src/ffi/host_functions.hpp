// ============================================================================
// saQut FFI — Host Fonksiyon Registry (Sayısal Dispatch)
// ============================================================================
//
// DİZİN:   src/ffi/host_functions.hpp
// KATMAN:  FFI — gömülü (C++ gövdeli) host fonksiyonlarının tek kaynağı
//
// AMAÇ (ADR-034, #107):
//   Gömülü root.sqt'teki `ffi ... : HOST_ID from mod` bildirimleri sembolik
//   HOST_ID taşır. Bu registry HOST_ID → sayısal index eşlemesini (derleme
//   zamanı) ve index → C++ gövde dispatch'ini (runtime, O(1)) sağlar.
//
//   Sayılar TABLONUN SIRASINDAN gelir; root.sqt ham sayı yazmaz, sembolik ad
//   kullanır → hostFnIndex ile çözülür. Ad tabloda yoksa (drift) symbol
//   collector hata verir.
//
// ============================================================================

#ifndef SAQUT_FFI_HOST_FUNCTIONS
#define SAQUT_FFI_HOST_FUNCTIONS

#include <string>
#include <vector>
#include "vm/value.hpp"

// Tek bir host fonksiyon kaydı. impl saf (heap/throw gerektirmeyen) fonksiyonlar
// içindir; fs/sys gibi capability'li fonksiyonlar geldiğinde imza genişletilecek
// (bkz. TODO). Şimdilik math (saf) kapsamı.
struct HostFn {
    const char* symbolicId;                                  // "MATH_SQRT"
    int         arity;                                       // beklenen argüman sayısı
    Value     (*impl)(const std::vector<Value>& args);       // C++ gövde
};

// Tüm host fonksiyonların düz tablosu. Index = sayısal host id (root.sqt'e
// gömülmez; hostFnIndex ile çözülür).
const std::vector<HostFn>& hostFnTable();

// Sembolik id → sayısal index. Bulunamazsa -1 (root.sqt ↔ C++ drift kontrolü).
int hostFnIndex(const std::string& symbolicId);

// id ile host fonksiyonu çağır. id geçersizse Value::null() döner.
Value callHostFn(int id, const std::vector<Value>& args);

#endif // SAQUT_FFI_HOST_FUNCTIONS
