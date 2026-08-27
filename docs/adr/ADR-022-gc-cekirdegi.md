# ADR-022 — GC çekirdeği: taşımasız, stop-the-world, backend-ortak mark-sweep

- Durum: Kabul edildi
- Tarih: 2026-08-28
- Sürüm hedefi: 0.9.6
- İlgili: ADR-032 (backend yönü), ADR-037 (JIT value ABI), ADR-038
  (determinizm/sürüm uyumluluğu), #112, #217, #228

## Bağlam

Bu ADR'ye dört belge atıf yapıyordu (`docs/MIRPLAN.md`, `ADR-032`,
`docs/gc-threading-altyapi-denetimi.md`, kaynak yorumları) ama dosyanın
kendisi repoda yoktu. "Taşımasız, stop-the-world, deterministik mark-sweep"
cümlesi kaynak yorumlarında yaşıyordu; normatif kaydı yoktu.

Bu kayıt eksikliği somut zarar üretti: `#217` incremental marking, ADR
düzeyinde bir kısıt olmadığı için eklendi ve yarım kaldı — tricolor durum
makinesi ve write barrier yazıldı ama allocate-black yoktu, barrier
tek-string-modelinden sonra String'i kapsamıyordu, ve ölçümde marking hiçbir
zaman birden fazla safepoint'e yayılmıyordu (yani fiilen stop-the-world
çalışıyor, karşılığında iki gizli use-after-free yolu taşıyordu).

Ayrıca VM ve JIT ayrı `Heap` örnekleri kullanıyordu (`mir_backend.cpp`
içinde `static Heap jitHeap`), ayrı toplama politikalarıyla. JIT'in 1.0'da
`[EXPERIMENTAL]` olmaktan çıkması hedefiyle bu ayrım sürdürülemezdi.

## Karar

### 1. Taşımasız (non-moving)

Nesne adresi ömrü boyunca sabittir. Bu bir tercih değil **kısıttır**:
`ArrayObject::jitData`, JIT'in eleman tamponuna doğrudan bellek görüntüsüdür
ve sabit offset'ten yüklenir (`mir_backend.cpp`, `MIR_load`). Taşıyan bir
toplayıcı bu görüntüleri geçersiz kılar.

Taşıyan (kopyalayan/sıkıştıran, jenerasyonel yarı-uzay) bir toplayıcıya
geçiş, bu ADR'nin revizyonunu ve ayrıca bir FFI sabitleme (pinning) /
safepoint sözleşmesini gerektirir. Kapsam dışıdır.

### 2. Stop-the-world, incremental değil

Toplama başladığında mutator durur, bitince devam eder. Ara durum yoktur;
dolayısıyla write barrier de yoktur.

Gerekçe: incremental marking, toplama sırasında programın çalışmaya devam
etmesi için vardır. saQut'un hedef eşzamanlılık modelinde (bkz. #222, izolat
+ mesajlaşma) her heap'in tek mutator'u vardır — toplama sırasında duran tek
şey o heap'in kendi iş parçacığıdır, diğerleri kendi heap'lerinde çalışmaya
devam eder. İncremental'ın çözdüğü problem bu mimaride doğmaz.

Ters yönde de maliyet vardır: incremental marking'i çok iş parçacıklı hale
getirmek işaret geçişlerini atomik yapmayı gerektirir; stop-the-world
per-heap modelinde o yarış hiç oluşmaz.

