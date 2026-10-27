#include "asm_win64.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

typedef struct { uint8_t *p; size_t n, cap; } Bytes;
typedef struct { char name[160]; int sec; uint32_t off; int global; int symbol; } Label;
/* D-M1-68: the callee-saved registers a function pushes after `movq %rsp,
 * %rbp`, in prologue order, with the offset where each push ends. */
#define MAX_SAVED_REGS 7
typedef struct {
    char name[160];
    uint32_t begin, end, push_end, fp_end, alloc_end, alloc;
    uint8_t saved_reg[MAX_SAVED_REGS];
    uint32_t saved_end[MAX_SAVED_REGS];
    size_t saved_count;
} Function;
typedef struct { char name[160]; int sec; uint32_t off; uint16_t type; } Fixup;
/* D-M1-63: labels, procedures, fixups and globals grow on the heap, and
 * labels and globals are found through an open-addressing hash of their
 * names, so a module the size of the compiler (1038 functions) assembles in
 * one pass without a fixed table. */
typedef struct { char (*names)[160]; size_t n, cap; int32_t *slots; size_t nslots; } NameSet;
typedef struct {
    R6CoffObject *obj;
    Bytes sec[5];
    Label *labels; size_t nlabels, label_cap;
    int32_t *label_slots; size_t label_nslots;
    Function *funcs; size_t nfuncs, func_cap; int active_func;
    Fixup *relocs; size_t nrelocs, reloc_cap;
    NameSet globals;
    char *error; size_t error_cap;
    int section;
} Asm;

static uint64_t name_hash(const char *s) {
    uint64_t h = 1469598103934665603ull;
    while (*s) { h ^= (unsigned char)*s++; h *= 1099511628211ull; }
    return h;
}

/* Grows *p (elements of `size` bytes) so it holds at least `need`. */
static bool grow_array(void **p, size_t *cap, size_t need, size_t size) {
    size_t next = *cap ? *cap : 256;
    void *q;
    if (need <= *cap) return true;
    while (next < need) { if (next > SIZE_MAX / 2 / size) return false; next *= 2; }
    q = realloc(*p, next * size);
    if (!q) return false;
    *p = q; *cap = next; return true;
}

/* Rebuilds a slot table of indexes into `names` (stride `stride` bytes, name
 * first) with twice the entries as slots; -1 marks an empty slot. */
static bool rehash(int32_t **slots, size_t *nslots, const char *names, size_t stride, size_t n) {
    size_t want = 1024, i;
    int32_t *t;
    while (want < n * 2 + 2) want *= 2;
    t = (int32_t *)malloc(want * sizeof *t);
    if (!t) return false;
    for (i = 0; i < want; ++i) t[i] = -1;
    for (i = 0; i < n; ++i) {
        size_t h = (size_t)(name_hash(names + i * stride) & (want - 1));
        while (t[h] >= 0) h = (h + 1) & (want - 1);
        t[h] = (int32_t)i;
    }
    free(*slots); *slots = t; *nslots = want; return true;
}

static int32_t lookup(const int32_t *slots, size_t nslots, const char *names, size_t stride,
                      const char *name) {
    size_t h;
    if (!nslots) return -1;
    h = (size_t)(name_hash(name) & (nslots - 1));
    while (slots[h] >= 0) {
        if (strcmp(names + (size_t)slots[h] * stride, name) == 0) return slots[h];
        h = (h + 1) & (nslots - 1);
    }
    return -1;
}

static bool insert_slot(int32_t **slots, size_t *nslots, const char *names, size_t stride,
                        size_t index) {
    size_t h;
    if ((index + 1) * 2 > *nslots) return rehash(slots, nslots, names, stride, index + 1);
    h = (size_t)(name_hash(names + index * stride) & (*nslots - 1));
    while ((*slots)[h] >= 0) h = (h + 1) & (*nslots - 1);
    (*slots)[h] = (int32_t)index; return true;
}

static bool bad(Asm *a, const char *fmt, ...) {
    va_list ap;
    if (a->error && a->error_cap) {
        va_start(ap, fmt); vsnprintf(a->error, a->error_cap, fmt, ap); va_end(ap);
    }
    return false;
}

static bool reserve(Bytes *b, size_t n) {
    size_t cap = b->cap ? b->cap : 256;
    uint8_t *p;
    if (n > UINT32_MAX - b->n) return false;
    if (b->n + n <= b->cap) return true;
    while (cap < b->n + n) { if (cap > SIZE_MAX / 2) return false; cap *= 2; }
    p = (uint8_t *)realloc(b->p, cap);
    if (!p) return false;
    b->p = p; b->cap = cap; return true;
}

static bool emit(Asm *a, uint8_t v) {
    Bytes *b = &a->sec[a->section];
    if (!reserve(b, 1)) return bad(a, "assembler output exceeds section limit or memory");
    b->p[b->n++] = v; return true;
}

static bool emit32(Asm *a, uint32_t v) {
    return emit(a, (uint8_t)v) && emit(a, (uint8_t)(v >> 8)) &&
           emit(a, (uint8_t)(v >> 16)) && emit(a, (uint8_t)(v >> 24));
}

static bool emit64(Asm *a, uint64_t v) {
    return emit32(a, (uint32_t)v) && emit32(a, (uint32_t)(v >> 32));
}

static char *trim(char *s) {
    char *e;
    while (isspace((unsigned char)*s)) ++s;
    e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
    return s;
}

static bool add_label(Asm *a, const char *name, int global) {
    int32_t i = lookup(a->label_slots, a->label_nslots, (const char *)a->labels,
                       sizeof *a->labels, name);
    if (i >= 0) {
        if (a->labels[i].sec != a->section || a->labels[i].off != a->sec[a->section].n)
            return bad(a, "duplicate assembler label '%s'", name);
        a->labels[i].global |= global; return true;
    }
    if (a->nlabels >= INT32_MAX ||
        !grow_array((void **)&a->labels, &a->label_cap, a->nlabels + 1, sizeof *a->labels))
        return bad(a, "too many assembler labels");
    snprintf(a->labels[a->nlabels].name, sizeof a->labels[a->nlabels].name, "%s", name);
    a->labels[a->nlabels].sec = a->section;
    a->labels[a->nlabels].off = (uint32_t)a->sec[a->section].n;
    a->labels[a->nlabels].global = global;
    a->labels[a->nlabels].symbol = -1;
    if (!insert_slot(&a->label_slots, &a->label_nslots, (const char *)a->labels,
                     sizeof *a->labels, a->nlabels))
        return bad(a, "out of memory indexing assembler labels");
    ++a->nlabels; return true;
}

