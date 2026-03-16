#include <stdio.h>

#define N 100

void matrix_multiply(int a[N][N], int b[N][N], int result[N][N]) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            result[i][j] = 0;
            for (int k = 0; k < N; k++) {
                result[i][j] += a[i][k] * b[k][j];
            }
        }
    }
}

int main(void) {
    static int a[N][N];
    static int b[N][N];
    static int result[N][N];
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            a[i][j] = i + j;
            b[i][j] = i * j + 1;
        }
    }
    matrix_multiply(a, b, result);

    unsigned int sum = 0;
    for (int i = 0; i < N; i++) {
        sum += result[i][i];
    }

    printf("%u\n", sum);
    return 0;
}
