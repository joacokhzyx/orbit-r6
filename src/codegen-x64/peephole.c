#include "peephole.h"

#include <stdint.h>
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

/* D-M1-83: the pass over the text that regalloc.c wrote. It knows which
 * general registers each line reads and fully writes, follows labels and jumps
 * to find what is live after a line, and then
 *   - fuses `movq A, %X` + `movq %X, B` when X is dead afterwards,
 *   - compares A directly in `movq A, %X` + `cmpq B, %X` and `testq`,
 *   - folds `movq $imm, %R` into the one compare, move or add that reads it,
 *   - drops a jump to the next label and the code after `ret`, `jmp`, `ud2`.
 * A line it does not know reads every register. */

#define K_PLAIN 0
#define K_LABEL 1
#define K_JMP 2
#define K_JCC 3
#define K_RET 4
#define K_END 5
#define K_OTHER 6

#define BIT(r) (1u << (r))
#define ALL_REGS 0xffffffffu

typedef struct {
    uint32_t use, def;
    int kind;
    size_t target; /* line index of the label a jump names, or (size_t)-1 */
    bool dead;
    char *text; /* replacement for the line, without its newline */
} PInfo;

typedef struct {
    const char *p;
    size_t n;
} PSpan;

static const char *const G64[16] = {"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
                                    "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15"};
static const char *const G32[16] = {"eax", "ecx",  "edx",  "ebx",  "esp",  "ebp",  "esi",  "edi",
                                    "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d"};
static const char *const G8[16] = {"al",  "cl",  "dl",   "bl",   "spl",  "bpl",  "sil",  "dil",
                                   "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b"};

/* Register of a token written after `%`: its number and width, -2 for a name
 * that is not a general register but harmless (xmm, rip), -1 for an unknown. */
static int token_reg(const char *s, size_t n, int *width) {
    int r;
    *width = 0;
    if (n >= 4 && n <= 5 && memcmp(s, "xmm", 3) == 0) {
        int v = 0;
        size_t d;
        for (d = 3; d < n; ++d) {
            if (s[d] < '0' || s[d] > '9') return -1;
            v = v * 10 + (s[d] - '0');
        }
        if (v > 15) return -1;
        *width = 128;
        return 16 + v;
    }
    if (n == 3 && memcmp(s, "rip", 3) == 0) return -2;
    for (r = 0; r < 16; ++r) {
        if (strlen(G64[r]) == n && memcmp(G64[r], s, n) == 0) {
            *width = 64;
            return r;
        }
        if (strlen(G32[r]) == n && memcmp(G32[r], s, n) == 0) {
            *width = 32;
            return r;
        }
        if (strlen(G8[r]) == n && memcmp(G8[r], s, n) == 0) {
            *width = 8;
            return r;
        }
    }
    return -1;
}

/* Registers named anywhere in the operand; *unknown is set on a name this
 * pass does not know. */
static uint32_t operand_mask(PSpan s, bool *unknown) {
    uint32_t mask = 0;
    size_t k = 0;
    while (k < s.n) {
        size_t start;
        int width, r;
        if (s.p[k] != '%') {
            ++k;
            continue;
        }
        start = ++k;
        while (k < s.n && ((s.p[k] >= 'a' && s.p[k] <= 'z') || (s.p[k] >= '0' && s.p[k] <= '9'))) ++k;
        r = token_reg(s.p + start, k - start, &width);
        if (r >= 0) mask |= BIT(r);
        else if (r == -1) *unknown = true;
    }
    return mask;
}

/* A bare general register operand: its number and width, else -1. */
static int bare_reg(PSpan s, int *width) {
    if (s.n < 3 || s.p[0] != '%' || memchr(s.p, '(', s.n)) return -1;
    {
        int r = token_reg(s.p + 1, s.n - 1, width);
        return r >= 0 ? r : -1;
    }
}

static bool has_paren(PSpan s) {
    return s.n > 0 && memchr(s.p, '(', s.n) != NULL;
}

static bool span_eq(PSpan s, const char *lit) {
    return s.n == strlen(lit) && memcmp(s.p, lit, s.n) == 0;
}

static void split_operands(const char *line, size_t len, PSpan *mn, PSpan *a, PSpan *b) {
    size_t k = len > 0 && line[0] == '\t' ? 1 : 0, start = k, depth = 0;
    while (k < len && line[k] != ' ') ++k;
    mn->p = line + start;
    mn->n = k - start;
    while (k < len && line[k] == ' ') ++k;
    a->p = line + k;
    start = k;
    while (k < len && !(line[k] == ',' && depth == 0)) {
        if (line[k] == '(') ++depth;
        if (line[k] == ')' && depth > 0) --depth;
        ++k;
    }
    a->n = k - start;
    b->p = line + len;
    b->n = 0;
    if (k < len) {
        ++k;
        while (k < len && line[k] == ' ') ++k;
        b->p = line + k;
        b->n = len - k;
    }
}

static bool mnemonic_in(PSpan mn, const char *const *list) {
    for (; *list; ++list)
        if (span_eq(mn, *list)) return true;
    return false;
}