static int reg_id(const char *s, int *width, int *xmm) {
    static const char *const r64[] = {"%rax","%rcx","%rdx","%rbx","%rsp","%rbp","%rsi","%rdi",
        "%r8","%r9","%r10","%r11","%r12","%r13","%r14","%r15"};
    static const char *const r32[] = {"%eax","%ecx","%edx","%ebx","%esp","%ebp","%esi","%edi",
        "%r8d","%r9d","%r10d","%r11d","%r12d","%r13d","%r14d","%r15d"};
    static const char *const r8[] = {"%al","%cl","%dl","%bl","%spl","%bpl","%sil","%dil",
        "%r8b","%r9b","%r10b","%r11b","%r12b","%r13b","%r14b","%r15b"};
    int i;
    *xmm = 0;
    for (i = 0; i < 16; ++i) if (strcmp(s, r64[i]) == 0) { *width = 64; return i; }
    for (i = 0; i < 16; ++i) if (strcmp(s, r32[i]) == 0) { *width = 32; return i; }
    for (i = 0; i < 16; ++i) if (strcmp(s, r8[i]) == 0) { *width = 8; return i; }
    if (strncmp(s, "%xmm", 4) == 0) {
        char *end; long n = strtol(s + 4, &end, 10);
        if (!*end && n >= 0 && n < 16) { *width = 128; *xmm = 1; return (int)n; }
    }
    return -1;
}

/* `reg` marks a register used where a memory operand may stand (ModRM mod
 * 11, D-M1-68). `base` then holds the register number and `byte` tells that
 * it was spelled as an 8-bit register, which needs a REX prefix from %spl on. */
typedef struct { int base, rip, reg, byte; int32_t disp; const char *symbol; } Mem;

static bool parse_mem(Asm *a, const char *s, Mem *m) {
    char copy[256], *lp, *rp, *end;
    long long d = 0;
    if (strlen(s) >= sizeof copy) return bad(a, "memory operand too long");
    strcpy(copy, s); lp = strchr(copy, '('); rp = strchr(copy, ')');
    if (!lp || !rp || rp[1] || lp > rp) return false;
    *lp++ = 0; *rp = 0;
    if (strcmp(lp, "%rip") == 0) {
        m->disp = 0; m->symbol = NULL; m->rip = 1; m->base = -1; m->reg = 0; m->byte = 0;
        return true;
    }
    if (*trim(copy)) { errno = 0; d = strtoll(trim(copy), &end, 0); if (errno || *trim(end)) return false; }
    if (d < INT32_MIN || d > INT32_MAX) return bad(a, "memory displacement outside signed 32-bit range");
    m->disp = (int32_t)d; m->symbol = NULL; m->rip = 0; m->base = -1; m->reg = 0; m->byte = 0;
    { int w, x; m->base = reg_id(lp, &w, &x); if (m->base < 0 || x || w != 64) return false; }
    return true;
}

/* A memory operand, or a general register of `width` bits where the
 * instruction accepts either (D-M1-68). */
static bool parse_rm(Asm *a, const char *s, int width, Mem *m) {
    int w, x, id;
    if (*s != '%') return parse_mem(a, s, m);
    id = reg_id(s, &w, &x);
    if (id < 0 || x || w != width || id == 4 || id == 5) return false;
    m->base = id; m->rip = 0; m->reg = 1; m->byte = width == 8;
    m->disp = 0; m->symbol = NULL;
    return true;
}

static bool add_fixup(Asm *a, const char *name, int section, uint32_t off, uint16_t type) {
    Fixup *f;
    if (!grow_array((void **)&a->relocs, &a->reloc_cap, a->nrelocs + 1, sizeof *a->relocs))
        return bad(a, "out of memory growing assembler relocations");
    f = &a->relocs[a->nrelocs++]; snprintf(f->name, sizeof f->name, "%s", name);
    f->sec = section; f->off = off; f->type = type; return true;
}

static bool rex_modrm_mem(Asm *a, int width, int reg, Mem m, uint8_t opcode,
                          int prefix_66, int prefix_f2, int opcode2) {
    int mod, rm, rex_r = reg >> 3, rex_b = m.rip ? 0 : m.base >> 3;
    int32_t disp = m.disp;
    if (m.reg) {
        int rex = width == 64 || rex_r || rex_b || (m.byte && m.base >= 4);
        if (prefix_66 && !emit(a, 0x66)) return false;
        if (rex && !emit(a, (uint8_t)(0x40 | (width == 64 ? 8 : 0) | (rex_r ? 4 : 0) | (rex_b ? 1 : 0)))) return false;
        if (opcode2 >= 0 && !emit(a, (uint8_t)opcode2)) return false;
        return emit(a, opcode) && emit(a, (uint8_t)(0xc0 | ((reg & 7) << 3) | (m.base & 7)));
    }
    if (prefix_66 && !emit(a, 0x66)) return false;
    if (prefix_f2 && !emit(a, 0xf2)) return false;
    if (width == 64 || rex_r || rex_b) {
        if (!emit(a, (uint8_t)(0x40 | (width == 64 ? 8 : 0) | (rex_r ? 4 : 0) | (rex_b ? 1 : 0)))) return false;
    }
    if (opcode2 >= 0 && !emit(a, (uint8_t)opcode2)) return false;
    if (!emit(a, opcode)) return false;
    if (m.rip) {
        if (!emit(a, (uint8_t)(((reg & 7) << 3) | 5))) return false;
        { uint32_t at = (uint32_t)a->sec[a->section].n;
          if (!emit32(a, 0) || !add_fixup(a, m.symbol ? m.symbol : "", a->section, at, 4)) return false; }
        return true;
    }
    mod = (disp >= -128 && disp <= 127) ? 1 : 2;
    rm = m.base & 7;
    if (rm == 5 && mod == 0) mod = 1;
    if (!emit(a, (uint8_t)((mod << 6) | ((reg & 7) << 3) | (rm == 4 ? 4 : rm)))) return false;
    if (rm == 4 && !emit(a, (uint8_t)((0 << 6) | (4 << 3) | rm))) return false;
    if (mod == 1) return emit(a, (uint8_t)disp);
    return emit32(a, (uint32_t)disp);
}

