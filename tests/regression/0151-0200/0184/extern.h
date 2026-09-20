#include <stdio.h>

enum MyEnum {
    VAL1 = 0,
    VAL2 = 1,
    VAL3 = 2,
    VAL4 = 4,
    VAL5 = 8,
};

void print_enum(const enum MyEnum e) {
    if (e == VAL1) {
        printf("is VAL1\n");
    } else if (e == VAL2) {
        printf("is VAL2\n");
    } else if (e == VAL3) {
        printf("is VAL3\n");
    } else if (e == VAL4) {
        printf("is VAL4\n");
    } else if (e == VAL5) {
        printf("is VAL5\n");
    } else {
        printf("is invalid enum value\n");
    }
}

void add(int *lhs, int rhs) {
    *lhs = *lhs + rhs;
}
