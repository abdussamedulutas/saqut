// ============================================================================
// saQut — Veri Tipi Registry'si
// ============================================================================
//
// DİZİN:   src/data/data_registry.hpp
// KATMAN:  data — tüm veri tipi modüllerini tek indeks uzayında toplar
//
// AMAÇ (#223):
//   string.cpp / array.cpp / struct.cpp kendi metod tablolarını verir; bu
//   registry onları tek bir düz listede birleştirir ve iki tüketiciye sunar:
//
//     1. Derleme zamanı  — TypeChecker, SymbolTable, LSP: imza doğrulama,
//                          dönüş tipi çözme, otomatik tamamlama.
//     2. Çalışma zamanı  — rt_host_call: id → thunk, O(1).
//
//   İkisi AYNI kaydı okur. Eskiden imza builtin_methods.hpp'de, gövde
//   interpreter.cpp'deki switch'teydi ve aralarında elle korunan bir sıra
//   sözleşmesi vardı — bir metodu tabloda yukarı taşımak sessizce yanlış
//   gövdeyi çağırırdı. Artık ayrılamazlar.
//
// YENİ VERİ TİPİ EKLEMEK:
//   1. src/data/<tip>.{hpp,cpp} yaz, data<Tip>Methods() sağla
//   2. dataAllMethods() içine bir satır ekle
//   Başka hiçbir dosyaya dokunulmaz.
//
// ============================================================================

#ifndef SAQUT_DATA_REGISTRY
#define SAQUT_DATA_REGISTRY

#include <string>
#include <vector>

#include "data/data_type.hpp"

// Tüm veri tiplerinin metodları, tek düz liste. İndeks = runtime id.
//
// Sıra, modüllerin dataAllMethods() içindeki sırasıdır ve KARARLIDIR: id'ler
// IR'ye gömülür (CALLHOST::intValue), dolayısıyla mevcut bir kaydın indeksi
// kayarsa eski IR yanlış metoda gider. Yeni kayıtlar SONA eklenir.
const std::vector<DataMethod>& dataAllMethods();

// Derleme zamanı arama — TypeChecker / SymbolTable / LSP kullanır.
//
// leftName: "int", "string", "Person", ...
// isStruct: TypeChecker'ın hasStruct() sonucu (struct array metodları için)
// Dönüş: nullptr = bu tip için böyle bir metod yok
const DataMethod* dataLookupMethod(const std::string& leftName,
                                   const std::string& methodName,
                                   bool               isStruct,
                                   bool               isReceiverArray);

// Bir metodun runtime id'si (dataAllMethods indeksi). Bulunamazsa -1.
int dataMethodId(const DataMethod* m);

// id → kayıt; geçersizse nullptr.
const DataMethod* dataMethodAt(int id);

// TypeChecker yardımcısı: leftName'den eleman tipini çöz.
// "int" → int, "string" → string, "Person" → struct Person
Type dataResolveElemType(const std::string& leftName);

#endif // SAQUT_DATA_REGISTRY