`#217` bu kararla **kaldırılmıştır** (kapatma gerekçesi issue'ya yazılır).
Duraklama süresi ölçülebilir bir sorun haline gelirse doğru cevap toplamayı
bölmek değil, toplanacak nesne sayısını azaltmaktır (`nogc`/`agc` — bkz. §7).

### 3. Tek çekirdek, iki backend

Toplayıcı `src/gc/` altında yaşar ve **backend'lerden bağımsızdır**. VM ve
MIR JIT aynı `Heap` örneğini paylaşır, aynı politikayı kullanır, aynı
sayaçları raporlar.

Konum kararı: nesne modeli (`Object`, `ArrayObject`, `StructObject`,
`StringObject`, `DecimalObject`) bir backend'in iç detayı değil, iki
backend'in ortak sözleşmesidir — `src/vm/object.hpp`'yi çeken dosyaların
çoğunluğu zaten `src/vm/` dışındaydı (`data/`, `dap/`, `ffi/`, `mir/`).

Bilinen kalıntı: `src/gc/` hâlâ `vm/value.hpp`'ye bağımlıdır. `Value`'nun
`core/` veya `gc/` altına taşınması ayrı bir turdur (tüketici kümesi geniş).
Kaynakta `TODO(gc-katman)` ile işaretlidir.

### 4. Kökler kaydolur, sorulmaz

`Heap` kök kümesini bilmez. Kökü olan taraf `RootSource` arayüzünü gerçekler
ve Heap'e kaydolur; toplama sırasında Heap kayıtlı her sağlayıcıya sorar
(`src/gc/gc_roots.hpp`).

- VM (`Interpreter`): modül global'leri, çağrı yığınındaki **canlı** frame
  slot'ları (kök daraltma — `src/ir/ir_liveness.cpp`), uçuştaki throw değeri.
- JIT (`JitRootSource`): shadow stack, global pointer slot'ları, uçuştaki
  hata nesnesi, host çağrısı argüman tamponu ve dönüş değeri.

Gerekçe: kök kümesi backend'in iç yapısıdır. Heap onu bilirse her yeni
backend Heap'i değiştirmeyi gerektirir. Bu ters çevirme, ileride her iş
parçacığının kendi heap'ini kullandığı modele de doğrudan uyar.

**Sağlayıcı sözleşmesi:** eksik bildirim canlı nesnenin süpürülmesidir
(use-after-free); fazla bildirim yalnızca gecikmiş toplamadır. Emin
olunmayan yerde bildirmek doğru taraftır.

### 5. Tempo canlı bayta bağlıdır

Toplama eşiği nesne **sayısına** değil tahmini canlı **bayta** bağlanır.
Her toplama sonunda eşik `canlı ayak izi × 2` olarak yenilenir, alt sınır
1 MiB'dir.

Gerekçe: sayı tabanlı eşik 10 baytlık bir string ile 10 MB'lık bir `byte[]`'i
aynı ağırlıkta sayar. Ölçüldü (`gc_baski.sqt`, 20.000 canlı kayıt + 200.000
çöp string): sayı tabanlı eşikle 2115 ms, bayt tabanlı eşikle 72 ms — aynı
toplama sayısında (29). Ölçek davranışı süper-lineerden düze döndü:

| canlı kayıt | sayı tabanlı | bayt tabanlı |
|---|---|---|
| 5.000  | 529 ms  | 58 ms  |
| 10.000 | 996 ms  | 64 ms  |
| 20.000 | 2159 ms | 71 ms  |
| 40.000 | 6204 ms | 100 ms |

### 6. Gözlemlenebilirlik sayım tabanlıdır

`--gc-stats` her iki backend'de aynı formatta raporlar: `collections`,
`freed`, `live`, `liveBytes`, `peakBytes`. `--gc-threshold=N` her iki
backend'de aynı anlama gelir (bayt; negatif = toplama kapalı).

Performans iddiaları **süreye değil olaya** bağlanır (#222 §8): "şu program
N turdan fazla toplamaz", "şu blokta canlı nesne sayısı K'yı geçmez". Bu
iddialar makineden, yükten ve backend'den bağımsız olarak tekrarlanabilir.

### 7. Kapsam dışı (bu ADR'de karara bağlanmayanlar)

- `nogc` / `agc` dil yüzeyi — tasarımı #222 §6'da, uygulaması sonraki tur.
  Çekirdek bastırma sayacına hazırdır ama sayaç henüz yoktur.
- Segment/bump/free-list allocator — tahsis maliyetini düşürür, toplama
  sıklığını değil; bu ADR toplama sıklığını hedefler.
- Jenerasyonel toplama, taşıyan toplayıcı, eşzamanlı toplama.
- Finalizer: **dile hiç girmeyecektir** (ADR-038 determinizmi ve
  `nogc`/`agc` tasarımı bunu bağımsız olarak gerektiriyor).

## Sonuçlar

**Olumlu**

- İki backend tek toplama davranışı gösterir; VM≡JIT paritesi GC'yi de kapsar.
- JIT heap'inin ömrü koşuya bağlandı (eskiden `static`, süreç ömrü boyunca
  yaşıyordu) — aynı süreçte arka arkaya program çalıştırmak artık mümkün.
- Yarım incremental altyapının iki gizli use-after-free yolu kalktı.
- Mark aşaması özyinelemeli değil (açık iş listesi): derin nesne grafı C++
  yığınını taşırmaz. 300.000 derinlikte zincir doğrulandı.
- Nesne başlığı küçüldü: `prev` (8 bayt) kaldırıldı — tek çağıranı sweep'ti
  ve sweep'in ona ihtiyacı yok.

**Olumsuz / kabul edilen**

- Duraklama süresi canlı küme ile orantılıdır ve sınırlanmamıştır. Soft
  real-time ihtiyacın cevabı `nogc`'dir, incremental değil.
- Bayt tahmini yaklaşıktır (STL'in gerçek kapasitesi değil mantıksal boyut
  sayılır). Tempo kararı için mertebe yeterlidir.
- 1 MiB alt sınırın altındaki programlar hiç toplamaz. Kasıtlıdır; küçük
  ayak izinde toplama, kazanılan bellekten pahalıdır.

## Kanıt

Revizyon: `issue-112-gc-cekirdek` dalı. Doğrulama komutları:

- `bash tests/run.sh` — golden 124, diferansiyel VM≡JIT 107, GC gate'leri 4,
  kök daraltma 1, agresif eşik kökleme 12. Hepsi geçti.
- ASan + `--gc-threshold=1` (her tahsiste toplama) tüm golden fixture'lar
  üzerinde: VM 139/139 temiz, JIT 133/133 temiz (5 fixture JIT dışı).
- Bu tarama sırasında **gerçek bir hata bulundu ve düzeltildi**: nesne üreten
  opcode'ların bir kısmı (string üreten cast'ler, `STRING_CONCAT`, decimal
  aritmetiği) sonucu shadow stack'e yansıtmıyordu → sessiz use-after-free.
  Düzeltme opcode başına değil topluca yapıldı (`mir_backend.cpp`, #221'in
  null-bayrağı bakımıyla aynı desen). Regresyon koruması:
  `tests/golden/gc/nesne_ureten_opcode_kokleme.sqt` + agresif eşik gate'i;
  gate'in gerçekten yakaladığı, düzeltme geçici geri alınarak doğrulandı.

DoD durumu: **Uygulandı + Test Edildi kanıtı sunuldu.** "Test Edildi" ve
"Release Edildi" kabulü ürün sahibinindir (AGENTS.md §5).
