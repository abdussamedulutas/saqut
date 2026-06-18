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
git'te izlenir.

## Kilitli kararlar (değiştirme — gerekçeler ADR'lerde)
- **Çalıştırma modeli: IR + bytecode VM (yorumlayıcı döngü).** Tree-walker DEĞİL,
  gerçek makine-kodu JIT DEĞİL (kapsam dışı; öncelik determinizm + incelenebilirlik,
  ham hız değil). C'ye transpile ileride geçerli 2. backend. İleride makine kodu
  gerekirse libgccjit/LLVM'e bağlanılır (çok uzak). Bellek = host C++ heap; özel
  allocator yok. (ADR-015)
- **Dil kimliği:** prosedürel, C-ailesi sözdizimi, value semantics, zorunlu
  class/main boilerplate yok. **Yok:** class/OOP, closure, generic, kullanıcı
  pointer'ı (`*`/`&`), auto/tip çıkarımı, gizli int↔float (tek istisna sabit
  folding). **Var:** struct, tipli fonksiyonlar, array (`int[]`). `interface`
  **ertelendi** (reddedilmedi, ADR-018).
- **Analiz vs Optimizasyon:** Analiz orijinal AST üstünde annotation; optimizasyon
  **klon** üstünde dönüşüm. `ASTNode::clone()` yük taşıyan merkezi bileşen
  (parent pointer'lar + sembol tablosu remap edilir, ADR-007). Fixpoint döngüsü +
  iterasyon tavanı (`maxFixpointRounds`, ADR-009).
- **Literal/tip kuralı:** tamsayı literali bağlama-göre tiplenir (`float x = 1;`
  geçerli; `int y = 1.5;`→E003; değişken→değişken gizli dönüşüm yok). Döngüsel
  by-value struct → E010. (ADR-010/011)
- **FFI seam:** kasıtlı "host fonksiyonu çağır" mekanizması (`callhost`); `print`
  ilk müşteri (ADR-016). Batteries = sınır/FFI problemi, "zlib'i yeniden yaz"
  değil; kripto asla elle yazılmaz (ADR-017).

## Mevcut durum (yapılan vs planlanan)
- **Çalışıyor:** lexer, tokenizer, Pratt parser, AST, AST'nin JSON serileştirmesi,
  CLI iskeleti (`tokens`/`ast`/`symbols`/`run`), konum takibi, basit aritmetiği
  düşüren minimal IR deneyi.
- **Planlı (henüz YOK):** sembol tablosu, semantik analiz, tip sistemi, diagnostic
  motoru, optimizasyon, IR+bytecode VM ile çalıştırma.
- **Birinci kilometre taşı ("bitti" tanımı):** `examples/fibonacci.sqt`
  (recursive + iterative) derlenip çalıştırılabilmeli.
- **İlke:** Önce uçtan uca tek **dikey dilim**, sonra çerçeve. Erken soyutlamadan kaçın.

## Belge haritası
- `readme.md` — toolbox çerçevesi, built-vs-planned, dil kimliği, çalıştırma modeli.
- `docs/fikirler.md` — ADR-001…005 (backend stratejisi, parser, header-only, token, IR).
- `docs/adr-frontend-analiz.md` — ADR-006…019 (frontend, analiz/optimizasyon,
  çalıştırma modeli, FFI, interface, bellek).
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
- Commit mesajları sonunda: `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`.
- Ana dal `master`; commit/push kullanıcı isteyince yapılır.
- Wiki API'si Gitea'da REST üzerinden çalışmadı; wiki içeriği `wiki.md`'ye yazılıp
  kullanıcı tarafından elle yapıştırılıyor.
