#ifndef R6_REGALLOC_H
#define R6_REGALLOC_H

#include <stdbool.h>
#include <stddef.h>

/* Moves the hottest frame slots of every function of the private asm text in
 * `path` into callee-saved registers, rewriting the file in place (D-M1-68).
 * On failure it returns false and fills `error`; the file is then unusable. */
bool r6_regalloc_win64(const char *path, char *error, size_t error_capacity);

#endif
