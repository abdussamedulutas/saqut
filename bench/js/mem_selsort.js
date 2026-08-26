// ============================================================================
// bench/js/mem_selsort.js — bellek-yoğun selection sort (Node.js, SAF JS)
//
// saQut referansı: bench/sqt/mem_selsort.sqt ile birebir aynı algoritma.
// Selection sort `<` sıkı karşılaştırmasıyla İLK minimumu seçer
// (deterministik, stabil değil). Sözleşme: bench/README.md.
//   - SAF JS: typed array (Int32Array) KULLANILMAZ — normal dizi + push
//     (V8'in C++ binary/buffer mekanizması yerine saf JS taban).
// ============================================================================

'use strict';

const SIZE = 1200;
const ROUNDS = 15;

function main() {
    let seed = 987654321 | 0;
    const t0 = performance.now();
    let acc = 0 | 0;
    for (let r = 0; r < ROUNDS; r++) {
        // SAF JS dizisi (typed array değil)
        const arr = [];
        for (let i = 0; i < SIZE; i++) {
            seed = (Math.imul(seed, 1103515245) + 12345) | 0;
            arr.push(seed);
        }

        // selection sort — bilinçli olarak optimize edilmemiş
        for (let i = 0; i < SIZE - 1; i++) {
            let minIdx = i;
            for (let j = i + 1; j < SIZE; j++) {
                if (arr[j] < arr[minIdx]) {
                    minIdx = j;
                }
            }
            if (minIdx !== i) {
                const tmp = arr[i];
                arr[i] = arr[minIdx];
                arr[minIdx] = tmp;
            }
        }

        acc ^= arr[0];
        acc ^= arr[SIZE >> 1];
        acc ^= arr[SIZE - 1];
    }
    const elapsedMs = Math.round(performance.now() - t0);

    console.log(`CHECKSUM=${acc}`);
    console.log(`ELAPSED_MS=${elapsedMs}`);
}

main();
