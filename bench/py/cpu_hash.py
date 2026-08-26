# ============================================================================
# bench/py/cpu_hash.py — CPU-yoğun FNV-1a benchmark (Python)
#
# saQut referansı: bench/sqt/cpu_hash.sqt ile birebir aynı algoritma.
# Sözleşme: bench/README.md §"Cross-language determinizm".
#   - int = 32-bit wrap taklidi: her aritmetik adım sonrası & 0xFFFFFFFF
#   - LCG: x = x*1103515245 + 12345, seed 123456789
#   - FNV-1a: offset basis 2166136261 (^ r), prime 16777619
#   - CHECKSUM çıktısı signed int32 karşılığıdır (saQut çıktısıyla eşleşir)
# ============================================================================

import time

SIZE = 2048
ROUNDS = 3000

MASK = 0xFFFFFFFF
FNV_OFFSET = (-2128831035) & MASK  # = 2166136261
FNV_PRIME = 16777619


def to_signed32(v):
    v &= MASK
    return v - 0x100000000 if v >= 0x80000000 else v


def main():
    buf = []
    x = 123456789
    for _ in range(SIZE):
        x = (x * 1103515245 + 12345) & MASK
        buf.append(x)

    t0 = time.perf_counter()
    acc = 0
    for r in range(ROUNDS):
        h = FNV_OFFSET ^ r
        for b in buf:
            h = ((h ^ b) * FNV_PRIME) & MASK
        acc ^= h
    elapsed_ms = round((time.perf_counter() - t0) * 1000)

    print(f"CHECKSUM={to_signed32(acc)}")
    print(f"ELAPSED_MS={elapsed_ms}")


if __name__ == "__main__":
    main()
