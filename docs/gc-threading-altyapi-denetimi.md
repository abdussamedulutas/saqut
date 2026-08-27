# GC / Threading Altyapı Denetimi — Tespit Kaydı

Tarih: 2026-08-27 · Revizyon: 8ee4879 (branch 0.9.6) · Amaç: 0.9.6–0.9.8
sürümlerinin konusu threading + GC (ürün sahibi kararı). Bu belge altyapı
denetiminin kanıtlı tespitlerini saklar; karar kaydı değildir, tüm maddeler
"doğrulandı" durumundadır (kaynak okunarak, dosya:satır çapalı).

Kapsam tartışması: AGENTS.md §9 "kapsamlı optimizer"ı 0.9.x/1.0.0'a gizlice
eklenmez sayar. Bu belgedeki işler GC/threading önkoşulu olan hedefli
analizlerdir (liveness, slot-DCE); optimizer paketi değil. Sınır ürün
sahibinindir.

## A. Kanıtlı tespitler (2026-08-27 taraması)

### A1. Mükemmel GC'ye engeller
1. **String'in iki dünyası:** VM string'i `Value` içinde inline `std::string`
   (`src/vm/value.hpp:48`) — GC görmez/toplamaz. JIT'te kutulu `StringObject`
   ama VM Heap'ine bağlı değil; `g_jitRuntimeStrings`'te birikir (ölçülmüş:
   200k concat → JIT 21,8 MB / VM 6,8 MB; kayıt `src/vm/object.hpp:211-239`).
2. **Value şişman struct (union değil):** kind+int+double+DecimalValue+
   std::string+Object*+int64 yan yana (`value.hpp:43-50`); ~100+ bayt, her
   kopya tüm alanları (string varsa deep-copy) taşır.
3. **Kök kümesi Interpreter'a gömülü:** Heap kökleri bilmez; işaretleme
   `maybeCollect` içinde (`object.hpp:188-191`, `interpreter.cpp:329-337`).
   Declaratif kök-listesi (root provider) yok.
4. **`marked` + `markState` aynı bilgi iki alanda** (`object.hpp:48-49`) —
   #217 sonrası `marked` türetilebilir; §10.2 ihlali.
5. **Taşımsızlık JIT view'a gömülü:** `ArrayObject::jitData` ham buffer
   adresini sabit offset'ten yükler (`object.hpp:81-106`) → moving/compacting
   GC yapısal olarak yasak. Ya view-geçersiz kılma mekanizması ya kalıcı
   kısıt ilanı gerekir.
6. **Heap tek yönlü liste + new/delete:** O(1) tekil çıkarma yok
   (`object.hpp:198`), freeliste/arena yok; sweep belleği doğrudan sisteme
   iade eder. agc/move/per-thread heap için O(1) çıkarma + heap-ait allocator
   gerekir.

### A2. Stack/değer yönetimini zorlaştıranlar
1. **Slot liveness bilgisi YOK:** `IRFunction` slotCount/slotNames/slotTypes
   tutar (`src/ir/ir_function.hpp:36-40`); "bu IP'te hangi slot canlı"
   bilinmez. Frame'in tüm slot'ları kök sayılır (`interpreter.cpp:331`).
   CFG yapısı var (`src/ir/ir_cfg.hpp:26-34`) ama tek tüketicisi `--ir`
   dump'ı (`src/cli/commands/ir.hpp:60`); hiç dataflow pass'i yok.
2. **Frame başına `std::vector<Value>` tahsisi** (`src/vm/call_frame.hpp:39`);
   frame havuzu yok.
