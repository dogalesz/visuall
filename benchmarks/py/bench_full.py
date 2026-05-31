# Complex Benchmark Suite for Python 3
import sys
sys.setrecursionlimit(1000000)
import time

def count_primes(limit):
    def is_prime(n):
        if n < 2: return 0
        if n < 4: return 1
        if n % 2 == 0: return 0
        d = 3
        while d * d <= n:
            if n % d == 0: return 0
            d += 2
        return 1
    count = 0
    for n in range(2, limit):
        count += is_prime(n)
    return count

def build_tree_sum(depth, val):
    if depth <= 0:
        return val
    left_sum = build_tree_sum(depth - 1, val * 2)
    right_sum = build_tree_sum(depth - 1, val * 2 + 1)
    return val + left_sum + right_sum

def collatz_steps(n):
    steps = 0
    while n != 1:
        if n % 2 == 0: n //= 2
        else: n = 3 * n + 1
        steps += 1
    return steps

def string_build(iterations):
    total_len = 0
    for i in range(iterations):
        s = f"item_{i}_value_{i * 7}"
        total_len += len(s)
    return total_len

def compute_pi(terms):
    pi = 0.0
    sign = 1.0
    for i in range(terms):
        pi += sign / (2 * i + 1)
        sign *= -1.0
    return pi * 4.0

def nested_loops(n):
    total = 0
    for i in range(n):
        for j in range(n):
            total += (i * j) % 97
    return total

def ackermann(m, n):
    if m == 0: return n + 1
    if n == 0: return ackermann(m - 1, 1)
    return ackermann(m - 1, ackermann(m, n - 1))

def gcd(a, b):
    while b != 0:
        a, b = b, a % b
    return a

def gcd_sum(limit):
    total = 0
    for i in range(1, limit + 1):
        for j in range(i, limit + 1):
            total += gcd(i, j)
    return total

def fibonacci(n):
    if n <= 1: return n
    a, b = 0, 1
    for _ in range(2, n + 1):
        a, b = b, a + b
    return b

def dist_bench(count):
    total_dist = 0.0
    for i in range(count):
        x = float(i % 100) - 50.0
        y = float(i % 73) - 36.5
        total_dist += x * x + y * y
    return total_dist

t0 = time.time()

r1 = 0
for _ in range(3):
    r1 = count_primes(100000)
t1 = time.time()

r2 = build_tree_sum(22, 1)
t2 = time.time()

r3 = 0
for n in range(1, 100001):
    r3 += collatz_steps(n)
t3 = time.time()

r4 = string_build(200000)
t4 = time.time()

r5 = compute_pi(10000000)
t5 = time.time()

r6 = nested_loops(2000)
t6 = time.time()

r7 = ackermann(3, 11)
t7 = time.time()

r8 = gcd_sum(2000)
t8 = time.time()

r9 = 0
for _ in range(500000):
    r9 += fibonacci(35)
t9 = time.time()

r10 = dist_bench(1000000)
t10 = time.time()

print(f"1-Primes:      {(t1-t0)*1000:.1f} ms")
print(f"2-TreeSum:     {(t2-t1)*1000:.1f} ms")
print(f"3-Collatz:     {(t3-t2)*1000:.1f} ms")
print(f"4-Strings:     {(t4-t3)*1000:.1f} ms")
print(f"5-Pi:          {(t5-t4)*1000:.1f} ms")
print(f"6-Nested:      {(t6-t5)*1000:.1f} ms")
print(f"7-Ackermann:   {(t7-t6)*1000:.1f} ms")
print(f"8-GCD:         {(t8-t7)*1000:.1f} ms")
print(f"9-Fibonacci:   {(t9-t8)*1000:.1f} ms")
print(f"10-Dist:       {(t10-t9)*1000:.1f} ms")

print(f"\nTotal: {sum([r1,r2,r3,r4,r5,r6,r7,r8,r9,r10])}")
