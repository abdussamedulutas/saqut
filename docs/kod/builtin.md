# Yerleşik Metodlar (`src/builtin/`)

## Sorumluluk

Array, String ve Struct tipleri için built-in metodların merkezi kaydını
tutar. Sözdizimi: `ElementTipi::method(args)` — örn. `int[]::push(arr, 12)`.
Üç tüketicisi vardır: TypeChecker (imza doğrulama), SymbolCollector (built-in
çözümleme), LSP (otomatik tamamlama).

## Dosya envanteri

| Dosya | Rol |
|-------|-----|
| `builtin_methods.hpp` | `BuiltinMethodRegistry`, `BuiltinMethod`, `ParamRule`, `ReturnRule` — metod tanımları ve arama. |

## Ana tipler

```
ParamKind: Fixed | ElemType | ElemArray | StringVal
ReturnKind: Fixed | ElemType | ElemArray

ParamRule
  ├─ kind     : ParamKind
  └─ fixedType : Type (kind == Fixed ise kullanılır)

ReturnRule
  ├─ kind      : ReturnKind
  └─ fixedType : Type (kind == Fixed ise kullanılır)

BuiltinMethod
  ├─ name        : string
  ├─ params      : vector<ParamRule>
  ├─ returnRule  : ReturnRule
  └─ runtimeId   : int (VM dispatch için)

BuiltinMethodRegistry
  ├─ findByTypeName(typeName, methodName) → BuiltinMethod*
  └─ allMethods() → vector<BuiltinMethod>

Kategoriler:
  Array (id 0-10)   : length, push, pop, insert, remove, slice, reverse, concat,
                      contains, indexOf, clear
  String (id 11-23) : length, upper, lower, trim, split, substring, replace, repeat,
                      charAt, indexOf, contains, startsWith, endsWith
  Struct (id 24-25) : toJson, dump
```

## Diğer modüllerle temas

| Modül | İlişki |
|-------|--------|
| TypeChecker | ScopeCall ifadelerinde `findByTypeName()` ile metod imzasını doğrular. |
| VM | `dispatchBuiltinMethod()` ile runtimeId üzerinden O(1) dispatch yapar. |
| LSP | `allMethods()` ile otomatik tamamlama önerileri üretir. |
| Symbol | Built-in metodların sembol çözümlemesinde kullanılır. |

## Tasarım kararları

- **ParamRule/ReturnRule**: Parametre ve dönüş tipleri iki şekilde belirtilir:
  sabit tip (Fixed) veya çağrılan eleman tipinden türetilen (ElemType/ElemArray).
  Bu, generic bir dil tip çıkarımına gerek kalmadan polimorfik metodlara izin verir.
- **runtimeId**: VM dispatch'te switch-case için kullanılır. Yeni metod eklerken
  hem registry hem VM dispatch güncellenmelidir.
- **Kategoriler yalnızca array/string/struct**: Daha fazla kategori eklenebilir
  ancak mevcut dil özellikleriyle sınırlıdır.
- **BuiltinMethodRegistry salt-okunur**: Üç tüketici de tabloya yazma yapmaz,
  yalnızca okur. Tablo `builtin_methods.hpp` içinde statik olarak tanımlıdır.
