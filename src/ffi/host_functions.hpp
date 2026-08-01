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

#include <set>
#include <string>
#include <vector>
#include "vm/value.hpp"
#include "vm/object.hpp"
#include "core/capability.hpp"
#include "ffi/host_abi.hpp"

// Tek bir host fonksiyon kaydı.
//
// #222: gövde artık HostThunk'tır — tüm host fonksiyonları ortak ABI'yi
// kullanır (host_abi.hpp). Eski HostContext ve Value-tabanlı imza kaldırıldı;
// VM durumuna erişim HostEnv üzerinden, hata dönüşü HostError üzerindendir.
struct HostFn {
    const char* symbolicId;   // "MATH_SQRT" — root.sqt bu adı yazar
    int         arity;        // beklenen argüman sayısı
    HostThunk   thunk;        // ortak ABI gövdesi
};

// Tüm host fonksiyonların düz tablosu. Index = sayısal host id (root.sqt'e
// gömülmez; hostFnIndex ile çözülür).
//
const std::vector<HostFn>& hostFnTable();

// Sembolik id → sayısal index. Bulunamazsa -1 (root.sqt ↔ C++ drift kontrolü).
int hostFnIndex(const std::string& symbolicId);

#endif // SAQUT_FFI_HOST_FUNCTIONS
