#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_LEN 60

static const char *alu = "GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGG"
                         "GAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAG"
                         "ACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAA"
                         "ATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATC"
                         "CCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACC"
                         "CGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGC"
                         "ACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAA";
static const char *iub_codes = "ACGTBDHKMNRSVWY";
static const char *iub = "ACGT";

typedef struct {
    char *data;
    uint32_t pos;
    uint32_t len;
} Buffer;

void write_char(Buffer *buf, char c) {
    if (buf->pos >= buf->len) {
        buf->len *= 2;
        buf->data = realloc(buf->data, buf->len);
    }
    buf->data[buf->pos++] = c;
}

void write_string(Buffer *buf, const char *str) {
    while (*str) {
        write_char(buf, *str++);
    }
}

void fasta(Buffer *buf, uint32_t n) {
    uint32_t todo = n;
    write_string(buf, ">ONE Homo sapiens alu\n");

    uint32_t j = 0;
    const uint32_t alu_len = strlen(alu);
    while (todo > 0) {
        uint32_t chunk = todo < LINE_LEN ? todo : LINE_LEN;
        for (uint32_t i = 0; i < chunk; i++) {
            write_char(buf, alu[j++ % alu_len]);
        }
        write_char(buf, '\n');
        todo -= chunk;
    }

    write_string(buf, ">TWO IUB ambiguity codes\n");
    todo = n;
    j = 0;
    const uint32_t iub_codes_len = strlen(iub_codes);
    while (todo > 0) {
        uint32_t chunk = todo < LINE_LEN ? todo : LINE_LEN;
        for (uint32_t i = 0; i < chunk; i++) {
            write_char(buf, iub_codes[j++ % iub_codes_len]);
        }
        write_char(buf, '\n');
        todo -= chunk;
    }

    write_string(buf, ">THREE Homo sapiens frequency\n");
    todo = n;
    j = 0;
    const uint32_t iub_len = strlen(iub);
    while (todo > 0) {
        uint32_t chunk = todo < LINE_LEN ? todo : LINE_LEN;
        for (uint32_t i = 0; i < chunk; i++) {
            write_char(buf, iub[j++ % iub_len]);
        }
        write_char(buf, '\n');
        todo -= chunk;
    }
    write_char(buf, '\0');
}

int main(int argc, char *argv[]) {
    int n = 1000;
    if (argc > 1) {
        n = atoi(argv[1]);
    }

    Buffer buf = (Buffer){
        .len = n * 100,
        .data = malloc(n * 100),
        .pos = 0,
    };
    fasta(&buf, n);

    printf("%s", buf.data);
    free(buf.data);
    return 0;
}
