#include "peephole.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* D-M1-67: the generator keeps every IR register in a stack slot, so a value
 * is stored and, on the next line, loaded again:
 *
 *     movq %rax, -24(%rbp)
 *     movq -24(%rbp), %rax        removed, rax already holds the slot
 *     movq -24(%rbp), %r11        becomes movq %rax, %r11
 *
 * The store stays, so every later read of the slot is unchanged. Only
 * adjacent lines match: a label or any other line between them ends the
 * match. Only 64-bit general registers are rewritten, because a 32-bit load
 * clears the upper half and an xmm load clears the upper lane, which a
 * dropped load would not. */

typedef struct {
    char reg[8];
    char slot[24];
} SlotMove;

static bool is_gpr(const char *s, size_t n) {
    size_t k;
    if (n < 3 || n > 5 || s[0] != '%' || s[1] != 'r') return false;
    for (k = 2; k < n; ++k)
        if (!((s[k] >= 'a' && s[k] <= 'z') || (s[k] >= '0' && s[k] <= '9'))) return false;
    return !(n == 4 && (memcmp(s, "%rsp", 4) == 0 || memcmp(s, "%rbp", 4) == 0));
}

/* A frame slot: `-`, digits, `(%rbp)`. */
static bool is_slot(const char *s, size_t n) {
    size_t k;
    if (n < 8 || n >= 24 || s[0] != '-' || memcmp(s + n - 6, "(%rbp)", 6) != 0) return false;
    for (k = 1; k < n - 6; ++k)
        if (s[k] < '0' || s[k] > '9') return false;
    return true;
}

/* Reads `\tmovq A, B` where one side is a 64-bit register and the other a
 * frame slot. *store is true when the register is the source. */
static bool parse_move(const char *line, size_t len, bool *store, SlotMove *out) {
    const char *a, *b, *sep;
    size_t alen, blen;
    if (len < 12 || memcmp(line, "\tmovq ", 6) != 0) return false;
    a = line + 6;
    sep = (const char *)memchr(a, ',', len - 6);
    if (!sep || sep + 2 > line + len || sep[1] != ' ') return false;
    alen = (size_t)(sep - a);
    b = sep + 2;
    blen = (size_t)(line + len - b);
    if (is_gpr(a, alen) && is_slot(b, blen)) {
        memcpy(out->reg, a, alen); out->reg[alen] = 0;
        memcpy(out->slot, b, blen); out->slot[blen] = 0;
        *store = true;
        return true;
    }
    if (is_slot(a, alen) && is_gpr(b, blen)) {
        memcpy(out->slot, a, alen); out->slot[alen] = 0;
        memcpy(out->reg, b, blen); out->reg[blen] = 0;
        *store = false;
        return true;
    }
    return false;
}

bool r6_peephole_win64(const char *path, char *error, size_t error_capacity) {
    FILE *file = fopen(path, "rb");
    char *text = NULL, *result = NULL;
    size_t size, used = 0, at = 0;
    SlotMove stored;
    bool have_store = false;
    long end;
    if (!file || fseek(file, 0, SEEK_END) != 0 || (end = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        snprintf(error, error_capacity, "cannot read generated assembler text");
        if (file) fclose(file);
        return false;
    }
    size = (size_t)end;
    text = (char *)malloc(size + 1);
    /* A rewritten load is shorter than the line it replaces. */
    result = (char *)malloc(size + 1);
    if (!text || !result || fread(text, 1, size, file) != size) {
        snprintf(error, error_capacity, "cannot load generated assembler text");
        fclose(file); free(text); free(result);
        return false;
    }
    fclose(file);
    while (at < size) {
        const char *nl = (const char *)memchr(text + at, '\n', size - at);
        size_t len = nl ? (size_t)(nl - (text + at)) + 1 : size - at;
        size_t body = nl ? len - 1 : len;
        bool store;
        SlotMove move;
        if (parse_move(text + at, body, &store, &move)) {
            if (store) {
                stored = move;
                have_store = true;
            } else if (have_store && strcmp(move.slot, stored.slot) == 0) {
                at += len;
                if (strcmp(move.reg, stored.reg) != 0)
                    used += (size_t)snprintf(result + used, 24, "\tmovq %s, %s\n",
                                             stored.reg, move.reg);
                continue;
            } else {
                have_store = false;
            }
        } else {
            have_store = false;
        }
        memcpy(result + used, text + at, len);
        used += len;
        at += len;
    }
    file = fopen(path, "wb");
    if (!file || fwrite(result, 1, used, file) != used || fclose(file) != 0) {
        snprintf(error, error_capacity, "cannot write rewritten assembler text");
        free(text); free(result);
        return false;
    }
    free(text); free(result);
    return true;
}