static bool split_operands(char *s, char **left, char **right) {
    char *comma = strchr(s, ',');
    if (!comma) return false;
    *comma = 0; *left = trim(s); *right = trim(comma + 1);
    return **left && **right && !strchr(*right, ',');
}

static bool parse_imm(const char *s, uint64_t *v) {
    char *end;
    if (*s != '$') return false;
    errno = 0; *v = strtoull(s + 1, &end, 0);
    return !errno && end != s + 1 && !*end;
}

/* The registers the allocator may hold values in: rbx, rsi, rdi, r12-r15. */
static bool saved_register(int id) {
    return id == 3 || id == 6 || id == 7 || (id >= 12 && id <= 15);
}

static bool encode(Asm *a, char *line) {
    /* Near conditional jumps: 0F 8x rel32, signed and unsigned conditions. */
    static const struct { const char *name; uint8_t opcode; } jcc[] = {
        {"jae", 0x83}, {"je", 0x84}, {"jne", 0x85}, {"jl", 0x8c},
        {"jge", 0x8d}, {"jle", 0x8e}, {"jg", 0x8f}};
    size_t j;
    char *sp = line, *l, *r, *op, *a1, *a2;
    int w1, x1, id1, w2, x2, id2;
    uint64_t imm;
    Mem m;
    while (*sp && !isspace((unsigned char)*sp)) ++sp;
    if (*sp) *sp++ = 0;
    op = line; a1 = trim(sp); a2 = NULL;
    { uint32_t start = (uint32_t)a->sec[a->section].n; bool ok;
      ok = false;
      /* Prologue offsets are captured from the actual encoded instruction bytes. */
      (void)start; (void)ok; }
    if (!*a1) a1 = NULL;
    if (strcmp(op, "pushq") == 0 && a1 && strcmp(a1, "%rbp") == 0) return emit(a, 0x55);
    if (strcmp(op, "pushq") == 0 && a1) {
        id1 = reg_id(a1, &w1, &x1);
        if (id1 < 0 || x1 || w1 != 64 || !saved_register(id1))
            return bad(a, "pushq supports %%rbp and the callee-saved registers only");
        return (id1 < 8 || emit(a, 0x41)) && emit(a, (uint8_t)(0x50 + (id1 & 7)));
    }
    if (strcmp(op, "leave") == 0) return emit(a, 0xc9);
    if (strcmp(op, "ret") == 0) return emit(a, 0xc3);
    if (strcmp(op, "ud2") == 0) return emit(a, 0x0f) && emit(a, 0x0b);
    if (strcmp(op, "cqto") == 0) return emit(a, 0x48) && emit(a, 0x99);
    if (strcmp(op, "negl") == 0 && a1) {
        id1 = reg_id(a1, &w1, &x1);
        if (id1 < 0 || x1 || w1 != 32) return bad(a, "negl requires a 32-bit GPR");
        return (id1 < 8 || emit(a, 0x41)) && emit(a, 0xf7) &&
               emit(a, (uint8_t)(0xd8 | (id1 & 7)));
    }
    if (strcmp(op, "negq") == 0 && a1) {
        id1 = reg_id(a1, &w1, &x1);
        if (id1 < 0 || x1 || w1 != 64) return bad(a, "negq requires a 64-bit GPR");
        return emit(a, (uint8_t)(0x48 | (id1 >> 3))) && emit(a, 0xf7) &&
               emit(a, (uint8_t)(0xd8 | (id1 & 7)));
    }
    if (strcmp(op, "idivq") == 0 && a1 && parse_rm(a, a1, 64, &m))
        return rex_modrm_mem(a, 64, 7, m, 0xf7, 0, 0, -1);
    if (strcmp(op, "cltd") == 0) return emit(a, 0x99);
    if (strcmp(op, "idivl") == 0 && a1 && parse_rm(a, a1, 32, &m))
        return rex_modrm_mem(a, 32, 7, m, 0xf7, 0, 0, -1);
    if ((strcmp(op, "sete") == 0 || strcmp(op, "setnp") == 0 || strcmp(op, "setp") == 0 ||
         strcmp(op, "setne") == 0 || strcmp(op, "setl") == 0 ||
         strcmp(op, "setle") == 0 || strcmp(op, "setg") == 0 ||
         strcmp(op, "setge") == 0 || strcmp(op, "setb") == 0 ||
         strcmp(op, "setbe") == 0 || strcmp(op, "seta") == 0 ||
         strcmp(op, "setae") == 0) && a1) {
        id1 = reg_id(a1, &w1, &x1);
        if (id1 < 0 || x1 || w1 != 8 || id1 > 3) return bad(a, "setcc requires %%al, %%cl, %%dl or %%bl");
        { uint8_t cc = strcmp(op, "sete") == 0 ? 0x94 : strcmp(op, "setne") == 0 ? 0x95 :
                       strcmp(op, "setl") == 0 ? 0x9c : strcmp(op, "setge") == 0 ? 0x9d :
                       strcmp(op, "setle") == 0 ? 0x9e : strcmp(op, "setg") == 0 ? 0x9f :
                       strcmp(op, "setb") == 0 ? 0x92 : strcmp(op, "setbe") == 0 ? 0x96 :
                       strcmp(op, "seta") == 0 ? 0x97 : strcmp(op, "setae") == 0 ? 0x93 :
                       strcmp(op, "setp") == 0 ? 0x9a : 0x9b;
          return emit(a, 0x0f) && emit(a, cc) && emit(a, (uint8_t)(0xc0 | (id1 & 7))); }
    }
    if (strcmp(op, "call") == 0) {
        uint32_t off;
        if (!a1 || *a1 == '%' || strchr(a1, '(')) return bad(a, "only direct call symbols are supported");
        if (!emit(a, 0xe8)) return false;
        off = (uint32_t)a->sec[a->section].n;
        return emit32(a, 0) && add_fixup(a, a1, a->section, off, 4);
    }
    if (strcmp(op, "callq") == 0 && a1 && a1[0] == '*') {
        /* Indirect call through a function value: FF /2 r/m64. */
        id1 = reg_id(a1 + 1, &w1, &x1);
        if (id1 < 0 || x1 || w1 != 64) return bad(a, "callq requires a 64-bit GPR");
        return emit(a, (uint8_t)(0x40 | (id1 >> 3))) && emit(a, 0xff) &&
               emit(a, (uint8_t)(0xd0 | (id1 & 7)));
    }
    for (j = 0; j < sizeof jcc / sizeof jcc[0]; ++j) {
        uint32_t off;
        if (strcmp(op, jcc[j].name) != 0) continue;
        if (!a1 || !emit(a, 0x0f) || !emit(a, jcc[j].opcode)) return false;
        off = (uint32_t)a->sec[a->section].n;
        return emit32(a, 0) && add_fixup(a, a1, a->section, off, 0xfffe);
    }
    if (strcmp(op, "jmp") == 0) {
        uint32_t off;
        if (!a1 || strchr(a1, ',') || !emit(a, 0xe9)) return bad(a, "jmp requires one local label");
        off = (uint32_t)a->sec[a->section].n;
        return emit32(a, 0) && add_fixup(a, a1, a->section, off, 0xfffe);
    }
    if (!a1 || !split_operands(a1, &l, &r)) return bad(a, "unsupported Win64 assembler instruction '%s'", op);
    a1 = l; a2 = r;
    if (strcmp(op, "leaq") == 0 && parse_mem(a, a1, &m) && m.rip) {
        id2 = reg_id(a2, &w2, &x2);
        if (id2 < 0 || x2 || w2 != 64) return bad(a, "lea destination must be a 64-bit GPR");
        if (!a1 || !strchr(a1, '(')) return false;
        { char *label = a1; char *paren = strchr(label, '('); *paren = 0; m.symbol = trim(label); }
        return rex_modrm_mem(a, 64, id2, m, 0x8d, 0, 0, -1);
    }
    if (strcmp(op, "leaq") == 0 && parse_mem(a, a1, &m) && !m.rip) {
        id2 = reg_id(a2, &w2, &x2);
        if (id2 < 0 || x2 || w2 != 64) return bad(a, "lea destination must be a 64-bit GPR");
        return rex_modrm_mem(a, 64, id2, m, 0x8d, 0, 0, -1);
    }
    if (strcmp(op, "movq") == 0 && parse_mem(a, a1, &m) && m.rip) {
        id2 = reg_id(a2, &w2, &x2);
        if (id2 < 0 || x2 || w2 != 64) return bad(a, "movq from a symbol must load a 64-bit GPR");
        { char *label = a1; char *paren = strchr(label, '('); *paren = 0; m.symbol = trim(label); }
        return rex_modrm_mem(a, 64, id2, m, 0x8b, 0, 0, -1);
    }
    id1 = reg_id(a1, &w1, &x1); id2 = reg_id(a2, &w2, &x2);
    if (strcmp(op, "testq") == 0 && id1 >= 0 && id2 >= 0 && !x1 && !x2 && w1 == 64 && w2 == 64) {
        uint8_t rex = (uint8_t)(0x48 | ((id1 >> 3) << 2) | (id2 >> 3));
        return emit(a, rex) && emit(a, 0x85) &&
               emit(a, (uint8_t)(0xc0 | ((id1 & 7) << 3) | (id2 & 7)));
    }
    if ((strcmp(op, "addq") == 0 || strcmp(op, "subq") == 0 ||
         strcmp(op, "cmpq") == 0) &&
        id1 >= 0 && id2 >= 0 && !x1 && !x2 && w1 == 64 && w2 == 64) {
        uint8_t rex = (uint8_t)(0x48 | ((id1 >> 3) << 2) | (id2 >> 3));
        uint8_t opcode = strcmp(op, "addq") == 0 ? 0x01 :
                         strcmp(op, "subq") == 0 ? 0x29 : 0x39;
        return emit(a, rex) && emit(a, opcode) &&
               emit(a, (uint8_t)(0xc0 | ((id1 & 7) << 3) | (id2 & 7)));
    }
    if (strcmp(op, "imulq") == 0 && id1 >= 0 && id2 >= 0 && !x1 && !x2 && w1 == 64 && w2 == 64) {
        uint8_t rex = (uint8_t)(0x48 | ((id2 >> 3) << 2) | (id1 >> 3));
        return emit(a, rex) && emit(a, 0x0f) && emit(a, 0xaf) &&
               emit(a, (uint8_t)(0xc0 | ((id2 & 7) << 3) | (id1 & 7)));
    }
    if (strcmp(op, "movabsq") == 0) {
        if (!parse_imm(a1, &imm) || id2 != 0 || x2 || w2 != 64) return bad(a, "unsupported movabsq operands");
        return emit(a, 0x48) && emit(a, 0xb8) && emit64(a, imm);
    }
    if (strcmp(op, "movslq") == 0 && id1 >= 0 && !x1 && w1 == 32 &&
        id2 >= 0 && !x2 && w2 == 64)
        /* REX.R extends the destination and REX.B the source: without them
           `movslq %r9d, %r9` encodes `movslq %ecx, %rcx`. */
        return emit(a, (uint8_t)(0x48 | ((id2 >> 3) << 2) | (id1 >> 3))) && emit(a, 0x63) &&
               emit(a, (uint8_t)(0xc0 | ((id2 & 7) << 3) | (id1 & 7)));
    if ((strcmp(op, "movq") == 0 || strcmp(op, "movl") == 0) && parse_imm(a1, &imm) && id2 >= 0 && !x2) {
        if (w2 == 32) return (id2 < 8 || emit(a, 0x41)) &&
                         emit(a, (uint8_t)(0xb8 + (id2 & 7))) && emit32(a, (uint32_t)imm);
        if (w2 == 64 && strcmp(op, "movq") == 0 && (int64_t)(int32_t)(uint32_t)imm == (int64_t)imm) {
            Mem r = {0};
            r.base = id2; r.reg = 1;
            return rex_modrm_mem(a, 64, 0, r, 0xc7, 0, 0, -1) && emit32(a, (uint32_t)imm);
        }
        if (w2 == 64) return bad(a, "movq immediate to GPR must use movabsq in M1 assembly");
    }
    if ((strcmp(op, "movq") == 0 || strcmp(op, "movl") == 0) && parse_imm(a1, &imm) && parse_mem(a, a2, &m)) {
        int width = op[3] == 'q' ? 64 : 32;
        /* REX.W C7 /0 imm32, or C7 /0 imm32. */
        if (!rex_modrm_mem(a, width, 0, m, 0xc7, 0, 0, -1)) return false;
        return emit32(a, (uint32_t)imm);
    }
    if (strcmp(op, "movq") == 0 && id1 >= 0 && !x1 && parse_mem(a, a2, &m))
        return rex_modrm_mem(a, 64, id1, m, 0x89, 0, 0, -1);
    if (strcmp(op, "movq") == 0 && parse_mem(a, a1, &m) && id2 >= 0 && !x2)
        return rex_modrm_mem(a, 64, id2, m, 0x8b, 0, 0, -1);
    if (strcmp(op, "movl") == 0 && id1 >= 0 && !x1 && w1 == 32 && parse_rm(a, a2, 32, &m))
        return rex_modrm_mem(a, 32, id1, m, 0x89, 0, 0, -1);
    if (strcmp(op, "movl") == 0 && parse_rm(a, a1, 32, &m) && id2 >= 0 && !x2 && w2 == 32)
        return rex_modrm_mem(a, 32, id2, m, 0x8b, 0, 0, -1);
    if (strcmp(op, "movslq") == 0 && parse_mem(a, a1, &m) && id2 >= 0 && !x2 && w2 == 64)
        return rex_modrm_mem(a, 64, id2, m, 0x63, 0, 0, -1);
    if (strcmp(op, "movb") == 0 && id1 >= 0 && !x1 && w1 == 8 && id1 < 4 && parse_mem(a, a2, &m))
        return rex_modrm_mem(a, 0, id1, m, 0x88, 0, 0, -1);
    if (strcmp(op, "movb") == 0 && parse_imm(a1, &imm) && parse_mem(a, a2, &m)) {
        if (imm > 255) return bad(a, "movb immediate outside byte range");
        return rex_modrm_mem(a, 0, 0, m, 0xc6, 0, 0, -1) && emit(a, (uint8_t)imm);
    }
    if (strcmp(op, "movq") == 0 && id1 >= 0 && !x1 && id2 >= 0 && !x2) {
        uint8_t rex = (uint8_t)(0x48 | ((id1 >> 3) << 2) | (id2 >> 3));
        return emit(a, rex) && emit(a, 0x89) &&
               emit(a, (uint8_t)(0xc0 | ((id1 & 7) << 3) | (id2 & 7)));
    }
    if (strcmp(op, "movq") == 0 && id1 >= 0 && !x1 && id2 >= 0 && x2) {
        if (!emit(a, 0x66) || !emit(a, (uint8_t)(0x48 | ((id2 >> 3) << 2) | (id1 >> 3))) ||
            !emit(a, 0x0f) || !emit(a, 0x6e)) return false;
        return emit(a, (uint8_t)(0xc0 | ((id2 & 7) << 3) | (id1 & 7)));
    }
    if (strcmp(op, "movsd") == 0 && parse_mem(a, a1, &m) && id2 >= 0 && x2)
        return rex_modrm_mem(a, 0, id2, m, 0x10, 0, 1, 0x0f);
    if (strcmp(op, "movsd") == 0 && id1 >= 0 && x1 && parse_mem(a, a2, &m))
        return rex_modrm_mem(a, 0, id1, m, 0x11, 0, 1, 0x0f);
    if (strcmp(op, "movsd") == 0 && id1 >= 0 && x1 && id2 >= 0 && x2) {
        if (!emit(a, 0xf2) || !emit(a, 0x0f) || !emit(a, 0x10)) return false;
        return emit(a, (uint8_t)(0xc0 | ((id2 & 7) << 3) | (id1 & 7)));
    }
    if (strcmp(op, "xorpd") == 0 && id1 >= 0 && x1 && id2 >= 0 && x2) {
        if (!emit(a, 0x66) || !emit(a, 0x0f) || !emit(a, 0x57)) return false;
        return emit(a, (uint8_t)(0xc0 | ((id2 & 7) << 3) | (id1 & 7)));
    }
    if ((strcmp(op, "addsd") == 0 || strcmp(op, "subsd") == 0 ||
         strcmp(op, "mulsd") == 0 || strcmp(op, "divsd") == 0) &&
        parse_mem(a, a1, &m) && id2 >= 0 && x2) {
        uint8_t opcode = strcmp(op, "addsd") == 0 ? 0x58 :
                         strcmp(op, "subsd") == 0 ? 0x5c :
                         strcmp(op, "mulsd") == 0 ? 0x59 : 0x5e;
        return rex_modrm_mem(a, 0, id2, m, opcode, 0, 1, 0x0f);
    }
    if (strcmp(op, "cvtsi2sdq") == 0 && parse_mem(a, a1, &m) && id2 >= 0 && x2)
        return rex_modrm_mem(a, 64, id2, m, 0x2a, 0, 1, 0x0f);
    if (strcmp(op, "ucomisd") == 0 && parse_mem(a, a1, &m) && id2 >= 0 && x2)
        return rex_modrm_mem(a, 0, id2, m, 0x2e, 1, 0, 0x0f);
    if ((strcmp(op, "andb") == 0 || strcmp(op, "orb") == 0) && id1 >= 0 && id2 >= 0 && !x1 && !x2)
        return emit(a, strcmp(op, "andb") == 0 ? 0x20 : 0x08) && emit(a, (uint8_t)(0xc0 | ((id1 & 7) << 3) | (id2 & 7)));
    if (strcmp(op, "movzbl") == 0 && parse_rm(a, a1, 8, &m) && id2 >= 0 && !x2 && w2 == 32)
        return rex_modrm_mem(a, 0, id2, m, 0xb6, 0, 0, 0x0f);
    if (strcmp(op, "addl") == 0 && parse_rm(a, a1, 32, &m) && id2 == 0 && !x2)
        return rex_modrm_mem(a, 32, 0, m, 0x03, 0, 0, -1);
    if (strcmp(op, "subl") == 0 && parse_rm(a, a1, 32, &m) && id2 == 0 && !x2)
        return rex_modrm_mem(a, 32, 0, m, 0x2b, 0, 0, -1);
    if (strcmp(op, "imull") == 0 && parse_rm(a, a1, 32, &m) && id2 == 0 && !x2)
        return rex_modrm_mem(a, 32, 0, m, 0xaf, 0, 0, 0x0f);
    if (strcmp(op, "cmpl") == 0 && parse_rm(a, a1, 32, &m) && id2 == 0 && !x2)
        return rex_modrm_mem(a, 32, 0, m, 0x3b, 0, 0, -1);
    if (strcmp(op, "addq") == 0 && parse_mem(a, a1, &m) && id2 >= 0 && !x2 && w2 == 64)
        return rex_modrm_mem(a, 64, id2, m, 0x03, 0, 0, -1);
    if (strcmp(op, "subq") == 0 && parse_mem(a, a1, &m) && id2 >= 0 && !x2 && w2 == 64)
        return rex_modrm_mem(a, 64, id2, m, 0x2b, 0, 0, -1);
    if (strcmp(op, "imulq") == 0 && parse_mem(a, a1, &m) && id2 >= 0 && !x2 && w2 == 64)
        return rex_modrm_mem(a, 64, id2, m, 0xaf, 0, 0, 0x0f);
    if (strcmp(op, "cmpq") == 0 && parse_mem(a, a1, &m) && id2 >= 0 && !x2 && w2 == 64)
        return rex_modrm_mem(a, 64, id2, m, 0x3b, 0, 0, -1);
    if (strcmp(op, "cmpl") == 0 && parse_imm(a1, &imm) && parse_rm(a, a2, 32, &m)) {
        if (imm <= 127) return rex_modrm_mem(a, 32, 7, m, 0x83, 0, 0, -1) && emit(a, (uint8_t)imm);
        return rex_modrm_mem(a, 32, 7, m, 0x81, 0, 0, -1) && emit32(a, (uint32_t)imm);
    }
    if (strcmp(op, "cmpb") == 0 && parse_imm(a1, &imm) && parse_rm(a, a2, 8, &m)) {
        if (imm > 127) return bad(a, "cmpb immediate outside signed-byte range");
        return rex_modrm_mem(a, 8, 7, m, 0x80, 0, 0, -1) && emit(a, (uint8_t)imm);
    }
    if (strcmp(op, "cmpb") == 0 && parse_mem(a, a1, &m) && id2 == 0 && !x2 && w2 == 8)
        return rex_modrm_mem(a, 8, 0, m, 0x3a, 0, 0, -1);
    if (strcmp(op, "subq") == 0 && parse_imm(a1, &imm) && strcmp(a2, "%rsp") == 0) {
        if (imm <= 127) return emit(a, 0x48) && emit(a, 0x83) && emit(a, 0xec) && emit(a, (uint8_t)imm);
        return emit(a, 0x48) && emit(a, 0x81) && emit(a, 0xec) && emit32(a, (uint32_t)imm);
    }
    (void)w1; (void)w2; (void)id1; (void)id2; (void)x1; (void)x2; (void)imm; (void)m;
    return bad(a, "unsupported Win64 assembler operands: %s %s, %s", op, a1, a2);
}

