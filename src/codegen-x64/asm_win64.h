#ifndef R6_ASM_WIN64_H
#define R6_ASM_WIN64_H

#include "coff.h"

bool r6_assemble_win64(FILE *input, R6CoffObject *object, char *error,
                       size_t error_capacity);

#endif
