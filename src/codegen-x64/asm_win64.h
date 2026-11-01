#ifndef R6_ASM_WIN64_H
#define R6_ASM_WIN64_H

#include <stddef.h>

#include "coff.h"

bool r6_assemble_win64(const char *text, size_t size, R6CoffObject *object,
                       char *error, size_t error_capacity);

#endif
