#!/usr/bin/env bash
# #163: argümansız exec semantic diagnostic yerine merkezi usage error üretir.
set -eu

binary=$1
tmpdir=$(mktemp -d /tmp/saqut_exec_usage_XXXXXX)
trap 'rm -rf "$tmpdir"' EXIT

expect_usage() {
    label=$1
    shift
    out="$tmpdir/${label}.stdout"
    err="$tmpdir/${label}.stderr"

    set +e
    "$binary" exec "$@" >"$out" 2>"$err"
    actual=$?
    set -e

    if [ "$actual" -ne 64 ]; then
        echo "FAIL: exec $label beklenen exit 64, gerçek $actual" >&2
        cat "$err" >&2
        exit 1
    fi
    if [ -s "$out" ]; then
        echo "FAIL: exec $label stdout boş olmalı, gerçek: $(cat "$out")" >&2
        exit 1
    fi
    if ! grep -Fq 'usage: saqut exec "<expression>"' "$err"; then
        echo "FAIL: exec $label usage mesajı içermiyor" >&2
        cat "$err" >&2
        exit 1
    fi
    if grep -Eq 'E001|source is not defined|<exec>:' "$err"; then
        echo "FAIL: exec $label semantic diagnostic'e düştü" >&2
        cat "$err" >&2
        exit 1
    fi
}

expect_usage no_args
expect_usage jit_only --jit

out="$tmpdir/positive.stdout"
err="$tmpdir/positive.stderr"
set +e
"$binary" exec '1 + 2' >"$out" 2>"$err"
actual=$?
set -e

if [ "$actual" -ne 0 ]; then
    echo "FAIL: exec positive beklenen exit 0, gerçek $actual" >&2
    cat "$err" >&2
    exit 1
fi
if [ "$(cat "$out")" != "3" ]; then
    echo "FAIL: exec positive stdout beklenen 3, gerçek: $(cat "$out")" >&2
    exit 1
fi
if [ -s "$err" ]; then
    echo "FAIL: exec positive stderr boş olmalı" >&2
    cat "$err" >&2
    exit 1
fi
