#!/usr/bin/env bash
# #145 SQ-100-SYMBOLS-JSONL — tracked regresyon.
set -eu

binary=$1
root=$2
clean="$root/tests/general/symbols_jsonl_clean.sqt"
bad="$root/tests/general/symbols_jsonl_error.sqt"

out=$(mktemp); err=$(mktemp)
trap 'rm -f "$out" "$err"' EXIT

# P1: header ilk, end son (temiz fixture); exit 0.
"$binary" symbols --jsonl "$clean" > "$out" 2>"$err"
head -n1 "$out" | grep -F '"kind":"symbols.header"' >/dev/null
head -n1 "$out" | grep -F '"schemaVersion":1' >/dev/null
tail -n1 "$out" | grep -F '"kind":"symbols.end"' >/dev/null

# P3: her satır bağımsız gecerli JSON.
while IFS= read -r line; do
    printf '%s' "$line" | python3 -c "import json,sys; json.loads(sys.stdin.read())" \
        || { echo "FAIL: gecersiz JSON satiri: $line" >&2; exit 1; }
done < "$out"

# P3b: schemaVersion yalniz ilk satirda.
count=$(grep -c '"schemaVersion"' "$out")
test "$count" -eq 1

# P4 (hata toleransli): hatali kaynak -> diagnostic kaydi + exit 65.
set +e
"$binary" symbols --jsonl "$bad" > "$out" 2>"$err"
actual=$?
set -e
test "$actual" -eq 65
grep -F '"kind":"diagnostic"' "$out" >/dev/null

# P5: determinizm — üç kez byte-identical.
o2=$(mktemp); o3=$(mktemp)
trap 'rm -f "$out" "$err" "$o2" "$o3"' EXIT
"$binary" symbols --jsonl "$clean" > "$out" 2>/dev/null
"$binary" symbols --jsonl "$clean" > "$o2" 2>/dev/null
"$binary" symbols --jsonl "$clean" > "$o3" 2>/dev/null
diff "$out" "$o2" >/dev/null
diff "$o2" "$o3" >/dev/null

# N1: --json kaldirildi -> exit 64, stdout bos.
set +e
"$binary" symbols --json "$clean" > "$out" 2>"$err"
actual=$?
set -e
test "$actual" -eq 64
test ! -s "$out"

# N2: --jsonl --compact -> exit 64, sessiz yutma yok.
set +e
"$binary" symbols --jsonl --compact "$clean" > "$out" 2>"$err"
actual=$?
set -e
test "$actual" -eq 64

# N3 (regresyon): bayraksiz text varsayilan hala calisir.
set +e
"$binary" symbols "$clean" > "$out" 2>"$err"
actual=$?
set -e
test "$actual" -eq 0
test -s "$out"
