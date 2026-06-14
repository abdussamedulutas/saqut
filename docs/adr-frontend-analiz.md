# saQut Derleyici — Frontend & Semantic Analiz Karar Kaydı (ADR-006 …)

> Bu belge, `docs/fikirler.md`'deki ADR-001…005'in **devamıdır**. Orada backend
> stratejisi, parser mimarisi, header-only tercihi, token sistemi ve IR tasarımı
> kararlaştırılmıştı. Bu belge ise **frontend'in tamamlanması** — symbol table,
> semantic analiz ve optimizasyon framework'ü — etrafında alınan kararları,
> **neden** alındıklarını, elenen alternatifleri ve gelecekteki sonuçlarını kaydeder.
>
> Bu kararlar bir tasarım oturumunda (kullanıcı + asistan) tartışılarak alındı.
> Tartışmanın tam akışı için bkz. `docs/transkript-frontend-tasarim.md`.
> Uygulama planı için bkz. `docs/roadmap-frontend.md`.

---

## ADR-006: Çok-Aşamalı (Multi-Pass) Frontend Mimarisi

### Bağlam

Derleyici tek bir monolitik geçişle çalışmaz; lexing, parsing, analiz,
optimizasyon, kod üretimi gibi **birbirinden bağımsız aşamalardan** oluşur.
saQut'un "alet çantası" (toolbox) felsefesi gereği bu aşamaların her biri:
- bağımsız çalışabilmeli,
- net bir girdi/çıktı sözleşmesine sahip olmalı,
- gerektiğinde çoğaltılabilmeli (projenin amacına sadık kalarak).

CLI komutları (`tokens`, `ast`, `symbols`, …) zaten bu aşama-modülü
yapısının dışa vurumudur — her komut bir aşamanın çıktısını gösterir.

### Değerlendirilen Yaklaşımlar

#### Tek geçişli (single-pass) parser+analiz
- **+** Basit, hızlı.
- **−** Forward reference (ileri başvuru) imkânsızlaşır; her şey tanımdan
  önce bilinmek zorunda kalır.
- **−** Analiz ve syntax iç içe girer, test edilemez, bakımı zor.

#### Çok geçişli (multi-pass) — net aşamalar
- **+** Her aşama tek bir iş yapar, ayrı ayrı test edilir.
- **+** Forward reference mümkün olur.
- **+** Aşamalar incelenebilir (`saqut ast`, `saqut symbols` …).
- **−** Daha fazla kod ve veri yapısı; aşamalar arası sözleşme tasarımı gerekir.

### Karar

✅ **Çok-aşamalı frontend.** Aşamalar şu üç katmana ayrılır (klasik derleyici
mimarisi):

```
FRONTEND                 MIDDLE-END               BACKEND
lexer → token →          optimizasyon             IR lowering →
parser → AST →           (opsiyonel, iteratif,    kod üretimi
symbol table →           toggle'lı, ortak         (C transpile /
semantic analiz          gösterim üstünde)        QBE / JIT)
(annotated AST)
```

- **Pass 1 (Syntax):** token → ham AST. (Büyük ölçüde mevcut.)
- **Pass 2 (Symbol):** AST → SymbolTable (scope'lu, iki-geçişli — bkz. ADR-011).
- **Pass 3 (Semantic / "ASTyi derinleştir"):** symbol table + AST kullanılarak
  her node zenginleştirilir (tip, symbol bağı, erişilebilirlik, reference sayısı).

"Parser ve symbol hikayesini bitirmek" = **frontend'i bitirmek.** Optimizasyon
ayrı bir katmandır (middle-end), backend'ler bu ortak çıktıdan beslenir.

**Neden bu katmanlama?** Birden çok backend (C transpile, QBE, JIT) planlandığı
için, ortak işler (analiz, optimizasyon) **bir kez** ortak katmanda yapılmalı;
yoksa her backend aynı optimizasyonu yeniden yazar.

---

## ADR-007: Analiz (Annotation) ile Optimizasyon (Transformation) Ayrımı

### Bağlam

`docs/fikirler.md` ve `docs/todo.md`'nin temel prensibi: **"AST bellek canavarı.
Hiçbir bilgi atılmaz."** Aynı zamanda constant folding (`1+2` → `3`), dead code
elimination gibi optimizasyonlar isteniyor. Bu ikisi doğrudan çelişir gibi
görünür: optimizasyon AST'yi bozarsa, kaynak kodun izdüşümü kaybolur ve
`saqut ast` artık kullanıcının yazdığını değil, optimize edilmiş hali gösterir.

Ayrıca kritik bir kullanıcı gereksinimi belirlendi: **kullanıcı, AST'nin veya
sembol tablosunun optimizasyondan önceki ve sonraki halini ayrı ayrı görebilmeli.**

### İki Kavram

- **Analiz (annotation) = programın gerçekleri.** "Bu node sabit, değeri 3",
  "bu kod erişilemez", "bu ifadenin tipi int", "bu değişken 2 kez kullanıldı".
  Bunlar **değişiklik değil, tespittir.** Backend'den bağımsızdır.
- **Optimizasyon (transformation) = ağacı/IR'ı gerçekten değiştirmek.**
  `1+2`'yi `3` ile değiştirmek, ölü kodu silmek.

### Karar

✅ **İki kavram net ayrılır:**

1. **Analiz, orijinal AST'nin üstüne yerinde işaretleme yapar** (node'lara tip,
   symbol bağı, erişilebilirlik, constness ekler). Ağacı **bozmaz**, zenginleştirir.
   Orijinal AST hâlâ kaynak kodun tam izdüşümüdür.

