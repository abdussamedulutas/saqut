// ============================================================================
// saQut VM — Object/Heap Gerçeklemesi
// ============================================================================

#include "vm/object.hpp"
#include "vm/value.hpp"

// ── Yardımcı: tek bir nesneyi ve geçişli çocuklarını işaretle ────────────────

// #217: tricolor marking — markObject artık yalnızca Grey yapar
// (çocuk taraması gcStep'te yapılır)
static void markObject(Object* obj) {
    if (!obj || obj->markState != MarkState::White) return;
    obj->markState = MarkState::Grey;
    obj->marked = true;
}

// #217: bir nesnenin çocuklarını tara (Grey → Black)
// Dönüş: bu adımda işlenen nesne sayısı (budget takibi için)
int drainGrey(Heap* heap, int budget) {
    int processed = 0;
    Object* cur = heap->head;
    while (cur && processed < budget) {
        if (cur->markState == MarkState::Grey) {
            cur->markChildren();
            cur->markState = MarkState::Black;
            ++processed;
        }
        cur = cur->next;
    }
    return processed;
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

// ── Heap::sweep ──────────────────────────────────────────────────────────────
//
// İntrusive listede iki işaretçiyle gezilir:
//   prev → bir önceki düğümün "next" alanına yazı için
//   cur  → şu an incelenen nesne
//
// İşaretlenmemiş (erişilemeyen) nesneler listeden çıkarılır ve silinir.
// İşaretlenmiş nesnelerin marked biti sıfırlanır — bir sonraki döngüye hazır.

int Heap::sweep() {
    Object** prev  = &head;
    Object*  cur   = head;
    int      freed = 0;

    while (cur) {
        // #217: markState'i sıfırla (incremental marking state)
        cur->markState = MarkState::White;
        if (!cur->marked) {
            // Erişilemeyen nesne — listeden çıkar ve sil
            Object* dead = cur;
            *prev = cur->next;
            cur   = cur->next;
            delete dead;
            --allocCount;
            ++freed;
        } else {
            // Canlı nesne — işaret bitini sıfırla, ilerle
            // markState zaten yukarıda sıfırlandı
            cur->marked = false;
            prev = &cur->next;
            cur  = cur->next;
        }
    }

    ++gcRuns;
    freedTotal += freed;
    return freed;
}

// ── ArrayObject::markChildren ────────────────────────────────────────────────

void ArrayObject::markChildren() {
    // #206: primitive array (Byte/Int/LongInt/Float32/Float64/Decimal) → çocuk yok, O(1)
    if (elemKind != ArrayElemKind::Ref) return;
    for (const Value& v : elements)
        if (v.kind == ValueKind::Ref)
            markObject(v.ref);
}

void writeBarrier(Object* target, Object* newRef) {
    if (!target || !newRef) return;
    if (target->markState == MarkState::Black &&
        newRef->markState == MarkState::White) {
        target->markState = MarkState::Grey;
    }
}

// ── StructObject::markChildren ───────────────────────────────────────────────

void StructObject::markChildren() {
    for (const Value& v : fields)
        if (v.kind == ValueKind::Ref)
            markObject(v.ref);
}
