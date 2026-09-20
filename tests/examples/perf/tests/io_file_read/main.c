#include <stdio.h>
#include <stdlib.h>

int main(void) {
    const char *filename = "input.txt";

    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Could not open file: %s\n", filename);
        return 1;
    }

    char buffer[4096];
    size_t total_bytes = 0;
    size_t bytes;

    while ((bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        total_bytes += bytes;
    }

    fclose(file);
    printf("%zu\n", total_bytes);
    return 0;
}
