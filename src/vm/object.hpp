#ifndef SAQUT_VM_OBJECT
#define SAQUT_VM_OBJECT

#include <vector>

// ADR-022: GC-hazır nesne modeli (v1: toplama yok, sadece header + liste kancası).
// Her heap nesnesi bu yapıdan türer.
// marked + next: mark-sweep için hazır; v1'de kullanılmaz.
// TODO(#56): mark-sweep v2 — Heap::collect() bu header'ı kullanacak.

enum class ObjectType { Array /*, Struct, String (ileride) */ };

struct Object {
    ObjectType type;
    bool       marked = false;  // mark-sweep için (v2); şimdilik kullanılmaz
    Object*    next   = nullptr; // "tüm nesneler" intrusive listesi
};

// Forward declare — Value, Object*'ı taşır; Object, Value içerir.
// Gerçek tanım value.hpp'den sonra gelir; burada sadece forward.
struct Value;

struct ArrayObject : Object {
    std::vector<Value> elements;
    explicit ArrayObject(int capacity = 0) {
        type = ObjectType::Array;
        if (capacity > 0) elements.reserve(capacity);
    }
};

// ── Heap ─────────────────────────────────────────────────────────────────────
// Tüm nesneleri intrusive listede tutar. v1'de serbest bırakma yok.
// TODO(#56): mark-sweep v2 — collect() kök taraması yapacak, ölü nesneleri silecek.
struct Heap {
    Object* head = nullptr;
    int     allocCount = 0;

    ArrayObject* allocArray(int capacity = 0) {
        auto* obj  = new ArrayObject(capacity);
        obj->next  = head;
        head       = obj;
        allocCount++;
        return obj;
        // TODO(#56): mark-sweep kök taraması buradan — GC eşiği aşılınca tetikle
    }

    // v1: process exit'te OS toplar; yıkıcı tüm nesneleri siler.
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
