#include <cstdio>
#include <cstdint>
#include <vector>
#include <numeric>

int main() {
    const int n = 50000;
    std::vector<int64_t> src(1000);
    std::iota(src.begin(), src.end(), 0);
    int64_t total = 0;
    for (int i = 0; i < n; i++) {
        int start = i % 800;
        std::vector<int64_t> s(src.begin() + start, src.begin() + start + 200);
        total += s[0];
    }
    printf("%lld\n", (long long)total);
    return 0;
}
