#!/usr/bin/env python3
# Cetvel karşılaştırması: bench_fib.sqt ile BİREBİR aynı algoritma.
# saQut/Java ile oran hesabı için bu dosya değiştirilmez.

import time
import sys

def fib(n):
    if n <= 1:
        return n
    return fib(n - 1) + fib(n - 2)

RUNS = 5
N = 25

# Isınma (JIT benzeri byte-code önbellekleme için)
fib(N)

times = []
for _ in range(RUNS):
    t0 = time.perf_counter()
    result = fib(N)
    t1 = time.perf_counter()
    times.append(t1 - t0)

best_us  = min(times) * 1e6
avg_us   = (sum(times) / len(times)) * 1e6

print(f"fib({N}) = {result}", file=sys.stderr)
print(f"CPython  runs={RUNS}  avg={avg_us:.1f}µs  best={best_us:.1f}µs", file=sys.stderr)

# stdout'a makine-okunabilir çıktı (run_bench.sh için)
print(f"PYTHON_FIB_BEST_US={best_us:.0f}")
print(f"PYTHON_FIB_AVG_US={avg_us:.0f}")
