// ============================================================================
// saQut VM — Object/Heap Gerçeklemesi
// ============================================================================

#include "vm/object.hpp"
#include "vm/value.hpp"

// ── Yardımcı: tek bir nesneyi ve geçişli çocuklarını işaretle ────────────────

// #217: tricolor marking — markObject yalnızca Grey yapar (çocuk taraması
// drainGrey'de yapılır). Faz 3: `marked` bool'u KALDIRILDI — markState tek
// canlılık kaynağıdır (aynı bilgi iki alanda tutulmaz).
static void markObject(Object* obj) {
    if (!obj || obj->markState != MarkState::White) return;
    obj->markState = MarkState::Grey;
}

// Faz 3: sanal markChildren yerine TIP ETIKETI switch'i — nesne başına
// vptr ve sanal çağrı kalkar. Yeni ObjectType eklendiğinde BURAYA ve
// deleteObject'e case eklenmelidir (iki switch yan yana, yorumlu).
static void markObjectChildren(Object* obj) {
    switch (obj->type) {
        case ObjectType::Array: {
            auto* a = static_cast<ArrayObject*>(obj);
            // #206: primitive array (Byte/Int/.../Decimal) -> çocuk yok, O(1)
            if (a->elemKind != ArrayElemKind::Ref) return;
            for (const Value& v : a->elements)
                if (v.kind == ValueKind::Ref || v.kind == ValueKind::String)
                    markObject(v.ref());  // tek-string-modeli: string de heap nesnesi
            return;
        }
        case ObjectType::Struct:
            for (const Value& v : static_cast<StructObject*>(obj)->fields)
                if (v.kind == ValueKind::Ref || v.kind == ValueKind::String)
                    markObject(v.ref());
            return;
        case ObjectType::String:   // string'in ref çocuğu yok
        case ObjectType::Decimal:  // decimal'in ref çocuğu yok
            return;
    }
}

// Tip-güvenli silme (vptr yok -> taban işaretçiden delete edilemez).
static void deleteObject(Object* obj) {
    switch (obj->type) {
        case ObjectType::Array:   delete static_cast<ArrayObject*>(obj);   return;
        case ObjectType::Struct:  delete static_cast<StructObject*>(obj);  return;
        case ObjectType::String:  delete static_cast<StringObject*>(obj);  return;
        case ObjectType::Decimal: delete static_cast<DecimalObject*>(obj); return;
    }
}

// #217: bir nesnenin çocuklarını tara (Grey -> Black)
// Dönüş: bu adımda işlenen nesne sayısı (budget takibi için)
int drainGrey(Heap* heap, int budget) {
    int processed = 0;
    Object* cur = heap->head;
    while (cur && processed < budget) {
        if (cur->markState == MarkState::Grey) {
            markObjectChildren(cur);
            cur->markState = MarkState::Black;
            ++processed;
        }
        cur = cur->next;
    }
    return processed;
}

// ── Value string tahsis kancası (tek-string-modeli) ─────────────────────────
//
// Value::fromString bu fonksiyonla tahsis eder. Aktif Heap bağlıysa string
// GC'li yolda yaşar (Interpreter/JIT çalışma başında bağlar); bağlı değilse
// (birim testleri, izole kullanım) yedek havuzda süresiz tutulur — eskiden
// inline string de otomatik yaşardı, davranış eşdeğerdir. Kanca
// thread_local'dır: her iş parçacığı kendi heap'ini bağlar (JitRuntime/rt()
// modeliyle aynı karar).
namespace {
thread_local Heap* t_valueStringHeap = nullptr;
}

void setValueStringHeap(Heap* h) { t_valueStringHeap = h; }

// Value::stringValue gövdesi burada: StringObject'un tam tanımı yalnızca
// bu katmanda mevcuttur (value.hpp yalnız ileri bildirim taşır).
const std::string& Value::stringValue() const {
    return static_cast<StringObject*>(p.r)->data;
}

Object* allocValueString(std::string s) {
    if (t_valueStringHeap) return t_valueStringHeap->allocString(std::move(s));
    static thread_local std::vector<std::unique_ptr<StringObject>> fallback;
    fallback.push_back(std::make_unique<StringObject>(std::move(s)));
    return fallback.back().get();
}

// ── Heap::markValue ──────────────────────────────────────────────────────────

void Heap::markValue(const Value& v) {
    // String de heap nesnesidir (tek-string-modeli): Ref gibi köklenir.
    // StringObject::markChildren no-op'tur — string'in ref çocuğu yoktur.
    if (v.kind == ValueKind::Ref || v.kind == ValueKind::String)
        markObject(v.ref());
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
    int freed = 0;
    Object* cur = head;

    while (cur) {
        Object* next = cur->next;
        // Tur sonunda canlı = markState Black (incremental işlemler bitti).
        // White = erişilemez -> listeden O(1) çıkar (çift yönlü bağ) ve sil.
        if (cur->markState == MarkState::White) {
            unlink(cur);
            deleteObject(cur);
            --allocCount;
            ++freed;
        } else {
            cur->markState = MarkState::White;  // bir sonraki tura hazır
        }
        cur = next;
    }

    ++gcRuns;
    freedTotal += freed;
    return freed;
}

// Çift yönlü bağdan O(1) çıkarma — agc/devir/move için de tek nokta.
void Heap::unlink(Object* obj) {
    if (obj->prev) obj->prev->next = obj->next;
    else           head = obj->next;
    if (obj->next) obj->next->prev = obj->prev;
    obj->next = obj->prev = nullptr;
}

Heap::~Heap() {
    Object* cur = head;
    while (cur) {
        Object* nxt = cur->next;
        deleteObject(cur);
        cur = nxt;
    }
}

// (Array/Struct markChildren tanimlari markObjectChildren switch'ine tasinmistir - yukarida)

void writeBarrier(Object* target, Object* newRef) {
    if (!target || !newRef) return;
    if (target->markState == MarkState::Black &&
        newRef->markState == MarkState::White) {
        target->markState = MarkState::Grey;
    }
}