static const char *const MOVES[] = {"movq", "movl", "movslq", "movzbl", "movabsq", "leaq", "movsd",
                                    "movaps", "movb", "movzwl", NULL};
static const char *const READS2[] = {"cmpq", "cmpl", "cmpb", "testq", "testl", "testb", "ucomisd",
                                     "addq", "addl", "subq", "subl", "imulq", "imull", "andq",
                                     "andl", "orq", "orl", "orb", "andb", "xorq", "xorl", "sarl",
                                     "sarq", "shrl", "shll", "addsd", "subsd", "mulsd", "divsd",
                                     "cvtsi2sdq", "xorpd", "negl", "negq", "notl", "notq", NULL};

static void analyze_line(const char *line, size_t len, PInfo *pi) {
    PSpan mn, a, b;
    bool unknown = false;
    pi->use = 0;
    pi->def = 0;
    pi->kind = K_PLAIN;
    pi->target = (size_t)-1;
    if (len == 0) {
        pi->kind = K_OTHER;
        return;
    }
    if (line[0] != '\t') {
        pi->kind = line[len - 1] == ':' ? K_LABEL : K_OTHER;
        return;
    }
    split_operands(line, len, &mn, &a, &b);
    if (mn.n == 0 || mn.p[0] == '.' || mn.p[0] == '#') {
        pi->kind = K_OTHER;
        return;
    }
    if (span_eq(mn, "jmp")) {
        pi->kind = K_JMP;
        return;
    }
    if (mn.p[0] == 'j') {
        pi->kind = K_JCC;
        return;
    }
    if (span_eq(mn, "ret")) {
        pi->kind = K_RET;
        pi->use = BIT(0) | BIT(3) | BIT(6) | BIT(7) | BIT(12) | BIT(13) | BIT(14) | BIT(15) | BIT(16);
        {
            int x;
            for (x = 6; x < 16; ++x) pi->use |= BIT(16 + x);
        }
        return;
    }
    if (span_eq(mn, "ud2")) {
        pi->kind = K_END;
        return;
    }
    if (span_eq(mn, "leave") || span_eq(mn, "nop")) return;
    if (span_eq(mn, "cltd") || span_eq(mn, "cqto")) {
        pi->use = BIT(0);
        pi->def = BIT(2);
        return;
    }
    if (span_eq(mn, "imulq") && b.n == 0) {
        /* One operand: %rdx:%rax = %rax * operand. */
        pi->use = BIT(0) | operand_mask(a, &unknown);
        pi->def = BIT(0) | BIT(2);
        if (unknown) pi->use = ALL_REGS;
        return;
    }
    if (span_eq(mn, "idivl") || span_eq(mn, "idivq")) {
        pi->use = BIT(0) | BIT(2) | operand_mask(a, &unknown);
        pi->def = BIT(0) | BIT(2);
        if (unknown) pi->use = ALL_REGS;
        return;
    }
    if (span_eq(mn, "call") || span_eq(mn, "callq")) {
        pi->use = BIT(1) | BIT(2) | BIT(8) | BIT(9) | BIT(16) | BIT(17) | BIT(18) | BIT(19);
        pi->def = BIT(0) | BIT(1) | BIT(2) | BIT(8) | BIT(9) | BIT(10) | BIT(11) | BIT(16) |
                  BIT(17) | BIT(18) | BIT(19) | BIT(20) | BIT(21);
        if (a.n > 0 && a.p[0] == '*') pi->use |= operand_mask(a, &unknown);
        if (span_eq(a, "___chkstk_ms")) pi->use |= BIT(0);
        if (unknown) pi->use = ALL_REGS;
        return;
    }
    if (span_eq(mn, "pushq")) {
        pi->use = operand_mask(a, &unknown);
        if (unknown) pi->use = ALL_REGS;
        return;
    }
    if (mnemonic_in(mn, MOVES)) {
        int width, r;
        pi->use = operand_mask(a, &unknown);
        r = bare_reg(b, &width);
        if (r >= 0 && width >= 32 && !span_eq(mn, "movb")) {
            if (span_eq(mn, "movsd") && r >= 16 && bare_reg(a, &width) >= 16) pi->use |= BIT(r);
            else pi->def = BIT(r);
        } else {
            pi->use |= operand_mask(b, &unknown);
        }
        if (unknown) {
            pi->use = ALL_REGS;
            pi->def = 0;
        }
        return;
    }
    if (mnemonic_in(mn, READS2) || (mn.n >= 3 && mn.p[0] == 's' && mn.p[1] == 'e' && mn.p[2] == 't')) {
        pi->use = operand_mask(a, &unknown) | operand_mask(b, &unknown);
        if (unknown) pi->use = ALL_REGS;
        return;
    }
    pi->use = ALL_REGS;
}

typedef struct {
    const char *name;
    size_t len, line;
} PLabel;

static int plabel_compare(const void *x, const void *y) {
    const PLabel *l = (const PLabel *)x, *r = (const PLabel *)y;
    size_t n = l->len < r->len ? l->len : r->len;
    int c = memcmp(l->name, r->name, n);
    if (c) return c;
    return l->len < r->len ? -1 : l->len > r->len;
}

