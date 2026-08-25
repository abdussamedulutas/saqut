// ============================================================================
// saQut FFI — Host Çağrı ABI'si (Backend-Nötr)
// ============================================================================
//
// DİZİN:   src/ffi/host_abi.hpp
// KATMAN:  FFI — VM ve tüm backend'lerin ORTAK host çağrı sözleşmesi
//
// AMAÇ (#222, docs/plan-0.9.3-host-abi.md):
//   Host fonksiyonları (math/date/fs/sys) ve built-in metodlar
//   (array/string/struct) bugün ÜÇ ayrı çağrı sözleşmesiyle çağrılıyor:
//
//     "__ffi__"             → callHostFn(id, vector<Value>, HostContext&)
//     "__builtin_method__"  → dispatchBuiltinMethod(id, vector<Value>, Heap&)
//     "print"               → executeHostFunction(name, slots, argSlots)
//
//   Bu, her backend'in 70 ayrı trampoline yazması demek. Bu dosya üçünü tek
//   bir C çağrı konvansiyonuna indirger: bir backend'i host çağrılarına açmak
//   = TEK bir fonksiyonu (rt_host_call) import etmek.
//
// TASARIM KISITLARI (neden böyle):
//
//   1. TRIVIALLY COPYABLE. JIT, argümanları stack'te doğrudan inşa eder.
//      std::string veya DecimalValue GÖMÜLÜ taşıyan bir yapı bunu imkânsız
//      kılar (ctor/dtor çağrısı gerekir). Bu yüzden HostSlot her bileşik
//      değeri POINTER olarak taşır — Value'nun 80 baytına karşı 16 bayt.
//
//   2. EXCEPTION YOK. Bugün host fonksiyonları std::runtime_error fırlatıyor,
//      VM yakalayıp saQut Error'a çeviriyor. C++ exception'ı JIT'ten geçirmek
//      taşınabilir değil ve her backend'e ayrı unwind bariyeri demek. Yerine
//      thunk sıfır-dışı döner ve HostError doldurur — tek dönüş yolu, her
//      backend'de aynı.
//
//   3. TAHSİSSİZ ÇAĞRI. args ve ret ÇAĞIRAN tarafından sahiplenilen, önceden
//      ayrılmış bölgeye işaret eder. VM frame slot'larından besler, JIT
//      stack tamponundan. Çağrı başına heap tahsisi YOK.
//
// ÖLÇÜM NOTU: bu iş bir performans işi DEĞİLDİR. 300k iterasyonda mevcut FFI
// yolu (0,88s) saf saQut fonksiyon çağrısından (1,35s) zaten hızlı — frame
// açmadığı için. Bu bir KAPSAM işidir: JIT'in host çağrılarını hiç
// kullanamaması. Yeni ABI'nin hızı bozmaması ölçülerek doğrulanır.
//
// ============================================================================

#ifndef SAQUT_FFI_HOST_ABI
#define SAQUT_FFI_HOST_ABI

#include <cstdint>
#include <string>
#include <vector>

struct Object;
struct Heap;
struct Value;

// ----------------------------------------------------------------------------
// HostKind — HostSlot'un taşıdığı değerin türü.
//
// ValueKind'ın backend-nötr karşılığı. Ayrı enum çünkü ValueKind VM'in iç
// temsiline bağlı (Value::stringValue inline), HostKind ise SINIR temsilini
// tanımlar (string her zaman pointer). İkisi arasındaki eşleme tek yerde
// (host_abi.cpp) ve testle çapraz doğrulanır.
// ----------------------------------------------------------------------------
enum class HostKind : uint8_t {
    Int,      // 32-bit signed — i alanı (bool da burada: 0/1)
    LongInt,  // 64-bit signed — i alanı
    Float,    // 64-bit IEEE double — d alanı
    Float32,  // 32-bit IEEE single — d alanı ((float) truncate'li)
    Date,     // UTC epoch-ms — i alanı
    Str,      // StringObject* — p alanı
    Decimal,  // DecimalObject* — p alanı (kutulu: coeff+exp register'a sığmaz).
              // Str'in StringObject* olmasıyla tutarlı: sınırda bileşik
              // değerler HER ZAMAN GC nesne ailesinden bir pointer'dır.
    Ref,      // Object* (array/struct) — p alanı
    Null,     // değer yok; p == nullptr
    Void,     // dönüş yok (arity 0 dönüşlü thunk'lar)
};

