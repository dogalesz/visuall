#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <chrono>

// ── 1. Sieve of Eratosthenes ──────────────────────────────────────
int sieve(int limit) {
    std::vector<bool> flags(limit, true);
    for (int p = 2; p * p < limit; p++) {
        if (flags[p]) {
            for (int j = p * p; j < limit; j += p)
                flags[j] = false;
        }
    }
    int count = 0;
    for (int k = 2; k < limit; k++)
        if (flags[k]) count++;
    return count;
}

// ── 2. Matrix multiply (flat arrays) ─────────────────────────────
double matrix_multiply(int n) {
    std::vector<double> a(n * n), b(n * n), c(n * n, 0.0);
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            a[i * n + j] = (double)(i + j) * 1.1;
            b[i * n + j] = (double)(i - j) * 0.9;
        }
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            double sum = 0.0;
            for (int k = 0; k < n; k++)
                sum += a[i * n + k] * b[k * n + j];
            c[i * n + j] = sum;
        }
    return c[0] + c[n * n - 1];
}

// ── 3. Recursive tree sum ─────────────────────────────────────────
int build_tree_sum(int depth, int val) {
    if (depth <= 0) return val;
    int left_sum = build_tree_sum(depth - 1, val * 2);
    int right_sum = build_tree_sum(depth - 1, val * 2 + 1);
    return val + left_sum + right_sum;
}

// ── 4. Collatz conjecture ─────────────────────────────────────────
int collatz_steps(int n) {
    int steps = 0;
    while (n != 1) {
        if (n % 2 == 0) n /= 2;
        else n = 3 * n + 1;
        steps++;
    }
    return steps;
}

// ── 5. String building ───────────────────────────────────────────
int string_build(int iterations) {
    int total_len = 0;
    for (int i = 0; i < iterations; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "item_%d_value_%d", i, i * 7);
        total_len += (int)strlen(buf);
    }
    return total_len;
}

// ── 6. Floating-point: Leibniz pi ────────────────────────────────
double compute_pi(int terms) {
    double pi = 0.0, sign = 1.0;
    for (int i = 0; i < terms; i++) {
        pi += sign / (double)(2 * i + 1);
        sign *= -1.0;
    }
    return pi * 4.0;
}

// ── 7. Insertion sort ────────────────────────────────────────────
int insertion_sort(int size) {
    std::vector<int> arr(size);
    for (int i = 0; i < size; i++)
        arr[i] = ((long long)i * 1103515245 + 12345) % 100000;
    for (int i = 1; i < size; i++) {
        int key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
    return arr[0] + arr[size - 1];
}

// ── 8. Nested loops + modular arithmetic ─────────────────────────
int nested_loops(int n) {
    int total = 0;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            total += (i * j) % 97;
    return total;
}

// ═══════════════════════════════════════════════════════════════════

int main() {
    // Warm up
    sieve(100);

    // 1. Sieve 20 times
    int r1 = 0;
    for (int iter = 0; iter < 20; iter++)
        r1 = sieve(100000);
    printf("Sieve primes:    %d\n", r1);

    // 2. Matrix multiply 150×150, 5 times
    double r2 = 0.0;
    for (int iter = 0; iter < 5; iter++)
        r2 = matrix_multiply(150);
    printf("Matrix corner:   %f\n", r2);

    // 3. Recursive tree sum depth=22
    int r3 = build_tree_sum(22, 1);
    printf("Tree sum:        %d\n", r3);

    // 4. Collatz 1..50000
    int r4 = 0;
    for (int n = 1; n <= 50000; n++)
        r4 += collatz_steps(n);
    printf("Collatz total:   %d\n", r4);

    // 5. String building 200000
    int r5 = string_build(200000);
    printf("String len sum:  %d\n", r5);

    // 6. Pi with 5M terms
    double r6 = compute_pi(5000000);
    printf("Pi approx:       %.15f\n", r6);

    // 7. Insertion sort 5000, 3 times
    int r7 = 0;
    for (int iter = 0; iter < 3; iter++)
        r7 = insertion_sort(5000);
    printf("Sort endpoints:  %d\n", r7);

    // 8. Nested loops 3000×3000
    int r8 = nested_loops(3000);
    printf("Nested total:    %d\n", r8);

    printf("done\n");
    return 0;
}
