#!/usr/bin/env python3
# Cetvel karşılaştırması: bench_sum.sqt ile BİREBİR aynı algoritma.

import time
import sys

def sum_loop(n):
    total = 0
    i = 1
    while i <= n:
        total += i
        i += 1
    return total

RUNS = 5
N = 40000

# Isınma
sum_loop(N)

times = []
for _ in range(RUNS):
    t0 = time.perf_counter()
    result = sum_loop(N)
    t1 = time.perf_counter()
    times.append(t1 - t0)

best_us = min(times) * 1e6
avg_us  = (sum(times) / len(times)) * 1e6

print(f"sum_loop({N}) = {result}", file=sys.stderr)
print(f"CPython  runs={RUNS}  avg={avg_us:.1f}µs  best={best_us:.1f}µs", file=sys.stderr)

print(f"PYTHON_SUM_BEST_US={best_us:.0f}")
print(f"PYTHON_SUM_AVG_US={avg_us:.0f}")
