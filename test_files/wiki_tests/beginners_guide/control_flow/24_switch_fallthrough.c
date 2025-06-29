#include <stdio.h>

typedef enum { VAL1, VAL2, VAL3 } MyEnum;

int main() {
  MyEnum e = VAL1; // In C you dont need to write MyEnum.VAL1
  switch (e) {
  case VAL1:
    printf("is val1\n");
  case VAL2:
    printf("is val2\n");
  case VAL3:
    printf("is val3\n");
  }
}
