# saQut — LSP/DAP Kurtarma Planı (Sonnet Handoff Promptu)

> **Bu belge bir yapay zekaya (Sonnet) verilecek görev promptudur.**
> Amaç: mevcut LSP'nin kronik eksiklerini semptom-yamama döngüsünden çıkarıp
> kök nedenlerden çözmek; ardından DAP'ı sağlam zemin üstüne yeniden kurmak.
>
> **ÇALIŞMA DİSİPLİNİ (ihlal etme):**
> 1. **Her oturumda YALNIZCA BİR faz uygula.** Faz bitmeden sonrakine geçme.
> 2. Faz sırası değiştirilemez — her faz bir öncekinin çıktısına dayanır.
> 3. Bir üst katmanda semptom görürsen ve kök neden alt katmandaysa, üst
>    katmana yama YAPMA; ilgili fazın işi olarak not düş (TODO + issue).
> 4. Mevcut 48 ctest testi her fazın sonunda %100 geçmeli. Her faz kendi
>    yeni testlerini ekler; test eklemeden faz "bitti" sayılmaz.
> 5. Kullanıcıyla tüm iletişim Türkçe. Commit mesajlarına Co-Authored-By /
>    Claude-Session satırı EKLEME.
> 6. `CLAUDE.md`'deki kilitli kararlara (ADR'ler) uy. Parser/TypeChecker'ın
>    dil semantiğini DEĞİŞTİRME — sadece hata toleransı ve raporlama kanalı
>    değişecek.

---

## Neden bu plan var? (Teşhis — önce oku)

Mevcut LSP (`src/lsp/`) özellik özellik yazıldı ama dört temel eksik yüzünden
her özellik kuma inşa edildi. Bugüne kadarki düzeltmeler (completion `.`
davranışı, `::` filtresi, E001 konumu…) hep semptomdu. Kök nedenler:

1. **LSP editör buffer'ını değil DİSKTEKİ dosyayı derliyor.**
   `document_store.cpp` → `runPipeline()` `state.content`'i tamamen yok
   sayıp `ModuleLoader::load(filePath)` çağırıyor; o da `std::ifstream` ile
   diskten okuyor (`module_loader.cpp:31`). `didOpen`/`didChange` ile gelen
   içerik hiçbir yerde kullanılmıyor. Kullanıcı kaydetmeden yazdığı sürece
   tüm diagnostics/hover/definition **bayat** — davranış "bazen doğru bazen
   yanlış" göründüğü için her seferinde farklı bir "eksik" sanılıyor.

2. **Parser hata-toleranssız ve hataları DiagnosticEngine'e vermiyor.**
   Sözdizimi hataları `std::cerr`'e yazılıyor (`parser.cpp:271,460,477,641`)
   ve `nullptr` dönülüyor; ModuleLoader konumsuz, jenerik bir
   "failed to parse module" hatası üretiyor. Editörde kod yazılırken çoğu
   anda kod sözdizimsel olarak geçersizdir → parse başarısız → sembol
   tablosu YOK → hover/definition/completion ölü. Ayrıca
   `runPipeline` ilk hatada erken çıkıyor (`document_store.cpp:62,67`):
   tek bir semantik hata bile tüm sembol servislerini kapatıyor.

3. **Konum sorgusu isim-eşleştirme hack'i, scope bilgisi yok.**
   `findSymbolAt` (`lsp_handler.cpp:139`) tüm sembolleri lineer tarayıp
   "satır eşit + sütun aralığı isim uzunluğu kadar" diye eşliyor. İki ayrı
   fonksiyondaki aynı adlı `x` karışır. Oysa `SymbolCollector` zaten her
   `IdentifierNode`'a `resolvedSymbol` yazıyor (`symbol_collector.cpp:556`)
   — konumdan token/AST düğümü bulunsa sembol scope-doğru gelirdi.
   Completion da AST değil satır-string'i kesip biçen yardımcılarla
   (`lineUpToCursor`/`wordBefore`) çalışıyor → `f().alan.` gibi zincirlerde
   asla çalışamaz, scope filtrelemesi yok (her sembol her yerde öneriliyor).

