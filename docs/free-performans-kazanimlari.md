# Bedava Performans Kazanımları

**Bedava** = kullanıcının karşısına çıkmadan, dil sözleşmesini değiştirmeden,
gözlenebilir davranışı bozmadan yapılabilen kazanımlar.

Bu dosya bu oturumdaki ölçümlerin özeti ve henüz uygulanmamış maddelerin
listesidir. **Hiçbir rakam tahmin değil** — hepsi ölçüldü, ölçüm yöntemi
her maddede yazıyor.

Ölçüm ortamı: `examples/large.sqt` (2.3 MB, 1.059.635 token, 371.915 IR
talimatı), `-O3 -DNDEBUG`. Bu makine bench'ler arası ~%10 dalgalandığı için
tüm karşılaştırmalar aynı oturumda dönüşümlü (eski/yeni/eski/yeni) yapıldı.

---

## Bölüm 0 — Bu oturumda UYGULANAN (kayıt için)

| iş | ölçülen kazanç | commit |
|---|---|---|
| **VM breakpoint sıcak yolu** | **VM ~1.87x; tahsis 153M → 18.8M (8.2x az)** | (bu oturum) |
| Parser token önbelleği | parse 172ms → 95ms (**~1.95x**) | `7fa3088` |
| `SourceLocation` → `fileId` | 48→16 bayt; tokenize ~1.20x; uzun yolda tahsis 9.5M→4.47M | `1332bb8` |
| Lexer yığın sızıntısı | derinlik 8481 → 1 (hız etkisi yok, sınırsız büyüme durdu) | `7fa3088` |

### VM breakpoint sıcak yolu — bu oturumun en büyük tek kazancı

`Interpreter::isBreakpoint()` **her talimatta** çağrılıyordu ve son satırı:

```cpp
return breakpoints_.count({file, ins.sourceLine}) > 0;
```

`{file, ins.sourceLine}` geçici bir `std::pair` kurup **dosya yolu string'ini
kopyalıyordu** — `breakpoints_` kümesi boş olsa bile, yani normal `saqut run`
sırasında debugger hiç bağlı değilken.

Kaynak, tahsis anında yığın izi alınarak bulundu (`backtrace` + `addr2line`);
tahsis boyut histogramı önce 96-111 bayt bandını işaret etti (%95.6).

Düzeltme tek satır — `if (breakpoints_.empty()) return false;`

Ölçüm (20.000 turluk stres programı, dönüşümlü 3 tur):
```
                eski            yeni           kazanç
süre            ~4.97 s         ~2.66 s        1.87x
tahsis          153.193.383     18.783.742     8.2x az
bellek trafiği  27.2 GB         11.2 GB        2.4x az
```

**134 milyon tahsis** ortadan kalktı. Boş bir `while` döngüsünde bile
iterasyon başına 5 tahsis vardı; bunun ~%92'si buradandı.

Doğrulama: ctest 243/243, DAP breakpoint testleri (10 adet) dahil geçiyor.

### Uygulananların ayrıntısı

**Parser token önbelleği.** `getToken()` her çağrıldığında `parseToken()`
yeniden çalışıyordu: `std::string` kopyası + zincirleme string karşılaştırması
+ hash araması. Ama `parseToken()` saf bir fonksiyon — aynı `Token*` için hep
aynı sonucu verir. Ölçüm: 1.059.635 token için **6.170.054 `parseToken`
çağrısı** (token başına 5.8 kat). Sonuç indeks başına bir kez hesaplanıp
saklanıyor.

**`SourceLocation` → `fileId`.** `SourceLocation` her token'da ve her AST
düğümünde kopyalanıyor; `filePath` bir `std::string` olduğu sürece her token
dosya yolunun tam kopyasını taşıyordu. Yol 15 karakteri (libstdc++ SSO sınırı)
aşınca token başına ayrı heap tahsisi demekti:

```
yol  5 karakter : 4.474.725 tahsis /  801 MB
yol 40 karakter : 9.504.058 tahsis / 1007 MB   ← 5M tahsis farkı SADECE dosya adı
```

`FileRegistry` ile yol bir kez saklanıyor, `SourceLocation` küçük bir int
taşıyor. Sonrası: yol uzunluğunun etkisi **tamamen** kalktı (5 ve 40 karakter
arasında 11 tahsis fark).

---

## Bölüm 1 — Ölçülmüş, HENÜZ UYGULANMAMIŞ

Getiriye göre sıralı.

### F1. `Instruction::sourceFile` → `fileId`  ★ en yüksek getiri/risk oranı

`Instruction` her talimatta `std::string sourceFile` taşıyor. Bu,
`SourceLocation`'da düzeltilen hatanın **birebir aynısı**, farklı katmanda.

```
Instruction boyutu : 232 bayt
talimat sayısı     : 371.915  (large.sqt)
```

