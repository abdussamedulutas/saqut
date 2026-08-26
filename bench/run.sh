#!/usr/bin/env bash
# ============================================================================
# bench/run.sh — cross-language benchmark koşturucu
#
# Amaç: aynı iki saf-hesaplama algoritmasını (CPU-yoğun FNV-1a hash,
# bellek-yoğun selection sort) altı uygulamada koşup CHECKSUM tutarlılığını
# denetler; her koşumda program içi ELAPSED_MS, GNU time (WALL/USER/SYS/
# MAXRSS_KB) ve perf stat (task-clock/cycles/instructions) ölçümlerini basar.
# Karşılaştırma YALNIZ aynı makine + aynı öncelik koşullarında geçerlidir.
#
# Dış ölçüm zinciri (per koşum):
#   timeout → [taskset/chrt/nice] → /usr/bin/time → perf stat → program
#
# Öncelik hijyeni:
#   - ROOT olarak koşarsa her koşum önce taskset (BENCH_PIN verilirse),
#     ardından SCHED_FIFO(50) + nice -20 ile sarılır. Bu mutlak hız
#     kazandırmaz, arka plan yükünün gürültüsünü azaltır.
#   - ROOT değilse düz koşturulur ve uyarı basılır; adil karşılaştırma
#     için 'sudo bash bench/run.sh' önerilir.
#   - perf sayacı root olmadan da kullanıcı process'i için çalışır;
#     root ile daha güvenilir (perf_event_paranoid).
#
# Kullanım:
#   bash bench/run.sh                 # all: tüm diller + vm + jit
#   bash bench/run.sh vm              # yalnız saQut VM
#   bash bench/run.sh jit             # yalnız saQut JIT
#   bash bench/run.sh langs           # yalnız python/php/node/kjs
#   sudo env BENCH_PIN=2 bash bench/run.sh
#   BENCH_RUNS=3 bash bench/run.sh    # her programı 3 kez koş, medyan süre
#
# Çevresel değişkenler:
#   BENCH_PIN      <çekirdekler>   taskset pinleme (root gerekir)
#   BENCH_RUNS     <n>             her program için koşum sayısı (varsayılar 1)
#   BENCH_TIMEOUT  <sn>           tek koşum timeout'u (varsayılan 300)
#   BENCH_PERF     0               perf stat'i kapat (varsayılan açık)
#
# Gereksinimler:
#   /usr/bin/time (GNU time, 'time' paketi) — WALL/MAXRSS için
#   perf (linux-tools paketi, opsiyonel ancak önerilen) — CPU sayaçları
#
# Çıktı sözleşmesi (programlar):
#   CHECKSUM=<signed int32>
#   ELAPSED_MS=<program içi ms>
# ============================================================================
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/build/saqut"
MODE="${1:-all}"
RUNS="${BENCH_RUNS:-1}"
TMO="${BENCH_TIMEOUT:-300}"
PERF_ENABLED="${BENCH_PERF:-1}"

# Referans checksum'lar — değişim durumunda README'deki tablo da güncellenir.
EXPECT_CPU=545460224
EXPECT_SORT=94980184

if [ ! -x "$BIN" ]; then
    echo "error: $BIN bulunamadı — önce projeyi derleyin" >&2
    exit 1
fi

# ── Öncelik öneki (root ise) ─────────────────────────────────────────────────
PRIO=()
if [ "$(id -u)" -eq 0 ]; then
    if [ -n "${BENCH_PIN:-}" ]; then
        if command -v taskset >/dev/null 2>&1; then
            PRIO+=(taskset -c "$BENCH_PIN")
        else
            echo "# uyarı: BENCH_PIN istendi ama taskset bulunamadı — pinleme atlandı"
        fi
    fi
    if command -v chrt >/dev/null 2>&1 && command -v nice >/dev/null 2>&1; then
        PRIO+=(chrt -f 50 nice -n -20)
        echo "# öncelik: SCHED_FIFO(50) + nice -20${BENCH_PIN:+ + taskset $BENCH_PIN}"
    else
        echo "# uyarı: chrt/nice bulunamadı — öncelik ayarlanmadı"
    fi
else
    echo "# not: root değilsiniz — nice/chrt uygulanmadı (adil karşılaştırma için 'sudo bash bench/run.sh')"
fi
echo "# koşum: $RUNS tekrar/program, timeout $TMO s"

HAS_TIME=0
if command -v /usr/bin/time >/dev/null 2>&1; then
    HAS_TIME=1
else
    echo "# uyarı: /usr/bin/time (GNU time, 'time' paketi) yok — WALL/USER/SYS/MAXRSS_KB ölçülmeyecek"
fi

HAS_PERF=0
if [ "$PERF_ENABLED" = "1" ] && command -v perf >/dev/null 2>&1; then
    HAS_PERF=1
else
    echo "# uyarı: perf (linux-tools 'perf' paketi) yok veya BENCH_PERF=0 — CPU cycles/instructions ölçülmeyecek"
fi

