// ============================================================================
// saQut FFI — Value ↔ HostSlot Köprüsü
// ============================================================================
//
// DİZİN:   src/ffi/host_bridge.hpp
// KATMAN:  FFI — VM'in iç temsili ile backend-nötr ABI arasındaki TEK çeviri
//
// AMAÇ (#222):
//   host_abi.hpp sınırı tanımlar; bu dosya VM tarafını o sınıra bağlar.
//   Backend'ler (JIT/LLVM/gccjit) HostSlot'u DOĞRUDAN üretir — bu köprüden
//   geçmezler. Çeviri yalnızca VM içindir, çünkü Value backend-nötr değildir
//   (80 bayt, gömülü std::string + DecimalValue).
//
// STRING SAHİPLİĞİ:
//   VM string'i Value::stringValue içinde inline tutar; HostSlot pointer
//   ister. toHostSlot bu yüzden bir StringObject'e ihtiyaç duyar. Ömrü
//   çağrı boyunca ÇAĞIRANIN tuttuğu geçici tampon (HostCallScratch)
//   tarafından garanti edilir — thunk dönene kadar yaşar, sonra ölür.
//   Thunk'un ÜRETTİĞİ string ise ayrı: onu çağıran Value'ya kopyalar.
//   (GC-yönetimli string için bkz. Heap::allocString TODO'su — shadow stack
//   önkoşulu.)
//
// ============================================================================

#ifndef SAQUT_FFI_HOST_BRIDGE
#define SAQUT_FFI_HOST_BRIDGE

#include <memory>
#include <string>
#include <vector>

#include "ffi/host_abi.hpp"
#include "gc/gc_object.hpp"
#include "gc/gc_heap.hpp"
#include "vm/value.hpp"

// ----------------------------------------------------------------------------
// HostCallScratch — tek bir host çağrısının geçici belleği.
//
// Çağrı başına heap tahsisini önlemek için ÇAĞIRAN tarafından yeniden
// kullanılır (Interpreter bir tane tutar, her CALLHOST'ta reset eder).
//
// Neden gerekli: HostSlot string'i pointer taşır ama VM'in Value'sundaki
// string inline'dır — pointer verecek bir nesne yok. Bu tampon çağrı süresince
// o nesneleri barındırır.
// ----------------------------------------------------------------------------
struct HostRetOwner;  // aşağıda tanımlı

struct HostCallScratch {
    std::vector<HostSlot>                      slots;
    std::vector<std::unique_ptr<StringObject>> strings;
    std::vector<std::unique_ptr<DecimalObject>> decimals;

    void reset() {
        slots.clear();
        strings.clear();
        decimals.clear();
    }
};

// ----------------------------------------------------------------------------
// HostRetOwner — dönüş değerinin ömür sahibi (HostCallFrame::retOwner).
//
// ret.p bir pointer'dır (Str/Decimal). Thunk yeni bir string ürettiğinde
// (sys::env, date::format, string metodları) o nesne thunk döndükten SONRA
// da yaşamalı — çağıran okuyup Value'ya kopyalayana dek.
//
// host_abi.hpp bu tipi bilmez (backend-nötr kalmalı, StringObject bir VM
// tipidir); frame yalnızca void* taşır ve buradan bağlanır.
//
// NEDEN GC DEĞİL: GC-yönetimli string doğru nihai çözümdür, ama JIT
// register'larındaki referanslar bugün kök gösterilemiyor (shadow stack yok,
// bkz. Heap::allocString TODO'su). Tek-çağrılık ömür o gelene dek hem güvenli
// hem sızıntısızdır.
// ----------------------------------------------------------------------------
struct HostRetOwner {
    StringObject  string;
    DecimalObject decimal{DecimalValue{}};
};

// Thunk'ların dönüş kurma yardımcıları — ret.p'yi elle yazmak sarkan pointer
// riskidir, sahiplik tek yerde bağlanır.
inline void hostSetRetString(HostCallFrame& f, std::string s) {
    auto* owner = static_cast<HostRetOwner*>(f.retOwner);
    owner->string.data = std::move(s);
    f.ret = HostSlot::fromStr(&owner->string);
}

inline void hostSetRetDecimal(HostCallFrame& f, const DecimalValue& d) {
    auto* owner = static_cast<HostRetOwner*>(f.retOwner);
    owner->decimal.val = d;
    f.ret = HostSlot::fromDecimal(&owner->decimal);
}

