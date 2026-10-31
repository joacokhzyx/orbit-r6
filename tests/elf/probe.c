#include "../../src/codegen-x64/coff.h"
#include "../../src/codegen-x64/elf64.h"

#include <stdio.h>

int main(int argc, char **argv) {
    static const unsigned char code[] = {0xB8, 0x2A, 0x00, 0x00, 0x00, 0xC3};
    R6CoffObject object;
    FILE *out;
    int text;
    bool ok;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <out.o>\n", argv[0]);
        return 2;
    }
    r6_coff_init(&object);
    text = r6_coff_add_section(&object, ".text", 0x60500020u);
    if (text < 0 || !r6_coff_section_bytes(&object, text, code, sizeof code) ||
        r6_coff_add_symbol(&object, "main", text, 0, 0x20, 2) < 0) {
        fprintf(stderr, "r6: %s\n", r6_coff_error(&object));
        r6_coff_free(&object);
        return 1;
    }
    out = fopen(argv[1], "wb");
    if (!out) {
        perror(argv[1]);
        r6_coff_free(&object);
        return 1;
    }
    ok = r6_elf_write(&object, out);
    if (fclose(out) != 0) ok = false;
    if (!ok) fprintf(stderr, "r6: %s\n", r6_coff_error(&object));
    r6_coff_free(&object);
    return ok ? 0 : 1;
}