// ----------------------------------------------------------------------------
// HostSlot — sınırdan geçen tek değer. 16 bayt, trivially copyable.
//
// Value (80 bayt) ile karşılaştırma: Value içinde std::string (32) +
// DecimalValue (16) GÖMÜLÜ taşır ve bu yüzden JIT'in stack'te inşa
// edemeyeceği bir tiptir. HostSlot bileşikleri pointer'lar.
// ----------------------------------------------------------------------------
struct HostSlot {
    HostKind kind = HostKind::Null;
    // 7 bayt padding — union 8'e hizalı. Bilinçli: alanı sıkıştırmak
    // (bitfield/packed) JIT tarafında offset aritmetiğini karmaşıklaştırır ve
    // hiçbir şey kazandırmaz; 16 bayt zaten iki register.
    union {
        int64_t i;   // Int, LongInt, Date, bool
        double  d;   // Float, Float32
        void*   p;   // Str, Decimal, Ref, Null
    };

    HostSlot() : i(0) {}

    static HostSlot fromInt(int32_t v)      { HostSlot s; s.kind = HostKind::Int;     s.i = v; return s; }
    static HostSlot fromLong(int64_t v)     { HostSlot s; s.kind = HostKind::LongInt; s.i = v; return s; }
    static HostSlot fromDate(int64_t v)     { HostSlot s; s.kind = HostKind::Date;    s.i = v; return s; }
    static HostSlot fromFloat(double v)     { HostSlot s; s.kind = HostKind::Float;   s.d = v; return s; }
    static HostSlot fromFloat32(double v)   { HostSlot s; s.kind = HostKind::Float32; s.d = (double)(float)v; return s; }
    static HostSlot fromStr(void* so)       { HostSlot s; s.kind = HostKind::Str;     s.p = so; return s; }
    static HostSlot fromDecimal(void* dv)   { HostSlot s; s.kind = HostKind::Decimal; s.p = dv; return s; }
    static HostSlot fromRef(void* o)        { HostSlot s; s.kind = HostKind::Ref;     s.p = o; return s; }
    static HostSlot null()                  { HostSlot s; s.kind = HostKind::Null;    s.p = nullptr; return s; }
    static HostSlot voidVal()               { HostSlot s; s.kind = HostKind::Void;    s.p = nullptr; return s; }

    bool isNull() const { return kind == HostKind::Null; }
};

static_assert(sizeof(HostSlot) == 16,
              "HostSlot 16 bayt olmalı — JIT stack'te sabit offset'le inşa eder");

// ----------------------------------------------------------------------------
// HostError — exception'ın backend-nötr yerine geçeni.
//
// Alanları saQut Error struct'ıyla hizalı tutulur ki VM tarafında çeviri
// maliyeti sıfıra yakın olsun (makeErrorValue doğrudan besler).
// ----------------------------------------------------------------------------
struct HostError {
    // Boş değilse hata var. Sabit tampon YOK — hata yolu sıcak değil, ve
    // std::string burada güvenli çünkü HostError JIT register'ında taşınmaz,
    // yalnızca HostCallFrame içinden pointer'la erişilir.
    std::string message;
    std::string code;     // "E_HOST"

    bool failed() const { return !message.empty(); }
    void clear() { message.clear(); code.clear(); }
    void set(std::string msg, std::string c) {
        message = std::move(msg);
        code    = std::move(c);
    }
};

// ----------------------------------------------------------------------------
// HostEnv — VM durumuna erişim.
//
// Mevcut HostContext'in yerini alır, artı built-in metodların ihtiyaç duyduğu
// heap ve print'in ihtiyaç duyduğu çıktı yönlendirmesi. Böylece ÜÇ ayrı
// bağlam tipi (HostContext / Heap& / hiçbiri) tek yapıda birleşir.
//
// Pointer'lar Interpreter'ın gerçek üyelerine işaret eder.
// ----------------------------------------------------------------------------
struct HostEnv {
    const std::vector<std::string>* programArgs = nullptr;
    Heap*                           heap        = nullptr;
    // #105 (DAP): print çıktısı protokol stdout'una çıplak sızmamalı.
    // Bugün yalnızca VM'de bağlı; JIT'te bağlamak ayrı iş (plan §6.3).
    void*                           outputSink  = nullptr;
};

// ----------------------------------------------------------------------------
// HostCallFrame — tek bir host çağrısının tüm durumu.
//
// Çağıran doldurur, thunk okur/yazar. Hiçbir alanı sahiplenmez: args ve ret
// çağıranın belleğine işaret eder.
// ----------------------------------------------------------------------------
struct HostCallFrame {
    HostSlot*  args = nullptr;   // argc uzunluğunda, çağıranın belleği
    int32_t    argc = 0;
    HostSlot   ret;              // thunk doldurur; Void ise yok sayılır
    HostEnv*   env  = nullptr;   // heap/args gerektiren thunk'lar okur
    HostError  err;              // thunk sıfır-dışı dönerse dolu

