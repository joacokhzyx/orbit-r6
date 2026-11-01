#include "../../src/codegen-x64/asm_win64.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    FILE *in, *out;
    R6CoffObject object;
    char error[256];
    char *text;
    long size;
    bool ok;
    if (argc != 3) {
        fprintf(stderr, "usage: asm-probe <input.s> <output.o>\n");
        return 2;
    }
    in = fopen(argv[1], "rb");
    if (!in) { perror(argv[1]); return 1; }
    if (fseek(in, 0, SEEK_END) != 0) { perror(argv[1]); fclose(in); return 1; }
    size = ftell(in);
    if (size < 0) { perror(argv[1]); fclose(in); return 1; }
    if (fseek(in, 0, SEEK_SET) != 0) { perror(argv[1]); fclose(in); return 1; }
    text = malloc((size_t)size + 1);
    if (!text) {
        fprintf(stderr, "asm-probe: out of memory reading %s\n", argv[1]);
        fclose(in);
        return 1;
    }
    if (fread(text, 1, (size_t)size, in) != (size_t)size) {
        fprintf(stderr, "asm-probe: cannot read %s\n", argv[1]);
        free(text);
        fclose(in);
        return 1;
    }
    text[size] = '\0';
    fclose(in);
    r6_coff_init(&object);
    if (!r6_assemble_win64(text, (size_t)size, &object, error, sizeof error)) {
        fprintf(stderr, "asm-probe: %s\n", error);
        free(text); r6_coff_free(&object); return 1;
    }
    free(text);
    out = fopen(argv[2], "wb");
    if (!out) { perror(argv[2]); r6_coff_free(&object); return 1; }
    ok = r6_coff_write(&object, out);
    if (fclose(out) != 0) ok = false;
    if (!ok) fprintf(stderr, "asm-probe: %s\n", r6_coff_error(&object));
    r6_coff_free(&object);
    return ok ? 0 : 1;
}
