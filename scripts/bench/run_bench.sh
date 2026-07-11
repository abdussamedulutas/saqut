#!/usr/bin/env bash
# =============================================================================
# run_bench.sh — saQut performans baseline ölçümü
#
# KULLANIM:
#   bash scripts/bench/run_bench.sh
#
# ÖLÇÜM KOŞULLARI (değiştirilmez — ilerideki kıyaslar aynı koşulla yapılır):
#   - Derleme modu     : Release (-O3, debug sembolleri yok)
#   - CMake hedef      : cmake -B build -DCMAKE_BUILD_TYPE=Release -G Ninja
#   - Tekrar sayısı    : BENCH_RUNS=5 (her aşama için)
#   - Java ısınma      : 1000 tur (JIT stabil olana kadar atılır)
#   - Cetvel diller    : CPython 3.x, Java 17+ (JIT)
#   - Ölçüm aracı      : saqut bench (std::chrono::high_resolution_clock)
#                        Python: time.perf_counter
#                        Java: System.nanoTime
#
# ÇIKTI: docs/benchmark.md (üzerine yazar)
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BENCH_DIR="${SCRIPT_DIR}"
BUILD_DIR="${ROOT}/build"
SAQUT="${BUILD_DIR}/saqut"
DOCS_DIR="${ROOT}/docs"
OUT_MD="${DOCS_DIR}/benchmark.md"
BENCH_RUNS=5
SUITE_MODULES=400
SUITE_FUNCS=12

# Renkler
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
info()  { echo -e "${GREEN}[bench]${NC} $*"; }
warn()  { echo -e "${YELLOW}[bench]${NC} $*"; }
error() { echo -e "${RED}[bench]${NC} $*" >&2; }

# =============================================================================
# 1. Release build
# =============================================================================
info "Release binary derleniyor..."
cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release -G Ninja -DCMAKE_CXX_FLAGS="-w" \
    -S "${ROOT}" 2>&1 | grep -v "^--" || true
ninja -C "${BUILD_DIR}" 2>&1 | grep -E "error:|Linking|FAILED" || true
SAQUT_VERSION="$("${SAQUT}" --help 2>/dev/null | head -1 || echo 'saqut')"
COMMIT_HASH="$(git -C "${ROOT}" rev-parse --short HEAD 2>/dev/null || echo 'unknown')"
BUILD_DATE="$(date -I)"
info "Binary: ${SAQUT}  (commit: ${COMMIT_HASH}  tarih: ${BUILD_DATE})"

# =============================================================================
# 2. Bölüm 1 — Derleme hızı (compile suite)
# =============================================================================
SUITE_DIR="${BENCH_DIR}/compile_suite"
info "Derleme benchmark suite üretiliyor (${SUITE_MODULES} modül)..."
MAIN_BENCH="$(python3 "${BENCH_DIR}/gen_compile_suite.py" \
    --modules="${SUITE_MODULES}" --funcs="${SUITE_FUNCS}" --out="${SUITE_DIR}" 2>&1 | tee /dev/stderr | tail -1)"

info "Derleme aşaması ölçülüyor (${BENCH_RUNS} tekrar, --compile-only)..."
COMPILE_BENCH_RAW="$("${SAQUT}" bench "${MAIN_BENCH}" \
    --runs="${BENCH_RUNS}" --compile-only 2>/dev/null)"
echo "${COMPILE_BENCH_RAW}"

# Derleme sonuçlarını ayrıştır
extract() { echo "${COMPILE_BENCH_RAW}" | grep "^$1" | awk '{print $NF}'; }
TOK_AVG=$(echo "${COMPILE_BENCH_RAW}" | grep "^tokenize"      | awk '{print $(NF-1)}')
TOK_BEST=$(echo "${COMPILE_BENCH_RAW}" | grep "^tokenize"     | awk '{print $NF}')
PARSE_AVG=$(echo "${COMPILE_BENCH_RAW}" | grep "^parse"       | awk '{print $(NF-1)}')
PARSE_BEST=$(echo "${COMPILE_BENCH_RAW}" | grep "^parse"      | awk '{print $NF}')
SYM_AVG=$(echo "${COMPILE_BENCH_RAW}" | grep "^symbol"        | awk '{print $(NF-1)}')
SYM_BEST=$(echo "${COMPILE_BENCH_RAW}" | grep "^symbol"       | awk '{print $NF}')
TC_AVG=$(echo "${COMPILE_BENCH_RAW}" | grep "^type-check"     | awk '{print $(NF-1)}')
TC_BEST=$(echo "${COMPILE_BENCH_RAW}" | grep "^type-check"    | awk '{print $NF}')
IR_AVG=$(echo "${COMPILE_BENCH_RAW}" | grep "^ir-gen"         | awk '{print $(NF-1)}')
IR_BEST=$(echo "${COMPILE_BENCH_RAW}" | grep "^ir-gen"        | awk '{print $NF}')
COMP_TOTAL_AVG=$(echo "${COMPILE_BENCH_RAW}" | grep "^derleme-toplam" | awk '{print $(NF-1)}')
COMP_TOTAL_BEST=$(echo "${COMPILE_BENCH_RAW}" | grep "^derleme-toplam" | awk '{print $NF}')
COMPILE_MBS=$(echo "${COMPILE_BENCH_RAW}" | grep "^Derleme verimi" | grep -o '[0-9.]*' | tail -1)

