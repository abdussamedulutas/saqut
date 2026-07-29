#!/usr/bin/env bash
# #134 bug(exec): parser hatası artık yutulmuyor — run/check ile aynı
# diagnostic kapısından geçiyor. Tracked regresyon.
set -eu

binary=$1
out=$(mktemp); err=$(mktemp)
trap 'rm -f "$out" "$err"' EXIT

# Regresyon: parser hatası içeren ifade artık YANLIŞ stdout üretmemeli ve
# exit 0 DÖNMEMELİ (asıl #134 kanıtı: eskiden stdout="10", exit=0 idi).
set +e
"$binary" exec 'print(1);' > "$out" 2>"$err"
actual=$?
set -e
if [ "$actual" -eq 0 ]; then
    echo "FAIL: 'print(1);' (parser hatası) exit 0 verdi (regresyon geri geldi)" >&2
    exit 1
fi
if grep -qE '^1?0$' "$out"; then
    echo "FAIL: 'print(1);' hâlâ yanlış stdout üretiyor: $(cat "$out")" >&2
    exit 1
fi
if [ -s "$out" ]; then
    echo "FAIL: parser hatasında stdout boş olmalı, gerçek: $(cat "$out")" >&2
    exit 1
fi

# Kontrol: geçerli ifadeler hâlâ doğru çalışıyor (regresyon yok).
check_ok() {
    expr=$1; expected=$2
    set +e
    actual_out=$("$binary" exec "$expr" 2>"$err")
    actual_exit=$?
    set -e
    if [ "$actual_exit" -ne 0 ] || [ "$actual_out" != "$expected" ]; then
        echo "FAIL: exec '$expr' -> beklenen '$expected'/exit 0, gercek '$actual_out'/exit $actual_exit" >&2
        cat "$err" >&2
        exit 1
    fi
}
check_ok '3+4*2' '11'
check_ok 'int x=5; print(x*x);' '25'
check_ok 'for(int i=0;i<3;i=i+1){print(i);}' '012'

# Determinizm: aynı hatalı ifade 3 kez aynı (exit + boş stdout) davranışı verir.
for i in 1 2 3; do
    set +e
    "$binary" exec 'print(1);' > "$out" 2>/dev/null
    actual=$?
    set -e
    test "$actual" -ne 0
    test ! -s "$out"
done
