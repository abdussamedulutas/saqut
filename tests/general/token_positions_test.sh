#!/usr/bin/env bash
set -eu

binary=$1
root=$2
utf8="$root/tests/general/token_positions_utf8.sqt"
multi="$root/tests/general/token_positions_multiline.sqt"

out1=$(mktemp)
out2=$(mktemp)
trap 'rm -f "$out1" "$out2"' EXIT

"$binary" tokens "$utf8" > "$out1"
grep -F 'byteLength=6' "$out1" >/dev/null

"$binary" tokens "$multi" > "$out2"
grep -F 'line=4' "$out2" >/dev/null

diff <("$binary" tokens "$utf8") <("$binary" tokens "$utf8")
