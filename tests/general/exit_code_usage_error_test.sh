#!/usr/bin/env bash
# #156 SQ-090-EXIT-CODE-INTEGRATION — kUsageError (64) tracked regresyon.
# "-" (stdin modu) run/check/ir için henüz desteklenmiyor (TODO: stdin);
# dosya argümanı gerçekten yokken bu üç komut da CLI kullanım hatası
# (64) vermeli, diagnostic (65) veya runtime (70) sınıfına düşmemeli.
set -eu

binary=$1

for cmd in run check ir; do
    set +e
    "$binary" "$cmd" - >/tmp/exit_code_usage_error_stdout 2>/tmp/exit_code_usage_error_stderr
    actual=$?
    set -e
    if [ "$actual" -ne 64 ]; then
        echo "FAIL: saqut $cmd - beklenen exit 64, gerçek $actual" >&2
        cat /tmp/exit_code_usage_error_stderr >&2
        exit 1
    fi
done
rm -f /tmp/exit_code_usage_error_stdout /tmp/exit_code_usage_error_stderr
