#include <cstdio>
#include <cstdint>
#include <tuple>

int main() {
    const int64_t n = 500000;
    int64_t total = 0;
    for (int64_t i = 0; i < n; i++) {
        auto t = std::make_tuple(i, i * 2, i * 3);
        auto [a, b, c] = t;
        total += a + b + c;
    }
    printf("%lld\n", (long long)total);
    return 0;
}
