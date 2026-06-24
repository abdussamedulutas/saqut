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

enum class ObjectType { Array, Struct };

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

// ── Heap ─────────────────────────────────────────────────────────────────────
//
// Tahsis: allocArray / allocStruct — her yeni nesneyi intrusive listeye ekler.
// Toplama: collect(globalSlots, callStack) — mark + sweep iki geçiş.
//
// Kök kaynakları:
//   - globalSlots : program genelinde yaşayan değerler
//   - callStack   : her aktif frame'in slot'ları (parametre + lokal + geçici)
//
// Çocuk kaynakları:
//   - ArrayObject::elements, StructObject::fields içindeki Ref değerleri
//   (markChildren() bunları kurgular)

struct CallFrame; // tam tanım call_frame.hpp'de

struct Heap {
    Object* head       = nullptr;
    int     allocCount = 0;

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

    // Tüm kök kaynaklarını işaretle.
    void markRoots(const std::vector<Value>&       globalSlots,
                   const std::vector<CallFrame>&   callStack);

    // ── Sweep ────────────────────────────────────────────────────────────────

    // İşaretlenmemiş nesneleri sil; işaretlenenlerin bitini sıfırla.
    void sweep();

    // ── Tam döngü ────────────────────────────────────────────────────────────

    void collect(const std::vector<Value>&     globalSlots,
                 const std::vector<CallFrame>& callStack) {
        markRoots(globalSlots, callStack);
        sweep();
    }

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
