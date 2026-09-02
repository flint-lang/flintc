#include "mylib.h"

#include <stdio.h>

int main(void) {
    if (!mylib_init()) {
        return 1;
    }
    int result = mylib_add(10, 20);
    printf("result = %d\n", result);

    f32x3 vec_result = mylib_add_vec(
        (f32x3){
            .v0 = 5.6,
            .v1 = 6.7,
            .v2 = 7.8,
        },
        (f32x3){
            .v0 = 2.2,
            .v1 = 3.3,
            .v2 = 4.4,
        });
    printf("vec_result = (%f, %f, %f)\n", vec_result.v0, vec_result.v1, vec_result.v2);
    mylib_MyData d3 = (mylib_MyData){
        .x = 70,
        .y = 80,
        .next = {.has_value = false, .value = NULL},
    };
    mylib_MyData d2 = (mylib_MyData){
        .x = 50,
        .y = 60,
        .next = {.has_value = true, .value = &d3},
    };
    mylib_MyData d1 = (mylib_MyData){
        .x = 30,
        .y = 40,
        .next = {.has_value = true, .value = &d2},
    };
    mylib_MyData d0 = (mylib_MyData){
        .x = 10,
        .y = 20,
        .next = {.has_value = true, .value = &d1},
    };
    mylib_MyFunc_print(&d0);
    mylib_MyFunc_print_mut(&d0);

    const char *s = mylib_add_str("String 1", " plus String 2");
    printf("s = %s\n", s);
    return 0;
}
