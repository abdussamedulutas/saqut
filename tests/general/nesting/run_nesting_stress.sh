#!/usr/bin/env bash
# Stres testi: derinlemesine iç-içe sözdizimi (parantezli ifade / if bloğu /
# düz operatör zinciri) -- parser/derleyici istismarı.
#
# EN ÖNEMLİ BULGU (bu turun en ciddi bulgusu): üç FARKLI sözdizimsel şekil de
# (parantezli ifade, iç-içe if, düz + zinciri) N ~ 4000-5000 civarında
# SIGSEGV (Parçalama arızası, exit=139) ile çöküyor. Tokenizer aynı girdide
# sorunsuz (bkz. aşağıdaki `saqut tokens` çağrısı) -- yani sorun parser
# (recursive-descent/Pratt) aşamasının kendi (host C++) çağrı yığınını
# sınırsız kullanmasında, muhtemelen bir derinlik koruması yok.
#
# saqut check/ast/ir gibi diğer komutlar da muhtemelen aynı şekilde etkilenir
# (parser aşamasından sonra gelen her komut aynı AST inşa mekanizmasını
# kullanıyor) -- bu script yalnızca `run`'ı test ediyor, PM/Mimar isterse
# check/ir/ast'ı da aynı fixture'larla deneyebilir.
#
# Eşik (bu makinede ölçüldü, ~8MB varsayılan thread stack ile tutarlı):
#   - parantezli ifade: N=4000 OK, N=5000 SIGSEGV
#   - iç-içe if:         N=5000 SIGSEGV (daha düşük N denenmedi, ayrı issue;
#                         fixture bu klasörde checked-in değil, gen_nesting.py
#                         ile yeniden üretin -- bkz. o script'in NOT'u)
#   - düz + zinciri:     N=3000 OK, N=5000 SIGSEGV
#
# Sözleşme beklentisi: aşırı derin/geniş ama SÖZDİZİMSEL OLARAK GEÇERLİ bir
# program, "kod çok karmaşık" gibi kontrollü bir tanı ile reddedilebilir
# (ör. "ifade derinliği sınırını aştı") ama derleyici SÜRECİNİN çökmesi
# (SIGSEGV) beklenmez -- organization.md'nin "pathological ASTs" / "parser
# abuse" test kategorisinin doğrudan hedefidir.

set -uo pipefail
SQT="${1:-./build/saqut}"
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

run_case() {
    local label="$1" file="$2"
    echo "=== $label ==="
    timeout 15 "$SQT" run "$file"
    local rc=$?
    echo " -> exit=$rc"
    if [ "$rc" -eq 139 ]; then
        echo " -> SIGSEGV (Parçalama arızası)"
    fi
    echo
}

echo "--- tokenizer kontrolü: aynı derin dosyada 'tokens' çöküyor mu? ---"
timeout 15 "$SQT" tokens "$DIR/nest_expr_5000_crash.sqt" > /dev/null
echo "tokens exit=$? (0 bekleniyor -- tokenizer parser'dan farklı, recursion yok)"
echo

run_case "nest_expr_4000_ok.sqt     (parantezli ifade, N=4000, calisir)" "$DIR/nest_expr_4000_ok.sqt"
run_case "nest_expr_5000_crash.sqt  (parantezli ifade, N=5000, SIGSEGV beklenir)" "$DIR/nest_expr_5000_crash.sqt"
run_case "chain_3000_ok.sqt         (duz + zinciri, N=3000, calisir)" "$DIR/chain_3000_ok.sqt"
run_case "chain_5000_crash.sqt      (duz + zinciri, N=5000, SIGSEGV beklenir)" "$DIR/chain_5000_crash.sqt"

echo "--- ic-ice if (N=5000) checked-in DEGIL, yeniden uretin: ---"
echo "    python3 $DIR/gen_nesting.py ifchain 5000 /tmp/nest_if_5000.sqt"
echo "    $SQT run /tmp/nest_if_5000.sqt   # SIGSEGV, exit=139 beklenir"