static int find_label(const Asm *a, const char *name) {
    return (int)lookup(a->label_slots, a->label_nslots, (const char *)a->labels,
                       sizeof *a->labels, name);
}

static bool is_global(const NameSet *g, const char *name) {
    return lookup(g->slots, g->nslots, (const char *)g->names, sizeof *g->names, name) >= 0;
}

static bool add_global(NameSet *g, const char *name, Asm *a) {
    if (is_global(g, name)) return true;
    if (g->n >= INT32_MAX ||
        !grow_array((void **)&g->names, &g->cap, g->n + 1, sizeof *g->names))
        return bad(a, "too many global assembler symbols");
    snprintf(g->names[g->n], sizeof *g->names, "%s", name);
    if (!insert_slot(&g->slots, &g->nslots, (const char *)g->names, sizeof *g->names, g->n))
        return bad(a, "out of memory indexing global assembler symbols");
    ++g->n; return true;
}

static void free_asm(Asm *a) {
    size_t i;
    for (i = 0; i < 5; ++i) free(a->sec[i].p);
    free(a->labels); free(a->label_slots); free(a->funcs); free(a->relocs);
    free(a->globals.names); free(a->globals.slots);
}

static bool emit_byte_directive(Asm *a, char *s) {
    char *p = s;
    while (*p) {
        char *end; long v;
        while (isspace((unsigned char)*p) || *p == ',') ++p;
        if (!*p) break;
        errno = 0; v = strtol(p, &end, 0);
        if (errno || end == p || v < 0 || v > 255) return bad(a, "invalid .byte value");
        if (!emit(a, (uint8_t)v)) return false;
        p = end;
        while (isspace((unsigned char)*p)) ++p;
        if (*p && *p != ',') return bad(a, "invalid .byte list");
    }
    return true;
}

