# saQut — Mimari Referans

**Programlanabilir, incelenebilir bir derleyici — bir "alet çantası" (toolbox).**

saQut'un asıl varlık sebebi dilin kendisinden çok, **derleme sürecinin her
aşamasının dışarıdan görülebilir ve müdahale edilebilir olmasıdır.** Token'lar,
AST, sembol tablosu, optimizasyonun öncesi/sonrası ve IR — hepsi ayrı ayrı
incelenebilir. Dil, bu aletin üzerinde çalıştığı küçük, prosedürel bir
örnektir; vitrin değil, alet.

Uygulama dili **C++**'tır (header-only eğilimli, bkz. `docs/fikirler.md` ADR-003).

---

## Şu an ne çalışıyor, ne çalışmıyor

Belgeler **planlanan** ile **yapılan**ı net ayırır. Bugünkü gerçek durum:

### ✅ Çalışıyor (built)
- **Lexer** — karakter seviyesi tarama, konum takibi.
- **Tokenizer** — token üretimi (6 token tipi), yorum satırı desteği.
- **Pratt parser** — ifade (Pratt) + statement (recursive descent) ayrıştırma.
- **AST** — fonksiyon, blok, değişken tanımı, if/for/while/do-while/return,
  ifade node'ları.
- **AST'nin JSON serileştirmesi** — `saqut ast` ile incelenebilir.
- **CLI komut yapısı** — `tokens`, `ast`, `symbols`, `run` iskeletleri.
- **Kaynak konum takibi** (SourceLocation) — offset → (satır, sütun).
- **Minimal IR deneyi** — basit aritmetiği düşürür (örn. `1 + (7/3)` → kısa
  doğrusal komut dizisi). Henüz tam bir backend değil, bir deneydir.

### 🚧 Henüz yok (planned)
- Sembol tablosu
- Semantik analiz
- Tip sistemi
- Diagnostic (hata raporlama) motoru
- Optimizasyon
- IR + bytecode VM ile çalıştırma

> `feature/frontend-analysis` dalı şu an yalnızca bu yapılmamış işin **tasarım
> belgelerini** içerir, kodunu değil.

**Birinci kilometre taşı ("bitti" tanımı):** derleyici **fibonacci'yi**
(recursive + iterative) ve basit matematik/döngü programlarını **derleyip
çalıştırabilmeli.** Referans program: `examples/fibonacci.sqt`.

---

## Dil kimliği (kilitli)

Prosedürel, **C ailesi sözdizimi**, **value semantics**. İlk ifade doğrudan bir
işlem/tanım olabilir; zorunlu class/`main` boilerplate'i yoktur (Java'nın aksine).

| Özellik | Karar |
|---|---|
| Class / OOP / kalıtım | **Yok** |
| Closure | **Yok** |
| Generic | **Yok** |
| Kullanıcıya açık pointer (`*` / `&`) | **Yok** — derleyici/runtime içeride pointer'ı serbestçe kullanır |
| `struct` | **Var** |
| Tipli fonksiyonlar (dönüş + parametre) | **Var** |
| Array (`int[]`) | **Var** |
| `interface` | **Ertelendi** (v0 değil — gerekçe ADR-018) |
| `auto` / tip çıkarımı | **Yok** |
| Gizli int↔float dönüşümü | **Yok** (tek istisna: sabit folding) |

