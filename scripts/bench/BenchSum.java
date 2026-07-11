// Cetvel karşılaştırması: bench_sum.sqt ile BİREBİR aynı algoritma.
public class BenchSum {
    static int sumLoop(int n) {
        int total = 0;
        for (int i = 1; i <= n; i++) {
            total += i;
        }
        return total;
    }

    public static void main(String[] args) {
        final int N    = 40000;
        final int WARM = 1000;
        final int RUNS = 5;

        for (int i = 0; i < WARM; i++) sumLoop(N);

        long[] times = new long[RUNS];
        int result = 0;
        for (int r = 0; r < RUNS; r++) {
            long t0 = System.nanoTime();
            result = sumLoop(N);
            long t1 = System.nanoTime();
            times[r] = t1 - t0;
        }

        long best = Long.MAX_VALUE;
        long total = 0;
        for (long t : times) { if (t < best) best = t; total += t; }
        long avg = total / RUNS;

        System.err.printf("sumLoop(%d) = %d%n", N, result);
        System.err.printf("Java(JIT)  runs=%d  avg=%dµs  best=%dµs%n",
            RUNS, avg / 1000, best / 1000);

        System.out.printf("JAVA_SUM_BEST_US=%d%n", best / 1000);
        System.out.printf("JAVA_SUM_AVG_US=%d%n", avg / 1000);
    }
}