static bool append_u32(R6CoffObject *o, int sec, uint32_t v) {
    return r6_coff_section_u32(o, sec, v);
}

static bool resolve_local_branches(Asm *a) {
    size_t i;
    for (i = 0; i < a->nrelocs; ++i) {
        Fixup *f = &a->relocs[i];
        int li;
        int64_t rel;
        uint8_t *p;
        if (f->type != 0xfffe) continue;
        li = find_label(a, f->name);
        if (li < 0 || a->labels[li].sec != f->sec)
            return bad(a, "undefined local branch label '%s'", f->name);
        rel = (int64_t)a->labels[li].off - ((int64_t)f->off + 4);
        if (rel < INT32_MIN || rel > INT32_MAX)
            return bad(a, "branch to '%s' is out of range", f->name);
        p = a->sec[f->sec].p + f->off;
        p[0] = (uint8_t)rel; p[1] = (uint8_t)(rel >> 8);
        p[2] = (uint8_t)(rel >> 16); p[3] = (uint8_t)(rel >> 24);
    }
    return true;
}

/* Index of the object's symbols by name, for the extern lookup in
 * finalize_object. The first symbol of a name wins, as the linear scan it
 * replaces did. It is sized once for the symbols present plus one new
 * extern per fixup, so it never rehashes. */
