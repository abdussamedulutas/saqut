// ============================================================================
// bench/js/cpu_hash.js — CPU-yoğun FNV-1a benchmark (Node.js, SAF JS)
//
// saQut referansı: bench/sqt/cpu_hash.sqt ile birebir aynı algoritma.
// Sözleşme: bench/README.md §"Cross-language determinizm".
//   - SAF JS: typed array (Int32Array) KULLANILMAZ — normal dizi + push.
//     V8'in C++ tabanlı binary/buffer mekanizması devre dışı; karşılaştırma
//     saf JavaScript tabandır (bkz. bench/kjs/ aynı saf yaklaşımda).
//   - JS Number int32 semantiği: çarpmalar Math.imul, toplamalar |0 ile.
//   - LCG: x = (Math.imul(x, 1103515245) + 12345) | 0, seed 123456789
//   - FNV-1a: offset basis -2128831035 (^ r), prime 16777619
// CHECKSUM zaten signed int32'dir (saQut çıktısıyla eşleşir).
// ============================================================================

'use strict';

const SIZE = 2048;
const ROUNDS = 3000;
const FNV_OFFSET = -2128831035;   // signed int32 karşılığı (= 2166136261)
const FNV_PRIME = 16777619;

function main() {
    // SAF JS dizisi (typed array değil)
    const buf = [];
    let x = 123456789 | 0;
    for (let i = 0; i < SIZE; i++) {
        x = (Math.imul(x, 1103515245) + 12345) | 0;
        buf.push(x);
    }

    const t0 = performance.now();
    let acc = 0 | 0;
    for (let r = 0; r < ROUNDS; r++) {
        let h = (FNV_OFFSET ^ r) | 0;
        for (let i = 0; i < SIZE; i++) {
            h = Math.imul(h ^ buf[i], FNV_PRIME);
        }
        acc ^= h;
    }
    const elapsedMs = Math.round(performance.now() - t0);

    console.log(`CHECKSUM=${acc}`);
    console.log(`ELAPSED_MS=${elapsedMs}`);
}

main();