typedef struct {
    const char **text;
    size_t *length;
    PInfo *info;
    PLabel *labels;
    uint32_t *live_in;
    size_t cap;
    unsigned long long pool_bits[16];
    size_t pool_label[16];
    size_t pool_count, pool_serial;
} PWork;

#define MAX_POOL 16

static const char *line_text(const PWork *w, size_t i, size_t *len) {
    if (w->info[i].text) {
        *len = strlen(w->info[i].text);
        return w->info[i].text;
    }
    *len = w->length[i];
    return w->text[i];
}

static uint32_t live_out(const PWork *w, size_t i, size_t end) {
    const PInfo *pi = &w->info[i];
    uint32_t out = 0;
    if (pi->kind == K_RET || pi->kind == K_END) return 0;
    if (pi->kind == K_JMP || pi->kind == K_JCC)
        out = pi->target == (size_t)-1 ? ALL_REGS : w->live_in[pi->target];
    if (pi->kind != K_JMP && i + 1 < end) out |= w->live_in[i + 1];
    return out;
}

static size_t next_live(const PWork *w, size_t i, size_t end) {
    size_t j = i + 1;
    while (j < end && w->info[j].dead) ++j;
    return j;
}

static bool set_text(PWork *w, size_t i, PSpan a, PSpan b, const char *mnemonic) {
    char buf[256];
    int n = snprintf(buf, sizeof buf, "	%s %.*s, %.*s", mnemonic, (int)a.n, a.p, (int)b.n, b.p);
    char *copy;
    size_t target = w->info[i].target;
    if (n <= 0 || (size_t)n >= sizeof buf) return false;
    copy = (char *)malloc((size_t)n + 1);
    if (!copy) return false;
    memcpy(copy, buf, (size_t)n + 1);
    free(w->info[i].text);
    w->info[i].text = copy;
    analyze_line(copy, (size_t)n, &w->info[i]);
    w->info[i].target = target;
    return true;
}

static void kill_line(PWork *w, size_t i) {
    w->info[i].dead = true;
    w->info[i].use = 0;
    w->info[i].def = 0;
    w->info[i].kind = K_OTHER;
}

static bool is_imm32(PSpan s, long long *value) {
    size_t k = 1;
    long long v = 0;
    bool neg = false;
    if (s.n < 2 || s.p[0] != '$') return false;
    if (s.p[k] == '-') {
        neg = true;
        ++k;
    }
    if (k >= s.n || s.n - k > 10) return false;
    for (; k < s.n; ++k) {
        if (s.p[k] < '0' || s.p[k] > '9') return false;
        v = v * 10 + (s.p[k] - '0');
    }
    if (neg) v = -v;
    if (v < -2147483648LL || v > 2147483647LL) return false;
    *value = v;
    return true;
}

static bool mentions_xmm(PSpan s) {
    size_t k;
    for (k = 0; k + 4 <= s.n; ++k)
        if (memcmp(s.p + k, "%xmm", 4) == 0) return true;
    return false;
}

static bool same_span(PSpan x, PSpan y) {
    return x.n == y.n && memcmp(x.p, y.p, x.n) == 0;
}

/* The mnemonic that reads the immediate in place of register `src` on line
 * `kmn ka, kb`, or NULL when the encoder has no such form. */
static const char *folded_mnemonic(PSpan kmn, int src_width, int dst_reg, int dst_width, bool dst_mem,
                                   bool from_q) {
    if (src_width == 64 && from_q) {
        if (span_eq(kmn, "cmpq") && (dst_reg >= 0 || dst_mem)) return "cmpq";
        if (span_eq(kmn, "movq") && (dst_reg >= 0 || dst_mem)) return "movq";
    }
    if (src_width == 32 && dst_reg == 0 && dst_width == 32) {
        if (span_eq(kmn, "addl")) return "addl";
        if (span_eq(kmn, "subl")) return "subl";
        if (span_eq(kmn, "cmpl")) return "cmpl";
        if (span_eq(kmn, "imull")) return "imull";
    }
    return NULL;
}


/* Text of span `s` with every 64- or 32-bit name of register `d` written as
 * the same-width name of `s`. False on an 8-bit name of `d` or an overflow. */
static bool rename_register(PSpan s, int d, int src, char *out, size_t cap, size_t *used) {
    size_t k = 0;
    while (k < s.n) {
        size_t start, n;
        int width, r;
        if (s.p[k] != '%') {
            if (*used + 1 >= cap) return false;
            out[(*used)++] = s.p[k++];
            continue;
        }
        start = ++k;
        while (k < s.n && ((s.p[k] >= 'a' && s.p[k] <= 'z') || (s.p[k] >= '0' && s.p[k] <= '9'))) ++k;
        r = token_reg(s.p + start, k - start, &width);
        if (r == d) {
            const char *name;
            char xname[16];
            if (width == 8) return false;
            if (width == 128) {
                snprintf(xname, sizeof xname, "xmm%d", src - 16);
                name = xname;
            } else {
                name = width == 64 ? G64[src] : G32[src];
            }
            n = strlen(name);
            if (*used + n + 2 >= cap) return false;
            out[(*used)++] = '%';
            memcpy(out + *used, name, n);
            *used += n;
        } else {
            n = k - start;
            if (*used + n + 2 >= cap) return false;
            out[(*used)++] = '%';
            memcpy(out + *used, s.p + start, n);
            *used += n;
        }
    }
    return true;
}

