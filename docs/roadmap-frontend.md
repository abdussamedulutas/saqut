# saQut Frontend Yol Haritası — Symbol Table + Semantic Analiz + Optimizasyon

> Bu belge, frontend'i tamamlamaya yönelik dosya-dosya uygulama planıdır.
> Kararların gerekçeleri: `docs/adr-frontend-analiz.md` (ADR-006…019).
> Tartışma akışı: `docs/transkript-frontend-tasarim.md`.
>
> **İlke:** Sıralama katıdır — her faz bir öncekine dayanır. Her faz sonunda
> geçerli örnekle (`examples/fibonacci.sqt`, `examples/source.sqt`) ve CLI
> komutlarıyla doğrulama yapılır; regresyon olmamalıdır. Kod temiz, anlaşılır ve
> yorum satırlarıyla takip edilebilir olmalıdır (header-only tarzı korunur,
> bkz. ADR-003).
>
> ⚠️ **Yapılan vs planlanan:** Bugün çalışan = lexer, tokenizer, Pratt parser,
> AST, AST'nin JSON serileştirmesi, CLI iskeleti, konum takibi, basit aritmetiği
> düşüren minimal IR deneyi. Bu yol haritasındaki **her şey planlıdır** (sembol
> tablosu, semantik analiz, tip sistemi, diagnostic, optimizasyon).
>
> 🎯 **Bu haftanın işi:** **sembol tablosu + iki-geçişli toplayıcı** (Faz 2),
> hedef **"fibonacci'yi derle ve çalıştır"** (`examples/fibonacci.sqt`). Faz 0–1
> bunun önkoşuludur.
>
> 🧭 **Önce dikey dilim, sonra çerçeve.** Bir şey çalışmadan önce genel pass
> manager / evrensel config / ağır soyutlama inşa etme. Uçtan uca tek bir dilim
> (kaynak → IR → çalıştır; tamsayı + değişken + kontrol akışı + tek `print`)
> önce çalışsın. Faz 4'ün framework'ü (OptimizationManager, fixpoint, config)
> ancak Faz 0–3 fibonacci'yi geçirdikten **sonra** anlam kazanır — erken
> soyutlama daha az değil, daha çok karmaşıklıktır.

---

## Genel Bakış

```
Faz 0  Temeller        Type sınıfı + Diagnostic modülü + Hata kataloğu
Faz 1  AST Refactor    ExpressionNode/StatementNode + analiz alanları
Faz 2  Symbol Table    Symbol + Scope + SymbolTable + iki-geçişli toplama
Faz 3  Semantic Analiz Tip kontrolü + yapısal doğrulama (diagnostic'e basar)
Faz 4  Optimizasyon    Pass manager (fixpoint) + constant folding + dead code
```

Katman eşlemesi (ADR-006):
- **Frontend:** Faz 0–3
- **Middle-end:** Faz 4
- **Backend:** bu yol haritasının dışında (birincil: IR + bytecode VM, ADR-015;
  ileride: C transpile; makine kodu uzak gelecek — "JIT" kapsam dışı)

---

## Faz 0 — Temeller (Type + Diagnostic + Hata Kataloğu)

**Bağımlılık:** yok. **Hedef:** her şeyin üstüne kurulacağı temel veri yapıları.
İlgili ADR: 010 (Type), 013 (Diagnostic).

### Dosyalar

| Dosya | İçerik |
|---|---|
| `src/core/type.hpp` | `Type` sınıfı. `enum class TypeKind { Primitive, Array, Struct, Function, Error }`. Primitif alt-tipleri: `int, float, double, char, string, bool, void`. Alanlar: array için `elementType`, function için `returnType`+`paramTypes`, struct için `structName`. Metotlar: `equals(const Type&)`, `toString()`, `toJson()`, factory'ler (`Type::primitive(...)`, `Type::array(...)`, `Type::error()`). |
| `src/diagnostic/diagnostic.hpp` | `enum class DiagLevel { Error, Warning, Note, Hint }`. `struct Diagnostic { DiagLevel level; std::string code; SourceLocation loc; std::string message; std::string hint; }`. **Hata kataloğu** (sabitler/enum): bkz. aşağı. |
| `src/diagnostic/diagnostic_engine.hpp` | `DiagnosticEngine`: `report(Diagnostic)`, `hasErrors()`, `errorCount()`, `printAll(std::ostream&)`, `toJson()`. Diagnostic'leri biriktirir; durdurma kararını pipeline verir (tüm hatalar toplanır, sonra raporlanır — ADR-013). |

