# saQut Benchmark — saf hesaplama zemini

Amaç: saQut programlarının **CPU-yoğun** ve **bellek-yoğun** saf hesaplama
hızını ölçülebilir, diller arası karşılaştırılabilir hale getirmek. Aynı iki
algoritma Node.js, Python, PHP ve KJS ile de yazıldı; CHECKSUM değerleri tüm
dillerde **birebir aynı** çıkmak zorundadır. Süreler yalnız aynı makine ve
aynı koşum koşullarında karşılaştırılır.

Bu klasör derleyici içi faz ölçümü (`saqut bench` → `src/bench/profile.hpp`)
değildir; buradaki programlar **kullanıcı seviyesi** algoritmalarıdır ve
VM/JIT çalışma zamanı hızını ölçer.

## Programlar

| Program | Yük | Algoritma | Varsayılan parametre |
|---|---|---|---|
| `sqt/cpu_hash.sqt` | CPU-yoğun | FNV-1a 32-bit hash zinciri, sabit tampon üzerinde tekrarlı tur | `size=2048`, `rounds=3000` |
| `sqt/mem_selsort.sqt` | bellek-yoğun | her turda yeni rastgele `int[]` tahsisi + optimize edilmemiş O(n²) selection sort + atma (GC baskısı) | `size=1200`, `rounds=15` |

Her iki algoritma şu uygulamalarda portlanmıştır (`bench/<dil>/`):

| Motor | Klasör | Uzantı | Not |
|---|---|---|---|
| saQut VM/JIT | `sqt/` | `.sqt` | normatif referans |
| Node.js (V8 JIT) | `js/` | `.js` | **SAF JS**: typed array (Int32Array) yok, normal dizi; `Math.imul`+`\|0` |
| Python | `py/` | `.py` | `& 0xFFFFFFFF` maskeli |
| PHP | `php/` | `.php` | `mul32()` split-multiply helper |
| KJS (KDE JS, interpreter) | `kjs/` | `.kjs` | ES5 port; typed array yok, `Math.imul`+`\|0` |

Tasarım kararları:

- Kriptografik güvenlik amaç değildir; hash yalnızca çarpma+xor yoğun saf
  hesap yükü için seçildi.
- Selection sort bilinçli olarak hız için optimize edilmemiştir: bol
  karşılaştırma, rastgele konuma swap, index tabanlı erişim.
- Bellek tarafındaki GC baskısı (`--gc-stats` gibi bir gözlem yüzeyiyle)
  bugün public CLI'da görünür DEĞİLDİR; dolaylı izleme dışarıdan yapılır
  (`/usr/bin/time` MAXRSS_KB). Bu bir bilinen eksik, sessizce gizlenmez.
- `run.sh` `/usr/bin/time` (GNU time, `time` paketi) bulursa `WALL` ve
  `MAXRSS_KB` satırını basar; bulamazsa ölçümsüz düz koşuma düşer ve
  kullanıcıya uyarı basar. RSS kanıtı için bu paketi kurup
  `sudo bash bench/run.sh` koşmak gerekir.

## Referans checksum'lar (dil-bağımsız sabit)

Varsayılan parametrelerle beklenen çıktı:

| Program | CHECKSUM |
|---|---|
| `cpu_hash.sqt` | `545460224` |
| `mem_selsort.sqt` | `94980184` |

Kanıt zinciri (2026-08-26): her iki README değeri bu depodaki `build/saqut`
binary'siyle **VM ve `--jit` modlarında**, ve Python/Node/PHP/KJS portlarıyla
doğrulandı — altı uygulama birebir aynı CHECKSUM üretti. Koşum komutu ve
tarih aşağıda örnek kayıttır:

```
timeout 60 build/saqut run bench/sqt/cpu_hash.sqt        # VM: 545460224
timeout 60 build/saqut run --jit bench/sqt/cpu_hash.sqt  # JIT: 545460224
python3 bench/py/cpu_hash.py                             # 545460224
node bench/js/cpu_hash.js                                # 545460224
php bench/php/cpu_hash.php                               # 545460224
kjs5 bench/kjs/cpu_hash.kjs                              # 545460224
timeout 120 build/saqut run bench/sqt/mem_selsort.sqt    # VM: 94980184
... diğer diller de 94980184
```

