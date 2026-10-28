#include "regalloc.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* D-M1-68: the generator keeps every IR register in an 8-byte frame slot. This
 * stage runs over the finished text of each function. It gives the slots that
 * the function reads and writes most to rbx, rsi, rdi and r12-r15, which every
 * callee preserves. A slot keeps its register from its first to its last
 * reference, so two slots whose references do not overlap share one. A slot
 * whose address is taken, whose reference the assembler cannot encode with a
 * register, or that lies outside the 8-byte slots stays in memory.
 *
 * The registers a function uses are pushed after the frame pointer is set,
 * so the slots move down by the size of the push area. The generator marks
 * where the plain slots end with a `# r6.slots` line, and this stage removes
 * it. */

#define REG_COUNT 7
/* D-M1-80: float slots take xmm6-xmm14, which every callee preserves. The
 * generator itself uses xmm0-xmm3 and xmm15. */
#define XMM_COUNT 9
#define MAX_REFS 4
/* A register costs a push and a reload; it is kept when the weights of its
 * slots sum to at least this much. */
#define MIN_REGISTER_WEIGHT 6

static const char *const REG_NAMES[REG_COUNT][3] = {
    {"%rbx", "%ebx", "%bl"},    {"%rsi", "%esi", "%sil"},
    {"%rdi", "%edi", "%dil"},   {"%r12", "%r12d", "%r12b"},
    {"%r13", "%r13d", "%r13b"}, {"%r14", "%r14d", "%r14b"},
    {"%r15", "%r15d", "%r15b"}};

static const char *const XMM_NAMES[XMM_COUNT] = {"%xmm6", "%xmm7", "%xmm8", "%xmm9", "%xmm10",
                                                 "%xmm11", "%xmm12", "%xmm13", "%xmm14"};

typedef struct {
    char *p;
    size_t n, cap;
} Buffer;

static bool buffer_add(Buffer *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 4096;
        char *q;
        while (cap < b->n + n + 1) cap *= 2;
        q = (char *)realloc(b->p, cap);
        if (!q) return false;
        b->p = q;
        b->cap = cap;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    return true;
}

static bool buffer_format(Buffer *b, const char *fmt, ...) {
    char tmp[128];
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    return n > 0 && (size_t)n < sizeof tmp && buffer_add(b, tmp, (size_t)n);
}

typedef struct {
    const char *p;
    size_t n;
} Span;

static bool span_is(Span s, const char *literal) {
    return s.n == strlen(literal) && memcmp(s.p, literal, s.n) == 0;
}

static bool span_starts(Span s, const char *prefix) {
    size_t n = strlen(prefix);
    return s.n >= n && memcmp(s.p, prefix, n) == 0;
}

/* One `-N(%rbp)` inside a line. */
typedef struct {
    size_t at, len;
    unsigned offset;
} Ref;

static size_t find_refs(const char *line, size_t len, Ref *refs, size_t max) {
    size_t k, n = 0;
    for (k = 1; k + 6 <= len; ++k) {
        size_t j;
        unsigned offset = 0;
        if (line[k] != '(' || memcmp(line + k, "(%rbp)", 6) != 0) continue;
        j = k;
        while (j > 0 && line[j - 1] >= '0' && line[j - 1] <= '9') --j;
        if (j == k || j == 0 || line[j - 1] != '-') continue;
        if (k - j > 8) offset = 0xffffffffu;
        else {
            size_t d;
            for (d = j; d < k; ++d) offset = offset * 10 + (unsigned)(line[d] - '0');
        }
        if (n < max) {
            refs[n].at = j - 1;
            refs[n].len = k + 6 - (j - 1);
            refs[n].offset = offset;
        }
        ++n;
    }
    return n;
}

