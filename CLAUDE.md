# CLAUDE.md — saQut Proje Bağlamı

> Bu dosya her oturumda yüklenir. Amaç: projenin kimliğini, kilitli kararları,
> mevcut durumu ve çalışma konvansiyonlarını hızlıca hatırlatmak.

## İletişim
- **Kullanıcıyla TÜM yazışmalar Türkçe.** (Sahibi: Abdussamed ULUTAŞ.)

## Proje nedir?
saQut, **programlanabilir ve incelenebilir bir derleyici** — bir "alet çantası"
(toolbox). Asıl varlık sebebi dilin kendisi değil, **derleme sürecinin her
aşamasının dışarıdan görülebilir/müdahale edilebilir olması**: token'lar, AST,
sembol tablosu, optimizasyon öncesi/sonrası ve IR ayrı ayrı incelenebilir.
Uygulama dili **C++** (header-only eğilimli, ADR-003). CMake + Ninja. `build/`
git'te **izlenmez** (üretilmiş dosyalar; `cmake -B build && ninja -C build` ile yeniden oluştur).

## Kilitli kararlar (değiştirme — gerekçeler ADR'lerde)
- **Çalıştırma modeli: IR + bytecode VM (yorumlayıcı döngü).** Tree-walker DEĞİL,
  gerçek makine-kodu JIT DEĞİL (kapsam dışı; öncelik determinizm + incelenebilirlik,
  ham hız değil). C'ye transpile ileride geçerli 2. backend. İleride makine kodu
  gerekirse libgccjit/LLVM'e bağlanılır (çok uzak). Bellek = host C++ heap; özel
  allocator yok. (ADR-015)