3. **Argüman/atama Value kopyası** — string'li kopya string'i de kopyalar
   (A1.2'nin sonucu).

### A3. Multithreading engelleri
| Engel | Kanıt | Çare |
|---|---|---|
| `jitShadowStack()` süreç-singleton | `src/vm/shadow_stack.hpp:37,76-78` | thread_local (şimdi ucuz) |
| JIT global mutable state (`g_jitHeap`, `g_jitPendingError`, `g_jitGlobalI/D/P`, `g_jitTraceStack`, `g_jitStructMeta`…) | `src/mir/mir_backend.cpp:56-99,395,457-459` | Context/Interpreter'e indirme |
| `globalSlots_` program-çapında tek flat dizi | `src/vm/interpreter.hpp:118-126` | sahiplik ürün kararı (K2) |
| `FileRegistry::instance()` singleton | `src/core/file_registry.hpp:63-66` | sınır kaydı (compile-time) |
| `sys_random` static `mt19937_64` | `src/ffi/functions/sys.cpp:18` | thread_local / seed |
| `new/delete` global malloc | tüm Heap tahsisleri | per-heap arena (A1.6) |

Olumlu: `Heap` instance + kopyası silinmiş (`object.hpp:282-284`),
Interpreter çağrı-başına, host scratch Interpreter içinde
(`interpreter.hpp:164-172`) — VM çekirdeği izolasyon-dostu; kir JIT katmanında.

### A4. Performans engelleri (etki sırasıyla)
1. Value kopya maliyeti (sıcak yol; interpreter.cpp'te 193 `slots[...]`
   erişimi).
2. ArrayObject 7 paralel vektör (`object.hpp:73-79`) — union değil; nesne
   başına şişkinlik, cache-dostu değil.
3. `virtual markChildren` — nesne başına vptr + mark'ta sanal dispatch;
   type-etiket + switch daha hızlı ve vptr'i kaldırır.
4. Frame başına vector tahsisi (A2.2).
5. Mark sırasında geçici `Value::fromRef` (`interpreter.cpp:337`) — önemsiz.

### A5. CFG'nin mevcut kalitesi (standart CFG değil)
- `buildCFG` lider-bazlı bölme + preds/succs (`src/ir/ir_cfg.cpp:55-118`);
  **yok:** dominance tree, natural loop/back-edge tespiti, unreachable-block
  temizliği, critical edge splitting, SSA, block yeniden sıralama
  (linearize orijinal sırayı korur, `ir_cfg.hpp:91-118`).
- Kenar çözümü `startIndex` lineer taramasıyla (`ir_cfg.cpp:84-93`).
- Pipeline'da canlı tüketicisi yok; dump için on-demand kurulur.

### A6. Mevcut optimizasyon konumu
ConstantFolding + DeadCodeElim **AST seviyesinde**, fixpoint ile
(`src/opt/optimization_manager.hpp`; run/ir komutlarında `runPassesInPlace`).
IR/CFG seviyesinde pass yok.

## B. Ürün kararları (verildi / bekliyor)

### Verilen kararlar (bu oturum, ürün sahibi)
- **nogc = dinamik kapsamlı:** bastırma imzada değil yığında yaşar; nogc
  frame'i kapanınca, atası nogc değilse ertelenen sweep çalışır. `nogc`'nin
  bastırması callee'lere dinamik olarak sızar (bilinçli karar).
- **`gc_collect()`** callhost'tur (print gibi); çağrı anında tam mark+sweep,
  bastırma derinliğinden bağımsız. `delete/free(x)` tarzı tekil-nesne silme
  REDDEDİLDİ (zamanlama sözü ver, erişilebilirlik verme).
- Kök semantiği aşama 1 muhafazakâr (slot durdukça canlı), aşama 2 liveness.

### Bekleyen kararlar
- K1–K8 (#222 §10: spawn, nesne paylaşımı, dolu kutu, kapasite, starvation,
  kapanışta mesajlar, graf devri kaçağı — K7, nogc/agc sürüm hedefi).
- Optimizasyonların CFG'ye kaydırılma derecesi (bkz. §C).
- jitData/moving-GC kısıtının kaderi (mekanizma mı ilan mı).
- globalSlots_ sahipliği (K2'ye bağlı).

## C. Faz durumu (2026-08-27 sonu)

- 0 CFG sağlamlaştırma — YAPILDI (8f9616b): exception kenarı, JMP sonrası lider,
  unreachable temizliği, dominance (CHK), natural loop; --cfg exc/loop etiketleri.
- 1 liveness + canlı-slot kökleme — YAPILDI (4390056 VM, fbd7bb8 JIT shadow).
- 2 Value daraltma + tek string modeli — YAPILDI (4ec9c2e, 2862123): Value
  112→24 bayt, düz-kopyalanır; string GC'li StringObject (VM≡JIT ortak).
- 3 Object başlığı — YAPILDI (b7610e3): marked kalktı, çift bağ O(1) unlink,
  vptr→tip-switch; String-çocuk kökleme bug'ı düzeltildi + regresyon fixture.
- 4 global state — JIT bağlamı YAPILDI (94882ee: JitRuntime/rt + shadow stack
  thread_local); sys_random thread_local YAPILDI; FileRegistry sınır kaydı
  yerinde (derleme-zamanı tek-parçacık, yorum satırı yeterli — değişiklik gerekmedi);
  globalSlots_ sahipliği K2 ürün kararına bağlı (bilinçli açık).
- 5 GC ÇEKİRDEĞİ — YAPILDI (ADR-022, dal issue-112-gc-cekirdek):
  toplayıcı `src/gc/` altına taşındı (backend-bağımsız katman); VM ve JIT
  AYNI Heap'i paylaşır (`static Heap jitHeap` kalktı, ömür koşuya bağlandı);
  kök kaydı ters çevrildi (RootSource — A1.3'ün istediği declaratif kök
  listesi); tempo canlı BAYTA bağlandı (A4 darboğazı: 20k canlı kayıtta
  2115ms → 72ms, ölçek süper-lineerden düze); #217 incremental KALDIRILDI
  (yarım kalmıştı: allocate-black yok, barrier String'i kapsamıyor, ve
  ölçümde hiç yayılmıyordu); mark özyinelemesiz (300k derinlik doğrulandı);
  `Object::prev` kalktı (tek çağıranı sweep'ti, ona gerek yok).
  Bulunan+düzeltilen hata: nesne üreten opcode'ların bir kısmı (string
  cast'leri, STRING_CONCAT, decimal aritmetiği) JIT'te shadow stack'e
  yansıtılmıyordu → sessiz use-after-free. Toplu kökleme + agresif-eşik
  gate'i eklendi.
- 6 allocator seam — SIRADA (arena/freelist). NOT: faz 5 ölçümü gösterdi ki
  asıl darboğaz tahsis maliyeti değil TOPLAMA SIKLIĞIydı; allocator bunu
  çözmez, tahsis yolunu ucuzlatır. Sıra bilinçli olarak tempodan sonraya
  alındı.

## D. Kapsam dışı bırakılanlar (kayıt)

- `nogc` / `agc`: ürün sahibi kararı — "nogc uygulamak için zaten mükemmel
  bir GC'ye ihtiyacımız var; en sona bırak" (2026-08-28). Çekirdek bastırma
  sayacına hazır bırakıldı, sayaç yazılmadı.
- #193 (string kopya baskısı) ve #206 (array/GC süper-lineer): bu turda
  ölçüldü, KAPANMADI. #193'ün darboğazı string birleştirmenin kendisinde
  (O(n²) kopya), GC'de değil — eski/yeni çekirdek arasında fark yok (96ms
  vs 98ms). #206'da fark küçük (64ms vs 53ms); o senaryonun asıl tetikleyicisi
  büyük canlı kümedir ve orada kazanç büyüktür, ama issue'nun kendi
  fixture'ı bunu göstermiyor.
