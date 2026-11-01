#include "text.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool text_reserve(TextBuf *b, size_t extra) {
    size_t need, cap;
    char *p;
    if (b->oom) return false;
    if (extra > (size_t)-1 - b->n) { b->oom = true; return false; }
    need = b->n + extra;
    if (need <= b->cap) return true;
    cap = b->cap ? b->cap : 65536;
    while (cap < need) {
        if (cap > ((size_t)-1) / 2) { b->oom = true; return false; }
        cap *= 2;
    }
    p = (char *)realloc(b->p, cap);
    if (!p) { b->oom = true; return false; }
    b->p = p;
    b->cap = cap;
    return true;
}

typedef enum { LEN_NONE, LEN_L, LEN_LL, LEN_Z } FmtLen;

static int render_uint(char *dst, size_t cap, uint64_t v, bool neg, unsigned width) {
    char digs[24];
    size_t n = 0, len, pad = 0, i = 0;
    do {
        digs[n++] = (char)('0' + (unsigned)(v % 10u));
        v /= 10u;
    } while (v);
    len = n + (neg ? 1u : 0u);
    if (width > len) pad = (size_t)width - len;
    if (len + pad > cap) return -1;
    if (neg) dst[i++] = '-';
    while (pad--) dst[i++] = '0';
    while (n) dst[i++] = digs[--n];
    return (int)i;
}

static int render_hex(char *dst, size_t cap, uint64_t v, unsigned width) {
    static const char digits[] = "0123456789abcdef";
    char digs[16];
    size_t n = 0, pad = 0, i = 0;
    do {
        digs[n++] = digits[v & 15u];
        v >>= 4;
    } while (v);
    if (width > n) pad = (size_t)width - n;
    if (pad + n > cap) return -1;
    while (pad--) dst[i++] = '0';
    while (n) dst[i++] = digs[--n];
    return (int)i;
}

/* Renders the conversion subset the generator uses (D-M1-133). Returns the
 * length, or -1 for any format the C library must handle. */
static int text_render(char *dst, size_t cap, const char *fmt, va_list ap) {
    size_t at = 0;
    const char *p = fmt;
    while (*p) {
        unsigned width = 0;
        bool zero = false;
        FmtLen len = LEN_NONE;
        int n;
        if (*p != '%') {
            if (at + 1u > cap) return -1;
            dst[at++] = *p++;
            continue;
        }
        ++p;
        if (*p == '%') {
            if (at + 1u > cap) return -1;
            dst[at++] = '%';
            ++p;
            continue;
        }
        if (*p == '0') {
            zero = true;
            ++p;
        }
        while (*p >= '0' && *p <= '9') {
            width = width * 10u + (unsigned)(*p - '0');
            if (width > cap) return -1;
            ++p;
        }
        if (width && !zero) return -1;
        if (*p == 'l') {
            len = LEN_L;
            ++p;
            if (*p == 'l') {
                len = LEN_LL;
                ++p;
            }
        } else if (*p == 'z') {
            len = LEN_Z;
            ++p;
        } else if (*p == 'h' || *p == 'j' || *p == 't') {
            return -1;
        }
        if (*p == 's') {
            const char *s;
            size_t sl;
            if (width || len != LEN_NONE) return -1;
            s = va_arg(ap, const char *);
            if (!s) return -1;
            sl = strlen(s);
            if (sl > cap - at) return -1;
            memcpy(dst + at, s, sl);
            at += sl;
            ++p;
            continue;
        }
        if (*p == 'd' || *p == 'u' || *p == 'x') {
            uint64_t magnitude;
            bool neg = false;
            if (*p == 'd') {
                long long v;
                if (len == LEN_NONE) v = va_arg(ap, int);
                else if (len == LEN_L) v = va_arg(ap, long);
                else if (len == LEN_LL) v = va_arg(ap, long long);
                else return -1;
                neg = v < 0;
                magnitude = neg ? (uint64_t)(-(v + 1)) + 1u : (uint64_t)v;
            } else if (len == LEN_NONE) {
                magnitude = va_arg(ap, unsigned int);
            } else if (len == LEN_L) {
                magnitude = va_arg(ap, unsigned long);
            } else if (len == LEN_LL) {
                magnitude = va_arg(ap, unsigned long long);
            } else {
                magnitude = va_arg(ap, size_t);
            }
            if (*p == 'x')
                n = render_hex(dst + at, cap - at, magnitude, width);
            else
                n = render_uint(dst + at, cap - at, magnitude, neg, width);
            if (n < 0) return -1;
            at += (size_t)n;
            ++p;
            continue;
        }
        return -1;
    }
    return (int)at;
}

void text_printf(TextBuf *b, const char *fmt, ...) {
    char tmp[4096];
    va_list ap, aq, ar;
    int n;
    if (b->oom) return;
    va_start(ap, fmt);
    va_copy(aq, ap);
    va_copy(ar, ap);
    n = text_render(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n >= 0) {
        if (n > 0 && text_reserve(b, (size_t)n)) {
            memcpy(b->p + b->n, tmp, (size_t)n);
            b->n += (size_t)n;
        }
        va_end(aq);
        va_end(ar);
        return;
    }
    n = vsnprintf(tmp, sizeof tmp, fmt, aq);
    va_end(aq);
    if (n < 0) { va_end(ar); b->oom = true; return; }
    if (n == 0) { va_end(ar); return; }
    if ((size_t)n < sizeof tmp) {
        if (text_reserve(b, (size_t)n)) {
            memcpy(b->p + b->n, tmp, (size_t)n);
            b->n += (size_t)n;
        }
        va_end(ar);
        return;
    }
    if (text_reserve(b, (size_t)n + 1u)) {
        int again = vsnprintf(b->p + b->n, (size_t)n + 1u, fmt, ar);
        if (again != n) b->oom = true;
        else b->n += (size_t)n;
    }
    va_end(ar);
}

void text_puts(TextBuf *b, const char *s) {
    size_t n = strlen(s);
    if (!n) return;
    if (text_reserve(b, n)) {
        memcpy(b->p + b->n, s, n);
        b->n += n;
    }
}

void text_release(TextBuf *b) {
    free(b->p);
    b->p = NULL;
    b->n = 0;
    b->cap = 0;
    b->oom = false;
}