- **Dil kimliği:** prosedürel, C-ailesi sözdizimi, zorunlu class/main boilerplate
  yok. **Semantik (ADR-020):** primitive (`int`/`float`/`bool`) = **değer**;
  bileşik (`struct`/`array`/`string`) = **referans** (JS/Java/C# modeli). "Pointer
  yok" = kullanıcıya `&`/`*` **sözdizimi** verilmez; derleyici/runtime içeride ve
  çalışma zamanında referansı sonuna kadar kullanır. **Yok:** OOP, closure,
  generic, auto/tip çıkarımı, gizli int↔float (tek istisna sabit folding). **Var:**
  struct, tipli fonksiyonlar, array (`int[]`). `class`/`function` sözdizimsel
  **rezerve** (semantik ileride). `interface` **ertelendi** (ADR-018).
  ⚠️ Referans semantiği döngüsel-referans **sızıntısını** açtı → GC/döngü
  toplayıcı borcu (**#56**, `karar-gerekli`).
- **Null güvenliği (ADR-021, REVİZE):** varsayılan non-null; nullable açıkça `Type?`
  (Kotlin/Swift). `null` yalnızca `T?`'ye atanır; `T?` üstünde doğrudan erişim derleme
  hatası. **Atama kuralı `T <: T?`** (notnull→nullable serbest, nullable→notnull yasak).
  **Katı operand kuralı:** non-null bağlamda her operand statik non-null olmalı
  (`int a=b+c+d`, biri nullable → hata). Aklama **yalnızca görünür `if` narrowing**
  (nested + sıralı guard + `&&`; alias takibi yok). ⚠️ **`a!`/`??`/`?.` YASAK** (gizli
  runtime null-aklama yok). Runtime maliyeti **sıfır**; frontend kesin çözer (backend
  yeniden analiz etmez) → runtime null-deref esasen FFI backstop'u. SSA gerekmez (#2).
- **Bellek/GC (ADR-022):** **basit, taşımasız, stop-the-world, deterministik
  mark-sweep** (döngüleri toplar, "cage" korunur). **`shared_ptr`'ı kalıcı model
  YAPMA** (refcount döngüde sızar = topuğa-sıkma). Kural: nesne modelini **baştan
  GC-hazır** kur (header: tip+mark biti+liste; VM kök sayar; nesne çocuk
  referansları sayar). v1: toplamasız (arena); v2: aynı model üstünde mark-sweep.
  Asıl perf-katili GC kararı → basit tutarak de-risk edildi.
- **Eşitlik (ADR-023):** `==` primitive'de değer, referans (struct/array) **kimlik**
  (aynı nesne). Derin/yapısal eşitlik **asla** `==`'e bağlanmaz → ayrı görünür
  `deepEquals()` (PHP `==`/`===`/`clone` ailesi gibi). **String istisnası:** `==`
  **içerik** olmalı (Java gotcha'sından kaçın) → string'i immutable değer-tipi
  modelle (#40). `obj==obj`'i hata yapmak + kullanıcı-tanımlı eşitlik = uzak gelecek.
- **String (ADR-024):** **immutable değer-tipi, iç temsil UTF-8.** `s = s + "x"`
  yeni string üretir; `==` içerik (ADR-023). Bayt/scalar/grapheme erişimi açıkça
  ayrı (sahte O(1) karakter indeksi YOK). Verimli birleştirme için ileride builder.
  Çözdüğü: #40 (yüzey), #9 (iç temsil). Mevcut `Value` string'i inline tutuyor —
  immutable olduğu için bu yeterli; heap/object-model'e taşımak zorunlu değil.
- **Hata yönetimi (ADR-025, #57):** **Swift-tarzı** yakalanabilir, **struct-tabanlı**
  hata — OOP/extend YOK. Standart `Error { line; char; message; trace; code }`
  (message=W/E metni, code=W/E kodu). **Klasik `try{}catch{}` bloğu, UNCHECKED**
  (Java/C#/JS usulü): fonksiyon işaretlenmez (`noexcept`/`constexpr` tarzı YOK),
  çağrıda `try f()` yok — developer'a güven, alışkanlık bozulmaz. Runtime null-deref
  (NPE analoğu), array OOB, /0, `a!` patlaması → yakalanabilir hata (ADR-021'in runtime
  backstop'u). `throw` ile kullanıcı da kaldırır. Deterministik stacktrace (IR satır
  tablosu önkoşul). **Tuple → ertelendi** (ADR-014'teki "yok" gevşedi). `finally` yerine
  ileride `defer`.
- **Tip dönüşümü (ADR-026, #42):** açık **`as`** (infix, sola-bağlı): `x as int`.
  Yalnızca **skaler + string** arası; **struct/array cast YOK** (elle yapıcı fonksiyon —
  derleyiciyi sade tutar, sessiz alan kaybı önlenir). Başarısızlık **hedef tipin
  nullable'lığıyla:** `as int` → `Error` fırlatır; `as int?` → `null`. Ayrı `as?` YOK.
  `float→int` sıfıra kırpar (NaN/Inf/taşma fallible). `int(x)` fonksiyon-stili reddedildi.
- **switch-case (ADR-027):** **statement** (expression sonra); **implicit fallthrough
  YOK** (otomatik break; bilerek paylaşım `case 1,2,3:`). Case'ler **tip-homojen**
  (switch konusuyla aynı tip; enum'da üyeler `Color.Red`; karışık yasak); **exhaustiveness
  YOK** (300-enum sorunu), `default` opsiyonel. Domen: int/**float**/bool/char/string/enum
  (struct/array yok). **Float izinli** ama tam-temsil-edilemeyen literal case → **W-uyarı**
  (`case 0.1:` uyarır, `case 1.5:` uyarmaz). Bağlar: `switch(T?)`+`case null`, `catch`'te `switch(e.code)`.
- **Analiz vs Optimizasyon:** Analiz orijinal AST üstünde annotation; optimizasyon
  **klon** üstünde dönüşüm. `ASTNode::clone()` yük taşıyan merkezi bileşen
  (parent pointer'lar + sembol tablosu remap edilir, ADR-007). Fixpoint döngüsü +
  iterasyon tavanı (`maxFixpointRounds`, ADR-009).
- **Literal/tip kuralı:** tamsayı literali bağlama-göre tiplenir (`float x = 1;`
  geçerli; `int y = 1.5;`→E003; değişken→değişken gizli dönüşüm yok). Döngüsel
  by-value struct → E010 (⚠️ ADR-020 ile revize: referansla tutulan struct alanı
  artık döngü kurabilir, `Node next` meşru). (ADR-010/011)
- **FFI seam:** kasıtlı "host fonksiyonu çağır" mekanizması (`callhost`); `print`
  ilk müşteri (ADR-016). Batteries = sınır/FFI problemi, "zlib'i yeniden yaz"
  değil; kripto asla elle yazılmaz (ADR-017).

## Mevcut durum (yapılan vs planlanan)
- **✅ Birinci kilometre taşı AŞILDI:** `examples/fibonacci.sqt`
  (recursive + iterative) `saqut run` ile çalışıyor → `55\n55`.
- **Çalışıyor (tam pipeline):**
  - Lexer, tokenizer, Pratt parser, AST + JSON serileştirmesi
  - Sembol tablosu (iki-geçişli toplayıcı, döngüsel struct tespiti)
  - Tip sistemi (`src/core/type.hpp`) + diagnostic motoru (`src/diagnostic/`)
  - Tip denetleyici + yapısal doğrulayıcı (`src/semantic/`)
  - Optimizasyon: constant folding (int/bool/logical) + dead code elimination
  - IR üreteci (3-adresli, slot tabanlı) + bytecode VM (yorumlayıcı döngü)
  - CLI: `tokens` / `ast` / `symbols` / `check` / `ir` / `run` (6 komut)
- **Henüz YOK (bilinen eksikler):**
  - float/double codegen (tip sistemi var, IR opcode'u yok)
  - struct IR (parse/semantik var, codegen yok)
  - array IR (parse/semantik var, codegen yok)
  - `%=` operatörü IR'da eksik (#37)
  - Global değişken IR üretimi sessizce atlıyor (#38)
  - W003 ölü kod uyarısı üretilmiyor (#36)
  - DCE silinen düğümleri `delete` etmiyor — bellek sızıntısı (#35)
- **İlke:** Önce uçtan uca tek **dikey dilim**, sonra çerçeve. Erken soyutlamadan kaçın.

## Belge haritası
- `readme.md` — toolbox çerçevesi, built-vs-planned, dil kimliği, çalıştırma modeli.
- `docs/fikirler.md` — ADR-001…005 (backend stratejisi, parser, header-only, token, IR).
- `docs/adr-frontend-analiz.md` — ADR-006…027 (frontend, analiz/optimizasyon,
  çalıştırma modeli, FFI, interface, bellek, **değer/referans semantiği, null
  güvenliği, mark-sweep GC, eşitlik, string, hata yönetimi, tip dönüşümü, switch-case**).
- `docs/sonnet-handoff.md` — **Sonnet için uygulama promptu** (ADR-020…024'ü koda
  döken sıralı görev planı; ilk görev: GC-hazır nesne modeli + array runtime).
- `docs/roadmap-frontend.md` — faz-faz uygulama planı (Faz 0–4 → fibonacci).
- `docs/transkript-frontend-tasarim.md` — tasarım oturumu transkripti.
- `examples/fibonacci.sqt` — geçerli referans program.
- `examples/parser-stress/` — yalnızca parser'ı zorlayan, **geçerli olmayan** fixture'lar.

## Gitea issue yönetimi
- Repo: `git.saqut.com/saqut/saqut-compiler` (Gitea). Cloudflare, Python-urllib
  User-Agent'ını banlıyor → tarayıcı User-Agent'ı şart.
- `scripts/gitea.py` — API istemcisi (`~/.git-credentials`'tan kimlik okur).
  Komutlar: `list/get/create/edit/comment`. Toplu issue üretimi:
  `scripts/create_issues.py`, `create_future_issues.py`, `create_syntax_test_issues.py`.
- **Issue yapısı (bu oturumda kuruldu):**
  - **#69–73** `faz-plani` — Faz 0–4 (Tip+Diagnostic, AST refactor, Symbol Table,
    Semantik Analiz, Optimizasyon). Format: Giriş/Gelişme/Sonuç-Başarı Kriterleri +
    mühendis-olmayan analiz.
  - **#74–98, #111** `fikir` — IR/VM tasarımı, modül/import, tip genişletmeleri
    (decimal/date/enum/string), FFI/builtin/stdlib, tooling (LSP/highlight/fmt),
    gelecek vizyonu (time-travel debug, WASM playground, test bloğu, paket yöneticisi).
    Format: Giriş/Gelişme/**Açık Sorular** (başarı kriteri YOK).
  - **#99–105** `test-senaryosu` — kaynak kod + beklenen çıktı içeren golden-test'ler.
  - **#106–110** `cli-ux`/`kalite-mimari` — CLI fikirleri, C/Java/Go tarzı tavsiyeler.
  - LSP (#91) ve CLI (#107) **Tier 0–4** katmanlı yetenek haritası olarak yazıldı;
    #111 ekosistem bağımlılık sırası.

## Lisans (LICENSE.md — bu oturumda yeniden yazıldı)
- **Model:** "Kaynağı Açık — Ticari Kullanımı Kısıtlı" (açık kaynak ama **özgür
  yazılım değil**). Önceki GPL-tarzı copyleft metin niyetle çelişiyordu, değiştirildi.
- **Çekirdek ilke:** Gelir saQut'un **ürettiği Çıktıdan** elde edilir, **saQut'un
  kendisinden değil.**
- **Serbest (ticari dahil):** kod yazmak/derlemek, üretilen programları/exe/işlenmiş
  veriyi satmak, derleyiciyi özel araç olarak iç kullanım.
- **İzin gerektirir (telif sahibinden):** derleyiciyi 3. tarafa kurup ücret almak,
  Web-IDE/derleme servisi, canlı backend motoru, otomasyon/AI aracı, alt-bileşen
  gömme, iç parçaları (AST/optimizatör) ticari yeniden kullanım. Runtime'a bağımlı
  sürümlerde sunucu-tarafı ticari kullanım da bu kapsamda.
- **Maddeler:** §7 Katkı (PR herkese açık, **merge kararı yalnızca Abdussamed
  ULUTAŞ**; katkı sahipleri ticari lisanslama dâhil hakları telif sahibine verir),
  §8 Patent (lisans + dava açana otomatik fesih), §9 Marka ("saQut" adı korunur),
  §11 Fesih (ihlalde otomatik + 30 gün düzeltme; edinilmiş Çıktı korunur).
- Üç dilde (TR/EN/DE), TR esas. Ticari lisans iletişimi: saqutsoftware+gitea@gmail.com
- **Not:** Bespoke lisans; ciddi ticari aşamada hukukçu gözden geçirmesi önerilir.
  Telif yalnızca kodu/belgeyi korur, **fikri/tasarımı değil** (sıfırdan yeniden
  yazım engellenemez); isim ise marka ile korunur.

## Çalışma konvansiyonları
- Commit mesajlarına `Co-Authored-By` veya `Claude-Session` satırı **ekleme**.
- Ana dal `0.1.0`; geliştirme branchi `0.2.0`. commit/push kullanıcı isteyince yapılır.
- `build/` artık git'te izlenmiyor (.gitignore'da). `wiki/` klasörü repo'ya dahil edildi.
- Wiki GitHub repo'sundaki `wiki/` klasöründen yönetilir.
