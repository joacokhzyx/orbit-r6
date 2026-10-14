#ifndef R6_COFF_H
#define R6_COFF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    char *name;
    uint8_t *data;
    size_t size;
    size_t capacity;
    uint32_t characteristics;
} R6CoffSection;

typedef struct {
    char *name;
    uint32_t value;
    int16_t section;
    uint16_t type;
    uint8_t storage_class;
    uint8_t aux_count;
} R6CoffSymbol;

typedef struct {
    uint32_t offset;
    uint32_t symbol;
    uint16_t type;
} R6CoffRelocation;

typedef struct {
    uint16_t section;
    R6CoffRelocation relocation;
} R6CoffRelocEntry;

typedef struct {
    R6CoffSection *sections;
    size_t section_count;
    size_t section_capacity;
    R6CoffSymbol *symbols;
    size_t symbol_count;
    size_t symbol_capacity;
    R6CoffRelocEntry *relocations;
    size_t relocation_count;
    size_t relocation_capacity;
    char error[192];
} R6CoffObject;

void r6_coff_init(R6CoffObject *object);
void r6_coff_free(R6CoffObject *object);
int r6_coff_add_section(R6CoffObject *object, const char *name,
                        uint32_t characteristics);
bool r6_coff_section_bytes(R6CoffObject *object, int section,
                           const void *bytes, size_t size);
bool r6_coff_section_u32(R6CoffObject *object, int section, uint32_t value);
int r6_coff_add_symbol(R6CoffObject *object, const char *name, int section,
                       uint32_t value, uint16_t type, uint8_t storage_class);
bool r6_coff_add_relocation(R6CoffObject *object, int section, uint32_t offset,
                            int symbol, uint16_t type);
bool r6_coff_write(const R6CoffObject *object, FILE *out);
const char *r6_coff_error(const R6CoffObject *object);

#endif
