#include "coff.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static bool set_error(R6CoffObject *o, const char *message) {
    if (o) snprintf(o->error, sizeof o->error, "%s", message);
    return false;
}

static char *copy_string(const char *s) {
    size_t n;
    char *p;
    if (!s) return NULL;
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static bool grow(void **p, size_t *capacity, size_t needed, size_t item_size) {
    size_t next = *capacity ? *capacity : 8;
    void *q;
    if (needed <= *capacity) return true;
    while (next < needed) {
        if (next > SIZE_MAX / 2) return false;
        next *= 2;
    }
    if (item_size && next > SIZE_MAX / item_size) return false;
    q = realloc(*p, next * item_size);
    if (!q) return false;
    *p = q;
    *capacity = next;
    return true;
}

void r6_coff_init(R6CoffObject *o) {
    if (o) memset(o, 0, sizeof *o);
}

void r6_coff_free(R6CoffObject *o) {
    size_t i;
    if (!o) return;
    for (i = 0; i < o->section_count; ++i) {
        free(o->sections[i].name);
        free(o->sections[i].data);
    }
    for (i = 0; i < o->symbol_count; ++i) free(o->symbols[i].name);
    free(o->sections);
    free(o->symbols);
    free(o->relocations);
    memset(o, 0, sizeof *o);
}

int r6_coff_add_section(R6CoffObject *o, const char *name,
                        uint32_t characteristics) {
    R6CoffSection *s;
    if (!o || !name || !*name || strlen(name) > 8)
        return o ? (set_error(o, "COFF section names must contain 1..8 bytes"), -1) : -1;
    if (o->section_count >= INT16_MAX ||
        !grow((void **)&o->sections, &o->section_capacity,
              o->section_count + 1, sizeof *o->sections)) {
        set_error(o, "COFF section table allocation/limit exceeded");
        return -1;
    }
    s = &o->sections[o->section_count];
    memset(s, 0, sizeof *s);
    s->name = copy_string(name);
    if (!s->name) {
        set_error(o, "out of memory copying COFF section name");
        return -1;
    }
    s->characteristics = characteristics;
    return (int)o->section_count++;
}

bool r6_coff_section_bytes(R6CoffObject *o, int section,
                           const void *bytes, size_t size) {
    R6CoffSection *s;
    if (!o || section < 0 || (size && !bytes) || (size_t)section >= o->section_count)
        return o ? set_error(o, "invalid COFF section/data range") : false;
    s = &o->sections[section];
    if (size > UINT32_MAX - s->size ||
        !grow((void **)&s->data, &s->capacity, s->size + size, 1))
        return set_error(o, "COFF section size limit or allocation exceeded");
    if (size) memcpy(s->data + s->size, bytes, size);
    s->size += size;
    return true;
}

bool r6_coff_section_u32(R6CoffObject *o, int section, uint32_t value) {
    uint8_t b[4] = {(uint8_t)value, (uint8_t)(value >> 8),
                    (uint8_t)(value >> 16), (uint8_t)(value >> 24)};
    return r6_coff_section_bytes(o, section, b, sizeof b);
}

int r6_coff_add_symbol(R6CoffObject *o, const char *name, int section,
                       uint32_t value, uint16_t type, uint8_t storage_class) {
    R6CoffSymbol *s;
    if (!o || !name || !*name || section < -1 || section >= (int)o->section_count) {
        if (o) set_error(o, "invalid COFF symbol name/section");
        return -1;
    }
    if (o->symbol_count >= INT_MAX ||
        !grow((void **)&o->symbols, &o->symbol_capacity,
              o->symbol_count + 1, sizeof *o->symbols)) {
        set_error(o, "COFF symbol table allocation/limit exceeded");
        return -1;
    }
    s = &o->symbols[o->symbol_count];
    memset(s, 0, sizeof *s);
    s->name = copy_string(name);
    if (!s->name) {
        set_error(o, "out of memory copying COFF symbol name");
        return -1;
    }
    s->section = (int16_t)(section + 1);
    s->value = value;
    s->type = type;
    s->storage_class = storage_class;
    return (int)o->symbol_count++;
}

bool r6_coff_add_relocation(R6CoffObject *o, int section, uint32_t offset,
                            int symbol, uint16_t type) {
    R6CoffRelocEntry *r;
    if (!o || section < 0 || (size_t)section >= o->section_count ||
        symbol < 0 || (size_t)symbol >= o->symbol_count ||
        offset > o->sections[section].size || o->sections[section].size - offset < 4)
        return o ? set_error(o, "invalid COFF relocation target or offset") : false;
    if (!grow((void **)&o->relocations, &o->relocation_capacity,
              o->relocation_count + 1, sizeof *o->relocations))
        return set_error(o, "out of memory growing COFF relocation table");
    r = &o->relocations[o->relocation_count++];
    r->section = (uint16_t)section;
    r->relocation.offset = offset;
    r->relocation.symbol = (uint32_t)symbol;
    r->relocation.type = type;
    return true;
}

static bool write_bytes(FILE *f, const void *p, size_t n) {
    return n == 0 || fwrite(p, 1, n, f) == n;
}

static bool write_u16(FILE *f, uint16_t v) {
    uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)};
    return write_bytes(f, b, sizeof b);
}

