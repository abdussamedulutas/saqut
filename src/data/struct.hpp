// ============================================================================
// saQut — struct veri tipi (arayüz)
// ============================================================================
//
// DİZİN:   src/data/struct.hpp
// KATMAN:  data — struct'ın çalışma zamanındaki TEK sahibi
//
// BELLEK TEMSİLİ (ADR-020):
//   struct REFERANS tiplidir: StructObject* her katmanda aynı pointer.
//   Alanlar Value dizisidir; alan ADLARI örnek başına kopyalanmaz, paylaşılan
//   bir tabloya (fieldNames) işaret eder.
//
//   StructObject tanımı gc/gc_object.hpp'dedir (GC nesne ailesi). GC işaretleme,
//   FIELD_GET/FIELD_SET opcode'ları ve zero-init maskesi bu modülün DIŞINDADIR.
//
// ============================================================================

#ifndef SAQUT_DATA_STRUCT
#define SAQUT_DATA_STRUCT

#include "data/data_type.hpp"

// struct'ın metodları: toJson, dump.
const std::vector<DataMethod>& dataStructMethods();

#endif // SAQUT_DATA_STRUCT
