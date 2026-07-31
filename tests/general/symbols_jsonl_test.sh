#!/usr/bin/env bash
# #145 (SQ-100-SYMBOLS-JSONL): saqut symbols varsayılan insan-okur metni
# KORUR; makine yüzeyi açıkça --jsonl ile seçilir.
#   symbols.header (schemaVersion bir kez) → symbol/diagnostic kayıtları
#   satır başına → symbols.end (errors/warnings/symbolCount sayaçları).
#   Eski --json preview kaldırıldı (64); --compact JSONL'de anlamsız (64).
#   Error-tolerant davranış ve nonzero exit (65) korunur.
set -eu

binary=$1
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

clean="$tmp/clean.sqt"
err="$tmp/err.sqt"

printf 'int main() { int x = 5; print(x); return 0; }\n' > "$clean"
printf 'int main() { print(undefined_thing); return 0; }\n' > "$err"

fail() { echo "FAIL: $1" >&2; exit 1; }

# 1) Varsayılan metin çıktı KORUNUR — symbols başlığında main sembolü var,
#    exit 0; stdout'a renksiz içerik basılır (ANSI kodları filtreli değilse
#    bile "main" metni oradadır).
set +e
text=$("$binary" symbols "$clean" 2>/dev/null); status=$?
set -e
[ "$status" -eq 0 ] || fail "varsayılan metin modu exit 0 olmali, gercek $status"
echo "$text" | grep -q "main" || fail "metin modu main sembolünü basmali: $(echo "$text" | head -1)"
# Metin modu JSONL üretmez
if echo "$text" | grep -q '"record":"symbols.header"'; then
    fail "varsayılan metin modu JSONL üretmemeli"
fi

# 2) --jsonl: header + en az 2 symbol (main, x) + end; exit 0.
set +e
out=$("$binary" symbols --jsonl "$clean" 2>/dev/null); status=$?
set -e
[ "$status" -eq 0 ] || fail "jsonl temiz dosya exit 0 olmali, gercek $status"
first=$(printf '%s\n' "$out" | head -1)
last=$(printf '%s\n' "$out"  | tail -1)
echo "$first" | grep -q '"record":"symbols.header"' || fail "ilk kayit symbols.header olmali: $first"
echo "$first" | grep -q '"schemaVersion":1' || fail "schemaVersion ilk kayitta olmali: $first"
echo "$last"  | grep -q '"record":"symbols.end"' || fail "son kayit symbols.end olmali: $last"
echo "$last"  | grep -q '"errors":0,"warnings":0' || fail "temiz sayaçlar 0 olmali: $last"
echo "$last"  | grep -q '"symbolCount":[1-9]' || fail "symbolCount en az 1 olmali: $last"
[ "$(printf '%s\n' "$out" | grep -c 'schemaVersion')" -eq 1 ] || fail "schemaVersion yalniz header'da"
echo "$out" | grep -q '"record":"symbols.symbol"' || fail "symbol kaydi yok"
echo "$out" | grep -q '"kind":"function".*"name":"main"' || fail "main fonksiyon kaydi yok"
echo "$out" | grep -q '"name":"x"' || fail "x değişken kaydi yok"

# 3) Semantik hata → header + diagnostic kayıtları + end, exit 65; error-tolerant
#    (parser/collector hataları toplanır, exit 65 korunur).
set +e
out=$("$binary" symbols --jsonl "$err" 2>/dev/null); status=$?
set -e
[ "$status" -eq 65 ] || fail "hatali dosya exit 65 olmali, gercek $status"
echo "$out" | grep -q '"record":"symbols.diagnostic"' || fail "diagnostic kaydi yok"
echo "$out" | grep -q '"code":"E001"' || fail "E001 kodu yok"
echo "$out" | tail -1 | grep -q '"errors":1' || fail "end errors sayaci 1 olmali"
printf '%s\n' "$out" | head -1 | grep -q 'symbols.header' || fail "hatali durumda da header ilk kayit"

# 4) Sıra deterministik — aynı girdi → bayt-bayt aynı çıktı.
set +e
out1=$("$binary" symbols --jsonl "$err" 2>/dev/null)
out2=$("$binary" symbols --jsonl "$err" 2>/dev/null)
set -e
[ "$out1" = "$out2" ] || fail "JSONL determinizm bozuldu"

# 5) --json kaldırıldı (exit 64, boş stdout); --jsonl --compact anlamsız (64).
set +e
"$binary" symbols --json "$clean" > "$tmp/o" 2>"$tmp/e"; s1=$?
"$binary" symbols --jsonl --compact "$clean" > "$tmp/o2" 2>"$tmp/e2"; s2=$?
set -e
[ "$s1" -eq 64 ] || fail "symbols --json exit 64 olmali, gercek $s1"
[ ! -s "$tmp/o" ] || fail "symbols --json stdout bos olmali"
grep -qi "jsonl" "$tmp/e" || fail "symbols --json stderr --jsonl yonlendirmesi icermiyor"
[ "$s2" -eq 64 ] || fail "symbols --jsonl --compact exit 64 olmali, gercek $s2"
grep -qi "compact" "$tmp/e2" || fail "symbols --jsonl --compact stderr acik mesaj icermiyor"

echo "symbols_jsonl: TUM TESTLER GECTI"