SUITE_SIZE_KB="$(du -sk "${SUITE_DIR}" 2>/dev/null | awk '{print $1}')"

# =============================================================================
# 3. Bölüm 2 — Çalışma hızı (fibonacci + sum)
# =============================================================================
info "Runtime benchmark — fibonacci (fib(25))..."
FIB_SQT_RAW="$("${SAQUT}" bench "${BENCH_DIR}/bench_fib.sqt" \
    --runs="${BENCH_RUNS}" 2>/dev/null)"
echo "${FIB_SQT_RAW}"
FIB_SAQUT_AVG=$(echo "${FIB_SQT_RAW}" | grep "^vm-execute" | awk '{print $(NF-1)}')
FIB_SAQUT_BEST=$(echo "${FIB_SQT_RAW}" | grep "^vm-execute" | awk '{print $NF}')

info "Runtime benchmark — döngü toplama (sum(40000))..."
SUM_SQT_RAW="$("${SAQUT}" bench "${BENCH_DIR}/bench_sum.sqt" \
    --runs="${BENCH_RUNS}" 2>/dev/null)"
echo "${SUM_SQT_RAW}"
SUM_SAQUT_AVG=$(echo "${SUM_SQT_RAW}" | grep "^vm-execute" | awk '{print $(NF-1)}')
SUM_SAQUT_BEST=$(echo "${SUM_SQT_RAW}" | grep "^vm-execute" | awk '{print $NF}')

# ── Python ───────────────────────────────────────────────────────────────────
PYTHON_BIN="$(command -v python3 || command -v python || echo "")"
FIB_PYTHON_BEST="N/A"; FIB_PYTHON_AVG="N/A"
SUM_PYTHON_BEST="N/A"; SUM_PYTHON_AVG="N/A"
PYTHON_VER="N/A"
if [[ -n "${PYTHON_BIN}" ]]; then
    PYTHON_VER="$("${PYTHON_BIN}" --version 2>&1)"
    info "Python fibonacci ölçülüyor..."
    eval "$("${PYTHON_BIN}" "${BENCH_DIR}/bench_fib.py" 2>/dev/null)"
    FIB_PYTHON_BEST="${PYTHON_FIB_BEST_US:-N/A}"
    FIB_PYTHON_AVG="${PYTHON_FIB_AVG_US:-N/A}"

    info "Python sum ölçülüyor..."
    eval "$("${PYTHON_BIN}" "${BENCH_DIR}/bench_sum.py" 2>/dev/null)"
    SUM_PYTHON_BEST="${PYTHON_SUM_BEST_US:-N/A}"
    SUM_PYTHON_AVG="${PYTHON_SUM_AVG_US:-N/A}"
fi

# ── Java ─────────────────────────────────────────────────────────────────────
JAVA_BIN="$(command -v java || echo "")"
JAVAC_BIN="$(command -v javac || echo "")"
FIB_JAVA_BEST="N/A"; FIB_JAVA_AVG="N/A"
SUM_JAVA_BEST="N/A"; SUM_JAVA_AVG="N/A"
JAVA_VER="N/A"
if [[ -n "${JAVA_BIN}" && -n "${JAVAC_BIN}" ]]; then
    JAVA_VER="$("${JAVA_BIN}" -version 2>&1 | head -1)"
    info "Java benchmark derleniyor..."
    JAVA_TMP="$(mktemp -d)"
    cp "${BENCH_DIR}/BenchFib.java" "${JAVA_TMP}/"
    cp "${BENCH_DIR}/BenchSum.java" "${JAVA_TMP}/"
    "${JAVAC_BIN}" "${JAVA_TMP}/BenchFib.java" -d "${JAVA_TMP}" 2>/dev/null
    "${JAVAC_BIN}" "${JAVA_TMP}/BenchSum.java" -d "${JAVA_TMP}" 2>/dev/null

    info "Java fibonacci ölçülüyor (1000 tur JIT ısınması + ${BENCH_RUNS} ölçüm)..."
    eval "$("${JAVA_BIN}" -cp "${JAVA_TMP}" BenchFib 2>/dev/null)"
    FIB_JAVA_BEST="${JAVA_FIB_BEST_US:-N/A}"
    FIB_JAVA_AVG="${JAVA_FIB_AVG_US:-N/A}"

    info "Java sum ölçülüyor..."
    eval "$("${JAVA_BIN}" -cp "${JAVA_TMP}" BenchSum 2>/dev/null)"
    SUM_JAVA_BEST="${JAVA_SUM_BEST_US:-N/A}"
    SUM_JAVA_AVG="${JAVA_SUM_AVG_US:-N/A}"
    rm -rf "${JAVA_TMP}"
