#include <cstdio>
#include <cstdint>
#include <vector>
#include <numeric>

int main() {
    const int n = 2000000;
    const int size = 10000;
    std::vector<int64_t> nums(size);
    std::iota(nums.begin(), nums.end(), 0);
    for (int i = 0; i < n; i++) {
        nums[i % size] = (int64_t)i * 3 + 1;
    }
    int64_t total = 0;
    for (int i = 0; i < size; i++) total += nums[i];
    printf("%lld\n", (long long)total);
    return 0;
}
