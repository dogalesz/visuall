#include <cstdio>
#include <cstring>
#include <chrono>

using Clock = std::chrono::high_resolution_clock;
using Ms = std::chrono::duration<double, std::milli>;

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
    for (int n = 2; n < limit; n++) count += is_prime(n);
    return count;
}

static long long build_tree_sum(int depth, long long val) {
    if (depth <= 0) return val;
    return val + build_tree_sum(depth - 1, val * 2) + build_tree_sum(depth - 1, val * 2 + 1);
}

static int collatz_steps(int n) {
    int steps = 0;
    while (n != 1) { if (n % 2 == 0) n /= 2; else n = 3 * n + 1; steps++; }
    return steps;
}

static int string_build(int iterations) {
    int total_len = 0;
    for (int i = 0; i < iterations; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "item_%d_value_%d", i, i * 7);
        total_len += (int)strlen(buf);
    }
    return total_len;
}

static double compute_pi(int terms) {
    double pi = 0.0, sign = 1.0;
    for (int i = 0; i < terms; i++) { pi += sign / (double)(2 * i + 1); sign *= -1.0; }
    return pi * 4.0;
}

static int nested_loops(int n) {
    int total = 0;
    for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) total += (i * j) % 97;
    return total;
}

static int ackermann(int m, int n) {
    if (m == 0) return n + 1;
    if (n == 0) return ackermann(m - 1, 1);
    return ackermann(m - 1, ackermann(m, n - 1));
}

static int gcd(int a, int b) { while (b) { int t = b; b = a % b; a = t; } return a; }
static long long gcd_sum(int limit) {
    long long total = 0;
    for (int i = 1; i <= limit; i++) for (int j = i; j <= limit; j++) total += gcd(i, j);
    return total;
}

static int fibonacci(int n) {
    if (n <= 1) return n;
    int a = 0, b = 1;
    for (int i = 2; i <= n; i++) { int t = b; b = a + b; a = t; }
    return b;
}

static double dist_bench(int count) {
    double total = 0.0;
    for (int i = 0; i < count; i++) {
        double x = (double)(i % 100) - 50.0, y = (double)(i % 73) - 36.5;
        total += x * x + y * y;
    }
    return total;
}

#define TIME(label, code) do { \
    auto t0 = Clock::now(); \
    code; \
    auto t1 = Clock::now(); \
    printf("  %-20s %8.1f ms\n", label, Ms(t1 - t0).count()); \
} while(0)

int main() {
    int r1; TIME("1-Primes", { for (int i = 0; i < 3; i++) r1 = count_primes(100000); });
    long long r2; TIME("2-TreeSum", { r2 = build_tree_sum(22, 1); });
    long long r3 = 0; TIME("3-Collatz", { for (int n = 1; n <= 100000; n++) r3 += collatz_steps(n); });
    int r4; TIME("4-Strings", { r4 = string_build(200000); });
    double r5; TIME("5-Pi", { r5 = compute_pi(10000000); });
    int r6; TIME("6-Nested", { r6 = nested_loops(2000); });
    int r7; TIME("7-Ackermann", { r7 = ackermann(3, 11); });
    long long r8; TIME("8-GCD", { r8 = gcd_sum(2000); });
    long long r9 = 0; TIME("9-Fibonacci", { for (int i = 0; i < 500000; i++) r9 += fibonacci(35); });
    double r10; TIME("10-Dist", { r10 = dist_bench(1000000); });

    printf("\nResults: %d %lld %lld %d %.5f %d %d %lld %lld %.0f\n",
        r1, r2, r3, r4, r5, r6, r7, r8, r9, r10);
}
