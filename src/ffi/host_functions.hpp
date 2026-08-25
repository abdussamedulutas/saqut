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
#include "ffi/host_abi.hpp"

// Tek bir host fonksiyon kaydı — thunk'ının yanında TAM HostEntry.
//
// #229: retKind ve flags artık kaydın KENDİSİNDE durur (kHostMeta çapraz
// tablosu yok). HostEntry host_abi.hpp'de tanımlıdır: symbolicId, arity,
// flags, retKind, thunk. Date fonksiyonları (15) ayrı tablodadır ve aynı
// tamlıktadır (src/data/date.cpp).
struct HostFn {
    const char* symbolicId;   // "MATH_SQRT" — root.sqt bu adı yazar
    int         arity;        // beklenen argüman sayısı
    HostKind    retKind;
    uint8_t     flags;
    HostThunk   thunk;        // ortak ABI gövdesi
};

// Tüm gömülü host fonksiyonların düz tablosu (math/fs/sys/date_now/core).
// Index = sayısal host id; root.sqt'e gömülmez, hostEntryIndex ile çözülür.
//
const std::vector<HostFn>& hostFnTable();

#endif // SAQUT_FFI_HOST_FUNCTIONS