`sourceFile` yalnızca **DAP breakpoint eşleştirmesinde** okunuyor
(`vm/interpreter.cpp:130,195,212` → `dap_handler.cpp`). Yani debugger işi,
ama her talimatta string olarak saklanıyor.

Yapılacak: `int sourceFileId` + `FileRegistry`. Okuma noktaları
`FileRegistry::instance().path(id)` ile beslenir.

Beklenen: talimat başına 32 bayt; uzun yolda 371.915 tahsis eksilir.
**Risk düşük** — `SourceLocation`'da aynısı yapıldı ve 243 test geçti.

### F2. `Token::type` → `enum class TokenKind`

`Token` sınıfında `std::string type` var, her constructor'da `type = "operator"`
gibi sabit etiket atanıyor. Token başına **32 bayt**, yalnızca 6 değerden
birini tutmak için.

Ayrıca `gettype()` **değer döndürüyor** (`std::string gettype() { return type; }`)
— her çağrı bir kopya. `parseToken` bunu string karşılaştırma zinciriyle
çözüyor; `enum` olsa `switch` olurdu.

Token boyutları (fileId sonrası):
```
Token=96  IdentifierToken=136  NumberToken=104  StringToken=136
OperatorToken=96  KeywordToken=96  DelimiterToken=96
```
Bir `;` token'ı 96 bayt tutuyor.

**Ürün sahibi onayladı.** Yüzey `Token` sınıfı + `parseToken` + 13 `gettype()`
çağrısı.

### F3. Tokenizer'da satır/sütun hesabını ERTELE

`SourceFile::offsetToLocation` her çağrıda `lineStarts` üzerinde
`std::upper_bound` çalıştırıyor. Tokenizer bunu **her token için** çağırıyor.

Ölçüm — fonksiyonu tamamen stub'layıp tokenize ettim:
```
mevcut                    112.8 ms
offsetToLocation stub'lı   84.1 ms     → tokenize'ın ~%25'i bu fonksiyonda
```

Büyük dosyada `lineStarts` ~100 bin eleman; her arama ~17 adım rastgele bellek
erişimi (önbellek ıskası).

**Ürün sahibinin kararı:** satır/sütun tokenizer ve parser aşamasında
gerekmiyor; sadece tanı (hata/uyarı), LSP ve DAP üretiminde lazım. Token
yalnızca `offset` taşısın, satır/sütun sonradan hesaplansın.

**DİKKAT — bu maddede bir düzeltme var:** VM satır bilgisini **çalışma
zamanında** kullanıyor: `E_DIVZERO` gibi runtime hataları, breakpoint
eşleşmesi, `stepLine`. Yani "sadece tanı ve LSP" tam doğru değil. İyi haber:
VM `Instruction.sourceLine`'ı okuyor, `SourceLocation`'ı değil — yani
tokenizer'da erteleme yapılabilir, IR'ye gömülürken hesaplanır.

**İkinci dikkat:** `rejectPosition` geriye atlıyor. "Hep ileri" varsayan
artımlı imleç körlemesine yazılamaz; geri sarma da desteklenmeli.

### F4. Token arena'sı  ★ en büyük kazanç, en geniş yüzey

Her token ayrı `new` ile tahsis ediliyor, `std::vector<Token*>` içinde
saklanıyor. Tarama ve serbest bırakmayı ayrı ölçtüm:

```
scan   = 128.8 ms
delete =  43.7 ms     ← toplam sürenin ~%25'i
```

**Sadece token'ları serbest bırakmak toplam sürenin dörtte biri.**

Arena = büyük bloklar (örn. 1 MB) ayır, token'ları içine sırayla yerleştir.
Tahsis "işaretçiyi ilerlet" kadar ucuz; sonda bloklar toptan bırakılır —
1 milyon `delete` yerine birkaç `free`. Önbellek yerelliği de düzelir.

**Daha hızlısı var ama sözleşme kırar:** token nesnesi hiç üretmemek —
`{kind, start, end}` üçlüsü düz dizide (12 bayt), metin gerektiğinde
kaynaktan okunur. `Token*` sözleşmesini kırar (parser, LSP, DAP, CLI).
Önce arena, gerekirse sonra bu.

**Kapsam:** `std::vector<Token*>` sözleşmesine ve tüm `delete` çağrılarına
dokunur. Mekanik değil — muhtemelen ADR gerektirir.

### F5. `Value` → `std::string` alanını ayır  ★ VM'in en sıcak yapısı

`Value` **80 bayt**, bunun **32'si `std::string stringValue`** — tipi `Int`
olsa bile. `Value` VM'in sıcak döngüsünde saniyede milyonlarca kez
kopyalanıyor (`frame.slots[dest] = Value::fromInt(...)`), her kopya bir
`std::string` kurucu/yıkıcı/kopyalayıcı çağrısı demek.

