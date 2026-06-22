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
>
> ✅ **Uygulama durumu:** Bu belgedeki ADR-006…019 kararlarında tarif edilen
> makine **kodlandı ve çalışıyor.** Sembol tablosu, semantik analiz, tip sistemi,
> diagnostic motoru, optimizasyon (constant folding + DCE), IR üreteci ve bytecode
> VM'in tamamı uygulandı. `examples/fibonacci.sqt` uçtan uca çalışıyor.
> Güncel "çalışıyor / henüz yok" listesi için bkz. `CLAUDE.md`.

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
parser → AST →           (opsiyonel, iteratif,    bytecode VM (birincil)
symbol table →           toggle'lı, ortak         + ileride C transpile
semantic analiz          gösterim üstünde)        (makine kodu = uzak
(annotated AST)                                    gelecek; ADR-015)
```

- **Pass 1 (Syntax):** token → ham AST. (Büyük ölçüde mevcut.)
- **Pass 2 (Symbol):** AST → SymbolTable (scope'lu, iki-geçişli — bkz. ADR-011).
- **Pass 3 (Semantic / "ASTyi derinleştir"):** symbol table + AST kullanılarak
  her node zenginleştirilir (tip, symbol bağı, erişilebilirlik, reference sayısı).

"Parser ve symbol hikayesini bitirmek" = **frontend'i bitirmek.** Optimizasyon
ayrı bir katmandır (middle-end), backend'ler bu ortak çıktıdan beslenir.

**Neden bu katmanlama?** Birden çok backend (birincil: IR+bytecode VM, ADR-015;
ileride: C transpile; çok uzak: makine kodu) hedeflendiği için, ortak işler
(analiz, optimizasyon) **bir kez** ortak katmanda yapılmalı; yoksa her backend
aynı optimizasyonu yeniden yazar.

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

2. **Optimizasyon dönüşümü iki yolla yapılabilir:**

   a. **`ast` komutu için klon:** orijinal AST dokunulmadan kalır, klon üstünde
      pass'ler çalışır. Kullanıcı "öncesi" ve "sonrası" AST'yi ayrı ayrı
      görebilir. `OptimizationManager::optimize()` bu yolu kullanır.

   b. **Diğer tüm komutlar için yerinde (in-place):** `run --optimized`,
      `ir --optimized` vb. tek versiyon üretiyor — orijinali saklamaya gerek yok.
      `OptimizationManager::runPassesInPlace()` bu yolu kullanır, klon maliyeti yok.

**Sonuç:** "bellek canavarı" felsefesi `ast` komutunda korunur; diğer komutlar
gereksiz klon maliyeti taşımaz.

```
saqut ast file.sqt              → ham + annotate edilmiş AST (1+2 burada durur)
saqut ast file.sqt --optimized  → klon, folding uygulanmış (3 var)
saqut run file.sqt --optimized  → yerinde optimize → IR → VM (klon yok)
saqut ir  file.sqt --optimized  → yerinde optimize → IR dump (klon yok)
```

### Güncelleme — Klon ve sembol tablosu paylaşımı

`deepClone` sembol tablosunu yeniden eşlemez (remap etmez) — klondaki
`IdentifierNode::resolvedSymbol` orijinal `Symbol` nesnelerini gösterir. Bu
**güvenlidir**, çünkü:

- `Symbol::references` bir **konum listesi** (`std::vector<SourceLocation>`),
  referans sayacı değildir. Klonda bir `IdentifierNode` silindiğinde bu liste
  değişmez.
- `IdentifierNode` destructor'ı yoktur; `resolvedSymbol`'e dokunan hiçbir yıkıcı
  kodu çalışmaz.
- Klondaki pass'ler Symbol nesnelerini **okur** (slot numarası, tip vb.),
  **yazmaz** — paylaşım salt-okunur (read-only) kullanımdır.

**Parent pointer'lar** ise yeniden bağlanır — klon node'larının `parent`'ı
orijinali değil, klonu gösterir (deepClone bunu zaten yapar).

Önceki versiyon "sembol tablosu klonlanır ve remap edilir" diyordu; bu hem hiç
implement edilmedi hem de gerekli değildi. Düzeltildi.

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
  2. Backend-bağımsız → bytecode VM ve ileride C transpile birden faydalanır.
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

### Güncelleme — Sonlanma değişmezi (termination invariant)

Fixpoint döngüsünün **sonlanacağı garanti edilmeli**. İki seçenekten en az biri
zorunludur:

1. **Monotonluk:** havuzdaki tüm pass'ler **monoton** olmalı — yalnızca
   küçültür/sadeleştirir, asla büyütmez. Constant folding ve dead code
   elimination bugün monotondur, dolayısıyla fixpoint sonlanır.
2. **Sert iterasyon tavanı (cap):** bir üst sınır (örn. `maxFixpointRounds`).

**Neden gerekli:** ileride **büyüten** pass'ler (inlining, loop unrolling)
eklenirse, naif fixpoint **salınabilir** (A büyütür, B küçültür, sonsuz döngü).
Büyüten bir pass eklendiği an, monotonluk bozulur ve **iterasyon tavanı zorunlu
hale gelir.** Bu değişmez şimdiden yazıya geçirildi ki ileride unutulmasın.

### Güncelleme — "Analiz bir kez çalışır" çelişkisinin çözümü

ADR-013 "analiz bir kez çalışır" diyor; ama folding **erişilebilirliği**
(`if(false)`) ve **referans sayımlarını** değiştirir, DCE de tam bunlara
dayanır. Eğer analiz gerçekten yalnızca bir kez çalışırsa, fixpoint'in ikinci
turundaki DCE **bayat (stale) veriyle** çalışır ve zincirleme fırsatları kaçırır.

**Çözüm — iki analiz sınıfını ayır:**

- **Kaynağa-bağlı analiz** (her ifadenin tipi, sembol bağları): kaynak değişmediği
  sürece sabittir → **bir kez** çalışır, klona taşınır.
- **Türetilmiş/akışa-bağlı analiz** (erişilebilirlik `isReachable`, referans
  sayıları): bir dönüşüm bunları geçersizleştirir → **fixpoint döngüsünün her
  turunda, klon üzerinde yeniden hesaplanır.**

Yani "analiz bir kez çalışır" ifadesi yalnızca **kaynağa-bağlı** analiz için
geçerlidir; akışa-bağlı analiz tur başına tazelenir. ADR-013 buna göre okunmalı.

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

### Güncelleme — Sayısal literal tipleme kuralı

"Gizli dönüşüm yok + tip çıkarımı yok" altında `float x = 1;` ifadesi
**tanımsızdı**. Bu açıkça karara bağlanmalı, çünkü tip denetleyicisini (Faz 3)
doğrudan yönlendirir.

**Değerlendirilen iki kural:**

- **(a) Literal her zaman `int`:** `1` daima `int`'tir. `float x = 1;` bir tip
  hatasıdır; `float x = 1.0;` yazmak zorunludur. En katı, en öngörülebilir; ama
  rahatsız edici ve "gizli dönüşüm yok" ilkesini literallere kadar gereksiz yere
  zorlar.
- **(b) Tamsayı literali bağlama-göre tiplenir (context-typed / polymorphic):**
  tipsiz bir tamsayı sabiti, beklenen tip ona **kayıpsız** sığıyorsa o tipe
  uyarlanır. `float x = 1;` çalışır (`1` → `1.0`); `int y = 1.5;` ise hata
  (kayıp olur).

**Karar:** ✅ **(b) Bağlama-göre tiplenen tamsayı literalleri.**

- Gerekçe: bu bir **değişken-değer dönüşümü değil, bir derleme-zamanı sabitinin
  uygun tipte yorumlanmasıdır** — tam olarak ADR-010'un zaten tanıdığı "sabit
  istisnası" (`int a = 5/2 → 2`) ruhuyla aynı kapıya çıkar. Çalışma zamanı
  `int` değişkenini `float`'a gizlice çevirmek hâlâ **yasaktır**; istisna
  yalnızca **literal/sabit** içindir.
- Kural net: *değişken→değişken* gizli dönüşüm yok; *literal→beklenen tip*
  kayıpsızsa serbest. `float x = anInt;` hata; `float x = 1;` serbest.

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

### Güncelleme — Global "tam forward reference" çok genişti: üç-parçalı kural

İlk metin "global = her zaman forward-reference güvenli" diyordu; bu **fazla
geniş**. Global bir değişkenin **başlatıcısının (initializer) bir çalışma
sırası vardır** (tıpkı lokaller gibi). Düzeltme — üç ayrı kural:

1. **Global fonksiyonlar / struct'lar → tam hoisting.** Tanım anında çalışmazlar,
   sıradan bağımsız her yerde görünür. (Güvenli; değişmedi.)
2. **Global değişken isimleri → hoist edilir.** İsim her yerde görünür.
3. **Global değişken başlatıcıları → değer sırasına tabidir** (lokaller gibi) →
   **declare-before-use** VEYA bir **definite-assignment (kesin-atama) analizi**
   gerektirir.

**Neden:** `int a = b; int b = 5;` global scope'ta, isim-hoisting'e güvenilirse,
`a`'ya **sessizce çöp değer** verir — kaçınmaya çalıştığımız tam o JS `var`
durumu. Java da aynı sebeple bunu kısıtlar. Karar: global başlatıcılar için de
**declare-before-use** uygulanır (en basit, definite-assignment'a gerek
bırakmaz). Yani isim görünür ama **kendinden önceki** bir global başlatıcıda
kullanılabilir.

### Güncelleme — Döngüsel / karşılıklı-özyinelemeli struct tespiti

Pointer olmadığı için tüm struct iç içeliği **değer (by-value)** ile olur →
herhangi bir kapsama döngüsü sonsuz boyut demektir:

```
struct A { B b }   // A, B'yi değer olarak içerir
struct B { A a }   // B, A'yı değer olarak içerir → sonsuz boyut
```

Bu **derleme hatası olmak zorunda** ve hata kataloğunda **eksikti**. Eklendi:
**`E010` — özyinelemeli/döngüsel struct tanımı.** Symbol toplama sonrası bir
**topolojik / kapsama-döngüsü kontrolü** çalışır (struct'ları düğüm, "alan
olarak içerir" kenarını çevrim arayan bir DFS ile). Çevrim bulunursa `E010`.

(Karşılaştır: `struct A { B b }` + `struct B { int x }` geçerlidir; yalnızca
**çevrim** yasaktır. Pointer olsaydı çevrim mümkün olurdu — ama pointer yok.)

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
| Class / OOP / kalıtım | ❌ Yok (başta) | `class` keyword'ü yok sayılır |
| Closure | ❌ Yok | Bkz. ADR-019 (bellek bağımlılığı) |
| Struct | ✅ Var | `struct A { B bVar }` olur (B başka yerde tanımlı); **çevrim yasak → `E010`** |
| `interface` | ⏸️ Ertelendi | Reddedilmedi; v0 değil — gerekçe aşağıda + ADR-018 |
| Array | ✅ `int[]` | Dinamik yönde; runtime bellek modeli ertelendi |
| Fonksiyonlar | ✅ Tipli | Dönüş + parametre tipleri zorunlu |
| `auto` / tip çıkarımı | ❌ Yok | Her şey açık tipli |
| Gizli int↔float dönüşümü | ❌ Yok | Sadece sabit/literal folding'de istisna (ADR-010) |

### Dinamik Array'in Bellek Yükümlülüğü (Gelecek Notu)

`int[]` büyüyebilen array = heap + bir yönetim stratejisi gerektirir. Bu gerçek
bir yükümlülüktür ama **frontend'i bloklamaz** ve kolay yolu vardır:

- **Frontend:** yalnızca "bu int dizisi" bilgisini ister; bellek modelinden habersiz.
- **IR + bytecode VM (ilk çalıştırma modeli, ADR-015):** bellek host (C++)
  heap'idir; array'ler host tarafında `std::vector` benzeri bir yapıyla tutulur.
  VM, array işlemleri için **host fonksiyonlarına** (FFI seam, ADR-016) çağrı
  yapar. v0 için özel allocator gerekmez.
- **C transpile backend (ileride, ikinci backend):** `int[]` → C'de
  `struct {int* data; size_t len, cap;}`, `malloc/realloc/free` ile yönetilir.
- **Yönetim stratejisi (ne zaman free):** scope-tabanlı ownership (array'i tutan
  değişken scope'tan çıkınca free). GC gerekmez — **neden gerekmediği aşağıda
  gerekçelendirildi.**

### Güncelleme — Scope-tabanlı bellek artık GEREKÇELİ (bağımlılığı belgele)

> ⚠️ **İPTAL — bu güncelleme ADR-020 ile geçersiz kılındı.** Bileşik tipler artık
> runtime'da referans (JS/Java/C# modeli); bileşikler scope'tan kaçar, bellek
> erişilebilirliğe bağlı, geri-kazanım stratejisi (#56) gerekecek. Aşağıdaki
> "GC gerekmez" sonucu **artık geçerli değildir** — tarihsel bağlam için bırakıldı.

Önceki kaygı ("scope çıkışında free, aliasing/escape altında bozulur") kilitli
dil kimliğiyle **lehte çözüldü:**

> prosedürel + value semantics + kullanıcı pointer'ı yok + closure yok + kaçan
> referans yok → array/string'ler tanımlandıkları scope'tan **kaçamaz** →
> scope-tabanlı ownership (scope çıkışında free) **gerçekten çalışır, GC
> gerekmez.**

**Bağımlılık açıkça yazılır:** scope-tabanlı bellek, *yalnızca* no-pointer /
value-semantics seçimi sayesinde geçerlidir. `interface` değerleri veya kaçan
referanslar eklenirse bu sorun **yeniden açılır**. (Bu, `interface`'i ertelemenin
ikinci sebebidir — bkz. ADR-018, ADR-019.)

---

## ADR-015: Çalıştırma Modeli — IR + Bytecode VM (Makine-Kodu JIT Kapsam Dışı)

### Bağlam

Daha önceki belge/konuşmalarda çalıştırma için "JIT" terimi geçiyordu. Hangi
çalıştırma modeli? Üç uç var: tree-walker, bytecode VM, gerçek makine-kodu JIT.

### Değerlendirilen Yaklaşımlar

- **Tree-walker (AST'yi doğrudan gez-çalıştır):** en basit, ama **çok yavaş**;
  her çalıştırmada ağaç gezilir.
- **Makine-kodu JIT** (register allocation, ABI/çağırma sözleşmeleri,
  çalıştırılabilir `mmap` bellek): en hızlı; ama **tek faydası ham hızdır**, ki
  burada öncelik değil. Determinizmi ve incelenebilirliği zorlaştırır, devasa
  mühendislik yükü getirir.
- **IR + bytecode VM** (kendi IR'imize derle, yorumlayıcı döngü ile çalıştır):
  determinizm ve incelenebilirliği **doğrudan** sağlar; tree-walker'dan hızlı;
  bellek host heap'iyle kolay.

### Karar

✅ **IR + bytecode VM.** saQut kendi IR'sine derler ve bir yorumlayıcı döngüyle
çalıştırır.

- ❌ **Makine-kodu JIT kapsam dışıdır** (terminoloji düzeltmesi: "JIT" demeyi
  bırak). Öncelikler **determinizm + incelenebilirlik** (toolbox), ham hız değil.
- **Bellek kolaydır:** host (C++) heap'i; özel runtime allocator yok (v0).
- **C'ye transpile, geçerli bir İKİNCİ backend olarak ileride kalır** (frontend
  backend-bağımsız, ADR-006).
- İleride makine kodu **gerçekten** istenirse: elle code generator yazmak yerine
  **libgccjit / LLVM'e bağlan** (ADR-001'deki QBE/custom değerlendirmeleri o gün
  için geçerli). Bu **çok uzak gelecektir.**

---

## ADR-016: FFI Seam — Host Fonksiyon Çağırma Deliği

### Bağlam

`print` bile bir "dış dünya" çağrısıdır; VM tek başına ekrana yazamaz, host'tan
bir fonksiyon çağırmalıdır. Bu ihtiyaç ya **kaza eseri** tek bir özel-durum
olarak gömülür, ya da **kasıtlı bir mekanizma** olarak tasarlanır.

### Karar

✅ **IR/runtime tasarımına bilinçli bir FFI seam konur:** "host fonksiyonu çağır"
için tek, genel bir IR mekanizması (örn. `callhost <id>, args...`).

- `print` bu seam'in **ilk müşterisidir**, özel-durum değil.
- İleride tüm "batteries" (bkz. ADR-017) bu sınır üzerinden gelir: sıkıştırma/
  kripto C kütüphaneleri buraya bağlanır.
- **Neden şimdi:** seam'i sonradan eklemek IR ve VM'i baştan değiştirmeyi
  gerektirir; deliği bir kez doğru açmak ucuzdur. Mekanizmayı **şimdi** doğru
  tasarla, içini sonra doldur.

---

## ADR-017: Batteries / Stdlib — Sınır Problemi (Ertelendi)

### Bağlam

Gerçek bir genel sürüm pil ile gelmeli (sıralama, sıkıştırma, kripto,
JSON/XML/HTML, ileride runtime/donanım, ses/görüntü/video). JSON/string
ergonomisi olmayan bir dil benimsenmez — bu doğru. Ama korku: pilleri çekirdeğe
gömmek **monolit** yaratır.

### Karar

✅ **Pil = sınır (boundary) problemi, "zlib'i yeniden yaz" problemi değil.**

- Çekirdek: **küçük bir gerçek builtin kümesi** (`print`, temel zorunlular) +
  **gerisi kütüphane/FFI.**
- **JSON/XML/HTML ayrıştırıcıları saQut'ta yazılabilir** (string + struct +
  fonksiyon + kontrol akışı yeter) — ilk "gerçek program" demoları.
- **Sıkıştırma/kripto:** denenmiş C kütüphanelerine **FFI** ile bağlan. **Kripto
  asla elle yazılmaz.**
- **Bugüne tek yansıması:** FFI seam'i (ADR-016) bırak. Gerisi **v0 kapsamı
  dışıdır.** Sınır bir kez çizilir, piller üstünde sonsuza dek birikir.

---

## ADR-018: `interface` Ertelemesi (Reddedilmedi)

### Bağlam

Kullanıcı struct'ın yanında `interface` de istedi (crypto, compression, custom
data types, JSON, string için). `interface` alınmalı mı, ne zaman?

### Karar

✅ **Şimdi `struct`, `interface` ise ertelenir (reddedilmez).**

**Neden ertelendi:** `interface`, `struct`'tan kategorik olarak ağırdır.
- Struct yalnızca alan yerleşimidir (field layout).
- Interface "bu metotları sağlayan herhangi bir tip" demektir → çağrı yerinde
  somut tip **bilinmez** → **dinamik dispatch** → vtable / fat pointer (içsel
  pointer, izinli) → ve bir interface değeri **herhangi bir tipi tutabilir**, bu
  da **kaçma/yaşam-süresi (escape/lifetime) problemini yeniden açar** (ADR-019).
- Go bunu fat pointer + GC ile çözer; saQut **GC istemiyor.**

Kullanıcının saydığı her şey (crypto, compression, custom data, JSON, string)
**yalnızca struct + fonksiyonla** yapılabilir (C bunu kanıtlar). Dolayısıyla:
şimdi struct'ı al, interface'i ertele.

**Metot-çağrı şekeri** (`list.push(5)` → `push(list, 5)`) ileride **parser
seviyesinde, sıfır semantik maliyetli** bir desugaring olarak eklenebilir —
şimdi değil.

---

## ADR-019: Frontend ↔ Runtime Sorumluluk Ayrımı

### Bağlam

"Hangi CPU çekirdeği, hangi cihaz, ne zaman tetiklenir, hangi çıktı formatı"
gibi sorular nereye ait? Frontend'e mi, runtime'a mı?

### Karar

✅ **Net ayrım, frontend'i runtime kaygılarıyla yükleme:**

- **Frontend:** **yapı ve anlam** — tip, scope, dataflow. (Bu yol haritasının
  konusu.)
- **Runtime/backend:** çekirdek/cihaz/tetikleme/çıktı formatı.

**Neden:** bu ayrım, kullanıcının önem verdiği **modülerliği korur**; frontend
backend-bağımsız kalır (ADR-006), böylece IR+VM ve ileride C-transpile aynı
frontend'den beslenir. Ayrıca **value-semantics + no-escape** kararının (kaçan
referans/closure yok) bağımlılığı buradadır: bu sayede scope-tabanlı bellek
çalışır (ADR-014). Closure veya interface değerleri eklemek bu ayrımı ve bellek
modelini birlikte zorlar — ikisi de bu yüzden ertelendi.

---

## ADR-020: Değer vs Referans Semantiği — Bileşik Tipler Runtime'da Referanstır

### Bağlam

ADR-014/018/019 boyunca bellek modeli tek bir **taşıyıcı varsayıma** dayanıyordu:

> "kullanıcı pointer'ı yok + kaçan referans yok → array/struct scope'tan kaçamaz
> → scope-tabanlı bellek çalışır, **GC gerekmez**."

Bu varsayım, `interface`'in (ADR-018) ve closure'ın ertelenmesinin de ikinci
gerekçesiydi. Tasarım oturumunda **bilinçli olarak değiştirildi.** "Pointer yok"
ilkesinin gerçekte ne demek olduğu netleşti:

> **"Pointer/referans yok" = kullanıcıya `&`/`*` *sözdizimi* verilmez.**
> Bu bir *value-semantics* iddiası değildi; amacı sözdizimsel pointer kontrolünü
> kullanıcıdan almaktı. Derleyici ve runtime, bileşik değerleri her aşamada
> **referansla** taşır — aksi halde her atama/çağrı/dönüşte derin kopya yaşanır
> ve bağlı yapılar (node) imkânsızlaşırdı.

### Karar

✅ **İki katmanlı semantik (JavaScript / Java / C# nesne modeli):**

| Kategori | Tipler | Atama / parametre semantiği |
|---|---|---|
| **Primitive** | `int`, `float`, `bool`, (`char` vb.) | **Saf değer** — kopyalanır |
| **Bileşik (referans)** | `struct`, `array`, `string`\*, (ileride `class`, `function`) | **Referans** — paylaşılır |

- `a=0; b=a; b=5` → `a` hâlâ `0` (primitive kopya). Fonksiyon parametresinde de aynı.
- `func(arr)` → array'in **kendisi** geçer; `func` içinde değişen çağıranı **etkiler**.
- `func(arr[0])` → eleman primitive → **kopya**; çağıranı **etkilemez**.
- \* `string`'in primitive-gibi mi (immutable değer) yoksa referans mı sayılacağı
  ayrı bir alt-karar; #40 ile beraber netleşecek.

✅ **`class` ve `function` tipleri sözdizimsel olarak rezerve** — şu an semantik
yok, backend'i ilgilendirmez; ileride referans tip olarak gelecekler. Lexer/parser
keyword'leri tanıyıp "henüz desteklenmiyor" diyebilir. (ADR-014'teki "class yok
sayılır" maddesi bu yönde yumuşatıldı: yok sayılmaz, rezerve edilir.)

### Bilinçli geri açtığımız problem: kaçma / yaşam-süresi

Bu karar, ADR-014'ün "scope-tabanlı bellek GEREKÇELİ / GC gerekmez" sonucunu
**iptal eder.** Referansla:

1. **Aliasing gerçek.** `b = a; b.x = 5` → `a.x` de değişir. "Takma ad yok, akıl
   yürütmesi kolay" sadeliği takas edildi. **Determinizm korunur** — tek
   iş-parçacığı, deterministik kayıt-tekrar / time-travel debug hâlâ doğal; takas
   edilen yalnızca aliasing-özgürlüğüdür.
2. **Bileşikler scope'tan kaçar.** Bir node `return` edilebilir veya başka bir
   struct'ın alanında saklanabilir → "scope çıkışında free" **artık yanlış.**
   Sahiplik scope'a değil **erişilebilirliğe** bağlı.
3. **Döngüsel yapılar artık meşru ve istenen.** `struct Node { Node next; }`
   ADR-011/014'te `E010` ile yasaktı (by-value → sonsuz boyut). Referansla alan
   pointer-boyutlu → **sonlu** → bağlı liste / ağaç / graf **yazılabilir.** Bunlar
   dilin hedef kullanımının kalbi: XML node'ları, JSON kalıpları, class'sız ORM.
   **Sonuç:** `E010` revize edilmeli — referansla tutulan struct alanı için döngü
   artık hata değildir.

### Bunun açtığı zorunlu problem (ayrı issue)

Döngüsel referans → naif **referans sayımı (`shared_ptr`) sızdırır.** Bu artık
"olabilir" değil, dilin **hedeflediği** yapıların (graf/döngü) doğrudan sonucu.
Bir geri-kazanım stratejisi (izleyici GC / döngü toplayıcı) **kesinlikle**
gerekecek. Bu güçlü mimari borç **#56**'da izlenir ve `karar-gerekli`. v1 motoru
`shared_ptr` ile başlayıp döngüyü **bilinçli ve belgeleyerek** sızdırabilir, ama
ürünleşmeden önce çözülmek zorundadır.

### İptal/revize edilen önceki kararlar

- **ADR-014** — "scope-tabanlı bellek GEREKÇELİ / GC gerekmez" sonucu **iptal.**
  Bellek artık scope'a değil erişilebilirliğe bağlı; geri-kazanım stratejisi #56.
- **ADR-018 / ADR-019** — `interface` / closure'ı ertelemenin "kaçma problemini
  yeniden açar" gerekçesi **artık geçersiz** (problem zaten açık). Bu ikisini
  *daha kolay* alınabilir kılar — ama hâlâ kapsam dışı, sadece engeli değişti.
- **ADR-011** — `E010` döngüsel struct kuralı revize edilecek (yukarı bkz.).

---

## ADR-021: Null Güvenliği — `Type?` Nullable + Akış-Duyarlı Null Analizi

### Bağlam

ADR-020 ile bileşik tipler referans oldu. Referans, "gösterecek bir şey yok"
durumunu (bağlı listenin sonu, başlatılmamış alan) **zorunlu** kılar. "null her
yerde" (Java/C#/JS) milyar dolarlık hatadır: null-deref çalışma zamanında patlar.
saQut'un kimliği "kafes — derleyici/VM seni korur" → bunu derleme zamanında
yakalamak istiyoruz.

### Karar

✅ **Kotlin/Swift modeli: varsayılan null-OLAMAZ, nullable açıkça `?` ile.**

- `Node a` → asla null olamaz; başlatılması zorunlu.
- `Node? a` → null olabilir; başlatılmazsa değeri **`null`**.
- `null` literali yalnızca `T?` tipine atanabilir; `Node a = null` → **derleme hatası**.
- `T?` üstünde doğrudan alan/eleman erişimi (`a.next`) → **derleme hatası**
  (önce null-kontrolü şart).

### Atama/operand kuralı — `T <: T?` (tek yönlü), katı

✅ **Alt-tip:** `T <: T?`. Yani:
- `int? a = 5;` → ✓ (int → int?, **genişletme serbest**).
- `int a = bir_int?;` → ✗ (int? → int, **daraltma yasak**).

✅ **Katı operand kuralı:** non-null bir bağlamda — atamanın sol tarafı, **her
operatör operandı**, non-null bekleyen bir argüman — değer **statik olarak non-null**
olmalı. `int a = b + c + d`'de `b/c/d`'den **biri bile** nullable ise → **derleme
hatası.** Sembol tablosu/akış görünümü seviyesinde: `notnull = notnull + notnull + …`.
Sezgi yok, deterministik. (`notnull + notnull` → `notnull`.)

### Akış-duyarlı null analizi (flow-sensitive narrowing)

`T?` bir değişken **program noktasına göre** "kesin non-null" kanıtlandıysa
daraltılır:

```c
Node? a = ...;
// a.next;            // E0xx: a null olabilir
if (a != null) {
    a.next;           // OK — bu dalda a, Node'a daraltıldı
}
// a.next;            // yine hata (daldan çıkıldı)

