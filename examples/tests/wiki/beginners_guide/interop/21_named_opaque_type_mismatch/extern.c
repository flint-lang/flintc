#include <stdlib.h>

typedef void *GLObject;
typedef void *GLFrame;

GLFrame create_frame() {
    GLFrame frame = (GLFrame)malloc(8);
    *(size_t *)frame = (size_t)100;
    return frame;
}

void free_frame(GLFrame *value) {
    free(*value);
    *value = NULL;
}

GLObject create_object() {
    GLObject frame = (GLObject)malloc(8);
    *(size_t *)frame = (size_t)100;
    return frame;
}

void free_object(GLObject *value) {
    free(*value);
    *value = NULL;
}
