#include <cstdio>
#include <cstdint>
#include <string>

int main() {
    const int n = 500000;
    int64_t matches = 0;
    for (int i = 0; i < n; i++) {
        std::string a = "key_" + std::to_string(i);
        std::string b = "key_" + std::to_string(i);
        if (a == b) matches++;
    }
    printf("%lld\n", (long long)matches);
    return 0;
}
