#include <stdio.h>

// Function to calculate the square root of a number using Babylonians method
int square_root(int n) {
    // if (n < 0) {
    //     printf("Error: Square root of negative numbers is not supported.\n");
    //     return -1; // Or any other error value
    // }

    if (n == 0 || n == 1) {
        return n; // Since sqrt(0) = 0 and sqrt(1) = 1
    }

    int x = n;
    int y = (x + 1) / 2;

    while (y < x) {
        x = y;
        y = (x + (n / x)) / 2;
    }

    return x;
}

// Calculate the sqrt of every number from 0 to 100000
int main() {
    int num = 0;
    int sum = 0;
    while (num < 10000000) {
        int sqrt = square_root(num);
        ++num;
        sum = sum + sqrt;
    }
    printf("%i\n", sum);
}
