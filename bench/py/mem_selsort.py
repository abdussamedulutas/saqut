# ============================================================================
# bench/py/mem_selsort.py — bellek-yoğun selection sort benchmark (Python)
#
# saQut referansı: bench/sqt/mem_selsort.sqt ile birebir aynı algoritma.
# Her turda yeni liste tahsis edilir; O(n²) selection sort `<` sıkı
# karşılaştırmasıyla İLK minimumu seçer (deterministik, stabil değil).
# Sözleşme: bench/README.md §"Cross-language determinizm".
# ============================================================================

import time

SIZE = 1200
ROUNDS = 15

MASK = 0xFFFFFFFF


def to_signed32(v):
    v &= MASK
    return v - 0x100000000 if v >= 0x80000000 else v


def main():
    seed = 987654321
    t0 = time.perf_counter()
    acc = 0
    for _ in range(ROUNDS):
        arr = []
        for _ in range(SIZE):
            seed = (seed * 1103515245 + 12345) & MASK
            # saQut int'i SIGNED'dır; `<` sıralama karşılaştırması signed
            # yapılmalı (JS Int32Array ile aynı davranış için).
            arr.append(to_signed32(seed))

        # selection sort — bilinçli olarak optimize edilmemiş
        for i in range(SIZE - 1):
            min_idx = i
            for j in range(i + 1, SIZE):
                if arr[j] < arr[min_idx]:
                    min_idx = j
            if min_idx != i:
                arr[i], arr[min_idx] = arr[min_idx], arr[i]

        acc ^= arr[0]
        acc ^= arr[SIZE // 2]
        acc ^= arr[SIZE - 1]
    elapsed_ms = round((time.perf_counter() - t0) * 1000)

    print(f"CHECKSUM={to_signed32(acc)}")
    print(f"ELAPSED_MS={elapsed_ms}")


if __name__ == "__main__":
    main()
