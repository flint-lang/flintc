#include <stdbool.h>
#include <stdio.h>

typedef struct Params {
  union {
    bool no_reset;
    bool keep;
  };
  int x;
  float y;
  bool b;
} Params;
void _CallParams(Params params);
#define CallParams(...) _CallParams((Params){__VA_ARGS__})

void _CallParams(Params params) {
  printf("no_reset = %b\n", params.no_reset);
  printf("keep = %b\n", params.keep);
  printf("x = %i\n", params.x);
  printf("y = %f\n", params.y);
  printf("b = %i\n", params.b);
}

int main() {
  _CallParams((Params){.x = 10, .y = 3.14f, .b = false});
  CallParams(.keep = true);
  CallParams(.no_reset = false);
  CallParams(.b = false, .x = 5);
  CallParams(.x = 10, .y = 5.0f);
  CallParams(.x = 15, .y = 10.0f, .b = true);
}
