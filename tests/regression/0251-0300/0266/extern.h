#include <stdbool.h>

typedef struct {
    int x, y;
    float speed;
    bool is_something;
} MyData;

MyData do_something(const MyData *md) {
    MyData result = {0};
    result.x = md->x + 2;
    result.y = md->y + 5;
    result.speed = md->speed / 2;
    result.is_something = true;
    return result;
}

typedef struct {
    int x, y, z;
    float dx, dy, dz;
    float speed;
} BigData;

BigData big_data_do(const BigData bd) {
    BigData result = {0};
    result.x = bd.x + 2;
    result.y = bd.y + 3;
    result.z = bd.z + 4;
    result.dx = bd.dx * 2;
    result.dy = bd.dy * 3;
    result.dz = bd.dz * 4;
    result.speed = bd.speed - 3.0;
    return result;
}
