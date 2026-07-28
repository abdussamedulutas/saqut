#!/usr/bin/env bash
set -eu
binary=$1
root=$2
clean="$root/tests/general/check_jsonl_clean.sqt"
bad="$root/tests/general/check_jsonl_error.sqt"
big="$root/tests/general/check_jsonl_20modules.sqt"
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

# P3: her satır bağımsız geçerli JSON (bad fixture — header + diagnostic + end)
"$binary" check "$bad" >"$out" 2>"$err" || true
while IFS= read -r line; do
    printf '%s' "$line" | python3 -c "import json,sys; json.loads(sys.stdin.read())" \
        || { echo "FAIL: gecersiz JSON satiri: $line" >&2; exit 1; }
done < "$out"

# P4 (Başmimar review — Amendment 01): 20+ modüllü fixture, streaming
# tüketim. İlk satır header, son satır end (errorCount=0, warningCount=20),
# aradaki 20 satırın her biri check.diagnostic. `head -n 3` ile kesilen
# çıktının ilk satırları hâlâ geçerli JSON olmalı (streaming tüketilebilir).
"$binary" check "$big" >"$out" 2>"$err"; e=$?
test "$e" -eq 0
total_lines=$(wc -l < "$out")
test "$total_lines" -eq 22
first_line=$(head -n1 "$out")
last_line=$(tail -n1 "$out")
printf '%s' "$first_line" | grep -F '"kind":"check.header"' >/dev/null
printf '%s' "$last_line"  | grep -F '"kind":"check.end"' >/dev/null
printf '%s' "$last_line"  | grep -F '"warningCount":20' >/dev/null
diag_count=$(grep -c '"kind":"check.diagnostic"' "$out")
test "$diag_count" -eq 20
head -n 3 "$out" | while IFS= read -r line; do
    printf '%s' "$line" | python3 -c "import json,sys; json.loads(sys.stdin.read())" \
        || { echo "FAIL: kesilmis stream ilk satirlari gecersiz JSON: $line" >&2; exit 1; }
done

# P5: determinizm — üç kez byte-identical
out2=$(mktemp); out3=$(mktemp)
trap 'rm -f "$out" "$err" "$out2" "$out3"' EXIT
"$binary" check "$big" > "$out2" 2>/dev/null
"$binary" check "$big" > "$out3" 2>/dev/null
diff "$out" "$out2" >/dev/null
diff "$out2" "$out3" >/dev/null