4. **Konum encoding'i ve çok-dosya URI'leri yanlış.**
   LSP varsayılan pozisyon birimi **UTF-16 code unit**'tir; `SourceLocation.column`
   ise byte sayıyor. Türkçe karakter (ç, ş, ü…) içeren satırlarda tüm
   range'ler kayar. Ayrıca `definition`/`references` her zaman sorgulanan
   belgenin URI'sini döndürüyor (`lsp_handler.cpp:176`) — import edilen
   modüldeki tanım yanlış dosyayı gösterir; diagnostics de dosyaya göre
   gruplanmadan tek URI'ye basılıyor.

**DAP tarafı** ise protokol düzeyinde yanlış kurulmuş — düzeltilerek değil,
sağlam zemin (Faz 5) üstüne **yeniden yazılarak** kurtarılır:
- DAP, JSON-RPC DEĞİLDİR. `sendEvent` (`dap_handler.cpp:17`) eventleri
  `{"jsonrpc":"2.0","method":"event",...}` zarfına sarıyor — VS Code bunları
  anlamaz; `stopped`/`terminated` hiç işlenmez, debugger asla "durdu" görünmez.
- Yaşam döngüsü ters: `initialized` eventi initialize **cevabından önce**
  gönderiliyor; `launch` içinde program derlenip hemen koşuluyor —
  `setBreakpoints`/`configurationDone` gelmeden. `stopOnEntry:false` ise
  program breakpoint kurulamadan bitmiş oluyor.
- `Interpreter::resume()` istek işleyicisi içinde programın sonuna kadar
  **bloklayarak** koşuyor; sonsuz döngüde DAP sunucusu kilitlenir, `pause` yok.
