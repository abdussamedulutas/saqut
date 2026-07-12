# saQut Kod Standardı

> Bu belge, saQut C++ kod tabanında uyulması gereken yazım ve biçim
> kurallarını tanımlar. Amaç, tüm kodun tek bir elden çıkmış gibi tutarlı
> görünmesini sağlamaktır. `.clang-format` bu kuralların bir kısmını otomatik
> uygular; geri kalanı el ile sağlanır.

## 1. Dosya Organizasyonu

### 1.1. Header / Gerçekleme Ayrımı

| Kural | Açıklama |
|-------|----------|
| Header uzantısı | `.hpp` |
| Gerçekleme uzantısı | `.cpp` |
| Header-only eğilim | ADR-003: Mümkün olan her yerde header-only yaz. Sadece derleme süresini ciddi şişiren veya döngüsel bağımlılık yaratan durumlarda `.cpp`'ye ayır. |

### 1.2. Dosya Başlık Yorumu

Her header ve `.cpp` dosyası şu formatta bir blok yorumla başlar:

```cpp
// ============================================================================
// saQut <MODÜL> — <Kısa Açıklama>
// ============================================================================
//
// DİZİN:   src/<modül>/<dosya>.hpp
// KATMAN:  Katman N — <katman açıklaması>
// BAĞIMLI: <bağımlılıklar>
// KULLANAN: <bu dosyayı kullanan modüller>
//
// AMAÇ:
//   <dosyanın ne yaptığı, neden var olduğu>
//
// ============================================================================
```

KATMAN, BAĞIMLI, KULLANAN alanları zorunlu değildir; dosyanın karmaşıklığına
göre eklenir. AMAÇ her zaman yazılır.

### 1.3. Include Guard

`#pragma once` **kullanılmaz.** Tüm header'larda `#ifndef` / `#define` / `#endif`
formatı:

```cpp
#ifndef SAQUT_MODÜL_DOSYA
#define SAQUT_MODÜL_DOSYA
// ...
#endif // SAQUT_MODÜL_DOSYA — yorum opsiyonel
```

İsimlendirme: `SAQUT_<MODÜL>_<DOSYA>` — büyük harf, alt çizgi ayrımı.

### 1.4. Include Sıralaması

1. Kendi header'ı (`.cpp` dosyasında)
2. Aynı modülün diğer header'ları
3. Diğer `src/` modülleri
4. `vendor/` (nlohmann/json.hpp gibi)
5. Standart kütüphane (`<string>`, `<vector>`, …)

Her grupta alfabetik sıralama. Grup aralarında boş satır **yok** — art arda
yazılır.

Örnek (`src/lsp/document_store.cpp`):
```cpp
#include "lsp/document_store.hpp"
#include "lsp/uri.hpp"
#include "lsp/position.hpp"
#include "module/module_loader.hpp"
#include "symbol/symbol_collector.hpp"
#include "semantic/type_checker.hpp"
#include "semantic/structural_validator.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
```

## 2. Girinti ve Boşluk

### 2.1. Temel Kurallar

| Kural | Değer |
|-------|-------|
| Girinti birimi | 4 boşluk |
| Tab karakteri | **Asla** kullanılmaz |
| Satır sonu boşluğu | **Yasak** (`.clang-format` temizler) |
| Dosya sonu newline | **Zorunlu** |
| Maksimum satır uzunluğu | 100 karakter (esnek, aşırı kasmadan) |

### 2.2. Boş Satırlar

- Üst seviye fonksiyon/sınıf tanımları arasına **bir** boş satır.
- Mantıksal bölümler arasına bir boş satır.
- Dosya başlık bloğu ile ilk `#include`/`#ifndef` arasına **bir** boş satır.

### 2.3. Operatör Çevresi

Tüm ikili ve üçlü operatörlerin çevresinde birer boşluk:

```cpp
int z = x + y;          // doğru
int z=x+y;              // yanlış
bool ok = a && b;       // doğru
```

Atama ve karşılaştırma dâhil.

### 2.4. Virgül ve Noktalı Virgül

Virgülden sonra bir boşluk, önce boşluk yok:

```cpp
fn(1, 2, 3);            // doğru
fn(1,2,3);              // yanlış
```

## 3. Parantez ve Blok

### 3.1. Süslü Parantez Stili

