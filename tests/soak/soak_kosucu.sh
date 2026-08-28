#!/usr/bin/env bash
# ============================================================================
# saQut — GC Soak Koşucusu
# ============================================================================
#
# NE ÖLÇER: dayanıklılık. Diğer testler "doğru mu, hızlı mı" sorar; bu
# "saatler sonra hâlâ aynı mı" sorar. Yakalamaya çalıştığı şeyler BİRİKİMLİDİR
# ve kısa koşuda görünmez:
#   - bellek sızıntısı (tur başına birkaç bayt → milyonlarca turda GB)
#   - fragmantasyon (ADR-022 taşımasız GC'nin asıl uzun vadeli riski:
#     bellek serbest ama parçalanmış; RSS düşmez, tahsis yavaşlar)
#   - tempo kayması (GC eşiği kötü yakınsarsa tur süresi zamanla artar)
#
# GEÇME ÖLÇÜTÜ — sayım tabanlı, makineden bağımsız:
#   RSS, canlı nesne sayısı ve tur süresi YATAY kalmalı. Yükselen eğri =
#   sızıntı veya fragmantasyon.
#
# NEDEN RELEASE BUILD: ASan kendi allocator'ını kullanır ve redzone ekler —
# ASan altındaki RSS eğrisi GC'nin değil ASan'ın davranışıdır, fragmantasyon
# hiç görünmez. Bellek güvenliği ayrı bir kanıt sınıfıdır (ASan/UBSan
# taramaları); burada ölçtüğümüz şey gerçek çalışma profilidir.
#
# KULLANIM:
#   tests/soak/soak_kosucu.sh [dilim_sayisi] [dilim_basina_tur] [backend]
#     dilim_sayisi      kaç ölçüm noktası (varsayılan 30)
#     dilim_basina_tur  her dilimde kaç iş turu (varsayılan 200000)
#     backend           "" (VM) veya "--jit" (varsayılan VM)
#
#   Kısa doğrulama : tests/soak/soak_kosucu.sh 10 50000
#   Uzun koşu      : tests/soak/soak_kosucu.sh 60 2000000
#
# ÖNEMLİ — İKİ AYRI ÖLÇÜM YAPILIR:
#   1. Süreç-içi eğri (asıl kanıt): TEK bir süreç artan tur sayılarıyla
#      koşar ve RSS'i kendi içinde büyütüp büyütmediğine bakılır. Sızıntı
#      ancak böyle görünür — her dilimde yeni süreç başlatmak birikimli
#      etkiyi sıfırlar ve sızıntıyı GİZLER.
#   2. Dilim tekrarı: aynı işi tekrar tekrar koşup tur süresinin kayıp
#      kaymadığına bakar (tempo kayması, ısınma etkileri).
# ============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SAQUT="$ROOT/build/saqut"
YUK="$ROOT/tests/soak/is_yuku.sqt"

DILIM=${1:-30}
TUR=${2:-200000}
BACKEND=${3:-}

if [ ! -x "$SAQUT" ]; then
    echo "HATA: $SAQUT yok. Önce: ./build-release.sh" >&2; exit 1
fi

# Release build kontrolü — Debug/ASan ile ölçüm yanıltıcıdır (yukarıdaki not).
if grep -q "CMAKE_BUILD_TYPE:STRING=Debug" "$ROOT/build/CMakeCache.txt" 2>/dev/null; then
    echo "UYARI: build Debug modunda. Soak ölçümü Release ile yapılmalıdır." >&2
    echo "       ./build-release.sh çalıştırıp tekrar deneyin." >&2
    exit 1
fi

CIKTI="$ROOT/temp/soak_$(date +%Y%m%d_%H%M%S).tsv"
mkdir -p "$ROOT/temp"

echo "saQut GC soak — $DILIM dilim × $TUR tur, backend: ${BACKEND:-vm}"
echo "Çıktı: $CIKTI"
echo ""
printf "asama\tdilim\tsure_ms\trss_kb\tcanli_nesne\tcanli_kb\ttoplama\ttepe_kb\n" > "$CIKTI"

# ── Ölçüm 1: süreç-İÇİ büyüme (sızıntının tek görünür olduğu yer) ───────────
# Aynı programı artan tur sayısıyla koşarız. Sızıntı varsa RSS tur sayısıyla
# DOĞRU ORANTILI büyür; yoksa yatay kalır (GC canlı kümeyi sabit tutar).
echo "── Ölçüm 1: süreç-içi büyüme ──"
printf "%-12s %-10s %-12s %-10s\n" "tur" "rss_kb" "canli_nesne" "canli_kb"
echo "----------------------------------------------"
buyume_ilk=""; buyume_son=""; kat=1
for kat in 1 2 4 8; do
    t=$(( TUR * kat ))
    olcum=$( { /usr/bin/time -f "%M" "$SAQUT" run $BACKEND --gc-stats "$YUK" -- "$t" ; } 2>&1 >/dev/null )
    r=$(echo "$olcum" | tail -1)
    g=$(echo "$olcum" | grep -o "gc: .*" || echo "")
    cn=$(echo "$g" | grep -oE "live=[0-9]+" | cut -d= -f2 || echo 0)
    cb=$(echo "$g" | grep -oE "liveBytes=[0-9]+" | cut -d= -f2 || echo 0)
    printf "%-12s %-10s %-12s %-10s\n" "$t" "$r" "$cn" "$((cb / 1024))"
    printf "buyume\t%s\t%s\t%s\t%s\n" "$t" "$r" "$cn" "$((cb / 1024))" >> "$CIKTI"
    [ -z "$buyume_ilk" ] && buyume_ilk=$r
    buyume_son=$r
