#!/usr/bin/env bash
# ============================================================================
# bench/bin/priority-run.sh — benchmarkları izole öncelikte koşturan wrapper
#
# AMAÇ: arka plan yükünün ölçümü kirletmesini engellemek. Benchmark sürecini
# realtime zamanlama sınıfına (SCHED_FIFO, rt prio 50), en yüksek nice'e
# (-20) alır; isteğe bağlı olarak belirli çekirdeklere pinner.
#
# KULLANIM (root gerekir):
#   sudo bash bench/bin/priority-run.sh build/saqut run bench/sqt/cpu_hash.sqt
#   sudo env BENCH_PIN=2 bash bench/bin/priority-run.sh \
#            build/saqut run bench/sqt/mem_selsort.sqt
#
# BENCH_PIN: "taskset -c <liste>" ile çekirdek pinleme (örn. "2" veya "2,3").
# Pinleme jitter azaltır; benchmark sırasında o çekirdeğin boş olduğundan
# emin olun. Karşılaştırılan TÜM diller aynı wrapper ile koşturulmalıdır —
# tek tarafa öncelik verip diğerine vermemek ölçümü bozar.
#
# NOT: realtime öncelik + nice, mutlak hız kazandırmaz; gürültüyü azaltır.
# Amacı ölçüm tekrarlanabilirliği olan bir ölçüm hijyeni aracıdır.
# ============================================================================
set -eu

if [ "$(id -u)" -ne 0 ]; then
    echo "hata: root gerekli — 'sudo bash $0 <komut...>' olarak çalıştırın" >&2
    exit 1
fi

if [ $# -lt 1 ]; then
    echo "kullanım: sudo [env BENCH_PIN=<çekirdekler>] bash $0 <komut [arg...]>" >&2
    exit 2
fi

PIN="${BENCH_PIN:-}"

PRE=()
if [ -n "$PIN" ]; then
    command -v taskset >/dev/null 2>&1 || {
        echo "hata: BENCH_PIN istendi ama taskset bulunamadı (util-linux)" >&2
        exit 3
    }
    PRE+=(taskset -c "$PIN")
fi

command -v chrt >/dev/null 2>&1 || {
    echo "hata: chrt bulunamadı (util-linux)" >&2
    exit 3
}

exec "${PRE[@]}" chrt -f 50 nice -n -20 "$@"
