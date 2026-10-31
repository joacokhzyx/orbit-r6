#include "elf64.h"

#include <stdlib.h>
#include <string.h>

#define ELF_HEADER_SIZE 64u
#define ELF_SHDR_SIZE 64u
#define ELF_SYM_SIZE 24u
#define ELF_RELA_SIZE 24u

#define ELF_SHT_PROGBITS 1u
#define ELF_SHT_SYMTAB 2u
#define ELF_SHT_STRTAB 3u
#define ELF_SHT_RELA 4u

#define ELF_SHF_WRITE 0x1u
#define ELF_SHF_ALLOC 0x2u
#define ELF_SHF_EXECINSTR 0x4u

#define ELF_STB_LOCAL 0u
#define ELF_STB_GLOBAL 1u

#define ELF_STT_NOTYPE 0u
#define ELF_STT_OBJECT 1u
#define ELF_STT_FUNC 2u

#define ELF_R_X86_64_PC32 2u
#define ELF_R_X86_64_PLT32 4u
#define ELF_R_X86_64_32S 11u

typedef struct {
    uint32_t name;
    uint32_t type;
    uint64_t flags;
    uint64_t offset;
    uint64_t size;
    uint32_t link;
    uint32_t info;
    uint64_t align;
    uint64_t entsize;
} ElfShdr;