### Hata Kataloğu (baştan belirlenir)

| Kod | Anlam | Hangi fazda üretilir |
|---|---|---|
| `E001` | Tanımsız değişken/isim (declare-before-use ihlali dahil — lokal ve **global başlatıcı**, ADR-011) | Faz 2/3 |
| `E002` | Aynı scope'ta çift tanım | Faz 2 |
| `E003` | Tip uyuşmazlığı (gizli dönüşüm yok; literal için bağlama-göre kural, ADR-010) | Faz 3 |
| `E004` | Döngü/switch dışı `break`/`continue` | Faz 3 |
| `E005` | Fonksiyon dışı `return` | Faz 3 |
| `E006` | Return tipi imzaya uymuyor | Faz 3 |
| `E007` | Tanımsız tip (bilinmeyen tip adı) | Faz 2/3 |
| `E008` | Fonksiyon çağrısı argüman sayısı/tipi uyuşmuyor | Faz 3 |
| `E009` | Array boyutu sabit değil / geçersiz | Faz 3 |
| `E010` | **Özyinelemeli/döngüsel struct tanımı** (by-value çevrim → sonsuz boyut, ADR-011) | Faz 2/3 |
| `W001` | Kullanılmayan değişken | Faz 4 |
| `W002` | Sıfıra bölme (sabit folding) | Faz 4 |
| `W003` | Erişilemez (ölü) kod | Faz 4 |

*(Liste uygulamada genişleyebilir; yeni hatalar buraya eklenir.)*

**Literal / başlatıcı kuralları (ADR-010/011) hata kataloğunu nasıl etkiler:**
- `float x = 1;` → **hata değil** (tamsayı literali bağlama-göre tiplenir,
  kayıpsız). `int y = 1.5;` → `E003` (kayıp). `float x = anInt;` → `E003`
  (değişken→değişken gizli dönüşüm yok).
- `int a = b; int b = 5;` (global) → `E001` (global başlatıcı declare-before-use).

### Doğrulama
- `Type::equals` / `toString` birim testleri.
- `DiagnosticEngine` topla → `printAll` çıktısı doğru sıralı.

---

## Faz 1 — AST Refactor (ExpressionNode / StatementNode + analiz alanları)

**Bağımlılık:** Faz 0 (Type). **Hedef:** node hiyerarşisini ifade/deyim olarak
ayır, analiz alanlarını ekle. İlgili ADR: 012, 013.

### Dosyalar

| Dosya | Değişiklik |
|---|---|
| `src/parser/ast_node.hpp` | İki ara taban ekle: `class ExpressionNode : public ASTNode { Type resolvedType; bool isConstant=false; /* foldedValue */ }` ve `class StatementNode : public ASTNode { bool isReachable=true; }`. |
| `src/parser/nodes/literal.hpp` · `binary_expr.hpp` · `identifier.hpp` · `expressions.hpp` (Call/Member/Index/Postfix) | Bu node'ları `ExpressionNode`'dan türet. |
| `src/parser/nodes/statements.hpp` (Block/If/While/For/DoWhile/Return/Break/Continue/ExpressionStatement) · `declarations.hpp`? | Statement'ları `StatementNode`'dan türet. (VariableDecl/FunctionDecl/StructDecl tartışmalı — declaration; şimdilik `StatementNode` veya doğrudan `ASTNode` altında değerlendirilir.) |
| `src/parser/nodes/identifier.hpp` | `IdentifierNode`'a `Symbol* resolvedSymbol = nullptr;` (Faz 2'de bağlanır). |
| `src/parser/nodes/*.cpp` | `toJson()`/`log()`'a yeni alanları (tip, isReachable) ekle — boş cpp'ler doluyor. |

### Doğrulama
- `saqut ast examples/fibonacci.sqt` hâlâ geçerli JSON (regresyon yok,
  `python3 -m json.tool` ile). Parser regresyonu için ayrıca
  `examples/parser-stress/Final.sqt` de geçerli JSON üretmeye devam etmeli (bu
  dosya **geçerli program değildir**, yalnızca parser/AST stres fixture'ıdır —
  semantik fazlarda fixture olarak kullanılmaz).
- Derleme uyarısız (`-Wall -Wextra`).

---

## Faz 2 — Symbol Table (scope'lu, iki-geçişli toplama)

**Bağımlılık:** Faz 0, 1. **Hedef:** isim çözümleme + scope + referans toplama.
İlgili ADR: 011, 013.

### Dosyalar