typedef struct { int32_t *slots; size_t nslots; } SymbolIndex;

static int32_t symbol_find(const SymbolIndex *x, const R6CoffObject *o, const char *name) {
    size_t h = (size_t)(name_hash(name) & (x->nslots - 1));
    while (x->slots[h] >= 0) {
        if (strcmp(o->symbols[x->slots[h]].name, name) == 0) return x->slots[h];
        h = (h + 1) & (x->nslots - 1);
    }
    return -1;
}

static void symbol_add(SymbolIndex *x, const R6CoffObject *o, size_t index) {
    const char *name = o->symbols[index].name;
    size_t h;
    if (symbol_find(x, o, name) >= 0) return;
    h = (size_t)(name_hash(name) & (x->nslots - 1));
    while (x->slots[h] >= 0) h = (h + 1) & (x->nslots - 1);
    x->slots[h] = (int32_t)index;
}

static bool symbol_index_build(SymbolIndex *x, const R6CoffObject *o, size_t extra) {
    size_t i;
    x->nslots = 1024;
    while (x->nslots < (o->symbol_count + extra) * 2 + 2) x->nslots *= 2;
    x->slots = (int32_t *)malloc(x->nslots * sizeof *x->slots);
    if (!x->slots) return false;
    for (i = 0; i < x->nslots; ++i) x->slots[i] = -1;
    for (i = 0; i < o->symbol_count; ++i) symbol_add(x, o, i);
    return true;
}

