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

#include <string>
#include <vector>

// ADR-022: Taşımasız, stop-the-world, deterministik mark-sweep GC.
//
// Her heap nesnesinde üç alan:
//   type   — ArrayObject / StructObject ayırımı için
//   marked — mark aşamasında işaretlenir; sweep aşamasında sıfırlanır
//   next   — Heap'in tuttuğu "tüm nesneler" intrusive listesinin bağı
//
// Toplama fonksiyon dönüşünde tetiklenir (deterministik safepoint).
// Nesne modeli bir daha değiştirilmez — collect() lokal bir eklemedir.

enum class ObjectType { Array, Struct, String };

struct Value; // object.hpp <-> value.hpp çapraz bağımlılık; tam tanım value.hpp'de

struct Object {
    ObjectType type;
    bool       marked = false;
    Object*    next   = nullptr;

    // Mark aşaması: bu nesneden ulaşılabilen tüm referansları işaretle.
    // Alt sınıflar kendi fields/elements'larını bildiğinden sanal metot.
    virtual void markChildren() = 0;

    virtual ~Object() = default;
};

// ── ArrayObject ──────────────────────────────────────────────────────────────

struct ArrayObject : Object {
    std::vector<Value> elements;

    explicit ArrayObject(int capacity = 0) {
        type = ObjectType::Array;
        if (capacity > 0) elements.reserve(capacity);
    }

    void markChildren() override;
};

// ── StructObject ─────────────────────────────────────────────────────────────

struct StructObject : Object {
    std::vector<Value>       fields;
    std::vector<std::string> fieldNames; // IR üretiminde doldurulur; toJson/dump için

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

// ── Heap ─────────────────────────────────────────────────────────────────────
//
// Tahsis: allocArray / allocStruct — her yeni nesneyi intrusive listeye ekler.
// Toplama: mark + sweep iki geçiş. Kökleri MARK EDEN Interpreter'dır
// (Interpreter::maybeCollect) — kök kümesi VM'in iç yapısına bağlı olduğundan
// (moduleSlots_ modül başına map, callStack_, uçuştaki pendingThrow_) Heap
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

    ArrayObject* allocArray(int capacity = 0) {
        auto* obj = new ArrayObject(capacity);
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

#endif // SAQUT_VM_OBJECT
