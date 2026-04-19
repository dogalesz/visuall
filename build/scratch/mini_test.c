#include <stdio.h>
#include <stdint.h>

static int fast_i64_to_buf(int64_t val, char* buf) {
    char tmp[21];
    int pos = 0;
    int neg = 0;
    uint64_t uv;

    if (val < 0) {
        neg = 1;
        uv = (uint64_t)(-(val + 1)) + 1;
    } else {
        uv = (uint64_t)val;
    }

    do {
        tmp[pos++] = '0' + (char)(uv % 10);
        uv /= 10;
    } while (uv);

    int len = 0;
    if (neg) buf[len++] = '-';
    for (int i = pos - 1; i >= 0; i--)
        buf[len++] = tmp[i];
    buf[len] = '\0';
    return len;
}

int main() {
    char buf[24];
    for (int64_t i = 0; i < 200000; i++) {
        fast_i64_to_buf(i, buf);
        fast_i64_to_buf(i * 7, buf);
    }
    printf("OK\n");
    return 0;
}
