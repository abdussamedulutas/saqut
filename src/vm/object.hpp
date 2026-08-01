// ============================================================================
// saQut VM — Heap, GC ve Object Tipleri (ArrayObject, StructObject)
// ============================================================================
//
// DİZİN:   src/vm/object.hpp
// KATMAN:  VM — Heap'te yaşayan nesneler ve garbage collector iskeleti
//
// AMAÇ:
//   ArrayObject ve StructObject için heap tahsisi, GCList ile mark-sweep
//   garbage collector altyapısı. Toplama Interpreter::maybeCollect() ile
//   instruction sınırında (safepoint) eşik tabanlı tetiklenir (#77).
//
// ============================================================================

#ifndef SAQUT_VM_OBJECT
#define SAQUT_VM_OBJECT

#include <memory>
#include <string>
#include <vector>

#include "core/array_elem_kind.hpp"
#include "core/decimal.hpp"  // DecimalObject (JIT decimal kutulama, ADR-037)

// ADR-022: Taşımasız, stop-the-world, deterministik mark-sweep GC.
//
// Her heap nesnesinde üç alan:
//   type   — ArrayObject / StructObject ayırımı için
//   marked — mark aşamasında işaretlenir; sweep aşamasında sıfırlanır
//   next   — Heap'in tuttuğu "tüm nesneler" intrusive listesinin bağı
//
// Toplama fonksiyon dönüşünde tetiklenir (deterministik safepoint).
// Nesne modeli bir daha değiştirilmez — collect() lokal bir eklemedir.

enum class ObjectType { Array, Struct, String, Decimal };

struct Value; // object.hpp <-> value.hpp çapraz bağımlılık; tam tanım value.hpp'de

// #217: Incremental marking için tricolor state
enum class MarkState : uint8_t {
    White = 0,  // işaretlenmemiş (canlı olmayabilir)
    Grey  = 1,  // işaretlendi ama çocukları henüz taranmadı
    Black = 2,  // işaretlendi ve çocukları tarandı
};

struct Object {
    ObjectType type;
    bool       marked = false;
    MarkState  markState = MarkState::White;  // #217: incremental marking
    Object*    next   = nullptr;

    // Mark aşaması: bu nesneden ulaşılabilen tüm referansları işaretle.
    // Alt sınıflar kendi fields/elements'larını bildiğinden sanal metot.
    virtual void markChildren() = 0;

    virtual ~Object() = default;
};

// ── ArrayObject (ADR-020: referans semantiği, #206: packed type-tagged array) ──
//
// Eleman tipi elemKind ile belirtilir. Sadece ilgili buffer kullanılır;
// diğerleri boştur.
//   Ref:      elements (vector<Value>)
//   Byte:     bytes    (vector<uint8_t>)
//   Int:      ints     (vector<int32_t>)
//   LongInt:  longs    (vector<int64_t>)
//   Float32:  f32s     (vector<float>)
//   Float64:  f64s     (vector<double>)
//   Decimal:  decimals (vector<DecimalValue>)

struct ArrayObject : Object {
    ArrayElemKind elemKind = ArrayElemKind::Ref;
    std::vector<Value>        elements;   // elemKind == Ref
    std::vector<uint8_t>      bytes;      // elemKind == Byte
    std::vector<int32_t>      ints;       // elemKind == Int
    std::vector<int64_t>      longs;      // elemKind == LongInt
    std::vector<float>        f32s;       // elemKind == Float32
    std::vector<double>       f64s;       // elemKind == Float64
    std::vector<DecimalValue> decimals;   // elemKind == Decimal