**Attach (Java/C# stili):** Açan parantez satır sonunda, kapatan kendi başına:

```cpp
if (x > 0) {
    return x;
}

while (true) {
    doWork();
}

void foo() {
    bar();
}
```

Fonksiyon, `if`, `for`, `while`, `switch`, `try/catch`, `struct`, `class`,
`enum class` hepsi bu stildedir.

### 3.2. Tek İfadelik Bloklar

Süslü parantez **her zaman yazılır**, tek ifadelik blokta bile:

```cpp
// doğru
if (x > 0) {
    return x;
}

// yanlış
if (x > 0) return x;
if (x > 0)
    return x;
```

### 3.3. Parantez İçi Boşluk

Parantez içinde boşluk yok:

```cpp
foo(a, b);              // doğru
foo( a, b );            // yanlış
```

## 4. Adlandırma

### 4.1. Özet Tablosu

| Öğe | Stil | Örnek |
|-----|------|-------|
| `class` / `struct` | PascalCase | `DocumentStore`, `IRGenerator`, `TryFrame` |
| `enum class` adı | PascalCase | `TypeKind`, `Opcode`, `RunState` |
| `enum class` üyesi | PascalCase | `TypeKind::Primitive`, `RunState::Running` |
| Metod / fonksiyon | camelCase | `runPipeline()`, `findSymbolAt()`, `freshSlot()` |
| Üye değişken | camelCase + `_` | `out_`, `nextSlot_`, `store_`, `responseSeq_` |
| Yerel değişken | camelCase | `irFn`, `slotIndex`, `globalVars` |
| Parametre | camelCase | `const std::string& uri`, `int frameDepth` |
| Global / sabit | UPPER_SNAKE_CASE | `ERROR_FIELD_COUNT` |
| `#define` guard | `SAQUT_MODÜL_DOSYA` | `SAQUT_LSP_DOCUMENT_STORE` |
| Dosya adı | snake_case | `document_store.hpp`, `dap_handler.cpp` |
| Klasör adı | snake_case | `symbol/`, `document_store/` değil |

### 4.2. Üye Değişken Soneki

Tüm private/protected üye değişkenler `_` ile biter. Public üyeler (struct
veri alanları, örn. `Type` / `Value` / `INumber`) soneksizdir:

```cpp
class DapHandler {
private:
    std::ostream& out_;             // private → _
    int           nextBpId_ = 1;    // private → _
    // ...
};

struct Type {
    TypeKind kind = TypeKind::Error; // public struct alanı → _ yok
    // ...
};
```

### 4.3. Fonksiyon Adlandırması

Fonksiyon ve metod adları fiil ile başlar (aksiyon):

```cpp
void enterScope();           // fiil
Symbol* resolve(...);        // fiil
int generateExpression(...); // fiil
bool isTruthy() const;       // soru (is/has öneki)
bool hasStruct(...) const;   // soru
```

### 4.4. enum class Üye Adlandırması

`enum class` üyeleri PascalCase, `_` ayracı **kullanılmaz** (istisna: IR
opcode'ları `LOAD_CONST` gibi tarihsel sebeplerle UPPER_SNAKE_CASE):

```cpp
enum class TypeKind { Primitive, Array, Struct, Enum, Function, Error };   // doğru
enum class ValueKind { Int, Float, Decimal, String, Ref, Null };           // doğru
enum class Opcode { ADD, LOAD_CONST, JIF_FALSE, ... };                     // istisna
```

## 5. Struct vs Class

| Öğe | Ne zaman |
|-----|----------|
| `struct` | Veri taşıyıcı; çoğu üyesi public; davranış az veya hiç yok. `Type`, `Value`, `INumber`, `CallFrame` |
| `class` | Davranış ve durum birlikte; private üyeler + public arayüz. `DocumentStore`, `DapHandler`, `Interpreter` |

`struct`'ta `public:` etiketi yazılmaz (zaten public). `class`'ta `public:`
etiketi 4 boşluk girintili, altındaki üyeler 8 boşluk (yani `public:` ile
aynı hizada düşünülmez — blok başlangıcı gibi davranılır).

## 6. Pointer ve Referans

### 6.1. Hizalama

`*` ve `&` **türün yanına** yapışır:

```cpp
std::string&           name;       // doğru
std::string            &name;      // yanlış
ASTNode*               parent;     // doğru
ASTNode                *parent;    // yanlış
```

### 6.2. Sahiplik

| Amaç | Araç |
|------|------|
| Tek sahiplik | `std::unique_ptr<T>` |
| Paylaşımlı sahiplik | `std::shared_ptr<T>` |
| Sahipsiz referans (çoğu durum) | Ham pointer `T*` |
| Opsiyonel (nullable) referans | `T*` |
| Asla null olmayan referans | `T&` |

Ham pointer silinmez — silmeyi sahibi (`unique_ptr` veya üst scope) yapar.

## 7. Construct / Destruct / Copy

### 7.1. Varsayılan Değerle Başlatma

Tüm üye değişkenler **tanımlandıkları yerde** varsayılan değer alır. Bu,
constructor'da unutulan başlatmayı önler:

```cpp
struct Value {
    ValueKind    kind         = ValueKind::Int;
    int          intValue     = 0;
    double       floatValue   = 0.0;
    DecimalValue decimalValue;
    std::string  stringValue;
    Object*      ref          = nullptr;
};
```

### 7.2. Copy/Move

- `= default` ile varsayılan kullanılacaksa açıkça yaz.
- Copy/Move silinecekse `= delete` ile açıkça yaz.
- Rule of Five: destructor, copy/move ctor, copy/move assign tanımlanacaksa
  beşi birden düşünülür.

```cpp
DocumentState(const DocumentState&) = delete;
DocumentState& operator=(const DocumentState&) = delete;
```

### 7.3. explicit

Tek parametreli constructor'lar `explicit` işaretlenir (istemli implicit
dönüşüm değilse):

```cpp
explicit Interpreter(IRProgram& program) : program_(program) {}
explicit DapHandler(std::ostream& out) : out_(out) {}
```

## 8. Yorum ve Dokümantasyon

### 8.1. Dil

**Tüm yorumlar Türkçe** yazılır. Kod (değişken/fonksiyon adları) İngilizce'dir.

### 8.2. Blok Yorumu (Büyük Bölüm Ayracı)

```cpp
// ── Bölüm Adı ──────────────────────────────────────────────────────────────
```

Büyük dosyalarda mantıksal bölümleri ayırmak için kullanılır. `─` karakteriyle
çizilir, sağa doğru aynı karakterle doldurulur.

### 8.3. Doxygen / Docstring

Zorunlu değil. Sınıf başındaki AMAÇ bloğu yeterli görülür. Karmaşık
fonksiyonlara kısa `//` satırı yazılabilir:

```cpp
// ── Değişken değerini DAP string'ine çevir (struct/array için özet) ──
std::string valueToString(const Value& v, int depth = 0) const;
```

### 8.4. TODO İşaretleme

Geçici/iskele kod `TODO` ile işaretlenir:

```cpp
// TODO(faz-ileri): SymbolCollector bir gün gerçekten yarıda kesilebilir hale
// gelirse (bugün mümkün değil — hep tamamlanır), gerçek bir "son iyi" anlık
// görüntüsü için SymbolTable derin kopyalanabilir hale getirilmeli.
```

Format: `TODO(faz-X):` veya `TODO(issue-N):` veya serbest `TODO:`.

## 9. Modern C++ Kullanımı

### 9.1. Kullanılan Özellikler

| Özellik | Kullanım |
|---------|----------|
| `enum class` | Her yerde, düz `enum` asla |
| `nullptr` | `NULL` veya `0` yerine her zaman |
| `auto` | İteratör, `make_unique`, uzun tip adlarında — aşırıya kaçmadan |
| Range-for | `for (auto& x : container)` |
| `override` | Tüm virtual override'larda zorunlu |
| `std::move` | Taşıma semantiği gerektiğinde |
| `using` alias | `namespace fs = std::filesystem;` gibi |
| Brace init | Tercihen `= default`; `{}` yalnızca gerekli yerde |

### 9.2. Kaçınılan Özellikler

| Özellik | Gerekçe |
|---------|---------|
| İstisna (exception) | ADR-025: saQut kendi hata yönetimini kullanır; C++ exception'ı yalnızca VM içinde `throw`/`catch` |
| RTTI (`dynamic_cast`) | Performans + tutarlılık; `enum class ASTKind` ile `static_cast` kullan |
| `using namespace std;` | Hiçbir header veya `.cpp`'de kullanılmaz |
| Makro (işlevsel `#define`) | Sadece include guard için |
| `printf`/`scanf` ailesi | `std::cout` / `std::ostringstream` / `std::to_string` |

## 10. Spesifik Desenler

### 10.1. Factory Metod (static)

Yaygın kullanılan kalıp — `struct` içinde `static` factory:

```cpp
struct Value {
    static Value fromInt(int n) { ... }
    static Value null() { ... }
};

struct Type {
    static Type Int() { ... }
    static Type array(Type elem) { ... }
};
```

### 10.2. Ziyaretçi / Kind Switch

AST ve Value gibi varyant tipler `enum class` kind + `switch` ile işlenir:

```cpp
switch (v.kind) {
    case ValueKind::Int:     return ...;
    case ValueKind::Float:   return ...;
    case ValueKind::String:  return ...;
    // ...
}
```

## 11. CMake / Build

- `build/` git'te izlenmez (`.gitignore`'da)
- Derleme: `cmake -B build && ninja -C build`
- Yeni `.cpp` dosyası eklenince `CMakeLists.txt`'deki `SOURCES` listesine eklenir
- Header-only dosyalar için CMake'de işlem gerekmez

## 12. .clang-format ile Otomatik Uygulama

Proje kökündeki `.clang-format` bu kuralların çoğunu otomatik uygular:

- Girinti (4 boşluk)
- Parantez stili (attach)
- Pointer hizalama (sola)
- Boşluk kuralları
- Include sıralaması

El ile uygulanan kurallar (`.clang-format`'ın kapsamadığı):
- Dosya başlık bloğu formatı
- Adlandırma (camelCase, PascalCase, `_` soneki)
- Yorum dili (Türkçe)
- `class` vs `struct` ayrımı
- Include guard formatı
- Üye değişken başlatma (in-class default)
- `explicit` kullanımı
