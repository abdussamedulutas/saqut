#!/usr/bin/env bash
# saQut test koşucusu — birim testler + golden testler
# Kullanım: bash tests/run.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CXX="${CXX:-g++}"
FLAGS=(-std=c++20 -Wall -Wextra -I"$ROOT/src")
SAQUT="$ROOT/build/saqut"

# ── Birim testler ─────────────────────────────────────────────────────────────
for t in test_type test_diagnostic test_opcode test_value_rep_contract test_cfg test_host_abi test_decimal_core; do
    echo "=== $t ==="
    # test_cfg buildCFG gerçeklemesini (ir_cfg.cpp) da derler — hata sınıfı
    # testi için implementasyon gerekli (#218).
    # test_host_abi StringObject/Heap gerçeklemesini (object.cpp) gerektirir —
    # sınır temsili string'i pointer olarak taşır (#222).
    extra=""
    [ "$t" = "test_cfg" ] && extra="$ROOT/src/ir/ir_cfg.cpp"
    # test_host_abi gerçek registry'yi çağırır (rt_host_call) — host gövdeleri
    # ve object.cpp gerekir. SAQUT_VERSION normalde CMake'ten gelir.
    # #223: registry built-in metodları src/data/ modüllerinden alır.
    # Host gövdeleri src/ffi/functions/ altında bölünmüştür (organizasyon, #115).
    [ "$t" = "test_host_abi" ] && extra="$ROOT/src/core/utf8.cpp $ROOT/src/vm/object.cpp $ROOT/src/ffi/host_registry.cpp $ROOT/src/ffi/host_functions.cpp $ROOT/src/ffi/functions/math.cpp $ROOT/src/ffi/functions/fs.cpp $ROOT/src/ffi/functions/sys.cpp $ROOT/src/ffi/functions/date.cpp $ROOT/src/ffi/functions/core.cpp $ROOT/src/ffi/functions/process.cpp $ROOT/src/ffi/functions/io.cpp $ROOT/src/ffi/functions/path.cpp $ROOT/src/ffi/functions/utf8.cpp $ROOT/src/ffi/functions/os.cpp $ROOT/src/data/data_registry.cpp $ROOT/src/data/string.cpp $ROOT/src/data/array.cpp $ROOT/src/data/struct.cpp $ROOT/src/data/date.cpp -DSAQUT_VERSION=\"test\""
    "$CXX" "${FLAGS[@]}" "$ROOT/tests/$t.cpp" $extra -o "/tmp/saqut_$t"
    "/tmp/saqut_$t"
done

# ── Taşınabilirlik denetimi (#224) ────────────────────────────────────────────
# decimal saQut'un sentetik tipidir: aritmetiği C++'a özgü hiçbir şeye
# dayanamaz, çünkü WASM/JS/PHP hedeflerinde aynı sonucu vermek zorundadır.
echo "=== tasinabilirlik: decimal cekirdegi ==="
if grep -n "__int128" "$ROOT/src/data/decimal_core.hpp" "$ROOT/src/core/decimal.hpp" | grep -v "^[^:]*:[0-9]*: *//" | grep -v "//.*__int128"; then
    echo "HATA: decimal cekirdeginde __int128 var — WASM/JS/PHP'ye tasinamaz"
    exit 1
fi
echo "  decimal cekirdegi tasinabilir (yalniz int64/uint64)"

# ── Golden testler ────────────────────────────────────────────────────────────
echo "=== golden ==="

if [ ! -x "$SAQUT" ]; then
    echo "HATA: $SAQUT bulunamadı — önce derleyin (cmake --build build)"
    exit 1
fi