    explicit ArrayObject(int capacity = 0, ArrayElemKind k = ArrayElemKind::Ref) : elemKind(k) {
        type = ObjectType::Array;
        // reserve kullan — resize DEĞİL. #206: slice/push builtin'leri push_back
        // ile eleman ekler; resize ön-doldurma yaparsa boyut iki katına çıkar.
        if (capacity <= 0) return;
        switch (elemKind) {
            case ArrayElemKind::Ref:     elements.reserve(capacity); break;
            case ArrayElemKind::Byte:    bytes.reserve(capacity);    break;
            case ArrayElemKind::Int:     ints.reserve(capacity);     break;
            case ArrayElemKind::LongInt: longs.reserve(capacity);    break;
            case ArrayElemKind::Float32: f32s.reserve(capacity);     break;
            case ArrayElemKind::Float64: f64s.reserve(capacity);     break;
            case ArrayElemKind::Decimal: decimals.reserve(capacity); break;
        }
    }

    void markChildren() override;
};

// ── StructObject (ADR-037, #206) ─────────────────────────────────────────────
//
// fieldNames tip başına bir kez tutulur (shared_ptr). Tüm örnekler aynı
// metadata'yı paylaşır. Registry: Interpreter (veya Heap) tip-adı → names
// eşlemesini tutar.

struct StructObject : Object {
    std::vector<Value>                        fields;
    std::shared_ptr<std::vector<std::string>> fieldNames; // paylaşımlı metadata

    explicit StructObject(int fieldCount = 0) {
        type = ObjectType::Struct;
        fields.resize(fieldCount);
    }

    void markChildren() override;
};

// ── StringObject (JIT sınırı, ADR-037) ──────────────────────────────────────
//
// VM string'i inline tutar (value.hpp: ValueKind::String, gömülü std::string —
// ADR-024 immutable değer-tipi). JIT sınırında bir MIR register bir string'in
// tamamını taşıyamaz; bu yüzden JIT tarafında her string heap'e kutulanır ve
// register yalnızca bu nesneye bir pointer (MIR_T_I64) taşır. ADR-037:
// "JIT'in gördüğü her String değeri heap'e kutulanır (StringObject : Object)".
//
// Kutulama farkı kasıtlı ve kayıtlı: iki backend'in İÇ bellek modeli farklı
// (VM inline, JIT kutulu) ama GÖZLEMLENEN davranış (stdout, içerik-eşitliği,
// immutability) aynı — diferansiyel test (#92) bunu doğrular.
//
// GC notu: string'in ref çocuğu yoktur (markChildren no-op). Dilim 3'te JIT
// string'leri henüz VM Heap'ine bağlanmaz (host-taraflı intern tablosunda
// yaşar, bkz. mir_backend.cpp); shadow-stack entegrasyonu Dilim 2/§8 işi.
struct StringObject : Object {
    std::string data;

    explicit StringObject(std::string s = "") : data(std::move(s)) {
        type = ObjectType::String;
    }

    void markChildren() override {}  // string'in ref çocuğu yok
};

// ── DecimalObject (JIT sınırı, ADR-037) ──────────────────────────────────────
// DecimalValue (coeff+exp, >8 byte) MIR register'ına sığmaz → JIT'te decimal
// heap'e kutulanır, register pointer taşır. Aritmetik runtime call'a gider
// (ADR-037: decimal v1'de her zaman kutulu). VM'de Value içine inline. GC çocuğu yok.
struct DecimalObject : Object {
    DecimalValue val;

    explicit DecimalObject(const DecimalValue& v) : val(v) {
        type = ObjectType::Decimal;
    }

    void markChildren() override {}
};

// ── Heap ─────────────────────────────────────────────────────────────────────
//
// Tahsis: allocArray / allocStruct — her yeni nesneyi intrusive listeye ekler.
// Toplama: mark + sweep iki geçiş. Kökleri MARK EDEN Interpreter'dır
// (Interpreter::maybeCollect) — kök kümesi VM'in iç yapısına bağlı olduğundan
// (globalSlots_ program-çapında tek dizi, callStack_, uçuştaki pendingThrow_) Heap
// yalnızca markValue/markSlots/sweep yapıtaşlarını sunar.
//
// Çocuk kaynakları:
//   - ArrayObject::elements, StructObject::fields içindeki Ref değerleri
//   (markChildren() bunları kurgular)

