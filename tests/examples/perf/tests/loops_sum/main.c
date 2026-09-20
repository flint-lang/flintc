#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

uint64_t sum(uint32_t n) {
    uint64_t total = 0;
    for (uint32_t i = 1; i <= n; i++) {
        total += i;
    }
    return total;
}

int main(int argc, char *argv[]) {
    uint32_t n = 1000000;
    if (argc > 1) {
        n = atoi(argv[1]);
    }
    printf("%ld\n", sum(n));
    return 0;
}
