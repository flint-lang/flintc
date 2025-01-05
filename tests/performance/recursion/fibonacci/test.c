#include <stdio.h>

int fibonacci(int num) {
    if (num <= 1) {
        return num;
    } else {
        return fibonacci(num - 1) + fibonacci(num - 2);
    }
}

int main() {
    int fac = fibonacci(40);
    printf("%i\n", fac);
}
