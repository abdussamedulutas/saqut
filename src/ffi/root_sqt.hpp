// ============================================================================
// saQut FFI — Gömülü root.sqt (Host Fonksiyon Bildirim Kataloğu)
// ============================================================================
//
// DİZİN:   src/ffi/root_sqt.hpp
// KATMAN:  FFI — gömülü ffi bildirimlerine erişim
//
// #226: Kaynak artık burada DEĞİL. src/internal/ffi.sqt gerçek bir saQut
// dosyasıdır ve derleme sırasında binary'ye gömülür
// (scripts/embed_internal.py → build/generated/internal_sources.hpp).
// Bu başlık yalnızca ona erişim sağlar.
//
// ============================================================================

#ifndef SAQUT_FFI_ROOT_SQT
#define SAQUT_FFI_ROOT_SQT

#include "internal_sources.hpp"

// Gömülü ffi bildirimleri. Bir kez parse edilir (FfiCatalog).
inline const char* kEmbeddedRootSqt = [] {
    auto it = internalSources().find("ffi.sqt");
    return it != internalSources().end() ? it->second : "";
}();

#endif // SAQUT_FFI_ROOT_SQT
