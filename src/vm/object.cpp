// ============================================================================
// saQut VM — Object/Heap Gerçeklemesi
// ============================================================================

#include "vm/object.hpp"
#include "vm/value.hpp"
#include "vm/call_frame.hpp"

// ── Yardımcı: tek bir nesneyi ve geçişli çocuklarını işaretle ────────────────

static void markObject(Object* obj) {
    if (!obj || obj->marked) return;
    obj->marked = true;
    obj->markChildren(); // çocukları özyinelemeli işaretle
}

// ── Heap::markValue ──────────────────────────────────────────────────────────

void Heap::markValue(const Value& v) {
    if (v.kind == ValueKind::Ref)
        markObject(v.ref);
}

// ── Heap::markSlots ──────────────────────────────────────────────────────────

void Heap::markSlots(const std::vector<Value>& slots) {
    for (const Value& v : slots)
        markValue(v);
}

// ── Heap::markRoots ──────────────────────────────────────────────────────────

void Heap::markRoots(const std::vector<Value>&     globalSlots,
                     const std::vector<CallFrame>& callStack) {
    markSlots(globalSlots);
    for (const CallFrame& frame : callStack)
        markSlots(frame.slots);
}

// ── Heap::sweep ──────────────────────────────────────────────────────────────
//
// İntrusive listede iki işaretçiyle gezilir:
//   prev → bir önceki düğümün "next" alanına yazı için
//   cur  → şu an incelenen nesne
//
// İşaretlenmemiş (erişilemeyen) nesneler listeden çıkarılır ve silinir.
// İşaretlenmiş nesnelerin marked biti sıfırlanır — bir sonraki döngüye hazır.

void Heap::sweep() {
    Object** prev = &head;
    Object*  cur  = head;

    while (cur) {
        if (!cur->marked) {
            // Erişilemeyen nesne — listeden çıkar ve sil
            Object* dead = cur;
            *prev = cur->next;
            cur   = cur->next;
            delete dead;
            --allocCount;
        } else {
            // Canlı nesne — işaret bitini sıfırla, ilerle
            cur->marked = false;
            prev = &cur->next;
            cur  = cur->next;
        }
    }
}

// ── ArrayObject::markChildren ────────────────────────────────────────────────

void ArrayObject::markChildren() {
    for (const Value& v : elements)
        if (v.kind == ValueKind::Ref)
            markObject(v.ref);
}

// ── StructObject::markChildren ───────────────────────────────────────────────

void StructObject::markChildren() {
    for (const Value& v : fields)
        if (v.kind == ValueKind::Ref)
            markObject(v.ref);
}
