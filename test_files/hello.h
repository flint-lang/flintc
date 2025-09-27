#include <stdbool.h>
#include <stdio.h>

void hello() {
    printf("Hello from C!\n");
}

int add(int x, int y) {
    return x + y;
}
typedef struct {
    int x, y;
    float speed;
    bool is_something;
} MyData;

MyData do_something(const MyData md) {
    MyData result = {0};
    result.x = md.x + 2;
    result.y = md.y + 5;
    result.speed = md.speed / 2;
    result.is_something = true;
    return result;
}
