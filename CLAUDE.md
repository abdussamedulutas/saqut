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
- **Çalıştırma modeli (ADR-015, ADR-032 ile REVİZE):** IR + bytecode VM
  **referans backend** olarak kalır (yeni opcode önce VM'de doğrulanır); ikinci
  çalıştırma yolu **MIR tabanlı JIT** (#80) + **gömülü-runtime AOT** `saqut build`
  (#81, `deno compile` modeli: runtime kopyası + IR gömülü tek exe, linker'sız).
  **Kısıt: kullanıcı makinesinde sıfır harici toolchain** → C transpile ELENDİ
  (C derleyicisi ister), libgccjit ELENDİ (içeride binutils'e şell atar),
  **LLVM fiilen kapalı — muhtemelen hiç yapılmayacak** (tek getirisi agresif
  optimizasyon, ki istemiyoruz; determinizmin düşmanı). Hedef ham hız değil,
  "kabul edilebilir normal hız" (MIR ≈ GCC -O2'nin %70-90'ı). GC kökleri JIT'te
  **shadow stack** ile bulunur (deterministik + incelenebilir). VM ve JIT aynı
  IR'de aynı çıktıyı vermek ZORUNDA (diferansiyel test). IR = **dar bel**: yeni
  özellik önce var olan opcodelara desugar edilmeye çalışılır. Tree-walker DEĞİL.
  Bellek = host C++ heap; özel allocator yok. WASM multi-backend planında,
  tarayıcı/playground en son. **1.0.0'da JIT/AOT bayraksız VARSAYILAN olacak,
  VM yalnızca debug/interpreter amaçlı bayrakla erişilebilir kalacak** —
  kullanıcı talimatı, dispatch noktası (`run.hpp`'deki tek `if`) değişmez,
  yalnızca bayrağın varsayılan yönü döner. **MIR entegrasyon detayı
  (opcode→MIR eşleme tablosu, Value ABI kutulama kararı, GC shadow-stack
  somutlaştırma, hata yönetimi açık sorusu, çoklu-iş-parçacığı uyumu) →
  `MIRPLAN.md`** (kök dizin) — koda başlamadan önce hâlâ **DUR ve sor**
  kısıtı geçerli, bu belge yalnızca zemin hazırlığı.
  **Optimizasyon seviyesi determinizm gerekçesiyle kısıtlanmaz** (kullanıcı
  düzeltmesi): MIR'in kendi RA/DCE gibi klasik codegen optimizasyonları
  LLVM'in UB-sömüren agresif dönüşümlerinden farklı, aynı girdi→aynı çıktı
  verir; asıl determinizm tehdidi bir modülün kendi işi dışına taşması
  (kapsam kayması), optimizasyon seviyesi değil.
- **Dil kimliği:** prosedürel, C-ailesi sözdizimi, zorunlu class/main boilerplate
  yok. **Semantik (ADR-020):** primitive (`int`/`float`/`bool`/`decimal`) = **değer**;
  bileşik (`struct`/`array`/`string`) = **referans** (JS/Java/C# modeli). "Pointer
  yok" = kullanıcıya `&`/`*` **sözdizimi** verilmez; derleyici/runtime içeride ve
  çalışma zamanında referansı sonuna kadar kullanır. **Nested struct:** iç struct
  alanları VarDecl anında özyinelemeli tahsis edilir (STRUCT_NEW zinciri, IR'de
  görünür); `b = a` sonrası b ve a dış yapıyı VE iç struct'ları referansla paylaşır
  (derin kopya değil) — ADR-029. **Yok:** OOP, closure, generic, auto/tip çıkarımı,
  gizli int↔float (tek istisna sabit folding). **Var:** struct, tipli fonksiyonlar,
  array (`int[]`). `class`/`function` sözdizimsel **rezerve** (semantik ileride).
  `interface` **ertelendi** (ADR-018).
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
  - Optimizasyon: constant folding (int/bool/logical) + dead code elimination (W003 dahil)
  - IR üreteci (3-adresli, slot tabanlı) + bytecode VM (yorumlayıcı döngü)
  - CLI: `tokens` / `ast` / `symbols` / `check` / `ir` / `run` / `exec` / `bench` / `lsp` / `dap`
  - **Tipler:** `int`, `float`, `bool`, `string`, `decimal`, `byte` (0-255, #86),
    `enum`, `struct` (nested dahil), `array`, nullable `T?`
  - **Operatörler:** aritmetik, bitwise, mantıksal, karşılaştırma, tüm bileşik atamalar (`%=` dahil)
  - **Kontrol akışı:** `if/else`, `for`, `while`, `do-while`, `switch-case`, `break`/`continue`/`return`
  - **Hata yönetimi:** `try/catch/throw` (ADR-025), cast `as` (ADR-026)
  - **Global değişkenler:** LOAD_GLOBAL/STORE_GLOBAL (issue #38 kapatıldı)
  - **Modül sistemi:** `import`/`export`, çok modüllü derleme; döngü tespiti
    `E_MODULE_CYCLE` (ADR-031, #78)
  - **Nested struct:** struct-tipli alanlar VarDecl anında özyinelemeli tahsis (ADR-029)
  - **GC (#77, ADR-022):** eşik tabanlı mark-sweep — `Interpreter::maybeCollect()`
    instruction sınırında (safepoint); kökler moduleSlots_ + frame slot'ları +
    pendingThrow_; adaptif eşik (canlı×2). CLI: `--gc-threshold=N`, `--gc-stats`.
    ⚠️ Kural: opcode ORTASINDA collect çağırma — slot'a bağlanmamış nesne toplanır.
  - **Builtin sözdizimi (#85, ADR-033):** birincil UFCS nokta çağrısı
    (`arr.push(12)`, `s.upper()`, `p.toJson()` — OOP değil, `f(a,b)` şekeri;
    aynı IR); ikincil ad alanı (`array::push(arr,12)`, `string::upper(s)`,
    `struct::toJson(p)`). Struct alanı builtin'i gölgeler (çağrı hatası).
    Eski `ElemTip::metod` W006 ile çalışır, v0.7.0'da kalkar.
  - **byte tipi (#86, ADR-026 genişlemesi):** 8-bit işaretsiz değer tipi
    (0-255); VM'de int taşınır. Literal bağlam-güdümlü + aralık denetimi
    (`byte b=300` derleme hatası); aritmetikte int'e terfi (`byte+byte→int`,
    bitwise dahil); cast `int↔byte` (int→byte fallible, CAST_INT_TO_BYTE_CHECKED),
    `byte→string`; `byte↔float/decimal` ve `string→byte` yasak (önce int'e).
    `byte[]` mevcut array runtime'ında çalışır.
- **Henüz YOK (gerçek eksikler):**
  - MIR JIT backend (#80) ve gömülü-runtime AOT `saqut build` (#81) — ADR-032
    ile kararlaştırıldı, henüz başlanmadı (⚠️ kullanıcı talimatı: MIR'e
    GELİNCE DUR ve sor; stdlib dalgasından önce builtin listesi onayı al)
- **DAP Faz 7–9 tamam (#105, 0.5.0):** print → output event (Interpreter
  outputSink seam'i — protokol stdout'u temiz, sürücüde çerçeve-dışı bayt
  kalkanı); stopOnEntry işleniyor; verified lineToFirstIP'e bakıyor (yol
  kanonik); gerçek pause (FrameReader — DAP okuma yolu std::cin DEĞİL,
  stdio tamponu pipe baytlarını yutuyordu; bütçe turları + tur arası poll);
  fetch-öncesi adım kontrolü (satır sınırındaki instruction yutulması
  düzeltildi — print adımlamada çalışmıyordu); duraklama satırı = sıradaki
  instruction. `tests/dap/` 10 senaryo.
- **LSP/DAP kurtarma planı** (`docs/prompt-lsp-dap-kurtarma.md`, Faz 0–6):
  Faz 0 tamam — `tests/lsp/` golden test altyapısı kuruldu. Faz 1 tamam —
  `ModuleLoader` artık bir `SourceOverlay` seam'i (`src/module/module_loader.hpp`)
  kabul ediyor; `DocumentStore::runPipeline` açık tüm belgeleri overlay olarak
  sağlıyor, LSP artık diski değil editör buffer'ını derliyor (kök neden #1
  kapandı). `uriToPath`/`pathToUri` `src/lsp/uri.hpp`'de ortak yardımcı oldu.
  `tests/lsp/` 8 senaryo (`07_buffer_overlay`, `08_didchange_overlay` yeni).
  Faz 2 tamam — Parser artık opsiyonel bir `DiagnosticEngine*` alıyor
  (`ModuleLoader` bağlıyor); sözdizimi hataları konumlu `E9xx` tanısına
  dönüşüp panic-mode recovery (`Parser::synchronizeAndMakeError`, yeni
  `ASTKind::Error`/`ErrorNode`) ile bilinen bir sınıra kadar atlayıp parse'a
  devam ediyor — kök neden #2 kapandı. `DocumentStore::runPipeline`'daki
  erken `return`'ler kaldırıldı: sözdizimi hatası olsa da hatanın dışındaki
  fonksiyonlar için hover/definition/documentSymbol çalışmaya devam ediyor.
  `tests/lsp/` 9 senaryo (`09_syntax_error_recovery` yeni). Faz 3 tamam —
  konum birimi anlaşması (`initialize`'da `positionEncoding`, `src/lsp/
  position.hpp` UTF-16↔byte dönüştürücüleri); `findSymbolAt` artık isim-
  uzunluğu aralık eşleştirmesi değil token binary search + (offset→Symbol*)
  indeksi (`DocumentState::tokens`/`symbolByOffset`) — kök neden #3 kapandı.
  `definition`/`references`/`documentSymbol`/`documentHighlight` artık
  sorgulanan değil TANIMIN bulunduğu dosyanın URI'sini döndürüyor
  (`DocumentStore::uriForPath`); diagnostics dosyaya göre gruplanıp ayrı
  `publishDiagnostics` ile gönderiliyor — kök neden #4 kapandı. `tests/lsp/`
  12 senaryo (`10_turkish_encoding`, `11_scoped_definition`,
  `12_cross_file_definition` yeni). Faz 4 tamam — completion token/sembol tabanlı yeniden kuruldu:
  `wordBefore`/`lineUpToCursor` string-hack'leri kaldırıldı; token-tabanlı
  bağlam çıkarma (`.` zinciri, `::` scope), structLayouts zincir çözümü
  (`a.b.c.`), scope filtrelemesi (başka fonksiyonun lokali önerilmez),
  builtin metodlar BuiltinMethodRegistry'den üretiliyor. `tests/lsp/` 16
  senaryo (`13_completion_scope`, `14_completion_dot_chain`,
  `15_completion_nonstruct_dot`, `16_completion_scope_method` yeni).
  LSP Faz 5–6 tamam (#84, 0.5.0 dalı) — `textDocument/rename` (çok dosyalı
  WorkspaceEdit: tanım + referanslar + import bağlayıcıları; symbolByOffset
  artık bildirimdeki TANIMLAYICI token'ı da indeksliyor — definitionLoc
  bildirim başını gösterir, `identOffsetFromDecl` düzeltir) ve
  `textDocument/signatureHelp` (token geri-taraması; kullanıcı fonksiyonları +
  `print` + `tip::metod(` builtin imzaları). Dayanıklılık: bozuk
  Content-Length/JSON/params sunucuyu düşürmez (doğrulama dispatch'te —
  nlohmann const `operator[]` eksik anahtarda ABORT eder, istisna değil;
  ayrıca sunucu döngüsünde istisna→InternalError backstop'u).
  `semanticTokens` ertelendi (opsiyoneldi). `tests/lsp/` 20 senaryo
  (`17_rename`, `18_rename_cross_file`, `19_signature_help`,
  `20_robustness` yeni; sürücüye bozuk-gövde için `__raw__` kaçış kapısı).
  Faz 5 tamam — IR satır tablosu %100 (IRGenerator::currentLoc_ +
  effectiveLoc ile tüm emit noktaları sourceLine dolduruyor); IRFunction::slotNames
  + Interpreter::slotName() gerçek değişken adlarını veriyor; runUntilEvent(maxInstr,
  startCallDepth) bütçeli/step'li koşu modeli; stepLine/stepOver/stepOut;
  lineToFirstIP breakpoint eşlemesi. Faz 6 tamam (protokol, testler, 67 test yeşil).
- **İlke:** Önce uçtan uca tek **dikey dilim**, sonra çerçeve. Erken soyutlamadan kaçın.

## Belge haritası
- `PLAN.md` (kök dizin) — 0.8.0/0.9.0 sıralı iş listesi (mevcut GitHub
  issue'ların özeti, yeni tasarım kararı içermez).
- `MIRPLAN.md` (kök dizin) — MIR entegrasyon tasarım belgesi: opcode→MIR
  eşleme tablosu, Value ABI (kutulama), GC shadow-stack somutlaştırma, hata
  yönetimi açık sorusu, çoklu-iş-parçacığı uyumu notu. Kod yazımından önce
  DUR-ve-sor kısıtı hâlâ geçerli.
- `docs/CLI.md` — CLI parametre formatı tasarımı: `--allow` (yalnızca
  permission/capability), GC bayrakları ayrı/değişmedi, servis bayrakları
  komuta özgü kalır (v3, onaylı).
- `readme.md` — toolbox çerçevesi, built-vs-planned, dil kimliği, çalıştırma modeli.
- `docs/kod-standardı.md` — C++ kod standardı (biçim, adlandırma, yorum, modern C++ kullanımı).
- `docs/fikirler.md` — ADR-001…005 (backend stratejisi, parser, header-only, token, IR).
- `docs/adr-frontend-analiz.md` — ADR-006…028 (frontend, analiz/optimizasyon,
  çalıştırma modeli, FFI, interface, bellek, **değer/referans semantiği, null
  güvenliği, mark-sweep GC, eşitlik, string, hata yönetimi, tip dönüşümü, switch-case,
  decimal**).
- `docs/adr/ADR-029-nested-struct-tahsis.md` — Nested struct alanları VarDecl anında
  özyinelemeli STRUCT_NEW zinciriyle tahsis edilir; referans paylaşımı iç içe de geçerli.
- `docs/adr/ADR-030-heavyir-lightir-ayrim.md` — heavyIR (full meta + alan adları) vs
  lightIR (sade opcode) ayrımı; `--optimized` bayrağıyla seçim.
- `docs/adr/ADR-031-modul-dongus-politikasi.md` — Modül döngüsü tespiti: `seen_` seti
  sonsuz döngüyü önler ama döngüde açık hata üretmez (TODO).
- `docs/adr/ADR-033-builtin-sozdizimi-ufcs.md` — Builtin reformu: UFCS nokta
  çağrısı (birincil) + array::/string::/struct:: ad alanları; alan gölgeleme;
  eski ElemTip::metod W006 ile v0.7.0'a kadar.
- `docs/adr/ADR-034-ffi-declaration-modeli.md` — Gömülü root.sqt'te `ffi` bildirim
  (imza+modül+sembolik host id+requires cap+unstable); import-gated stdlib
  (tırnaksız=modül, tırnaklı=dosya); sayısal host dispatch (HostFnId enum, O(1),
  print taşınır); `fs::`/`math::` çağrı sözdizimini eler (#107).
- `docs/adr/ADR-032-mir-jit-gomulu-runtime-aot.md` — İkinci backend: MIR JIT +
  gömülü-runtime AOT (`saqut build`); shadow stack GC kökleri; C transpile/libgccjit
  elendi, LLVM fiilen kapalı; VM referans backend, diferansiyel test zorunlu.
- `docs/sonnet-handoff.md` — **Sonnet için uygulama promptu** (ADR-020…024'ü koda
  döken sıralı görev planı; ilk görev: GC-hazır nesne modeli + array runtime).
- `docs/roadmap-frontend.md` — faz-faz uygulama planı (Faz 0–4 → fibonacci).
- `docs/kod/` — modül başına mimari dokümantasyon (15 belge + indeks).
- `docs/transkript-frontend-tasarim.md` — tasarım oturumu transkripti.
- `examples/fibonacci.sqt` — geçerli referans program.
- `examples/parser-stress/` — yalnızca parser'ı zorlayan, **geçerli olmayan** fixture'lar.

## GitHub issue yönetimi
- Repo: `github.com/abdussamedulutas/saqut` (GitHub). **Her zaman `gh` CLI kullan.**
  `scripts/gitea.py` ve git.saqut.com (Gitea) artık kullanılmıyor.
- **Issue işlemleri:** `gh issue list`, `gh issue create`, `gh issue edit`,
  `gh issue comment`, `gh label list`, `gh pr create` vb.
- Label eklemek için: `gh issue create --label "bug"` ya da
  `gh issue edit <N> --add-label "bug"`.
- **Issue yapısı:**
  - **#69–73** `faz-plani` — Faz 0–4 (Tip+Diagnostic, AST refactor, Symbol Table,
    Semantik Analiz, Optimizasyon). Format: Giriş/Gelişme/Sonuç-Başarı Kriterleri +
    mühendis-olmayan analiz.
  - **#74–98, #111** `fikir` — IR/VM tasarımı, modül/import, tip genişletmeleri
    (decimal/date/enum/string), FFI/builtin/stdlib, tooling (LSP/highlight/fmt),
    gelecek vizyonu (time-travel debug, WASM playground, test bloğu, paket yöneticisi).
    Format: Giriş/Gelişme/**Açık Sorular** (başarı kriteri YOK).
  - **#99–105** `test-senaryosu` — kaynak kod + beklenen çıktı içeren golden-test'ler.
  - **#106–110** `cli-ux`/`kalite-mimari` — CLI fikirleri, C/Java/Go tarzı tavsiyeler.
  - **#80–81** `enhancement`+`ir-vm` — ADR-032 backend işleri: MIR JIT (#80),
    gömülü-runtime AOT `saqut build` (#81).
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
- **Kod standardı:** `docs/kod-standardı.md`'ye uy. `.clang-format` biçimi otomatik
  uygular; adlandırma, yorum dili, `class`/`struct` ayrımı gibi kurallar el ile
  sağlanır. Yeni kod yazarken veya mevcut kodu değiştirirken bu standarda uy.
  Tüm yorumlar Türkçe, tüm tanımlayıcılar İngilizce. Header-only eğilimli
  (ADR-003), `#pragma once` değil `#ifndef` guard.
