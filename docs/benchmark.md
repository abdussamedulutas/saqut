# saQut Performans Baseline

> **Bu belge otomatik üretilmiştir.**
> Yeniden ölçmek için: `bash scripts/bench/run_bench.sh`
>
> Amaç: Bir **referans çizgisi** oluşturmak ve dondurmak.
> Diğer diller rakip değil — sabit **cetvel** (ilerideki saQut sürümleriyle oran kıyasına zemin).

---

## Ölçüm Koşulları (dondurulmuş)

| Konu               | Değer                                        |
|--------------------|----------------------------------------------|
| Tarih              | 2026-06-24                                |
| Commit             | `46e640f`                           |
| Derleme modu       | **Release** (`cmake -DCMAKE_BUILD_TYPE=Release`, `-O3`) |
| Tekrar (BENCH_RUNS)| 5                               |
| Java ısınma        | 1000 tur (JIT stabil olana kadar atılır)     |
| Zamanlama          | `std::chrono::high_resolution_clock` (C++), `time.perf_counter` (Python), `System.nanoTime` (Java) |
| Raporlanan değer   | **best** (en düşük, sistem gürültüsünden en az etkilenen) + avg |
| Makine             | `Linux 7.0.10-1-MANJARO` `x86_64`            |

> Gelecekteki ölçümler **aynı koşulda** yapılmalıdır — aksi hâlde kıyas geçersiz olur.

---

## Bölüm 1 — Derleme Hızı (Frontend Pipeline)

**Test verisi:** 400 modül, her birinde 12 fonksiyon, DAG import grafı
**Toplam kaynak:** 1624 KB
**Araç:** `saqut bench main_bench.sqt --compile-only --runs=5`

| Aşama             | Ort (µs) | En iyi (µs) | Açıklama                        |
|-------------------|----------|-------------|----------------------------------|
| tokenize          | 53712   | 47576   | Tokenizer (tüm modüller)         |
| parse             | 45188 | 39928 | Parser (tüm modüller)            |
| symbol-collect    | 29902   | 27165   | 3-geçiş sembol toplama           |
| type-check        | 25107    | 23958    | Tip denetimi + yapısal doğrulama |
| ir-gen            | 29799    | 27284    | IR üretimi                       |
| **derleme-toplam**| **183708** | **165911** | Yukarıdaki 5 aşama toplamı |

**Derleme verimi (best):** 7.97 MB/s

> **Not:** `tokenize` ve `parse` ayrı ölçülür çünkü `bench` komutu her modül
> için `Tokenizer::scan()` ve `Parser::parse()` aşamalarını sırayla zamanlar.

> **Cetvel karşılaştırması:** Java/Python modül sistemleri farklı olduğu için bu
> bölümde cetvel dili yoktur. Bu ölçüm, gelecekteki saQut sürümleriyle kıyaslanır.

---

## Bölüm 2 — Çalışma Hızı (VM Runtime)

**Araç:** `saqut bench <dosya>.sqt --runs=5` → `vm-execute` satırı
**Cetvel diller:** CPython Python 3.14.5 / openjdk version "26.0.1" 2026-04-21

### (a) Özyinelemeli Fibonacci — `fib(25)` = 75025

| Dil/VM             | Ort (µs)           | En iyi (µs)        |
|--------------------|--------------------|--------------------|
| **saQut (bytecode VM)** | **51963** | **51194** |
| CPython            | 9819 | 9630 |
| Java (JIT, +1000 ısınma) | 342 | 292 |

**saQut / CPython oranı (best):** 5.3x yavaş
**saQut / Java oranı (best):**    175.3x yavaş

> Kıyaslama notu: CPython = saf yorumlayıcı (saQut'un gerçek akranı).
> Java = JIT-derlenmiş (uzak referans; JIT ısınması atılmıştır).

### (b) Döngü Toplama — `sum(40000)`, döngüsüz değer 800,020,000

| Dil/VM             | Ort (µs)           | En iyi (µs)        |
|--------------------|--------------------|--------------------|
| **saQut (bytecode VM)** | **1795** | **1782** |
| CPython            | 1959 | 1898 |
| Java (JIT, +1000 ısınma) | 10 | 10 |

**saQut / CPython oranı (best):** 0.9x yavaş
**saQut / Java oranı (best):**    178.2x yavaş

---

## Yorum

Bu ölçümler saQut'un **mevcut hâlinin fotoğrafıdır** — optimize edilmemiş
bytecode VM, tek-geçişli IR, JIT yok. Amaç iyileştirmek değil, belgelemek.

Kullanım biçimi:
- **Regresyon tespiti:** Bir özellik eklenince `run_bench.sh` yeniden koştur.
  Eğer derleme aşamalarından biri kayda değer uzadıysa (> 10%) incelenmelidir.
- **Cetvel oranı:** Gelecekte JIT veya derleyici optimizasyonu eklenince
  CPython/Java oranlarının değişimini buradaki referansla kıyasla.

---

## Tekrar Çalıştırma

```bash
# Release build + ölçüm (tüm bölümler)
bash scripts/bench/run_bench.sh

# Yalnızca tek dosya ölç (hızlı kontrol)
./build/saqut bench <dosya.sqt> --runs=5

# Yalnızca derleme (VM atla)
./build/saqut bench <dosya.sqt> --runs=5 --compile-only
```