static bool finalize_object(Asm *a) {
    int text = r6_coff_add_section(a->obj, ".text", 0x60500020u);
    int rdata = r6_coff_add_section(a->obj, ".rdata", 0x40500040u);
    int xdata = r6_coff_add_section(a->obj, ".xdata", 0x40300040u);
    int pdata = r6_coff_add_section(a->obj, ".pdata", 0x40300040u);
    int data = r6_coff_add_section(a->obj, ".data", 0xC0500040u);
    int section_ids[5] = {text, rdata, xdata, pdata, data};
    SymbolIndex index = {NULL, 0};
    size_t i, j;
    if (text < 0 || rdata < 0 || xdata < 0 || pdata < 0 || data < 0)
        return bad(a, "%s", r6_coff_error(a->obj));
    if (!resolve_local_branches(a)) return false;
    for (i = 0; i < 5; ++i) {
        if (i == 3) continue; /* .pdata is emitted through append_u32 below */
        if (!r6_coff_section_bytes(a->obj, section_ids[i], a->sec[i].p, a->sec[i].n))
            return bad(a, "%s", r6_coff_error(a->obj));
    }
    for (i = 0; i < a->nlabels; ++i) {
        Label *l = &a->labels[i];
        l->symbol = r6_coff_add_symbol(a->obj, l->name, l->sec, l->off,
            l->global ? 0x20 : 0, l->global ? 2 : 3);
        if (l->symbol < 0) return bad(a, "%s", r6_coff_error(a->obj));
    }
    for (i = 0; i < a->nfuncs; ++i) {
        Function *f = &a->funcs[i];
        char endname[192], uwname[192];
        uint8_t unwind[40]; size_t count = 0, nslots, k;
        int end_symbol, uw_symbol;
        int fi = find_label(a, f->name);
        if (fi < 0 || a->labels[fi].sec != 0 || f->end < f->begin)
            return bad(a, "function '%s' has invalid symbol range", f->name);
        snprintf(endname, sizeof endname, "__r6_end_%s", f->name);
        snprintf(uwname, sizeof uwname, "__r6_unwind_%s", f->name);
        end_symbol = r6_coff_add_symbol(a->obj, endname, 0, f->end, 0, 3);
        uw_symbol = r6_coff_add_symbol(a->obj, uwname, 2, (uint32_t)a->sec[2].n, 0, 3);
        if (end_symbol < 0 || uw_symbol < 0) return bad(a, "%s", r6_coff_error(a->obj));
        if (!f->push_end || !f->fp_end || !f->alloc_end ||
            f->alloc < 8 || f->alloc > 524280 || (f->alloc & 7))
            return bad(a, "unsupported/invalid Win64 unwind prologue for '%s'", f->name);
        unwind[count++] = 1; unwind[count++] = (uint8_t)f->alloc_end;
        unwind[count++] = 0; unwind[count++] = 5;
        if (f->alloc <= 128) {
            unwind[count++] = (uint8_t)f->alloc_end;
            unwind[count++] = (uint8_t)((((f->alloc - 8) / 8) << 4) | 2);
            nslots = 3;
        } else {
            uint16_t scaled = (uint16_t)(f->alloc / 8);
            unwind[count++] = (uint8_t)f->alloc_end; unwind[count++] = 1;
            unwind[count++] = (uint8_t)scaled; unwind[count++] = (uint8_t)(scaled >> 8);
            nslots = 4;
        }
        /* Codes run in reverse prologue order: the registers pushed after the
           frame pointer was set come before the set_fpreg code. */
        for (k = f->saved_count; k-- > 0;) {
            unwind[count++] = (uint8_t)f->saved_end[k];
            unwind[count++] = (uint8_t)((f->saved_reg[k] << 4) | 0);
            ++nslots;
        }
        unwind[count++] = (uint8_t)f->fp_end; unwind[count++] = 3;
        unwind[count++] = (uint8_t)f->push_end; unwind[count++] = 0x50;
        unwind[2] = (uint8_t)nslots;
        if (nslots & 1) { unwind[count++] = 0; unwind[count++] = 0; }
        if (!r6_coff_section_bytes(a->obj, xdata, unwind, count)) return bad(a, "%s", r6_coff_error(a->obj));
        /* PDATA start, end, and UNWIND_INFO RVA; ADDR32NB is COFF type 3. */
        for (j = 0; j < 3; ++j) if (!append_u32(a->obj, pdata, 0)) return bad(a, "%s", r6_coff_error(a->obj));
        if (!r6_coff_add_relocation(a->obj, pdata, (uint32_t)(i * 12), a->labels[fi].symbol, 3) ||
            !r6_coff_add_relocation(a->obj, pdata, (uint32_t)(i * 12 + 4), end_symbol, 3) ||
            !r6_coff_add_relocation(a->obj, pdata, (uint32_t)(i * 12 + 8), uw_symbol, 3))
            return bad(a, "%s", r6_coff_error(a->obj));
    }
    if (!symbol_index_build(&index, a->obj, a->nrelocs))
        return bad(a, "out of memory indexing COFF symbols");
    for (i = 0; i < a->nrelocs; ++i) {
        Fixup *f = &a->relocs[i];
        int sym = -1, li = find_label(a, f->name);
        if (f->type == 0xfffe) continue;
        if (li >= 0) sym = a->labels[li].symbol;
        else {
            sym = symbol_find(&index, a->obj, f->name);
            if (sym < 0) {
                sym = r6_coff_add_symbol(a->obj, f->name, -1, 0, 0x20, 2);
                if (sym >= 0) symbol_add(&index, a->obj, (size_t)sym);
            }
        }
        if (sym < 0 || !r6_coff_add_relocation(a->obj, f->sec, f->off, sym, f->type)) {
            free(index.slots);
            return bad(a, "%s", r6_coff_error(a->obj));
        }
    }
    free(index.slots);
    return true;
}

