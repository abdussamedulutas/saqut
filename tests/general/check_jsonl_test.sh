#!/usr/bin/env bash
# #144 (SQ-100-CHECK-JSONL): saqut check stdout canonical JSONL olmalı.
#   İlk kayıt check.header (schemaVersion yalnız burada), sonra 0+ diagnostic,
#   son kayıt check.end (sayaçlar). Sıra deterministik; exit 0 / 65 (kDataError).
#   check.end yoksa stream incomplete sayılır — bu test end'in her zaman
#   mevcut olduğunu ve sayaçların doğru olduğunu doğrular.
set -eu

binary=$1
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

clean="$tmp/clean.sqt"
err="$tmp/err.sqt"
multi="$tmp/multi.sqt"
warn="$tmp/warn.sqt"

printf 'int main() { print(1); return 0; }\n' > "$clean"
printf 'int main() { print(undefined_thing); return 0; }\n' > "$err"
printf 'int main() {\n  int x = 1;\n  print(x + unknown_a);\n  print(unknown_b);\n  return 0;\n}\n' > "$multi"
# W006 (ADR-033 eski builtin sözdizimi) — check pipeline'ında üretilen tek
# uyarı örneği (TypeChecker): uyarı exit kodunu değiştirmemeli (exit 0).
printf 'int main() {\n  int[] a = [1, 2];\n  print(int::length(a));\n  return 0;\n}\n' > "$warn"

fail() { echo "FAIL: $1" >&2; exit 1; }

# 1) Temiz dosya → header + end (2 kayıt), exit 0, sayaçlar 0.
set +e
out=$("$binary" check "$clean" 2>/dev/null); status=$?
set -e
[ "$status" -eq 0 ] || fail "temiz dosya exit 0 olmali, gercek $status"
[ "$(printf '%s\n' "$out" | grep -c '^{')" -eq 2 ] || fail "temiz dosya 2 kayit olmali (header+end)"
first=$(printf '%s\n' "$out" | head -1)
last=$(printf '%s\n' "$out"  | tail -1)
echo "$first" | grep -q '"record":"check.header"' || fail "ilk kayit check.header olmali: $first"
echo "$first" | grep -q '"schemaVersion":1' || fail "schemaVersion ilk kayitta olmali: $first"
echo "$last"  | grep -q '"record":"check.end"' || fail "son kayit check.end olmali: $last"
echo "$last"  | grep -q '"errors":0,"warnings":0' || fail "temiz sayaçlar 0 olmali: $last"
# schemaVersion yalnız header'da (1 kez)
[ "$(printf '%s\n' "$out" | grep -c 'schemaVersion')" -eq 1 ] || fail "schemaVersion yalniz header'da"

# 2) Semantik hata → header + 1 diagnostic + end, exit 65.
set +e
out=$("$binary" check "$err" 2>/dev/null); status=$?
set -e
[ "$status" -eq 65 ] || fail "hatali dosya exit 65 olmali, gercek $status"
[ "$(printf '%s\n' "$out" | grep -c '^{')" -eq 3 ] || fail "hatali dosya 3 kayit olmali (header+diag+end)"
echo "$out" | grep -q '"record":"check.diagnostic"' || fail "diagnostic kaydi yok"
echo "$out" | grep -q '"code":"E001"' || fail "E001 kodu diagnostic'ta yok"
echo "$out" | grep -q '"file":' || fail "diagnostic file taşımıyor"
echo "$out" | grep -q '"line":1' || fail "diagnostic line taşımıyor"
echo "$out" | grep -q '"column":20' || fail "diagnostic column taşımıyor"
echo "$out" | grep -q '"offset":19' || fail "diagnostic offset taşımıyor"

# 3) Çoklu hata → sıra deterministik (aynı girdi → bayt-bayt aynı çıktı),
#    kaynak sırası korunuyor (unknown_a önce, unknown_b sonra).
set +e
out1=$("$binary" check "$multi" 2>/dev/null); s1=$?
out2=$("$binary" check "$multi" 2>/dev/null); s2=$?
set -e
[ "$s1" -eq 65 ] && [ "$s2" -eq 65 ] || fail "multi dosya exit 65 olmali"
[ "$out1" = "$out2" ] || fail "JSONL determinizm bozuldu (iki koşu farklı)"
[ "$(printf '%s\n' "$out1" | grep -c 'check.diagnostic')" -eq 2 ] || fail "2 diagnostic bekleniyor"
# nlohmann JSON alanları alfabetik sıralıdır — line/column bitişik değil;
# bu yüzden her alan ayrı grep -o ile çekilir (|| true: set -e koruması).
line_a=$(printf '%s\n' "$out1" | grep 'unknown_a' | grep -o '"line":[0-9]*' || true)
line_b=$(printf '%s\n' "$out1" | grep 'unknown_b' | grep -o '"line":[0-9]*' || true)
[ "$line_a" = '"line":3' ] || fail "unknown_a satır 3'te olmali: $line_a"
[ "$line_b" = '"line":4' ] || fail "unknown_b satır 4'te olmali: $line_b"
echo "$out1" | tail -1 | grep -q '"errors":2,"warnings":0' || fail "end sayaçları (2 hata) yanlış"

# 4) Yalnız uyarı → exit 0, warnings sayacı dolu.
set +e
out=$("$binary" check "$warn" 2>/dev/null); status=$?
set -e
[ "$status" -eq 0 ] || fail "yalniz uyari exit 0 olmali, gercek $status"
echo "$out" | grep -q '"record":"check.diagnostic"' || fail "uyari diagnostic kaydi yok"
echo "$out" | grep -q '"code":"W006"' || fail "W006 uyarisi yok"
echo "$out" | tail -1 | grep -q '"errors":0,"warnings":1' || fail "end sayaçları (1 uyarı) yanlış"

# 5) Syntax hatası → exit 65, diagnostic kodu E serisi (parser E905 ailesi).
printf 'int main() { print(1) }' > "$tmp/syntax.sqt"
set +e
out=$("$binary" check "$tmp/syntax.sqt" 2>/dev/null); status=$?
set -e
[ "$status" -eq 65 ] || fail "syntax hatasi exit 65 olmali, gercek $status"
echo "$out" | grep -q '"level":"error"' || fail "syntax hatasi error seviyesinde olmali"

echo "check_jsonl: TUM TESTLER GECTI"
