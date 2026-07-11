# saQut — LSP/DAP Kurtarma Planı: KALAN İŞLER (Sonnet Handoff Promptu)

> **Bu belge bir yapay zekaya verilecek görev promptudur.**
> Orijinal plan Faz 0–6'ydı ve **tamamlandı** (aşağıdaki özet). Bu belge yalnızca
> doğrulama sırasında tespit edilen **kalan işleri** içerir: Faz 7–9.
>
> **ÇALIŞMA DİSİPLİNİ (ihlal etme):**
> 1. **Her oturumda YALNIZCA BİR faz uygula.** Faz bitmeden sonrakine geçme.
> 2. Mevcut 67 ctest testi her fazın sonunda %100 geçmeli. Her faz kendi
>    yeni testlerini ekler; test eklemeden faz "bitti" sayılmaz.
> 3. Kullanıcıyla tüm iletişim Türkçe. Commit mesajlarına Co-Authored-By /
>    Claude-Session satırı EKLEME.
> 4. `CLAUDE.md`'deki kilitli kararlara (ADR'ler) uy.
> 5. DAP test istemcisi yazarken **Content-Length BAYT sayar, karakter değil**
>    — fixture yolları Türkçe karakter içeriyor (`Masaüstü`), karakter sayan
>    parser tam burada desenkronize olur ve "event gelmedi" yanılgısı yaratır.

---

## Tamamlananlar (Faz 0–6 — DOKUNMA, sadece bağlam)

- **Faz 0:** `tests/lsp/` golden altyapısı (`lsp_test_driver.py`), 16 senaryo.
- **Faz 1:** Kaynak overlay — LSP editör buffer'ından derliyor (`SourceOverlay`).
- **Faz 2:** Parser hata toleransı — `E9xx` + panic-mode recovery + `ErrorNode`;
  pipeline erken çıkmıyor.
- **Faz 3:** Konum doğruluğu — `position.hpp` UTF-16↔byte, token binary search,
  çok-dosya URI, dosyaya göre gruplanmış diagnostics.
- **Faz 4:** Completion token/sembol tabanlı; string-hack'ler silindi.
- **Faz 5:** IR satır tablosu %100, `slotNames`, `runUntilEvent` bütçe modeli,
  `stepLine/stepOver/stepOut`, `lineToFirstIP`.
- **Faz 6:** DAP doğru telgraf formatı + yaşam döngüsü; `tests/dap/` 3 golden test.
- 67/67 ctest yeşil. VS Code eklentisi (`editor/vscode/`) LSP + DAP kayıtlı.

**Canlı testle doğrulanmış durum (bu belge yazılırken):**
- `continue` → program bitince `exited` + `terminated` event'leri **GELİYOR**.
  (`dap_handler.cpp` baş kısmındaki `TODO(faz6)` yorumu BAYAT — muhtemelen
  bayt/karakter karışan bir test istemcisi yüzünden "gelmiyor" sanıldı. Bkz.
  disiplin maddesi 5.)
- Breakpoint, launch'taki `program` ile `setBreakpoints`'taki `source.path`
  **bire bir aynı mutlak yol** olduğunda çalışıyor ve `stopped(breakpoint)`
  geliyor.

---

## KALAN KUSURLAR (teşhis — canlı testle kanıtlı)

1. **`print` çıktısı DAP stream'ini kirletiyor (EN KRİTİK).**
   `interpreter.cpp:942` — CALLHOST `print` doğrudan `std::cout`'a yazıyor.
   DAP modunda protokol da stdout'ta: program çıktısı Content-Length
   çerçevelerinin ARASINA çıplak bayt olarak giriyor (kanıt: `continue`
   response'u ile `exited` event'i arasında çerçevesiz `3` baytı gözlendi).
   VS Code Debug Console'da program çıktısı hiç görünmez; istemci parser'ı
   bozulabilir.

2. **`stopOnEntry` yok sayılıyor.** `handleConfigurationDone` launch
   argümanlarına hiç bakmıyor; `stopOnEntry:false` gönderilse de her zaman
   `stepInstruction()` + `stopped(entry)` yapıyor. Plan: false ise doğrudan
   koşuya başlamalı.