static bool is_compare_mnemonic(PSpan mn) {
    return span_eq(mn, "cmpq") || span_eq(mn, "cmpl") || span_eq(mn, "testq") ||
           span_eq(mn, "testl") || span_eq(mn, "ucomisd");
}

/* A load that cannot fault: a frame slot, the stack or a symbol. */
static bool cannot_fault(PSpan a) {
    if (a.n == 0) return false;
    if (a.p[0] == '$') return true;
    if (a.p[0] == '%') return !has_paren(a);
    return (a.n >= 6 && memcmp(a.p + a.n - 6, "(%rbp)", 6) == 0) ||
           (a.n >= 6 && memcmp(a.p + a.n - 6, "(%rsp)", 6) == 0) ||
           (a.n >= 6 && memcmp(a.p + a.n - 6, "(%rip)", 6) == 0);
}

#define MAX_WINDOW 8

/* True when the line may change register `r` in a way the def mask does not
 * show: an 8-bit write, a read-modify-write operand, or a flag set. */
static bool writes_register(PSpan mn, PSpan a, PSpan b, int r) {
    int w;
    bool unknown = false;
    if (mn.n >= 3 && mn.p[0] == 's' && mn.p[1] == 'e' && mn.p[2] == 't') return true;
    if (span_eq(mn, "movb") || span_eq(mn, "orb") || span_eq(mn, "andb"))
        return (operand_mask(b, &unknown) & BIT(r)) != 0 || unknown;
    if (is_compare_mnemonic(mn) || span_eq(mn, "cmpb") || span_eq(mn, "ucomisd")) return false;
    if (b.n > 0 && bare_reg(b, &w) == r) return !mnemonic_in(mn, MOVES) || w < 32 || span_eq(mn, "movsd");
    if (b.n == 0 && bare_reg(a, &w) == r) return !span_eq(mn, "idivl") && !span_eq(mn, "idivq") &&
                                                   !span_eq(mn, "pushq");
    return false;
}

/* movq %S, %D with every reader of D within the window rewritten to read S
 * and D dead afterwards: the copy goes. Returns 1 on a rewrite, 0 for none,
 * -1 on allocation failure. */
static int propagate_copy(PWork *w, size_t i, size_t end, int src, int dst) {
    size_t seen[MAX_WINDOW];
    char *texts[MAX_WINDOW];
    size_t count = 0, k, steps;
    bool ok = false;
    int result = 0;
    k = next_live(w, i, end);
    for (steps = 0; steps < MAX_WINDOW && k < end; ++steps, k = next_live(w, k, end)) {
        const PInfo *pi = &w->info[k];
        const char *kl;
        size_t klen, used = 0;
        PSpan kmn, ka, kb;
        char buf[256];
        bool unknown = false, b_bare_dst;
        int wd;
        if (pi->kind == K_JMP || pi->kind == K_JCC) {
            ok = !(live_out(w, k, end) & BIT(dst)) && !((pi->use) & BIT(dst));
            break;
        }
        if (pi->kind != K_PLAIN) break;
        if (!((pi->use | pi->def) & BIT(dst))) {
            const char *ol;
            size_t olen;
            PSpan omn, oa, ob;
            if (pi->def & BIT(src)) break;
            ol = line_text(w, k, &olen);
            split_operands(ol, olen, &omn, &oa, &ob);
            if (writes_register(omn, oa, ob, src)) break;
            if (!(live_out(w, k, end) & BIT(dst))) {
                ok = true;
                break;
            }
            continue;
        }
        kl = line_text(w, k, &klen);
        split_operands(kl, klen, &kmn, &ka, &kb);
        {
            uint32_t explicit_use = (uint32_t)(operand_mask(ka, &unknown) | operand_mask(kb, &unknown));
            if (unknown || (pi->use & BIT(dst) & (uint32_t)~explicit_use)) break;
        }
        if (((pi->def & BIT(src)) || writes_register(kmn, ka, kb, src)) &&
            (live_out(w, k, end) & BIT(dst)))
            break;
        if (kb.n == 0 && bare_reg(ka, &wd) == dst && !span_eq(kmn, "idivl") &&
            !span_eq(kmn, "idivq") && !span_eq(kmn, "pushq"))
            break;
        b_bare_dst = bare_reg(kb, &wd) == dst;
        if (b_bare_dst && !is_compare_mnemonic(kmn) && !(pi->def & BIT(dst))) break;
        if (kmn.n + 2 >= sizeof buf) break;
        buf[used++] = '\t';
        memcpy(buf + used, kmn.p, kmn.n);
        used += kmn.n;
        buf[used++] = ' ';
        if (!rename_register(ka, dst, src, buf, sizeof buf, &used)) break;
        if (kb.n > 0) {
            buf[used++] = ',';
            buf[used++] = ' ';
            if (b_bare_dst && (pi->def & BIT(dst))) {
                if (used + kb.n + 1 >= sizeof buf) break;
                memcpy(buf + used, kb.p, kb.n);
                used += kb.n;
            } else if (!rename_register(kb, dst, src, buf, sizeof buf, &used)) {
                break;
            }
        }
        buf[used] = 0;
        texts[count] = (char *)malloc(used + 1);
        if (!texts[count]) {
            size_t c;
            for (c = 0; c < count; ++c) free(texts[c]);
            return -1;
        }
        memcpy(texts[count], buf, used + 1);
        seen[count++] = k;
        if (pi->def & BIT(dst)) {
            ok = true;
            break;
        }
        if (!(live_out(w, k, end) & BIT(dst))) {
            ok = true;
            break;
        }
    }
    if (ok) {
        size_t c;
        for (c = 0; c < count; ++c) {
            size_t target = w->info[seen[c]].target;
            free(w->info[seen[c]].text);
            w->info[seen[c]].text = texts[c];
            analyze_line(texts[c], strlen(texts[c]), &w->info[seen[c]]);
            w->info[seen[c]].target = target;
        }
        kill_line(w, i);
        result = 1;
    } else {
        size_t c;
        for (c = 0; c < count; ++c) free(texts[c]);
    }
    return result;
}