if (a == null) return;   // guard / erken çıkış
a.next;                  // OK — buradan sonrası kesin non-null
```

`if` bu sistemin **bel kemiğidir** — sadece büyük/küçük/eşitlik değil, nullable
aklamanın da aracı. **İki form da desteklenir:**

1. **Nested (blok-kapsamlı):** `if (a != null) { /* a: T burada */ }`. Ayrıca
   `if/else`'in zıt dalı, `while (a != null) { … }`.
2. **Sıralı (guard / erken-çıkış):** `if (a == null) return; /* a: T bundan sonra */`.
   Dal kesin çıkıyorsa (`return`/`throw`/`break`/`continue`) negasyonu **ardışık**
   koda taşınır.

**Mekanik:** CFG üzerinde ileri-yönlü dataflow; her nullable değişken için kafes
`{MaybeNull, NonNull}`. Koşullarda daraltma (`!= null`, `== null` guard, `&&`
kısa-devre sağ tarafı); kesin-çıkış dalları negasyonu ardına taşır; birleşme (join)
muhafazakâr (bir daldan MaybeNull gelirse MaybeNull); atama RHS'e göre sıfırlar.

> **Karmaşıklaştırma sınırı:** narrowing yalnızca **doğrudan test edilen değişken**
> için tanınır (`x == null`/`x != null`). **Alias takibi YOK** (`y = x; if (y != null)`
> → x daralmaz) ve keyfi teorem-ispatı yok. Bu, derleyiciyi basit tutarken yaygın
> durumların hepsini kapsar → developer **uzun/karmaşık kod yazmak zorunda kalmaz.**

> **Runtime maliyeti SIFIR** — tamamen derleme-zamanı analizi; üretilen kodda
> fazladan kontrol yok.

### Kaçış kapısı YOK — `!` ve `??` YASAK

Null **yalnızca görünür kontrol akışıyla** (yukarıdaki `if` narrowing) aklanır.
Gizli runtime null-aklama operatörleri **yasaktır:**

- ❌ **`x!`** (non-null iddiası) — "compiler'a güvenme, runtime'da kontrol et"
  = statik garantiyi delen gizli backdoor. *(ADR-021'in ilk taslağındaki `a!`
  KALDIRILDI.)*
- ❌ **`x ?? default`** (elvis), **`x?.field`** (güvenli çağrı) — null durumunu
  sessizce gizleyen şeker.

> Ayrım: `as int`'in başarısızlıkta fırlatması yasak **değil** — o bir null-backdoor
> değil, kendiliğinden başarısız olabilen bir *dönüşüm* (ADR-026).

### Frontend her şeyi kesin çözer (backend-bağımsızlık)

Nullability **tamamen frontend'de** çözülür; tüm null-güvenlik hataları IR'den
**önce** verilir. Backend'ler (IR+VM, ileride C-transpile) null-güvenliği **yeniden
analiz etmez** — garantiyi hazır devralır (ADR-006/019). Bu sayede: well-typed saf
saQut kodu **statik null-güvenlidir** → non-null referans deref'i runtime null-kontrolü
**gerektirmez** (perf + sadelik). Runtime null-deref hatası (ADR-025) bu yüzden
geriye esas olarak **FFI sınırı** (host non-null sözünü çiğnerse) ve savunma amaçlı
backstop olarak kalır — saf saQut kodu bunu üretmez.

### Mimari yeri

Bu, saQut'un ilk gerçek **akış-duyarlı** analizidir. **Yapısal kontrol akışı**
üstünde (AST + structured CFG) yapılabilir; tam SSA gerektirmez → **#2 (CFG/SSA
gerekli mi?)** için somut veri: şimdilik yapısal akış analizi yeter. **#20**
(akıllı diagnostic) bu analizden beslenir ("burada null olabilir, çünkü …").

---

## ADR-022: Bellek Geri-Kazanımı — Basit Deterministik Mark-Sweep + GC-Hazır Nesne Modeli

### Bağlam

ADR-020 referans semantiği → döngüsel yapılar (#56). Kısıtlar: GC **basit ve
deterministik** olmalı, "karmaşık ve rastgele" istenmiyor. Ayrıca bu, **geç
değiştirilmesi en pahalı** karardır (nesne modeline işler) → topuğa sıkmamak
kritik.

### Önce yanlış-eşleştirmeyi temizle

**`null`/`?` GC'yi zorlaştırmaz.** Nullable tamamen derleme-zamanı/tip meselesidir;
runtime'da null referans sadece "boş işaretçi" → GC için *daha kolay* (izlenecek
nesne yok). null ile GC **dik (orthogonal)**; aralarında gerilim yoktur.

### Seçenekler ve neden mark-sweep

| Strateji | Döngü | Basitlik | Topuğa-sıkma riski |
|---|---|---|---|
| Refcount (`shared_ptr` her yerde) | ❌ sızdırır | başta basit | **Yüksek** — node dilinde döngü kaçınılmaz; üstüne döngü toplayıcı = CPython karmaşıklığı (tam "karmaşık/rastgele") |
| **Mark-sweep, taşımasız, stop-the-world** | ✅ | **en basit *doğru* GC** | **Düşük** — gelişmiş GC'lerin tabanı; üstüne eklenir, yeniden yazılmaz |
| Generational / incremental / compacting | ✅ | karmaşık (write barrier, remembered set) | pause'lar belirsizleşir = istenmeyen "rastgele" |

✅ **Karar: taşımasız (non-moving), stop-the-world, basit mark-sweep.**
- Döngüleri **bedavaya** toplar (izleme döngü umursamaz) → #56'yı gerçekten çözer.
- **Deterministik:** GC belirli safepoint'lerde çalışır (ör. her N tahsiste) →
  kayıt-tekrar / time-travel bit-aynı kalır ("cage" korunur). "Rastgele" değil.
- Taşımasız → işaretçi düzeltme / barrier yok → VM'in geri kalanı GC'ye katılmak
  zorunda değil. *Crafting Interpreters*'ın `clox`'u tam bunu yapar (~birkaç yüz satır).

### Topuğa-sıkmama kuralı — nesne modelini ŞİMDİ GC-hazır kur

Asıl risk GC'yi *yazmak* değil, nesne modelini sonradan ona uyduramamaktır. O
yüzden **bugünden** (toplama yokken bile):

1. Her heap nesnesine küçük **header**: tip tag + mark biti + tüm-nesneler listesi için `next`.
2. VM **kök (root) sayımı** yapabilsin: operand stack, frame local'leri, global'ler.
3. Bir nesne **içerdiği referansları** sayabilsin: referans-tipli struct alanları,
   referans-tipli array elemanları.

Bu üçü hazırsa "mark-sweep'i aç" **lokal bir ekleme** olur, nesne-modeli yeniden
yazımı değil.

### Aşamalandırma (#56'nın yönü)

- **v1 (şimdi):** GC-header'lı tahsis + intrusive tüm-nesneler listesi + kök sayımı.
  **Toplama yok** (program sonunda hepsini bırak / arena). Fibonacci/test ölçeğinde
  sorunsuz; kısa programlar sızıntıdan etkilenmez.
- **v2 (#56 ciddileşince):** aynı header+kök+çocuk-sayımı üstünde mark-sweep'i aç.
  Model yeniden yazılmaz.
- **`shared_ptr`'dan kaçın:** v1'de bile her referansa refcount gömmek, sonra
  mark-sweep için **sökmek** ayrı bir topuğa-sıkmadır. Baştan GC-header modeli kur,
  sadece henüz toplama.

### Performans notu — asıl "katil" nerede?

- **Nullability / null:** runtime maliyeti **sıfır** — katil değil.
- **Referans modeli:** her bileşik heap'te + işaretçi dolaylılığı → düzenli ama
  yönetilebilir maliyet; ileride **escape analizi** ile kaçmayan nesneleri stack'e
  alıp *semantiği bozmadan* hızlandırılır (opt-in, sonra).
- **Tek yüksek-değişim-maliyetli karar = GC.** Onu da (a) basit mark-sweep seçip
  (b) modeli baştan GC-hazır kurarak de-risk ettik. **Kaçınılacak gerçek katil:
  refcount'u kalıcı model yapmak.**

---

## ADR-023: Eşitlik Semantiği — Referanslarda Kimlik Eşitliği (`==`)

### Bağlam

ADR-020 ile bileşik tipler referans. `==` / `!=` referans tipler için ne yapsın?
Yapısal (derin) eşitlik sezgisel ama üç sorunu var: (1) büyük yapıda **derin
gezinme maliyeti**, (2) yeni açtığımız **döngüsel grafta sonsuz döngü** riski
(ziyaret-takibi şart), (3) seçtiğimiz referans modeliyle **tutarsız**.

### Karar

✅ **Kimlik eşitliği (A):**

| Kategori | `==` davranışı |
|---|---|
| Primitive (`int`/`float`/`bool`) | **değer** karşılaştırması (`3 == 3`) |
| Referans (`struct`, `array`) | **kimlik** — aynı nesne mi? (işaretçi aynılığı) |
| `string` | ⏸️ **#40'a bağlı** — aşağıdaki nota bak |
| `null` | `null == null` → true; `null == nesne` → false; `a == null` null-daraltma deyimi (ADR-021) |

İçerik karşılaştırması istenirse **ayrı, niyeti görünür** bir mekanizmayla gelir
(ileride builtin `deepEquals()` / PHP'nin `==` vs `===` vs `clone` ailesi gibi) —
asla sessizce `==`'e bağlanmaz. Gerekçe: deepEqual'ı `==`'e bağlamak büyük/döngüsel
yapılarda performans ve sonsuz-döngü tuzağıdır; "cam kutu, sürpriz yok" kimliğiyle
de çelişir.

### ⚠️ String istisnası (Java gotcha'sı)

Saf kimlik eşitliğini string'e de uygularsak `"abc" == "abc"` → **false** olur —
Java'nın en çok sövülen hatası. Çoğu dil string'i istisna yapar (JS'te string
primitive → içerik; C# overload; Python intern). Bu yüzden **string'in `==`'i
içerik eşitliği olmalı**, ki bu string'i **immutable değer-tipi** olarak modellemeyi
güçlü biçimde öneriyor (bkz. #40). ADR-023 struct/array'i kilitler; string'in `==`'i
#40'ta netleşir ama **varsayılan yön: içerik eşitliği.**

### Açık (ileride, çok uzak — şimdi karar değil)

- **`obj == obj`'i hata/uyarı yapmak:** kullanıcıyı niyetini açık yazmaya zorlamak
  (kimlik mi içerik mi). Daha katı bir duruş; v0'da `==` = kimlik serbest.
- **Kullanıcı-tanımlı eşitlik (OOP'siz):** ileride bir tip için `equals(T,T)->bool`
  konvansiyonu veya benzeri ile `==`'i kullanıcının tanımlamasına izin vermek —
  operator-overload'un OOP'siz karşılığı. Çok uzak.

---

## ADR-024: String — Immutable Değer-Tipi, İç Temsil UTF-8

### Bağlam

ADR-020 string'i "bileşik (referans)" listesine `?` ile koymuştu; ADR-023 string
`==`'inin **içerik** olmasını istedi (Java gotcha'sından kaçınmak için). İkisi de
string'i değişmez-değer modeline itti.

### Karar

✅ **String = immutable (değişmez) değer-tipi; iç temsil UTF-8 bayt.**

- **Immutable:** oluşturulduktan sonra içeriği değişmez; `s = s + "x"` **yeni**
  string üretir, eskisini değiştirmez.
- **`==` içerik eşitliği** (ADR-023 istisnası). Paylaşılınca değişmediği için
  içerik-eşitliği güvenlidir; aliasing sürprizi yok (JS'in string'i primitive gibi
  davranmasının sebebi budur).
- **GC dostu:** serbestçe paylaşılır / intern edilebilir.
- **İç temsil UTF-8** (Rust/Go/Swift hattı): kompakt, web-doğal, ASCII'de ucuz.
  `s[i]` **karakter** indeksi O(1) **değildir** → bayt / scalar / grapheme erişimi
  **açıkça** ayrılır; sahte O(1) vaat edilmez (Java/JS'in "uzunluk emoji'de yalan
  söylüyor" sürprizinden kaçın). Host tarafında `std::string` ham bayt olarak oturur.
- **Verimli birleştirme** için ileride ayrı **builder** tipi (StringBuilder / `join`)
  — çekirdeği kirletmeden, döngüde O(n²)'den kaçınmak için.

### Etkilenen

- **#40** (string işlem yüzeyi) bu kararla netleşti; **#9** (iç temsil) = UTF-8.
- ADR-020'deki string `?` işareti → "değer-tipi" olarak çözüldü.

---

## ADR-025: Hata Yönetim Modeli — Struct-Tabanlı Yakalanabilir Hatalar (Swift-tarzı)

### Bağlam

ADR-020 (struct = referans) → null bir struct alanına erişim/yazma ihtimali doğdu:
klasik NullPointerException. ADR-021 statik analizi *kanıtlayabildiğini* derleme
zamanında yakalar, ama `!` iddiası ve kanıtlanamayan durumlar (struct alanı,
cross-fonksiyon) için bir **runtime backstop** gerekir. Ayrıca array OOB, /0 gibi
faults. Java/C#/JS bunları **yakalanabilir** hata yapar — ama OOP exception
hiyerarşisi (`extends Exception`) bizde yok.

### Karar

✅ **Yakalanabilir, struct-tabanlı hata modeli — OOP'siz.**
Hata *değeri* Swift gibi (düz struct, hiyerarşi/extend yok); *görünürlük* Java/C#/JS
gibi (**unchecked** — fonksiyon işaretlenmez, klasik `try{}catch{}`). "Exception'ın
tanıdık catch-and-jump ergonomisi + OOP'suz değer."

1. **Hata değeri = standart built-in struct** — extend yok, OOP yok, deterministik:
   ```
   struct Error {
       int    line;      // hata satırı
       int    col;       // sütun ("char" tip adıyla çakışmaması için col)
       string message;   // insan-okunur (derleyicinin W/E kataloğundan)
       string trace;     // stacktrace, en içten dışa
       string code;      // makine-okunur W/E kodu (E010 vb.) — JSON/toolbox filtresi
   }
   ```
2. **try/catch (unwind + jump):** hata oluşunca en yakın çevreleyen `catch`'e
   zıplanır; `catch (e)` → `e : Error`.
3. **Runtime null-deref = yakalanabilir hata** (NPE analoğu). ADR-021 statik
   analizinin **backstop'u**: `a!` patlayınca + analizin kanıtlayamadığı durumlar.
   Array OOB ve /0 da aynı kapıdan.
4. **`throw`** ile kullanıcı da hata kaldırabilir (`Error` doldurup).
5. **Determinizm:** unwind deterministik; stacktrace frame'lerden üretilir;
   time-travel/replay handle eklenebilir.

### Görünürlük — KARAR: (ii) görünmez / unchecked (Java/C#/JS usulü)

✅ **Fonksiyonlar işaretlenmez.** "Bu hata yapabilir / yapamaz" anotasyonu **YOK**
(C++'ın `noexcept`/`constexpr` benzeri kirlilik istenmiyor). Çağrıda `try f()`
işareti de yok. **Klasik `try { ... } catch (e) { ... }` bloğu** — "anam babam usulü".

**Gerekçe:** developer'a **güven** + insanların derin try-catch alışkanlığını bozmamak
(sözdizimini tanıdık tut, içgüdüye dokunma). Hatalar zaten çoğunlukla FFI, bellek
dolması ve derleyici-içi durumlardan doğar; her çağrıyı işaretlemenin bedeli faydadan
büyük.

> Not: Bu, `Type?` (explicit nullable) ile **bilinçli** felsefi ayrışmadır — null
> *tipte* görünür, ama hata akışı *blok* düzeyinde tanıdık tutulur. Reddedilen (i):
> Swift/Zig'in imza-işaretli + çağrıda `try f()` modeli.

### Stacktrace mekaniği (modelden bağımsız önkoşul)

- Her `CallFrame` → `IRFunction` + komut işaretçisi; **IR'a satır tablosu**
  (komut index → kaynak konum) eklenir (önce taşıyıp taşımadığı doğrulanmalı).
- panic/throw'da frame stack gezilir → `fonksiyon + konum`, en içten dışa → `trace`.
- Sunum: derleme-zamanı diagnostic ile **aynı kabuk** (kod + mesaj + konum +
  "nasıl düzelt" #20), hem insan hem **JSON** (toolbox: her hata yapılandırılmış nesne).
- Farklılaştırıcı: deterministik → trace'e adım indeksi → hataya **geri sar**.

### İlişkili güncellemeler

- **ADR-014 "tuple yok" → "tuple ERTELENDİ"** (reddedilmedi; çoklu-dönüş kodu
  spagettileştirir, şimdilik uzak ama masada — `interface` gibi).
- **`finally` yerine ileride `defer`** (GC'li, RAII'siz dilde daha temiz). Ayrı küçük karar.
- ADR-021 ile uyum: statik analiz provable null'ı yakalar; bu hata onun backstop'u + `!`.

---

## ADR-026: Tip Dönüşümü — `as` (Skaler/String), Başarısızlık Hedef Tipinin Nullable'lığıyla

### Bağlam

ADR-010 "gizli int↔float yok" → değişken-değişken dönüşüm **açık** olmalı. float
runtime'ı var ama cast sözdizimi yoktu → int↔float dönüşümü imkânsızdı. Ayrıca
elimizde null (ADR-021) + hata (ADR-025) modelleri var; cast bunlarla örtüşmeli.

### Karar

✅ **Sözdizimi: `as` (infix, sola-bağlı).** `deger as int`.
- Sola-bağlı olduğu için zincir **lineer** okunur: `a as int as string` =
  `((a as int) as string)`, parantez gerekmez (fonksiyon-stili `int(float(a))`'nın
  iç içe çirkinliği yok).
- `int(x)` fonksiyon-stili **reddedildi** ("int adlı fonksiyon mu, cast mı"
  belirsizliği); C-tarzı `(int)x` ve `static_cast<>` reddedildi.

✅ **Kapsam: yalnızca skaler + string** (`int`/`float`/`bool`/`string` arası).
- **Struct/array cast'e GİRMEZ.** Farklı struct'lar ayrı tiplerdir; "dönüşümleri"
  geliştiricinin yazdığı **açık yapıcı fonksiyonlarla** olur (`Employee yap(Person p)`).
  Gerekçe: yapısal/duck eşleme veya reinterpret = derleyiciyi karmaşıklaştırır +
  sessiz alan kaybı = hataya açık. OOP'siz "cage" kimliğiyle uyumsuz.

✅ **Başarısızlık davranışı = HEDEF TİPİN nullable'lığı** (ayrı `as?` operatörü YOK):
- `x as int` → hedef non-null → başarısızsa **`Error` fırlatır** (ADR-025), sonuç `int`.
- `x as int?` → hedef nullable → başarısızsa **`null` döner**, sonuç `int?`.

Nullable her zaman **tipte** (`?`) yaşar, ayrı operatör icat edilmez. Sonra `int?`'i
`int`'e çevirmek için **narrowing** (`if`) gerekir — `!`/`??` yasak (ADR-021).

### Dönüşüm matrisi

| Dönüşüm | Hatasız mı? | Not |
|---|---|---|
| `int → float` | ✅ hatasız | büyük int'te kesinlik kaybı olabilir, patlamaz |
| `int → string`, `float → string` | ✅ hatasız | biçimlendirme |
| `string → int`/`float` | ⚠️ fallible | parse; `"abc"` → `as int` fırlatır / `as int?` null |
| `float → int` | ⚠️ fallible | sonlu & aralık-içi: **sıfıra doğru kırpılır** (`1.71→1`, `-1.71→-1`); NaN/Inf/taşma → fırlatır / null |
| `bool ↔ int` | (karar) | başta yasak tutmak en güvenlisi; gerekirse açılır |

### Örnek (ADR-021 ile birlikte)

```
int a = 1.71 as int?;   // ✗ DERLEME HATASI: int? → int (daraltma); cast başarılı olsa bile statik tip int?
int a = 1.71 as int;    // ✓ a = 1 (kırpma); başarısızsa Error
int? a = 1.71 as int?;  // ✓ tipler eşit
```

---

## ADR-027: switch-case — Statement, Fallthrough Yok, Tip-Homojen, Float İzinli (uyarılı)

### Bağlam

C-ailesi bir dilde `switch` bekleniyor ama klasik switch'in iki ünlü gotcha'sı var
(implicit fallthrough; float eşitliği). saQut'un gotcha-avcısı kimliğiyle bunları
ele almak gerek.

### Karar

✅ **Statement** (değer döndürmez). Expression-switch (değer dönen `switch`) **ileride**,
sözdizimini bozmayacak şekilde eklenebilir; v0 statement.

✅ **Implicit fallthrough YOK.** Her `case` kendi bloğu, **otomatik break** (Go/Rust/
Swift/C# gibi; C'nin `break`-unutma tuzağı yok). Bilerek paylaşım → **çok-değerli case:**
```
switch (x) {
    case 1, 2, 3:  ...     // üçü aynı dal
    case 4:        ...
    default:       ...
}
```
`fallthrough` keyword'ü **yok** (çok-değerli case yeter; sadelik).

✅ **Tip-homojenlik (exhaustiveness DEĞİL).** Case etiketleri **switch konusunun
tipiyle aynı** olmalı: string'e switch → tüm case'ler string (int olamaz); enum'a
switch → case'ler o enum'un **üyeleri** (`Color.Red`), ham int/string değil; karışık
**yasak** ("ortalık karışmasın"). **`default` opsiyonel**, eşleşmeyenleri yakalar;
eşleşme yoksa no-op.
- **Mandatory exhaustiveness reddedildi:** 300-öğeli enum'da 5'iyle ilgilenip 300 case
  yazmak saçma. (İleride **opt-in** exhaustiveness — örn. işaretli switch — dönebilir.)

✅ **Domen:** `int`, `float`, `bool`, `char`, `string`, `enum`. **Struct/array YOK**
(referans kimliği üstünde switch nadiren anlamlı).

✅ **Float İZİNLİ — ama tam-temsil-edilemeyen literal case'de W-uyarısı.**
- `case 1.5:` / `2.5` / `3.5` → binary float'ta **birebir** → çalışır, **uyarı yok.**
- `case 0.1:` → tam değil → **W-uyarı:** "0.1 float'ta tam temsil edilemez, eşleşme
  güvenilmez." (Frontend: literal'i double'a çevir, geri çevir, eşit mi diye bak.)
- Gerekçe: C/C++/Java float-switch'i **yasaklar**, Rust float pattern'i **kaldırdı** —
  sebep tam bu eşitlik tuzağı. Biz **yasaklamak yerine aydınlatıyoruz** (#20 ruhu).

### Mevcut kararlarla bağlar

- `switch (T?)` + `case null:` → null'ı bir dal olarak ele al (ADR-021 narrowing'le tutarlı).
- `catch (e)` içinde `switch (e.code)` → hata kodu dallanması (ADR-025 `code` alanı).
- enum (#8) ile güçlü. **Pattern matching / destructuring / guard / range = uzak gelecek**
  (şimdilik değer-eşleme; derleyiciyi sade tut).

---

## Kararların Özet Tablosu

| ADR | Konu | Karar |
|---|---|---|
| 006 | Frontend mimarisi | Çok-aşamalı; frontend/middle-end/backend katmanları |
| 007 | Analiz vs optimizasyon | Analiz yerinde; `ast` komutu klon üstünde dönüştürür (öncesi/sonrası karşılaştırması); `run`/`ir` yerinde optimize eder (klon yok); sembol bağları salt-okunur paylaşım (remap gerekmez) |
| 008 | Optimizasyon konumu | Basitler AST'de, dataflow gerektirenler IR'de |
| 009 | Pass yönetimi | Fixpoint döngüsü, toggle'lı; monotonluk/iterasyon-tavanı değişmezi; akışa-bağlı analiz tur başına tazelenir |
| 010 | Tip sistemi | Minimal+genişletilebilir Type; gizli dönüşüm yok; Error tipi; tamsayı literali bağlama-göre tiplenir |
| 011 | Scope/forward ref | Global'de forward ref (fonksiyon/struct), ama global başlatıcı declare-before-use; lokal declare-before-use; döngüsel struct → `E010` |
| 012 | Node hiyerarşisi | ExpressionNode / StatementNode ara tabanları |
| 013 | Analiz verisi yeri | Her şey AST'de; ref-count Symbol'da |
| 014 | Dil kapsamı | Pointer/class/generic/closure yok; struct+array+tipli fonksiyon var; scope-tabanlı bellek gerekçeli |
| 015 | Çalıştırma modeli | IR + bytecode VM; makine-kodu JIT kapsam dışı |
| 016 | FFI seam | Kasıtlı "host fonksiyonu çağır" mekanizması; `print` ilk müşteri |
| 017 | Batteries/stdlib | Sınır problemi; küçük builtin + FFI/kütüphane; ertelendi |
| 018 | `interface` | Ertelendi (reddedilmedi); struct+fonksiyon yeter |
| 019 | Frontend↔runtime | Frontend yapı+anlam; çekirdek/cihaz/çıktı runtime'a ait |
| 020 | Değer/referans semantiği | Primitive=değer, bileşik (struct/array/string)=referans; "pointer yok"=`&`/`*` sözdizimi yok; kaçma/lifetime problemi bilinçli açıldı → GC borcu (#56); ADR-014'ün "GC gerekmez" sonucu iptal |
| 021 | Null güvenliği | `Type?` nullable, varsayılan non-null; akış-duyarlı null analizi (compile-time, runtime maliyeti sıfır); `!` runtime-kontrollü non-null iddiası |
| 022 | Bellek geri-kazanımı | Basit taşımasız stop-the-world mark-sweep (deterministik); nesne modeli baştan GC-hazır (header+root+child); v1 toplamasız, v2 mark-sweep; refcount kalıcı model DEĞİL; #56'nın yönü |
| 023 | Eşitlik semantiği | `==` = primitive değer / referans (struct,array) **kimlik**; deepEqual asla `==`'e bağlanmaz (ayrı `deepEquals()`); string `==` içerik (→ #40, Java gotcha'sından kaçın); `obj==obj` hata + kullanıcı-tanımlı eşitlik = uzak gelecek |
| 024 | String | Immutable değer-tipi, iç temsil **UTF-8**; `==` içerik; mutasyon yeni string üretir; bayt/scalar/grapheme açıkça ayrı; verimli birleştirme için ileride builder; #40/#9'u çözer |
| 025 | Hata yönetimi | Struct-tabanlı yakalanabilir hata (değer Swift gibi, OOP yok); standart `Error{line,col,message,trace,code}`; klasik `try{}catch{}` **unchecked** (fonksiyon işaretsiz, Java usulü); runtime null-deref/OOB yakalanabilir (esasen FFI backstop); deterministik stacktrace (IR satır tablosu); tuple→ertelendi; finally→`defer`; #57 |
| 026 | Tip dönüşümü | `as` (infix, sola-bağlı), yalnızca skaler+string; struct/array cast YOK (elle yapıcı fonksiyon); başarısızlık hedef nullable'lığıyla (`as int` fırlatır / `as int?` null); float→int kırpma; #42 |
| 027 | switch-case | Statement (expression sonra); implicit fallthrough YOK (çok-değerli `case 1,2,3:`); case'ler tip-homojen (exhaustiveness DEĞİL, `default` opsiyonel); domen int/float/bool/char/string/enum (struct/array yok); float izinli + tam-temsil-edilemeyen literal → W-uyarı |
| 028 | decimal tipi | İki virgüllü tip: `float` (binary) + `decimal` (ondalık, finansal). Hiyerarşi `int < float < decimal`; karışık ifadede en geniş tip kazanır. İç temsil: `int64_t coefficient × 10^exponent`. Literal bağlam-güdümlü, suffix yok. NaN/Inf yok — taşma/sıfır-bölme Error fırlatır. `as` ile cast: decimal↔int/float/string; `float + decimal` sessiz decimal'e terfi. |

---

## ADR-028: `decimal` Tipi — İki Virgüllü Tip, Ondalık Aritmetik

### Bağlam

Developer'ın ne kodlayacağını önceden bilemeyiz: fibonacci fibonacci için `float`
yeter, finansal algoritma için binary float'ın `0.1 + 0.2 ≠ 0.3` sorunu bir bug
kaynağıdır. Bir dil olarak her iki kullanım alanına kapı açmak zorunludur.

### Karar

✅ **İki ayrı virgüllü tip:**
- `float` (mevcut) = C++ `double` arkaplanlı, binary IEEE 754, ~15-16 basamak.
  Genel hesap, hız öncelikli.
- `decimal` (yeni) = ondalık hassasiyet, finansal/bilimsel kullanım.
  İç temsil: `int64_t coefficient × 10^exponent` (~18 ondalık basamak).
  Bağımlılık sıfır, taşınabilir.

✅ **Tip hiyerarşisi (genişletildi):** `int < float < decimal`
Karışık ifadede **en geniş tip kazanır:**
```
int + decimal   → decimal   (int sessizce decimal'e terfi)
float + decimal → decimal   (float sessizce decimal'e terfi — IEEE pislikleri kabul)
decimal + int   → decimal
decimal op decimal → decimal
```
Gerekçe: `float → decimal` terfi **bilinçli bir seçim** — developer `decimal`
kullanarak "bu hesapta hassasiyet istiyorum" demiş demektir. Float'ın binary
bozukluğunu içeri almak onun sorumluluğundadır; biz gizlemek yerine aydınlatıyoruz.

✅ **Literal: bağlam-güdümlü, suffix yok (ADR-010 kuralına uyar):**
```saQut
decimal price = 19.99;    // parser "19.99" stringini decimal'e tam çevirir
decimal x     = 1;        // int literal → decimal terfi
float   f     = 1.1;      // float literal → binary double (değişmez)
```
`m` veya `d` gibi literal suffix'i **reddedildi**: ADR-010 bağlam-güdümlü tipin
zaten bu işi yapabileceğini kararlaştırdı; `19.99m` karmaşıklık katar, kazanç yok.

✅ **NaN/Inf yasak; taşma/sıfır-bölme Error fırlatır:**
`float`'ın IEEE 754 NaN/Inf davranışı korunur (C++ double mecburi). Ama `decimal`'de:
- Sıfıra bölme → `Error` (kod: `"E-DECIMAL-DIVZERO"`)
- Katsayı taşması → `Error` (kod: `"E-DECIMAL-OVERFLOW"`)
- Alt-taşma (underflow, çok küçük): sessizce sıfıra yuvarlanır.
Gerekçe: "sonsuz diye bir şey bilgisayarda yoktur" — belirsiz özel değerler yerine
her zaman belirli bir hata veya sıfır.

✅ **`as` cast (ADR-026 genişlemesi):**
| Dönüşüm | Hatasız mı? |
|---|---|
| `int as decimal` | ✅ hatasız |
| `float as decimal` | ✅ hatasız (float'ın binary temsili kabul edilir) |
| `string as decimal` | ⚠️ fallible — parse edilemezse Error/null |
| `decimal as int` | ⚠️ fallible — sıfıra kırpar; taşma → Error/null |
| `decimal as float` | ✅ hatasız (kesinlik kaybı kabul: kullanıcı bilinçli seçti) |
| `decimal as string` | ✅ hatasız |

✅ **`==` semantiği (ADR-023 ile tutarlı):**
`decimal` primitiftir → değer karşılaştırması. `0.30 == 0.3` → `true`
(her ikisi normalize edilince aynı coefficient+exponent).

### Elenen alternatifler

- **`std::decimal` (C++ TR 24733):** Standart dışı; GCC uzantısı, MSVC/Clang taşınabilirliği kırık.
- **`mpdecimal` kütüphanesi:** Güçlü ama dış bağımlılık; saQut'un sıfır-bağımlılık ilkesiyle çelişir.
- **`float as decimal` derleme hatası:** C#'ın katı yaklaşımı. saQut için fazla kısıtlayıcı; user "decimal fonksiyona float argüman geç" dediğinde sürpriz üretir.
- **Literal suffix (`19.99m`):** Bağlam-güdümlü tip zaten yeterli; ADR-010'a saygı.

### Etkilenen bileşenler

- `PrimitiveKind::Decimal` + `Type::Decimal()` (type.hpp)
- `DecimalValue` struct — `core/decimal.hpp` (yeni dosya)
- `ValueKind::Decimal` + `Value::fromDecimal()` (value.hpp)
- 14 yeni IR opcode'u: `LOAD_DECIMAL`, `DADD/DSUB/DMUL/DDIV/DMOD`, `DNEG`,
  `INT_TO_DECIMAL`, `FLOAT_TO_DECIMAL`, `CAST_DECIMAL_TO_STR/INT/FLOAT`, `CAST_STR_TO_DECIMAL`
- Tip denetleyicisi: `numericRank(Decimal) = 3`
- IR üreteci: binary aritmetik + literal + variable decl + cast
- VM interpreter: tüm decimal opcode'ları
