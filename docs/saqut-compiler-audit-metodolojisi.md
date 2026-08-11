# saQut Compiler Audit — Yeniden Kullanılabilir Metodoloji Raporu

> Bu rapor, saQut derleyicisi üzerinde yapılan incelemelerden çıkarılan, modelden bağımsız bir "compiler audit metodolojisi"dir. Her iddia dosya/sembol/test/komut referansıyla ilişkilendirilmiştir. Doğrulanmamış iddialar "muhtemelen" / "bilinmiyor" olarak işaretlenmiştir.

## 1. Yönetici Özeti

Üç farklı bug sınıfı incelendi ve hepsi **aynı kök aileye** bağlandı: *pipeline boyunca tip/temsil bilgisinin kaybı ve katman sınırı ihlalleri*.

| Bug | Sınıf | Kök neden | Doğrulama |
|---|---|---|---|
| #199 `0x80`→0 | Literal-base uyumsuzluğu | `std::stoll(token)` base 10; AST'de var olan `literalBase` tüketilmiyor | smoke: `0x80→128, 0x800+0x800→4096` |
| #168 `int[]` dönüş tipi | Katman-asimetrisi (parametre/var destekli, dönüş destek değil) | `parseFunctionDecl` return-type `[]` döngüsü yok + 3 dispatch sitesi `[]` tanımıyor | smoke: `int[] makeArr()` çalıştı |
| #206 byte[] churn super-lineer | Temsil şişirmesi + O(N²) GC re-scan | Tüm array'ler `vector<Value>` (80B/byte); element tipi IR'de siliniyor; `markChildren` her toplamada O(N) | `sizeof(Value)=80` ölçüldü; koddan markChildren/safepoint doğrulandı |

Ortak desen: **bilgi upstream'da hesaplanıp (lexer base, semantic element tipi, AST array return tipi) bir sonraki katmanda siliniyor veya yeniden keşfediliyor**. Ayrıca debug katmanı (DAP) runtime temsilini kirletmiş (`StructObject::fieldNames` her örnekte `vector<string>`).

Çıktı: özellik matrisi, test-boşluk matrisi, veri-temsili/GC analizi (somut bayt hesaplarıyla), profiling planı, 10-fazlı denetim protokolü, itiraz protokolü, ve başka LLM'lere verilecek master prompt.

## 2. Problemi Nasıl Anladım

**Yöntem — görev-yeniden-ifade (Faz 0):** Kullanıcının açık sözüyle dolaylı hedefi ayrıldı.

- Açık: "0x80 IR/VM'de 0 oluyor", "int[] dönüş tipi atlanmış", "float/double ikisi de 64-bit", "AES S-box matematik hatalı".
- Dolaylı hedef: bu tekil görünen bug'ların arkasında **sistematik bir uyumluluk/temsil problemi** var mı? Kullanıcı "testler işe yaramıyor veya her şeyi kapsamıyor" dedi — bu, test sınırlarını da incelemeyi gerektiriyordu.

**Tekil→sistematik çıkarsama kriteri:** Bir bug bulunduğunda, aynı bilginin **başka katmanlarda da** kaybolup kaybolmadığına bakıldı. `0x80` hatasında base bilgisinin lexer'da var olduğu (`lexer.cpp:231 num.base=16`) ama 5 downstream sitede (`stoll` base 10) tüketilmediği görülünce, "tek bir unutkanlık" değil "pipeline boyunca bilginin akışının kopuk" sonucuna varıldı. Bu, #168 (return tipi `[]`'si parametrede var dönüşte yok) ve #206 (element tipi semantic'te var IR/VM'de silik) ile **aynı örüntü** oldu — üçü de "bilgi bir katmanda var, diğerinde yok".

**Hipotez tablosu formatı (her önemli çıkarım):**

| # | Gözlem | Kaynak | Teknik yorum | Alternatif açıklama | Doğrulama | Güven |
|---|---|---|---|---|---|---|
| H1 | `0x80`→0 | `ir_generator.cpp:655 std::stoll(token)` base 10 | `stoll("0x80")` "0"da durur | Lexer base'i yanlış saklıyor olabilir | `lexer.cpp:231-233 num.base=16` + `parser.cpp:469 lit->literalBase=nt->base` okundu → base doğru, tüketim yanlış | Yüksek |
| H2 | `int[] f()` parse hatası | `parser.cpp:652 parseFunctionDecl` return `[]` döngüsü yok | Parametrede `[]` var (`:680`), ffi'de var (`:722`), dönüşte yok | Semantic/IR dönüş tipini reddediyor olabilir | Dispatch (`:291-306`) `int[]`'i değişken decl sanıyor → parse katmanı | Yüksek |
| H3 | byte[] super-lineer | `sizeof(Value)=80`, `object.cpp:68 markChildren` O(N) | 80× şişme + her toplamada O(N) mark | Sadece 80× sabit (lineer) olabilir; GC dışı neden olabilir | #206 verisi 512KB+ sapma gösteriyor → sabit değil; O(N²) hipotezi | Orta→Yüksek |

**Hâlâ varsayım kalanlar:** signed `>>`'nin uint32 semantiği için güvenli olup olmadığı (xorshift128 şansla geçmiş olabilir — §15). GC'nin struct[]/string[] churn'de hâlâ O(N²) olup olmadığı (Fix A sonrası ölçülmedi).

## 3. Repo'yu Nasıl Haritaladım

**Anchor-point yöntemi:** Tüm dosyaları okumak yerine, her katmanın **giriş noktasını** ve **veri yapısını** çapalladım.

