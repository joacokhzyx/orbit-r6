#ifndef R6_TEXT_H
#define R6_TEXT_H

#include <stdbool.h>
#include <stddef.h>

/* The generated assembler text, from codegen to the assembler (D-M1-134). */
typedef struct {
    char *p;
    size_t n, cap;
    bool oom;
} TextBuf;

bool text_reserve(TextBuf *b, size_t extra);
void text_printf(TextBuf *b, const char *fmt, ...);
void text_puts(TextBuf *b, const char *s);
void text_release(TextBuf *b);

#endif
