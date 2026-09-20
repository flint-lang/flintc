#include <stdio.h>

typedef struct MyStruct {
    int x;
    float y;
    size_t z;
} MyStruct;

typedef enum MyEnum {
    VAL1 = 0,
    VAL2 = 1,
    VAL3 = 2,
    VAL4 = 4,
    VAL5 = 8,
} MyEnum;

void print_enum(const MyEnum e) {
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

void add_structs(MyStruct *s1, const MyStruct s2) {
    s1->x += s2.x;
    s1->y += s2.y;
    s1->z += s2.z;
}

void add(int *lhs, int rhs) {
    *lhs = *lhs + rhs;
}
