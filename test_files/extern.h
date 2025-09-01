typedef struct BigStruct {
    float x, y, z;
    int r, g, b, a;
} BigStruct;

BigStruct add_big(const BigStruct b1, const BigStruct b2) {
    BigStruct result = {0};
    result.x = b1.x + b2.x;
    result.y = b1.y + b2.y;
    result.z = b1.z + b2.z;
    result.r = b1.r + b2.r;
    result.g = b1.g + b2.g;
    result.b = b1.b + b2.b;
    result.a = b1.a + b2.a;
    return result;
}

#include <stdio.h>

typedef struct TestStruct {
    char x;
    int y;
    char z, w;
    int a;
} TestStruct;

TestStruct test_add(const TestStruct s1, const TestStruct s2) {
    printf("s1.(x, y, z, w, a) = (%d, %i, %d, %d, %i)\n", s1.x, s1.y, s1.z, s1.w, s1.a);
    printf("s2.(x, y, z, w, a) = (%d, %i, %d, %d, %i)\n", s2.x, s2.y, s2.z, s2.w, s2.a);
    TestStruct result = {0};
    result.x = (char)(s1.x + s2.x);
    result.y = s1.y + s2.y;
    result.z = (char)(s1.z + s2.z);
    result.w = (char)(s1.w + s2.w);
    result.a = s1.a + s2.a;
    return result;
}
