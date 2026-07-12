# Çekirdek (`src/core/`)

## Sorumluluk

Katman 0'da yer alan temel veri yapıları kümesi. Hiçbir saQut iç modülüne bağımlı
değildir (sadece standart C++ ve nlohmann/json kullanır). Tüm derleyici katmanları
(lexer, parser, semantic, IR, VM) tarafından kullanılan ortak tipleri barındırır:
tip sistemi, kaynak kod konumu ve satır yapısı, derleyici yapılandırması, decimal
aritmetik ve modül adı havuzu.

## Dosya envanteri

| Dosya | Rol |
|-------|-----|
| `type.hpp` | Tip sistemi — `Type` yapısı, `TypeKind`/`PrimitiveKind` enum'ları. Her ifadenin/sembolün veri tipini temsil eder. |
| `config.hpp` | Derleyici yapılandırması — hangi optimizasyon pass'lerinin çalışacağı (constant folding, DCE, fixpoint tavanı). |
| `location.hpp` | Kaynak kod konumu — `SourceLocation`, LSP uyumlu `LspPosition`/`LspRange`. |
| `sourcefile.hpp` / `sourcefile.cpp` | Kaynak kod yöneticisi — offset → (line, column) dönüşümü, satır metni erişimi. |
| `decimal.hpp` | DecimalValue — `coefficient × 10^exponent` temsili, kayıpsız ondalık aritmetik (ADR-028). |
| `module_registry.hpp` | Modül adı havuzu — string dosya yollarını int ID'ye eşler, string kopyalamayı önler. |

## Ana tipler ve ilişkileri

```
Type
  ├─ TypeKind    : Primitive | Array | Struct | Enum | Function | Error
  ├─ PrimitiveKind : Int | Float | Double | Decimal | Char | String | Bool | Void
  ├─ shared_ptr<Type>  → elementType (Array), returnType (Function)
  └─ vector<Type>      → paramTypes  (Function)

SourceFile
  ├─ text        : string — kaynak kodun tamamı (UTF-8)
  ├─ lineStarts  : vector<int> — her satırın başlangıç offset'i
  └─ offsetToLocation() → SourceLocation (filePath, line, column, offset)

SourceLocation
  ├─ line/column : 1-tabanlı
  ├─ offset      : 0-tabanlı
  └─ toLspPosition() → LspPosition (0-tabanlı, LSP protokolü için)

ModuleRegistry
  ├─ paths_      : vector<string> — ID → dosya yolu
  ├─ index_      : unordered_map   — dosya yolu → ID
  └─ INVALID_ID=-1, BUILTIN_ID=0  — özel sabitler

DecimalValue
  ├─ coeff (int64_t) × 10^exp (int32_t)
  ├─ ~18 anlamlı basamak
  └─ add/sub/mul/div/mod — __int128 ile taşma korumalı

CompilerConfig
  ├─ optConstantFolding (bool)
  ├─ optDeadCodeElim    (bool)
  └─ maxFixpointRounds  (int, varsayılan 10)
```

## Veri akışı

```
Kaynak dosya (disk/editör)
       ↓
  SourceFile.setText()  → lineStarts hesaplanır
       ↓
  Lexer okur, offset'leri token'lara ekler
       ↓
  Parser → AST düğümlerine SourceLocation atanır
       ↓
  Semantic + Tip denetleyici → Type karşılaştırması
       ↓
  IRGenerator → ModuleRegistry::intern() ile dosya yolu kaydedilir
       ↓
  VM/Interpreter → ModuleRegistry::filePath() ile hata konumu çözülür
```

## Diğer modüllerle temas

| Modül | İlişki |
|-------|--------|
| Tüm modüller | `Type` ve `SourceLocation` her yerde kullanılır — temel veri alışveriş birimi. |
| Lexer | `SourceFile` üzerinden token konumlarını belirler. |
| Parser | `Type::fromName()` ile string tip adından Type üretir. |
| Semantic (tip denetleyici) | `Type::equals()`, `isNumeric()`, `asNullable()` ile tip kurallarını uygular. |
| IRGenerator | `ModuleRegistry::intern()` ile dosya yollarını ID'ye çevirir. |
| Interpreter/VM | `ModuleRegistry::filePath()` ile hata konumlarını çözümler. |
| LSP | `SourceLocation::toLspPosition()` ile 0-tabanlı konum üretir. |

## Tasarım kararları

- **Gizli tip dönüşümü YOK** (ADR-010): `Type::equals()` yapısal ve katıdır;
  "int → float uyumu" gibi kurallar tip denetleyicinin işidir.
- **Nullable tipler** (ADR-021): `nullable` bayrağı + `asNullable()`/`asNonNull()`
  yardımcıları. `equalsBase()` nullable farkını yok sayar (`T == T?` çakışması için).
- **Error tipi** (TypeKind::Error): Hatalı/çözümlenememiş tipler için. Tip
  denetleyicide Error operandlı ifadeye yeni hata üretilmez (ardışık sahte hatayı
  bastırır).
- **Tamsayı literali bağlama-göre tiplenir** (ADR-010): Literal tipini Type değil
  parser/semantic belirler; Type bu kararı yalnızca depolar.
- **ModuleRegistry string kopyalamaz**: Dosya yolları intern edilir, IR ve VM
  boyunca int ID kullanılır.
- **DecimalValue harici bağımlılıksız** (ADR-028): Sıfır bağımlılık, GCC/Clang/MSVC
  taşınabilir. `__int128` ile taşma korumalı aritmetik.
- **Instruction union değil düz struct**: İncelenebilirlik önceliği.
- **SourceLocation 1-tabanlı line/column**; LSP 0-tabanlı ister — `toLspPosition()`
  dönüşümü bu farkı kapatır.

## Bilinen sınırlar / TODO

- Decimal aritmetikte `__int128` GCC/Clang'a özgüdür; MSVC'de taşınabilirlik
  sorunu olabilir (şu an hedef platform değil).
- `computeLineStarts()` yalnızca `\n` bazlıdır; `\r\n` (Windows) doğru işlenir,
  ancak `\r` tek başına (eski Mac) yeni satır sayılmaz.
- `Type::fromName()` yalnızca bilinen tipleri tanır; kullanıcı tanımlı struct/enum
  adları için ayrı çözümleme gerekir (semantic katmanında).