bool r6_assemble_win64(FILE *input, R6CoffObject *object, char *error, size_t error_capacity) {
    Asm a; char line[1024]; size_t lineno = 0;
    if (!input || !object) {
        if (error && error_capacity) snprintf(error, error_capacity, "null assembler input/output");
        return false;
    }
    memset(&a, 0, sizeof a); a.obj = object; a.error = error; a.error_cap = error_capacity;
    a.active_func = -1; a.section = 0;
    if (error && error_capacity) error[0] = 0;
    r6_coff_init(object);
    while (fgets(line, sizeof line, input)) {
        char *s, *p; size_t len; char original[1024]; ++lineno;
        if (!strchr(line, '\n') && !feof(input)) { bad(&a, "assembler line %zu exceeds 1023 bytes", lineno); goto fail; }
        s = trim(line); if (!*s) continue;
        if (strcmp(s, ".text") == 0) { a.section = 0; continue; }
        if (strncmp(s, ".section .rdata", 15) == 0) { a.section = 1; continue; }
        if (strncmp(s, ".section .data", 14) == 0) { a.section = 4; continue; }
        if (strncmp(s, ".globl ", 7) == 0) { if (!add_global(&a.globals, trim(s + 7), &a)) goto fail; continue; }
        if (strncmp(s, ".def ", 5) == 0 || strcmp(s, ".seh_endprologue") == 0 ||
            strncmp(s, ".seh_pushreg ", 13) == 0 || strcmp(s, ".seh_setframe %rbp, 0") == 0) continue;
        if (strncmp(s, ".seh_proc ", 10) == 0) {
            if (a.active_func >= 0) { bad(&a, "invalid/nested .seh_proc at line %zu", lineno); goto fail; }
            if (!grow_array((void **)&a.funcs, &a.func_cap, a.nfuncs + 1, sizeof *a.funcs)) { bad(&a, "out of memory growing procedures"); goto fail; }
            memset(&a.funcs[a.nfuncs], 0, sizeof *a.funcs);
            a.active_func = (int)a.nfuncs++;
            snprintf(a.funcs[a.active_func].name, sizeof a.funcs[a.active_func].name, "%s", trim(s + 10));
            a.funcs[a.active_func].begin = (uint32_t)a.sec[0].n; continue;
        }
        if (strncmp(s, ".seh_stackalloc ", 16) == 0) {
            if (a.active_func < 0) { bad(&a, ".seh_stackalloc outside procedure"); goto fail; }
            a.funcs[a.active_func].alloc = (uint32_t)strtoul(s + 16, NULL, 10); continue;
        }
        if (strcmp(s, ".seh_endproc") == 0) {
            if (a.active_func < 0) { bad(&a, ".seh_endproc without procedure"); goto fail; }
            a.funcs[a.active_func].end = (uint32_t)a.sec[0].n; a.active_func = -1; continue;
        }
        if (strncmp(s, ".byte ", 6) == 0) { if (!emit_byte_directive(&a, s + 6)) goto fail; continue; }
        len = strlen(s);
        if (len && s[len - 1] == ':') {
            s[len - 1] = 0;
            if (!add_label(&a, s, is_global(&a.globals, s))) goto fail;
            continue;
        }
        snprintf(original, sizeof original, "%s", s);
        p = s;
        if (!encode(&a, p)) { if (error && error_capacity && error[0]) {
                char detail[256]; snprintf(detail, sizeof detail, "%s", error);
                snprintf(error, error_capacity, "line %zu: %s", lineno, detail);
            } goto fail; }
        if (a.active_func >= 0 && a.section == 0) {
            Function *f = &a.funcs[a.active_func];
            uint32_t off = (uint32_t)a.sec[0].n;
            if (strcmp(original, "pushq %rbp") == 0) f->push_end = off - f->begin;
            else if (strncmp(original, "pushq %", 7) == 0) {
                int w, x, id = reg_id(original + 6, &w, &x);
                if (f->saved_count >= MAX_SAVED_REGS) { bad(&a, "too many callee-saved pushes"); goto fail; }
                f->saved_reg[f->saved_count] = (uint8_t)id;
                f->saved_end[f->saved_count++] = off - f->begin;
            }
            else if (strcmp(original, "movq %rsp, %rbp") == 0) f->fp_end = off - f->begin;
            else if (strncmp(original, "subq $", 6) == 0) f->alloc_end = off - f->begin;
        }
    }
    if (ferror(input)) { bad(&a, "error reading assembler input"); goto fail; }
    if (a.active_func >= 0) { bad(&a, "unterminated .seh_proc"); goto fail; }
    if (!finalize_object(&a)) goto fail;
    free_asm(&a);
    return true;
fail:
    free_asm(&a);
    r6_coff_free(object);
    return false;
}

