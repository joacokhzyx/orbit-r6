#ifndef R6_PEEPHOLE_H
#define R6_PEEPHOLE_H

#include <stdbool.h>
#include <stddef.h>

/* Rewrites the private asm text in `path` in place (D-M1-67). On failure it
 * returns false and fills `error`; the file is then unusable. */
bool r6_peephole_win64(const char *path, char *error, size_t error_capacity);

#endif
