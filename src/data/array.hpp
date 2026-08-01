// ============================================================================
// saQut — array veri tipi (arayüz)
// ============================================================================
//
// DİZİN:   src/data/array.hpp
// KATMAN:  data — array'in çalışma zamanındaki TEK sahibi
//
// BELLEK TEMSİLİ (ADR-020 / #206):
//   array REFERANS tiplidir: ArrayObject* hem VM'de (Value::ref) hem sınırda
//   (HostKind::Ref) hem JIT'te aynı pointer'dır — string'in aksine tek temsil.
//
//   Elemanlar PACKED tutulur: eleman tipine göre yedi buffer'dan biri kullanılır
//   (Ref/Byte/Int/LongInt/Float32/Float64/Decimal). Küçük sayısal dizilerde
//   eleman başına 80 baytlık Value ödememek içindir.
//
//   ArrayObject tanımı vm/object.hpp'dedir çünkü GC nesne ailesine aittir;
//   GC işaretleme/süpürme bu modülün kapsamı DIŞINDADIR.
//
// ============================================================================

#ifndef SAQUT_DATA_ARRAY
#define SAQUT_DATA_ARRAY

#include "data/data_type.hpp"

// array'in tüm metodları: length, push, pop, insert, remove, slice, reverse,
// concat, contains, indexOf, clear.
//
// Yeni metod eklemek = array.cpp'deki tabloya bir satır. Başka dosya yok.
const std::vector<DataMethod>& dataArrayMethods();

// ── Packed eleman erişimi (dışa açık) ────────────────────────────────────────
//
// Array'in yedi eleman buffer'ı arasındaki dallanma YALNIZCA array.cpp'de
// yapılır. Diğer modüller (ör. struct'ın JSON serileştirmesi) elemanlara bu
// iki fonksiyonla erişir ve eleman tiplerini hiç bilmez.
//
// Eski kodda bu 7-dallı switch sekiz metodda + JSON serileştirmede ayrı ayrı
// tekrarlanıyordu; yeni bir eleman tipi eklemek her birine dokunmak demekti.
struct ArrayObject;
int   dataArraySize(const ArrayObject* arr);
Value dataArrayElemAt(const ArrayObject* arr, int idx);

#endif // SAQUT_DATA_ARRAY