- IR satır tablosu yarım: `ir_generator.cpp`'de ~95 emit noktasından ~22'si
  `sourceLine` dolduruyor → satır bazlı breakpoint/step güvenilmez (#79).
- `slotName()` stub (`interpreter.hpp:69`, `""` döner); Variables paneli
  "slot[N]" + saçma bir sezgisel (`dap_handler.cpp:258-260`) gösteriyor.
  `stepOver()` gerçek implementasyon değil (`interpreter.cpp:101`).

---

## FAZ 0 — LSP Golden Test Altyapısı (kod düzeltmesi YOK)

**Amaç:** "3 kez döndük, yine eksik çıktı" döngüsünü kıran şey regresyon
testidir. Önce test altyapısı, sonra düzeltme.

**Yapılacaklar:**
1. `tests/lsp/` dizini aç. Test formatı: her senaryo bir `.jsonl` dosyası —
   satır başına bir istemci mesajı (LSP JSON-RPC gövdesi, Content-Length'siz).
   Yanına `.expected.jsonl` — sunucudan beklenen cevaplar (normalize edilmiş).
2. Bir test sürücüsü yaz (`tests/lsp/lsp_test_driver.cmake` veya küçük bir
   C++/shell aracı): senaryodaki mesajları Content-Length zarfıyla
   `saqut lsp` stdin'ine sırayla yaz, stdout'tan cevapları topla,
   normalize et (alan sırası bağımsız JSON karşılaştırması; `id` eşlemesi),
   `.expected.jsonl` ile karşılaştır. CTest'e kaydet
   (`cmake/` altındaki mevcut `run_golden.cmake` desenini örnek al).
3. İlk senaryolar (mevcut davranışı KİLİTLE, düzeltme yapma):
   - `initialize` → capabilities cevabı
   - `didOpen` (geçerli dosya) → publishDiagnostics boş
   - `didOpen` (E003 içeren dosya) → konumlu diagnostic
   - `hover`, `definition`, `documentSymbol` birer örnek
4. Bilinen-bozuk davranışlar için senaryoyu yaz, `DISABLED_` önekiyle veya
   CTest `WILL_FAIL` ile işaretle — sonraki fazlar bunları açacak.

**Başarı kriteri:** `ctest -R lsp` yeşil; en az 6 senaryo; README'ye
(`tests/lsp/README.md`) yeni senaryo ekleme tarifi yazılmış.

---

## FAZ 1 — Kaynak Overlay: LSP Buffer'dan Derleme

**Amaç:** Kök neden #1. Derleme girdisi artık editör buffer'ından gelir.

**Yapılacaklar:**
1. `ModuleLoader`'a bir kaynak sağlayıcı seam'i ekle:
   `std::function<bool(const std::string& path, std::string& out)>`
   (veya `SourceOverlay` map'i: `path → content`). `loadUnit` önce
   overlay'e bakar, yoksa diske düşer. Varsayılan davranış (CLI `run`)
   değişmez.
2. `DocumentStore::runPipeline` overlay'e **açık tüm belgeleri** koyar
   (`store_`'daki her URI → path → content). Böylece A.sqt, B.sqt'yi import
   ediyorsa ve B de editörde açıksa B'nin kaydedilmemiş hali görülür.
3. `uriToPath`'i tek yardımcıya çıkar, tersi `pathToUri`'yi de ekle
   (Faz 3'te lazım).
4. Faz 0'daki senaryolara ekle: `didOpen` içeriği ile diskteki içerik
   FARKLI olsun (testte diske kasıtlı eski sürüm yaz) → diagnostics
   buffer'a göre gelmeli. `didChange` sonrası diagnostics güncellenmeli.

**Başarı kriteri:** "diskte hatalı / buffer'da düzeltilmiş" senaryosu geçer;
48 ctest + Faz 0 testleri yeşil.

---

## FAZ 2 — Parser Hata Toleransı + Kesintisiz Pipeline

**Amaç:** Kök neden #2. Kod yazılırken de sembol servisleri yaşasın.

**Yapılacaklar:**
1. Parser'a `DiagnosticEngine*` enjeksiyonu ekle (opsiyonel; null ise eski
   `cerr` davranışı korunur — CLI kırılmasın). Tüm `std::cerr << "parser
   error..."` noktaları konumlu (`SourceLocation`) diagnostic üretsin
   (yeni kod ailesi: `E9xx` sözdizimi hataları — mevcut E-kodlarıyla çakışma).
2. **Panic-mode recovery:** statement seviyesinde hata görünce `;` veya `}`
   veya satır başındaki bilinen statement-başlangıç token'ına (if/for/while/
   return/tip-adı…) kadar atla, `ErrorNode` (yeni ASTKind) bırak, parse'a
   devam et. Parser artık "ilk hatada nullptr" değil, "hatalı bölgeleri
   ErrorNode olan tam AST" döndürür. `parser.cpp:70`'teki sonsuz-döngü
   koruması korunur.
3. `SymbolCollector`/`TypeChecker`/`StructuralValidator` `ErrorNode`'u
   sessizce atlar (crash yok, ikincil hata üretme).
4. `DocumentStore::runPipeline`'daki erken `return`'leri kaldır: hata olsa
   da SymbolCollector'a kadar in; TypeChecker yalnızca parse tamamen
   çökmüşse atlanır. `DocumentState`'e `lastGoodSymbolTable` ekle: pipeline
   sembol üretebildiyse güncellenir; üretemediyse LSP sorguları son iyi
   tabloya düşer.
5. Test: içinde 1 sözdizimi hatası olan dosyada (a) hata konumlu diagnostic
   olarak gelir, (b) hatanın DIŞINDAKİ fonksiyon için hover/definition hâlâ
   çalışır. Faz 0'daki `DISABLED_` senaryolarından ilgili olanları aç.

**Başarı kriteri:** yukarıdaki iki senaryo + tüm eski testler yeşil.
`saqut check` CLI çıktı formatı değişmemiş (golden testler kanıtlar).

---

## FAZ 3 — Konum Doğruluğu: Encoding, Token-Tabanlı Sorgu, Çok-Dosya URI

**Amaç:** Kök neden #3 ve #4. "Doğru yerde doğru sembol."

**Yapılacaklar:**
1. **Pozisyon encoding:** `initialize`'da istemcinin
   `general.positionEncodings`'ine bak; `utf-8` destekliyorsa capabilities'e
   `"positionEncoding":"utf-8"` yaz ve dönüşüm yapma. Desteklemiyorsa
   (varsayılan UTF-16) `DocumentState.content` üzerinden satır bazlı
   byte↔UTF-16 dönüştürücü yardımcılar yaz (`src/lsp/position.hpp`):
   `lspToByteCol(content, line, utf16Col)` ve tersi. **Tüm** handler'lar
   konum çevirisini bu iki yardımcıdan geçirir — elle `+1/-1` hesabı kalmaz.
2. **findSymbolAt'ı yeniden yaz:** `DocumentState.tokens` zaten duruyor
   (yoksa pipeline'da sakla). Verilen offset'e düşen token'ı bul (binary
   search, `SourceLocation.offset` alanı var). Token identifier ise AST'de
   o konumdaki `IdentifierNode`'u bul ve `resolvedSymbol`'ünü döndür
   (SymbolCollector `symbol_collector.cpp:556`'da dolduruyor). İsim-uzunluğu
   aralık eşleştirmesini ve `allSymbols` lineer taramasını sil.
3. **Çok-dosya URI:** `definition`/`references`/`documentSymbol` sonuç
   URI'sini `sym->definitionLoc.filePath` / `ref.filePath`'ten üret
   (`pathToUri`). Sorgulanan URI'yi kopyalama.
4. **Diagnostics'i dosyaya göre grupla:** `publishDiagnostics` çağrısından
   önce `diag` kayıtlarını `loc.filePath`'e göre ayır; her dosyanın
   diagnostics'i kendi URI'sine gider (import edilen modülün hatası ana
   dosyada görünmesin).
5. Test: Türkçe karakterli satırda hover/definition range'i doğru; iki
   fonksiyonda aynı adlı değişkende definition doğru olanı bulur;
   import edilen sembolde definition öteki dosyanın URI'sini döndürür.

**Başarı kriteri:** bu üç senaryo + tüm eski testler yeşil.

---

## FAZ 4 — Completion'ı Token/Sembol Tabanlı Yeniden Kur

**Amaç:** String-hack completion'ı (`lineUpToCursor`/`wordBefore`) kaldır.

**Yapılacaklar:**
1. İmleç öncesi bağlamı **token dizisinden** çıkar: son anlamlı token'lara
   bak (`IDENT . │`, `IDENT :: │`, `IDENT . IDENT . │` zinciri).
   Zincir çözümü: ilk identifier'ı Faz 3'teki konum sorgusuyla çöz,
   sonra alan tiplerini `structLayouts` üzerinden yürüt — böylece
   `a.b.c.` de çalışır. Çözülemiyorsa boş liste dön (uydurma önerme).
2. **Scope filtrelemesi:** yalnızca imlecin bulunduğu scope'tan görünür
   semboller öner. `SymbolTable`'da scope'ların kaynak aralığı yoksa,
   pratik yaklaşım: imlecin içinde olduğu fonksiyonu AST'den bul
   (`FunctionDecl.loc` aralığı), o fonksiyonun lokalleri + parametreleri +
   globaller + fonksiyon/struct/enum adları. Başka fonksiyonun lokali
   ASLA önerilmez.
3. Builtin metod listesi (`::`) elle yazılmış tablo olarak kalabilir ama
   `src/builtin/` içindeki gerçek kayıt kaynağından üretilebiliyorsa oradan
   üret (tek doğruluk kaynağı). Üretilemiyorsa tabloyu builtin kayıtlarıyla
   karşılaştıran bir birim test ekle (tablo bayatlarsa test kırılsın).
4. Test: scope filtre senaryosu, `a.b.` zincir senaryosu, `x.` (struct
   olmayan) boş dönüş senaryosu, `::` tip filtresi senaryosu.

**Başarı kriteri:** 4 yeni senaryo + tümü yeşil. `wordBefore`/`lineUpToCursor`
silinmiş.

---

## FAZ 5 — DAP Zemini: IR Satır Tablosu + VM Debug API (protokol YOK)

**Amaç:** DAP'a dokunmadan önce zemini bitir. Bu faz yalnızca IR + VM.

**Yapılacaklar:**
1. **IR satır tablosu %100:** `ir_generator.cpp`'deki TÜM instruction üretim
   noktaları (~95) `sourceLine/sourceCol/sourceFile` doldursun. Pratik yol:
   `IRGenerator`'a `currentLoc_` üyesi + `emit()` tek kapısı; her
   `generateStatement/Expression` girişinde `currentLoc_ = node->loc`.
   Doğrulama testi: örnek programın IR'ında `sourceLine==0` olan
   instruction sayısı 0 (fonksiyon prolog/epilog istisnaları açıkça
   listelenir).
2. **Slot→isim debug tablosu:** `IRFunction`'a
   `std::vector<std::string> slotNames` (veya `slot→{name,typeStr}` map)
   ekle; IR üretimi sırasında `SymbolTable`'dan doldur.
   `Interpreter::slotName()` stub'ını buna bağla; frame'deki canlı slot
   sayısı da fonksiyondan okunur (16'lık tahmin silinir).
3. **VM koşu modeli:** `run()`'ı "instruction budget" ile çağrılabilir yap:
   `runUntilEvent(maxInstructions)` → dönüş nedeni: Breakpoint / StepDone /
   Finished / BudgetExhausted. Böylece DAP döngüsü bloklanmaz, `pause`
   uygulanabilir olur.
4. **Satır bazlı adımlar:** `stepLine()` (sourceLine değişene kadar ilerle),
   `stepOver()` (satır değişene kadar VE callDepth ≤ başlangıç derinliği),
   `stepOut()` (callDepth < başlangıç). Breakpoint eşlemesi: her (file,line)
   için o satırın İLK instruction'ı (satır tablosundan önceden hesaplanan
   `line→ip` index'i).
5. Test: golden IR testine satır tablosu ekle; VM adım API'leri için birim
   test (küçük program üzerinde: satırlar sırayla, stepOver çağrıyı atlar,
   breakpoint doğru satırda durur, slotName gerçek adı verir).

**Başarı kriteri:** yeni birim testler + tüm eski testler yeşil. Issue #79'a
ilerleme yorumu yaz.

---

## FAZ 6 — DAP Protokolünü Doğru Formatla Yeniden Yaz

**Amaç:** `dap_handler`/`dap_server`'ı gerçek DAP telgraf formatıyla kur.

**Yapılacaklar:**
1. **Telgraf formatı:** DAP mesajları `{"seq":N,"type":"request|response|event",...}`.
   Cevap: `{"type":"response","request_seq":<istek seq>,"success":bool,
   "command":...,"body":{...}}`. Event: `{"type":"event","event":"stopped",
   "body":{...}}`. `jsonrpc`/`method`/`params` zarfı TAMAMEN kalkar
   (Content-Length çerçevesi aynı kalır, `JsonRpc::readMessage/writeMessage`
   yeniden kullanılabilir).
2. **Yaşam döngüsü:** `initialize` isteğine ÖNCE response dön, SONRA
   `initialized` eventi gönder. `launch`: derle + VM'i kur ama **çalıştırma**;
   response dön. `setBreakpoints` VM kurulmuşsa uygular (satır doğrulamasını
   Faz 5'teki `line→ip` index'iyle yap; eşleşmeyen satıra `verified:false`).
   `configurationDone` gelince: `stopOnEntry` ise ilk satırda durup
   `stopped(reason:"entry")` gönder; değilse koşuya başla.
3. **Koşu döngüsü:** `continue`/`step*` istekleri `runUntilEvent` budget
   döngüsüyle çalışır; her budget turu arasında stdin'de bekleyen istek var
   mı bak (`pause` desteği). Program bitince `terminated` + `exited` eventleri.
4. **stackTrace/variables:** frame başına kendi `sourceFile/line`
   (`currentSourceFile()`'ı frame-parametreli yap); Variables paneli
   Faz 5'teki `slotNames` ile gerçek değişken adları ve tipleri; struct/array
   için `variablesReference` ile genişletme (çocuk alanlar) — ilk sürümde
   tek seviye yeterli.
5. **DAP golden test altyapısı:** Faz 0'daki sürücünün DAP varyantı
   (`tests/dap/`): senaryo = istek dizisi, beklenen = response+event dizisi
   (seq'ler normalize edilir). Asgari senaryolar: launch→configurationDone→
   entry'de durma; breakpoint'te durma; step ile satır ilerleme; variables'ta
   gerçek isim; sonsuz döngüde `pause`.
6. VS Code eklentisinde (`editor/vscode/`) DAP kaydını doğrula
   (`DebugAdapterExecutable('saqut',['dap'])`) — TOOLING-PLAN.md'deki
   `package.json` "debuggers" bölümü uygulanmamışsa uygula.

**Başarı kriteri:** DAP golden senaryoları + tüm eski testler yeşil;
VS Code'da F5 → entry'de durur, breakpoint çalışır, Variables gerçek
adları gösterir, F10 satır satır ilerler.

---

## Faz Bitiş Kontrol Listesi (her fazın sonunda uygula)

- [ ] `cmake -B build && ninja -C build` temiz derleniyor (uyarı artışı yok)
- [ ] `ctest` %100 (eski 48 + yeni fazın testleri)
- [ ] Bu faza ait `DISABLED_`/`WILL_FAIL` senaryoları açıldı ve geçiyor
- [ ] Geçici/eksik bırakılan her şey kodda `// TODO(fazN):` ile işaretli
- [ ] `CLAUDE.md` "Mevcut durum" bölümü güncellendi (1-2 satır)
- [ ] Kullanıcıya Türkçe kısa özet: ne değişti, hangi senaryolar artık çalışıyor
