#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

uint32_t ackermann(uint32_t m, uint32_t n) {
    if (m == 0) {
        return n + 1;
    }
    if (n == 0) {
        return ackermann(m - 1, 1);
    }
    return ackermann(m - 1, ackermann(m, n - 1));
}

int main(int argc, char *argv[]) {
    uint32_t m = 3;
    uint32_t n = 10;
    if (argc > 2) {
        m = atoi(argv[1]);
        n = atoi(argv[2]);
    }
    printf("%d\n", ackermann(m, n));
    return 0;
}
