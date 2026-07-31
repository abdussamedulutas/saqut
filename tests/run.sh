#!/usr/bin/env bash
# saQut test koşucusu — birim testler + golden testler
# Kullanım: bash tests/run.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CXX="${CXX:-g++}"
FLAGS=(-std=c++20 -Wall -Wextra -I"$ROOT/src")
SAQUT="$ROOT/build/saqut"

# ── Birim testler ─────────────────────────────────────────────────────────────
for t in test_type test_diagnostic test_opcode test_value_rep_contract test_cfg; do
    echo "=== $t ==="
    # test_cfg buildCFG gerçeklemesini (ir_cfg.cpp) da derler — hata sınıfı
    # testi için implementasyon gerekli (#218).
    extra=""
    [ "$t" = "test_cfg" ] && extra="$ROOT/src/ir/ir_cfg.cpp"
    "$CXX" "${FLAGS[@]}" "$ROOT/tests/$t.cpp" $extra -o "/tmp/saqut_$t"
    "/tmp/saqut_$t"
done

# ── Golden testler ────────────────────────────────────────────────────────────
echo "=== golden ==="

if [ ! -x "$SAQUT" ]; then
    echo "HATA: $SAQUT bulunamadı — önce derleyin (cmake --build build)"
    exit 1
fi

PASS=0; FAIL=0
while IFS= read -r -d '' sqt; do
    dir=$(dirname "$sqt")
    base=$(basename "$sqt" .sqt)
    exp="$dir/$base.expected"
    [ -f "$exp" ] || continue

    # ADR-036 (#76): BASE.flags — --allow-fs vb. gerektiren testler.
    extra_flags=()
    flags_file="$dir/$base.flags"
    if [ -f "$flags_file" ]; then
        mapfile -t extra_flags < "$flags_file"
    fi

    actual=$("$SAQUT" run "${extra_flags[@]}" "$sqt" 2>/dev/null) || true
    expected=$(cat "$exp")

    if [ "$actual" = "$expected" ]; then
        PASS=$((PASS + 1))
    else
        echo "  FAIL: ${sqt#"$ROOT"/}"
        echo "    beklenen : $(echo "$expected" | head -1)"
        echo "    gerçek   : $(echo "$actual"   | head -1)"
        FAIL=$((FAIL + 1))
    fi
done < <(find "$ROOT/tests/golden" -name "*.sqt" -print0 | sort -z)

echo "  $PASS geçti, $FAIL başarısız"
[ "$FAIL" -eq 0 ] || exit 1

# ── Diferansiyel test (#92): VM ≡ MIR JIT ────────────────────────────────────
# Her golden fixture'ı hem VM hem JIT ile koşup stdout+exit code'u bayt-bayt
# karşılaştırır. JIT şu an MIR dilimlerinin kapsadığı opcode alt kümesiyle
# sınırlı (mir_backend.hpp) — bir fixture bu kümenin dışına çıkan bir opcode
# içeriyorsa (struct/array/global/try-catch/nullable vb.) JIT programın
# TAMAMINI reddeder (kısmi JIT yok); bu durum parity hatası DEĞİL, henüz
# kapsanmamış bir dilim demektir → SKIP sayılır, FAIL sayılmaz.
echo "=== diferansiyel (VM≡JIT) ==="
DPASS=0; DFAIL=0; DSKIP=0
while IFS= read -r -d '' sqt; do
    dir=$(dirname "$sqt")
    base=$(basename "$sqt" .sqt)
    exp="$dir/$base.expected"
    [ -f "$exp" ] || continue

    extra_flags=()
    flags_file="$dir/$base.flags"
    if [ -f "$flags_file" ]; then
        mapfile -t extra_flags < "$flags_file"
    fi

    set +e
    jit_err=$("$SAQUT" run --jit "${extra_flags[@]}" "$sqt" 2>&1 >/dev/null)
    set -e
    if echo "$jit_err" | grep -q "desteklenmeyen opcode"; then
        DSKIP=$((DSKIP + 1))
        continue
    fi

    set +e
    vm_out=$("$SAQUT" run "${extra_flags[@]}" "$sqt" 2>/dev/null); vm_exit=$?
    jit_out=$("$SAQUT" run --jit "${extra_flags[@]}" "$sqt" 2>/dev/null); jit_exit=$?
    set -e

    if [ "$vm_out" = "$jit_out" ] && [ "$vm_exit" = "$jit_exit" ]; then
        DPASS=$((DPASS + 1))
    elif [ -f "$dir/$base.jit_known_broken" ]; then
        # CMakeLists.txt'teki WILL_FAIL mekanizmasıyla aynı fikir: JIT bu
        # fixture'da bilinen, ayrı issue'da kayıtlı bir crash/parity hatası
        # üretiyor. VM (normatif) doğru; JIT [EXPERIMENTAL], parity bu
        # release kapısının şartı değil (AGENTS.md §9).
        DSKIP=$((DSKIP + 1))
    else
        echo "  FAIL (parity): ${sqt#"$ROOT"/} (vm_exit=$vm_exit jit_exit=$jit_exit)"
        echo "    VM  : $(echo "$vm_out"  | head -1)"
        echo "    JIT : $(echo "$jit_out" | head -1)"
        DFAIL=$((DFAIL + 1))
    fi