PASS=0; FAIL=0
while IFS= read -r -d '' sqt; do
    dir=$(dirname "$sqt")
    base=$(basename "$sqt" .sqt)
    exp="$dir/$base.expected"
    cerr="$dir/$base.compile_error"
    rerr="$dir/$base.runtime_error"
    xexit="$dir/$base.expected_exit"
    if [ ! -f "$exp" ] && [ ! -f "$cerr" ] && [ ! -f "$rerr" ] && [ ! -f "$xexit" ]; then
        continue
    fi

    # ADR-036 (#76): BASE.flags — --allow-fs vb. gerektiren testler.
    extra_flags=()
    flags_file="$dir/$base.flags"
    if [ -f "$flags_file" ]; then
        mapfile -t extra_flags < "$flags_file"
    fi

    if [ -f "$cerr" ]; then
        # Derleme-hatası fixture'ı: program derlenMEMELİ. stderr beklenen
        # tanıyı içermeli (E-kodu `[$want]` biçiminde ya da sabit mesaj
        # parçası) ve exit code sıfırdan farklı olmalı (.expected_exit
        # varsa tam değeriyle). Sessiz-kabul sınıfını yakalar.
        want=$(cat "$cerr")
        set +e
        err=$("$SAQUT" run "${extra_flags[@]}" "$sqt" 2>&1 >/dev/null)
        rc=$?
        set -e
        if [[ "$want" =~ ^E[0-9]+$ ]]; then
            echo "$err" | grep -q "\[$want\]" && found=1 || found=0
        else
            echo "$err" | grep -qF "$want" && found=1 || found=0
        fi
        if [ -f "$xexit" ]; then
            want_rc=$(cat "$xexit")
            rc_ok=$([ "$rc" -eq "$want_rc" ] && echo 1 || echo 0)
        else
            rc_ok=$([ "$rc" -ne 0 ] && echo 1 || echo 0)
        fi
        if [ "$found" -eq 1 ] && [ "$rc_ok" -eq 1 ]; then
            PASS=$((PASS + 1))
        else
            echo "  FAIL (compile_error): ${sqt#"$ROOT"/}"
            echo "    beklenen : derleme hatası [$want], exit=${want_rc:-nonzero}"
            echo "    gerçek   : exit=$rc, ilk satır: $(echo "$err" | head -1)"
            FAIL=$((FAIL + 1))
        fi
        continue
    fi

    if [ -f "$rerr" ]; then
        # Runtime-hata fixture'ı: program koşar ama tanımlı runtime hatasıyla
        # sonlanır. exit code .expected_exit ile (yoksa 70), stderr
        # .runtime_error regex'iyle eşleşmeli. Sessiz-yanlış-sonuç sınıfını
        # yakalar (ör. sıfıra bölme 0 dönmemeli).
        want_rc=$( [ -f "$xexit" ] && cat "$xexit" || echo 70 )
        want_re=$(cat "$rerr")
        set +e
        out=$("$SAQUT" run "${extra_flags[@]}" "$sqt" 2>/tmp/saqut_rerr)
        rc=$?
        set -e
        err=$(cat /tmp/saqut_rerr)
        if [ "$rc" -eq "$want_rc" ] && echo "$err" | grep -Eq "$want_re"; then
            PASS=$((PASS + 1))
        else
            echo "  FAIL (runtime_error): ${sqt#"$ROOT"/}"
            echo "    beklenen : exit=$want_rc, stderr ~ /$want_re/"
            echo "    gerçek   : exit=$rc, stderr: $(echo "$err" | head -1)"
            FAIL=$((FAIL + 1))
        fi
        continue
    fi

    actual=$("$SAQUT" run "${extra_flags[@]}" "$sqt" 2>/dev/null) || true
    expected=$(cat "$exp")

    if [ "$actual" = "$expected" ]; then
        PASS=$((PASS + 1))
    else
        echo "  FAIL: ${sqt#"$ROOT"/}"
        echo "    beklenen : $(echo "$expected" | head -1)"
        echo "    gerçek   : $(echo "$actual"   | head -1)"
        FAIL=$((FAIL + 1))
    fi
done < <(find "$ROOT/tests/golden" -name "*.sqt" -print0 | sort -z)

echo "  $PASS geçti, $FAIL başarısız"
[ "$FAIL" -eq 0 ] || exit 1