# ── Tek programı N kez koş, CHECKSUM denetle, medyan ELAPSED_MS bas ──────────
run_one() {
    local label="$1" expected="$2"
    shift 2

    printf '\n================================================================\n'
    printf '== %s\n' "$label"
    printf '================================================================\n'

    # Ölçüm zinciri: perf kullanılırsa program perf'in içine alınır;
    # /usr/bin/time zincirin en dışında kalır.
    local wrap=()
    if [ "$HAS_PERF" -eq 1 ]; then
        wrap+=(perf stat -e task-clock,cycles,instructions --)
    fi

    local times=() sum ok=0 rc out i
    for ((i = 1; i <= RUNS; i++)); do
        if [ "$HAS_TIME" -eq 1 ]; then
            out=$(timeout "$TMO" "${PRIO[@]}" /usr/bin/time -f "WALL=%e USER=%U SYS=%S MAXRSS_KB=%M" "${wrap[@]}" "$@" 2>&1)
        else
            out=$(timeout "$TMO" "${PRIO[@]}" "${wrap[@]}" "$@" 2>&1)
        fi
        rc=$?
        printf '  [%d] %s\n' "$i" "$(printf '%s\n' "$out" | tr '\n' ' ')"
        printf '%s\n' "$out" >&2

        if [ "$rc" -eq 124 ]; then
            echo "!! TIMEOUT (${TMO}s): kesildi — BENCH_TIMEOUT'u artırın veya parametreleri küçültün"
            return 0
        fi
        if [ "$rc" -ne 0 ]; then
            echo "!! FAILED exit=$rc"
            return 0
        fi

        sum=$(printf '%s\n' "$out" | grep '^CHECKSUM=' | head -1 | cut -d= -f2)
        if [ -z "$sum" ]; then
            echo "!! CHECKSUM SATIRI YOK — çıktı sözleşmesi ihlali"
            return 0
        fi
        if [ "$sum" != "$expected" ]; then
            echo "!! CHECKSUM MISMATCH: $sum (beklenen $expected)"
            ok=0
        else
            ok=1
        fi

        local ms=$(printf '%s\n' "$out" | grep '^ELAPSED_MS=' | head -1 | cut -d= -f2)
        [ -n "$ms" ] && times+=("$ms")
    done

    [ "$ok" -eq 1 ] && echo "CHECKSUM OK ($sum)"
    if [ "${#times[@]}" -gt 0 ]; then
        # Bash integer sıralama: medyan dizinin ortasındaki değer
        local sorted=() med
        read -r -a sorted < <(printf '%s\n' "${times[@]}" | sort -n | tr '\n' ' ')
        local n=${#sorted[@]}
        local idx=$((n / 2))
        med="${sorted[$idx]}"
        echo "ELAPSED_MS medyan: $med  (koşumlar: ${times[*]})"
    fi
    return 0
}

have() { command -v "$1" >/dev/null 2>&1; }
want() { [ "$MODE" = "all" ] || [ "$MODE" = "$1" ]; }

# ── Python / PHP / Node ──────────────────────────────────────────────────────
if want langs; then
    have python3 && {
        run_one "python  cpu_hash"     "$EXPECT_CPU"  python3 "$ROOT/bench/py/cpu_hash.py"
        run_one "python  mem_selsort"  "$EXPECT_SORT" python3 "$ROOT/bench/py/mem_selsort.py"
    } || echo "== SKIP python3 bulunamadı =="

    have php && {
        run_one "php     cpu_hash"     "$EXPECT_CPU"  php "$ROOT/bench/php/cpu_hash.php"
        run_one "php     mem_selsort"  "$EXPECT_SORT" php "$ROOT/bench/php/mem_selsort.php"
    } || echo "== SKIP php bulunamadı =="

    have node && {
        run_one "node    cpu_hash"     "$EXPECT_CPU"  node "$ROOT/bench/js/cpu_hash.js"
        run_one "node    mem_selsort"  "$EXPECT_SORT" node "$ROOT/bench/js/mem_selsort.js"
    } || echo "== SKIP node bulunamadı =="

    # KJS (KDE JavaScript, interpreter) — ayrı motor, kjs/ altında ES5 port.
    # KJS debug build stderr'e 'LEAK: n KJS::Node' basar; stderr yok sayılır
    # (2>&1 ile birleşmemesi için). WALL/MAXRSS yine /usr/bin/time'dan gelir.
    have kjs5 && {
        run_one "kjs5    cpu_hash"     "$EXPECT_CPU"  sh -c 'kjs5 "$1" 2>/dev/null' _ "$ROOT/bench/kjs/cpu_hash.kjs"
        run_one "kjs5    mem_selsort"  "$EXPECT_SORT" sh -c 'kjs5 "$1" 2>/dev/null' _ "$ROOT/bench/kjs/mem_selsort.kjs"
    } || echo "== SKIP kjs5 bulunamadı =="
fi

# ── saQut VM / JIT ───────────────────────────────────────────────────────────
if want vm; then
    run_one "saqut-vm  cpu_hash"     "$EXPECT_CPU"  "$BIN" run "$ROOT/bench/sqt/cpu_hash.sqt"
    run_one "saqut-vm  mem_selsort"  "$EXPECT_SORT" "$BIN" run "$ROOT/bench/sqt/mem_selsort.sqt"
fi
if want jit; then
    run_one "saqut-jit cpu_hash"     "$EXPECT_CPU"  "$BIN" run --jit "$ROOT/bench/sqt/cpu_hash.sqt"
    run_one "saqut-jit mem_selsort"  "$EXPECT_SORT" "$BIN" run --jit "$ROOT/bench/sqt/mem_selsort.sqt"
fi

printf '\n================================================================\n'
echo "Karşılaştırma kuralı: CHECKSUM diller/modlar arası BİREBİR aynı olmalı;"
echo "süreler yalnız aynı makine + aynı öncelik koşullarında karşılaştırılır."
