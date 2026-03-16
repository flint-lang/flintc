#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

uint64_t fibonacci(uint32_t n) {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

int main(int argc, char *argv[]) {
    uint32_t n = 30;
    if (argc > 1) {
        n = atoi(argv[1]);
    }
    printf("%ld\n", fibonacci(n));
    return 0;
}
