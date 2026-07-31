#!/usr/bin/env bash
# #146 (SQ-100-TOKEN-POSITIONS): saqut tokens her token için
# file + line + column + byteOffset + byteLength taşır.
#   - byteOffset : 0-tabanlı UTF-8 bayt offset'i (Token.start)
#   - byteLength : bayt cinsinden lexeme uzunluğu (end - start)
#   - line       : 1-tabanlı
#   - column     : 1-tabanlı UTF-8 BYTE-tabanlı (görüntü kolonu DEĞİL)
#   - tür + lexeme korunur; sıra deterministik (bayt-bayt aynı çıktı).
set -eu

binary=$1
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# ASCII:  int main() { print(1); return 0; }
printf 'int main() { print(1); return 0; }\n' > "$tmp/ascii.sqt"
# UTF-8:  print("héllo");   — é = 2 bayt (0xC3 0xA9), görüntüde tek kolon
printf 'print("h\xc3\xa9llo");\n' > "$tmp/utf8.sqt"

fail() { echo "FAIL: $1" >&2; exit 1; }

# 1) ASCII konumları — bilinen token'ların exact değerleri.
out=$("$binary" tokens "$tmp/ascii.sqt" 2>/dev/null)
printf '%s\n' "$out" | grep -q '\[keyword\] "int"  .*:1:1  byteOffset=0 byteLength=3' || fail "int konumu yanlış"
printf '%s\n' "$out" | grep -q '\[identifier\] "main"  .*:1:5  byteOffset=4 byteLength=4' || fail "main konumu yanlış"
printf '%s\n' "$out" | grep -q '\[identifier\] "print"  .*:1:14  byteOffset=13 byteLength=5' || fail "print konumu yanlış"
printf '%s\n' "$out" | grep -q '\[number\] "1"  .*:1:20  byteOffset=19 byteLength=1' || fail "1 konumu yanlış"
printf '%s\n' "$out" | grep -q '\[keyword\] "return"  .*:1:24  byteOffset=23 byteLength=6' || fail "return konumu yanlış"
printf '%s\n' "$out" | grep -q '\[delimiter\] "}"  .*:1:34  byteOffset=33 byteLength=1' || fail "} konumu yanlış"

# 2) UTF-8 çok baytlı — byteOffset/byteLength bayt cinsinden, column BYTE-tabanlı.
out=$("$binary" tokens "$tmp/utf8.sqt" 2>/dev/null)
# print: 5 bayt, kolon 1
printf '%s\n' "$out" | grep -q '\[identifier\] "print"  .*:1:1  byteOffset=0 byteLength=5' || fail "utf8 print konumu yanlış"
# string "héllo": 8 bayt (2 tırnak + h + é(2 bayt) + l + l + o); başlangıç offset 6
printf '%s\n' "$out" | grep -q '\[string\] ""héllo""  .*:1:7  byteOffset=6 byteLength=8' || fail "utf8 string byteLength 8 olmali (é=2 bayt)"
# ")" stringden sonra: byte kolon 15 (é 2 bayt sayılır); görüntü kolonu 13 olurdu —
# byte ve görüntü kolonu karıştırılmıyor.
printf '%s\n' "$out" | grep -q '\[delimiter\] ")"  .*:1:15  byteOffset=14 byteLength=1' || fail "utf8 ')' byte kolonu 15 olmali (byte-tabanlı)"
# ";" offset 15, kolon 16
printf '%s\n' "$out" | grep -q '\[delimiter\] ";"  .*:1:16  byteOffset=15 byteLength=1' || fail "utf8 ';' konumu yanlış"
# Toplam 5 token (print, (, string, ), ;)
[ "$(printf '%s\n' "$out" | grep -c '^  \[')" -eq 5 ] || fail "utf8 token sayısı 5 olmali"

# 3) Determinizm — aynı girdi → bayt-bayt aynı çıktı (UTF-8 dahil).
set +e
a1=$("$binary" tokens "$tmp/utf8.sqt" 2>/dev/null)
a2=$("$binary" tokens "$tmp/utf8.sqt" 2>/dev/null)
b1=$("$binary" tokens "$tmp/ascii.sqt" 2>/dev/null)
b2=$("$binary" tokens "$tmp/ascii.sqt" 2>/dev/null)
set -e
[ "$a1" = "$a2" ] || fail "utf8 determinizm bozuldu"
[ "$b1" = "$b2" ] || fail "ascii determinizm bozuldu"

# 4) Çok satırlı dosyada line tabanı — ikinci satırdaki token line=2.
printf 'int x = 1;\nprint(x);\n' > "$tmp/twoline.sqt"
out=$("$binary" tokens "$tmp/twoline.sqt" 2>/dev/null)
printf '%s\n' "$out" | grep -q '\[identifier\] "print"  .*:2:1  byteOffset=11 byteLength=5' || fail "2. satır print konumu yanlış"

echo "tokens_positions: TUM TESTLER GECTI"
