#!/usr/bin/env bash
# Stres testi: özyinelemeli çağrı derinliği + patlayan (base-case'siz) özyineleme.
#
# Bulgular (Testçi turu, #117 sonrası genel stres turu):
#
# 1) Meşru derin özyineleme (base-case VAR) VM'de çökmüyor: deep_recursion
#    1.000 / 100.000 / 1.000.000 derinlikte doğru sonucu verdi (host C++ çağrı
#    yığınına bağlı değil gibi görünüyor -- GÜÇLÜ NOKTA).
#    Bu turda ayrıca 5.000.000 ve 20.000.000 derinlik de denendi (repo dışı,
#    scratchpad'te; boyut nedeniyle burada checked-in değil): 20M derinlik
#    ~35s sürdü ve exit=0 verdi, ama "sys" süresi "user" süresinden ~3 kat
#    fazlaydı (26s sys / 8.7s user) -- derinlik arttıkça çağrı-çerçevesi
#    büyütme mekanizması ağırlaşıyor gibi gözüküyor (üstel değil ama düz de
#    değil). gc-stats runs=0 verdi (bu saf-int özyineleme, heap nesnesi yok).
#
# 2) Base-case'i OLMAYAN özyineleme (infinite_recursion.sqt) hiçbir üst sınıra
#    çarpmıyor: bellek RSS'i saniyede ~1.8GB büyüyor (5 saniyede 1.8GB->9.3GB
#    ölçüldü), ne "max call depth" ne "stack overflow" hatası -- sonunda
#    bellek tükenince (ulimit -v ile sınırlandığında) "runtime error:
#    std::bad_alloc" ile çöküyor.
#
# 3) infinite_recursion_try.sqt AYNI kodu try/catch içine alıyor -- ADR-025
#    "her runtime hatası yakalanabilir" sözleşmesine göre std::bad_alloc'un
#    Error'a çevrilip catch(Error e) bloğuna düşmesi beklenirdi. GÖZLENEN:
#    catch bloğu HİÇ tetiklenmiyor, "survived" hiç basılmıyor -- program
#    doğrudan exit=1 ile "runtime error: std::bad_alloc" basıp ölüyor.
#    Bu, error-handling sözleşmesinin (dist/error-handling.md + ADR-025)
#    bellek tükenmesi durumunda gerçek karşılığı olmadığını gösteriyor.

set -uo pipefail
SQT="${1:-./build/saqut}"
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "=== deep_recursion_1000.sqt (VM, base-case var) ==="
time "$SQT" run "$DIR/deep_recursion_1000.sqt"
echo

echo "=== deep_recursion_100000.sqt ==="
time "$SQT" run "$DIR/deep_recursion_100000.sqt"
echo

echo "=== deep_recursion_1000000.sqt ==="
time "$SQT" run "$DIR/deep_recursion_1000000.sqt"
echo

echo "=== infinite_recursion.sqt, bellek tavanı 2GB (ulimit -v) altında ==="
echo "beklenen (ADR-025/error-handling.md): yakalanabilir bir Error ya da en"
echo "azından tanımlı bir E-kodlu çalışma zamanı hatası"
( ulimit -v 2000000; timeout 30 "$SQT" run "$DIR/infinite_recursion.sqt" )
echo "gözlenen exit=$?"
echo

echo "=== infinite_recursion_try.sqt, aynı bellek tavanı, try/catch SARILI ==="
echo "beklenen: catch(Error e) tetiklenir, 'caught:...' ve 'survived' basılır"
( ulimit -v 2000000; timeout 30 "$SQT" run "$DIR/infinite_recursion_try.sqt" )
echo "gözlenen exit=$? -- 'survived' hiç basılmadıysa catch tetiklenmedi demektir"