/* One forward sweep of the patterns over [first, end). Returns -1 on
 * allocation failure, else the number of rewrites. */
static long sweep_function(PWork *w, size_t first, size_t end) {
    size_t i;
    long changed = 0;
    for (i = first; i < end; ++i) {
        size_t len, jlen, j;
        const char *line, *jline;
        PSpan mn, a, b, mn2, a2, b2;
        int wa, wb, ra, rb;
        bool is_q, is_l;
        if (w->info[i].dead) continue;
        if (w->info[i].kind == K_JMP || w->info[i].kind == K_RET || w->info[i].kind == K_END) {
            size_t k = next_live(w, i, end);
            while (k < end && w->info[k].kind == K_PLAIN) {
                kill_line(w, k);
                ++changed;
                k = next_live(w, k, end);
            }
            if (w->info[i].kind == K_JMP && k < end && w->info[k].kind == K_LABEL &&
                w->info[i].target == k) {
                kill_line(w, i);
                ++changed;
            }
            continue;
        }
        if (w->info[i].kind != K_PLAIN) continue;
        line = line_text(w, i, &len);
        split_operands(line, len, &mn, &a, &b);
        j = next_live(w, i, end);
        /* A move of a register onto itself. */
        if ((span_eq(mn, "movq") || span_eq(mn, "movaps")) && a.n > 0 && same_span(a, b) &&
            bare_reg(a, &wa) >= 0 && (wa == 64 || wa == 128)) {
            kill_line(w, i);
            ++changed;
            continue;
        }
        is_q = span_eq(mn, "movq");
        is_l = span_eq(mn, "movl");
        rb = (is_q || is_l) ? bare_reg(b, &wb) : -1;
        /* A definition nothing reads, from a source that cannot fault. */
        if ((is_q || is_l || span_eq(mn, "movslq") || span_eq(mn, "leaq") || span_eq(mn, "movabsq") ||
             span_eq(mn, "movaps") || span_eq(mn, "movsd")) &&
            (rb = bare_reg(b, &wb)) >= 0 && wb >= 32 && rb != 4 && rb != 5 && cannot_fault(a) &&
            !(live_out(w, i, end) & BIT(rb))) {
            kill_line(w, i);
            ++changed;
            continue;
        }
        rb = (is_q || is_l) ? bare_reg(b, &wb) : -1;
        /* movq/movl A, %X ; movq/movl %X, B  becomes  movq/movl A, B */
        if (rb >= 0 && wb == (is_q ? 64 : 32) && j < end && w->info[j].kind == K_PLAIN &&
            !mentions_xmm(a)) {
            bool unknown = false;
            jline = line_text(w, j, &jlen);
            split_operands(jline, jlen, &mn2, &a2, &b2);
            if (span_eq(mn2, is_q ? "movq" : "movl") && same_span(a2, b) && b2.n > 0 &&
                !(has_paren(a) && has_paren(b2)) && !mentions_xmm(b2) &&
                (bare_reg(b2, &wb) >= 0 || has_paren(b2)) &&
                !(operand_mask(b2, &unknown) & BIT(rb)) && !unknown &&
                !(live_out(w, j, end) & BIT(rb))) {
                if (!set_text(w, i, a, b2, is_q ? "movq" : "movl")) return -1;
                kill_line(w, j);
                ++changed;
                continue;
            }
        }
        /* movq A, %X ; cmpq B, %X  and  movq A, %X ; testq %X, %X */
        if (is_q && (ra = bare_reg(a, &wa)) >= 0 && wa == 64 && rb >= 0 && wb == 64 && ra != rb &&
            j < end && w->info[j].kind == K_PLAIN) {
            bool unknown = false;
            jline = line_text(w, j, &jlen);
            split_operands(jline, jlen, &mn2, &a2, &b2);
            if (span_eq(mn2, "cmpq") && same_span(b2, b) && a2.n > 0 &&
                !(operand_mask(a2, &unknown) & BIT(rb)) && !unknown && !mentions_xmm(a2) &&
                !(live_out(w, j, end) & BIT(rb))) {
                if (!set_text(w, j, a2, a, "cmpq")) return -1;
                kill_line(w, i);
                ++changed;
                continue;
            }
            if (span_eq(mn2, "testq") && same_span(a2, b) && same_span(b2, b) &&
                !(live_out(w, j, end) & BIT(rb))) {
                if (!set_text(w, j, a, a, "testq")) return -1;
                kill_line(w, i);
                ++changed;
                continue;
            }
        }
        /* movabsq $bits, %D ; movq %D, %xmmK  loads the constant from .rdata */
        if (span_eq(mn, "movabsq") && (ra = bare_reg(b, &wa)) >= 0 && wa == 64 && ra < 16 &&
            a.n > 3 && a.p[0] == '$' && a.p[1] == '0' && a.p[2] == 'x' && j < end && w->info[j].kind == K_PLAIN) {
            PSpan jm, ja, jb;
            int rx, wx;
            jline = line_text(w, j, &jlen);
            split_operands(jline, jlen, &jm, &ja, &jb);
            if (span_eq(jm, "movq") && same_span(ja, b) && (rx = bare_reg(jb, &wx)) >= 16 &&
                !(live_out(w, j, end) & BIT(ra))) {
                unsigned long long bits = strtoull(a.p + 1, NULL, 16);
                size_t e;
                char label[40], text[128];
                for (e = 0; e < w->pool_count; ++e)
                    if (w->pool_bits[e] == bits) break;
                if (e == w->pool_count) {
                    if (e >= MAX_POOL) goto no_pool;
                    w->pool_bits[e] = bits;
                    w->pool_label[e] = w->pool_serial++;
                    ++w->pool_count;
                }
                snprintf(label, sizeof label, ".LCr6f%zu(%%rip)", w->pool_label[e]);
                snprintf(text, sizeof text, "\tmovsd %s, %.*s", label, (int)jb.n, jb.p);
                {
                    char *copy = (char *)malloc(strlen(text) + 1);
                    if (!copy) return -1;
                    strcpy(copy, text);
                    free(w->info[j].text);
                    w->info[j].text = copy;
                    analyze_line(copy, strlen(copy), &w->info[j]);
                }
                kill_line(w, i);
                ++changed;
                continue;
            }
        }
    no_pool:
        /* movaps/movsd A, %X ; OP B, %X ; movaps %X, D  becomes  movaps A, D ; OP B, D */
        if ((span_eq(mn, "movaps") || span_eq(mn, "movsd")) && (rb = bare_reg(b, &wb)) >= 16 &&
            j < end && w->info[j].kind == K_PLAIN) {
            PSpan jm, ja, jb, km, ka2, kb2;
            size_t k2 = next_live(w, j, end), klen2;
            const char *kline2;
            int rd, wd2, rbb, wbb;
            jline = line_text(w, j, &jlen);
            split_operands(jline, jlen, &jm, &ja, &jb);
            if ((span_eq(jm, "addsd") || span_eq(jm, "subsd") || span_eq(jm, "mulsd") ||
                 span_eq(jm, "divsd")) && same_span(jb, b) && k2 < end && w->info[k2].kind == K_PLAIN) {
                kline2 = line_text(w, k2, &klen2);
                split_operands(kline2, klen2, &km, &ka2, &kb2);
                rd = bare_reg(kb2, &wd2);
                rbb = bare_reg(ja, &wbb);
                if (span_eq(km, "movaps") && same_span(ka2, b) && rd >= 16 && rd != rb &&
                    rbb != rd && !(operand_mask(ja, &(bool){false}) & (BIT(rd) | BIT(rb))) &&
                    !(live_out(w, k2, end) & BIT(rb))) {
                    char first[128];
                    char *copy;
                    const char *fm = span_eq(mn, "movaps") ? "movaps" : "movsd";
                    bool same = a.n == kb2.n && memcmp(a.p, kb2.p, a.n) == 0;
                    if (same) {
                        snprintf(first, sizeof first, "\t%.*s %.*s, %.*s", (int)jm.n, jm.p, (int)ja.n, ja.p,
                                 (int)kb2.n, kb2.p);
                        kill_line(w, i);
                    } else {
                        if (!set_text(w, i, a, kb2, fm)) return -1;
                        snprintf(first, sizeof first, "\t%.*s %.*s, %.*s", (int)jm.n, jm.p, (int)ja.n, ja.p,
                                 (int)kb2.n, kb2.p);
                    }
                    copy = (char *)malloc(strlen(first) + 1);
                    if (!copy) return -1;
                    strcpy(copy, first);
                    free(w->info[j].text);
                    w->info[j].text = copy;
                    analyze_line(copy, strlen(copy), &w->info[j]);
                    kill_line(w, k2);
                    ++changed;
                    continue;
                }
            }
        }
        /* movaps %S, %D ; readers of D read S instead */
        if (span_eq(mn, "movaps") && (ra = bare_reg(a, &wa)) >= 16 && (rb = bare_reg(b, &wb)) >= 16 &&
            ra != rb) {
            int r = propagate_copy(w, i, end, ra, rb);
            if (r < 0) return -1;
            if (r > 0) {
                ++changed;
                continue;
            }
        }
        /* movsd .LC(%rip), %K ; OP %K, %X  becomes  OP .LC(%rip), %X */
        if (span_eq(mn, "movsd") && (rb = bare_reg(b, &wb)) >= 16 && a.n > 6 &&
            memcmp(a.p + a.n - 6, "(%rip)", 6) == 0) {
            size_t k = j, steps;
            for (steps = 0; steps < 4 && k < end; ++steps, k = next_live(w, k, end)) {
                const char *kl;
                size_t klen;
                PSpan kmn, ka, kb;
                if (w->info[k].kind != K_PLAIN) break;
                if (!((w->info[k].use | w->info[k].def) & BIT(rb))) continue;
                kl = line_text(w, k, &klen);
                split_operands(kl, klen, &kmn, &ka, &kb);
                if ((span_eq(kmn, "addsd") || span_eq(kmn, "subsd") || span_eq(kmn, "mulsd") ||
                     span_eq(kmn, "divsd") || span_eq(kmn, "ucomisd")) &&
                    bare_reg(ka, &wa) == rb && bare_reg(kb, &wa) >= 16 && !same_span(kb, b) &&
                    !(live_out(w, k, end) & BIT(rb))) {
                    char mnbuf[16];
                    memcpy(mnbuf, kmn.p, kmn.n);
                    mnbuf[kmn.n] = 0;
                    if (!set_text(w, k, a, kb, mnbuf)) return -1;
                    kill_line(w, i);
                    ++changed;
                    break;
                }
                break;
            }
            if (w->info[i].dead) continue;
        }
        /* movq %S, %D ; readers of D read S instead */
        if (is_q && (ra = bare_reg(a, &wa)) >= 0 && wa == 64 && rb >= 0 && wb == 64 && ra != rb &&
            ra != 4 && ra != 5 && rb != 4 && rb != 5) {
            int r = propagate_copy(w, i, end, ra, rb);
            if (r < 0) return -1;
            if (r > 0) {
                ++changed;
                continue;
            }
        }
        /* movq $imm, %R ; ... one reader of R ... */
        {
            long long imm;
            if ((is_q || is_l) && rb >= 0 && wb == (is_q ? 64 : 32) && rb != 4 && rb != 5 &&
                is_imm32(a, &imm)) {
                size_t k = j, steps;
                for (steps = 0; steps < 4 && k < end; ++steps, k = next_live(w, k, end)) {
                    const char *kl;
                    size_t klen;
                    PSpan kmn, ka, kb;
                    int ua, ub, wua = 0, wub = 0;
                    bool unknown = false;
                    const char *newmn;
                    if (w->info[k].kind != K_PLAIN) break;
                    if (!((w->info[k].use | w->info[k].def) & BIT(rb))) continue;
                    kl = line_text(w, k, &klen);
                    split_operands(kl, klen, &kmn, &ka, &kb);
                    ua = bare_reg(ka, &wua);
                    ub = bare_reg(kb, &wub);
                    if (ua != rb || mentions_xmm(kb)) break;
                    if (operand_mask(kb, &unknown) & BIT(rb)) break;
                    if (unknown) break;
                    newmn = folded_mnemonic(kmn, wua, ub, wub, has_paren(kb), is_q);
                    if (!newmn) break;
                    if (live_out(w, k, end) & BIT(rb)) break;
                    if (!set_text(w, k, a, kb, newmn)) return -1;
                    kill_line(w, i);
                    ++changed;
                    break;
                }
            }
        }
    }
    return changed;
}

