#ifndef R6_REGALLOC_H
#define R6_REGALLOC_H

#include <stdbool.h>
#include <stddef.h>

#include "text.h"

/* Moves the hottest frame slots of every function of the private asm text in
 * `t` into callee-saved registers, rewriting the buffer in place (D-M1-68).
 * On failure it returns false and fills `error`; the text is then unusable. */
bool r6_regalloc_win64(TextBuf *t, char *error, size_t error_capacity);

#endif
