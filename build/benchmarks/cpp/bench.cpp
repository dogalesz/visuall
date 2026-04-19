#include <cstdio>

int fibonacci(int n) {
    if (n <= 1) return n;
    int a = 0, b = 1;
    for (int i = 2; i <= n; i++) {
        int temp = b;
        b = a + b;
        a = temp;
    }
    return b;
}

int main() {
    long long total = 0;
    for (int i = 0; i < 1000000; i++) {
        total += fibonacci(30);
    }
    printf("%lld\n", total);
    return 0;
}