Gerekçe: prosedürel tasarım semantik karmaşıklığı en aza indirir ve hedeflerle
(fibonacci, matematik, sıralama, ayrıştırma) örtüşür. Standart C'de `class`
yoktur (o C++'tır); C, struct + fonksiyonun yettiğini kanıtlar.

---

## Çalıştırma modeli: IR + bytecode VM (referans) + MIR JIT (ADR-032)

saQut, kendi **IR**'sine derler. Birincil ve **referans** çalıştırıcı bir
**yorumlayıcı döngü (bytecode VM)**; ikinci çalıştırma yolu **MIR tabanlı JIT**
+ **gömülü-runtime AOT** (`saqut build`) — bkz.
`docs/adr/ADR-032-mir-jit-gomulu-runtime-aot.md`.

- **Tree-walker DEĞİL** (çok yavaş).
- **VM = referans backend.** Yeni bir IR opcode'u önce VM'de doğrulanır; JIT
  eşlemesi sonra yazılır. VM ve JIT aynı IR'de **aynı çıktıyı** vermek
  zorundadır (diferansiyel test).
- **MIR JIT (#80):** saf C, sıfır bağımlılık, statik linklenir → kullanıcı
  makinesinde **hiçbir harici toolchain gerekmez**. Hedef ham hız değil,
  "kabul edilebilir normal hız" (~GCC -O2'nin %70-90'ı). GC kökleri **shadow
  stack** ile deterministik ve incelenebilir bulunur.
- **Gömülü-runtime AOT (#81):** `saqut build` runtime kopyası + gömülü IR'den
  linker'sız **tek bağımsız exe** üretir (`deno compile` modeli).
- **Bellek bu modelde kolaydır:** host (C++) heap'i kullanılır; v0 için özel
  runtime allocator gerekmez.
- **ELENDİ:** C'ye transpile ve libgccjit (ikisi de kullanıcıya harici
  toolchain bağımlılığı getirir); **LLVM fiilen kapalı, muhtemelen hiç
  yapılmayacak** (tek getirisi agresif optimizasyon — istenmiyor, determinizmin
  düşmanı). WASM multi-backend planında; tarayıcı/playground en son.

---

## Tasarım felsefesi — neden saQut farklı?

saQut "daha iyi bir dil" iddiasında değil. Farkı, **derleyiciyi bir platform**
olarak ele almasında. İki taahhüt üstüne kuruludur:

**1. Cam (gör + sorgula + içine müdahale et).** Her aşama — token, AST, sembol
tablosu, tip, IR — kararlı, makine-okur ve **çift yönlü** bir arayüzden
erişilebilir olmalıdır. Sadece "dök ve göster" değil: kendi AST'ini verip tip
kontrolü isteyebilmeli, IR verip çalıştırabilmelisin. **Turnusol testi:** bir
yabancı, yalnızca `saqut ast` + `saqut symbols` çıktısından, bizden habersiz bir
LSP yazabiliyor mu? Cevap "evet" ise platform gerçektir.

**2. Kafes (deterministik + yetenek-güvenli çalıştırma).** Pointer yok, value
semantics, scope-tabanlı bellek ve **tek dış-dünya kapısı olan FFI seam**
(ADR-016) sayesinde saQut kodu, host'un açıkça izin verdiği fonksiyonlar dışında
dünyaya dokunamaz. Bytecode VM deterministiktir (ADR-015): aynı girdi → aynı
çıktı → aynı çalışma. "Sadelik" diye tasarlanan bu kararlar aslında bir
**yetenek-güvenliği (capability sandbox)** kurar — güvenilmeyen veya
AI-üretimi kodu güvenle çalıştırmak için biçilmiş kaftan.

**Bu ikisinin ödülü — kayıt & tekrar (record-replay).** 🚧 *(vizyon, v0 değil;
bkz. issue #94.)* Belirsizliğin tek kaynağı (kullanıcı girdisi, zaman, IO,
GC/thread kararları) FFI kapısından geçtiği için, mükemmel tekrar oynatma için
**her değişkeni her adımda kaydetmek gerekmez** — yalnızca kapıdan geçen değerler
kaydedilir, gerisi VM deterministik olarak yeniden çalıştırılarak üretilir. Boyut
gigabayttan kilobayta düşer. Replay modunda FFI çağrıları gerçekten çalışmaz,
kaydedilmiş değeri döndürür (dosyayı tekrar silmez, sunucuya tekrar istek atmaz).
Böylece "benim makinemde çalışıyor, müşteride patlıyor" sorunu: müşteri bir dump
yollar, sen çöküşü adım adım, aynı verilerle geri sararsın.

> Log, önceden sormayı akıl ettiğin sorulara cevap verir; **tekrar-oynatma,
> çöküşten *sonra* aklına gelen sorulara.** Zaman-yolculuğu hata ayıklama ayrı
> bir altyapı değildir — cam sorgularına bir **zaman koordinatı** eklemektir.

⚠️ Bu ödülün bedeli v0'da ödenir: **determinizm kutsaldır ve her belirsizlik
kaynağı kayıt-altına-alınabilir tek kapıdan geçmelidir.** Sonradan eklenemez;
baştan korunur.

---

## Mimari hatlar

```
KAYNAK KOD
   │  lexer
   ▼
TOKEN'LAR ──────────────  saqut tokens
   │  parser (Pratt + recursive descent)
   ▼
AST ────────────────────  saqut ast
   │  sembol toplama (iki geçişli)        ┐
   ▼                                       │
SEMBOL TABLOSU ─────────  saqut symbols    │  FRONTEND
   │  semantik analiz (annotation)         │  (yapı + anlam)
   ▼                                       │
ANNOTATE EDİLMİŞ AST ───  saqut ast        ┘
   │  optimizasyon (opsiyonel, klon üstünde)   ── MIDDLE-END
   ▼
IR ─────────────────────  (planlanan)      ┐
   │  bytecode VM / yorumlayıcı döngü       │  BACKEND
   ▼                                        │  (çalıştırma + FFI seam)
ÇALIŞTIRMA / ÇIKTI ─────  saqut run        ┘
```

- **Frontend** yapıyı ve anlamı modeller (tip, scope, dataflow).
- **"Hangi çekirdek, hangi cihaz, ne zaman, hangi çıktı formatı"** runtime/backend
  meselesidir — frontend'e yüklenmez.

---

## CLI (mevcut + planlanan)

```
# --- çalışıyor ---
saqut tokens   file:kaynak.sqt      # token listesi
saqut ast      file:kaynak.sqt      # AST (JSON)
saqut symbols  file:kaynak.sqt      # sembol tablosu
saqut run      file:kaynak.sqt      # IR üret + bytecode VM ile çalıştır
saqut ast      file:kaynak.sqt --optimized   # klon, optimize edilmiş AST (öncesi/sonrası)

# --- planlanan (ADR-032) ---
saqut run --jit file:kaynak.sqt     # MIR JIT ile çalıştır (#80)
saqut mir      file:kaynak.sqt      # MIR dökümü (incelenebilirlik)
saqut build    kaynak.sqt -o prog   # gömülü-runtime AOT: tek bağımsız exe (#81)
```

Tasarım gereği her aşamanın çıktısı erken bir noktada dosyalanabilir/loglanabilir
(programlanabilir derleyici). Token, ham AST, optimize AST ve IR ayrı ayrı
kaydedilebilir.

---

## Batteries / stdlib — kuzey yıldızı, ertelendi

Gerçek bir genel sürüm pil ile gelmeli (sıralama, sıkıştırma, kripto,
JSON/XML/HTML ayrıştırma). Ama bu **bugünün işi ve v0.1 değildir.**

Mimari çerçeve (monolit korkusunu önler): derleyici pilleri çekirdeğine
gömmez. Bunun yerine **küçük bir gerçek builtin kümesi** (`print`, temel
zorunlular) + **gerisi kütüphane/FFI** ile gelir. "Batteries" sorunu aslında
bir **sınır (FFI/link seam) sorunudur**, "zlib'i yeniden yaz" sorunu değil.
Sınır bir kez çizilir, piller üstünde sonsuza dek birikir.

- **JSON/XML/HTML ayrıştırıcıları saQut'un kendisinde yazılabilir** (string +
  struct + fonksiyon + kontrol akışı yeter). İlk gerçek demo programları.
- **Sıkıştırma/kripto:** denenmiş C kütüphanelerine FFI ile bağlan. **Kripto
  asla elle yazılmaz.**
- **Bugüne tek yansıması:** IR/runtime tasarlanırken **bilinçli bir FFI seam**
  ("host fonksiyonu çağır" deliği) bırakılır. `print` bunu zaten zorlar — bunu
  kaza değil, **kasıtlı bir mekanizma** yapıyoruz (ADR-016).

---

## Belge haritası

| Belge | İçerik |
|---|---|
| `docs/fikirler.md` | ADR-001…005: backend stratejisi, parser, header-only, token, IR |
| `docs/adr-frontend-analiz.md` | ADR-006…019: frontend, analiz/optimizasyon, çalıştırma modeli, FFI, interface, bellek |
| `docs/roadmap-frontend.md` | Faz-faz uygulama planı (sembol tablosu → fibonacci) |
| `docs/transkript-frontend-tasarim.md` | Tasarım oturumunun transkripti |
| `examples/fibonacci.sqt` | Geçerli referans program (semantik + kod üretimi fixture'ı) |
| `examples/parser-stress/` | Yalnızca parser'ı zorlayan, **geçerli olmayan** fixture'lar |

---

## İlke

Bir şey çalışmadan önce çerçeve inşa etmekten kaçın. Önce **uçtan uca tek bir
dikey dilim** çalıştır (kaynak → IR → çalıştır; tamsayı aritmetiği + değişken +
kontrol akışı + tek bir `print`). Modülerlik bir kuzey yıldızıdır, v0.1
gereksinimi değil; ihtiyaç doğmadan eklenen her soyutlama **daha az değil, daha
çok** karmaşıklıktır.
