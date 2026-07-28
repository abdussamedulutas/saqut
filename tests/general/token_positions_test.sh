#!/usr/bin/env bash
set -eu

binary=$1
root=$2
utf8="$root/tests/general/token_positions_utf8.sqt"
multi="$root/tests/general/token_positions_multiline.sqt"
escapes="$root/tests/general/token_positions_escapes.sqt"

out1=$(mktemp)
out2=$(mktemp)
out3=$(mktemp)
tmpdir=$(mktemp -d)
trap 'rm -f "$out1" "$out2" "$out3"; rm -rf "$tmpdir"' EXIT

"$binary" tokens "$utf8" > "$out1"
grep -F 'byteLength=6' "$out1" >/dev/null

"$binary" tokens "$multi" > "$out2"
grep -F 'line=4' "$out2" >/dev/null

diff <("$binary" tokens "$utf8") <("$binary" tokens "$utf8")

# BAŞMİMAR — REVIEW (#146, Amendment 02): bir token kaydı bir fiziksel satır
# olmalı. Kaynakta gerçek escape edilen (\n \t \" \\) bir string literal,
# eski std::quoted yaklaşımıyla kaydı birden fazla satıra bölüyordu. Satır
# sayısı == token sayısı + 1 (başlık) invariant'ı bunu doğrular: kırılırsa
# ya kayıt bölünmüştür ya da beklenmeyen ekstra satır üretilmiştir.
"$binary" tokens "$escapes" > "$out3"
token_count=$(head -n1 "$out3" | grep -oE '[0-9]+')
line_count=$(wc -l < "$out3")
record_count=$(grep -c '^  \[' "$out3")
if [ "$line_count" -ne "$((token_count + 1))" ]; then
    echo "FAIL: beklenen $((token_count + 1)) fiziksel satır, gerçek $line_count (kayıt bölünmüş olabilir)" >&2
    exit 1
fi
if [ "$record_count" -ne "$token_count" ]; then
    echo "FAIL: beklenen $token_count '  [' başlangıçlı kayıt satırı, gerçek $record_count" >&2
    exit 1
fi
# Kontrol karakterleri görünür, tersine çevrilebilir escape'lere dönüşmüş
# olmalı — kaçılmamış ham newline/tab tek bir alan içinde kalmamalı.
grep -F '\n' "$out3" >/dev/null
grep -F '\t' "$out3" >/dev/null
grep -F '\"' "$out3" >/dev/null
grep -F '\\\\' "$out3" >/dev/null

# BAŞMİMAR — REVIEW (#146, Amendment 02): boşluk ve "=" içeren dosya yolu
# key/value ayrıştırmasını belirsizleştirmemeli — file= alanı tırnaklı ve
# tek parça olmalı.
spacePath="$tmpdir/token positions=path test.sqt"
cp "$utf8" "$spacePath"
out4=$("$binary" tokens "$spacePath")
echo "$out4" | grep -F "file=\"$spacePath\"" >/dev/null
space_line_count=$(printf '%s\n' "$out4" | wc -l)
space_token_count=$(printf '%s\n' "$out4" | head -n1 | grep -oE '[0-9]+')
if [ "$space_line_count" -ne "$((space_token_count + 1))" ]; then
    echo "FAIL: boşluk/= içeren path testinde satır sayısı beklenmedik ($space_line_count != $((space_token_count + 1)))" >&2
    exit 1
fi
