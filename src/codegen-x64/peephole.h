#ifndef R6_PEEPHOLE_H
#define R6_PEEPHOLE_H

#include <stdbool.h>
#include <stddef.h>

#include "text.h"

/* Rewrites the private asm text in `t` in place (D-M1-67). On failure it
 * returns false and fills `error`; `t` still owns its buffer (D-M1-134). */
bool r6_peephole_win64(TextBuf *t, char *error, size_t error_capacity);

/* The pass over the text after regalloc.c: moves through scratch registers,
 * compares and immediates folded, dead jumps removed (D-M1-83). */
bool r6_peephole_final(TextBuf *t, char *error, size_t error_capacity);

#endif
