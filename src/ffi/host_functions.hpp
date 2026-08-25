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
//   kullanır → hostEntryIndex ile çözülür (#229; kHostFnBase toplamı orada).
//   Ad tabloda yoksa (drift) symbol collector hata verir.
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

// Tek bir host fonksiyon kaydı — HostEntry'nin (host_abi.hpp) alias'ı.
//
// #229 + AGENTS.md §10.2: alan-kopyası struct yerine alias — HostEntry zaten
// tam kaydı taşır (symbolicId, arity int8_t, flags, retKind, thunk); ayrı bir
// HostFn struct'ı ikinci tanım olurdu. Date fonksiyonları (15) ayrı tablodadır
// ve aynı tamlıktadır (src/data/date.cpp).
using HostFn = HostEntry;

// Tüm gömülü host fonksiyonların düz tablosu (math/fs/sys/date_now/core).
// Index = sayısal host id; root.sqt'e gömülmez, hostEntryIndex ile çözülür.
//
const std::vector<HostFn>& hostFnTable();

#endif // SAQUT_FFI_HOST_FUNCTIONS