static bool set_error(R6CoffObject *o, const char *message) {
    if (o) snprintf(o->error, sizeof o->error, "%s", message);
    return false;
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

static bool write_u64(FILE *f, uint64_t v) {
    uint8_t b[8] = {(uint8_t)v,         (uint8_t)(v >> 8),  (uint8_t)(v >> 16),
                    (uint8_t)(v >> 24), (uint8_t)(v >> 32), (uint8_t)(v >> 40),
                    (uint8_t)(v >> 48), (uint8_t)(v >> 56)};
    return write_bytes(f, b, sizeof b);
}

static bool advance(uint64_t *cursor, uint64_t amount) {
    if (*cursor > UINT64_MAX - amount) return false;
    *cursor += amount;
    return true;
}

static bool align_cursor(uint64_t *cursor, uint64_t align) {
    uint64_t rest = *cursor % align;
    return rest == 0 || advance(cursor, align - rest);
}

static bool pad_to(FILE *f, uint64_t *pos, uint64_t target) {
    if (target < *pos) return false;
    while (*pos < target) {
        if (fputc(0, f) == EOF) return false;
        ++*pos;
    }
    return true;
}

static bool add_name_size(uint32_t *size, const char *name) {
    size_t n = strlen(name) + 1;
    if (n > UINT32_MAX - *size) return false;
    *size += (uint32_t)n;
    return true;
}

static bool write_symbol(FILE *f, uint32_t name, uint8_t info, uint16_t shndx,
                         uint64_t value) {
    return write_u32(f, name) && fputc(info, f) != EOF &&
           fputc(0, f) != EOF && write_u16(f, shndx) &&
           write_u64(f, value) && write_u64(f, 0);
}

static bool write_symbol_pass(FILE *f, const R6CoffObject *o,
                              const uint32_t *sym_name,
                              const uint32_t *obj_to_elf, bool global) {
    size_t i;
    for (i = 0; i < o->symbol_count; ++i) {
        const R6CoffSymbol *s = &o->symbols[i];
        uint8_t bind, type;
        uint16_t shndx = 0;
        if ((s->storage_class == 2) != global) continue;
        bind = global ? ELF_STB_GLOBAL : ELF_STB_LOCAL;
        if (s->section == 0) {
            type = s->type == 0x20 ? ELF_STT_FUNC : ELF_STT_NOTYPE;
        } else {
            type = s->type == 0x20 ? ELF_STT_FUNC : ELF_STT_OBJECT;
            shndx = (uint16_t)obj_to_elf[s->section - 1];
        }
        if (!write_symbol(f, sym_name[i], (uint8_t)((bind << 4) | type), shndx,
                          s->value))
            return false;
    }
    return true;
}

bool r6_elf_write(const R6CoffObject *object, FILE *out) {
    R6CoffObject *o = (R6CoffObject *)object;
    ElfShdr *sh = NULL;
    uint32_t *obj_to_elf = NULL;
    int *section_of = NULL;
    size_t *rel_start = NULL, *rel_next = NULL, *rel_order = NULL;
    uint32_t *sym_index = NULL, *sym_name = NULL;
    char *strtab = NULL, *shstrtab = NULL;
    uint32_t data_count = 0, rela_count = 0, local_count = 0;
    uint32_t total_sections, note_index, rela_base;
    uint32_t symtab_index, strtab_index, shstrtab_index;
    uint32_t str_size = 1, shstr_size, name_at;
    uint64_t cursor, shoff, pos;
    size_t i, j;
    bool ok = false;

    if (!o || !out) return o ? set_error(o, "invalid ELF output handle") : false;
    if (o->section_count > UINT32_MAX - 5 ||
        o->symbol_count >= UINT32_MAX)
        return set_error(o, "ELF section or symbol count exceeds header limits");

    for (i = 0; i < o->section_count; ++i) {
        if (!o->sections[i].name || !*o->sections[i].name)
            return set_error(o, "ELF section has no name");
        if (o->sections[i].size) ++data_count;
    }

    rel_start = (size_t *)calloc(o->section_count + 1, sizeof *rel_start);
    rel_next = (size_t *)malloc((o->section_count + 1) * sizeof *rel_next);
    if (!rel_start || !rel_next) goto oom;
    for (j = 0; j < o->relocation_count; ++j) {
        const R6CoffRelocEntry *e = &o->relocations[j];
        if (e->section >= o->section_count) {
            set_error(o, "ELF relocation names a missing section");
            goto done;
        }
        if (e->relocation.symbol >= o->symbol_count) {
            set_error(o, "ELF relocation names a missing symbol");
            goto done;
        }
        if (e->relocation.type != 4 && e->relocation.type != 3) {
            set_error(o, "ELF relocation type is not 3 or 4");
            goto done;
        }
        if (e->relocation.offset > o->sections[e->section].size ||
            o->sections[e->section].size - e->relocation.offset < 4) {
            set_error(o, "ELF relocation offset exceeds its section");
            goto done;
        }
        ++rel_start[e->section + 1];
    }
    for (i = 0; i < o->section_count; ++i) rel_start[i + 1] += rel_start[i];
    for (i = 0; i < o->section_count; ++i) {
        size_t count = rel_start[i + 1] - rel_start[i];
        if (!count) continue;
        if (!o->sections[i].size) {
            set_error(o, "ELF relocation names an empty section");
            goto done;
        }
        if ((uint64_t)count > UINT64_MAX / ELF_RELA_SIZE) goto overflow;
        ++rela_count;
    }

    if ((uint64_t)data_count + rela_count + 5 > UINT16_MAX) {
        set_error(o, "ELF section count exceeds the 16-bit header field");
        goto done;
    }
    total_sections = 1 + data_count + 1 + rela_count + 3;
    note_index = 1 + data_count;
    rela_base = note_index + 1;
    symtab_index = rela_base + rela_count;
    strtab_index = symtab_index + 1;
    shstrtab_index = strtab_index + 1;

    sh = (ElfShdr *)calloc(total_sections, sizeof *sh);
    obj_to_elf = (uint32_t *)malloc((o->section_count ? o->section_count : 1) *
                                    sizeof *obj_to_elf);
    section_of = (int *)malloc(total_sections * sizeof *section_of);
    rel_order = (size_t *)malloc((o->relocation_count ? o->relocation_count : 1) *
                                 sizeof *rel_order);
    sym_index = (uint32_t *)malloc((o->symbol_count ? o->symbol_count : 1) *
                                   sizeof *sym_index);
    sym_name = (uint32_t *)malloc((o->symbol_count ? o->symbol_count : 1) *
                                  sizeof *sym_name);
    if (!sh || !obj_to_elf || !section_of || !rel_order || !sym_index ||
        !sym_name)
        goto oom;

    for (i = 0; i < o->section_count; ++i) obj_to_elf[i] = UINT32_MAX;
    for (i = 0; i < total_sections; ++i) section_of[i] = -1;

    {
        uint32_t elf = 1;
        for (i = 0; i < o->section_count; ++i) {
            const R6CoffSection *s = &o->sections[i];
            ElfShdr *d;
            if (!s->size) continue;
            obj_to_elf[i] = elf;
            section_of[elf] = (int)i;
            d = &sh[elf];
            d->type = ELF_SHT_PROGBITS;
            d->flags = ELF_SHF_ALLOC;
            d->align = 16;
            d->size = (uint64_t)s->size;
            if (strcmp(s->name, ".text") == 0) d->flags |= ELF_SHF_EXECINSTR;
            else if (strcmp(s->name, ".data") == 0) d->flags |= ELF_SHF_WRITE;
            ++elf;
        }
    }

    sh[note_index].type = ELF_SHT_PROGBITS;
    sh[note_index].flags = 0;
    sh[note_index].size = 0;
    sh[note_index].align = 0;

    {
        uint32_t at = rela_base;
        for (i = 0; i < o->section_count; ++i) {
            size_t count = rel_start[i + 1] - rel_start[i];
            ElfShdr *d;
            if (!count) continue;
            section_of[at] = (int)i;
            d = &sh[at];
            d->type = ELF_SHT_RELA;
            d->flags = 0;
            d->link = symtab_index;
            d->info = obj_to_elf[i];
            d->align = 8;
            d->entsize = ELF_RELA_SIZE;
            d->size = (uint64_t)count * ELF_RELA_SIZE;
            ++at;
        }
    }

    for (i = 0; i < o->symbol_count; ++i) {
        const R6CoffSymbol *s = &o->symbols[i];
        if (!s->name || !*s->name) {
            set_error(o, "ELF symbol has no name");
            goto done;
        }
        if (s->section < 0 || (size_t)s->section > o->section_count) {
            set_error(o, "ELF symbol names a missing section");
            goto done;
        }
        if (s->section > 0 && obj_to_elf[s->section - 1] == UINT32_MAX) {
            set_error(o, "ELF symbol names an empty section");
            goto done;
        }
        if (s->storage_class != 2) ++local_count;
        sym_name[i] = str_size;
        if (!add_name_size(&str_size, s->name)) {
            set_error(o, "ELF string table exceeds 4 GiB");
            goto done;
        }
    }
    {
        uint32_t next_local = 1, next_global = 1 + local_count;
        for (i = 0; i < o->symbol_count; ++i)
            sym_index[i] = o->symbols[i].storage_class == 2 ? next_global++
                                                           : next_local++;
    }
    sh[symtab_index].type = ELF_SHT_SYMTAB;
    sh[symtab_index].link = strtab_index;
    sh[symtab_index].info = 1 + local_count;
    sh[symtab_index].align = 8;
    sh[symtab_index].entsize = ELF_SYM_SIZE;
    sh[symtab_index].size = ((uint64_t)o->symbol_count + 1) * ELF_SYM_SIZE;

    sh[strtab_index].type = ELF_SHT_STRTAB;
    sh[strtab_index].align = 1;
    sh[strtab_index].size = str_size;
    sh[shstrtab_index].type = ELF_SHT_STRTAB;
    sh[shstrtab_index].align = 1;

    shstr_size = 1;
    for (i = 0; i < o->section_count; ++i)
        if (o->sections[i].size &&
            !add_name_size(&shstr_size, o->sections[i].name)) {
            set_error(o, "ELF section string table exceeds 4 GiB");
            goto done;
        }
    if (!add_name_size(&shstr_size, ".note.GNU-stack") ||
        !add_name_size(&shstr_size, ".symtab") ||
        !add_name_size(&shstr_size, ".strtab") ||
        !add_name_size(&shstr_size, ".shstrtab")) {
        set_error(o, "ELF section string table exceeds 4 GiB");
        goto done;
    }
    for (i = 0; i < o->section_count; ++i) {
        if (!(rel_start[i + 1] - rel_start[i])) continue;
        if (strlen(o->sections[i].name) + 6 > UINT32_MAX - shstr_size) {
            set_error(o, "ELF section string table exceeds 4 GiB");
            goto done;
        }
        shstr_size += (uint32_t)strlen(o->sections[i].name) + 6;
    }
    sh[shstrtab_index].size = shstr_size;

    strtab = (char *)malloc(str_size);
    shstrtab = (char *)malloc(shstr_size);
    if (!strtab || !shstrtab) goto oom;
    strtab[0] = 0;
    shstrtab[0] = 0;
    for (i = 0; i < o->symbol_count; ++i) {
        const char *name = o->symbols[i].name;
        memcpy(strtab + sym_name[i], name, strlen(name) + 1);
    }

    name_at = 1;
    for (i = 1; i < note_index; ++i) {
        const char *name = o->sections[section_of[i]].name;
        size_t n = strlen(name) + 1;
        sh[i].name = name_at;
        memcpy(shstrtab + name_at, name, n);
        name_at += (uint32_t)n;
    }
    sh[note_index].name = name_at;
    memcpy(shstrtab + name_at, ".note.GNU-stack", sizeof ".note.GNU-stack");
    name_at += (uint32_t)sizeof ".note.GNU-stack";
    {
        uint32_t at = rela_base;
        for (i = 0; i < o->section_count; ++i) {
            size_t n = strlen(o->sections[i].name) + 1;
            if (!(rel_start[i + 1] - rel_start[i])) continue;
            sh[at].name = name_at;
            memcpy(shstrtab + name_at, ".rela", 5);
            memcpy(shstrtab + name_at + 5, o->sections[i].name, n);
            name_at += 5 + (uint32_t)n;
            ++at;
        }
    }
    sh[symtab_index].name = name_at;
    memcpy(shstrtab + name_at, ".symtab", sizeof ".symtab");
    name_at += (uint32_t)sizeof ".symtab";
    sh[strtab_index].name = name_at;
    memcpy(shstrtab + name_at, ".strtab", sizeof ".strtab");
    name_at += (uint32_t)sizeof ".strtab";
    sh[shstrtab_index].name = name_at;
    memcpy(shstrtab + name_at, ".shstrtab", sizeof ".shstrtab");

    memcpy(rel_next, rel_start, (o->section_count + 1) * sizeof *rel_next);
    for (j = 0; j < o->relocation_count; ++j)
        rel_order[rel_next[o->relocations[j].section]++] = j;

    cursor = ELF_HEADER_SIZE;
    for (i = 1; i < symtab_index; ++i) {
        if (!align_cursor(&cursor, sh[i].align ? sh[i].align : 1))
            goto overflow;
        sh[i].offset = cursor;
        if (!advance(&cursor, sh[i].size)) goto overflow;
    }
    if (!align_cursor(&cursor, 8)) goto overflow;
    sh[symtab_index].offset = cursor;
    if (!advance(&cursor, sh[symtab_index].size)) goto overflow;
    sh[strtab_index].offset = cursor;
    if (!advance(&cursor, sh[strtab_index].size)) goto overflow;
    sh[shstrtab_index].offset = cursor;
    if (!advance(&cursor, sh[shstrtab_index].size)) goto overflow;
    if (!align_cursor(&cursor, 8)) goto overflow;
    shoff = cursor;
    if (cursor > UINT64_MAX - (uint64_t)total_sections * ELF_SHDR_SIZE)
        goto overflow;

    {
        static const uint8_t ident[16] = {0x7f, 'E', 'L', 'F', 2, 1, 1, 0,
                                          0, 0, 0, 0, 0, 0, 0, 0};
        if (!write_bytes(out, ident, sizeof ident) ||
            !write_u16(out, 1) || !write_u16(out, 62) ||
            !write_u32(out, 1) || !write_u64(out, 0) || !write_u64(out, 0) ||
            !write_u64(out, shoff) || !write_u32(out, 0) ||
            !write_u16(out, ELF_HEADER_SIZE) || !write_u16(out, 0) ||
            !write_u16(out, 0) || !write_u16(out, ELF_SHDR_SIZE) ||
            !write_u16(out, (uint16_t)total_sections) ||
            !write_u16(out, (uint16_t)shstrtab_index))
            goto io;
        pos = ELF_HEADER_SIZE;
    }
    for (i = 1; i < total_sections; ++i) {
        if (!pad_to(out, &pos, sh[i].offset)) goto io;
        if (!sh[i].size) continue;
        if (i < symtab_index) {
            if (sh[i].type == ELF_SHT_RELA) {
                int owner = section_of[i];
                size_t k;
                for (k = rel_start[owner]; k < rel_start[owner + 1]; ++k) {
                    const R6CoffRelocation *r =
                        &o->relocations[rel_order[k]].relocation;
                    const R6CoffSymbol *target = &o->symbols[r->symbol];
                    uint32_t type =
                        r->type == 4
                            ? (target->section ? ELF_R_X86_64_PC32
                                               : ELF_R_X86_64_PLT32)
                            : ELF_R_X86_64_32S;
                    uint64_t addend = r->type == 4 ? (uint64_t)(int64_t)-4 : 0;
                    uint64_t info = ((uint64_t)sym_index[r->symbol] << 32) | type;
                    if (!write_u64(out, r->offset) || !write_u64(out, info) ||
                        !write_u64(out, addend))
                        goto io;
                }
            } else if (!write_bytes(out, o->sections[section_of[i]].data,
                                    o->sections[section_of[i]].size)) {
                goto io;
            }
        } else if (i == symtab_index) {
            if (!write_symbol(out, 0, 0, 0, 0) ||
                !write_symbol_pass(out, o, sym_name, obj_to_elf, false) ||
                !write_symbol_pass(out, o, sym_name, obj_to_elf, true))
                goto io;
        } else if (i == strtab_index) {
            if (!write_bytes(out, strtab, sh[i].size)) goto io;
        } else {
            if (!write_bytes(out, shstrtab, sh[i].size)) goto io;
        }
        if (!advance(&pos, sh[i].size)) goto io;
    }
    if (!pad_to(out, &pos, shoff)) goto io;
    for (i = 0; i < total_sections; ++i) {
        if (!write_u32(out, sh[i].name) || !write_u32(out, sh[i].type) ||
            !write_u64(out, sh[i].flags) || !write_u64(out, 0) ||
            !write_u64(out, sh[i].offset) || !write_u64(out, sh[i].size) ||
            !write_u32(out, sh[i].link) || !write_u32(out, sh[i].info) ||
            !write_u64(out, sh[i].align) || !write_u64(out, sh[i].entsize))
            goto io;
    }
    ok = true;
    goto done;
oom:
    set_error(o, "out of memory laying out ELF object");
    goto done;
overflow:
    set_error(o, "ELF object exceeds 64-bit file offsets");
    goto done;
io:
    set_error(o, "I/O error writing ELF object");
done:
    free(sh);
    free(obj_to_elf);
    free(section_of);
    free(rel_start);
    free(rel_next);
    free(rel_order);
    free(sym_index);
    free(sym_name);
    free(strtab);
    free(shstrtab);
    return ok;
}
