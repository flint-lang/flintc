#include <stdio.h>
#include <stdlib.h>

typedef void *NamedOpaque;

NamedOpaque create_named_opaque() {
    printf("Creating new named opaque\n");
    NamedOpaque no = (NamedOpaque)malloc(8);
    *(size_t *)no = (size_t)100;
    return no;
}

void free_named_opaque(NamedOpaque *value) {
    printf("Freeing named opaque %p\n", *value);
    free(*value);
    *value = NULL;
}