fi

# =============================================================================
# 4. Oran hesapları
# =============================================================================
ratio() {
    local a="$1" b="$2"
    if [[ "${a}" == "N/A" || "${b}" == "N/A" || "${b}" == "0" ]]; then
        echo "N/A"
    else
        awk "BEGIN{printf \"%.1f\", ${a}/${b}}"
    fi
}

FIB_VS_PYTHON_BEST="$(ratio "${FIB_SAQUT_BEST}" "${FIB_PYTHON_BEST}")"
FIB_VS_JAVA_BEST="$(ratio "${FIB_SAQUT_BEST}" "${FIB_JAVA_BEST}")"
SUM_VS_PYTHON_BEST="$(ratio "${SUM_SAQUT_BEST}" "${SUM_PYTHON_BEST}")"
SUM_VS_JAVA_BEST="$(ratio "${SUM_SAQUT_BEST}" "${SUM_JAVA_BEST}")"

# =============================================================================
# 5. docs/benchmark.md üret
# =============================================================================
mkdir -p "${DOCS_DIR}"
cat > "${OUT_MD}" << MARKDOWN_EOF
# saQut Performans Baseline

> **Bu belge otomatik üretilmiştir.**
> Yeniden ölçmek için: \`bash scripts/bench/run_bench.sh\`
>
> Amaç: Bir **referans çizgisi** oluşturmak ve dondurmak.
> Diğer diller rakip değil — sabit **cetvel** (ilerideki saQut sürümleriyle oran kıyasına zemin).

---

## Ölçüm Koşulları (dondurulmuş)

| Konu               | Değer                                        |
|--------------------|----------------------------------------------|
| Tarih              | ${BUILD_DATE}                                |
| Commit             | \`${COMMIT_HASH}\`                           |
| Derleme modu       | **Release** (\`cmake -DCMAKE_BUILD_TYPE=Release\`, \`-O3\`) |
| Tekrar (BENCH_RUNS)| ${BENCH_RUNS}                               |
| Java ısınma        | 1000 tur (JIT stabil olana kadar atılır)     |
| Zamanlama          | \`std::chrono::high_resolution_clock\` (C++), \`time.perf_counter\` (Python), \`System.nanoTime\` (Java) |
| Raporlanan değer   | **best** (en düşük, sistem gürültüsünden en az etkilenen) + avg |
| Makine             | \`$(uname -sr)\` \`$(uname -m)\`            |

> Gelecekteki ölçümler **aynı koşulda** yapılmalıdır — aksi hâlde kıyas geçersiz olur.

---

## Bölüm 1 — Derleme Hızı (Frontend Pipeline)

**Test verisi:** ${SUITE_MODULES} modül, her birinde ${SUITE_FUNCS} fonksiyon, DAG import grafı
**Toplam kaynak:** ${SUITE_SIZE_KB} KB
**Araç:** \`saqut bench main_bench.sqt --compile-only --runs=${BENCH_RUNS}\`

| Aşama             | Ort (µs) | En iyi (µs) | Açıklama                        |
|-------------------|----------|-------------|----------------------------------|
| tokenize          | ${TOK_AVG:-?}   | ${TOK_BEST:-?}   | Tokenizer (tüm modüller)         |
| parse             | ${PARSE_AVG:-?} | ${PARSE_BEST:-?} | Parser (tüm modüller)            |
| symbol-collect    | ${SYM_AVG:-?}   | ${SYM_BEST:-?}   | 3-geçiş sembol toplama           |
| type-check        | ${TC_AVG:-?}    | ${TC_BEST:-?}    | Tip denetimi + yapısal doğrulama |
| ir-gen            | ${IR_AVG:-?}    | ${IR_BEST:-?}    | IR üretimi                       |
| **derleme-toplam**| **${COMP_TOTAL_AVG:-?}** | **${COMP_TOTAL_BEST:-?}** | Yukarıdaki 5 aşama toplamı |

**Derleme verimi (best):** ${COMPILE_MBS:-?} MB/s

> **Not:** \`tokenize\` ve \`parse\` ayrı ölçülür çünkü \`bench\` komutu her modül
> için \`Tokenizer::scan()\` ve \`Parser::parse()\` aşamalarını sırayla zamanlar.

> **Cetvel karşılaştırması:** Java/Python modül sistemleri farklı olduğu için bu
> bölümde cetvel dili yoktur. Bu ölçüm, gelecekteki saQut sürümleriyle kıyaslanır.

---

## Bölüm 2 — Çalışma Hızı (VM Runtime)

**Araç:** \`saqut bench <dosya>.sqt --runs=${BENCH_RUNS}\` → \`vm-execute\` satırı
**Cetvel diller:** CPython ${PYTHON_VER} / ${JAVA_VER}

### (a) Özyinelemeli Fibonacci — \`fib(25)\` = 75025

| Dil/VM             | Ort (µs)           | En iyi (µs)        |
|--------------------|--------------------|--------------------|
| **saQut (bytecode VM)** | **${FIB_SAQUT_AVG:-?}** | **${FIB_SAQUT_BEST:-?}** |
| CPython            | ${FIB_PYTHON_AVG:-N/A} | ${FIB_PYTHON_BEST:-N/A} |
| Java (JIT, +1000 ısınma) | ${FIB_JAVA_AVG:-N/A} | ${FIB_JAVA_BEST:-N/A} |

**saQut / CPython oranı (best):** ${FIB_VS_PYTHON_BEST}x yavaş
**saQut / Java oranı (best):**    ${FIB_VS_JAVA_BEST}x yavaş

> Kıyaslama notu: CPython = saf yorumlayıcı (saQut'un gerçek akranı).
> Java = JIT-derlenmiş (uzak referans; JIT ısınması atılmıştır).

### (b) Döngü Toplama — \`sum(40000)\`, döngüsüz değer 800,020,000

| Dil/VM             | Ort (µs)           | En iyi (µs)        |
|--------------------|--------------------|--------------------|
| **saQut (bytecode VM)** | **${SUM_SAQUT_AVG:-?}** | **${SUM_SAQUT_BEST:-?}** |
| CPython            | ${SUM_PYTHON_AVG:-N/A} | ${SUM_PYTHON_BEST:-N/A} |
| Java (JIT, +1000 ısınma) | ${SUM_JAVA_AVG:-N/A} | ${SUM_JAVA_BEST:-N/A} |

**saQut / CPython oranı (best):** ${SUM_VS_PYTHON_BEST}x yavaş
**saQut / Java oranı (best):**    ${SUM_VS_JAVA_BEST}x yavaş

---

## Yorum

Bu ölçümler saQut'un **mevcut hâlinin fotoğrafıdır** — optimize edilmemiş
bytecode VM, tek-geçişli IR, JIT yok. Amaç iyileştirmek değil, belgelemek.

Kullanım biçimi:
- **Regresyon tespiti:** Bir özellik eklenince \`run_bench.sh\` yeniden koştur.
  Eğer derleme aşamalarından biri kayda değer uzadıysa (> 10%) incelenmelidir.
- **Cetvel oranı:** Gelecekte JIT veya derleyici optimizasyonu eklenince
  CPython/Java oranlarının değişimini buradaki referansla kıyasla.

---

## Tekrar Çalıştırma

\`\`\`bash
# Release build + ölçüm (tüm bölümler)
bash scripts/bench/run_bench.sh

# Yalnızca tek dosya ölç (hızlı kontrol)
./build/saqut bench <dosya.sqt> --runs=5

# Yalnızca derleme (VM atla)
./build/saqut bench <dosya.sqt> --runs=5 --compile-only
\`\`\`
MARKDOWN_EOF

info "Benchmark tamamlandı → ${OUT_MD}"
echo ""
echo "=== ÖZET ==="
echo "  Derleme verimi  : ${COMPILE_MBS:-?} MB/s  (${SUITE_MODULES} modül, ${SUITE_SIZE_KB} KB)"
echo "  fib(25) saQut   : ${FIB_SAQUT_BEST:-?} µs   vs CPython: ${FIB_PYTHON_BEST:-N/A} µs (${FIB_VS_PYTHON_BEST}x)   Java: ${FIB_JAVA_BEST:-N/A} µs (${FIB_VS_JAVA_BEST}x)"
echo "  sum(40k) saQut  : ${SUM_SAQUT_BEST:-?} µs   vs CPython: ${SUM_PYTHON_BEST:-N/A} µs (${SUM_VS_PYTHON_BEST}x)   Java: ${SUM_JAVA_BEST:-N/A} µs (${SUM_VS_JAVA_BEST}x)"