2. **Optimizasyon dönüşümü, ağacın bir KOPYASI (klon) üzerinde yapılır.**
   Orijinal analizli AST = "öncesi"; klon + dönüştürülmüş AST = "sonrası".
   Ağaç klonlamak ucuz ve basittir, yalnızca `--optimized` istendiğinde yapılır.

**Sonuç:** Hem "bellek canavarı" felsefesi korunur (orijinal AST her şeyi tutar),
hem optimizasyon yapılır, hem de öncesi/sonrası ayrı ayrı incelenebilir.

```
saqut ast file.sqt              → ham + annotate edilmiş AST (1+2 burada durur)
saqut ast file.sqt --optimized  → klon, folding uygulanmış (3 var)
```

---

## ADR-008: Optimizasyon Konumu — AST mı, IR mı?

### Bağlam

"Optimizasyonu IR/derleme zamanında mı yapmalıyız, yoksa AST aşamasında mı?"
sorusu tartışıldı. İki uç yaklaşım var:

- **AST seviyesi:** kaynak-seviyesi, dile yakın, incelenebilir.
- **IR seviyesi:** açık kontrol-akış grafiği (CFG), dataflow analizi için uygun
  (LLVM modeli).

### Karar

✅ **Hibrit, optimizasyon türüne göre bölünür:**

- **Kaynak-seviyesi, ağaç-yerel optimizasyonlar** (constant folding, ölü kod
  işaretleme, unused variable) → **AST'de** yapılır. Çünkü:
  1. Dil JS gibi basit; ağır optimizasyona ihtiyaç yok.
  2. Backend-bağımsız → C transpile, QBE, JIT üçü birden faydalanır.
  3. İncelenebilir kalır (`saqut ast --optimized`) — projenin varlık sebebi.

- **CFG/dataflow gerektiren optimizasyonlar** ("bir kez atanıp bir kez
  kullanılan değişken" = copy propagation, common subexpression elimination,
  loop optimizasyonları) → **IR'de** yapılır, IR olgunlaşınca ertelenir.
  Çünkü bunlar açık kontrol akışı ister, ağaçta yapmak işkencedir.

**Neden backend'e bırakmıyoruz?** 3 backend varsa, optimizasyon backend'e
konulursa 3 kez yazılır. Ortak katmanda (middle-end) bir kez yazılır.

---

## ADR-009: Optimizasyon Pass Yönetimi — Sabit Sayı Değil, Fixpoint

### Bağlam

Optimizasyon adımları birbirini tetikler: constant folding yeni ölü kod doğurur,
dead code elimination yeni kullanılmayan değişken doğurur. Tek geçişte zincirleme
fırsatlar kaçırılır. "5 pass mı, 10 pass mı çalıştıralım?" sorusu yanlış kurgu.

> **Not:** Buradaki "pass", ADR-006'daki derleyici aşamalarından (lexing/parsing
> gibi makro-aşamalar) farklıdır. Burada "pass" = tek bir optimizasyon adımının
> AST üzerindeki bir gezisidir.

### Karar

