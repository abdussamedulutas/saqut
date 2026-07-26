#!/usr/bin/env bash
set -eu
binary=$1
root=$2
clean="$root/tests/general/check_jsonl_clean.sqt"
bad="$root/tests/general/check_jsonl_error.sqt"
out=$(mktemp); err=$(mktemp); trap 'rm -f "$out" "$err"' EXIT
set +e
"$binary" check "$clean" >"$out" 2>"$err"; e=$?
set -e
test "$e" -eq 0
grep -F '"kind":"check.header"' "$out" >/dev/null
grep -F '"kind":"check.end"' "$out" >/dev/null
set +e
"$binary" check "$bad" >"$out" 2>"$err"; e=$?
set -e
test "$e" -eq 65
grep -F '"kind":"check.diagnostic"' "$out" >/dev/null
set +e
"$binary" check --compact "$clean" >"$out" 2>"$err"; e=$?
set -e
test "$e" -eq 64
test ! -s "$out"