```
Value = 80 bayt = kind4 + int4 + double8 + decimal16 + STRING32 + ptr8 + ll8
```

Saf maliyetini izole ölçtüm (aynı 72 baytlık yapı, biri string'li biri
string'siz — slot yazımı + frame kurulumu deseniyle):
```
std::string ICEREN      13.3 ms
std::string ICERMEYEN    9.0 ms     → 1.48x
```

Boyut eşitken bile 1.48x; `Value` 80→48 bayta inerse fark büyür.

Çözüm: string'i `Object*` üzerinden (heap'te) tutmak — `ref` alanı zaten var.
Böylece `Value` bir POD'a yaklaşır, `memcpy` ile kopyalanabilir hale gelir.

**Kapsam uyarısı:** string yaşam döngüsü GC'ye taşınır. `Value::fromString`
ve tüm string opcode'ları etkilenir. ADR gerektirir.

---

## Bölüm 2 — Ölçek davranışı (15 MB endişesi)

Kullanıcının sorusu: "ilerde 10-15 MB derlerken acı çekmeyelim."

### Zaman: DOĞRUSAL, sorun değil

```
girdi    tokenize      verim
2.3 MB   117 ms        20.0 MB/s
4.6 MB   235 ms        19.9 MB/s
9.4 MB   506 ms        18.5 MB/s
```

Tam pipeline de doğrusal: 2.3 MB → 393 ms, 4.6 MB → 811 ms (2.06 kat).
**Süper-lineer patlama yok.** 15 MB öngörüsü: ~2.6 saniye derleme — kabul
edilebilir.

### Bellek: DOĞRUSAL ama katsayı çok yüksek ★ ASIL TEHLİKE

```
kaynak    tepe bellek    oran
2.3 MB    296 MB         ~130x
4.5 MB    588 MB         ~130x
9.0 MB    1174 MB        ~130x
15 MB     ~1.9 GB (öngörü)
```

**15 MB'lık dosyada ~2 GB RAM.** CI konteynerinde veya kısıtlı ortamda bu
ölür. Acı hızda değil, **bellekte** olacak.

### Aşama aşama bellek — nerede tutuluyor

```
tokens : 145 MB      tokenizer'ın kendisi
ast    : 312 MB      parser +167 MB   ← EN BÜYÜK TÜKETİCİ
check  : 296 MB
ir     : 409 MB      IR +97 MB
```

**Kritik gözlem:** tokenizer toplam belleğin sadece üçte biri. F1–F4'ün
hepsi tokenizer ve IR'yi düzeltir, **AST'ye dokunmaz**. 15 MB hedefinde asıl
duvar parser/AST olabilir — bu dosyanın yazıldığı an itibarıyla AST bellek
profili **henüz incelenmedi**.

---

## Bölüm 2.5 — AST / Parser bellek profili (ÖLÇÜLDÜ)

`examples/large.sqt` → **579.770 AST düğümü**.

### Düğüm boyutları

```
ASTNode (taban)       =  64 bayt     ← makul (vtable+kind+parent+loc+children)
ExpressionNode        = 208 bayt     ← taban + Type(136) + bool
StatementNode         =  72 bayt
LiteralNode           = 248 bayt     ← `42` yazmak için
IdentifierNode        = 240 bayt
BinaryExpressionNode  = 224 bayt
VariableDeclNode      = 152 bayt
FunctionDeclNode      = 160 bayt
BlockNode             =  72 bayt
```

### Kök neden: `Type` = 136 bayt, her ifade düğümünde

`ExpressionNode::resolvedType` bir `Type` **değeri** taşıyor. `Type` içinde:

```
std::shared_ptr<Type> elementType;   16 bayt  (atomik sayaç!)
std::shared_ptr<Type> returnType;    16 bayt  (atomik sayaç!)
std::vector<Type>     paramTypes;    24 bayt
std::string           structName;    32 bayt
std::string           enumName;      32 bayt
+ kind, prim, nullable               16 bayt
```

**`int` tipi için bunların HEPSİ boş**, ama 136 bayt yer tutuyor ve
kurucu/yıkıcı her düğümde çalışıyor. Bu, `Value`'daki `std::string` hatasının
(bkz. F5) ve `SourceLocation::filePath` hatasının AST'deki eşdeğeri —
**aynı hata sınıfının üçüncü örneği**.

Saf maliyeti ölçtüm (579.770 düğüm kurup yıkma, gerçek düğüm sayısı):
```
Type ICEREN dugum      sizeof=168   42.1 ms
Type ICERMEYEN dugum   sizeof= 32   15.0 ms
                                    → 2.81x, 27 ms fazladan
```

Kaba bellek payı: 580K × 136 ≈ **79 MB** yalnızca `Type` alanları için —
ölçülen +167 MB'lık AST artışının yaklaşık yarısı.

