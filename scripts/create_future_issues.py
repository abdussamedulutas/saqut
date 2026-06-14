#!/usr/bin/env python3
"""Faz 0-4 sonrasi icin vizyon/backlog issue'lari.

Bu issue'lar PLAN degildir; fikir/tartisma kayitlaridir. Faz issue'larindan
(#69-73) farkli olarak "Sonuc ve Basari Kriterleri" / "Muhendis olmayan analiz"
bolumleri yoktur - bunun yerine "Acik Sorular" bolumu vardir. Iceriklerin
zamanla degismesi beklenir.
"""
from gitea import make_request, REPO_PATH

L_FIKIR = 66
L_IRVM = 67
L_MODUL = 68
L_TIP = 69
L_FFI = 70
L_TOOLING = 71
L_VIZYON = 72

issues_data = [
    # ---------------- IR & Bytecode VM ----------------
    {
        "title": "[Fikir] IR Komut Seti Tasarımı (Üç-Adresli Kod / Three-Address Code)",
        "labels": [L_FIKIR, L_IRVM],
        "body": """### Giriş (Nedir, Neden Önemli?)
Faz 0-4 bitince AST (Abstract Syntax Tree, Soyut Sözdizimi Ağacı) tip ve sembol bilgisiyle dolu olacak. Ama AST hâlâ "ağaç" — VM'in (Bytecode VM, Bayt Kodu Sanal Makinesi) çalıştırabileceği düz bir komut listesi değil. Bu issue, AST'den üretilecek **IR'in (Immediate Representation, Ara Temsil)** komut setini tartışır.

---

### Gelişme (Olası Yaklaşımlar)
- **Üç-adresli kod (three-address code):** `t1 = a + b`, `t2 = t1 * c` gibi her komut en fazla bir işlem yapar. Okunabilirlik (alet çantası felsefesiyle uyumlu — `saqut ir` ile basılabilir) ile uygulama kolaylığı arasında iyi bir denge.
- Minimal opcode ailesi: aritmetik (`add/sub/mul/div`), karşılaştırma, atama (`store/load`), kontrol akışı (`jump/jump_if_false/label`), fonksiyon (`call/return`), dizi/struct erişimi (`get_field/set_field/get_index/set_index`).
- Her IR komutu kaynak `SourceLocation`'ı taşımalı — hata mesajları ve "öncesi/sonrası" karşılaştırması için (alet çantası amacı).
- `examples/fibonacci.sqt` için elle IR çıktısı yazılıp hedef format netleştirilebilir (taslak doküman olarak).

---

### Açık Sorular
- Üç-adresli kod mu, yoksa stack-tabanlı (yığın tabanlı) bir ara form mu daha basit olur? (VM tasarımıyla bağlantılı, bkz. ilgili issue)
- Struct/array gibi değer-semantikli (value semantics) tipler IR seviyesinde nasıl temsil edilir — kopyalama ne zaman gerçek bir IR komutu olur?
- Optimizasyon (Faz 4) pass'leri AST üzerinde mi kalır, yoksa IR seviyesine de mi taşınır?

*İmza/Yorum:* Bu issue, "IR güçlendirme" yol haritasının (roadmap-frontend.md sonu) ilk taslağıdır. Faz 0-4 bitmeden kilitli karar alınmamalı — sadece beyin fırtınası."""
    },
    {
        "title": "[Fikir] IR'in Serileştirilmesi ve İncelenebilirliği (`saqut ir` komutu)",
        "labels": [L_FIKIR, L_IRVM],
        "body": """### Giriş (Nedir, Neden Önemli?)
saQut'un varlık sebebi "her aşamanın dışarıdan görülebilir olması". Token, AST ve sembol tablosu zaten `saqut tokens/ast/symbols` ile JSON olarak görülebiliyor. IR de aynı muameleyi görmeli.

---

### Gelişme (Olası Yaklaşımlar)
- `saqut ir file:kaynak.sqt` komutu: AST'den üretilen IR'i hem **insan-okur metin** (örn. assembly benzeri `t1 = a + b`) hem de **JSON** (`--format=json`) olarak basar.
- `--optimized` bayrağıyla optimizasyon öncesi/sonrası IR yan yana (diff) gösterilebilir — Faz 4'teki AST öncesi/sonrası karşılaştırmasının IR karşılığı.
- Her IR komutuna kaynak satır/sütun referansı eklenip, IR'den kaynağa "geri işaretleme" (source map benzeri) yapılabilir — ileride debugger için temel oluşturur.

---

### Açık Sorular
- IR metin formatı kendi mini-sözdizimi mi olacak, yoksa mevcut JSON şemasının bir uzantısı mı?
- IR dump'ı dosyaya yazılıp tekrar VM'e beslenebilir mi (round-trip) — bu, derleme adımlarını birbirinden bağımsız test etmeyi kolaylaştırır.

*İmza/Yorum:* Bu, "programlanabilir derleyici" vaadinin IR ayağı. Küçük ama saQut'un kimliği için sembolik önemde."""
    },
    {
        "title": "[Fikir] Bytecode Formatı: Yığın-Tabanlı mı (Stack-based), Register-Tabanlı mı VM?",
        "labels": [L_FIKIR, L_IRVM],
        "body": """### Giriş (Nedir, Neden Önemli?)
ADR-015 çalıştırma modelini kilitledi: IR + bytecode VM (yorumlayıcı döngü), gerçek makine-kodu JIT yok. Ama VM'in *iç mimarisi* henüz seçilmedi: **yığın-tabanlı (stack-based, Python/JVM/Wasm tarzı)** mi, **register-tabanlı (Lua 5'in VM'i tarzı, sanal register'lar)** mı?

---

### Gelişme (Olası Yaklaşımlar)
- **Yığın-tabanlı:** Komutlar değer push/pop eder (`push a`, `push b`, `add`). Üretmesi basit, IR'den bytecode'a çeviri kolay. Dezavantaj: gereksiz push/pop'lar (performans, ama saQut için öncelik değil — bkz. ADR-015 "determinizm/incelenebilirlik > hız").
- **Register-tabanlı:** Her fonksiyonun sanal register'ları olur (`add r1, r2, r3`). Daha az komut, ama derleyici tarafı (register allocation - kayıt tahsisi) daha karmaşık.
- saQut'un önceliği **basitlik + incelenebilirlik** olduğu için yığın-tabanlı VM ile başlamak "önce dikey dilim" ilkesine daha uygun olabilir; register-tabanlı'ya geçiş ileride bir "VM v2" issue'su olabilir.

---

### Açık Sorular
- Yığın-tabanlı seçilirse, fonksiyon çağrıları için ayrı bir "call stack" mı, tek bir birleşik yığın mı kullanılır?
- Debug/inceleme modunda her adımdan sonra yığının tam içeriği dump edilebilir mi (alet çantası vaadiyle doğrudan ilişkili)?

*İmza/Yorum:* Bu karar IR komut setini (önceki issue) doğrudan etkiler — birlikte tartışılmalı. Önerim: v0 için yığın-tabanlı, basitliği ve incelenebilirliği önceliklendirdiği için."""
    },
    {
        "title": "[Fikir] VM Yorumlayıcı Döngüsü ve Dispatch Stratejisi",
        "labels": [L_FIKIR, L_IRVM],
        "body": """### Giriş (Nedir, Neden Önemli?)
Bytecode VM'in kalbi, her komutu okuyup ilgili işlemi yapan "yorumlayıcı döngü" (interpreter loop / dispatch loop). Bu döngünün nasıl yazılacağı hem performansı hem de kod okunabilirliğini etkiler.

---

### Gelişme (Olası Yaklaşımlar)
- **Switch-case dispatch:** `switch(opcode) { case ADD: ...; case JUMP: ...; }`. En basit, en okunabilir, derleyici bağımsız (portable). saQut'un "incelenebilir" felsefesiyle en uyumlu başlangıç noktası.
- **Computed goto / jump table:** Daha hızlı ama derleyiciye özel uzantılar gerektirir (GCC/Clang `&&label`), taşınabilirlik düşer. Hız öncelik değilse (ADR-015) v0'da gereksiz karmaşıklık.
- **Threaded code:** Her IR komutunu doğrudan bir C++ fonksiyon pointer'ına çeviren ileri teknik — şimdilik kapsam dışı.
- v0 önerisi: basit switch-case + her adımda "trace" modu (her komut çalışmadan önce/sonra durum dump edilebilir, ADR-016 FFI seam ile `print` zaten bu yola hazırlanıyor).

---

### Açık Sorular
- Yorumlayıcı tek bir `run()` fonksiyonu mu olacak, yoksa her opcode için ayrı `handle_*` fonksiyonu mu (okunabilirlik vs. fonksiyon çağrı maliyeti — ama maliyet öncelik değil)?
- Adım-adım çalıştırma (step) ve "tüm programı çalıştır" (run) aynı döngüyü mü paylaşacak?

*İmza/Yorum:* "Önce dikey dilim" ilkesi burada da geçerli: en basit switch-case ile fibonacci çalışsın, optimize dispatch ileri bir konu."""
    },
    {
        "title": "[Fikir] Fonksiyon Çağrı Mekanizması ve Call Frame Tasarımı",
        "labels": [L_FIKIR, L_IRVM],
        "body": """### Giriş (Nedir, Neden Önemli?)
`examples/fibonacci.sqt` özyinelemeli (recursive) bir fonksiyon. VM'in fonksiyon çağrılarını (call/return), parametreleri ve yerel değişkenleri nasıl tuttuğu — yani **call frame (çağrı çerçevesi)** tasarımı — özyinelemenin doğru çalışması için kritik.

---

### Gelişme (Olası Yaklaşımlar)
- Her fonksiyon çağrısında bir "frame" oluşturulur: dönüş adresi, parametreler, yerel değişkenler için ayrılan yer. Bu frame'ler bir "call stack" üzerinde tutulur (host C++ heap'i üzerinde — ADR-015, özel allocator yok).
- Özyinelemeli çağrılar (fibonacci(n-1), fibonacci(n-2)) her biri kendi frame'ine sahip olmalı — değişkenler birbirine karışmamalı (scope-based memory model, ADR-014).
- Maksimum çağrı derinliği (stack overflow koruması) — basit bir sayaç/limit ile "stack taşması" hatası verilebilir (yeni bir E-kodu olabilir, ileride katalogla birleştirilir).

---

### Açık Sorular
- Frame boyutu derleme zamanında mı (compile-time, fonksiyonun yerel değişken sayısına göre) hesaplanır, yoksa dinamik mi büyür?
- Fonksiyon dönüş değeri yığının üstüne mi konur (yığın-tabanlı VM ile uyumlu), yoksa ayrı bir "return register" mı kullanılır?

*İmza/Yorum:* Bu issue VM tasarımı (önceki issue) seçimine bağımlıdır; ama fibonacci'nin "bitti" tanımı (recursive + iterative) bu mekanizma olmadan anlamsızdır — birinci öncelikli IR/VM konusu."""
    },
    {
        "title": "[Fikir] Array ve Struct'ların Runtime Bellek Düzeni",
        "labels": [L_FIKIR, L_IRVM],
        "body": """### Giriş (Nedir, Neden Önemli?)
Dil kimliği `struct` ve `int[]` (array/dizi) içeriyor, value semantics (değer semantiği) ile. VM çalışırken bu değerlerin bellekte nasıl yer alacağı — kopyalama ne zaman olur, struct içinde struct/array nasıl saklanır — netleşmeli.

---

### Gelişme (Olası Yaklaşımlar)
- Value semantics demek: `struct Point p2 = p1;` ifadesi `p1`'in **tam kopyasını** oluşturur (referans değil). VM'de bu, struct'ın tüm alanlarının yığın/heap'te kopyalanması anlamına gelir.
- Array boyutu derleme zamanında sabit mi (E009 hatası bunu zaten kontrol ediyor), yoksa dinamik büyüyebilen bir array tipi de mi olacak (`std::vector` benzeri, host heap üzerinde)?
- Struct içinde struct: iç içe value-copy maliyeti büyüyebilir — ama saQut'ta "hız öncelik değil" (ADR-015), önce doğruluk.

---

### Açık Sorular
- Dinamik boyutlu array (`int[]` boyutu çalışma zamanında belirlenen) v0 kapsamında mı, yoksa v1'e mi bırakılır?
- Struct kopyalama IR'de tek bir "copy" komutu mu olur, yoksa alan-alan kopyalama komut dizisine mi açılır (incelenebilirlik açısından ikincisi daha "alet çantası" uyumlu olabilir)?

*İmza/Yorum:* ADR-014'teki "scope-based bellek artık gerekçeli" notuyla doğrudan ilişkili — bu issue o kararın runtime'a yansımasıdır."""
    },
    {
        "title": "[Fikir] IR Seviyesinde Kontrol Akışı Grafiği (CFG) ve SSA — Gerekli mi?",
        "labels": [L_FIKIR, L_IRVM],
        "body": """### Giriş (Nedir, Neden Önemli?)
Çoğu "gerçek" derleyici (GCC, LLVM) IR'i bir **CFG (Control Flow Graph, Kontrol Akışı Grafiği)** üzerinde **SSA (Static Single Assignment, Statik Tekil Atama)** formunda tutar. Bu, ileri optimizasyonları (constant propagation, dead code elimination'ın güçlü hali) çok kolaylaştırır ama ciddi bir mühendislik yatırımıdır.

---

### Gelişme (Olası Yaklaşımlar)
- saQut'un Faz 4 optimizasyonları (constant folding, dead code elimination) şu an **AST üzerinde** çalışıyor ve bu yeterli olabilir — CFG/SSA şart değil.
- CFG/SSA'nın getirisi: döngü içindeki optimizasyonlar, daha güçlü dead-code analizi. Maliyeti: yeni bir ara temsil katmanı, "basit kalsın" ilkesiyle gerilim yaratabilir.
- Orta yol: basit IR (üç-adresli kod, lineer komut listesi + `jump`/`label`) yeterli olduğu sürece CFG/SSA'ya **geçilmez**; ancak optimizasyon ihtiyaçları büyürse (örn. döngü-invariant kod taşıma) bu issue tekrar açılır.

---

### Açık Sorular
- "Yeterlilik" sınırı nasıl ölçülür? Belki: fibonacci + birkaç döngü/dizi örneği optimize edildiğinde gözle görülür iyileşme varsa CFG/SSA'ya gerek yoktur.
- CFG olmadan da basit "basic block" (temel blok) kavramı IR'e eklenebilir mi (sadece jump hedeflerini gruplamak için, SSA olmadan)?

*İmza/Yorum:* Bu issue kasıtlı olarak "yapma" eğiliminde — erken soyutlama riskine karşı bir hatırlatma (roadmap'teki "önce dikey dilim" ilkesi)."""
    },
    # ---------------- Moduller / Import ----------------
    {
        "title": "[Fikir] Import Sözdizimi ve Modül Çözümleme",
        "labels": [L_FIKIR, L_MODUL],
        "body": """### Giriş (Nedir, Neden Önemli?)
Şu ana kadar tüm tasarım **tek dosyalı** programları (`examples/fibonacci.sqt`) hedefliyor. Ama gerçek projeler birden fazla dosyaya bölünür. saQut'a "başka bir dosyadaki fonksiyon/struct'ı kullan" diyebilmek için bir **import (içe aktarma)** sözdizimi gerekir.

---

### Gelişme (Olası Yaklaşımlar)
- En basit model: `import "util.sqt"` — dosya yolu tabanlı, C'nin `#include`'una benzer ama metin yapıştırma değil, **sembol tablosu seviyesinde birleştirme** (her dosya kendi AST'sine sahip kalır, sembol çözümleme dosyalar arası genişler).
- Alternatif: isim-tabanlı modül sistemi (`import util` → `util.sqt` veya `util/` dizini arar), Go/Python tarzı.
- saQut'ta `class`/namespace yok — modül adı, dosya adına eşlenebilecek doğal bir "isim alanı" (namespace) adayı olabilir: `util.helper()` gibi nokta-erişimi.

---

### Açık Sorular
- Döngüsel import (A, B'yi; B, A'yı import ediyor) nasıl ele alınır — derleme hatası mı, yoksa forward-declaration ile mi çözülür (ADR-011'deki forward-reference mantığına benzer)?
- Import edilen dosyanın AST'si her derlemede yeniden mi ayrıştırılır, yoksa bir "derleme birimi cache"i mi olur (LSP issue'suyla bağlantılı)?

*İmza/Yorum:* Bu, "Faz 5: Modüller" için doğal bir başlangıç noktası olabilir — ama Faz 0-4 (tek dosya, fibonacci) bitmeden ele alınmamalı."""
    },
    {
        "title": "[Fikir] Modüller Arası Görünürlük (public/private) ve İsim Çakışmaları",
        "labels": [L_FIKIR, L_MODUL],
        "body": """### Giriş (Nedir, Neden Önemli?)
İmport sistemi geldiğinde, "her şey her yerden görülebilir mi" sorusu ortaya çıkar. Bu, sembol tablosunun (Faz 2) modüller arası nasıl genişletileceğiyle doğrudan ilgili.

---

### Gelişme (Olası Yaklaşımlar)
- En basit kural: bir dosyadaki **tüm üst-seviye tanımlar** (fonksiyon, struct, global değişken) import eden dosyaya açık olur — "herkese açık" (public-by-default), C'nin extern'siz fonksiyonlarına benzer. Basitlik açısından v0 için cazip.
- Daha kontrollü model: `private` anahtar kelimesi ile bir dosyaya özel tanımlar işaretlenebilir — ama bu, dil kimliğine yeni bir anahtar kelime ekler (ADR'lerle uyumlu mu tartışılmalı).
- İsim çakışması (iki modülde aynı isimde fonksiyon): modül-öneki (`util.process()`) zorunlu kılınarak çözülebilir — bu, import sözdizimi issue'suyla birlikte kararlaştırılmalı.

---

### Açık Sorular
- "Herkese açık" model, sembol tablosunun global scope'unu modüller arasında nasıl birleştirir — her modül kendi global scope'una mı sahip olur, yoksa tek bir "program-geneli" scope mu?
- Bu karar E002 (çift tanım) hatasının kapsamını değiştirir mi — aynı isim iki modülde tanımlanırsa hata mı, yoksa öneklemeyle ayrışır mı?

*İmza/Yorum:* Önerim: v0 için "herkese açık, modül-önekiyle eriş" — basit ve declare-before-use (ADR-011) felsefesiyle tutarlı."""
    },
    {
        "title": "[Fikir] Çoklu Dosya Derleme ve Bağımlılık Sırası",
        "labels": [L_FIKIR, L_MODUL],
        "body": """### Giriş (Nedir, Neden Önemli?)
Import sistemi geldiğinde, `saqut run main.sqt` komutu artık tek dosya değil, bir **bağımlılık ağacı** (dependency graph) derlemek zorunda kalır. Bu issue, CLI ve derleme akışının (pipeline) buna nasıl uyacağını tartışır.

---

### Gelişme (Olası Yaklaşımlar)
- Derleyici önce tüm `import` ifadelerini tarayıp bir **dosya bağımlılık grafiği** çıkarır (basit DFS/topological sort — ADR-011'deki struct çevrim kontrolüyle aynı algoritmik aile).
- Her dosya kendi AST'sine ve (Faz 2 sonrası) kendi sembol tablosu parçasına sahip olur; sembol çözümleme son adımda birleştirilir.
- "Programlanabilir derleyici" felsefesiyle: `saqut ast --module=util.sqt` gibi tek bir modülün AST'si de ayrı incelenebilir kalmalı — çoklu dosya derleme tek-dosya inceleme yeteneğini kırmamalı.

---

### Açık Sorular
- Döngüsel bağımlılık (önceki issue'da bahsedilen) burada nasıl raporlanır — yeni bir E-kodu mu (örn. `E011`)?
- Derleme çıktısı (IR/bytecode) tek bir birleşik dosya mı olur, yoksa modül-başına ayrı IR mi üretilip VM'de "link" edilir (gerçek linker'lara benzer ama çok daha basit)?

*İmza/Yorum:* Bu konunun karmaşıklığı yüksek olabilir — "önce dikey dilim" ilkesi burada özellikle geçerli: tek-dosya fibonacci bitmeden bu issue'ya başlanmamalı."""
    },
    # ---------------- Tip Sistemi Genişletmeleri ----------------
    {
        "title": "[Fikir] Native Decimal Tipi (Ondalıklı Sayılarda Hassasiyet)",
        "labels": [L_FIKIR, L_TIP],
        "body": """### Giriş (Nedir, Neden Önemli?)
`float`/`double` ikili (binary) kayan-nokta sayılardır ve ondalık kesirleri (örn. `0.1`) tam temsil edemez — bu, para/finans hesaplamalarında hatalara yol açar. Birçok modern dil (C#, Python `Decimal`) bunun için ayrı bir **decimal (ondalık) tip** sunar.

---

### Gelişme (Olası Yaklaşımlar)
- `decimal` yeni bir primitif `TypeKind` olarak `src/core/type.hpp`'ye eklenebilir (Faz 0'ın `Type` sınıfı genişletilebilir tasarlanmalı — bu issue o genişletilebilirliği şimdiden akılda tutmak için var).
- Runtime temsili: sabit-noktalı (fixed-point) tamsayı + ölçek (scale) çifti (örn. "scaled integer" — `12345` ve ölçek `2` → `123.45`). Host heap'te basit bir struct olarak tutulabilir, özel kütüphane gerekmez (v0).
- Literal Adaptasyon Kuralı (ADR-010) ile ilişki: `decimal x = 1;` geçerli olmalı (tamsayı → decimal kayıpsız); `decimal x = 1.5;` (float literal → decimal) hassasiyet tartışması gerektirir.

---

### Açık Sorular
- `decimal` aritmetiği (`+`, `*`) için taşma (overflow) kuralları ne olacak — hata mı, yoksa otomatik büyüme mi (bu, "host heap, özel allocator yok" ADR-015 ilkesiyle gerilim yaratabilir)?
- Bu tip v0.1'in parçası mı, yoksa "batteries" (ADR-017) kapsamında ileride bir FFI/kütüphane çözümü mü?

*İmza/Yorum:* "Modern dillerin standart hale getirdiği ama derleyici çekirdeğine gömülmesi gereken" tiplerin ilk örneği — kullanıcı isteğiyle doğrudan örtüşüyor."""
    },
    {
        "title": "[Fikir] Native Date/Time (Tarih/Zaman) Tipi",
        "labels": [L_FIKIR, L_TIP],
        "body": """### Giriş (Nedir, Neden Önemli?)
Tarih/zaman, hemen her gerçek programda karşılaşılan ama yanlış yapılması çok kolay bir konudur (saat dilimleri, artık yıllar, formatlar). Modern diller (Go, Rust, JS `Temporal`) bunu standart kütüphaneye/derleyiciye yakın tutar.

---

### Gelişme (Olası Yaklaşımlar)
- v0 için minimal yaklaşım: `date`/`datetime` yeni bir primitif tip olarak değil, **struct olarak saQut'un kendisinde** tanımlanabilir (`struct Date { int year; int month; int day; }`) — bu, "batteries = FFI/kütüphane sınırı" (ADR-017) felsefesiyle örtüşür ve derleyici çekirdeğine yük bindirmez.
- Daha ileri: gerçek saat/takvim hesapları (örn. "bugünden 30 gün sonrası") host sisteminden FFI (ADR-016, `callhost`) ile alınabilir — saat dilimi/takvim mantığı saQut içine yazılmaz, denenmiş bir C kütüphanesine (örn. sistem `time.h`) bağlanır.
- Eğer ileride performans/ergonomi nedeniyle native bir `date` tipi gerekirse, bu issue "native tip mi, struct+FFI mi" kararının kaydı olarak kalır.

---

### Açık Sorular
- "Şu anki zaman" gibi dış-dünya bağımlı bilgi nasıl elde edilir — bu doğrudan FFI seam'in (`print` sonrası) ikinci gerçek kullanım örneği olabilir mi?
- Tarih aritmetiği (`tarih + 5 gün`) operatör overload'ı gerektirir mi — ama dilde operatör overload yok (kilitli karar); bu çatışmayı nasıl çözeriz (fonksiyon: `addDays(d, 5)`)?

*İmza/Yorum:* Önerim: v0'da struct+FFI, native tip değil — "batteries boundary" ilkesinin ikinci somut test vakası."""
    },
    {
        "title": "[Fikir] Enum (Sabit Kümesi) Desteği",
        "labels": [L_FIKIR, L_TIP],
        "body": """### Giriş (Nedir, Neden Önemli?)
Şu anki dil kimliğinde `enum` yok. Ama "renk", "durum", "yön" gibi sabit bir küme içinden değer alan değişkenler çok yaygındır ve bunları `int` ile temsil etmek (örn. `0=KIRMIZI, 1=MAVI`) hem hata-eğilimli hem de okunaksızdır.

---

### Gelişme (Olası Yaklaşımlar)
- En basit model: `enum Renk { KIRMIZI, MAVI, YESIL }` — derleme zamanında `int`'e eşlenen adlandırılmış sabitler. Tip denetimi (Faz 3) açısından `Renk` ayrı bir `TypeKind` (ya da struct'a benzer adlandırılmış-tip) olarak ele alınabilir; `int`'le gizli dönüşüm yasak (ADR-010 ile tutarlı — `Renk x = 0;` yerine `Renk x = KIRMIZI;`).
- `switch`/`match` ile birlikte düşünülürse (dilde şu an `switch` yok, sadece `if`) enum'un asıl gücü ortaya çıkar — bu issue, ileride bir `switch` tartışmasını da tetikleyebilir.
- Struct'lara ek yük getirmez, sembol tablosuna (Faz 2) yeni bir `SymbolKind::EnumValue` eklenmesi yeterli olabilir.

---

### Açık Sorular
- Enum değerleri sadece `int` mi temsil eder, yoksa ileride `string` etiketli enum (örn. JSON serileştirmede yararlı) da düşünülür mü?
- `interface` gibi enum da "ertelendi" listesine mi girer, yoksa v0.1'e (fibonacci sonrası ilk gerçek dil-genişletmesi) yakın bir öncelik mi?

*İmza/Yorum:* Düşük karmaşıklıkta, yüksek günlük-kullanım faydası olan bir özellik — "fibonacci sonrası ilk küçük kazanımlar" listesine iyi bir aday."""
    },
    {
        "title": "[Fikir] String'in İç Temsili ve Unicode/UTF-8 Stratejisi",
        "labels": [L_FIKIR, L_TIP],
        "body": """### Giriş (Nedir, Neden Önemli?)
`string` tipi dil kimliğinde var ama "string nedir" sorusu (bayt dizisi mi, karakter dizisi mi, UTF-8 mi) henüz hiç tartışılmadı. Bu karar, `length()`, indeksleme (`s[0]`), ve dosya/konsol I/O'sunun davranışını doğrudan belirler.

---

### Gelişme (Olası Yaklaşımlar)
- **UTF-8 bayt dizisi (Go/Rust tarzı):** En basit runtime temsili (host C++ `std::string`'e doğrudan eşlenir, ADR-015 "host heap" ilkesiyle uyumlu). Ama `s[0]` bir bayt döner, bir "karakter" değil — Türkçe karakterler (ç, ş, ğ) çok-baytlı olduğundan indeksleme kafa karıştırabilir.
- **Kod-noktası (code point) dizisi:** Her "karakter" tek birim — daha sezgisel ama dönüşüm/depolama maliyeti var.
- v0 önerisi: UTF-8 bayt temsilini benimse (C++'la doğal uyum), ama `length()`/indeksleme dokümantasyonunda "bayt sayısı/bayt indeksi" olduğu açıkça yazılsın — ileride `runeAt()`/`codepointLength()` gibi yardımcı builtin'lerle (ADR-016/017) kod-noktası erişimi sağlanabilir.

---

### Açık Sorular
- String **immutable (değişmez)** mi olacak (Java/Python tarzı, value-semantics ile de uyumlu) yoksa mutable mi?
- String birleştirme (`+`) ve karşılaştırma operatörleri tip denetiminde (Faz 3) nasıl ele alınacak — `string + int` gizli dönüşüm mü (ADR-010 "gizli dönüşüm yok" ile çatışır, muhtemelen `E003`)?

*İmza/Yorum:* Bu karar ne kadar erken alınırsa, sonraki tüm builtin/stdlib tasarımı (özellikle JSON/XML parser örnekleri, ADR-017) o kadar sağlam temellenir."""
    },
    # ---------------- FFI / Builtin / Stdlib ----------------
    {
        "title": "[Fikir] FFI Seam Detaylı Tasarımı — `callhost` İmzası ve Tip Eşleme (Marshaling)",
        "labels": [L_FIKIR, L_FFI],
        "body": """### Giriş (Nedir, Neden Önemli?)
ADR-016, "host fonksiyonu çağır" (FFI — Foreign Function Interface, Yabancı Fonksiyon Arayüzü) mekanizmasının **kasıtlı** bir tasarım kararı olduğunu, `print`'in ilk müşterisi olacağını söylüyor. Ama mekanizmanın somut hali (fonksiyon imzası, parametre/dönüş tipi eşlemesi) henüz tasarlanmadı.

---

### Gelişme (Olası Yaklaşımlar)
- VM'de bir `callhost <isim> <argüman sayısı>` IR/bytecode komutu: argümanlar yığından alınır, C++ tarafındaki kayıtlı bir fonksiyon tablosuna (`std::map<std::string, HostFunction>`) bakılır, sonuç yığına geri konur.
- Tip eşleme: saQut `int`/`float`/`string`/`bool` değerleri C++ `int`/`double`/`std::string`/`bool`'a nasıl çevrilir (marshaling) — basit primitiflerle başlamak ("önce dikey dilim"), struct/array marshaling'i ileri bir adım olarak bırakmak.
- Hata yönetimi: host fonksiyon başarısız olursa (örn. dosya bulunamadı) bu, saQut tarafına nasıl yansır — bir hata kodu mu, yoksa `panic` benzeri bir çalışma-zamanı hatası mı (yeni runtime hata kategorisi gerekebilir)?

---

### Açık Sorular
- Host fonksiyon tablosu derleme zamanında mı sabit (yalnızca derleyicinin sunduğu `print` gibi fonksiyonlar), yoksa kullanıcı kendi C++ fonksiyonunu kayıt edebilir mi (gömülü/embeddable derleyici senaryosu — "alet çantası" ruhuna çok uygun ama kapsam büyütür)?
- `callhost` IR seviyesinde mi, yoksa daha yüksek seviyeli bir AST-annotation mı (Faz 1'in `ExpressionNode` alanlarına eklenebilir)?

*İmza/Yorum:* `print(fibonacci(n))` çalıştığı gün, bu issue'nun "v0" kapsamı fiilen tamamlanmış olacak — minimal hedef gayet net."""
    },
    {
        "title": "[Fikir] Builtin Fonksiyon Kataloğu ve Çözümleme Mekanizması",
        "labels": [L_FIKIR, L_FFI],
        "body": """### Giriş (Nedir, Neden Önemli?)
`print` ilk builtin (yerleşik) fonksiyon. Ama "builtin" kelimesinin derleyici içinde nasıl bir yeri olacağı — sembol tablosunda mı, ayrı bir kayıt defterinde mi — henüz netleşmedi. Bu issue, `print`'ten sonra gelecek `len`, `toString`, `parseInt` gibi fonksiyonların **sistematik** bir şekilde eklenmesini planlar.

---

### Gelişme (Olası Yaklaşımlar)
- Faz 2'nin Symbol Table'ı (sembol tablosu) başlangıçta "önceden tanımlı" (pre-defined) bir global scope ile kurulabilir — her builtin, kullanıcı kodu hiç okunmadan önce bu scope'a `SymbolKind::Function` olarak eklenir. Böylece `print(x)` çağrısı normal bir fonksiyon çağrısı gibi tip denetiminden (Faz 3) geçer.
- Builtin kataloğu merkezi bir dosyada (`src/runtime/builtins.hpp` gibi) tek listede tutulur: isim, parametre tipleri, dönüş tipi, ve VM'deki karşılığı (FFI üzerinden mi, yoksa doğrudan bir IR opcode'u mu).
- İlk aday liste (fibonacci sonrası "küçük kazanımlar"): `print`, `len` (string/array uzunluğu), `toString` (sayıdan string'e), `parseInt`/`parseFloat`.

---

### Açık Sorular
- Builtin'ler kullanıcı tarafından **gölgelenebilir (shadow)** mi — yani kullanıcı kendi `print` fonksiyonunu tanımlarsa ne olur (muhtemelen `E002` çift tanım, ama global scope'ta builtin'lerle çakışma ayrı ele alınmalı)?
- Builtin kataloğu büyüdükçe (örn. 50+ fonksiyon), bunların hepsi "her zaman yüklü" mü olacak, yoksa modül sistemi (ayrı issue) ile "import edilen" bir stdlib mantığına mı evrilecek?

*İmza/Yorum:* `print`'in nasıl çözümlendiğini doğru kurmak, sonraki tüm builtin'lerin şablonu olacak — bu yüzden bu issue erken (Faz 2-3 sırasında) gündeme gelmeli."""
    },
    {
        "title": "[Fikir] Minimal Stdlib Seti (String/Math/Koleksiyon Yardımcıları)",
        "labels": [L_FIKIR, L_FFI],
        "body": """### Giriş (Nedir, Neden Önemli?)
ADR-017 "batteries = sınır problemi" diyor ve JSON/XML/HTML gibi parser'ların saQut'un kendisinde yazılabileceğini söylüyor. Ama bunlar için bile bir **minimal temel** (string birleştirme, karşılaştırma, sayı↔string dönüşümleri, dizi üzerinde gezinme yardımcıları) gerekir. Bu issue, "demo programları yazılabilir olması için gereken en küçük stdlib" kümesini tanımlar.

---

### Gelişme (Olası Yaklaşımlar)
- **String:** `length(s)`, `substring(s, start, end)`, `charAt(s, i)`, `concat(a, b)` (veya `+` operatörü), `equals(a, b)`.
- **Sayı:** `toString(n)`, `parseInt(s)`, `parseFloat(s)`, `abs`, `min`, `max`.
- **Array:** `length(arr)`, belki `append`/`push` (dinamik array tartışmasına bağlı — ayrı issue).
- Hepsi builtin kataloğu (önceki issue) üzerinden, ya doğrudan IR opcode'u ya da FFI (`callhost`) ile saç host C++ standart kütüphanesine (`<string>`, `<cmath>`) yaslanır — "asla elle yeniden yazma" (ADR-017, kripto örneğiyle aynı ilke string/math için de geçerli).

---

### Açık Sorular
- Bu set "derleyiciye gömülü" mü olmalı (her zaman hazır) yoksa ileride bir "standart modül" (`import std`) olarak mı paketlenmeli — import sistemi (ayrı issue) ile zamanlama ilişkisi var.
- JSON parser demo'su (ADR-017'de bahsedilen) için bu listenin yeterli olup olmadığı, demo yazılmaya başlandığında netleşecek.

*İmza/Yorum:* "Fibonacci çalıştı" milestone'undan sonraki ikinci somut hedef belki de "stdlib ile küçük bir JSON parser demo'su" olabilir — bu issue o hedefin ön koşullarını listeliyor."""
    },
    # ---------------- Tooling: LSP / Syntax Highlighting ----------------
    {
        "title": "[Fikir] LSP (Language Server Protocol) Sunucusu Mimarisi",
        "labels": [L_FIKIR, L_TOOLING],
        "body": """### Giriş (Nedir, Neden Önemli?)
**LSP (Language Server Protocol, Dil Sunucusu Protokolü)**, editörlerin (VS Code, Neovim vb.) "kırmızı çizgi göster", "tanıma git", "otomatik tamamla" gibi özellikleri standart bir protokolle derleyiciden alabilmesini sağlar. saQut'un "incelenebilir derleyici" felsefesi, LSP için aslında olağanüstü bir başlangıç noktası: AST, sembol tablosu ve diagnostic'ler zaten JSON olarak dışa açık.

---

### Gelişme (Olası Yaklaşımlar)
- LSP sunucusu, saQut derleyicisinin **üstüne** yazılan ayrı bir süreç (process) olabilir — derleyici çekirdeğini bir kütüphane (library) olarak kullanır, stdin/stdout üzerinden JSON-RPC (LSP'nin temel protokolü) ile editörle konuşur.
- En kritik performans sorunu: editördeki her tuş vuruşunda **tüm dosyayı yeniden derlemek** pahalıdır. "Incremental parsing" (artımlı ayrıştırma) veya en azından "değişen fonksiyonu yeniden derle" gibi bir strateji gerekir — ama bu, v0'ın çok ilerisinde bir konu.
- "Önce dikey dilim": LSP'nin ilk versiyonu sadece `DiagnosticEngine`'in (Faz 0) çıktısını "hover/error squiggle" olarak göstermekle başlayabilir — tüm dosyayı her kaydette yeniden derlemek v0 için yeterli olabilir.

---

### Açık Sorular
- LSP sunucusu aynı C++ binary'sinin bir modu mu (`saqut lsp`) olacak, yoksa tamamen ayrı bir proje mi?
- "Tanıma git" (go-to-definition) özelliği doğrudan Faz 2'nin Symbol Table'ındaki `definitionLoc`/`references` alanlarından besleniyor — bu alanların LSP ihtiyaçlarına göre yeterli olup olmadığı (örn. cross-file references, modül sistemi geldiğinde) kontrol edilmeli.

*İmza/Yorum:* "Derleyici tamamlandıktan sonra" diye düşünülecek bir konu ama Faz 2'nin sembol tablosu tasarımı şimdiden bunu destekleyecek şekilde (her referansın konumu kayıtlı) kurulursa, LSP'ye geçiş çok daha az acı verir."""
    },
    {
        "title": "[Fikir] Syntax Highlighting (Sözdizimi Renklendirme) Grameri",
        "labels": [L_FIKIR, L_TOOLING],
        "body": """### Giriş (Nedir, Neden Önemli?)
Editörlerde `.sqt` dosyalarının renkli görünmesi (anahtar kelimeler, string'ler, yorumlar farklı renkte) geliştirici deneyiminin en temel parçalarından biri — ve LSP'den bağımsız, çok daha basit bir kazanım.

---

### Gelişme (Olası Yaklaşımlar)
- **TextMate grameri** (VS Code'un kullandığı, regex-tabanlı `.tmLanguage.json`): saQut'un tokenizer'ındaki (Tokenizer) 6 token tipinden (`docs` belgelerine göre) yola çıkarak basit bir regex seti yazılabilir — derleyiciden bağımsız, statik bir dosya.
- **Tree-sitter grameri:** Daha güçlü (gerçek bir parser üretir, hata-toleranslı), ama saQut'un kendi Pratt parser'ından ayrı bir gramer tanımı gerektirir — bakım yükü iki ayrı "dil tanımı" demektir.
- v0 önerisi: TextMate grameri (en düşük efor, en hızlı görsel kazanım) — `saqut tokens` çıktısı referans alınarak anahtar kelimeler/operatörler/literal'ler renklendirilir.

---

### Açık Sorular
- Bu gramer, tokenizer'daki token tanımları değiştiğinde **elle senkronize** mi tutulacak, yoksa `saqut tokens --dump-grammar` gibi bir komutla **otomatik üretilebilir** mi (programlanabilir derleyici felsefesiyle çok uyumlu bir fikir)?
- Hedef editör önceliği ne olacak — VS Code (TextMate) mı, Neovim/Helix (Tree-sitter ekosistemi daha güçlü) mı?

*İmza/Yorum:* Bu, listedeki **en düşük efor / en yüksek "keyif" oranına** sahip issue'lardan biri — derleyici bitmeden bile, sadece tokenizer çıktısından üretilebilir."""
    },
    {
        "title": "[Fikir] `saqut fmt` — Otomatik Kod Biçimlendirici (Formatter)",
        "labels": [L_FIKIR, L_TOOLING],
        "body": """### Giriş (Nedir, Neden Önemli?)
`gofmt`, `rustfmt`, `prettier` gibi araçlar artık modern bir dilin "olmazsa olmaz"ı haline geldi: kod her zaman aynı şekilde biçimlenir, "stil tartışmaları" ortadan kalkar. saQut'un AST'si zaten JSON'a serileştirilebiliyor — bu, bir formatter için iyi bir temel.

---

### Gelişme (Olası Yaklaşımlar)
- `saqut fmt file:kaynak.sqt`: dosyayı ayrıştırır (AST), AST'yi **kanonik bir şekilde** tekrar kaynak koda yazdırır (pretty-printer / AST → metin).
- Yorumların (comment) korunması en zor kısım — AST yorum bilgisini taşımıyorsa (Faz 0-1 sırasında bu netleşmeli), formatter yorumları silebilir veya yanlış yere koyabilir. Bu, **AST tasarımına şimdiden not edilmesi gereken bir kısıt.**
- "Öncesi/sonrası" felsefesiyle: `saqut fmt --check` (CI'da kullanılabilir — "bu dosya zaten formatlı mı?") ve `saqut fmt --write` (dosyayı yerinde değiştir) ayrımı yapılabilir.

---

### Açık Sorular
- Pretty-printer, AST'den mi yoksa doğrudan token akışından mı (yorumlar dahil tüm token'lar tokenizer'da zaten tutuluyor) üretilmeli — token-tabanlı yaklaşım yorum-koruma sorununu çözebilir.
- Biçimlendirme kuralları (girinti, boşluk, satır uzunluğu) nasıl ve nerede sabitlenecek — `docs/` altında bir "stil kılavuzu" mu yazılacak?

*İmza/Yorum:* AST tasarımı (Faz 1) sırasında "yorumlar AST'de nasıl temsil edilir" sorusu bu issue'yu doğrudan etkiler — şimdiden not düşülmeye değer bir bağımlılık."""
    },
    # ---------------- Gelecek Vizyonu / Deneysel ----------------
    {
        "title": "[Fikir] Zaman-Yolculuğu Hata Ayıklama (Time-Travel Debugging) — IR Anlık Görüntüleri",
        "labels": [L_FIKIR, L_VIZYON],
        "body": """### Giriş (Nedir, Neden Önemli?)
saQut'un "her aşama incelenebilir" felsefesi VM çalışma zamanına da taşınabilir: VM her IR komutunu çalıştırdığında **tüm değişken/yığın durumunun anlık görüntüsünü (snapshot)** kaydederse, kullanıcı programı "ileri-geri sarabilir" — modern debugger'larda (rr, Chrome DevTools "time travel") popüler ama çoğu küçük dilde olmayan bir özellik.

---

### Gelişme (Olası Yaklaşımlar)
- VM'in yorumlayıcı döngüsüne (ilgili issue) bir "trace modu" eklenir: her adımda (komut, önceki durum, sonraki durum) bir listeye/dosyaya yazılır.
- `saqut run file --trace=trace.json` → `saqut replay trace.json` gibi bir akış: kullanıcı adım adım ileri/geri gidebilir, herhangi bir adımdaki değişken değerlerini görebilir.
- Bellek modeli basit olduğu için (host heap, özel allocator yok — ADR-015) snapshot almak büyük bir teknik engel değil; asıl maliyet disk/bellek kullanımıdır (büyük döngülerde trace çok büyüyebilir — örnekleme/limit gerekebilir).

---

### Açık Sorular
- Trace formatı IR serileştirmesiyle (ilgili issue) aynı JSON şemasını mı paylaşmalı?
- Bu özellik "VM v1"in parçası mı, yoksa tamamen ayrı bir "saqut debug" aracı mı?

*İmza/Yorum:* Bu, "alet çantası" felsefesinin VM çalışma zamanına en doğal uzantısı — diğer küçük dillerde nadiren bulunan, saQut'u **ayırt edici** kılabilecek bir özellik."""
    },
    {
        "title": "[Fikir] Derleyici Playground — WebAssembly'e Derleyip Tarayıcıda Çalıştırma",
        "labels": [L_FIKIR, L_VIZYON],
        "body": """### Giriş (Nedir, Neden Önemli?)
Rust/Go/TypeScript gibi dillerin "playground" siteleri (tarayıcıda kod yaz, çalıştır, AST/derleme adımlarını gör) hem öğretim hem de tanıtım için çok güçlü bir araç. saQut'un C++ çekirdeği **WebAssembly (Wasm)**'a derlenebilirse (Emscripten ile), tüm derleyici tarayıcıda çalışabilir — sunucu gerekmez.

---

### Gelişme (Olası Yaklaşımlar)
- Emscripten ile `saqut` binary'si bir `.wasm` + JS "glue" koduna derlenir. Web sayfası bu Wasm modülünü yükler, kullanıcı kodunu derleyiciye verir.
- saQut'un CLI komutları (`tokens`, `ast`, `symbols`, ileride `ir`, `run`) zaten JSON döndürdüğü için, web arayüzü bu JSON'ları görselleştirir — örneğin AST'yi bir ağaç diyagramı olarak çizmek.
- "Önce dikey dilim, sonra çerçeve" ilkesiyle: bu issue, derleyicinin **kendisi** tamamlanmadan ele alınmaması gereken, ama CMake yapılandırmasına (ADR-003, header-only) Wasm hedefinin **erkenden uyumlu** olup olmadığının kontrol edilmesi gereken bir konudur.

---

### Açık Sorular
- Wasm derlemesi CI'a (varsa) eklenir mi — "her commit'te playground güncel" gibi bir otomasyon hedefi olur mu?
- Playground sadece görüntüleme mi yapar, yoksa `saqut run` ile gerçek **çalıştırma** da (VM tamamlandıktan sonra) tarayıcıda mümkün olur mu?

*İmza/Yorum:* "Keyif veren" kategorisinin tipik örneği — teknik olarak büyük bir engel yok (C++ → Wasm olgun bir yol), ama önceliği derleyici çekirdeğinden sonra gelmeli."""
    },
    {
        "title": "[Fikir] Dil-İçi Test Bloğu (`test { }`) ve Yerleşik Test Çalıştırıcı",
        "labels": [L_FIKIR, L_VIZYON],
        "body": """### Giriş (Nedir, Neden Önemli?)
Rust'ın `#[test]` ve Go'nun `_test.go` dosyaları gibi, modern diller test yazmayı dilin/araç zincirinin **bir parçası** haline getiriyor. saQut için de "fonksiyonumu yazdım, hemen yanına küçük bir test ekleyeyim" akışı düşünülebilir — derleyicinin kendi `examples/fibonacci.sqt` gibi referans programlarını doğrulaması için bile yararlı olur.

---

### Gelişme (Olası Yaklaşımlar)
- Sözdizimi fikri: `test "fibonacci(5) dogru sonucu verir" { assert(fibonacci(5) == 5); }` — `test` ve `assert` yeni anahtar kelimeler/builtin'ler olarak eklenir.
- `saqut test file:kaynak.sqt`: dosyadaki tüm `test` bloklarını çalıştırır, `assert` başarısız olursa hangi test/satır olduğunu raporlar (DiagnosticEngine'in bir "test sonucu" varyantı gibi düşünülebilir).
- Bu özellik aslında `assert` builtin'i (FFI/builtin kataloğu issue'suyla ilişkili) + basit bir "bu bloğu normal akıştan ayrı çalıştır" kuralı kadar küçük olabilir — büyük bir çerçeveye gerek yok.

---

### Açık Sorular
- `test` blokları normal derlemede (`saqut run`) tamamen yok sayılır mı (production kodundan ayrışma), yoksa özel bir bayrakla mı dahil edilir?
- Bu, dilin **sözdizimine** mi (yeni anahtar kelime) yoksa sadece bir **kütüphane fonksiyonu** (`assert(...)` + isim kuralına dayalı dosya/fonksiyon tarama) düzeyinde mi kalmalı — ikincisi dil kimliğini (kilitli kararlar) bozmaz.

*İmza/Yorum:* "Günümüzde frameworklerde olan ama derleyiciye gömülü gelebilecek" taleplere iyi bir örnek — minimal versiyonu (`assert` builtin'i) çok düşük maliyetli."""
    },
    {
        "title": "[Fikir] Paket Yöneticisi / Modül Kayıt Defteri (Registry) Vizyonu",
        "labels": [L_FIKIR, L_VIZYON],
        "body": """### Giriş (Nedir, Neden Önemli?)
Modül sistemi (ayrı issue) "aynı projede birden fazla dosya" sorusunu çözer. Ama "başka birinin yazdığı bir saQut kütüphanesini projeme nasıl eklerim" sorusu bir **paket yöneticisi** gerektirir (npm, cargo, go modules tarzı). Bu, şu an çok uzak bir vizyon ama erken not edilmeye değer.

---

### Gelişme (Olası Yaklaşımlar)
- En minimal hali: `import "https://git.saqut.com/.../util.sqt"` gibi URL-tabanlı import (Go'nun ilk modül modeline benzer) — merkezi bir registry gerektirmez, ADR-017'deki "FFI/kütüphane sınırı" felsefesiyle uyumlu (saQut çekirdeği paket yönetimini bilmez, sadece dosya/URL okur).
- Daha "gerçek" hali: `saqut.toml` gibi bir manifest dosyası, bağımlılık sürümleri, kilit dosyası (lock file) — bu, derleyici çekirdeğinden tamamen ayrı bir CLI aracı (`saqut-pkg`) olarak düşünülebilir, çekirdeği şişirmez.
- saQut'un "alet çantası" kimliği burada bir fırsat sunuyor: paket yöneticisi, saQut'un kendisiyle yazılabilecek bir **dogfooding** (kendi aracını kendi diliyle yazma) projesi olabilir — ama bu, dilin dosya I/O ve string işleme (stdlib issue'ları) yeterince olgunlaştıktan sonra mümkün.

---

### Açık Sorular
- Bu vizyon, projenin "küçük/incelenebilir alet çantası" kimliğiyle ne kadar uyumlu — paket ekosistemi büyüdükçe "monolit korkusu" (ADR-017) geri gelir mi?
- Merkezi bir registry (sunucu, hosting maliyeti) gerçekten gerekli mi, yoksa Git-tabanlı/URL-tabanlı dağıtık model yeterli mi?

*İmza/Yorum:* En "uzak gelecek" issue'lardan biri — ama "import sistemi" tasarlanırken (örn. URL desteği baştan düşünülürse mi, sonradan mı eklenir) bu vizyonun gölgesi düşebilir, o yüzden şimdiden not."""
    },
    {
        "title": "[Fikir] Akıllı Diagnostic — Hata Mesajlarına \"Neden\" ve \"Nasıl Düzeltilir\" Ekleme",
        "labels": [L_FIKIR, L_VIZYON],
        "body": """### Giriş (Nedir, Neden Önemli?)
Faz 0'ın `Diagnostic` yapısında zaten bir `hint` (ipucu) alanı planlanıyor. Bu issue, o alanın **nasıl** dolduğunu ve modern derleyicilerin (Rust, Elm) "hata mesajı bir öğretmen gibi konuşur" felsefesini saQut'a nasıl taşıyabileceğimizi tartışır — yapay zeka kullanmadan, tamamen kural-tabanlı (rule-based).

---

### Gelişme (Olası Yaklaşımlar)
- Her hata kodu (`E001`...`E010`) için, hata mesajına ek olarak **statik bir "neden" şablonu** ve mümkünse **"şunu dener misin"** önerisi eklenir. Örnek: `E003` (tip uyuşmazlığı) → "saQut'ta değişkenler arası gizli dönüşüm yapılmaz (bkz. ADR-010); `int` bir değeri `float` değişkene atamak için ... yapabilirsiniz."
- Bu şablonlar `docs/adr-frontend-analiz.md`'deki kararlarla **doğrudan bağlantılı** olabilir — hata mesajı, ilgili ADR'nin "neden"ini özetleyen bir cümle içerebilir. Bu, dökümantasyon ile derleyici çıktısını birbirine bağlar (saQut'un "her şey ilişkili ve incelenebilir" ruhuna uygun).
- Elm tarzı "did you mean...?" (yazım hatası önerisi): tanımsız isim hatasında (`E001`), sembol tablosundaki (Faz 2) benzer isimli sembollere Levenshtein-mesafesi gibi basit bir algoritmla öneri sunulabilir — küçük, eğlenceli, yüksek-etkili bir özellik.

---

### Açık Sorular
- Bu zenginleştirilmiş mesajlar `DiagnosticEngine`'in (Faz 0) temel yapısına mı gömülür, yoksa ayrı bir "mesaj kataloğu" dosyası mı olur (çeviri/yerelleştirme ihtimaline de açık kapı bırakır)?
- "Did you mean" önerisi performans maliyeti yaratır mı (her hata için sembol tablosu taraması) — büyük projelerde sorun olur mu?

*İmza/Yorum:* Bu, derleyicinin "kullanıcıyla konuşma tarzını" belirleyen, teknik olarak küçük ama **algıyı** çok değiştirebilecek bir issue — Faz 0 bittiğinde, kataloğa hint şablonları eklemek ucuz bir ek iştir."""
    },
]

for issue in issues_data:
    endpoint = f"{REPO_PATH}/issues"
    res = make_request(endpoint, method="POST", data=issue)
    print(f"Created issue #{res['number']}: {res['title']}")