static bool write_u32(FILE *f, uint32_t v) {
    uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8),
                    (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
    return write_bytes(f, b, sizeof b);
}

static uint32_t alignment(uint32_t value, uint32_t align) {
    return (value + align - 1) & ~(align - 1);
}

static bool write_name(FILE *f, const char *name, uint32_t string_offset) {
    uint8_t field[8] = {0};
    size_t n = strlen(name);
    if (n <= 8) memcpy(field, name, n);
    else {
        field[4] = (uint8_t)string_offset;
        field[5] = (uint8_t)(string_offset >> 8);
        field[6] = (uint8_t)(string_offset >> 16);
        field[7] = (uint8_t)(string_offset >> 24);
    }
    return write_bytes(f, field, sizeof field);
}

bool r6_coff_write(const R6CoffObject *o, FILE *out) {
    uint32_t *sec_raw = NULL, *sec_reloc = NULL, *sym_name_offsets = NULL;
    uint32_t *sec_name_offsets = NULL;
    size_t *rel_order = NULL, *rel_start = NULL, *rel_next = NULL;
    uint64_t cursor, sym_count = 0;
    uint32_t string_size = 4, sym_ptr;
    size_t i, j;
    bool ok = false;
    if (!o || !out || o->section_count > UINT16_MAX || o->symbol_count > UINT32_MAX)
        return o ? set_error((R6CoffObject *)o, "invalid COFF output or header limits") : false;
    sec_raw = (uint32_t *)calloc(o->section_count ? o->section_count : 1, sizeof *sec_raw);
    sec_reloc = (uint32_t *)calloc(o->section_count ? o->section_count : 1, sizeof *sec_reloc);
    sym_name_offsets = (uint32_t *)calloc(o->symbol_count ? o->symbol_count : 1, sizeof *sym_name_offsets);
    sec_name_offsets = (uint32_t *)calloc(o->section_count ? o->section_count : 1, sizeof *sec_name_offsets);
    rel_order = (size_t *)malloc((o->relocation_count ? o->relocation_count : 1) * sizeof *rel_order);
    rel_start = (size_t *)calloc(o->section_count + 1, sizeof *rel_start);
    rel_next = (size_t *)calloc(o->section_count + 1, sizeof *rel_next);
    if (!sec_raw || !sec_reloc || !sym_name_offsets || !sec_name_offsets ||
        !rel_order || !rel_start || !rel_next) goto oom;
    /* One counting sort replaces a scan of every relocation per section:
     * rel_order lists the relocations of section i, in input order, at
     * [rel_start[i], rel_start[i + 1]). */
    for (j = 0; j < o->relocation_count; ++j) {
        if (o->relocations[j].section >= o->section_count) {
            set_error((R6CoffObject *)o, "COFF relocation names a missing section");
            goto done;
        }
        ++rel_start[o->relocations[j].section + 1];
    }
    for (i = 0; i < o->section_count; ++i) rel_start[i + 1] += rel_start[i];
    for (i = 0; i < o->section_count; ++i) sec_reloc[i] = (uint32_t)(rel_start[i + 1] - rel_start[i]);
    for (i = 0; i < o->section_count; ++i) {
        if (strlen(o->sections[i].name) > 8) {
            sec_name_offsets[i] = string_size;
            string_size += (uint32_t)strlen(o->sections[i].name) + 1;
        }
        if (sec_reloc[i] > UINT16_MAX) {
            set_error((R6CoffObject *)o, "COFF relocation overflow is unsupported");
            goto done;
        }
    }
    memcpy(rel_next, rel_start, (o->section_count + 1) * sizeof *rel_next);
    for (j = 0; j < o->relocation_count; ++j)
        rel_order[rel_next[o->relocations[j].section]++] = j;
    for (i = 0; i < o->symbol_count; ++i) {
        if (o->symbols[i].aux_count) {
            set_error((R6CoffObject *)o, "COFF auxiliary symbols are not yet supported");
            goto done;
        }
        if (strlen(o->symbols[i].name) > 8) {
            if (strlen(o->symbols[i].name) > UINT32_MAX - string_size - 1) {
                set_error((R6CoffObject *)o, "COFF string table exceeds 4 GiB");
                goto done;
            }
            sym_name_offsets[i] = string_size;
            string_size += (uint32_t)strlen(o->symbols[i].name) + 1;
        }
        sym_count += 1u + o->symbols[i].aux_count;
    }
    if (sym_count > UINT32_MAX) {
        set_error((R6CoffObject *)o, "COFF symbol count exceeds header limit");
        goto done;
    }
    cursor = 20u + (uint64_t)o->section_count * 40u;
    for (i = 0; i < o->section_count; ++i) {
        uint32_t count;
        cursor = alignment((uint32_t)cursor, 16);
        if (o->sections[i].size) sec_raw[i] = (uint32_t)cursor;
        cursor += o->sections[i].size;
        cursor = alignment((uint32_t)cursor, 4);
        count = sec_reloc[i];
        if (count) sec_reloc[i] = (uint32_t)cursor;
        cursor += (uint64_t)count * 10u;
        if (cursor > UINT32_MAX) {
            set_error((R6CoffObject *)o, "COFF object exceeds 4 GiB file offsets");
            goto done;
        }
    }
    sym_ptr = (uint32_t)cursor;
    if (sym_ptr > UINT32_MAX - sym_count * 18u - string_size) {
        set_error((R6CoffObject *)o, "COFF symbol/string table exceeds 4 GiB");
        goto done;
    }
    if (!write_u16(out, 0x8664) || !write_u16(out, (uint16_t)o->section_count) ||
        !write_u32(out, 0) || !write_u32(out, sym_ptr) ||
        !write_u32(out, (uint32_t)sym_count) || !write_u16(out, 0) ||
        !write_u16(out, 0)) goto io;
    cursor = 20u + (uint64_t)o->section_count * 40u;
    for (i = 0; i < o->section_count; ++i) {
        size_t rc = rel_start[i + 1] - rel_start[i];
        uint32_t ch = o->sections[i].characteristics;
        if (!write_name(out, o->sections[i].name, sec_name_offsets[i]) ||
            !write_u32(out, 0) || !write_u32(out, 0) ||
            !write_u32(out, (uint32_t)o->sections[i].size) ||
            !write_u32(out, sec_raw[i]) || !write_u32(out, sec_reloc[i]) ||
            !write_u32(out, 0) || !write_u16(out, (uint16_t)rc) ||
            !write_u16(out, 0) || !write_u32(out, ch)) goto io;
    }
    cursor = 20u + (uint64_t)o->section_count * 40u;
    for (i = 0; i < o->section_count; ++i) {
        size_t rc = rel_start[i + 1] - rel_start[i], k;
        if (sec_raw[i]) {
            while (cursor < sec_raw[i]) { if (fputc(0, out) == EOF) goto io; ++cursor; }
        }
        if (sec_raw[i] && !write_bytes(out, o->sections[i].data, o->sections[i].size)) goto io;
        if (sec_raw[i]) cursor += o->sections[i].size;
        if (rc) {
            while (cursor < sec_reloc[i]) { if (fputc(0, out) == EOF) goto io; ++cursor; }
            for (k = rel_start[i]; k < rel_start[i + 1]; ++k) {
                const R6CoffRelocation *r = &o->relocations[rel_order[k]].relocation;
                if (!write_u32(out, r->offset) || !write_u32(out, r->symbol) ||
                    !write_u16(out, r->type)) goto io;
                cursor += 10;
            }
        }
    }
    while (cursor < sym_ptr) { if (fputc(0, out) == EOF) goto io; ++cursor; }
    for (i = 0; i < o->symbol_count; ++i) {
        const R6CoffSymbol *s = &o->symbols[i];
        if (!write_name(out, s->name, sym_name_offsets[i]) ||
            !write_u32(out, s->value) || !write_u16(out, (uint16_t)s->section) ||
            !write_u16(out, s->type) || fputc(s->storage_class, out) == EOF ||
            fputc(s->aux_count, out) == EOF) goto io;
        for (j = 0; j < s->aux_count; ++j) {
            uint8_t zero[18] = {0};
            if (!write_bytes(out, zero, sizeof zero)) goto io;
        }
    }
    if (!write_u32(out, string_size)) goto io;
    for (i = 0; i < o->section_count; ++i) if (sec_name_offsets[i]) {
        const char *p = o->sections[i].name;
        if (!write_bytes(out, p, strlen(p) + 1)) goto io;
    }
    for (i = 0; i < o->symbol_count; ++i) if (sym_name_offsets[i]) {
        const char *p = o->symbols[i].name;
        if (!write_bytes(out, p, strlen(p) + 1)) goto io;
    }
    ok = true;
    goto done;
oom:
    set_error((R6CoffObject *)o, "out of memory laying out COFF object");
    goto done;
io:
    set_error((R6CoffObject *)o, "I/O error writing COFF object");
done:
    free(sec_raw); free(sec_reloc); free(sym_name_offsets); free(sec_name_offsets);
    free(rel_order); free(rel_start); free(rel_next);
    return ok;
}

const char *r6_coff_error(const R6CoffObject *o) {
    return o && o->error[0] ? o->error : "COFF operation failed";
}