### F6. `Type` → tip havuzu (interning)  ★ AST'nin en büyük kalemi

Tipler değişmez (immutable) ve **az sayıda ayrık değer** alır: `int`,
`string`, `bool`, `int[]`, `Point`... Her düğümde 136 baytlık bir kopya
tutmak yerine havuzda bir kez saklanıp düğümde bir `int typeId` taşınabilir —
`SourceLocation::fileId` ile birebir aynı desen.

Beklenen: `ExpressionNode` 208 → ~80 bayt; AST belleğinde ~79 MB azalma;
düğüm kurma/yıkma 2.8x ucuzlama; `shared_ptr` atomik sayaç trafiği sıfırlanır.

**Kapsam uyarısı:** `Type` semantik katmanın her yerinde değer olarak
kullanılıyor (`equals`, `toString`, `toJson`, factory'ler). Havuz eklemek
mekanik değil; `Type`'ın değer semantiği korunarak yapılmalı. Muhtemelen ADR.

---

## Bölüm 3 — İNCELENMEMİŞ alanlar (dürüstlük bölümü)

Bunlar hakkında ölçümüm yok; bu dosyada rakam vermem yanıltıcı olur.

- **`symbol` katmanı** — sembol tablosu string anahtarlı hash map'lerle
  dolu; kopya maliyeti ölçülmedi.
- **`type_checker`** — 76 ms, aşamalar arasında üçüncü. Neyin pahalı olduğu
  ayrıştırılmadı (ama F6 buraya da dokunur).
- **`ir-gen`** — 110 ms, breakpoint düzeltmesinden sonra en pahalı aşama.
- **JIT sıcak döngüsü** — VM ölçüldü, JIT ölçülmedi.
- **AST'nin kalan ~88 MB'ı** — `Type` ~79 MB'ı açıklıyor; geri kalanın
  düğüm başına `new` mi yoksa başka alanlar mı olduğu ayrıştırılmadı.

---

## Bölüm 4 — Backtracking felsefesi hakkında (soru cevabı)

Ürün sahibi sordu: "hız odaklı daha temiz bir lexer var mı, felsefeyi
değiştirmeli miyiz, gerekirse baştan yazalım mı?"

**Cevap: hayır, felsefe suçlu değil.** `begin/accept/reject` üçlüsünün saf
maliyetini izole ölçtüm (gerçek konum yönetimini birebir taklit eden, tarama
yapmayan bir mikro-benchmark, 1.059.635 token + %12 geri dönüş oranı):

```
backtracking makinesinin TAMAMI : 2.4 ms
tokenize toplamı                : ~107 ms
                                  → %2.2
```

Ek kanıt: `acceptPosition` sızıntısı düzeltildiğinde yığın 8481'den 1'e indi
ve **hız hiç değişmedi**. Backtracking makinesi fiilen bedava çalışıyor.

Maliyetin tamamı token *nesnesinin nasıl saklandığıyla* ilgili, token'ın
*nasıl tanındığıyla* değil. F1–F4'ün hiçbiri `begin/accept/reject`
üçlüsüne dokunmaz; iç içe XML/JSON/CSS çözme yeteneği hiç etkilenmez.

**Yeniden yazma önerilmiyor:** getirisi %2.2 ile sınırlı, riski büyük
(backtracking mantığı doğru çalışıyor ve 243 test onu koruyor).

---

## Bölüm 5 — Ölçüm notları (tekrarlanabilirlik)

- Süre + token sayısı: `saqut bench <dosya>`
- Tahsis sayısı: global `operator new`/`delete` override'ı ile sayaç
  (bu dosyadaki tüm tahsis rakamları böyle alındı)
- Tepe bellek: `/proc/<pid>/status` → `VmHWM` yoklaması
- `perf` ve `valgrind` bu makinede **kurulu değil** — profil ayırma
  yöntemi "parçayı stub'la, farkı ölç" oldu

**TUZAK:** `tests/run.sh` `build/saqut` kullanır; `cmake --build build-rel`
ise `build-rel/saqut` üretir. İkisi ayrı derlenmezse yanlış binary ölçülür.
Bu oturumda bir kez bu tuzağa düşüldü ve "test kırıldı" sanıldı — aslında
eski binary çalışıyordu.

**İKİNCİ TUZAK:** bu makine ölçümler arası ~%10 dalgalanıyor. Tek ölçümle
karar verilmemeli; eski/yeni binary'ler aynı oturumda dönüşümlü koşulmalı.

---

## İlgili issue'lar

- **#220** — Tokenizer bellek modeli (F2, F3, F4 burada; `Performans`+`hata`)
- **#219** — Sessiz kaçış taraması (A1/A2 kapatıldı, kalanı açık)
