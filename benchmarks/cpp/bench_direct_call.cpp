#include <cstdio>
int compute(int x) { return x * 3 + 1; }
int main() { long long total=0; for (int i=0; i<1000000; i++) total+=compute(i); printf("%lld\n", total); return 0; }