done < <(find "$ROOT/tests/golden" -name "*.sqt" -print0 | sort -z)

echo "  $DPASS geçti, $DFAIL başarısız, $DSKIP atlandı (JIT henüz desteklemiyor)"
[ "$DFAIL" -eq 0 ] || exit 1

# ── Modül döngüsü testleri (ADR-031, #78) ────────────────────────────────────
# Döngüsel bağımlılık E_MODULE_CYCLE tanısı + sıfır-dışı exit üretmeli.
echo "=== modül döngüsü ==="
for f in cycle_a self_import; do
    if out=$("$SAQUT" check "$ROOT/tests/module/$f.sqt" 2>/dev/null); then
        echo "  FAIL: $f.sqt — döngüde exit 0 döndü"; exit 1
    fi
    if ! echo "$out" | grep -q "E_MODULE_CYCLE"; then
        echo "  FAIL: $f.sqt — E_MODULE_CYCLE tanısı yok"; exit 1
    fi
done
echo "  2 geçti, 0 başarısız"

# ── GC testleri (#77, ADR-022) ───────────────────────────────────────────────
# 1) Varsayılan eşikle koşuda GC gerçekten tetikleniyor (runs >= 1)
# 2) Stress modda (--gc-threshold=1) çıktı normal koşuyla aynı (canlılık)
# 3) --gc-threshold=-1 otomatik GC'yi kapatıyor (runs=0)
echo "=== gc ==="
GC_SQT="$ROOT/tests/golden/gc/liveness.sqt"
gcout=$("$SAQUT" run --gc-stats "$GC_SQT" 2>&1 >/dev/null)
if ! echo "$gcout" | grep -Eq "gc: runs=[1-9]"; then
    echo "  FAIL: varsayılan eşikte GC hiç koşmadı: $gcout"; exit 1
fi
stress=$("$SAQUT" run --gc-threshold=1 "$GC_SQT" 2>/dev/null)
normal=$("$SAQUT" run "$GC_SQT" 2>/dev/null)
if [ "$stress" != "$normal" ]; then
    echo "  FAIL: stress modda (--gc-threshold=1) çıktı farklı"; exit 1
fi
gcoff=$("$SAQUT" run --gc-threshold=-1 --gc-stats "$GC_SQT" 2>&1 >/dev/null)
if ! echo "$gcoff" | grep -q "runs=0"; then
    echo "  FAIL: --gc-threshold=-1 GC'yi kapatmadı: $gcoff"; exit 1
fi
echo "  3 geçti, 0 başarısız"

# ── Builtin sözdizimi testleri (ADR-033, #85) ────────────────────────────────
# 1) Eski ElemTip::metod sözdizimi W006 uyarısı verir ama çalışır (exit 0)
# 2) Struct alanı builtin'i gölgeler: k.length() alan varken derleme hatası
echo "=== builtin sözdizimi ==="
legout=$("$SAQUT" check "$ROOT/tests/semantic/legacy_builtin.sqt" 2>/dev/null) || {
    echo "  FAIL: legacy_builtin.sqt derlenmeliydi (yalnızca W)"; exit 1; }
if ! echo "$legout" | grep -q "W006"; then
    echo "  FAIL: eski sözdizimi W006 uyarısı üretmedi"; exit 1
fi
if out=$("$SAQUT" check "$ROOT/tests/semantic/field_shadow.sqt" 2>/dev/null); then
    echo "  FAIL: field_shadow.sqt derlenmemeliydi (alan gölgeleme)"; exit 1
fi
if ! echo "$out" | grep -q "is a field of struct"; then
    echo "  FAIL: alan gölgeleme tanısı beklenen mesajı içermiyor"; exit 1
fi
echo "  2 geçti, 0 başarısız"

echo "=== TUM TESTLER GECTI ==="
