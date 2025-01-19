#include <stdio.h>

int main() {
    int num = 0;
    while (num < 1000000) {
        printf("%i", num);
        ++num;
    }
}
