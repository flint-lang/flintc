#include "mylib.h"

#include <stdio.h>

int main(void) {
    if (!mylib_init()) {
        printf("mylib_init failed\n");
        return 1;
    }
    int result = mylib_add(10, 20);
    printf("result = %i\n", result);
    return 0;
}