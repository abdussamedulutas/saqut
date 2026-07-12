#!/usr/bin/env bash
# saQut test koşucusu — birim testler + golden testler
# Kullanım: bash tests/run.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CXX="${CXX:-g++}"
FLAGS=(-std=c++20 -Wall -Wextra -I"$ROOT/src")
SAQUT="$ROOT/build/saqut"

# ── Birim testler ─────────────────────────────────────────────────────────────
for t in test_type test_diagnostic; do
    echo "=== $t ==="
    "$CXX" "${FLAGS[@]}" "$ROOT/tests/$t.cpp" -o "/tmp/saqut_$t"
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

    actual=$("$SAQUT" run "$sqt" 2>/dev/null) || true
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

echo "=== TUM TESTLER GECTI ==="
