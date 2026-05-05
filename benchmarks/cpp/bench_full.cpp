#include <cstdio>
#include <cstring>

// ── 1. Count primes via trial division ────────────────────────────
static int is_prime(int n) {
    if (n < 2) return 0;
    if (n < 4) return 1;
    if (n % 2 == 0) return 0;
    for (int d = 3; d * d <= n; d += 2)
        if (n % d == 0) return 0;
    return 1;
}

static int count_primes(int limit) {
    int count = 0;
    for (int n = 2; n < limit; n++)
        count += is_prime(n);
    return count;
}

// ── 2. Recursive tree sum ─────────────────────────────────────────
static long long build_tree_sum(int depth, long long val) {
    if (depth <= 0) return val;
    long long left_sum = build_tree_sum(depth - 1, val * 2);
    long long right_sum = build_tree_sum(depth - 1, val * 2 + 1);
    return val + left_sum + right_sum;
}

// ── 3. Collatz conjecture ─────────────────────────────────────────
static int collatz_steps(int n) {
    int steps = 0;
    while (n != 1) {
        if (n % 2 == 0) n /= 2;
        else n = 3 * n + 1;
        steps++;
    }
    return steps;
}

// ── 4. String building ───────────────────────────────────────────
static int string_build(int iterations) {
    int total_len = 0;
    for (int i = 0; i < iterations; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "item_%d_value_%d", i, i * 7);
        total_len += (int)strlen(buf);
    }
    return total_len;
}

// ── 5. Floating-point: Leibniz pi ────────────────────────────────
static double compute_pi(int terms) {
    double pi = 0.0, sign = 1.0;
    for (int i = 0; i < terms; i++) {
        pi += sign / (double)(2 * i + 1);
        sign *= -1.0;
    }
    return pi * 4.0;
}

// ── 6. Nested loops + modular arithmetic ─────────────────────────
static int nested_loops(int n) {
    int total = 0;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            total += (i * j) % 97;
    return total;
}

// ── 7. Ackermann function ────────────────────────────────────────
static int ackermann(int m, int n) {
    if (m == 0) return n + 1;
    if (n == 0) return ackermann(m - 1, 1);
    return ackermann(m - 1, ackermann(m, n - 1));
}

// ── 8. GCD-heavy computation ─────────────────────────────────────
static int gcd(int a, int b) {
    while (b != 0) {
        int temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

static long long gcd_sum(int limit) {
    long long total = 0;
    for (int i = 1; i <= limit; i++)
        for (int j = i; j <= limit; j++)
            total += gcd(i, j);
    return total;
}

// ── 9. Fibonacci ─────────────────────────────────────────────────
static int fibonacci(int n) {
    if (n <= 1) return n;
    int a = 0, b = 1;
    for (int i = 2; i <= n; i++) {
        int temp = b;
        b = a + b;
        a = temp;
    }
    return b;
}

// ── 10. Float distance (no struct) ───────────────────────────────
static double dist_bench(int count) {
    double total_dist = 0.0;
    for (int i = 0; i < count; i++) {
        double x = (double)(i % 100) - 50.0;
        double y = (double)(i % 73) - 36.5;
        total_dist += x * x + y * y;
    }
    return total_dist;
}

int main() {
    // 1. Count primes 3 times
    int r1 = 0;
    for (int iter = 0; iter < 3; iter++)
        r1 = count_primes(100000);
    printf("Primes < 100000: %d\n", r1);

    // 2. Recursive tree sum depth=22
    long long r2 = build_tree_sum(22, 1);
    printf("Tree sum:        %lld\n", r2);

    // 3. Collatz 1..100000
    long long r3 = 0;
    for (int n = 1; n <= 100000; n++)
        r3 += collatz_steps(n);
    printf("Collatz total:   %lld\n", r3);

    // 4. String building 200000
    int r4 = string_build(200000);
    printf("String len sum:  %d\n", r4);

    // 5. Pi with 10M terms
    double r5 = compute_pi(10000000);
    printf("Pi approx:       %.15f\n", r5);

    // 6. Nested loops 2000×2000
    int r6 = nested_loops(2000);
    printf("Nested total:    %d\n", r6);

    // 7. Ackermann(3, 11)
    int r7 = ackermann(3, 11);
    printf("Ackermann(3,11): %d\n", r7);

    // 8. GCD sum for 1..2000
    long long r8 = gcd_sum(2000);
    printf("GCD sum:         %lld\n", r8);

    // 9. Fibonacci(35) x 500000
    long long r9 = 0;
    for (int iter = 0; iter < 500000; iter++)
        r9 += fibonacci(35);
    printf("Fib sum:         %lld\n", r9);

    // 10. Float distance 1000000 times
    double r10 = dist_bench(1000000);
    printf("Dist sum:        %f\n", r10);

    printf("done\n");
    return 0;
}