    // ── Dönüş değeri sahipliği ───────────────────────────────────────────
    //
    // ret.p bir POINTER'dır (Str/Decimal/Ref). Thunk yeni bir string ürettiğinde
    // (sys::env, date::format, string metodları) o nesnenin thunk döndükten
    // SONRA da yaşaması gerekir — çağıran onu okuyup Value'ya kopyalayana dek.
    //
    // Bu iki tampon o ömrü sağlar: thunk üretilen nesneyi buraya koyar,
    // ret.p ona işaret eder. Çağıran değeri okuduktan sonra frame'i reset
    // eder ve nesne ölür. Tek çağrılık ömür — sahiplik belirsizliği yok.
    //
    // NEDEN GC DEĞİL: GC-yönetimli string doğru nihai çözümdür, fakat JIT
    // register'larındaki referanslar bugün kök gösterilemiyor (shadow stack
    // yok) — bkz. Heap::allocString TODO'su. Bu tampon o gelene kadar
    // güvenli ve sızıntısız ara çözümdür.
    // Dönüş tamponu OPAK tutulur: host_abi.hpp backend-nötr kalmalı ve
    // StringObject/DecimalValue (VM tipleri) buraya sızmamalı. Tamponu
    // host_bridge.hpp sahiplenir ve bu pointer üzerinden bağlar.
    //
    // Sözleşme: retOwner, çağrı boyunca ret.p'nin işaret ettiği nesnelerin
    // ömrünü garanti eden nesnedir. Çağıran sahiplenir; thunk yalnızca
    // hostSetRetString()/hostSetRetDecimal() üzerinden doldurur.
    void* retOwner = nullptr;

    // retOwner KASITLI olarak sıfırlanmaz: sahibi çağırandır ve çağrılar
    // arasında yeniden kullanılır (çağrı başına tahsis yapmamanın yolu).
    void reset() {
        argc = 0;
        ret  = HostSlot::null();
        err.clear();
    }
};

// ----------------------------------------------------------------------------
// HostThunk — TEK çağrı konvansiyonu.
//
// Dönüş: 0 = başarılı, sıfır-dışı = hata (f->err doldurulmuş).
// Bir backend'in bilmesi gereken tek imza budur.
// ----------------------------------------------------------------------------
using HostThunk = int (*)(HostCallFrame* f);

// ----------------------------------------------------------------------------
// HostEntry — registry'deki tek kayıt.
//
// hostFnTable() (44 kayıt) ve BuiltinMethodRegistry (26 kayıt) bu tek tabloda
// birleşir. CALLHOST artık functionName string'ine bakmaz — intValue bu
// tablonun indeksidir.
// ----------------------------------------------------------------------------
// Bayrakların anlamı BACKEND'İN SORACAĞI soruya göre tanımlıdır, "fonksiyon
// matematiksel olarak saf mı" sorusuna göre değil.
//
// Örnek: fs::exists ve sys::env dış dünyayı okur (yan etkili sayılır) ama
// HEAP'e dokunmaz ve HostEnv istemez → bu ABI açısından HOST_PURE'dur.
// JIT'in bilmek istediği tek şey budur: çağrı GC'yi tetikleyebilir mi,
// env pointer'ı geçmem gerekir mi.
enum : uint8_t {
    HOST_PURE       = 0,       // HostEnv gerekmez, heap'e dokunmaz → JIT safepoint istemez
    HOST_NEEDS_HEAP = 1u << 0, // env->heap kullanır (tahsis yapabilir → GC tetikleyebilir)
    HOST_NEEDS_ARGS = 1u << 2, // env->programArgs okur
    HOST_MUTATING   = 1u << 3, // receiver'ı yerinde değiştirir (built-in metod)
    HOST_CAN_FAIL   = 1u << 4, // hata döndürebilir (eski gövdelerde: throw)
};

struct HostEntry {
    const char*     symbolicId;   // "MATH_SQRT" | "ARRAY_PUSH" | "PRINT"
    int8_t          arity;        // beklenen argüman sayısı; -1 = değişken
    uint8_t         flags;        // HOST_* bayrakları
    HostKind        retKind;
    HostThunk       thunk;
};

#endif // SAQUT_FFI_HOST_ABI