static bool process_final(PWork *w, size_t first, size_t end) {
    size_t i, nlabels = 0;
    int round;
    for (round = 0; round < 4; ++round) {
        long changed;
        bool again;
        nlabels = 0;
        for (i = first; i < end; ++i) {
            size_t len;
            const char *line;
            if (w->info[i].dead) continue;
            line = line_text(w, i, &len);
            analyze_line(line, len, &w->info[i]);
            if (w->info[i].kind == K_LABEL) {
                w->labels[nlabels].name = line;
                w->labels[nlabels].len = len - 1;
                w->labels[nlabels++].line = i;
            }
        }
        qsort(w->labels, nlabels, sizeof *w->labels, plabel_compare);
        for (i = first; i < end; ++i) {
            size_t len;
            const char *line;
            PSpan mn, a, b;
            PLabel key, *hit;
            if (w->info[i].dead || (w->info[i].kind != K_JMP && w->info[i].kind != K_JCC)) continue;
            line = line_text(w, i, &len);
            split_operands(line, len, &mn, &a, &b);
            key.name = a.p;
            key.len = a.n;
            key.line = 0;
            hit = (PLabel *)bsearch(&key, w->labels, nlabels, sizeof *w->labels, plabel_compare);
            w->info[i].target = hit ? hit->line : (size_t)-1;
        }
        /* Dead lines are transparent: their use and def are cleared. */
        for (i = first; i < end; ++i)
            if (w->info[i].dead) {
                w->info[i].use = 0;
                w->info[i].def = 0;
                w->info[i].kind = K_OTHER;
            }
        for (i = first; i < end; ++i) w->live_in[i] = 0;
        do {
            again = false;
            for (i = end; i-- > first;) {
                uint32_t in, out = live_out(w, i, end);
                in = (uint32_t)(w->info[i].use | (out & (uint32_t)~w->info[i].def));
                if (in != w->live_in[i]) {
                    w->live_in[i] = in;
                    again = true;
                }
            }
        } while (again);
        changed = sweep_function(w, first, end);
        if (changed < 0) return false;
        if (changed == 0) break;
    }
    if (w->pool_count > 0) {
        char *block = (char *)malloc(w->pool_count * 160 + 64);
        size_t e, used = 0;
        if (!block) return false;
        used += (size_t)sprintf(block + used, "\t.section .rdata,\"dr\"");
        for (e = 0; e < w->pool_count; ++e) {
            unsigned long long v = w->pool_bits[e];
            used += (size_t)sprintf(block + used, "\n.LCr6f%zu:\n\t.byte %llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu",
                                    w->pool_label[e], v & 255, (v >> 8) & 255, (v >> 16) & 255,
                                    (v >> 24) & 255, (v >> 32) & 255, (v >> 40) & 255, (v >> 48) & 255,
                                    (v >> 56) & 255);
        }
        used += (size_t)sprintf(block + used, "\n\t.text\n\t.seh_endproc");
        free(w->info[end - 1].text);
        w->info[end - 1].text = block;
        w->pool_count = 0;
    }
    return true;
}

