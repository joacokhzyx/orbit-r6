#include "../../src/codegen-x64/coff.h"

#include <stdio.h>

int main(int argc, char **argv) {
    static const unsigned char code[] = {
        0xe8, 0x00, 0x00, 0x00, 0x00, 0xc3, /* call rel32; ret */
        0x31, 0xc0, 0xc3                    /* orbit_probe: xor eax,eax; ret */
    };
    static const unsigned char unwind[] = {1, 0, 0, 0, 1, 0, 0, 0};
    R6CoffObject object;
    FILE *out;
    int text, xdata, pdata;
    bool ok;
    if (argc != 2) {
        fprintf(stderr, "usage: coff-probe <output.o>\n");
        return 2;
    }
    r6_coff_init(&object);
    text = r6_coff_add_section(&object, ".text", 0x60500020u);
    xdata = r6_coff_add_section(&object, ".xdata", 0x40300040u);
    pdata = r6_coff_add_section(&object, ".pdata", 0x40300040u);
    {
        int main_symbol, probe_symbol, probe_end, uw_main, uw_probe;
        size_t i;
        ok = text >= 0 && xdata >= 0 && pdata >= 0 &&
             r6_coff_section_bytes(&object, text, code, sizeof code) &&
             r6_coff_section_bytes(&object, xdata, unwind, sizeof unwind);
        main_symbol = ok ? r6_coff_add_symbol(&object, "main", text, 0, 0x20, 2) : -1;
        probe_symbol = ok ? r6_coff_add_symbol(&object, "orbit_probe", text, 6, 0x20, 2) : -1;
        probe_end = ok ? r6_coff_add_symbol(&object, "probe_end", text, 9, 0, 3) : -1;
        uw_main = ok ? r6_coff_add_symbol(&object, "unwind_main", xdata, 0, 0, 3) : -1;
        uw_probe = ok ? r6_coff_add_symbol(&object, "unwind_probe", xdata, 4, 0, 3) : -1;
        ok = ok && main_symbol >= 0 && probe_symbol >= 0 && probe_end >= 0 &&
             uw_main >= 0 && uw_probe >= 0 &&
             r6_coff_add_relocation(&object, text, 1, probe_symbol, 0x0004);
        for (i = 0; ok && i < 6; ++i)
            ok = r6_coff_section_u32(&object, pdata, 0);
        ok = ok && r6_coff_add_relocation(&object, pdata, 0, main_symbol, 0x0003) &&
             r6_coff_add_relocation(&object, pdata, 4, probe_symbol, 0x0003) &&
             r6_coff_add_relocation(&object, pdata, 8, uw_main, 0x0003) &&
             r6_coff_add_relocation(&object, pdata, 12, probe_symbol, 0x0003) &&
             r6_coff_add_relocation(&object, pdata, 16, probe_end, 0x0003) &&
             r6_coff_add_relocation(&object, pdata, 20, uw_probe, 0x0003);
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