Not: CHECKSUM değerleri 2026-08-26 tarihinde değişti. Eski README değeri
`cpu_hash` için `528336056` idi ve XOR ifadesinin literal-form hatalı
değerlenmesinden (#230, below) etkilenmişti; benchmark'ın değişkenli `basis`
yoluna geçmesiyle güncel değer `545460224` oldu ve tüm uygulamalarda sabitlendi.
Süreler makine/binary revizyonuna bağlıdır; binary değiştiğinde CHECKSUM'lar
yeniden teyit edilmelidir.

## Çalıştırma

```bash
bash bench/run.sh                 # all: langs + vm + jit, 1 koşum/program
sudo bash bench/run.sh            # root: SCHED_FIFO + nice (adil ölçüm için)
sudo env BENCH_PIN=2 bash bench/run.sh
BENCH_RUNS=3 bash bench/run.sh    # 3 koşum, medyan süre
bash bench/run.sh vm              # yalnız saQut VM
bash bench/run.sh jit 300         # yalnız JIT
bash bench/run.sh langs           # yalnız python/php/node/kjs
```

Doğrudan kullanım (öncelik wrapper'ı ile):

```bash
timeout 300 build/saqut run bench/sqt/cpu_hash.sqt
sudo bash bench/bin/priority-run.sh build/saqut run bench/sqt/cpu_hash.sqt
sudo env BENCH_PIN=2 bash bench/bin/priority-run.sh node bench/js/mem_selsort.js
sudo env BENCH_PIN=2 bash bench/bin/priority-run.sh kjs5 bench/kjs/cpu_hash.kjs
```

Çıktı sözleşmesi:

```
CHECKSUM=<signed int32>
ELAPSED_MS=<program içi wall-clock ölçümü>
```

`run.sh` ayrıca `/usr/bin/time` varsa `WALL` ve `MAXRSS_KB` (bellek) satırı
basar; `BENCH_RUNS>1` ise medyan `ELAPSED_MS` raporlanır. Timeout zorunludur
(sonsuz döngü koruması); varsayılan 300s, `BENCH_TIMEOUT` ile değiştirilir.

## Öncelik hijyeni (ölçüm gürültüsü)

- `run.sh` root olarak koşarsa her koşumu önce `taskset -c $BENCH_PIN`
  (verilirse), ardından `chrt -f 50` + `nice -n -20` ile sarar. Bu mutlak
  hız kazandırmaz; arka plan yükünün gürültüsünü azaltır, tekrarlanabilirlik
  artar.
- Root değilse düz koşturulur ve uyarı basılır — adil karşılaştırma için
  `sudo bash bench/run.sh` kullanın.
- **Aynı koşum içinde tek tarafa öncelik verip diğerine vermemek** ölçümü
  bozar: karşılaştırılan tüm diller aynı öncelik + pin koşulunda koşmalı.

## Adil karşılaştırma kuralları (diğer dillerle)

1. Aynı makine, aynı sabit parametreler, arka plan yükü kapalı.
2. TÜM diller aynı öncelik wrapper'ı ile koşturulur (bkz. üstte).
3. Her program için en az 3 koşum (`BENCH_RUNS=3`); medyan alınır.
   Tek koşum kanıt sayılmaz.
4. Karşılaştırılan şey CHECKSUM eşleşmesi + medyan süre.

## Cross-language determinizm sözleşmesi

Tüm dillerde birebir aynı checksum için:

- **Doldurma LCG:** `x = x * 1103515245 + 12345`, başlangıç seed'i
  - cpu_hash: `123456789`
  - mem_selsort: `987654321`
- **FNV-1a:** offset basis `2166136261` (unsigned) / `-2128831035` (signed
  int32 karşılığı), prime `16777619`; adım: `h ^= buf[i]; h *= prime`.
  cpu_hash her turda offset basis'i tur numarasıyla xor'lar: `h = basis ^ r`.
- **int = 32-bit signed**, taşma tanımlı 2's-complement wrap (ADR-040).
- Dil bazında uyarlama:
  - C/C++/Rust/Go/saQut: native davranış.
  - **Node.js:** çarpmalar `Math.imul(a, b)` ile; toplama sonrası `| 0`;
    işaretsiz sağa shift gerekirse `>>>`.
  - **Python:** her aritmetik adım sonrası `& 0xFFFFFFFF`; yazdırma sırasında
    signed'e çevirme (`v >= 2**31 ise v -= 2**32`).
  - **PHP:** int64 ortamda 32-bit taşan çarpma float'a düşer; 32-bit wrap
    çarpma elle bölünerek yapılmalıdır (split multiplication helper, `mul32`).
  - **KJS:** ES5 tabanlı (let/const/arrow/typed array yok, `var` gerekir);
    32-bit semantiği Node ile aynı şekilde `Math.imul` + `| 0` ile;
    zamanlama `performance.now` yerine `Date.now()` ile (KJS testinde
    `timeout` + `BENCH_TIMEOUT` bu yüzden yeterlidir).
- Selection sort `<` sıkı karşılaştırmasıyla İLK minimumu seçer; stabil
  değildir ama deterministiktir — tüm dillerde aynı permütasyonu üretir.
- saQut `int` signed olduğundan sıralama karşılaştırması Python/PHP portunda
  da signed yapılır (metin içinde işaretli).

## Gözlem: hız karşılaştırması (2026-08-26, önceliksiz, tek koşum)

Aşağıdaki süreler bu depo makinesinde, **öncelik wrapper'ı olmadan tek koşumla**
alınmıştır — DEBUG amaçlı bağıl fikir verir; kesin karşılaştırma için
`sudo BENCH_RUNS=3` ile tekrar koşun (bu tablo kanıt değildir). ELAPSED_MS
program içi ölçüm, dış `time` ile onaylandı.

| Motor | cpu_hash | mem_selsort |
|---|---|---|
| Node.js (V8 JIT) | 8–9 ms | 16 ms |
| Node.js `--jitless` (JIT kapalı) | ~230–260 ms | ~293 ms |
| Deno (V8, farklı runtime) | ~20 ms | — |
| saQut JIT `[EXPERIMENTAL]` | ~464–480 ms | ~1 500–1 540 ms |
| KJS (interpreter) | ~1 035–1 080 ms | ~520–725 ms |
| Python (CPython) | ~700–730 ms | ~470 ms |
| PHP | ~2 550–2 860 ms | ~675–725 ms |
| saQut VM | ~12 850–13 150 ms | ~20 270–22 300 ms |

### Node neden fark atıyor? (kanıtlı olarak)

- **Sonuç doğru:** aynı algoritma bağımsız `gcc`/`clang -O2` derlemesiyle de
  `545460224` / `94980184` üretti; saQut VM/JIT, Node, Deno, Python, PHP,
  KJS — altı uygulama checksum'da birebir aynı. Node sonuç uydurmuyor.
- **Hızının kaynağı JIT (typed-array DEĞİL):** `node --jitless` ile aynı
  program 8 ms → ~230 ms'ye (~28×) geriliyor. V8, SAF diziyi (packed SMI)
  + sıkı deterministik döngüde SMI optimizasyonu, loop-invariant hoisting ve
  unrolling gibi optimizasyonları uygular; `Int32Array` kullanmaya gerek
  kalmaz. SAF JS ile talimat sayısı bile DÜŞTÜ (cpu: 270M→236M,
  mem: 418M→365M) — hız typed-array mekanizmasından değil JIT kod üretiminden
  gelir, bu bir interpreter'ın yapamayacağı şeydir.
- **Startup değil:** dış `time` Node toplam `real 47 ms` gösterdi (startup
  ~38 ms + hesap 9 ms); saQut JIT toplam 480 ms (tamamı hesap).
- **Sol tarafın düzeyi:** V8 on yıllarca optimize edilmiş üretim JIT'i;
  saQut JIT `[EXPERIMENTAL]` baseline (henüz optimize edici değil), saQut VM
  interpreter. Bu bench "eşitlik değil, dürüst merdiven" göstergesidir.
- KJS katılımı ikinci bir kanıt: interpreter-tabanlı JS (KJS ~1 000 ms) ile
  V8 JIT (8 ms) arasındaki uçurum, saQut VM ile saQut JIT arasındakiyle aynı
  doğadadır — yani fark saQut'a özgü bir kusur değil, JIT katmanının olgunluk
  farkıdır.

## Bilinen sınırlar

- **XOR literal-form hatası (#230):** negatif integer literal, XOR
  operatörünün operand'ı olarak kullanıldığında 2'lik değer kayması üretiyor
  (ör. `(-2128831035) ^ 1` doğrusu `-2128831036` yerine `-2128831034`
  veriyor). Bu benchmark bu yüzden `cpu_hash.sqt` içinde değeri bir
  değişkene alıp `(basis ^ r)` biçimini kullanır — kanıtlı doğru çalışan yol.
  Hata kapatılınca literal forma dönüp CHECKSUM'lar yeniden teyit edilmelidir.
  Teknik ayrıntı #230 issue'suna işlendi (2026-08-26).
- GC davranışının sayısal gözlemi için public yüzey yoktur (bkz. v1 kapsam
  bildirgesi §3.2 hedefi); bellek kanıtı `/usr/bin/time` MAXRSS_KB ile
  dolaylıdır.
- Süre sonuçları makine/binary revizyonuna bağlıdır; repoda kanıt olarak
  yalnız CHECKSUM eşleşmesi + koşum komutu + tarih taşır.
