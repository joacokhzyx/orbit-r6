#ifndef R6_ELF64_H
#define R6_ELF64_H

#include "coff.h"

/* Emits the object model as an ELF64 ET_REL for x86-64. */
bool r6_elf_write(const R6CoffObject *object, FILE *out);

#endif
