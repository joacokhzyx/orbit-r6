#include "../../src/codegen-x64/coff.h"

#include <stdio.h>

int main(int argc, char **argv) {
    static const unsigned char code[] = {
        0xe8, 0x00, 0x00, 0x00, 0x00, 0xc3, /* call rel32; ret */
        0x31, 0xc0, 0xc3                    /* orbit_probe: xor eax,eax; ret */
    };
    R6CoffObject object;
    FILE *out;
    int text;
    bool ok;
    if (argc != 2) {
        fprintf(stderr, "usage: coff-probe <output.o>\n");
        return 2;
    }
    r6_coff_init(&object);
    text = r6_coff_add_section(&object, ".text", 0x60500020u);
    {
        int main_symbol, probe_symbol;
        ok = text >= 0 && r6_coff_section_bytes(&object, text, code, sizeof code);
        main_symbol = ok ? r6_coff_add_symbol(&object, "main", text, 0, 0x20, 2) : -1;
        probe_symbol = ok ? r6_coff_add_symbol(&object, "orbit_probe", text, 6, 0x20, 2) : -1;
        ok = ok && main_symbol >= 0 && probe_symbol >= 0 &&
             r6_coff_add_relocation(&object, text, 1, probe_symbol, 0x0004);
    }
    out = ok ? fopen(argv[1], "wb") : NULL;
    if (!out || !r6_coff_write(&object, out)) {
        fprintf(stderr, "coff-probe: %s\n", r6_coff_error(&object));
        if (out) fclose(out);
        r6_coff_free(&object);
        return 1;
    }
    if (fclose(out) != 0) {
        fprintf(stderr, "coff-probe: failed closing output\n");
        r6_coff_free(&object);
        return 1;
    }
    r6_coff_free(&object);
    return 0;
}