done
echo ""

# ── Ölçüm 2: dilim tekrarı (tempo kayması / ısınma) ─────────────────────────
echo "── Ölçüm 2: dilim tekrarı ──"
printf "%-6s %-10s %-12s %-12s %-10s %-12s %-8s\n" \
    "dilim" "sure_ms" "rss_kb" "canli_nesne" "canli_kb" "toplama" "tepe_kb"
echo "--------------------------------------------------------------------------------"

ilk_rss=""; son_rss=""; ilk_sure=""; son_sure=""

for ((d = 1; d <= DILIM; d++)); do
    # /usr/bin/time -f "%M" → tepe RSS (KB). Sürecin TAMAMININ tepe değeri.
    olcum=$( { /usr/bin/time -f "%M" "$SAQUT" run $BACKEND --gc-stats "$YUK" -- "$TUR" ; } 2>&1 >/dev/null )

    rss=$(echo "$olcum" | tail -1)
    gc=$(echo "$olcum" | grep -o "gc: .*" || echo "")
    canli=$(echo "$gc"  | grep -oE "live=[0-9]+"      | cut -d= -f2 || echo 0)
    canlib=$(echo "$gc" | grep -oE "liveBytes=[0-9]+" | cut -d= -f2 || echo 0)
    topla=$(echo "$gc"  | grep -oE "collections=[0-9]+" | cut -d= -f2 || echo 0)
    tepe=$(echo "$gc"   | grep -oE "peakBytes=[0-9]+" | cut -d= -f2 || echo 0)

    t0=$(date +%s%N)
    "$SAQUT" run $BACKEND "$YUK" -- "$TUR" >/dev/null 2>&1
    t1=$(date +%s%N)
    sure=$(( (t1 - t0) / 1000000 ))

    printf "%-6s %-10s %-12s %-12s %-10s %-12s %-8s\n" \
        "$d" "$sure" "$rss" "$canli" "$((canlib / 1024))" "$topla" "$((tepe / 1024))"
    printf "dilim\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n" \
        "$d" "$sure" "$rss" "$canli" "$((canlib / 1024))" "$topla" "$((tepe / 1024))" >> "$CIKTI"

    [ -z "$ilk_rss" ]  && { ilk_rss=$rss; ilk_sure=$sure; }
    son_rss=$rss; son_sure=$sure
done

echo ""
echo "=== ÖZET ==="
# Süreç-içi büyüme: tur sayısı 8 KAT arttı. Sızıntı varsa RSS de ~8 kat
# artmalıydı; GC çalışıyorsa yatay kalır.
buyume_fark=$(( buyume_son - buyume_ilk ))
buyume_yuzde=$(( buyume_ilk > 0 ? buyume_fark * 100 / buyume_ilk : 0 ))
echo "Süreç-içi (8x iş): ${buyume_ilk} KB → ${buyume_son} KB  (${buyume_yuzde}%)"
rss_fark=$(( son_rss - ilk_rss ))
rss_yuzde=$(( ilk_rss > 0 ? rss_fark * 100 / ilk_rss : 0 ))
sure_fark=$(( son_sure - ilk_sure ))
sure_yuzde=$(( ilk_sure > 0 ? sure_fark * 100 / ilk_sure : 0 ))

echo "RSS  : ${ilk_rss} KB → ${son_rss} KB  (${rss_yuzde}%)"
echo "Süre : ${ilk_sure} ms → ${son_sure} ms  (${sure_yuzde}%)"
echo ""
# Eşikler geniş bilerek: dar eşik gürültüde kırılır, amaç FELAKETİ yakalamak
# (#222 §8: "bench bir kanıt aracı değil, bir alarm aracıdır").
sonuc=0
if [ "$buyume_yuzde" -gt 40 ]; then
    echo "BAŞARISIZ: 8 kat iş için RSS %${buyume_yuzde} büyüdü — SIZINTI şüphesi"; sonuc=1
fi
if [ "$rss_yuzde" -gt 25 ]; then
    echo "BAŞARISIZ: RSS %${rss_yuzde} büyüdü — sızıntı veya fragmantasyon şüphesi"; sonuc=1
fi
if [ "$sure_yuzde" -gt 50 ]; then
    echo "BAŞARISIZ: tur süresi %${sure_yuzde} arttı — tempo kayması şüphesi"; sonuc=1
fi
if [ "$sonuc" -eq 0 ]; then
    echo "GEÇTİ: RSS ve tur süresi kararlı."
fi
echo "Ham veri: $CIKTI"
exit $sonuc
