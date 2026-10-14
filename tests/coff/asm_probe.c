#include "../../src/codegen-x64/asm_win64.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    FILE *in, *out;
    R6CoffObject object;
    char error[256];
    bool ok;
    if (argc != 3) {
        fprintf(stderr, "usage: asm-probe <input.s> <output.o>\n");
        return 2;
    }
    in = fopen(argv[1], "rb");
    if (!in) { perror(argv[1]); return 1; }
    r6_coff_init(&object);
    if (!r6_assemble_win64(in, &object, error, sizeof error)) {
        fprintf(stderr, "asm-probe: %s\n", error);
        fclose(in); r6_coff_free(&object); return 1;
    }
    fclose(in);
    out = fopen(argv[2], "wb");
    if (!out) { perror(argv[2]); r6_coff_free(&object); return 1; }
    ok = r6_coff_write(&object, out);
    if (fclose(out) != 0) ok = false;
    if (!ok) fprintf(stderr, "asm-probe: %s\n", r6_coff_error(&object));
    r6_coff_free(&object);
    return ok ? 0 : 1;
}
