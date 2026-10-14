#ifndef R6_CODEGEN_WIN64_H
#define R6_CODEGEN_WIN64_H

#include "../loader/ir.h"

#include <stdio.h>

/* Emit GNU assembler x86-64 Windows ABI text. Returns false and fills diag
 * for every unsupported operation or invalid codegen-level combination. */
bool r6_codegen_win64(FILE *out, const R6Module *module, R6Diagnostic *diag);

#endif
