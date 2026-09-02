#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifndef FLINT_OPT
#define FLINT_OPT(T) struct { bool has_value; T value; }
#endif

#ifndef FLINT_f32x3
#define FLINT_f32x3
typedef struct f32x3 { float v0; float v1; float v2; } f32x3;
#endif

typedef struct mylib_MyData mylib_MyData;

typedef struct mylib_MyData {
    int32_t x;
    int32_t y;
    FLINT_OPT(mylib_MyData *) next;
} mylib_MyData;

/// @brief Returns 'false' on failure and 'true' if everything was OK
extern bool mylib_init(void) asm("flint.init.mylib");

void mylib_MyFunc_print(const mylib_MyData *d1) asm("Z5OgXomk.MyFunc.print.export");
void mylib_MyFunc_print_mut(mylib_MyData *d1) asm("Z5OgXomk.MyFunc.print_mut.export");
int32_t mylib_add(const int32_t x, const int32_t y) asm("Z5OgXomk.add.export");
f32x3 mylib_add_vec(const f32x3 v1, const f32x3 v2) asm("Z5OgXomk.add_vec.export");
char * mylib_add_str(const char *s1, const char *s2) asm("Z5OgXomk.add_str.export");
