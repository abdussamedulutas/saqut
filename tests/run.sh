#!/usr/bin/env bash
# saQut birim testleri — çerçevesiz (assert tabanlı), tek komutla.
# Kullanım: bash tests/run.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CXX="${CXX:-g++}"
FLAGS=(-std=c++20 -Wall -Wextra -I"$ROOT/src")

for t in test_type test_diagnostic; do
    echo "=== $t ==="
    "$CXX" "${FLAGS[@]}" "$ROOT/tests/$t.cpp" -o "/tmp/saqut_$t"
    "/tmp/saqut_$t"
done

echo "=== TUM TESTLER GECTI ==="
