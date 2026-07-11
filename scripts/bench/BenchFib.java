// Cetvel karşılaştırması: bench_fib.sqt ile BİREBİR aynı algoritma.
// JVM ısınması atılır: ilk 1000 tur ölçülmez.
public class BenchFib {
    static int fib(int n) {
        if (n <= 1) return n;
        return fib(n - 1) + fib(n - 2);
    }

    public static void main(String[] args) {
        final int N    = 25;
        final int WARM = 1000;
        final int RUNS = 5;

        // JIT ısınması
        for (int i = 0; i < WARM; i++) fib(N);

        long[] times = new long[RUNS];
        int result = 0;
        for (int r = 0; r < RUNS; r++) {
            long t0 = System.nanoTime();
            result = fib(N);
            long t1 = System.nanoTime();
            times[r] = t1 - t0;
        }

        long best = Long.MAX_VALUE;
        long total = 0;
        for (long t : times) { if (t < best) best = t; total += t; }
        long avg = total / RUNS;

        System.err.printf("fib(%d) = %d%n", N, result);
        System.err.printf("Java(JIT)  runs=%d  avg=%dµs  best=%dµs%n",
            RUNS, avg / 1000, best / 1000);

        System.out.printf("JAVA_FIB_BEST_US=%d%n", best / 1000);
        System.out.printf("JAVA_FIB_AVG_US=%d%n", avg / 1000);
    }
}
