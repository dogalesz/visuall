#include <cstdio>
#include <cstdint>
#include <unordered_map>
#include <string>

int main() {
    const int n = 200000;
    int64_t total = 0;
    for (int i = 0; i < n; i++) {
        std::unordered_map<std::string, int64_t> d;
        d["alpha"] = i;
        d["beta"] = i + 1;
        d["gamma"] = i + 2;
        d["delta"] = i + 3;
        d["epsilon"] = i + 4;
        total += i;
    }
    printf("%lld\n", (long long)total);
    return 0;
}