struct Heap {
    Object*   head       = nullptr;
    int       allocCount = 0;   // canlı (listede duran) nesne sayısı
    int       gcRuns     = 0;   // toplam sweep sayısı (istatistik)
    long long freedTotal = 0;   // toplam serbest bırakılan nesne (istatistik)

    ArrayObject* allocArray(int capacity = 0, ArrayElemKind k = ArrayElemKind::Ref) {
        auto* obj = new ArrayObject(capacity, k);
        obj->next = head;
        head      = obj;
        ++allocCount;
        return obj;
    }

    // #222: GC-yönetimli string tahsisi.
    //
    // VM string'i Value::stringValue içinde INLINE tutar ve buraya hiç
    // uğramaz — bu yol yalnızca SINIR temsili içindir (JIT register'ı ve
    // host ABI'si string'i pointer olarak taşır, bkz. host_abi.hpp).
    //
    // Bugün JIT ürettiği string'leri g_jitRuntimeStrings'te süresiz tutuyor:
    // 200k concat'te JIT 21,8 MB / VM 6,8 MB (ölçüldü). Bu sızıntının çözümü
    // burasıdır — AMA HENÜZ BAĞLANAMAZ:
    //
    //   GC kökleri yalnızca globalSlots_ ve VM callStack_ frame'leridir
    //   (Interpreter::maybeCollect). JIT'in string'leri hiçbir Value'da
    //   yaşamaz, yalnızca MIR register'ında — yani kök gösterilemezler.
    //   Şimdi bağlarsak GC onları CANLIYKEN siler: sızıntı use-after-free'ye
    //   dönüşür, ki bu kesinlikle daha kötüdür.
    //
    // Önkoşul: JIT register'larındaki referansları GC'ye görünür kılan shadow
    // stack (Ref dilimi). O geldiğinde JIT string'leri ve host thunk'larının
    // dönüş string'leri buraya taşınır. TODO(#222/Ref dilimi).
    //
    // Bugün kullanan: VM tarafında host ABI dönüş string'leri (kök: çağıran
    // frame'in slot'u — maybeCollect zaten tarar).
    StringObject* allocString(std::string s = "") {
        auto* obj = new StringObject(std::move(s));
        obj->next = head;
        head      = obj;
        ++allocCount;
        return obj;
    }

    // #228: JIT decimal kutulaması da GC'ye girer (shadow stack ile görünür).
    DecimalObject* allocDecimal(const DecimalValue& v) {
        auto* obj = new DecimalObject(v);
        obj->next = head;
        head      = obj;
        ++allocCount;
        return obj;
    }

    StructObject* allocStruct(int fieldCount) {
        auto* obj = new StructObject(fieldCount);
        obj->next = head;
        head      = obj;
        ++allocCount;
        return obj;
    }

    // ── Mark ─────────────────────────────────────────────────────────────────

    // Tek bir Value'dan ulaşılabilen nesneyi ve onun çocuklarını işaretle.
    void markValue(const Value& v);

    // Bir slot dizisindeki tüm Value'ları tara.
    void markSlots(const std::vector<Value>& slots);

    // ── Sweep ────────────────────────────────────────────────────────────────

    // İşaretlenmemiş nesneleri sil; işaretlenenlerin bitini sıfırla.
    // Dönüş: bu turda serbest bırakılan nesne sayısı.
    int sweep();

    // Program sonunda kalan her şeyi temizle.
    ~Heap() {
        Object* cur = head;
        while (cur) {
            Object* nxt = cur->next;
            delete cur;
            cur = nxt;
        }
    }

    Heap()                       = default;
    Heap(const Heap&)            = delete;
    Heap& operator=(const Heap&) = delete;
};

// #217: write barrier — object.cpp'te tanımlı
void writeBarrier(Object* target, Object* newRef);

// #217: incremental marking step — Grey queue'dan budget kadar işle
int drainGrey(Heap* heap, int budget);

#endif // SAQUT_VM_OBJECT