// Value dönüşünü frame'e yerleştir — sahiplik gerektiren türleri owner'a
// kopyalar, skalerleri doğrudan yazar. Adım 2 sarmalayıcısı bunu kullanır.
inline void hostSetRetValue(HostCallFrame& f, const Value& v) {
    switch (v.kind) {
        case ValueKind::String:  hostSetRetString(f, v.stringValue()); return;
        case ValueKind::Decimal: hostSetRetDecimal(f, v.decimalValue()); return;
        case ValueKind::Int:     f.ret = HostSlot::fromInt(v.intValue()); return;
        case ValueKind::LongInt: f.ret = HostSlot::fromLong(v.int64Value()); return;
        case ValueKind::Date:    f.ret = HostSlot::fromDate(v.int64Value()); return;
        case ValueKind::Float:   f.ret = HostSlot::fromFloat(v.floatValue()); return;
        case ValueKind::Float32: f.ret = HostSlot::fromFloat32(v.floatValue()); return;
        case ValueKind::Ref:     f.ret = HostSlot::fromRef(v.ref()); return;
        case ValueKind::Null:    f.ret = HostSlot::null(); return;
    }
    f.ret = HostSlot::null();
}

// ----------------------------------------------------------------------------
// toHostSlot — VM Value → sınır temsili.
//
// String ve Decimal için scratch'te bir nesne oluşturur (pointer ömrü çağrı
// boyunca garanti). Diğer türler doğrudan kopyalanır.
// ----------------------------------------------------------------------------
inline HostSlot toHostSlot(const Value& v, HostCallScratch& scratch) {
    switch (v.kind) {
        case ValueKind::Int:     return HostSlot::fromInt(v.intValue());
        case ValueKind::LongInt: return HostSlot::fromLong(v.int64Value());
        case ValueKind::Date:    return HostSlot::fromDate(v.int64Value());
        case ValueKind::Float:   return HostSlot::fromFloat(v.floatValue());
        case ValueKind::Float32: return HostSlot::fromFloat32(v.floatValue());
        case ValueKind::Null:    return HostSlot::null();
        case ValueKind::Ref:     return HostSlot::fromRef(v.ref());
        case ValueKind::String: {
            scratch.strings.push_back(std::make_unique<StringObject>(v.stringValue()));
            return HostSlot::fromStr(scratch.strings.back().get());
        }
        case ValueKind::Decimal: {
            scratch.decimals.push_back(std::make_unique<DecimalObject>(v.decimalValue()));
            return HostSlot::fromDecimal(scratch.decimals.back().get());
        }
    }
    return HostSlot::null();
}

// ----------------------------------------------------------------------------
// fromHostSlot — sınır temsili → VM Value.
//
// Thunk'un ürettiği string/decimal İÇERİK OLARAK kopyalanır: dönüş
// değerinin ömrü çağrıdan uzundur (çağıranın slot'unda yaşar), scratch ise
// çağrı sonunda ölür.
// ----------------------------------------------------------------------------
inline Value fromHostSlot(const HostSlot& s) {
    switch (s.kind) {
        case HostKind::Int:     return Value::fromInt(static_cast<int>(s.i));
        case HostKind::LongInt: return Value::fromLongInt(s.i);
        case HostKind::Date:    return Value::fromDate(s.i);
        case HostKind::Float:   return Value::fromFloat(s.d);
        case HostKind::Float32: return Value::fromFloat32(s.d);
        case HostKind::Ref:     return Value::fromRef(static_cast<Object*>(s.p));
        case HostKind::Null:    return Value::null();
        case HostKind::Void:    return Value::null();
        case HostKind::Str:
            return s.p ? Value::fromString(static_cast<StringObject*>(s.p)->data)
                       : Value::fromString("");
        case HostKind::Decimal:
            // Sınırda decimal HER ZAMAN DecimalObject*'tır (VM scratch'i de,
            // JIT'in kutuladığı da). Ham DecimalValue* okumak 8 bayt kayma
            // demekti — ölçüldü: print(decimal) ham katsayı basıyordu.
            return s.p ? Value::fromDecimal(static_cast<DecimalObject*>(s.p)->val)
                       : Value::fromDecimal(DecimalValue{});
    }
    return Value::null();
}

// ----------------------------------------------------------------------------
// Thunk gövdelerinin okuma yardımcıları.
//
// Host fonksiyonları bugün `args[0].intValue()` gibi doğrudan Value alanına
// erişiyor. Yeni imzada aynı kolaylığı sağlar, ayrıca sayısal genişletmeyi
// tek yerde toplar (Value::asI64/asDouble ile aynı kurallar — ADR-040).
// ----------------------------------------------------------------------------
inline int64_t hostAsI64(const HostSlot& s) {
    if (s.kind == HostKind::Float || s.kind == HostKind::Float32)
        return static_cast<int64_t>(s.d);
    return s.i;
}

inline double hostAsDouble(const HostSlot& s) {
    if (s.kind == HostKind::Float || s.kind == HostKind::Float32) return s.d;
    return static_cast<double>(s.i);
}

inline const std::string& hostAsString(const HostSlot& s) {
    static const std::string kEmpty;
    if (s.kind != HostKind::Str || !s.p) return kEmpty;
    return static_cast<StringObject*>(s.p)->data;
}

#endif // SAQUT_FFI_HOST_BRIDGE
