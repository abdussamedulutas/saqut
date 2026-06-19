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
