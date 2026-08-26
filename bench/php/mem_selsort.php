<?php
// ============================================================================
// bench/php/mem_selsort.php — bellek-yoğun selection sort benchmark (PHP)
//
// saQut referansı: bench/sqt/mem_selsort.sqt ile birebir aynı algoritma.
// Selection sort `<` sıkı karşılaştırmasıyla İLK minimumu seçer
// (deterministik, stabil değil). Sözleşme: bench/README.md.
// ============================================================================

const BENCH_SIZE = 1200;
const BENCH_ROUNDS = 15;
const M32 = 0xFFFFFFFF;

/** 32-bit wrap çarpma: a*b mod 2^32 (16-bit parçalara bölerek). */
function mul32(int $a, int $b): int {
    $a &= M32;
    $b &= M32;
    $hi = (($a >> 16) * $b) & M32;
    $lo = (($a & 0xFFFF) * $b) & M32;
    return ((($hi << 16) + $lo) & M32);
}

function toSigned32(int $v): int {
    $v &= M32;
    return $v >= 0x80000000 ? $v - 0x100000000 : $v;
}

$seed = 987654321;
$t0 = microtime(true);
$acc = 0;
for ($r = 0; $r < BENCH_ROUNDS; $r++) {
    $arr = [];
    for ($i = 0; $i < BENCH_SIZE; $i++) {
        $seed = mul32($seed, 1103515245);
        $seed = ($seed + 12345) & M32;
        // saQut int'i SIGNED'dır; sıralama karşılaştırması signed yapılmalı.
        $arr[] = toSigned32($seed);
    }

    // selection sort — bilinçli olarak optimize edilmemiş
    for ($i = 0; $i < BENCH_SIZE - 1; $i++) {
        $minIdx = $i;
        for ($j = $i + 1; $j < BENCH_SIZE; $j++) {
            if ($arr[$j] < $arr[$minIdx]) {
                $minIdx = $j;
            }
        }
        if ($minIdx !== $i) {
            $tmp = $arr[$i];
            $arr[$i] = $arr[$minIdx];
            $arr[$minIdx] = $tmp;
        }
    }

    $acc ^= $arr[0];
    $acc ^= $arr[intdiv(BENCH_SIZE, 2)];
    $acc ^= $arr[BENCH_SIZE - 1];
}
$elapsedMs = (int)round((microtime(true) - $t0) * 1000);

echo "CHECKSUM=" . toSigned32($acc) . "\n";
echo "ELAPSED_MS={$elapsedMs}\n";
