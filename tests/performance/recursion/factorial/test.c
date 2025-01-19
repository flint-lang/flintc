#include <stdio.h>

int factorial(int num) {
    if (num == 0) {
        return 1;
    } else {
        return num * factorial(num - 1);
    }
}

int main() {
    int fac = factorial(12);
    printf("%i", fac);
}