✅ **Fixpoint döngüsü.** Önceden belirlenmiş sayıda değil; bir pass havuzu,
**hiçbir pass değişiklik yapmayana kadar** döngüde çalışır. Belki 2 tur sürer,
belki 7 — kodun kendisi belirler.

- Her **tur** bir öncekinden daha az iş yapar (giderek azalan değişiklik), ta ki
  sıfır değişiklikle stabilize olana kadar.
- Pass'ler `CompilerConfig` ile tek tek açılıp kapatılabilir.
- `OptimizationManager` pass listesini tutar, sırayı ve fixpoint döngüsünü yönetir.

> Düzeltme notu: "Her pass bir öncekinden daha kolay" sezgisi yanlıştır. Doğrusu:
> her **tur** daha az değişiklik yapar. Analiz pass'leri (symbol table, type check)
> "kolaylaşmaz"; onlar bir kez çalışır.

---

## ADR-010: Tip Sistemi Tasarımı

### Bağlam

Dil **tipli** olacak. Şu anda `varType`/`returnType` AST'de yalnızca
`std::string`. Tip kontrolünü string karşılaştırmasıyla yazmak kırılgandır ve
`int[]`, `struct Point`, fonksiyon tipi gelince baştan yazmayı gerektirir.

### Alınan Kararlar

✅ **Minimal ama genişletilebilir `Type` sınıfı** (`src/core/type.hpp`):
- `kind`: `Primitive / Array / Struct / Function / Error`.
- Primitifler: `int, float, double, char, string, bool, void`.
- `Array` → eleman tipi (boyut tipin parçası DEĞİL — bkz. aşağı).
- `Function` → dönüş tipi + parametre tipleri.
- İleride `Pointer`, `Generic` eklenebilir.

✅ **`Error` tipi şart.** Tip hatası olduğunda node'a `Error` atanır; böylece
ardışık sahte hatalar üretilmez (tek hata, tek mesaj).

✅ **Gizli (implicit) dönüşüm YOK.** `int → float` otomatik olmaz; her şey açık.
- **Tek istisna:** sabit ifadelerde (constant folding) — `int a = 5 / 2;` → `2`.
  Sabitler üzerinde küçük analiz/hesap yapılır.

✅ **Tip çıkarımı (auto/var) YOK.** Her şey açıkça tiplenir. `auto` keyword'ü
yok sayılır. Sebep: basitlik, öngörülebilirlik, kafa karışıklığını önlemek.

✅ **Array tip temsili: `int[]` (boyut tipte yok).** `int[]` sadece "int dizisi";
boyut tip eşitliğine girmez (JS gibi). Tip kontrolü basit kalır.

**Neden genişletilebilir?** "Bu dilin geleceğini bilmiyoruz; beklenenden popüler
de olabilir, yıllarca repolarda tozlanabilir de." Temel sağlam ve büyümeye açık
olmalı.

---

## ADR-011: Scope ve Forward Reference Kuralları

### Bağlam

Dil "Java gibi forward reference, C gibi syntax, başta OOP yok" olarak
tasarlandı (JS yalnızca **syntax basitliği** örneği olarak verildi; JS'in kötü
yanları — null/undefined ikiliği, var hoisting — **alınmıyor**).

### "Hoisting" nedir?

Bir tanımın, yazıldığı satırdan **önce de** görünür olması (scope'un tepesine
"kaldırılmış" gibi).

### Karar

