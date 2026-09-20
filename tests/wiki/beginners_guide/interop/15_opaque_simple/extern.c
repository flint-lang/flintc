#include <stdlib.h>

typedef struct Container {
    void *value;
    size_t len;
} Container;

Container create() {
    return (Container){
        .value = malloc(100),
        .len = 100,
    };
}

void destroy(Container *c) {
    free(c->value);
    c->value = NULL;
    c->len = 0;
}