3. **`setBreakpoints.verified` sahte.** `dap_handler.cpp:278-279`:
   `verified = (line > 0)` — Faz 5'te yazılan `lineToFirstIP` index'ine hiç
   bakılmıyor. Yorum satırı olan/boş satıra konan breakpoint "verified"
   görünüyor ama asla vurulmuyor. Ayrıca yol eşleşmesi ham string: launch
   `program`'ı göreli/symlink'li gelirse `source.path` (mutlak) ile eşleşmez.

4. **`pause` gerçek değil.** `handlePause` sadece cevap dönüyor
   (`dap_handler.cpp:389-391`); `runWithBudget()` bütçeyi `-1` (sınırsız)
   veriyor — sonsuz döngülü programda `continue` handler'ı bloklar, DAP
   sunucusu kilitlenir. Faz 6 planındaki "budget turu arasında stdin'e bak"
   hiç uygulanmadı.

5. **DAP golden kapsamı asgarinin altında.** Mevcut 3 senaryo: initialize,
   launch→configurationDone→entry, stackTrace/variables. Planın asgarisinden
   eksik olanlar: breakpoint'te durma, step ile satır ilerleme, sonsuz
   döngüde pause, bitişte exited/terminated, output event.

6. **Kapanış ritüeli eksik.** Issue #79 hâlâ açık ve yorumsuz (Faz 5'in
   kriteriydi); `dap_handler.cpp:7-11`'deki bayat `TODO(faz6)` yorumları
   duruyor.

---

## FAZ 7 — DAP Davranış Düzeltmeleri (output, stopOnEntry, verified)

**Amaç:** Kusur 1–3'ü kapat. Protokol değişikliği yok, davranış düzeltmesi var.

**Yapılacaklar:**
1. **Output sink seam'i:** `Interpreter`'a bir çıktı kancası ekle
   (`std::function<void(const std::string&)> outputSink_`; varsayılan
   `std::cout` davranışı — CLI `run`/`exec` DEĞİŞMEZ, golden testler kanıtlar).
   CALLHOST `print` (interpreter.cpp:942) `outputSink_` üzerinden yazsın.
   `DapHandler::handleLaunch` sink'i bağlar:
   `sendEvent("output", {{"category","stdout"},{"output", text}})`.
