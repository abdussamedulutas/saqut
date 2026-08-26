<?php
// ============================================================================
// bench/php/cpu_hash.php — CPU-yoğun FNV-1a benchmark (PHP)
//
// saQut referansı: bench/sqt/cpu_hash.sqt ile birebir aynı algoritma.
// Sözleşme: bench/README.md §"Cross-language determinizm".
//   PHP int64'tür; 32-bit taşan çarpma float'a düşer. Bu yüzden tüm
//   çarpmalar mul32() üzerinden (16-bit split multiplication) yapılır ve
//   her adım & 0xFFFFFFFF ile maskelenir. Çıktıda signed int32'ye çevrilir.
//   - LCG: x = mul32(x, 1103515245) + 12345 (masked), seed 123456789
//   - FNV-1a: offset basis 2166136261 (^ r), prime 16777619
// ============================================================================

const BENCH_SIZE = 2048;
const BENCH_ROUNDS = 3000;
const M32 = 0xFFFFFFFF;
const FNV_OFFSET_U = 2166136261;
const FNV_PRIME = 16777619;

/** 32-bit wrap çarpma: a*b mod 2^32 (16-bit parçalara bölerek). */
function mul32(int $a, int $b): int {
    $a &= M32;
    $b &= M32;
    $hi = (($a >> 16) * $b) & M32;      // üst 16 bit katkısı
    $lo = (($a & 0xFFFF) * $b) & M32;   // alt 16 bit katkısı
    return ((($hi << 16) + $lo) & M32);
}

function toSigned32(int $v): int {
    $v &= M32;
    return $v >= 0x80000000 ? $v - 0x100000000 : $v;
}

$buf = [];
$x = 123456789;
for ($i = 0; $i < BENCH_SIZE; $i++) {
    $x = mul32($x, 1103515245);
    $x = ($x + 12345) & M32;
    $buf[] = $x;
}

$t0 = microtime(true);
$acc = 0;
for ($r = 0; $r < BENCH_ROUNDS; $r++) {
    $h = FNV_OFFSET_U ^ $r;
    for ($i = 0; $i < BENCH_SIZE; $i++) {
        $h = mul32($h ^ $buf[$i], FNV_PRIME);
    }
    $acc ^= $h;
}
$elapsedMs = (int)round((microtime(true) - $t0) * 1000);

echo "CHECKSUM=" . toSigned32($acc) . "\n";
echo "ELAPSED_MS={$elapsedMs}\n";