| Katman | Anchor dosya | Ne öğrenildi |
|---|---|---|
| Tip sistemi | `src/core/type.hpp` | `PrimitiveKind` enum, `Type::array(elem)`, `fromName("int[]")` — element tipi burada YAŞAR |
| Lexer | `src/lexer/lexer.cpp:208 readNumeric` | `INumber.base` doğru saklanıyor |
| Parser | `src/parser/parser.cpp` dispatch (240-340) + `parseFunctionDecl` (652) | Tip→decl yönlendirme mantığı |
| AST literal | `src/parser/nodes/literal.hpp` | `literalBase` alanı VAR ama unused downstream |
| Semantic | `src/semantic/type_checker.cpp:551` | Literal tiplendirme, byte aralık |
| IR | `src/ir/ir_generator.cpp:1604 slotTypeFromTypeName` | **Element tipi burada SİLİNİYOR** (array→Ref) |
| VM temsil | `src/vm/value.hpp`, `object.hpp` | `Value`=80B fat union; `ArrayObject=vector<Value>` |
| GC | `src/vm/object.cpp`, `interpreter.cpp:297 maybeCollect` | Safepoint + mark/sweep |
| Test | `tests/golden/*`, `tests/run.sh` | 29 alt klasör; golden = stdout black-box |

**Bağımlılık yönü çıkarımı:** include grafı okundu: `core/` ← (her şey), `tokenizer` ← `parser` ← `semantic`/`symbol` ← `ir` ← `vm` ← `cli`. **İhlal:** `dap/` → `vm/*` iç structları (§14). `lsp/` → statik katmanlar (temiz).

## 4. Arama ve İnceleme Sırası

**Sıra (tekrarlanabilir protokol):**

1. **Sembol grep'i (yatay):** Hedef sembolü tüm katmanlarda ara. `0x80` için: `stoi|stol|stoul` → 39 eşleşme → 5'i literal dönüşümü, diğerleri runtime cast (ayırt edildi).
2. **Anchor okuma (dikey):** İlgili dosyayı tam oku. `type.hpp` tam okundu (350 satır) — tip sisteminin merkezini tek seferde kavradı.
3. **İnclude grafiği:** Bir dosyayı ilgili sayma kriteri = "hedef sembolü veya hedef veri yapısını tanımlıyor mu". `dap/frame_reader.hpp`'teki `stoi` ilgisiz (header parse) → elendi.
4. **Test karşılaştırması:** Özelliğin golden test'i var mı? hex literal → **yok** (gap tespiti).
5. **Git考古:** `git log -- <file>` ile temsilin ne zaman değiştiği. ADR-040 float32 = `b73c21c`/`1ceaaa9`; print newline kaldırma = `9ac66d5`.

**İlgisiz sayma kriterleri:** vendor kodu (`mir/vendor/`, `nlohmann/json.hpp`) — bunlardaki `stoi`/`strtoul` saQut'un değil. Build/sandbox kodu. Bu ayrım yapılmazsa 39 eşleşmeden 20'si vendor gürültüsü olurdu.

## 5. Önemli Dosyalar ve Neden Önemli

| Dosya | Neden kritik |
|---|---|
| `src/core/type.hpp` | Tip sisteminin tek doğrusu; `fromName`, `array()`, `equals` — tüm katmanlar burayı referans alır |
| `src/parser/nodes/literal.hpp` | `literalBase` burada; düzeltmenin merkezi |
| `src/ir/ir_generator.cpp:1604` | **Tip-erasür sınırı** — array element tipi burada kaybolur |
| `src/vm/value.hpp` | 80B fat-union; tüm runtime maliyetin kaynağı |
| `src/vm/object.cpp:68` | `markChildren` — GC traversali maliyeti |
| `src/vm/interpreter.cpp:383` | GC safepoint — kök küme tanımı |
| `src/dap/dap_handler.hpp:12-14` | Katman ihlali kanıtı |
| `tests/golden/numeric/widths.sqt` | Tip genişlikleri testi — ama hex/oktal/binary İÇERMİYOR |

## 6. Kurulan Hipotezler

(§2 tablosu + aşağıdaki ek hipotezler)