2. **stopOnEntry:** `handleLaunch`'ta `args.value("stopOnEntry", false)`'u
   üye değişkende sakla. `handleConfigurationDone`: true ise mevcut davranış
   (`stepInstruction()` + `stopped(entry)`); false ise response'tan sonra
   doğrudan `runWithBudget()` çağır (program breakpoint'e çarpar ya da biter).
3. **Gerçek verified + yol kanonikleştirme:**
   - Tek yardımcı: `canonicalPath(p)` (`std::filesystem::weakly_canonical`).
     `handleLaunch` program yolunu, `handleSetBreakpoints` `source.path`'i,
     `Interpreter::setBreakpoint/isBreakpoint` karşılaştırmaları bundan geçirir.
   - `verified`: VM kuruluysa Faz 5'in `lineToFirstIP` index'inde
     `(file,line)` var mı diye bak; yoksa `verified:false` dön (VS Code
     içi boş daire gösterir — doğru davranış).
4. Bayat `TODO(faz6)` yorumlarını (dap_handler.cpp:7-11) sil — davranışlar
   bu fazda testle kanıtlanmış olacak.
5. **Golden testler (`tests/dap/`):**
   - `04_output_event`: print'li program → `output` event'i, çerçeve dışı
     bayt YOK (sürücü çerçeve dışı baytı hata saysın — `dap_test_driver.py`'a
     bu kontrolü ekle).
   - `05_no_stop_on_entry`: `stopOnEntry:false` → `stopped(entry)` YOK,
     program akar.
   - `06_breakpoint_hit`: breakpoint kur → `stopped(breakpoint)` → continue →
     `exited`+`terminated` (bayat TODO'nun yerine geçen regresyon kilidi).
   - `07_breakpoint_unverified`: koda denk gelmeyen satıra bp →
     `verified:false`.

**Başarı kriteri:** 4 yeni DAP senaryosu + tüm eski testler yeşil.
`saqut run` CLI çıktısı değişmemiş (mevcut golden testler kanıtlar).

---

## FAZ 8 — Gerçek `pause`: Non-Blocking Koşu Döngüsü

**Amaç:** Kusur 4. Sonsuz döngüde DAP sunucusu kilitlenmesin, `pause` çalışsın.

**Yapılacaklar:**
1. `runWithBudget()`'ı gerçek bütçe döngüsüne çevir: `runUntilEvent(-1,-1)`
   yerine örn. `runUntilEvent(100000, -1)` turları. Her `BudgetExhausted`
   turu arasında stdin'de bekleyen mesaj var mı bak (poll/select veya
   `JsonRpc`'ye non-blocking `tryReadMessage`).
2. Bekleyen mesaj `pause` ise: koşuyu bırak, `stopped(reason:"pause")` gönder,
   `pause` isteğine response dön (DAP sırası: response önce — pause response'u
   stopped event'inden ÖNCE yazılmalı). Diğer istekler (ör. `threads`)
   kuyruklanıp koşu bitince/durunca işlenir — basit bir `std::deque` yeter.
3. `handlePause`'daki yer tutucuyu gerçek uygulamayla değiştir;
   `runWithBudget`'taki `BudgetExhausted → stopped(pause)` yer tutucusunu da
   (artık bütçe turu içeride işlendiği için) düzelt.
4. **Golden test:** `08_pause_infinite_loop`: sonsuz döngülü fixture
   (`while(true){}`) → continue → pause → `stopped(pause)` → disconnect
   temiz kapanır (timeout'a düşmez).

**Başarı kriteri:** pause senaryosu + tüm eski testler yeşil; sürücüde
timeout güvencesi (kilitlenme = test hatası).

---

## FAZ 9 — Kapsam Tamamlama + Kapanış Ritüeli

**Amaç:** Kusur 5–6. Orijinal planın asgari senaryo listesi tamamlanır, borçlar
kapatılır.

**Yapılacaklar:**
1. **Golden test:** `09_step_lines`: `next` (stepOver) ile satır satır ilerleme
   — ardışık `stopped(step)` event'lerinde `stackTrace` satırları artıyor;
   çağrı içeren satırda `next` çağrının İÇİNE girmiyor (stepOver semantiği).
2. **Golden test:** `10_variables_struct`: struct/array içeren programda
   `variables` → `variablesReference` ile tek seviye çocuk alanlar geliyor
   (alan adları gerçek).
3. **Issue #79'u kapat:** Faz 5–9'da ne yapıldığının özet yorumu + kapat
   (`gh issue close 79 --comment "..."`). Kalan bilinçli sınırlamalar varsa
   (ör. tek thread, column hep 0) yeni küçük issue'lara ayır.
4. `CLAUDE.md` "Mevcut durum" bölümünü güncelle: kurtarma planı Faz 0–9 TAMAM;
   `tests/lsp` 16 + `tests/dap` 10 senaryo.
5. **Elle doğrulama (kullanıcıyla birlikte):** VS Code'da F5 →
   `stopOnEntry:true` ile entry'de durur; breakpoint vurur; F10 satır satır;
   Variables gerçek adlar; Debug Console'da `print` çıktısı görünür;
   sonsuz döngüde pause butonu çalışır. Sonucu kullanıcıya sor, belgeye işle.

**Başarı kriteri:** `tests/dap/` ≥ 10 senaryo, tüm ctest yeşil, #79 kapalı,
elle doğrulama checklist'i işaretli.

---

## Faz Bitiş Kontrol Listesi (her fazın sonunda uygula)

- [ ] `cmake -B build && ninja -C build` temiz derleniyor (uyarı artışı yok)
- [ ] `ctest` %100 (eski 67 + yeni fazın testleri)
- [ ] Geçici/eksik bırakılan her şey kodda `// TODO(fazN):` ile işaretli
- [ ] `CLAUDE.md` "Mevcut durum" bölümü güncellendi (1-2 satır)
- [ ] Kullanıcıya Türkçe kısa özet: ne değişti, hangi senaryolar artık çalışıyor
