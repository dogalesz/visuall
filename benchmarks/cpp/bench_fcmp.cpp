#include <cstdio>
#include <cstdint>
#include <vector>

int main() {
    const int n = 10000;
    int64_t total = 0;
    for (int i = 0; i < n; i++) {
        std::vector<int64_t> evens;
        for (int x = 0; x < 100; x++) {
            if (x % 2 == 0) evens.push_back((int64_t)x * x);
        }
        total += evens[0] + evens[49];
    }
    printf("%lld\n", (long long)total);
    return 0;
}