- **H4 (profiling):** 512KB+ super-lineer = O(N²) GC re-scan (büyük canlı array × toplama sayısı). Alternatif: 80× sabit (çürütüldü — sabit olsa 4KB'de de sapar).
- **H5 (backward-compat):** `print()` newline kaldırma (`9ac66d5`) eski golden'ları etkiledi. Doğrulama: smoke çıktımda newline yoktu — tutarlı.
- **H6 (DAP kirliliği):** `fieldNames` runtime'a DAP için sızmış. Doğrulama: `object.hpp:66` yorumu "toJson/dump için", tüketici `dap_handler.cpp:91,676`.

## 7. Elenen Hipotezler

- **"Lexer base'i yanlış saklıyor"** — elendi: `lexer.cpp:231-259` doğru.
- **"GC use-after-free (array doldururken canlı veri toplanıyor)"** — elendi: safepoint tasarımı sağlam (§12.12).
- **"#77 GC hiç toplamıyor hâlâ açık"** — elendi: `maybeCollect` (interpreter.cpp:297) var, `#77` CLOSED.
- **"byte cast truncate etmeli (DeepSeek şüphesi)"** — design kararı olarak bırakıldı: `rt_jit_int_to_byte_checked` range-check kasıtlı (ADR'lerde).

## 8. Doğrulanan Problemler (Örnek Vakalar)

### Vaka A — Literal parsing (`0x80`→0) [§5]

**Hata katmanları:** Lexer ✓, Parser ✓, **IR/semantic/constant-folding ✗**. Dönüşüm `std::stoll`'de.

**API karşılaştırması:**

| Fonksiyon | `"0x80"` sonucu | base parametresi | Not |
|---|---|---|---|
| `std::stoll(s)` (default base 10) | **0** (x'te durur) | 10 (örtük) | #199'un nedeni |
| `std::stoll(s, nullptr, 16)` | 128 | explicit 16 | 0x önekini kabul eder |
| `std::stoll(s, nullptr, 2)` | **0** (0b kabul ETMEZ) | 2 | binary için prefix soyunmalı |
| `std::strtol(s, nullptr, 0)` | 128 | auto-detect | 0x/0 tanır, **0b tanımaz** |
| `std::from_chars(s, base)` | 128 | explicit | en güvenli, hata fırlatmaz |

**Prefix politikası:** Lexer prefix'i token'da koruyor (`"0x80"`). İki seçenek: (a) downstream prefix'i soyup base ver (benimsenen), (b) lexer prefix'i soyup sadece rakakları sakla. (a) daha güvenli — token inspectability (AST dump) korunur.

**Etkilenen gösterimler:** hex ✓ (0x80→0), **binary kesin** (0b1010→0, çünkü 'b' base-10'da durur), **octal kısmen** (0777→777, çünkü stoll leading 0'yı atlar — yanlış değer ama 0 değil). Yani #199 hex+binary'yi bozuyor, octal'ı sessizçe yanlış yapıyor.

**Zorunlu regression test'leri (düzeltme sonrası):** her taban × {min, max, 0, sınır ötesi}; `0x0,0x7FFFFFFF,0x80000000,0xFFFFFFFF`; `0b0,0b1,0b11111111`; `0777,0`; karışık `0x800+0x800`; negatif bağlam `-0x80`.

**Genelleme — parser audit metodu:** "Bir literal tipi lexer'da tanınıyorsa, **her dönüşüm noktasında** base/format bilgisinin aktığı doğrulanmalı." Bu, string escape, char literal, scientific notation, decimal exponent için aynı protokole uyarlanır.

### Vaka B — Array dönüş tipi asimetrisi [§6]

**Asimetri tespit yöntemi — "yatay dilim":** Aynı özelliğin (`T[]` tipi) tüm kullanım bağlamlarında destekini karşılaştır:

| Bağlam | Destek | Kanıt |
|---|---|---|
| Değişken tanımı `int[] x=[1,2,3]` | ✓ | `tests/golden/array/ref_semantics.sqt` |
| Parametre `void f(int[] a)` | ✓ | aynı dosya satır 1 |
| **Fonksiyon dönüş `int[] f()`** | ✗ (düzeltildi) | `parseFunctionDecl:652` `[]` döngüsü yok |
| ffi dönüş `int[] f()` | ✓ | `parseFfiDecl:722` `[]` döngüsü var |
| Struct alan `int[]` | (muhtemelen ✓, doğrulanmadı) | — |
| Array elemanı `int[][]` | (kısmen) | dispatch `[]` çok-boyut tanıyor |

**Denetim listesi (bir tip bir bağlamda destekliyse):** type parser ✓ → return type parser ✗ → function signature ✓ → semantic return validation (?) → return opcode ✓ (Ref taşır) → calling convention ✓ → stack frame ✓ → **ownership/GC root** (RETURN'da Ref C++ lokalinde kısa süre — ama safepoint değil, güvenli) → value copy ✓ → native boundary (?) → test coverage ✗.

**Genel ders — "destek varsayımı" tehlikesi:** "parametre destekliysa dönüş de destekli varsay" yanlış; çünkü parser **iki ayrı kod yolu** (parametre döngüsü vs dönüş döngüsü) ve **üç ayrı dispatch** kullanır. Bir özelliğin her bağlamı **bağımsız kanıtlanmalı**. Bu, `T?` nullable, `T[][]` çok-boyut, enum dönüş, struct dönüş için aynı şekilde denetlenmeli.

### Vaka C — byte[] ve GC performans [§7]

**Mantıksal vs fiziksel boyut** (`sizeof(Value)=80` ölçüldü):

| Mantıksal | Fiziksel (`vector<Value>`) | GC mark işi/eleman |
|---:|---:|---|
| 100 byte | ~8 KB + 48B header | 100 eleman tara |
| 1 KB | ~80 KB | 1024 |
| 1 MB | **~80 MB** | 1 048 576 |
| 100 MB | **~8 GB** | 104 857 600 |

**Maliyet ayrıştırması:** padding (Value 80B ama int 4B → 76B boş); tagged-union (DecimalValue 16B + string 32B her Value'de gömülü, byte için kullanılmıyor); heap allocation (vector buffer); **cache locality** (80B stride → L1 miss); **mark traversal** (`markChildren` N eleman, hepsi Int → boş döngü); sweep (O(allocCount)); **write barrier yok** (gerekli de değil — stop-the-world).

**Tasarım karşılaştırması:**

| Tasarım | Doğruluk | Performans | GC karmaşıklığı | Uygulama maliyeti | Semantiğe etki | Backward-compat | Native uyum | Debug kolaylığı |
|---|---|---|---|---|---|---|---|---|
| `vector<Value>` (mevcut) | ✓ | ✗ 80× | düşük | — | — | — | düşük | yüksek |
| `vector<uint8_t>` (byte-only) | ✓ | ✓✓ | düşük | düşük | byte[] sınırlı | kırılabilir | yüksek | orta |
| `ByteArrayObject` ayrı tip | ✓ | ✓✓ | düşük | orta | byte[] özel | güvenli | yüksek | orta |
| **Typed array (elemType etiketi)** | ✓ | ✓✓ | orta | **orta-yüksek** | genel | güvenli | yüksek | orta |
| Small-buffer optimization | ✓ | ✓ (küçük) | düşük | orta | sınırlı | güvenli | düşük | yüksek |
| External/native buffer | ✓ | ✓✓ | yüksek (rooting) | yüksek | ownership karmaşık | kırılabilir | çok yüksek | düşük |
| Copy-on-write | ✗ (mutability?) | ✓ (az kopya) | orta | yüksek | semantik değişir | kırılabilir | düşük | düşük |
| Immutable byte string | ✗ (mutable değil) | ✓ | düşük | orta | `byte[]` mutable değil | kırılabilir | orta | yüksek |
| GC-taranmayan atomic buffer | ✓ | ✓✓ | **düşük** | orta | rooting dikkat | güvenli | yüksek | orta |

**Önerilen:** Typed array (elemType etiketi) + packed storage primitive tipler için — genel, backward-compat'i korunabilir (byte[] hala byte[]), GC primitive array'de O(1). Bu, §16 kısa-vadeli işin merkezidir.

> **Öğretici kutu — Tagged union / NaN-boxing / Tagged pointers:** `Value` bir **tagged union** (discriminator `ValueKind` + tüm alanlar gömülü). Bu basit ama maliyetli: her değer 80B. Alternatif: **NaN-boxing** (double'daki NaN uzayına pointer/int gömme, 8B) veya **tagged pointers** (işaretçinin düşük bitlerinde tip etiketi). V8/SpiderMonkey NaN-boxing kullanır. saQut basitlik seçmiş — küçük programlarda iyi, byte[] gibi bulk veride patlar. **Anahtar kelimeler:** NaN-boxing, tagged pointer, fat pointer, boxing/unboxing.

> **Öğretici kutu — Tricolor marking / Precise vs conservative GC / Safepoints:** saQut **tricolor mark-sweep** (beyaz→gri→siyah, `marked` biti ile iki renge indirgenmiş), **precise** (sadece slot'ları tara, C++ stack'i taramaz — `markValue` sadece `Ref`), **safepoint-tabanlı** (döngü başında `maybeCollect`). Precise GC'nin zorunluluğu: **tüm canlı Ref'ler safepoint'te bir slotta olmalı** — C++ lokalinde tek Ref varsa ve GC koşarsa use-after-free. saQut bu kuralı "instruction içinde GC yok" ile sağlıyor (sağlam). **Anahtar kelimeler:** tricolor marking, precise GC, conservative GC, safepoint, root set, mutator.

## 9. Kullanılan Test ve Doğrulama Yöntemleri

| Yöntem | Ne için | Sonuç |
|---|---|---|
| `/tmp` smoke programı + `./build/saqut run` | Düzeltme doğrulama | `0x80→128`, `int[] makeArr()`→çalıştı |
| `g++ -Isrc /tmp/sizeof_check.cpp` | `sizeof(Value)` koddan | **80** (somut) |
| `cmake --build build` | Düzeltme derleme | temiz, 2 mevcut uyarı |
| `git log -- <file>` | Temsil değişiklik tarihi | ADR-040, print-newline |
| `grep -rln` golden testlerde | Kapsam denetimi | hex/oktal/binary literal **yok** |
| `gh issue view` | Issue triage durumu | #77 CLOSED, #206 TRIAGE |

**Başarısız/beklenmedik sonuçlar:** Smoke çıktısında newline yoktu — araştırınca `print()` newline'ı `9ac66d5` ile bırakmış (backward-compat kırılması, §14). clangd "undeclared identifier" LSP hatası — PCH bayatlığı false-positive, gerçek build temiz (doğrulandı).

## 10. Feature Support Matrix

Satır = tip, Sütun = bağlam. Durum kodu: **✓A**=açıkça destek, **✓T**=testle doğrulanmış, **K**=kodda var test yok, **P**=kısmen, **!**=tutarsız, **✗**=destek yok, **?**=bilinmiyor.

| Tip | Tanım | Atama | Parametre | Dönüş | Array elem | Struct alan | Karşılaştırma | Aritmetik | Cast | Serialization | GC scan | Native | Const-fold |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| int (32b) | ✓T | ✓T | ✓T | ✓T | ✓T | ✓T | ✓T | ✓T | ✓T | ✓T | n/a (değer) | ✓T | ✓T |
| longint (64b) | ✓T | ✓T | ✓T | ✓T | K | K | ✓T | ✓T | ✓T | ✓T | n/a | K | P |
| float (32b) | ✓T | ✓T | ✓T | ✓T | K | K | ✓T | ✓T (F32) | ✓T | ✓T | n/a | K | ✗ |
| double (64b) | ✓T | ✓T | ✓T | ✓T | K | K | ✓T | ✓T | ✓T | ✓T | n/a | K | ✗ |
| byte (0-255) | ✓T | ✓T | ✓T | ✓T | **K** | ✓T(`library`) | ✓T | **!** (int'e terfi) | **!** (range-check) | ✓T | n/a | K | P |
| bool | ✓T | ✓T | ✓T | ✓T | K | K | ✓T | ✗ (mantıksal) | ✓T | ✓T | n/a | K | ✓T |
| string | ✓T | ✓T | ✓T | ✓T | K | K | ✓T | ✓T (birleştirme) | ✓T | ✓T | n/a (inline) | K | ✗ |
| decimal | ✓T | ✓T | ✓T | ✓T | K | K | ✓T | ✓T | ✓T | ✓T | n/a (inline VM) | K | ✗ |
| **Array `T[]`** | ✓T | ✓T | ✓T | **✓T (düzeltildi)** | ? | K | ✓T (ref) | ✗ | P | ✓T | **! O(N) mark** | P | ✗ |
| Nested `T[][]` | K | K | K | ? | ? | ? | ? | ✗ | ? | ? | ! | ? | ✗ |
| Struct | ✓T | ✓T | ✓T | ✓T | K | ✓T | ✓T (ref) | ✗ | ✓T | ✓T | **! fieldNames/örnek** | K | ✗ |
| Enum | ✓T | ✓T | ✓T | ✓T | K | K | ✓T | ✗ | ✓T | ? | n/a (int) | ? | ✗ |
| Nullable `T?` | ✓T | ✓T | ✓T | ✓T | K | K | ✓T | ✗ | ✓T | ? | ? | ? | ✗ |
| date | ✓T | ✓T | ✓T | ✓T | K | K | ✓T (sadece) | ✗ (ADR) | ✓T | ✓T | n/a | K | ✗ |
| Function value | ✗ | ✗ | ✗ | ✗ | ✗ | ✗ | ✗ | ✗ | ✗ | ✗ | n/a | ✗ | ✗ |

**En riskli boşluklar (kalın `!`):**
1. **byte aritmetiği tutarsız** — byte aritmetikte int'e terfi eder, sonuç asla byte olmaz (`type.hpp:141-143`); `as byte` range-check edip hata verir (çoğu dil truncate eder). İki davranış da mantıklı — **dil tasarımı kararı** (§10 itiraz).
2. **Array GC mark O(N)** — primitive array'lerde gereksiz tarama (#206 kök nedeni).
3. **Struct fieldNames örnek kopyası** — debug kirliliği.
4. **Function value yok** — first-class fonksiyon desteklenmiyor (v1 kapsam dışı, ADR-042 ile tutarlı — tasarım kararı).
5. **Nested array çoğu `?`** — doğrulanmamış, riskli.

## 11. Test Coverage Gap Matrix

Her gap için: amaç / hata sınıfı / min örnek / beklenen / katmanlar / mevcut test neden yakalayamadı / öncelik.

| # | Gap | Hata sınıfı | Min örnek | Beklenen | Katmanlar | Neden yakalanamadı | Öncelik |
|---|---|---|---|---|---|---|---|
| G1 | **Hex/oktal/binary literal** | base-mismatch | `print(0x80)` | 128 | lexer→parser→semantic→IR→VM | Golden'larda hiç non-decimal literal yok | P0 |
| G2 | **Array dönüş tipi** | katman-asimetrisi | `int[] f(){return [1];}` | derler | parser→semantic→IR | Sadece parametre/var testi var | P0 |
| G3 | **Octal doğru değer** | sessiz yanlış | `print(0777)` | 511 | IR→VM | G1 ile aynı; 0 değil yanlış değer | P0 |
| G4 | **byte[] bulk churn** | O(N²) GC | 1MB byte[] doldur-boşalt | lineer süre | VM→GC | Sadece correctness, perf yok | P1 |
| G5 | **float vs double precision** | genişlik karışması | `float f=123456789` | 123456792 | VM→value | widths.sqt var ama sınırlı | P1 |
| G6 | **Signed >> uint32 semantiği** | signedness | `print((-1) >> 1)` | ? (aritmetik mi?) | VM | bitwise test var, negatif shift yok | P1 |
| G7 | **print() newline backward-compat** | davranış kırılması | eski program çıktı | satır satır | cli→vm | Golden'lar güncellenmiş, eski sürüm karşılaştırma yok | P2 |
| G8 | **Struct fieldNames maliyet** | debug şişirme | 1M struct alloc | düşük bellek | VM→object | Hiç struct-churn perf testi yok | P2 |
| G9 | **Nested array derinliği** | çok-boyut tutarsızlık | `int[][] m=[[1],[2]]` | derler | parser→IR→VM | Sadece 1-boyut testi | P2 |
| G10 | **GC canlı veri toplamıyor (regresyon)** | use-after-free | büyük array + churn | doğru çıktı | VM→GC | #213 açık, fixture yok | P1 |

**Mevcut testlerin neden kaçırdığı (sistematik nedenler):**
- **Golden = stdout black-box tek katman proxy'si** — katmanlar izole değil, hatalı binary'den üretilmiş expected varsa self-fulfilling.
- **Pozitif-odaklı** — negatif/false-reject/literal-sınır testleri zayıf.
- **Perf testi yok** — sadece correctness; O(N²) ölçülemiyor.
- **Sürüm-arası determinism yok** — ADR-038 öngörüyor ama uygulanmamış.

## 12. Veri Temsili ve GC Analizi (derinlemesine)

**GC doğruluk incelemesi (use-after-free pencere araması) — sağlam:**

- Safepoint: `maybeCollect()` sadece `interpreter.cpp:383` (döngü başı) — instruction içinden çağrılmaz. allocate (`allocArray`) `maybeCollect` çağırmaz.
- `ARRAY_NEW` (`:861`): alloc → hemen `slots[dest]`'e Ref → sonraki safepoint'te köklü.
- `RETURN` (`:650`): `Value returnValue = frame.slots[src]` (C++ lokal) → `pop_back()` → caller slot'a yaz. Arada GC yok (safepoint değil). Sonraki safepoint'te caller slot'undan köklü. **Sağlam.**
- Kök küme: `globalSlots_` + tüm `callStack_` frame slot'ları + `pendingThrow_`. **Precise** — C++ stack taramaz.
- `markChildren` (array/struct) eleman Ref'lerini özyinelemeli marklar. Doğru.

**Sonuç:** GC doğruluk sağlam. **Sorun correctness değil, maliyet.** #206 = temsil şişirmesi + O(N²) mark re-scan.

> **Öğretici kutu — Generational GC / Write barriers / Mark-compact:** saQut **tek-nesil mark-sweep**. Sorun: büyük canlı nesneler her toplamada yeniden taranır. **Generational GC** (genç/yaşlı nesil ayır; minor toplama genç nesli tarar) bu quadratik'i kaldırır — ama **write barrier** gerektirir (yaşıl nesneye genç referans yazılınca remembered-set'e eklen). saQut write barrier yok (stop-the-world olduğu için gerekmez). Alternatif: **mark-compact** (sweep + sıkıştırma, fragmentasyonu çözer). **Anahtar kelimeler:** generational hypothesis, minor/major collection, write barrier, remembered set, card table, mark-compact, fragmentation.

## 13. Performance Profiling Planı

"2× yerine 8× yavaşlama" analizi — hipotez başına ölçüm/araç/doğrulama/çürütme/en-ucuz-deney:

| Hipotez | Ölçüm | Araç | Destekleyen sonuç | Çürütüp sonuç | En ucuz deney |
|---|---|---|---|---|---|
| O(N²) GC re-scan | süre vs N log-log eğri | `--profile --gc-stats` | eğri >slope 1 | slope≈1 (lineer) | `byte[1MB]` canlı tut + temp churn, ölç |
| Allocation patlaması | alloc/s | `--gc-stats freed/runs` | freed∝N²? | freed∝N | tek büyük array, churn yok → hâlâ yavaş mı? |
| Mark traversal hacmi | mark ops/koleksiyon | instrumentation (mark sayacı) | mark∝N/koleksiyon | sabit | `markChildren`'a sayaç ekle (geçici) |
| Cache miss | L1/L2 miss rate | `perf stat -e cache-misses` | miss∝N | sabit | `perf stat` |
| Value boxing | — | kod okuma | her byte Value | — | zaten kanıtlandı (sizeof=80) |
| Reallocation/copy | realloc sayısı | `strace -c brk/mmap` | brk patlaması | sabit | #206 zaten elendi (syscall değil) |
| Interpreter dispatch | dispatch/insn | `--profile vmLoopIter` | yüksek sabit | — | VM vs JIT (JIT ARRAY_NEW desteksiz) |
| Hidden quadratic | — | kod okuma (markChildren O(N)×koleksiyon) | bulundu | — | kanıtlandı |

**saQut'a özel plan:** (1) `--gc-stats` ile `runs`/`freed`/`live` ölç (N=4K→2M); (2) log-log eğri slope hesapla (>1.0 = super-lineer); (3) `markChildren`'a geçici sayaç koy, mark op/eleman ölç; (4) `perf stat -e cache-misses,branches,branch-misses`; (5) hipotez eleme: tek büyük canlı array + sıfır churn → hâlâ yavaşsa mark re-scan kanıtlanır.

## 14. Mimari Riskler

1. **Tip-erasür sınırı (IR→VM):** element tipi `slotTypeFromTypeName:1620`'de siliniyor. Bu, #199 (base), #168 (return tipi), #206 (byte[]) **hepsinin** ailevi kökü. **En yüksek risk.**
2. **DAP↔VM sık bağı (katman ihlali):** `dap_handler.hpp:12-14` `vm/*` iç structları. Value/Object değişikliği DAP'ı kırar; DAP ihtiyaçları runtime'ı basar (`fieldNames`). **Stabil arayüz yok.**
3. **`print()` backward-compat kırılması** (`9ac66d5`): newline kaldırıldı. Eski program/golden çıktıları etkilenir. Sürüm geçişi riski.
4. **Signed `>>` uint32 güvenliği belirsiz:** xorshift128 şansla geçti olabilir (§15). int32 semantiği kullanan her algoritma riskli.
5. **LSP temiz ama DAP kirli** — asimetri; iki debug yüzeyi farklı bağımlılık disiplinine sahip.
6. **`tests/general/` untracked** — crypto testdrive'ları (kanıt) repo otoritesinde değil.

> **İtiraz protokolü (§10) — örnek:** "byte cast truncate etsin" önerisi → **(1)** varsayım: `as byte` sessiz kırpma yapmalı. **(2)** Riskli: sessiz veri kaybı, bug maskeleme. **(3)** Kanıt: `rt_jit_int_to_byte_checked` (`mir_backend.cpp:166`) kasıtlı range-check; `type.hpp:141` byte aritmetikte int'e terfi (C modeli). **(4)** Alternatif: `as byte` checked kalsın, `& 255` açık kırpma kullanıcıya bırakılsın. **(5)** Maliyet: kullanıcı `& 255` yazar (1 karakter fazladan). **(6)** Öneri: mevcut checked davranış korunsun. **(7)** **Karar kullanıcıya ait** — dil tasarımı.

## 15. Bilinmeyenler ve Belirsizlikler

- signed `>>`'nin negatif int'te aritmetik (sign-extend) mi yoksa saQut mantıksal mı yapıyor — **doğrulanmadı**. xorshift128 değerleri doğru çıktı ama "şans" olabilir (ara değer negatif düşmedi). **Test edilmeli:** `print((-1) >> 1)` → aritmetik ise `-1`, mantıksal ise büyük pozitif.
- Struct içinde `int[]` / nested array alanı — **doğrulanmadı**.
- `decimal` array'i — **?**.
- JIT'in struct/string/decimal array desteği — **bilinmiyor** (ARRAY_NEW desteksiz göründü).
- GC'nin struct[]/string[] churn'de hâlâ O(N²) olup olmadığı (Fix A sonrası ölçülmedi).
- `tests/general/crypto/` artifact'larının otorite durumu.

## 16. Önerilen Kısa Vadeli İşler

1. **Non-decimal literal golden test'leri** (G1, G3) — en ucuz, #199 regression koruması.
2. **Array dönüş tipi golden test'i** (G2) — #168 regression.
3. **`(-1) >> 1` shift-semantik testi** (G6) — belirsizliği kapat.
4. **byte[] churn perf ölçüm harness'i** (G4) — #206 kanıtı tracked'a taşı.
5. **`print()` newline geçiş dokümantasyonu** — migration notu.

## 17. Önerilen Uzun Vadeli İşler

1. **Typed array (elemType etiketi + packed storage)** — #206 kökten, tip-erasür sınırını kısmen kapatır.
2. **Tip-içeri-akış kuralı:** `SlotType::Byte` ekle; array'ler `Ref<elemType>` taşısın; AST type→symbol→IR→VM zinciri eksiksiz.
3. **DAP debug-observability boundary:** `IDebugValueView` arayüzü; DAP `vm/*`'a direkt erişmesin.
4. **`fieldNames` → type-metadata** (örnekten tip'e).
5. **Generational GC** (write barrier ile) — struct[]/string[] churn için.
6. **Sürüm-arası determinism differential** (ADR-038).
7. **Perf regression CI** (log-log slope alarmı).

## 18. Yeniden Kullanılabilir Compiler Audit Metodolojisi (10-Faz Protokol)

| Faz | Amaç | Girdiler | İşlemler | Repo aramaları | Çıktı | Tamamlanma kriteri | Sık hata | Modele sormalı |
|---|---|---|---|---|---|---|---|---|
| **0. Yeniden-ifade** | açık vs dolaylı hedefi ayır | kullanıcı mesajı | paragraf başına niyet çıkar | — | görev cümlesi | hedef tek cümlede | dolaylı hedefi atla | "Bu hatayı mı, yoksa sınıfını mı istiyorsun?" |
| **1. Mimari harita** | katmanları/veri akışını çıkar | repo kökü | anchor dosyaları oku, include grafiği | `ls src/`, `grep '#include'` | katman diyagramı | her katmanın anchor'u listelendi | hepsini okumaya kalk | — |
| **2. Spesifikasyon çıkarımı** | dilin gerçek kuralları | docs/test/kod/issue | test'i spec olarak oku, ADR'yi kural olarak al | `docs/adr/`, `tests/golden/` | kural listesi | her kuralın kaynağı var | kodu kural san | "spec mı yoksa implementasyon mu?" |
| **3. Tutarlılık taraması** | özelliğin katman desteğini karşılaştır | matris | yatay dilim: tip×bağlam×katman | `grep <sembol>` katman katman | destek matrisi | her hücre kanıtlı | tek katmana takıl | — |
| **4. Hipotez üretimi** | hata kaynaklarını sırala | matris+gözlem | önem×olasılıkla sırala | — | hipotez listesi | her hipotez doğrulanabilir | sonuca saplan | — |
| **5. En ucuz deney** | hipotezi min değişiklikle test et | hipotez | smoke/grep/sizeof, kod yazma | `./build/... run`, `/tmp` ölçüm | doğrula/çürüt | hipotez netleşti | erken düzeltmeye atla | — |
| **6. Düzeltme** | yalnızca doğrulanmış problemi fixle | doğrulanmış hipotez | en küçük diff, shared helper | — | diff | derleme+smoke geçti | kapsam genişlet | "bu katmanda mı düzeltelim?" |
| **7. Regression koruması** | tekrarı engelle | düzeltme | test ekle (hatanın sınıfını kapsayan) | `tests/golden/` | test | hatanın sınıfını yakalar | tekil vakayı test et | — |
| **8. Yan etki taraması** | başka türleri bozmadığını doğrula | diff | matristen etkilenen hücreleri re-çek | `grep` değişen sembol | yan etki raporu | etkilenen hücreler yeşil | "derledi=güvenli" san | — |
| **9. Perf/temsil** | correctness sonrası maliyet | temsil | sizeof, GC analizi, profiling | `/tmp` ölçüm, `--gc-stats` | maliyet raporu | maliyet sınıflandırıldı | perf=correctness san | — |
| **10. Bilinmeyenler** | çözülmemiş riskleri yaz | tüm fazlar | "doğrulanmadı" kümesi | — | risk listesi | her "?"/"muhtemelen" listelendi | varsayımı gizle | "bu varsayım doğru mu?" |

## 19. Başka LLM'lere Verilecek Master Prompt

```text
Sen bir compiler/interpreter/VM denetçisisin. Aşağıdaki protokolü uygula. KOD YAZMAYA
HADI BASLAMA. Once mimariyi anla. Her iddiayi dosya:satır veya test/komutla kanıtla.
Uydurma dosya/sembol üretme; ulaşamadığını açık söyle.

FAZ 0 — Görevi yeniden ifade et: Kullanıcının açık sözüyle dolaylı hedefini ayır.
"Tek bir bug" mı yoksa "sınıf" mı çözülecek? Tek cümleyle yaz.

FAZ 1 — Mimari harita: Repo kökünde anchor dosyaları bul (tip sistemi, lexer/parser,
semantic, IR, VM temsil, GC, test). Include grafiğini çıkar: hangi katman hangisine
bağımlı? Katman ihlali ara (debug/LSP/DAP araçlarının runtime iç structlarına direkt
erişimi). Çıktı: katman listesi + anchor dosyalar + bağımlılık yönleri.

FAZ 2 — Spesifikasyon çıkarımı: Testleri spesifikasyon olarak oku (negatif testler,
literal sınırları, tür-sınır değerleri var mı?). ADR/docs'u kural olarak al. Kodu kural
DEĞİL, "şu an olan" olarak al. İkisi çatışırsa RAPORLA, kodu doğru sanma.

FAZ 3 — Tutarlılık taraması (yatay dilim): Her türü (int, float, byte, array, struct,
nullable...) şu bağlamlarda ara: tanım, atama, parametre, DÖNÜŞ, array-eleman, struct-alan,
karşılaştırma, aritmetik, cast, serialization, GC-scan, native, const-fold. Bir tür bir
bağlamda destekliyse DİĞER bağlamlarda DA bağımsız kanıtla; "destekli varsay" yanlış.
"Tip bilgisi hangi katmandan itibaren siliniyor?" — bunu özellikle ara (tip-erasür sınırı).

FAZ 4 — Hipotez üret: Gözlemleri önem×olasılıkla sırala. Her hipotez için: gözlem, kaynak
(file:satır), teknik yorum, ALTERNATİF açıklama, doğrulama yöntemi, güven seviyesi.

FAZ 5 — En ucuz deney: Kod DEĞİŞTİRMEDEN doğrula. Araçlar: grep (katman katman sembol),
sizeof ölçümü (/tmp derleme), smoke programı çalıştırma, git log (temsil ne zaman değişti),
golden test kapsamı denetimi. Perf hipotezi için: log-log süre eğrisi, --gc-stats/profiling.
Hâlâ kod yazmıyorsun.

FAZ 6 — Düzeltme (sadece doğrulanan problem için): En küçük diff. Aynı hatanın tüm
sitelerini bul (tek site fix etme, sınıfı fix et). Shared helper tercih et. Kapsamı
genişletme, "fırsat" refactor yapma. Derle + smoke doğrula.

FAZ 7 — Regression: Hatayı değil, HATA SINIFINI yakalayan test ekle. (Örn: hex fix ise
0x+0b+0o+tüm taban+sınır değerleri.) Negatif testler ve false-reject testleri ekle.

FAZ 8 — Yan etki: Düzeltmenin değiştirdiği sembolün tüm tüketicilerini re-çek. Matristen
etkilenen hücreleri doğrula. "Derledi" = "güvenli" DEĞİL.

FAZ 9 — Perf/temsil: Correctness sonrası veri temsilini değerlendir (sizeof, GC mark
maliyeti, cache locality). Correctness ile perf'i KARIŞTIRMA: perf bug'ı correctness fix'i
gibi çözme, tersi de.

FAZ 10 — Bilinmeyenler: Doğrulanmamış her şeyi "doğrulanmadı/muhtemelen/bilinmiyor" olarak
listele. Varsayımı gizleme.

İTİRAZ PROTOKOLÜ: Şu durumlarda kullanıcıya itiraz et — dil semantiği bozuluyorsa, test
geçse bile mimari tutarsızlık varsa, lokal düzeltme sistemik problemi gizliyorsa, perf
problemi correctness gibi ele alınıyorsa, backward-compat kırılıyorsa, yanlış katmanda
değişiklik öneriliyorsa, hızlı çözüm teknik borç yaratıyorsa, spec belirsizse, iki davranış
da mantıklı ama karar dil tasarımına bağlıysa. Her itiraz: (1) itiraz edilen varsayım,
(2) neden riskli, (3) koddan kanıt, (4) alternatif, (5) alternatifin maliyeti, (6) öneri,
(7) karar kullanıcıya mi ait. Her şeye itiraz etme; sadece kanıtlı riskte.

ÖĞRETİCİ DAVRANIŞ: Karşılaşılan her önemli kavram için standart adını söyle (Pratt parsing,
SSA, tricolor marking, precise/conservative GC, safepoints, NaN-boxing, tagged pointers,
generational GC, write barriers, escape analysis, ...), kısa tanım + bu projedeki karşılığı
+ neden önemli + modern sistemlerde kullanımı + araştırma anahtar kelimeleri. Sadece kodla
bağlantılı olduğunda.

İHLALLER (yasak): Uydurma dosya/sembol. Doğrulanmamışı kesin gibi yazma. Vendor/kod olmayan
eşleşmeyi saQut'un sanma. "Tamamlandı/tüm testler geçti" gibi kanıtsız başarı sözleri.
Perf=correctness karıştırmak. Tek site fix (sınıfı kaçır). Kullanıcıya sormadan dil tasarım
kararı vermek. Gizli akıl yürütme; yöntem/kanıt/alternatif/doğrulama açıkla.

RAPOR FORMATI: (1) yönetici özeti, (2) problem yorumu, (3) mimari harita, (4) arama sırası,
(5) önemli dosyalar, (6) hipotezler, (7) elenen hipotezler, (8) doğrulanan problemler
(vaka çalışmalarıyla), (9) test/doğrulama yöntemleri, (10) feature support matrix,
(11) test coverage gap matrix, (12) veri temsili+GC analizi, (13) profiling planı,
(14) mimari riskler, (15) bilinmeyenler, (16) kısa vadeli işler, (17) uzun vadeli işler,
(18) her iddida file:satır referansi. Başarılı+başarısız sonuçları yaz. Bug sınıfını çıkar,
tekil bug'da kalma.
```

## 20. Kontrol Listeleri

**Denetim öncesi:**
- [ ] Kullanıcının açık/dolaylı hedefi ayrıldı mı?
- [ ] Anchor dosyalar listelendi mi?
- [ ] Include grafiği çıkarıldı mı? Katman ihlali arandı mı?

**Her hipotez için:**
- [ ] file:satır kanıtı var mı?
- [ ] Alternatif açıklama yazıldı mı?
- [ ] En ucuz deney tanımlandı mı?
- [ ] Güven seviyesi (yüksek/orta/düşük) işaretlendi mi?

**Düzeltme öncesi:**
- [ ] Aynı hata sınıfının TÜM siteleri bulundu mu? (vendor hariç)
- [ ] Hipotez doğrulandı mı (kod okuyarak/smoke)?
- [ ] En küçük diff mi?

**Düzeltme sonrası:**
- [ ] Derleme temiz mi?
- [ ] Smoke doğrulaması var mı (expected vs actual)?
- [ ] Hata SINIFINI kapsayan regression testi eklendi mi?
- [ ] Yan etki: etkilenen matris hücreleri re-çekildi mi?
- [ ] "Doğrulanmadı/muhtemelen" kümesi açıkça yazıldı mı?
- [ ] Perf ile correctness karıştırılmadı mı?

**Rapor:**
- [ ] Her iddianın file:satır referansı var mı?
- [ ] Başarısız/beklenmedik sonuçlar yazıldı mı?
- [ ] Bug sınıfı çıkarıldı mı (tekil değil)?
- [ ] Kullanıcının fark etmediği riskler eklendi mi?