# ── Diferansiyel test (#92): VM ≡ MIR JIT ────────────────────────────────────
# Her golden fixture'ı hem VM hem JIT ile koşup stdout+exit code'u bayt-bayt
# karşılaştırır. JIT şu an MIR dilimlerinin kapsadığı opcode alt kümesiyle
# sınırlı (mir_backend.hpp) — bir fixture bu kümenin dışına çıkan bir opcode
# içeriyorsa (struct/array/global/try-catch/nullable vb.) JIT programın
# TAMAMINI reddeder (kısmi JIT yok); bu durum parity hatası DEĞİL, henüz
# kapsanmamış bir dilim demektir → SKIP sayılır, FAIL sayılmaz.
echo "=== diferansiyel (VM≡JIT) ==="
DPASS=0; DFAIL=0; DSKIP=0
while IFS= read -r -d '' sqt; do
    dir=$(dirname "$sqt")
    base=$(basename "$sqt" .sqt)
    exp="$dir/$base.expected"
    [ -f "$exp" ] || continue

    extra_flags=()
    flags_file="$dir/$base.flags"
    if [ -f "$flags_file" ]; then
        mapfile -t extra_flags < "$flags_file"
    fi

    set +e
    jit_err=$("$SAQUT" run --jit "${extra_flags[@]}" "$sqt" 2>&1 >/dev/null)
    set -e
    if echo "$jit_err" | grep -q "desteklenmeyen opcode"; then
        DSKIP=$((DSKIP + 1))
        continue
    fi

    set +e
    vm_out=$("$SAQUT" run "${extra_flags[@]}" "$sqt" 2>/dev/null); vm_exit=$?
    jit_out=$("$SAQUT" run --jit "${extra_flags[@]}" "$sqt" 2>/dev/null); jit_exit=$?
    set -e

    if [ "$vm_out" = "$jit_out" ] && [ "$vm_exit" = "$jit_exit" ]; then
        DPASS=$((DPASS + 1))
    elif [ -f "$dir/$base.jit_known_broken" ]; then
        # CMakeLists.txt'teki WILL_FAIL mekanizmasıyla aynı fikir: JIT bu
        # fixture'da bilinen, ayrı issue'da kayıtlı bir crash/parity hatası
        # üretiyor. VM (normatif) doğru; JIT [EXPERIMENTAL], parity bu
        # release kapısının şartı değil (AGENTS.md §9).
        DSKIP=$((DSKIP + 1))
    else
        echo "  FAIL (parity): ${sqt#"$ROOT"/} (vm_exit=$vm_exit jit_exit=$jit_exit)"
        echo "    VM  : $(echo "$vm_out"  | head -1)"
        echo "    JIT : $(echo "$jit_out" | head -1)"
        DFAIL=$((DFAIL + 1))
    fi
done < <(find "$ROOT/tests/golden" -name "*.sqt" -print0 | sort -z)

echo "  $DPASS geçti, $DFAIL başarısız, $DSKIP atlandı (JIT henüz desteklemiyor)"
[ "$DFAIL" -eq 0 ] || exit 1

# ── Modül döngüsü testleri (ADR-031, #78) ────────────────────────────────────
# Döngüsel bağımlılık E_MODULE_CYCLE tanısı + sıfır-dışı exit üretmeli.
echo "=== modül döngüsü ==="
for f in cycle_a self_import; do
    if out=$("$SAQUT" check "$ROOT/tests/module/$f.sqt" 2>/dev/null); then
        echo "  FAIL: $f.sqt — döngüde exit 0 döndü"; exit 1
    fi
    if ! echo "$out" | grep -q "E_MODULE_CYCLE"; then
        echo "  FAIL: $f.sqt — E_MODULE_CYCLE tanısı yok"; exit 1
    fi
done
echo "  2 geçti, 0 başarısız"