bool r6_peephole_final(const char *path, char *error, size_t error_capacity) {
    FILE *file = fopen(path, "rb");
    char *text = NULL, *result = NULL;
    size_t size, count = 0, cap = 0, at = 0, i, used = 0, fn_first = 0;
    long end;
    PWork w;
    bool in_function = false, ok = false;
    memset(&w, 0, sizeof w);
    if (!file || fseek(file, 0, SEEK_END) != 0 || (end = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        snprintf(error, error_capacity, "cannot read generated assembler text");
        if (file) fclose(file);
        return false;
    }
    size = (size_t)end;
    text = (char *)malloc(size + 1);
    if (!text || fread(text, 1, size, file) != size) {
        snprintf(error, error_capacity, "cannot load generated assembler text");
        fclose(file);
        free(text);
        return false;
    }
    fclose(file);
    while (at < size) {
        const char *nl = (const char *)memchr(text + at, '\n', size - at);
        size_t length = nl ? (size_t)(nl - (text + at)) : size - at;
        if (count == cap) {
            size_t ncap = cap ? cap * 2 : 4096;
            const char **t = (const char **)realloc(w.text, ncap * sizeof *t);
            size_t *l;
            if (!t) goto oom;
            w.text = t;
            l = (size_t *)realloc(w.length, ncap * sizeof *l);
            if (!l) goto oom;
            w.length = l;
            cap = ncap;
        }
        w.text[count] = text + at;
        w.length[count++] = length;
        at += length + (nl ? 1 : 0);
    }
    w.info = (PInfo *)calloc(count + 1, sizeof *w.info);
    w.labels = (PLabel *)malloc((count + 1) * sizeof *w.labels);
    w.live_in = (uint32_t *)calloc(count + 1, sizeof *w.live_in);
    if (!w.info || !w.labels || !w.live_in) goto oom;
    for (i = 0; i < count; ++i) {
        if (!in_function) {
            if (w.length[i] > 10 && memcmp(w.text[i], "\t.seh_proc ", 11) == 0) {
                in_function = true;
                fn_first = i;
            }
            continue;
        }
        if (w.length[i] == 13 && memcmp(w.text[i], "\t.seh_endproc", 13) == 0) {
            if (!process_final(&w, fn_first, i + 1)) goto oom;
            in_function = false;
        }
    }
    {
        size_t total = 0;
        for (i = 0; i < count; ++i) {
            size_t len;
            if (w.info[i].dead) continue;
            line_text(&w, i, &len);
            total += len + 1;
        }
        result = (char *)malloc(total + 1);
        if (!result) goto oom;
    }
    for (i = 0; i < count; ++i) {
        size_t len;
        const char *line;
        if (w.info[i].dead) continue;
        line = line_text(&w, i, &len);
        memcpy(result + used, line, len);
        used += len;
        result[used++] = '\n';
    }
    file = fopen(path, "wb");
    if (!file || fwrite(result, 1, used, file) != used || fclose(file) != 0) {
        snprintf(error, error_capacity, "cannot write rewritten assembler text");
        goto done;
    }
    ok = true;
    goto done;
oom:
    snprintf(error, error_capacity, "out of memory in the final peephole pass");
done:
    if (w.info) {
        for (i = 0; i < count; ++i) free(w.info[i].text);
    }
    free(w.info);
    free(w.labels);
    free(w.live_in);
    free(w.text);
    free(w.length);
    free(text);
    free(result);
    return ok;
}