| Dosya | İçerik |
|---|---|
| `src/symbol/symbol.hpp` | `enum class SymbolKind { Variable, Function, Parameter, Struct, Field }`. `struct Symbol { std::string name; SymbolKind kind; Type type; SourceLocation definitionLoc; std::vector<SourceLocation> references; Scope* scope; }`. |
| `src/symbol/scope.hpp` | `class Scope { Scope* parent; std::unordered_map<std::string, Symbol*> symbols; ... }`. `defineLocal()`, `lookupLocal()`. Her katman bir namespace. |
| `src/symbol/symbol_table.hpp` | Scope yığını yönetimi: `enterScope()`, `exitScope()`, `define(Symbol)` (aynı scope'ta duplicate → false + `E002`), `resolve(name)` (içten dışa), `addReference(name, loc)`, `getAllSymbols()`, `toJson()`. |
| `src/symbol/symbol_collector.hpp/.cpp` | **İki geçiş** (ADR-011): **Geçiş 1** → tüm üst-seviye tanımlarını (fonksiyon imzaları, struct isim+alan tipleri, global değişkenler) global scope'a hoist et. **Geçiş 2** → fonksiyon gövdelerine in; lokal'leri declare-before-use ile topla; her `IdentifierNode`'u `resolve()` edip `resolvedSymbol`'a bağla + `addReference`. |

### Notlar
- Scope oluşturan node'lar: Program, FunctionDecl (parametreler), Block, for/while.
- `src/json.hpp`'deki eski `collectSymbolsRecursive` bu sistemle değiştirilir.
- Tanımsız isim → `E001`; bilinmeyen tip → `E007`; çift tanım → `E002`.
- **Döngüsel struct kontrolü (`E010`):** Geçiş 1'den sonra struct'lar düğüm,
  "alanı olarak içerir" kenarıyla bir çevrim-arama (DFS) çalıştır; çevrim →
  `E010` (ADR-011). Pointer olmadığı için tüm kapsama by-value'dur; çevrim =
  sonsuz boyut.
- **Global başlatıcı declare-before-use (ADR-011):** fonksiyon/struct tam hoist
  edilir, global değişken **ismi** hoist edilir, ama global değişken
  **başlatıcısı** kendinden önce tanımlı isimleri kullanabilir; aksi → `E001`.

### Doğrulama
- `saqut symbols examples/fibonacci.sqt` → zengin tablo (her sembolün tipi, tanım
  yeri, referansları). Forward reference çalışır (sonra tanımlı fonksiyon
  çağrılabilir — `main`, `fibonacci`'yi çağırır).
- Hatalı örnekler → `E001`/`E002`/`E007`/`E010` diagnostic'leri.

---

## Faz 3 — Semantic Analiz (Tip Kontrolü + Yapısal Doğrulama)

**Bağımlılık:** Faz 2. **Hedef:** tipleri ata/kontrol et, yapısal kuralları
doğrula. İlgili ADR: 010, 013.

### Dosyalar

| Dosya | İçerik |
|---|---|
| `src/sema/type_checker.hpp/.cpp` | İfadeleri alttan üste gez, her `ExpressionNode`'a `resolvedType` ata. Gizli **değişken→değişken** dönüşüm yok (ADR-010) → uyuşmazlıkta `E003`. **Tamsayı literali bağlama-göre tiplenir** (kayıpsızsa beklenen tipe uyar; `float x = 1;` geçerli, `int y = 1.5;` → `E003`). Kontrol noktaları: atama (`=`), binary op operand tipleri, fonksiyon çağrısı argümanları (`E008`), array index, return değeri (`E006`), variable init tip uyumu. Hata olunca node'a `Type::error()` → ardışık sahte hata yok. |
| `src/sema/structural_validator.hpp/.cpp` | Parent pointer ile ağaç-tırmanma kontrolleri: `break`/`continue` döngü/switch içinde mi (`E004`), `return` fonksiyon içinde mi (`E005`), array boyutu sabit mi (`E009`). |

### Notlar
- Bu iki modül + Faz 2'nin collector'ı birlikte "semantic analiz" fazını oluşturur.
- Tüm hatalar toplanır, ilk hatada durulmaz; faz sonunda `DiagnosticEngine`
  hepsini raporlar, pipeline durur (ADR-013).

### Doğrulama
- Doğru `.sqt` → temiz geçer, her expression node'unun tipi JSON'da görünür.
- Hatalı `.sqt` örnekleri → tüm semantik hatalar tek seferde listelenir.

---

## Faz 4 — Optimizasyon Framework

**Bağımlılık:** Faz 3. **Hedef:** opsiyonel, iteratif, toggle'lı kaynak-seviyesi
optimizasyon. **Orijinali bozmaz — klon üstünde** (ADR-007). İlgili ADR: 007, 008, 009.

### Dosyalar

| Dosya | İçerik |
|---|---|
| `src/core/config.hpp` | `CompilerConfig`: pass toggle'ları (`optConstantFolding`, `optDeadCodeElim`, …), `outputFormat`, `mode`, `optimized` bayrağı. |
| `src/opt/optimization_pass.hpp` | Soyut `OptimizationPass`: `virtual bool run(ASTNode* root, SymbolTable* table) = 0;` (değişiklik yaptıysa true). `name()`. |
| `src/opt/optimization_manager.hpp` | Pass listesi; `CompilerConfig`'e göre seçim; **fixpoint döngüsü** (hiçbir pass değişiklik yapmayana kadar + **sert iterasyon tavanı** `maxFixpointRounds`, ADR-009). **Sonlanma değişmezi:** havuzdaki pass'ler monoton (yalnızca küçültür); büyüten pass (inlining) eklenirse tavan zorunlu. Çalışmadan önce AST'yi **klonlar** — `ASTNode::clone()` **merkezi bir bileşendir** (ADR-007): parent pointer'lar yeniden bağlanır, sembol tablosu klonlanıp `IdentifierNode→Symbol` bağları **remap** edilir. **Her turda**, klon üzerinde akışa-bağlı analiz (`isReachable`, ref-count) **yeniden hesaplanır** (ADR-009); aksi halde zincirleme fırsatlar bayat veriyle kaçar. |
| `src/opt/constant_folding.hpp/.cpp` | `BinaryExpression` operandları sabitse hesapla, sonucu sabit `Literal` ile değiştir (klonda). Tipe saygılı (`5/2`→int `2`, ADR-010). Sıfıra bölme → `W002`, katlama yapma. |
| `src/opt/dead_code_elim.hpp/.cpp` | `isReachable` (Faz 3) işaretine göre: `return`/`break`/`continue` sonrası statement'lar, `if(false)`, sıfır-referanslı değişken (`W001`/`W003`). |

### CLI Entegrasyonu

| Dosya | Değişiklik |
|---|---|
| `src/cli/args.hpp` | `--optimized` ve `--opt-all`/`--opt-none`/`--skip-*` bayrakları → `CompilerConfig`. |
| `src/cli/commands/ast.hpp` · `symbols.hpp` | `--optimized` verilince klon+optimize edilmiş hali göster; verilmezse orijinal. |

### Doğrulama
- `saqut ast file --optimized` → `1+2` katlanmış, ölü kod elenmiş.
- `saqut ast file` (bayraksız) → orijinal, **değişmemiş** (öncesi/sonrası ayrımı).
- Fixpoint: zincirleme optimizasyon (folding → yeni dead code → DCE) yakalanır.

---

## Tamamlanınca

Bu yol haritası bittiğinde frontend tamamlanmış olur:
- Parser → analizli, tipli, sembol-çözümlü AST + zengin symbol table.
- Tam hata raporlama (toplu, kataloglu).
- Opsiyonel, incelenebilir optimizasyon.

Sonraki adım (ayrı yol haritası): **IR güçlendirme** (kontrol akışı/fonksiyon/
bellek opcode'ları + **FFI seam** `callhost`, ADR-016) → **bytecode VM ile
çalıştırma** (ADR-015) → hedef: **`examples/fibonacci.sqt` derlenir ve çalışır.**

- **Çalıştırma modeli IR + bytecode VM'dir** (ADR-015). **Makine-kodu JIT
  açıkça kapsam dışıdır;** öncelik determinizm + incelenebilirlik, ham hız değil.
- **C transpile**, ileride geçerli bir **ikinci** backend olarak kalır (frontend
  backend-bağımsız). Makine kodu gerçekten istenirse libgccjit/LLVM'e bağlanılır
  — çok uzak gelecek.
- IR/VM tasarlanırken **FFI seam'i şimdiden bırak** (ADR-016); `print` ilk
  müşteridir. Bellek host (C++) heap'idir; özel allocator yok. Dinamik array'in
  runtime modeli (ADR-014) bu fazda netleşir.

> Çerçeve uyarısı: bu sonraki adım da **önce dikey dilim** ilkesine tabidir —
> genel VM/optimizasyon altyapısı kurmadan önce fibonacci uçtan uca çalışsın.
