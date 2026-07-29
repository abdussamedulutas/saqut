#!/usr/bin/env bash
# #170 bug(parser): eksik kapanış delimiter'ı ((), [], {}) 25 site'ta sessizce
# yutuluyordu — hiç diagnostic üretmiyordu, program "başarıyla" (exit 0)
# çalışıyordu. Artık her site E905 diagnostic'i basıyor ve run/check/exec/ast
# hasErrors() kapısında durup kDataError (65) döndürüyor. Tracked regresyon.
set -eu

binary=$1
out=$(mktemp); err=$(mktemp); src=$(mktemp --suffix=.sqt)
trap 'rm -f "$out" "$err" "$src"' EXIT

# Beklenen: verilen kaynak E905 basmalı ve run exit'i kSuccess(0) OLMAMALI.
expect_e905() {
    local name="$1" code="$2"
    printf '%s' "$code" > "$src"
    set +e
    "$binary" run "$src" > "$out" 2>"$err"
    actual=$?
    set -e
    if [ "$actual" -eq 0 ]; then
        echo "FAIL [$name]: eksik kapanis delimiter'i sessizce kabul edildi (exit 0)" >&2
        echo "--- kaynak ---" >&2; cat "$src" >&2
        exit 1
    fi
    if ! grep -q "E905" "$err"; then
        echo "FAIL [$name]: E905 diagnostic'i basilmadi. stderr:" >&2
        cat "$err" >&2
        exit 1
    fi
}

# Beklenen: verilen kaynak temiz calismali (exit 0, stdout eslesir) — regresyon
# koruma: E905 eklenmesi GECERLI programlari bozmamali.
expect_clean() {
    local name="$1" code="$2" expected_out="$3"
    printf '%s' "$code" > "$src"
    set +e
    actual_out=$("$binary" run "$src" 2>"$err")
    actual=$?
    set -e
    if [ "$actual" -ne 0 ] || [ "$actual_out" != "$expected_out" ]; then
        echo "FAIL [$name]: gecerli program bozuldu. beklenen exit=0/out='$expected_out', gercek exit=$actual/out='$actual_out'" >&2
        cat "$err" >&2
        exit 1
    fi
}

# --- #170'in kendi minimal repro'su: cagri arguman listesi ---
expect_e905 "call_missing_rparen" 'int main() { print(2 print(3); return 0; }'
expect_clean "call_ok" 'int main() { print(23); return 0; }' "23"

# --- parantezli ifade ---
expect_e905 "paren_expr_missing_rparen" 'int main() { int x = (1 + 2; return 0; }'
expect_clean "paren_expr_ok" 'int main() { int x = (1 + 2); print(x); return 0; }' "3"

# --- array literal ---
expect_e905 "array_literal_missing_rbracket" 'int main() { int[] a = [1, 2, 3; return 0; }'
expect_clean "array_literal_ok" 'int main() { int[] a = [1, 2, 3]; print(a[0]); return 0; }' "1"

# --- indeksleme ---
expect_e905 "index_missing_rbracket" 'int main() { int[] a = [1,2,3]; return a[0; }'
expect_clean "index_ok" 'int main() { int[] a = [1,2,3]; print(a[1]); return 0; }' "2"

# --- fonksiyon parametre listesi ---
expect_e905 "func_params_missing_rparen" 'int add(int a, int b { return a+b; } int main() { return 0; }'
expect_clean "func_params_ok" 'int add(int a, int b) { return a+b; } int main() { print(add(2,3)); return 0; }' "5"

# --- if kosulu ---
expect_e905 "if_cond_missing_rparen" 'int main() { if (1 < 2 { return 1; } return 0; }'
expect_clean "if_cond_ok" 'int main() { if (1 < 2) { print(1); } return 0; }' "1"

# --- while kosulu ---
expect_e905 "while_cond_missing_rparen" 'int main() { int i = 0; while (i < 3 { i = i + 1; } return 0; }'
expect_clean "while_cond_ok" 'int main() { int i = 0; while (i < 3) { i = i + 1; } print(i); return 0; }' "3"

# --- for kosulu ---
expect_e905 "for_missing_rparen" 'int main() { for (int i=0; i<3; i=i+1 { print(i); } return 0; }'
expect_clean "for_ok" 'int main() { for (int i=0; i<3; i=i+1) { print(i); } return 0; }' "012"

# --- do-while kosulu ---
expect_e905 "do_while_missing_rparen" 'int main() { int i = 0; do { i = i + 1; } while (i < 3 ; return 0; }'
expect_clean "do_while_ok" 'int main() { int i = 0; do { i = i + 1; } while (i < 3); print(i); return 0; }' "3"

# --- blok kapanisi ---
expect_e905 "block_missing_rbrace" 'int main() { int x = 1; return x;'

# --- switch subject + govde ---
expect_e905 "switch_subject_missing_rparen" 'int main() { switch (1 { default: return 0; } }'
expect_e905 "switch_body_missing_rbrace" 'int main() { switch (1) { default: return 0; return 0; }'
expect_clean "switch_ok" 'int main() { switch (1) { case 1: print(1); default: print(0); } return 0; }' "1"

# --- struct/enum govde kapanisi ---
expect_e905 "struct_missing_rbrace" 'struct P { int x; int main() { return 0; }'
expect_e905 "enum_missing_rbrace" 'enum Color { Red, Green int main() { return 0; }'
expect_clean "struct_ok" 'struct P { int x; } int main() { P p; p.x = 5; print(p.x); return 0; }' "5"

# --- import listesi kapanisi ---
expect_e905 "import_missing_rbrace" 'import { fs int main() { return 0; }'

# Determinizm: bayrak #170 repro'su 3 kez ayni (non-zero exit + E905) davranisi verir.
for i in 1 2 3; do
    printf 'int main() { print(2 print(3); return 0; }' > "$src"
    set +e
    "$binary" run "$src" > "$out" 2>"$err"
    actual=$?
    set -e
    test "$actual" -ne 0
    grep -q "E905" "$err"
done
