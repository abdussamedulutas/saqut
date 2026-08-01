// ============================================================================
// saQut FFI — Birleşik Host Registry
// ============================================================================
//
// DİZİN:   src/ffi/host_registry.hpp
// KATMAN:  FFI — host fonksiyonları + built-in metodlar + print, TEK tabloda
//
// AMAÇ (#222, plan adım 2):
//   Bugün CALLHOST üç ayrı çağrı ailesini functionName STRING'ine bakarak
//   ayırıyor (interpreter.cpp:1323). Bu tablo üçünü tek indeks uzayında
//   birleştirir: CALLHOST::intValue artık buranın indeksidir, functionName
//   karşılaştırması sıcak yoldan tümüyle kalkar.
//
// İNDEKS UZAYI — bloklara ayrılmıştır:
//
//   [0 .. kHostFnBase+N)     gömülü host fonksiyonları  (math/date/fs/sys/caps)
//   [kBuiltinBase .. +M)     built-in metodlar          (array/string/struct)
//   [kCoreBase .. +K)        çekirdek                   (print)
//
//   Blok tabanları SABİTTİR. Bir aileye kayıt eklemek diğer ailenin
//   indekslerini KAYDIRMAZ — aksi halde IR'ye gömülü indeksler bozulur ve
//   eski derlenmiş çıktı sessizce yanlış fonksiyonu çağırır.
//
// SEMBOLİK AD TEK KAYNAK: root.sqt ham sayı yazmaz ("MATH_SQRT" yazar),
// indeks buradan çözülür. Ad tabloda yoksa (C++ ↔ root.sqt drift) symbol
// collector hata verir — sessiz yanlış dispatch olmaz.
//
// ============================================================================

#ifndef SAQUT_FFI_HOST_REGISTRY
#define SAQUT_FFI_HOST_REGISTRY

#include <string>
#include <vector>

#include "ffi/host_abi.hpp"

// ── Blok tabanları ───────────────────────────────────────────────────────────
//
// Aralıklar cömert bırakıldı: bir aile büyürken diğerini itmesin. Boş
// aralıklar tabloda geçersiz kayıt (thunk == nullptr) olarak durur ve
// çağrılırsa hata döner — sessizce yanlış fonksiyona gitmez.
enum : int32_t {
    kHostFnBase   = 0,     // gömülü ffi (root.sqt bildirimlerinin gövdeleri)
    kBuiltinBase  = 256,   // built-in metodlar (array/string/struct)
    kCoreBase     = 512,   // çekirdek (print, ...)
    kHostIdInvalid = -1,
};

// Tüm kayıtların düz tablosu. Blok aralıklarındaki boşluklar geçersiz
// kayıtlarla doludur (thunk == nullptr).
const std::vector<HostEntry>& hostRegistry();

// Sembolik id → indeks. Bulunamazsa kHostIdInvalid.
// Drift kontrolü: root.sqt'te yazılı ad C++ tablosunda yoksa -1 döner.
int32_t hostEntryIndex(const std::string& symbolicId);

// Indeksten kayıt; geçersizse nullptr.
const HostEntry* hostEntryAt(int32_t id);

// ----------------------------------------------------------------------------
// rt_host_call — TÜM backend'lerin tek giriş noktası.
//
// Bir backend'i host çağrılarına açmak = bu tek fonksiyonu import etmek.
// 70 ayrı trampoline yerine 1. LLVM/gccjit eklenirse de aynı imza geçerlidir.
//
// extern "C": MIR ve diğer JIT'ler C sembolü import eder; C++ mangling yok.
//
// Dönüş: 0 = başarılı, sıfır-dışı = hata (f->err doldurulmuş).
// ----------------------------------------------------------------------------
extern "C" int rt_host_call(int32_t entryId, HostCallFrame* f);

#endif // SAQUT_FFI_HOST_REGISTRY
