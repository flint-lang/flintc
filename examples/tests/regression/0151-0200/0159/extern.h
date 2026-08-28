#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef struct {
    double x, y;
} Vector2;

Vector2 add_vectors_d(const Vector2 v1, const Vector2 v2) {
    Vector2 result = {v1.x + v2.x, v1.y + v2.y};
    return result;
}

void print_stuff(const char *stuff) {
    printf("%s\n", stuff);
}

typedef struct {
    float x;
    float y;
    float z;
} Vector3;

Vector3 add_vectors(const Vector3 v1, const Vector3 v2) {
    Vector3 result = {v1.x + v2.x, v1.y + v2.y, v1.z + v2.z};
    return result;
}

int foo(const int x, const int y) {
    return x + y;
}

int bar() {
    return 69;
}

bool check() {
    return true;
}
