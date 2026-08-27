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
// Her heap nesnesinde dört alan:
//   type      — Array/Struct/String/Decimal ayırımı (işaretleme/silme switch'i)
//   markState — tricolor canlılık durumu (#217); TEK canlılık kaynağı
//   next/prev — Heap'in "tüm nesneler" ÇİFT YÖNLÜ intrusive listesi (O(1) çıkarma)
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
    MarkState  markState = MarkState::White;  // #217: tricolor; tek canlılık kaynağı
    // ÇİFT YÖNLÜ intrusive liste (faz 3): listeden O(1) çıkarma — tekil
    // nesne silme (agc/devir/yapılacak move) ve sweep için. Önceden yalnız
    // `next` vardı; çıkarmak için listede O(n) tarama gerekiyordu.
    Object*    next   = nullptr;
    Object*    prev   = nullptr;

    // NOT: sanal markChildren/yıkıcı KALDIRILDI (faz 3). İşaretleme ve silme
    // tip etiketi (type) üzerinden switch ile object.cpp'te tek yerde yapılır
    // — nesne başına vptr (8 bayt + sanal çağrı) kalkar. Yeni ObjectType
    // eklendiğinde markObjectChildren/deleteObject switch'lerine case girme
    // zorunluluğu -Wswitch=… ile korunamaz; iki switch yan yana ve yorumlu.
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

    // JIT direct-memory view (non-owning). MIR, std::vector::data()'ı
    // çağıramaz; bu alanlar scalar buffer'ın güncel adresini/length'ini
    // sabit offset'ten yüklemesi için tutulur. GC tarafından taranmaz —
    // canonical elements/fields GC hakikat kaynağıdır (markChildren DEĞİŞMEZ).
    void*          jitData     = nullptr;
    int64_t        jitLength   = 0;
    ArrayElemKind  jitElemKind = ArrayElemKind::Ref;

    // Scalar buffer'a işaret eden view'ı vector'ün güncel durumundan
    // senkronize eder. Çağrı sırası zorunlu: allocArray(reserve) → resize()
    // → syncJitView(). allocArray içine konulmaz — reserve aşamasında
    // data() geçerli bir eleman buffer'ı göstermeyebilir.
    void syncJitView() {
        jitElemKind = elemKind;
        switch (elemKind) {
            case ArrayElemKind::Byte:    jitData = bytes.data();    jitLength = (int64_t)bytes.size();    break;
            case ArrayElemKind::Int:     jitData = ints.data();     jitLength = (int64_t)ints.size();     break;
            case ArrayElemKind::LongInt: jitData = longs.data();    jitLength = (int64_t)longs.size();    break;
            case ArrayElemKind::Float32: jitData = f32s.data();     jitLength = (int64_t)f32s.size();     break;
            case ArrayElemKind::Float64: jitData = f64s.data();     jitLength = (int64_t)f64s.size();     break;
            case ArrayElemKind::Decimal: jitData = decimals.data(); jitLength = (int64_t)decimals.size(); break;
            default:
                // Ref array: vector<Value> adresi — pointer elemanlar için
                // doğrudan lowering yok (Aşama 4); view yine de senkron tutulur.
                jitData = elements.data(); jitLength = (int64_t)elements.size(); break;
        }
    }

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

    // string'in ref çocuğu yok — markObjectChildren'da case yok
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

    // decimal'in ref çocuğu yok — markObjectChildren'da case yok
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
//   (markObjectChildren bunları kurgular — object.cpp)

struct Heap {
    Object*   head       = nullptr;
    int       allocCount = 0;   // canlı (listede duran) nesne sayısı
    int       gcRuns     = 0;   // toplam sweep sayısı (istatistik)
    long long freedTotal = 0;   // toplam serbest bırakılan nesne (istatistik)

    // Liste bağlama: öne ekle, ÇİFT bağla (sweep/O(1) çıkarma prev ister).
    // Tek yer — yeni tahsis türü eklenirse bu yardımcı kullanılır.
private:
    template <typename T>
    T* linkFront(T* obj) {
        obj->next = head;
        obj->prev = nullptr;
        if (head) head->prev = obj;
        head = obj;
        ++allocCount;
        return obj;
    }

public:
    ArrayObject* allocArray(int capacity = 0, ArrayElemKind k = ArrayElemKind::Ref) {
        return linkFront(new ArrayObject(capacity, k));
    }

    // Tek-string-modeli: VM ve JIT string'leri ARTIK burada yaşar — Value
    // inline string taşımaz (value.hpp). (Eski "JIT sızıntısı / #222 önkoşul"
    // notu: shadow stack geldi (#228) ve string'ler köklendi — kapandı.)
    StringObject* allocString(std::string s = "") {
        return linkFront(new StringObject(std::move(s)));
    }

    // #228: JIT decimal kutulaması da GC'ye girer (shadow stack ile görünür).
    DecimalObject* allocDecimal(const DecimalValue& v) {
        return linkFront(new DecimalObject(v));
    }

    StructObject* allocStruct(int fieldCount) {
        return linkFront(new StructObject(fieldCount));
    }

    // ── Mark ─────────────────────────────────────────────────────────────────

    // Tek bir Value'dan ulaşılabilen nesneyi ve onun çocuklarını işaretle.
    void markValue(const Value& v);

    // Bir slot dizisindeki tüm Value'ları tara.
    void markSlots(const std::vector<Value>& slots);

    // ── Sweep ────────────────────────────────────────────────────────────────

    // İşaretlenmemiş (White) nesneleri O(1) listeden çıkarıp sil;
    // canlıların markState'ini sıfırla. Dönüş: serbest bırakılan nesne sayısı.
    int sweep();

    // Nesneyi intrusive listeden O(1)'e çıkar (silmeksizin) — tekil nesne
    // taşıma/devir (yapılacak move/agc) için de tek nokta.
    void unlink(Object* obj);

    // Program sonunda kalan her şeyi temizle. Silme tip etiketiyle
    // deleteObject'te (object.cpp) — vptr kalktığı için taban işaretçiden
    // `delete` yapılamaz.
    ~Heap();

    Heap()                       = default;
    Heap(const Heap&)            = delete;
    Heap& operator=(const Heap&) = delete;
};

// #217: write barrier — object.cpp'te tanımlı
void writeBarrier(Object* target, Object* newRef);

// #217: incremental marking step — Grey queue'dan budget kadar işle
int drainGrey(Heap* heap, int budget);

// Tek-string-modeli: Value::fromString'in tahsis yapacağı aktif heap'i
// bağla/çöz (Interpreter ve JIT çalışma başında bağlar). Ayrıntı:
// object.cpp içindeki Value string tahsis kancası bölümü.
void setValueStringHeap(Heap* h);

#endif // SAQUT_VM_OBJECT