✅ **Asimetrik kurallar (tam olarak Java'nın davranışı):**

- **Üst seviye (global): tam forward reference (hoisting var).** Fonksiyonlar,
  global değişkenler, struct'lar sırasından bağımsız her yerde görünür.
  ```
  int main() { return kare(5); }   // kare aşağıda ama görünür → OK
  int kare(int n) { return n * n; }
  ```
  **Neden güvenli?** `main`'in gövdesi tanımlandığı anda çalışmaz; çağrılınca
  çalışır, o ana kadar `kare` zaten vardır. Tanımların çalışma sırası yoktur.

- **Lokal (fonksiyon içi): declare-before-use (hoisting YOK).**
  ```
  int main() {
      int x = y + 1;   // HATA: y henüz tanımlı değil
      int y = 5;
  }
  ```
  **Neden?** Lokal değişkenin bir çalışma sırası ve değeri vardır; tanımdan önce
  kullanmak, var olmayan/değeri olmayan bir şeyi kullanmaktır. Local hoisting
  olsaydı isim olur ama değeri çöp/undefined olurdu (JS `var` derdi) — kaçınılan
  durum.

**Asimetri tutarsızlık değildir:** global tanımlar yerinde çalışmaz (forward ref
güvenli), lokal değişkenlerin sırası ve değeri vardır (declare-before-use güvenli).
Bu, Java/C#'ın da davranışıdır.

✅ **Duplicate kesinlikle yasak.** Aynı scope'ta aynı isimli iki
değişken/fonksiyon tanımlanamaz → diagnostic. (Overloading yok.)

✅ **Shadowing serbest.** İç scope, dış scope'u gölgeleyebilir (hata değil).

✅ **Scope oluşturan node'lar:** `Program` (global), `FunctionDecl` (parametreler),
`Block`, `for`/`while` (init değişkeni döngüye ait; döngü dışında görünmez).
Her katman bir namespace tutar; değişken bulunamazsa bir üst katmanda aranır.

### Symbol Table'ın İki Geçişi

✅ **Sadece üst seviyede iki geçiş gerekir:**
- **Geçiş 1:** tüm üst-seviye tanımları (fonksiyon imzaları, struct isim+alanları,
  global değişkenler) global scope'a hoist et.
- **Geçiş 2:** gövdelere in; lokal'leri declare-before-use ile topla, her
  `Identifier`'ı çöz, reference ekle.
- **Fonksiyon içi tek geçiş yeter** (lokal'de forward ref yok). "Öncesi/sonrası"
  derdi yalnızca global'ler içindir, onu da Geçiş 1 çözer (global'ler en baştan
  tamamen doludur).

---

## ADR-012: ExpressionNode / StatementNode Ara Tabanları

### Bağlam

Şu anda tüm AST node'ları doğrudan `ASTNode`'dan türüyor; "ifade" (değer üreten)
ve "deyim" (iş yapan ama değer olmayan) ayrımı yok. Tipli bir dilde yalnızca
**ifadelerin** tipi vardır: `5 + 3` → int; `if (...) {...}` → tipi yok.

`resolvedType` alanını nereye koyacağımız bir tasarım kararı. Seçenekler:
- (a) `ASTNode` tabanına koy → her node'da olur, `if`/`while`'da boşa durur.
- (b) `ExpressionNode`/`StatementNode` ara tabanları → alanlar doğru yere oturur.
- (c) Yan-tablo `map<ASTNode*, Annotation>` → AST temiz ama dolaylı/karmaşık.

### Karar

✅ **(b) İki ara taban eklenir:**
- `ExpressionNode : ASTNode` → `resolvedType`, `isConstant`, `foldedValue`.
- `StatementNode : ASTNode` → `isReachable` (ölü kod analizi için).

```
ASTNode
 ├─ ExpressionNode   (resolvedType, isConstant, foldedValue)
 │   ├─ LiteralNode / BinaryExpressionNode / IdentifierNode / CallExpressionNode …
 └─ StatementNode    (isReachable)
     ├─ IfStatementNode / WhileStatementNode / ReturnStatementNode / BlockNode …
```

**Kazanımlar:**
1. `resolvedType` yalnızca tip taşıyabilen node'larda olur.
2. Parser/analiz "burası ifade olmalı" diyebilir (örn. `if` koşulu bir
   `ExpressionNode` olmalı, fonksiyon argümanı `ExpressionNode` olmalı).

**Önemli:** Bu karar, "her şey AST'de" felsefesini bozmaz (bkz. ADR-013); yalnızca
analiz alanlarını doğru node sınıflarına dağıtır. Node cpp dosyaları zaten boştu;
bu tabanlar onları doldururken ekleniyor. Maliyeti şimdi düşük, sonra yüksek
olurdu.

---

## ADR-013: Analiz Verisi Nerede Yaşar — Her Şey AST'de

### Bağlam

İki model: (1) her şey AST node'larının üstünde; (2) AST temiz, analiz sonuçları
ayrı yan-tablolarda.

- **Her şey AST'de:** tek doğruluk kaynağı, gezinmesi kolay (`node->type`),
  kullanıcının zihinsel modeli, boş node class'larını doldurur. Ancak öncesi/
  sonrası için ağacı klonlamak gerekir.
- **Temiz AST + yan-tablolar:** AST sade kalır, çoklu bağımsız analiz mümkün;
  ancak dolaylılık ve karmaşıklık artar, "node class'larını doldur" isteğine ters.

### Karar

✅ **Her şey AST node'larının üstünde** (kullanıcının modeli):
- **Analiz (tip, constness, erişilebilirlik) = node'lara yerinde işaretlenir.**
- **Optimizasyon dönüşümü = ağacın klonunda yapılır** (ADR-007), böylece
  öncesi/sonrası korunur.

✅ **Önemli ayrım — "kaç kez kullanıldı" bilgisi node'da değil, Symbol'da:**
- `IdentifierNode` → işaret ettiği `Symbol`'a pointer tutar.
- `Symbol` → o değişkenin tüm referanslarının listesini + sayısını tutar.
- `ExpressionNode` → kendi sonuç tipini, sabit olup olmadığını tutar.

Sebep: kullanım sayısı **değişkene** aittir, tek bir kullanım node'una değil.

> **Pointer notu:** Burada ve genel olarak derleyici **içinde** pointer serbestçe
> kullanılır (Symbol bağları, parent pointer'lar vb.). Kullanıcıya sunulan
> **dilde** pointer syntax'ı (`*`, `&`) yoktur — bkz. ADR-014.

---

## ADR-014: Dil Kapsamı ve Özellik Kararları

### Karar — Başlangıç Dili (v0)

| Özellik | Karar | Not |
|---|---|---|
| Pointer (kullanıcı syntax'ı `*`/`&`) | ❌ Yok | Ama derleyici/runtime **içeride** pointer'ı sonuna kadar kullanır |
| Tuple / Generic (`<T,U>`) | ❌ Yok | |
| Class / OOP | ❌ Yok (başta) | `class` keyword'ü yok sayılır |
| Struct | ✅ Var | `struct A { B bVar }` olur (B başka yerde tanımlı); recursive define yok |
| Array | ✅ `int[]` | Dinamik yönde; runtime bellek modeli ertelendi |
| Fonksiyonlar | ✅ Tipli | Dönüş + parametre tipleri zorunlu |
| `auto` / tip çıkarımı | ❌ Yok | Her şey açık tipli |
| Gizli int↔float dönüşümü | ❌ Yok | Sadece sabit folding'de istisna |

### Dinamik Array'in Bellek Yükümlülüğü (Gelecek Notu)

`int[]` büyüyebilen array = heap + bir yönetim stratejisi gerektirir. Bu gerçek
bir yükümlülüktür ama **frontend'i bloklamaz** ve kolay yolu vardır:

- **Frontend:** yalnızca "bu int dizisi" bilgisini ister; bellek modelinden habersiz.
- **C transpile backend (ilk backend):** `int[]` → C'de `struct {int* data; size_t len, cap;}`,
  `malloc/realloc/free` ile yönetilir. Bellek yönetimi C'den hazır gelir.
- **JIT backend:** bellek yönetimini kendi yapmaz; minik bir runtime kütüphanesi
  (`array_new`, `array_push`, `array_free`) olur, JIT bunlara **call** emit eder.
- **Yönetim stratejisi (ne zaman free):** en basiti scope-tabanlı ownership
  (array'i tutan değişken scope'tan çıkınca free). GC gerekmez. Runtime'a
  gelince kararlaştırılır.

---

## Kararların Özet Tablosu

| ADR | Konu | Karar |
|---|---|---|
| 006 | Frontend mimarisi | Çok-aşamalı; frontend/middle-end/backend katmanları |
| 007 | Analiz vs optimizasyon | Analiz yerinde işaretler; optimizasyon klonda dönüştürür |
| 008 | Optimizasyon konumu | Basitler AST'de, dataflow gerektirenler IR'de |
| 009 | Pass yönetimi | Fixpoint döngüsü, toggle'lı |
| 010 | Tip sistemi | Minimal+genişletilebilir Type; gizli dönüşüm yok; Error tipi |
| 011 | Scope/forward ref | Global'de forward ref, lokal'de declare-before-use (Java gibi) |
| 012 | Node hiyerarşisi | ExpressionNode / StatementNode ara tabanları |
| 013 | Analiz verisi yeri | Her şey AST'de; ref-count Symbol'da |
| 014 | Dil kapsamı | Pointer/class/generic yok; struct+array+tipli fonksiyon var |