static void split_line(const char *line, size_t len, Span *mnemonic, Span *a, Span *b) {
    size_t k = len > 0 && line[0] == '\t' ? 1 : 0, start, depth = 0;
    start = k;
    while (k < len && line[k] != ' ') ++k;
    mnemonic->p = line + start;
    mnemonic->n = k - start;
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

static int gpr_width(Span s) {
    static const char *const r64[] = {"%rax", "%rcx", "%rdx", "%rbx", "%rsi", "%rdi", "%r8",
                                      "%r9",  "%r10", "%r11", "%r12", "%r13", "%r14", "%r15"};
    static const char *const r32[] = {"%eax", "%ecx",  "%edx",  "%ebx",  "%esi",  "%edi", "%r8d",
                                      "%r9d", "%r10d", "%r11d", "%r12d", "%r13d", "%r14d", "%r15d"};
    size_t k;
    for (k = 0; k < sizeof r64 / sizeof r64[0]; ++k)
        if (span_is(s, r64[k])) return 64;
    for (k = 0; k < sizeof r32 / sizeof r32[0]; ++k)
        if (span_is(s, r32[k])) return 32;
    return 0;
}

static bool is_immediate(Span s) {
    size_t k = 1;
    if (s.n < 2 || s.p[0] != '$') return false;
    if (s.p[k] == '-') ++k;
    if (k >= s.n) return false;
    for (; k < s.n; ++k)
        if (s.p[k] < '0' || s.p[k] > '9') return false;
    return true;
}

/* How an instruction touches the one slot it names. `width` is the register
 * width the slot's text becomes. Only the forms the assembler encodes with a
 * register in place of the memory operand are listed (D-M1-68). */
typedef struct {
    bool write;
    int width;
    int cls; /* 0 integer register, 1 xmm register, 2 either (movq with a GPR) */
} Use;

static bool is_xmm(Span s) {
    return s.n >= 5 && memcmp(s.p, "%xmm", 4) == 0;
}

static bool classify_line(const char *line, size_t len, const Ref *refs, size_t nrefs,
                          Use *use) {
    Span mn, a, b;
    bool slot_a, slot_b;
    if (nrefs != 1) return false;
    split_line(line, len, &mn, &a, &b);
    slot_a = refs[0].at == (size_t)(a.p - line) && refs[0].len == a.n;
    slot_b = refs[0].at == (size_t)(b.p - line) && refs[0].len == b.n && b.n > 0;
    if (!slot_a && !slot_b) return false;
    use->write = false;
    use->cls = 0;
    if (span_is(mn, "movsd")) {
        use->width = 64;
        use->cls = 1;
        if (slot_a && is_xmm(b)) return true;
        use->write = true;
        return slot_b && is_xmm(a);
    }
    if (span_is(mn, "addsd") || span_is(mn, "subsd") || span_is(mn, "mulsd") ||
        span_is(mn, "divsd") || span_is(mn, "ucomisd")) {
        use->width = 64;
        use->cls = 1;
        return slot_a && is_xmm(b);
    }
    if (span_is(mn, "cvtsi2sdq")) {
        use->width = 64;
        return slot_a && is_xmm(b);
    }
    if (span_is(mn, "movq")) {
        use->width = 64;
        if (slot_a && gpr_width(b) == 64) {
            use->cls = 2;
            return true;
        }
        use->write = true;
        if (slot_b && gpr_width(a) == 64) use->cls = 2;
        return slot_b && (is_immediate(a) || gpr_width(a) == 64);
    }
    if (span_is(mn, "movl")) {
        use->width = 32;
        if (slot_a && gpr_width(b) == 32) return true;
        use->write = true;
        return slot_b && (is_immediate(a) || gpr_width(a) == 32);
    }
    if (span_is(mn, "cmpl")) {
        use->width = 32;
        if (slot_b) return is_immediate(a);
        return span_is(b, "%eax");
    }
    if (span_is(mn, "cmpb")) {
        use->width = 8;
        return slot_b && is_immediate(a);
    }
    if (span_is(mn, "addl") || span_is(mn, "subl") || span_is(mn, "imull") ||
        span_is(mn, "andl") || span_is(mn, "orl") || span_is(mn, "xorl")) {
        use->width = 32;
        return slot_a && span_is(b, "%eax");
    }
    if (span_is(mn, "cmpq") || span_is(mn, "addq") || span_is(mn, "subq") ||
        span_is(mn, "imulq")) {
        use->width = 64;
        return slot_a && gpr_width(b) == 64;
    }
    if (span_is(mn, "movzbl")) {
        use->width = 8;
        return slot_a && gpr_width(b) == 32;
    }
    if (span_is(mn, "movslq")) {
        use->width = 32;
        return slot_a && gpr_width(b) == 64;
    }
    if (span_is(mn, "idivl")) {
        use->width = 32;
        return slot_a && b.n == 0;
    }
    if (span_is(mn, "idivq")) {
        use->width = 64;
        return slot_a && b.n == 0;
    }
    return false;
}

/* True when the line names a register the allocator hands out. The generator
 * never does; a line that does stops the stage from touching its function. */
static bool mentions_saved_register(const char *line, size_t len) {
    size_t k = 0;
    while (k < len) {
        size_t start, n;
        if (line[k] != '%') {
            ++k;
            continue;
        }
        start = ++k;
        while (k < len && ((line[k] >= 'a' && line[k] <= 'z') || (line[k] >= '0' && line[k] <= '9')))
            ++k;
        n = k - start;
        if (n >= 4 && memcmp(line + start, "xmm", 3) == 0) {
            int v = 0;
            size_t d;
            for (d = start + 3; d < start + n; ++d) v = v * 10 + (line[d] - '0');
            if (v >= 6 && v <= 14) return true;
        }
        if (n >= 3 && line[start] == 'r' && line[start + 1] == '1' && line[start + 2] >= '2' &&
            line[start + 2] <= '5' && (n == 3 || (n == 4 && (line[start + 3] == 'd' ||
                                                              line[start + 3] == 'w' ||
                                                              line[start + 3] == 'b'))))
            return true;
        if ((n == 3 && (memcmp(line + start, "rbx", 3) == 0 || memcmp(line + start, "ebx", 3) == 0 ||
                        memcmp(line + start, "rsi", 3) == 0 || memcmp(line + start, "esi", 3) == 0 ||
                        memcmp(line + start, "sil", 3) == 0 || memcmp(line + start, "rdi", 3) == 0 ||
                        memcmp(line + start, "edi", 3) == 0 || memcmp(line + start, "dil", 3) == 0)) ||
            (n == 2 && (memcmp(line + start, "bl", 2) == 0 || memcmp(line + start, "bh", 2) == 0 ||
                        memcmp(line + start, "bx", 2) == 0 || memcmp(line + start, "si", 2) == 0 ||
                        memcmp(line + start, "di", 2) == 0)))
            return true;
    }
    return false;
}

typedef struct {
    const char *name;
    size_t len, line;
} Label;

static int label_compare(const void *x, const void *y) {
    const Label *l = (const Label *)x, *r = (const Label *)y;
    size_t n = l->len < r->len ? l->len : r->len;
    int c = memcmp(l->name, r->name, n);
    if (c) return c;
    return l->len < r->len ? -1 : l->len > r->len;
}

typedef struct {
    size_t from, to;
} Edge;

typedef struct {
    bool bad;
    bool gp, fl;
    bool write_first;
    size_t first, last, refs, reads;
    unsigned long long weight;
    int reg;
} Slot;

typedef struct {
    size_t slot, first, last;
    unsigned long long weight;
    bool fl;
} Candidate;

static int candidate_compare(const void *x, const void *y) {
    const Candidate *l = (const Candidate *)x, *r = (const Candidate *)y;
    if (l->first != r->first) return l->first < r->first ? -1 : 1;
    return l->slot < r->slot ? -1 : l->slot > r->slot;
}

typedef struct {
    const char *text;
    size_t *start, *length;
    size_t count, capacity;
} Lines;

static bool lines_add(Lines *l, size_t start, size_t length) {
    if (l->count == l->capacity) {
        size_t cap = l->capacity ? l->capacity * 2 : 4096;
        size_t *s = (size_t *)realloc(l->start, cap * sizeof *s);
        size_t *n;
        if (!s) return false;
        l->start = s;
        n = (size_t *)realloc(l->length, cap * sizeof *n);
        if (!n) return false;
        l->length = n;
        l->capacity = cap;
    }
    l->start[l->count] = start;
    l->length[l->count++] = length;
    return true;
}

static bool line_is(const Lines *l, size_t i, const char *literal) {
    size_t n = strlen(literal);
    return l->length[i] == n && memcmp(l->text + l->start[i], literal, n) == 0;
}

static bool line_starts(const Lines *l, size_t i, const char *prefix) {
    size_t n = strlen(prefix);
    return l->length[i] >= n && memcmp(l->text + l->start[i], prefix, n) == 0;
}

static bool is_jump(Span mn) {
    return span_is(mn, "jmp") || span_is(mn, "je") || span_is(mn, "jne") || span_is(mn, "jl") ||
           span_is(mn, "jge") || span_is(mn, "jle") || span_is(mn, "jg") || span_is(mn, "jae");
}

static unsigned long long parse_number(const char *s, size_t n) {
    unsigned long long v = 0;
    size_t k;
    for (k = 0; k < n && s[k] >= '0' && s[k] <= '9'; ++k) v = v * 10 + (unsigned)(s[k] - '0');
    return v;
}

typedef struct {
    Label *labels;
    Edge *edges;
    long *depth_delta;
    Slot *slots;
    Candidate *candidates;
    size_t label_cap, edge_cap, delta_cap, slot_cap, candidate_cap;
} Work;

static bool work_reserve(void **p, size_t *cap, size_t need, size_t size) {
    void *q;
    if (need <= *cap) return true;
    q = realloc(*p, need * size);
    if (!q) return false;
    *p = q;
    *cap = need;
    return true;
}

static bool emit_line(Buffer *out, const Lines *l, size_t i) {
    return buffer_add(out, l->text + l->start[i], l->length[i]) && buffer_add(out, "\n", 1);
}

/* Rewrites the lines [first, end) of one function; `out` receives them. */
static bool process_function(Buffer *out, const Lines *l, size_t first, size_t end, Work *w) {
    size_t i, p_sub = 0, p_alloc = 0, p_end = 0, marker = 0, p_fp = 0, p_setframe = 0;
    bool found_fp = false, found_setframe = false;
    unsigned long long plain_end = 0, lo = 1, hi = 0, frame = 0;
    size_t nlabels = 0, nedges = 0, nslots, ncand = 0, used = 0, k;
    bool found_sub = false, found_alloc = false, found_end = false, found_marker = false;
    bool safe = true;
    unsigned long long reg_weight[REG_COUNT];
    int reg_used[REG_COUNT];
    unsigned long long xmm_weight[XMM_COUNT];
    int xmm_used[XMM_COUNT];
    size_t xused = 0;
    int cls;
    Candidate *cand;
    unsigned shift, area = 0;
    unsigned long long new_frame;

    for (i = first; i < end; ++i) {
        if (!found_fp && line_is(l, i, "\tmovq %rsp, %rbp")) {
            found_fp = true;
            p_fp = i;
        } else if (!found_setframe && line_is(l, i, "\t.seh_setframe %rbp, 0")) {
            found_setframe = true;
            p_setframe = i;
        }
        if (!found_sub && line_starts(l, i, "\tsubq $") && l->length[i] > 8 &&
            memcmp(l->text + l->start[i] + l->length[i] - 6, ", %rsp", 6) == 0) {
            found_sub = true;
            p_sub = i;
            frame = parse_number(l->text + l->start[i] + 7, l->length[i] - 13);
        } else if (!found_alloc && line_starts(l, i, "\t.seh_stackalloc ")) {
            found_alloc = true;
            p_alloc = i;
        } else if (line_is(l, i, "\t.seh_endprologue")) {
            found_end = true;
            p_end = i;
            break;
        }
    }
    if (found_end && p_end + 1 < end && line_starts(l, p_end + 1, "\t# r6.slots ")) {
        const char *s = l->text + l->start[p_end + 1] + 12;
        size_t n = l->length[p_end + 1] - 12, a = 0;
        found_marker = true;
        marker = p_end + 1;
        plain_end = parse_number(s, n);
        while (a < n && s[a] != ' ') ++a;
        if (a < n) lo = parse_number(s + a + 1, n - a - 1);
        ++a;
        while (a < n && s[a] != ' ') ++a;
        if (a < n) hi = parse_number(s + a + 1, n - a - 1);
    }
    if (!found_sub || !found_alloc || !found_end || !found_marker || frame == 0 ||
        frame > 1000000 || (frame & 15) != 0 || plain_end > frame)
        safe = false;

    nslots = safe ? (size_t)(frame / 8) + 1 : 0;
    if (safe) {
        size_t body = marker + 1;
        /* Labels, jumps and the checks that make the function safe to touch. */
        if (!work_reserve((void **)&w->labels, &w->label_cap, end - first, sizeof *w->labels) ||
            !work_reserve((void **)&w->edges, &w->edge_cap, end - first, sizeof *w->edges) ||
            !work_reserve((void **)&w->depth_delta, &w->delta_cap, end - first + 2,
                          sizeof *w->depth_delta) ||
            !work_reserve((void **)&w->slots, &w->slot_cap, nslots, sizeof *w->slots) ||
            !work_reserve((void **)&w->candidates, &w->candidate_cap, nslots,
                          sizeof *w->candidates))
            return false;
        memset(w->depth_delta, 0, (end - first + 2) * sizeof *w->depth_delta);
        for (i = body; i < end; ++i) {
            const char *line = l->text + l->start[i];
            size_t len = l->length[i];
            if (len > 0 && line[0] != '\t' && line[len - 1] == ':') {
                w->labels[nlabels].name = line;
                w->labels[nlabels].len = len - 1;
                w->labels[nlabels++].line = i;
            } else if (mentions_saved_register(line, len)) {
                safe = false;
            } else if (line_is(l, i, "\tret") && !line_is(l, i - 1, "\tleave")) {
                safe = false;
            }
        }
        qsort(w->labels, nlabels, sizeof *w->labels, label_compare);
        for (i = body; i < end && safe; ++i) {
            Span mn, a, b;
            Label key, *hit;
            const char *line = l->text + l->start[i];
            size_t len = l->length[i];
            if (len < 4 || line[0] != '\t' || line[1] != 'j') continue;
            split_line(line, len, &mn, &a, &b);
            if (!is_jump(mn) || !span_starts(a, ".L")) continue;
            key.name = a.p;
            key.len = a.n;
            key.line = 0;
            hit = (Label *)bsearch(&key, w->labels, nlabels, sizeof *w->labels, label_compare);
            if (!hit) {
                safe = false;
            } else if (hit->line <= i) {
                w->edges[nedges].from = i;
                w->edges[nedges++].to = hit->line;
                w->depth_delta[hit->line - first] += 1;
                w->depth_delta[i + 1 - first] -= 1;
            }
        }
    }

    if (safe) {
        long depth = 0;
        memset(w->slots, 0, nslots * sizeof *w->slots);
        for (k = 0; k < nslots; ++k) w->slots[k].reg = -1;
        for (i = first; i < marker + 1; ++i) depth += w->depth_delta[i - first];
        for (i = marker + 1; i < end; ++i) {
            const char *line = l->text + l->start[i];
            size_t len = l->length[i];
            Ref refs[MAX_REFS];
            size_t nrefs, r;
            Use use;
            bool good;
            depth += w->depth_delta[i - first];
            if (len == 0 || line[0] != '\t') continue;
            nrefs = find_refs(line, len, refs, MAX_REFS);
            if (nrefs == 0) continue;
            if (nrefs > MAX_REFS) {
                safe = false;
                break;
            }
            good = classify_line(line, len, refs, nrefs, &use);
            for (r = 0; r < nrefs; ++r) {
                unsigned offset = refs[r].offset;
                size_t index = offset / 8;
                bool tracked;
                if (offset == 0 || offset > frame) {
                    safe = false;
                    break;
                }
                tracked = offset <= plain_end || (offset >= lo && offset <= hi);
                if (!tracked) continue;
                if (offset % 8 != 0) {
                    w->slots[index].bad = true;
                    w->slots[index + 1 <= nslots - 1 ? index + 1 : index].bad = true;
                    continue;
                }
                if (!good) {
                    w->slots[index].bad = true;
                    continue;
                }
                {
                    Slot *s = &w->slots[index];
                    int shift_depth = depth > 4 ? 4 : (int)depth;
                    if (use.cls == 1) s->fl = true;
                    else if (use.cls == 0) s->gp = true;
                    if (s->refs == 0) {
                        s->first = i;
                        s->write_first = use.write;
                    }
                    s->last = i;
                    ++s->refs;
                    if (!use.write) ++s->reads;
                    s->weight += 1ull << (3 * shift_depth);
                }
            }
            if (!safe) break;
        }
    }

    if (safe) {
        /* Intervals, in order of their first reference. A slot read before
         * any write, or written in an earlier iteration of a loop, is live
         * from the start of the function or to the end of the loop. */
        bool changed;
        for (k = 1; k < nslots; ++k) {
            Slot *s = &w->slots[k];
            if (s->refs == 0 || s->bad || s->reads == 0 || (s->fl && s->gp)) continue;
            w->candidates[ncand].fl = s->fl;
            w->candidates[ncand].slot = k;
            w->candidates[ncand].first = s->write_first ? s->first : first;
            w->candidates[ncand].last = s->last;
            w->candidates[ncand++].weight = s->weight;
        }
        cand = w->candidates;
        do {
            changed = false;
            for (i = 0; i < nedges; ++i) {
                for (k = 0; k < ncand; ++k) {
                    if (cand[k].first < w->edges[i].to && cand[k].last >= w->edges[i].to &&
                        cand[k].last < w->edges[i].from) {
                        cand[k].last = w->edges[i].from;
                        changed = true;
                    }
                }
            }
        } while (changed);
        qsort(cand, ncand, sizeof *cand, candidate_compare);
        for (cls = 0; cls < 2; ++cls) {
            int nregs = cls ? XMM_COUNT : REG_COUNT;
            int owner_of[XMM_COUNT];
            for (k = 0; k < (size_t)nregs; ++k) owner_of[k] = -1;
            for (k = 0; k < ncand; ++k) {
                int r, free_reg = -1, weakest = -1;
                if (cand[k].fl != (cls == 1)) continue;
                for (r = 0; r < nregs; ++r) {
                    int owner = owner_of[r];
                    if (owner >= 0 && cand[owner].last < cand[k].first) {
                        owner_of[r] = -1;
                        owner = -1;
                    }
                    if (owner < 0) {
                        if (free_reg < 0) free_reg = r;
                    } else if (weakest < 0 || cand[owner].weight < cand[owner_of[weakest]].weight) {
                        weakest = r;
                    }
                }
                if (free_reg >= 0) {
                    owner_of[free_reg] = (int)k;
                    w->slots[cand[k].slot].reg = free_reg;
                } else if (weakest >= 0 && cand[owner_of[weakest]].weight < cand[k].weight) {
                    w->slots[cand[owner_of[weakest]].slot].reg = -1;
                    owner_of[weakest] = (int)k;
                    w->slots[cand[k].slot].reg = weakest;
                }
            }
        }
        for (k = 0; k < REG_COUNT; ++k) {
            reg_weight[k] = 0;
            reg_used[k] = -1;
        }
        for (k = 0; k < XMM_COUNT; ++k) {
            xmm_weight[k] = 0;
            xmm_used[k] = -1;
        }
        for (k = 0; k < ncand; ++k) {
            int r = w->slots[cand[k].slot].reg;
            if (r < 0) continue;
            if (cand[k].fl) xmm_weight[r] += cand[k].weight;
            else reg_weight[r] += cand[k].weight;
        }
        for (k = 0; k < REG_COUNT; ++k) {
            if (reg_weight[k] >= MIN_REGISTER_WEIGHT) reg_used[k] = (int)used++;
        }
        if (found_fp && found_setframe) {
            for (k = 0; k < XMM_COUNT; ++k) {
                if (xmm_weight[k] >= MIN_REGISTER_WEIGHT) xmm_used[k] = (int)xused++;
            }
        }
        for (k = 0; k < ncand; ++k) {
            int r = w->slots[cand[k].slot].reg;
            if (r < 0) continue;
            if (cand[k].fl ? xmm_used[r] < 0 : reg_used[r] < 0) w->slots[cand[k].slot].reg = -1;
        }
        if (used == 0 && xused == 0) safe = false;
    }

    if (!safe) {
        for (i = first; i < end; ++i)
            if (!(found_marker && i == marker) && !emit_line(out, l, i)) return false;
        return true;
    }

    area = (unsigned)(xused * 16);
    shift = (unsigned)(used * 8) + area;
    new_frame = frame + ((shift + frame) % 16 ? 8 : 0);
    for (i = first; i < end; ++i) {
        const char *line = l->text + l->start[i];
        size_t len = l->length[i];
        Ref refs[MAX_REFS];
        size_t nrefs, r, at = 0;
        if (i == marker) continue;
        if (xused && i == p_fp) {
            if (!buffer_format(out, "\tsubq $%u, %%rsp\n\t.seh_xmmarea %u\n", area, area) ||
                !buffer_format(out, "\tleaq %u(%%rsp), %%rbp\n", area))
                return false;
            continue;
        }
        if (xused && i == p_setframe) {
            size_t x;
            if (!buffer_format(out, "\t.seh_setframe %%rbp, %u\n", area)) return false;
            for (x = 0; x < XMM_COUNT; ++x) {
                if (xmm_used[x] < 0) continue;
                if (!buffer_format(out, "\tmovaps %s, %d(%%rsp)\n\t.seh_savexmm %s, %d\n",
                                   XMM_NAMES[x], xmm_used[x] * 16, XMM_NAMES[x], xmm_used[x] * 16))
                    return false;
            }
            continue;
        }
        if (i == p_sub) {
            for (r = 0; r < REG_COUNT; ++r) {
                if (reg_used[r] < 0) continue;
                if (!buffer_format(out, "\tpushq %s\n\t.seh_pushreg %s\n", REG_NAMES[r][0],
                                   REG_NAMES[r][0]))
                    return false;
            }
            if (!buffer_format(out, "\tsubq $%llu, %%rsp\n", new_frame)) return false;
            continue;
        }
        if (i == p_alloc) {
            if (!buffer_format(out, "\t.seh_stackalloc %llu\n", new_frame)) return false;
            continue;
        }
        if (line_is(l, i, "\tleave")) {
            for (r = 0; r < XMM_COUNT; ++r) {
                if (xmm_used[r] < 0) continue;
                if (!buffer_format(out, "\tmovaps -%u(%%rbp), %s\n", area - (unsigned)xmm_used[r] * 16,
                                   XMM_NAMES[r]))
                    return false;
            }
            for (r = 0; r < REG_COUNT; ++r) {
                if (reg_used[r] < 0) continue;
                if (!buffer_format(out, "\tmovq -%u(%%rbp), %s\n", area + (unsigned)(reg_used[r] + 1) * 8,
                                   REG_NAMES[r][0]))
                    return false;
            }
        }
        nrefs = len > 0 && line[0] == '\t' && i > p_end ? find_refs(line, len, refs, MAX_REFS) : 0;
        if (nrefs == 0) {
            if (!emit_line(out, l, i)) return false;
            continue;
        }
        /* D-M1-82: a store to a slot that nothing reads. */
        if (nrefs == 1 && refs[0].offset % 8 == 0 && refs[0].offset / 8 < nslots &&
            w->slots[refs[0].offset / 8].refs > 0 && w->slots[refs[0].offset / 8].reads == 0 &&
            !w->slots[refs[0].offset / 8].bad)
            continue;
        for (r = 0; r < nrefs; ++r) {
            unsigned offset = refs[r].offset;
            int reg = -1;
            bool fl = false;
            if (offset % 8 == 0 && offset / 8 < nslots) {
                reg = w->slots[offset / 8].reg;
                fl = w->slots[offset / 8].fl;
            }
            if (reg >= 0 && fl && at == 0 && len > 7 && memcmp(line, "\tmovsd ", 7) == 0) {
                if (!buffer_add(out, "\tmovaps", 7)) return false;
                at = 6;
            }
            if (!buffer_add(out, line + at, refs[r].at - at)) return false;
            if (reg >= 0) {
                Use use;
                const char *name;
                if (!classify_line(line, len, refs, nrefs, &use)) return false;
                name = fl ? XMM_NAMES[reg]
                          : REG_NAMES[reg][use.width == 64 ? 0 : use.width == 32 ? 1 : 2];
                if (!buffer_add(out, name, strlen(name))) return false;
            } else if (!buffer_format(out, "-%u(%%rbp)", offset + shift)) {
                return false;
            }
            at = refs[r].at + refs[r].len;
        }
        if (!buffer_add(out, line + at, len - at) || !buffer_add(out, "\n", 1)) return false;
    }
    return true;
}

bool r6_regalloc_win64(const char *path, char *error, size_t error_capacity) {
    FILE *file = fopen(path, "rb");
    char *text = NULL;
    size_t size, at = 0, line_start = 0;
    long end;
    Lines lines;
    Buffer out = {NULL, 0, 0};
    Work work;
    bool in_function = false;
    size_t function_first = 0;

    memset(&lines, 0, sizeof lines);
    memset(&work, 0, sizeof work);
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
    lines.text = text;
    while (at < size) {
        const char *nl = (const char *)memchr(text + at, '\n', size - at);
        size_t length = nl ? (size_t)(nl - (text + at)) : size - at;
        line_start = at;
        if (!lines_add(&lines, line_start, length)) goto oom;
        at += length + (nl ? 1 : 0);
    }
    for (at = 0; at < lines.count; ++at) {
        if (!in_function) {
            if (line_starts(&lines, at, "\t.seh_proc ")) {
                in_function = true;
                function_first = at;
            } else if (!emit_line(&out, &lines, at)) {
                goto oom;
            }
            continue;
        }
        if (line_is(&lines, at, "\t.seh_endproc")) {
            if (!process_function(&out, &lines, function_first, at + 1, &work)) goto oom;
            in_function = false;
        }
    }
    if (in_function) {
        snprintf(error, error_capacity, "unterminated .seh_proc in generated assembler text");
        goto fail;
    }
    file = fopen(path, "wb");
    if (!file || fwrite(out.p, 1, out.n, file) != out.n || fclose(file) != 0) {
        snprintf(error, error_capacity, "cannot write rewritten assembler text");
        goto fail;
    }
    free(text);
    free(out.p);
    free(lines.start);
    free(lines.length);
    free(work.labels);
    free(work.edges);
    free(work.depth_delta);
    free(work.slots);
    free(work.candidates);
    return true;
oom:
    snprintf(error, error_capacity, "out of memory allocating registers");
fail:
    free(text);
    free(out.p);
    free(lines.start);
    free(lines.length);
    free(work.labels);
    free(work.edges);
    free(work.depth_delta);
    free(work.slots);
    free(work.candidates);
    return false;
}