# ── GC testleri (#77, ADR-022) ───────────────────────────────────────────────
# 1) Varsayılan eşikle koşuda GC gerçekten tetikleniyor (runs >= 1)
# 2) Stress modda (--gc-threshold=1) çıktı normal koşuyla aynı (canlılık)
# 3) --gc-threshold=-1 otomatik GC'yi kapatıyor (runs=0)
echo "=== gc ==="
GC_SQT="$ROOT/tests/golden/gc/liveness.sqt"
gcout=$("$SAQUT" run --gc-stats "$GC_SQT" 2>&1 >/dev/null)
if ! echo "$gcout" | grep -Eq "gc: runs=[1-9]"; then
    echo "  FAIL: varsayılan eşikte GC hiç koşmadı: $gcout"; exit 1
fi
stress=$("$SAQUT" run --gc-threshold=1 "$GC_SQT" 2>/dev/null)
normal=$("$SAQUT" run "$GC_SQT" 2>/dev/null)
if [ "$stress" != "$normal" ]; then
    echo "  FAIL: stress modda (--gc-threshold=1) çıktı farklı"; exit 1
fi
gcoff=$("$SAQUT" run --gc-threshold=-1 --gc-stats "$GC_SQT" 2>&1 >/dev/null)
if ! echo "$gcoff" | grep -q "runs=0"; then
    echo "  FAIL: --gc-threshold=-1 GC'yi kapatmadı: $gcoff"; exit 1
fi
echo "  3 geçti, 0 başarısız"

# ── Builtin sözdizimi testleri (ADR-033, #85) ────────────────────────────────
# 1) Eski ElemTip::metod sözdizimi W006 uyarısı verir ama çalışır (exit 0)
# 2) Struct alanı builtin'i gölgeler: k.length() alan varken derleme hatası
echo "=== builtin sözdizimi ==="
legout=$("$SAQUT" check "$ROOT/tests/semantic/legacy_builtin.sqt" 2>/dev/null) || {
    echo "  FAIL: legacy_builtin.sqt derlenmeliydi (yalnızca W)"; exit 1; }
if ! echo "$legout" | grep -q "W006"; then
    echo "  FAIL: eski sözdizimi W006 uyarısı üretmedi"; exit 1
fi
if out=$("$SAQUT" check "$ROOT/tests/semantic/field_shadow.sqt" 2>/dev/null); then
    echo "  FAIL: field_shadow.sqt derlenmemeliydi (alan gölgeleme)"; exit 1
fi
if ! echo "$out" | grep -q "is a field of struct"; then
    echo "  FAIL: alan gölgeleme tanısı beklenen mesajı içermiyor"; exit 1
fi
echo "  2 geçti, 0 başarısız"

# ── requires uyarısı (ADR-043, #229) ────────────────────────────────────────
# requires <cap> grameri parse edilir; capability enforcement kaldırıldığı
# için kullanım DERLEME ZAMANI uyarısıdır (W007) — program yine derlenir ve
# çalışır. Yeni .expected_warning marker'ı yalnız bu fixture kullanır;
# mevcut testler zayıflamaz.
echo "=== requires uyarısı ==="
RW_SQT="$ROOT/tests/golden/ffi/requires_warning.sqt"
rw_want=$(cat "${RW_SQT%.sqt}.expected_warning")
rw_out=$("$SAQUT" check "$RW_SQT" 2>&1)
if ! echo "$rw_out" | grep -q "W007"; then
    echo "  FAIL: requires uyarısı [$rw_want] görünmedi: $(echo "$rw_out" | head -1)"
    exit 1
fi
rw_run=$("$SAQUT" run "$RW_SQT" 2>/dev/null)
rw_exp=$(cat "${RW_SQT%.sqt}.expected")
if [ "$rw_run" != "$rw_exp" ]; then
    echo "  FAIL: requires fixture çıktısı farklı (beklenen: $rw_exp)"
    exit 1
fi
echo "  1 geçti, 0 başarısız"

echo "=== TUM TESTLER GECTI ==="
