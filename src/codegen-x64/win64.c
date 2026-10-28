#include "win64.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Each IR register lives in an 8-byte frame slot here. D-M1-66 lets a branch
 * read the flags of the comparison before it. The later text stages,
 * `peephole.c` (D-M1-67) and `regalloc.c` (D-M1-68), rewrite the finished
 * asm. No semantic fallback is performed. */

static bool fail(R6Diagnostic *d, const R6Instruction *ins, const char *code,
                 const char *fmt, ...) {
    va_list ap;
    if (d) {
        d->line = ins ? ins->line : 0;
        d->column = ins ? ins->column : 0;
        snprintf(d->code, sizeof d->code, "%s", code);
        va_start(ap, fmt);
        vsnprintf(d->message, sizeof d->message, fmt, ap);
        va_end(ap);
    }
    return false;
}

static const char *type_at(const R6Function *f, int32_t reg) {
    if (reg < 0 || (size_t)reg >= f->register_type_count) return NULL;
    return f->register_types[reg];
}

static bool is_int(const char *t) { return t && strcmp(t, "int") == 0; }
static bool is_float(const char *t) { return t && strcmp(t, "float") == 0; }
static bool int_widens_to_float(const char *arg_type, const char *param_type) {
    return is_int(arg_type) && is_float(param_type);
}
static bool is_string(const char *t) { return t && strcmp(t, "string") == 0; }
static bool is_bool(const char *t) { return t && strcmp(t, "bool") == 0; }
static bool is_list(const char *t) { return t && strcmp(t, "list") == 0; }
static bool is_result(const char *t) { return t && strcmp(t, "result") == 0; }
static size_t hidden_slots(const R6Function *f, bool is_main);
static bool is_void(const char *t) { return t && strcmp(t, "void") == 0; }
static bool is_void_ptr(const char *t) { return t && strcmp(t, "void*") == 0; }
/* D-M1-108: the opaque OrbitArena* of a runtime module; the loader admits the
   spelling in runtime modules only. */
static bool is_rtarena(const char *t) { return t && strcmp(t, "RtArena") == 0; }
static bool is_scalar(const char *t) {
    return is_int(t) || is_float(t) || is_string(t) || is_bool(t);
}
static bool is_register_type(const char *t) {
    return is_scalar(t) || is_list(t) || is_void(t) || is_result(t);
}
static bool same_type(const char *a, const char *b) {
    return a && b && strcmp(a, b) == 0;
}

/* D-M1-32: the pinned C type map (c_backend.orb:478-499) answers the same
 * pointer for every list spelling - `list`, `List`, and any `list<...>` at any
 * depth - because the arguments name what is IN the list, not what the pointer
 * is. R6's list register type is that pointer, so the ABI positions
 * (parameter types, return type, register types) and the model field types
 * (D-M1-39) are rewritten to "list" before any check reads them. A `map<...>` keeps its spelling: R6 has no map
 * type and refuses it. The replaced string is freed here because the module
 * owns it and r6_module_free frees whatever it holds. */
static char *canonical_list_spelling(char *t) {
    const char *base = NULL;
    size_t n;
    char *out;
    if (!t) return t;
    if (strcmp(t, "List") == 0) base = "list";
    else if (strncmp(t, "list<", 5) == 0 || strncmp(t, "List<", 5) == 0) {
        n = strlen(t);
        if (n > 5 && t[n - 1] == '>') base = "list";
    }
    if (!base) return t;
    out = malloc(strlen(base) + 1);
    if (!out) return t; /* keep the spelling: the register gate fails closed */
    strcpy(out, base);
    free(t);
    return out;
}

static void canonicalize_list_types(R6Module *m) {
    size_t k, j;
    for (k = 0; k < m->function_count; ++k) {
        R6Function *f = &m->functions[k];
        if (f->param_types)
            for (j = 0; j < f->param_count; ++j)
                f->param_types[j] = canonical_list_spelling(f->param_types[j]);
        f->return_type = canonical_list_spelling(f->return_type);
        if (f->register_types)
            for (j = 0; j < f->register_type_count; ++j)
                f->register_types[j] = canonical_list_spelling(f->register_types[j]);
    }
    if (m->models)
        for (k = 0; k < m->model_count; ++k)
            if (m->models[k].fields)
                for (j = 0; j < m->models[k].field_count; ++j)
                    m->models[k].fields[j].type_name =
                        canonical_list_spelling(m->models[k].fields[j].type_name);
}

/* D-M1-35: an enumeration is an int. The pinned backend emits
 * `typedef enum { Name_TAG_Vi = i }` in declaration order plus `Name_COUNT`
 * (c_backend.orb:1692-1706), and its verifier does not warn when a comparison
 * mixes an enum with an `int` or another enum (:1362-1363). A field of an enum
 * type is 8 bytes with alignment 8, the size `model_field_size` gives an int.
 * R6 rewrites every *use* of an enumeration name to "int" before any check
 * reads it, and keeps the type table entry, which is what resolves
 * `Name_TAG_V` to its index. The type name itself is not rewritten: it is the
 * key of that lookup. */
static bool is_enum_type(const R6Module *m, const char *t) {
    size_t n;
    if (!m || !t || !m->types) return false;
    for (n = 0; n < m->type_count; ++n)
        if (m->types[n].name && m->types[n].kind &&
            strcmp(m->types[n].kind, "enumeration") == 0 &&
            strcmp(m->types[n].name, t) == 0)
            return true;
    return false;
}

/* D-M1-47: a declared union is `typedef struct Name Name;` in the pin and
 * `Name*` in every value position (mapTypeToC, c_backend.orb:524-526): the
 * same pointer word D-M1-12 already gives a union value, typed void*. The
 * type table keeps the name, which resolves `Name_TAG_Variant`. */
static char *canonical_union_spelling(const R6Module *m, char *t) {
    size_t n = 0;
    char *out;
    if (!m || !t) return t;
    /* D-M1-59: the builder types the `NULL` literal's register "NULL", which
       mapTypeToC declares `void*` (c_backend.orb:472). */
    if (strcmp(t, "NULL") != 0 && strcmp(t, "object") != 0) {
        if (!m->types) return t;
        for (n = 0; n < m->type_count; ++n)
            if (m->types[n].name && m->types[n].kind &&
                strcmp(m->types[n].kind, "union_type") == 0 &&
                strcmp(m->types[n].name, t) == 0)
                break;
        if (n == m->type_count) return t;
    }
    out = malloc(sizeof "void*");
    if (!out) return t; /* keep the spelling: the register gate fails closed */
    strcpy(out, "void*");
    free(t);
    return out;
}

static void canonicalize_union_types(R6Module *m) {
    size_t k, j;
    /* D-M1-56: a union-typed model field is the same cell word. */
    for (k = 0; m->models && k < m->model_count; ++k)
        for (j = 0; m->models[k].fields && j < m->models[k].field_count; ++j)
            m->models[k].fields[j].type_name =
                canonical_union_spelling(m, m->models[k].fields[j].type_name);
    for (k = 0; m->functions && k < m->function_count; ++k) {
        R6Function *f = &m->functions[k];
        if (f->param_types)
            for (j = 0; j < f->param_count; ++j)
                f->param_types[j] = canonical_union_spelling(m, f->param_types[j]);
        f->return_type = canonical_union_spelling(m, f->return_type);
        if (f->register_types)
            for (j = 0; j < f->register_type_count; ++j)
                f->register_types[j] = canonical_union_spelling(m, f->register_types[j]);
    }
}

/* D-M1-50: mapTypeToC answers OrbitResult for `result`, `Result` and
 * `OrbitResult` alike (c_backend.orb:502); the capitalised spellings are the
 * same type. */
static char *canonical_result_spelling(char *t) {
    char *out;
    if (!t || (strcmp(t, "Result") != 0 && strcmp(t, "OrbitResult") != 0)) return t;
    out = malloc(sizeof "result");
    if (!out) return t; /* keep the spelling: the register gate fails closed */
    strcpy(out, "result");
    free(t);
    return out;
}

static void canonicalize_result_types(R6Module *m) {
    size_t k, j;
    for (k = 0; m->functions && k < m->function_count; ++k) {
        R6Function *f = &m->functions[k];
        if (f->param_types)
            for (j = 0; j < f->param_count; ++j)
                f->param_types[j] = canonical_result_spelling(f->param_types[j]);
        f->return_type = canonical_result_spelling(f->return_type);
        if (f->register_types)
            for (j = 0; j < f->register_type_count; ++j)
                f->register_types[j] = canonical_result_spelling(f->register_types[j]);
    }
}

static char *canonical_enum_spelling(const R6Module *m, char *t) {
    char *out;
    if (!is_enum_type(m, t)) return t;
    out = malloc(sizeof "int");
    if (!out) return t; /* keep the spelling: the register gate fails closed */
    strcpy(out, "int");
    free(t);
    return out;
}

static void canonicalize_enum_types(R6Module *m) {
    size_t k, j;
    /* A module the loader cannot produce still fails closed: the absent
     * arrays are diagnosed later, and the rewrite skips them here. */
    for (k = 0; m->functions && k < m->function_count; ++k) {
        R6Function *f = &m->functions[k];
        if (f->param_types)
            for (j = 0; j < f->param_count; ++j)
                f->param_types[j] = canonical_enum_spelling(m, f->param_types[j]);
        f->return_type = canonical_enum_spelling(m, f->return_type);
        if (f->register_types)
            for (j = 0; j < f->register_type_count; ++j)
                f->register_types[j] = canonical_enum_spelling(m, f->register_types[j]);
    }
    if (m->models)
        for (k = 0; k < m->model_count; ++k)
            if (m->models[k].fields)
                for (j = 0; j < m->models[k].field_count; ++j)
                    m->models[k].fields[j].type_name =
                        canonical_enum_spelling(m, m->models[k].fields[j].type_name);
    if (m->globals)
        for (k = 0; k < m->global_count; ++k)
            m->globals[k].type_name = canonical_enum_spelling(m, m->globals[k].type_name);
}

/* D-M1-31: the pinned comparison arms (c_backend.orb:2747-2814) pick three
 * lowerings. Two strings compare with strcmp; either side float promotes the
 * pair to double; anything else compares the two 64-bit slots as words. The
 * relational ops have no string arm. A string against a number is the pin's
 * word fallthrough (`n == "5"` is false in tests/suite/peephole_folds.orb).
 * Requirement: float still needs both sides typed float, because the emitter
 * has no int-to-double conversion. */
static bool is_null_symbol(const R6Value *v) {
    return v->kind == R6_VAL_SYMBOL && v->as.string && strcmp(v->as.string, "NULL") == 0;
}

/* D-M1-59: eq/ne between two pointer words (a list, a model, a union cell or
 * NULL) compare the words, `(uintptr_t)a == (uintptr_t)b` in the pin
 * (c_backend.orb eq/ne arms). The lowering is the 64-bit cmpq already used for
 * ints. */
static bool word_compare_ok(const R6Module *m, const char *a, const char *b);

static bool comparison_types_ok(const char *a, const char *b, bool allow_string) {
    bool a_num = is_int(a) || is_bool(a);
    bool b_num = is_int(b) || is_bool(b);
    if (is_string(a) || is_string(b))
        return allow_string &&
               ((is_string(a) && is_string(b)) ||
                (is_string(a) && is_int(b)) || (is_string(b) && is_int(a)));
    if (is_float(a) || is_float(b)) return is_float(a) && is_float(b);
    return a_num && b_num;
}

/* D-M1-91: add, sub and mul over words. The pin computes them as
 * `(uintptr_t)a OP (uintptr_t)b` and stores the result with the cast of the
 * destination register. An `int` and a `void*` are both 64 bits (D-M1-94), so
 * the result is the whole word either way. At least one operand is a word, so
 * the all-int case stays with the typed rule. */
static bool word_arith_ok(const char *a, const char *b, const char *dest) {
    return (is_void_ptr(a) || is_void_ptr(b)) &&
           (is_int(a) || is_void_ptr(a)) && (is_int(b) || is_void_ptr(b)) &&
           (is_int(dest) || is_void_ptr(dest));
}

static const R6Function *find_function(const R6Module *m, const char *name);

/* D-M1-27: the runtime functions whose C signature carries the arena as an
 * implicit first parameter (c_backend.orb:4416-4437, `functionNeedsArena`).
 * The IR declaration the builder emits does not repeat it, so a call to one of
 * these externs must prepend the executable arena or it lands on the wrong
 * ABI. The set is copied verbatim; every other name is called as declared. */
/* D-M1-78: the runtime getters of a parsed JSON object take and return an
 * `OrbitObject*`, which the IR spells `object` and R6 carries as a pointer
 * word. The set is the 11 externs of std/json/json.orb. */
static bool extern_takes_object(const char *name) {
    static const char *const names[] = {
        "orbit_object_get_int", "orbit_object_get_float", "orbit_object_get_bool",
        "orbit_object_get_string", "orbit_object_get_object", "orbit_json_array_len",
        "orbit_json_array_get_int", "orbit_json_array_get_float",
        "orbit_json_array_get_bool", "orbit_json_array_get_string",
        "orbit_json_array_get_object"
    };
    size_t n;
    for (n = 0; name && n < sizeof names / sizeof names[0]; ++n)
        if (strcmp(name, names[n]) == 0) return true;
    return false;
}

/* D-M1-86: the creator and the six setters of IR_SPEC 3.3. The loader
 * canonicalises the IR type `object` to `void*` (the `OrbitObject*` word), so
 * these externs declare `void*` and, for set_list, a list. */
static bool extern_object_symbol(const char *name) {
    static const char *const names[] = {
        "orbit_object_create", "orbit_object_set_int", "orbit_object_set_float",
        "orbit_object_set_bool", "orbit_object_set_string", "orbit_object_set_object",
        "orbit_object_set_list"
    };
    size_t n;
    for (n = 0; name && n < sizeof names / sizeof names[0]; ++n)
        if (strcmp(name, names[n]) == 0) return true;
    return false;
}

/* IR_SPEC 1.4: the destination type names the getter. */
static const char *object_getter(const char *dest_type) {
    if (is_int(dest_type)) return "orbit_object_get_int";
    if (is_float(dest_type)) return "orbit_object_get_float";
    if (is_bool(dest_type)) return "orbit_object_get_bool";
    if (is_string(dest_type)) return "orbit_object_get_string";
    if (is_void_ptr(dest_type)) return "orbit_object_get_object";
    if (is_list(dest_type)) return "orbit_object_get_list";
    return NULL;
}

static bool extern_needs_arena(const char *name) {
    static const char *const names[] = {
        "orbit_file_read", "orbit_file_list_dir", "orbit_response_header_line",
        "orbit_list_create", "orbit_map_create", "orbit_object_create",
        "orbit_object_to_json", "orbit_response_create", "orbit_response_json",
        "orbit_response_error", "orbit_response_html", "orbit_string_slice",
        "orbit_string_split", "orbit_string_replace", "orbit_string_concat",
        "orbit_os_exec", "orbit_os_env", "orbit_os_argv", "orbit_random_bytes",
        "orbit_json_parse", "orbit_int_to_string", "orbit_float_to_string",
        "orbit_http_query_get", "orbit_http_query_get_all",
        "orbit_http_header_get", "orbit_http_param_get", "orbit_http_body_get",
        "orbit_db_get", "orbit_db_all", "orbit_db_where", "orbit_db_first",
        "orbit_auth_bearer_token", "orbit_auth_role", "orbit_auth_current_role",
        "orbit_auth_has_role", "orbit_db_query_all", "orbit_db_query_where",
        "orbit_db_query_where_p", "orbit_db_query_where_v", "orbit_db_query_get",
        "orbit_db_query_first", "orbit_db_query_order", "orbit_db_query_order_desc",
        "orbit_db_count_all", "orbit_db_count_v", "orbit_http_client_fetch",
        "orbit_cache_get", "orbit_file_upload_save", "orbit_base64url_encode_str",
        "orbit_sha256_hex", "orbit_hmac_sha256_base64url",
        /* D-M1-108: the arena-first export of tests/runtime-module, the only way
           an ordinary program obtains an RtArena. */
        "rtmod_probe_lower", "rtmod_probe_copy_in",
        "rtmod_probe_slice"
    };
    size_t n;
    for (n = 0; n < sizeof names / sizeof names[0]; ++n)
        if (strcmp(name, names[n]) == 0) return true;
    return false;
}

/* D-M1-29: the runtime string helpers the language emits as calls without
 * declaring them. The pinned C backend includes the runtime headers, so these
 * names always resolve to the C prototypes; the IR carries only the
 * program-visible parameters (the arena is not repeated) and no declaration.
 * Signatures copied from runtime/orbit_api.h: len :411-413, at :385,
 * slice :386. `orbit_string_len` is ORBIT_INLINE in that header and has no
 * exported symbol, so it is lowered inline in emit_function instead of called.
 * A name the module does declare is resolved from the declaration, never
 * here. */
static char *intrinsic_string[] = { (char *)"string" };
static char *intrinsic_string_int[] = { (char *)"string", (char *)"int" };
static char *intrinsic_string_int_int[] = { (char *)"string", (char *)"int",
                                            (char *)"int" };
static char *intrinsic_string_string[] = { (char *)"string", (char *)"string" };

static R6Function r6_intrinsics[] = {
    { .name = (char *)"orbit_string_len", .param_count = 1,
      .param_types = intrinsic_string, .return_type = (char *)"int",
      .is_extern = true },
    { .name = (char *)"orbit_string_at", .param_count = 2,
      .param_types = intrinsic_string_int, .return_type = (char *)"int",
      .is_extern = true },
    { .name = (char *)"orbit_string_slice", .param_count = 3,
      .param_types = intrinsic_string_int_int, .return_type = (char *)"string",
      .is_extern = true },
    /* D-M1-60: `orbit_int orbit_string_indexOf(orbit_string, orbit_string)`
       (orbit_api.h:413). The pin renames `orbit_string_find`, and `find` or
       `indexOf` when no user function has the name, to it
       (c_backend.orb:3001-3006), so all four call the one symbol. */
    { .name = (char *)"orbit_string_indexOf", .param_count = 2,
      .param_types = intrinsic_string_string, .return_type = (char *)"int",
      .is_extern = true },
    { .name = (char *)"orbit_string_find", .param_count = 2,
      .param_types = intrinsic_string_string, .return_type = (char *)"int",
      .is_extern = true },
    { .name = (char *)"find", .param_count = 2,
      .param_types = intrinsic_string_string, .return_type = (char *)"int",
      .is_extern = true },
    { .name = (char *)"indexOf", .param_count = 2,
      .param_types = intrinsic_string_string, .return_type = (char *)"int",
      .is_extern = true },
};

/* The symbol an intrinsic call lands on (D-M1-60). */
static const char *intrinsic_symbol(const char *name) {
    if (strcmp(name, "orbit_string_find") == 0 || strcmp(name, "find") == 0 ||
        strcmp(name, "indexOf") == 0)
        return "orbit_string_indexOf";
    return name;
}

static const R6Function *find_intrinsic(const char *name) {
    size_t n;
    for (n = 0; n < sizeof r6_intrinsics / sizeof r6_intrinsics[0]; ++n)
        if (strcmp(r6_intrinsics[n].name, name) == 0) return &r6_intrinsics[n];
    return NULL;
}

/* A call whose declared target takes the arena: only the IR's own declaration
 * (or, for an undeclared intrinsic, the table above) makes it one, and the
 * arena is prepended only for that enumerated set. */
static bool call_needs_arena(const R6Module *m, const char *name) {
    const R6Function *fn = find_function(m, name);
    if (!fn) fn = find_intrinsic(name);
    return fn && fn->is_extern && extern_needs_arena(name);
}

/* ORBIT_ELEM_* codes carried in elemKind for the list opcodes. Kinds 0
 * (opaque) and 5 (ref) are outside the M1 native surface and stay
 * fail-closed; 1..4 map 1:1 to the register type of a slot value. */
static const char *elem_kind_type(int kind) {
    switch (kind) {
    case 1: return "string";
    case 2: return "int";
    case 3: return "float";
    case 4: return "bool";
    default: return NULL;
    }
}

static bool kind_matches_type(int kind, const char *type) {
    const char *want = elem_kind_type(kind);
    return want != NULL && same_type(want, type);
}

static bool is_model_type(const R6Module *m, const char *t);

/* D-M1-57: a ref element (elemKind 5) is a pointer word: a list, a model or a
 * union cell. The pin reads it with orbit_list_read_ref and writes it with
 * orbit_list_push_ref/orbit_list_set_ref (c_backend.orb:605-662). */
static bool ref_elem_type(const R6Module *m, const char *type) {
    return is_list(type) || is_void_ptr(type) || is_model_type(m, type);
}

/* D-M1-59: a register whose only definition is `load_const NULL`. The pin
 * returns it as `NULL` from a function of any pointer type. */
static bool null_register(const R6Function *f, const R6Value *v) {
    size_t pc, defs = 0;
    bool null_def = false;
    if (v->kind != R6_VAL_REG) return false;
    for (pc = 0; pc < f->instruction_count; ++pc) {
        const R6Instruction *i = &f->instructions[pc];
        if (!i->has_dest || i->dest != v->as.i32) continue;
        ++defs;
        null_def = i->opcode == R6_OP_LOAD_CONST && is_null_symbol(&i->operands[0]);
    }
    return defs == 1 && null_def;
}

static bool word_compare_ok(const R6Module *m, const char *a, const char *b) {
    return ref_elem_type(m, a) && ref_elem_type(m, b);
}

static bool elem_matches(const R6Module *m, int kind, const char *type) {
    if (kind == 5) return ref_elem_type(m, type);
    return kind_matches_type(kind, type);
}

/* D-M1-58: an untyped (elemKind 0) element is the bare word the pin moves
 * through `uintptr_t` (orbit_list_read_word and the `_tmp` of orbit_list_push,
 * c_backend.orb:605-640). An int is its sign-extended value, which is the R6
 * slot; a string and a pointer word are the word itself. bool and float keep
 * out: their conversions through the word are not measured. */
static bool opaque_word_type(const R6Module *m, const char *type) {
    return is_int(type) || is_string(type) || ref_elem_type(m, type);
}

static bool valid_symbol(const char *s) {
    size_t i;
    if (!s || !(s[0] == '_' || (s[0] >= 'A' && s[0] <= 'Z') ||
                (s[0] >= 'a' && s[0] <= 'z'))) return false;
    for (i = 1; s[i]; ++i)
        if (!(s[i] == '_' || (s[i] >= 'A' && s[i] <= 'Z') ||
              (s[i] >= 'a' && s[i] <= 'z') || (s[i] >= '0' && s[i] <= '9')))
            return false;
    return true;
}

static bool operand_reg(const R6Function *f, const R6Instruction *i,
                        unsigned n, const char **type, R6Diagnostic *d) {
    R6Value v = i->operands[n];
    if (v.kind != R6_VAL_REG || !type_at(f, v.as.i32))
        return fail(d, i, "CGEN", "%s operand %u must be a valid register",
                    r6_opcode_name(i->opcode), n + 1);
    *type = type_at(f, v.as.i32);
    return true;
}

static bool value_type(const R6Function *f, const R6Value *v,
                       const char **type) {
    switch (v->kind) {
    case R6_VAL_INT: *type = "int"; return true;
    case R6_VAL_FLOAT64: *type = "float"; return true;
    case R6_VAL_STRING: *type = "string"; return true;
    case R6_VAL_BOOL: *type = "bool"; return true;
    case R6_VAL_REG:
        *type = type_at(f, v->as.i32);
        return *type != NULL;
    default: return false;
    }
}

/* D-M1-21: the two forms a value operand may take in the admitted
 * operations - a register with a known type, or the inline literal the
 * pinned builder writes in that position. Only Int/String/Bool are admitted
 * (the builder never inlines a Float into an operation, so one stays fail
 * closed) and every other kind still goes through operand_reg(). */
static bool operand_value(const R6Function *f, const R6Instruction *i,
                          unsigned n, const char **type, R6Diagnostic *d) {
    R6Value v = i->operands[n];
    if (v.kind == R6_VAL_FLOAT64)
        return fail(d, i, "CGEN",
                    "%s operand %u is a float literal: the pinned builder never inlines one into an operation",
                    r6_opcode_name(i->opcode), n + 1);
    if (v.kind == R6_VAL_INT || v.kind == R6_VAL_STRING || v.kind == R6_VAL_BOOL)
        return value_type(f, &v, type);
    return operand_reg(f, i, n, type, d);
}

/* The type of a value operand, register or inline literal. Validation has
 * already admitted the form, so on a reached emit path this is never NULL. */
static const char *operand_type_of(const R6Function *f, const R6Instruction *i,
                                   unsigned n) {
    const char *type = NULL;
    value_type(f, &i->operands[n], &type);
    return type;
}

static const char *ir_name(const R6Value *v) {
    if (v->kind == R6_VAL_SYMBOL || v->kind == R6_VAL_STRING) return v->as.string;
    return NULL;
}

static const R6Global *find_global(const R6Module *m, const char *name) {
    size_t n;
    if (!m || !name) return NULL;
    for (n = 0; n < m->global_count; ++n)
        if (m->globals[n].name && strcmp(m->globals[n].name, name) == 0)
            return &m->globals[n];
    return NULL;
}

/* In the pinned builder, top-level int/bool literals are serialized as
 * IRValue.String decimal text and c_backend.orb turns them into #defines. */
static bool global_declared_type(const char *t) {
    return t && t[0] && strcmp(t, "unknown") != 0;
}

static bool global_integer(const R6Global *g, int64_t *value) {
    char *end;
    long long parsed;
    const char *p;
    if (!g || !g->name || !valid_symbol(g->name) ||
        (global_declared_type(g->type_name) &&
         !is_int(g->type_name) && !is_bool(g->type_name)) ||
        g->value.kind != R6_VAL_STRING || !g->value.as.string ||
        !g->value.string_len || strlen(g->value.as.string) != g->value.string_len)
        return false;
    p = g->value.as.string;
    if (*p == '-') ++p;
    if (!*p) return false;
    for (; *p; ++p) if (*p < '0' || *p > '9') return false;
    /* long is 32 bits on this target: the parse has to be 64 bits wide. */
    errno = 0;
    parsed = strtoll(g->value.as.string, &end, 10);
    if (errno == ERANGE || *end || end == g->value.as.string)
        return false;
    if (is_bool(g->type_name) && parsed != 0 && parsed != 1) return false;
    *value = (int64_t)parsed;
    return true;
}

/* D-M1-33: a top-level constant carries no type of its own. c_backend.orb:398-401
 * writes `#define NAME <text>` for every global and :1647-1651 renders the
 * value's String payload verbatim, so the text is the type: a decimal integer
 * text is an int. The builder's "unknown" (what it writes when it cannot tell,
 * the same convention as a register type) defers to the text. A declared type
 * wins, and a text that is not a decimal integer stays untyped. */
static const char *global_type(const R6Global *g) {
    int64_t ignored;
    if (!g) return NULL;
    if (global_declared_type(g->type_name)) return g->type_name;
    return global_integer(g, &ignored) ? "int" : NULL;
}

/* D-M1-64: the locals of one function, found by name through an open
 * addressing table instead of a scan of every earlier decl_var. The frame
 * helpers ask for the local count of the same function once per instruction,
 * so the table is built once per function and kept until another function
 * asks. `first_pc[k]` is the instruction of the first decl_var that names
 * the k-th distinct local. */
typedef struct {
    const R6Function *f;
    size_t count;
    size_t *first_pc;
    int32_t *slots;
    size_t nslots;
} LocalTable;

static LocalTable local_table;

static uint64_t name_hash(const char *s) {
    uint64_t h = 1469598103934665603ull;
    while (*s) { h ^= (unsigned char)*s++; h *= 1099511628211ull; }
    return h;
}

static void local_table_release(void) {
    free(local_table.first_pc);
    free(local_table.slots);
    memset(&local_table, 0, sizeof local_table);
}

static int32_t local_table_find(const R6Function *f, const char *name) {
    size_t h;
    if (!local_table.nslots) return -1;
    h = (size_t)(name_hash(name) & (local_table.nslots - 1));
    while (local_table.slots[h] >= 0) {
        const char *candidate = ir_name(&f->instructions[local_table.first_pc[local_table.slots[h]]].operands[0]);
        if (strcmp(candidate, name) == 0) return local_table.slots[h];
        h = (h + 1) & (local_table.nslots - 1);
    }
    return -1;
}

static bool local_table_build(const R6Function *f) {
    size_t pc, declared = 0, i;
    if (local_table.f == f) return true;
    local_table_release();
    for (pc = 0; pc < f->instruction_count; ++pc)
        if (f->instructions[pc].opcode == R6_OP_DECL_VAR && ir_name(&f->instructions[pc].operands[0]))
            ++declared;
    local_table.nslots = 16;
    while (local_table.nslots < declared * 2 + 2) local_table.nslots *= 2;
    local_table.first_pc = (size_t *)malloc((declared ? declared : 1) * sizeof *local_table.first_pc);
    local_table.slots = (int32_t *)malloc(local_table.nslots * sizeof *local_table.slots);
    if (!local_table.first_pc || !local_table.slots) { local_table_release(); return false; }
    for (i = 0; i < local_table.nslots; ++i) local_table.slots[i] = -1;
    for (pc = 0; pc < f->instruction_count; ++pc) {
        const R6Instruction *ins = &f->instructions[pc];
        const char *name = ir_name(&ins->operands[0]);
        size_t h;
        if (ins->opcode != R6_OP_DECL_VAR || !name) continue;
        if (local_table_find(f, name) >= 0) continue;
        h = (size_t)(name_hash(name) & (local_table.nslots - 1));
        while (local_table.slots[h] >= 0) h = (h + 1) & (local_table.nslots - 1);
        local_table.slots[h] = (int32_t)local_table.count;
        local_table.first_pc[local_table.count++] = pc;
    }
    local_table.f = f;
    return true;
}

/* The distinct locals the function declares. Callers build the table for
 * `f` first (local_table_build); a function it was not built for counts as
 * having none, which the callers' own build call rules out. */
static size_t local_count(const R6Function *f) {
    return local_table.f == f ? local_table.count : 0;
}

/* D-M1-65: functions by name through an open addressing table built once per
 * module, so a call does not scan the 1038 functions of the compiler. The
 * first function of a name wins, as the scan it replaces did. */
typedef struct {
    int32_t *slots;
    size_t nslots;
} FunctionTable;

static FunctionTable function_table;

static void function_table_release(void) {
    free(function_table.slots);
    memset(&function_table, 0, sizeof function_table);
}

static bool function_table_build(const R6Module *m) {
    size_t n, i;
    function_table_release();
    function_table.nslots = 16;
    while (function_table.nslots < m->function_count * 2 + 2) function_table.nslots *= 2;
    function_table.slots = (int32_t *)malloc(function_table.nslots * sizeof *function_table.slots);
    if (!function_table.slots) { function_table.nslots = 0; return false; }
    for (i = 0; i < function_table.nslots; ++i) function_table.slots[i] = -1;
    for (n = 0; n < m->function_count; ++n) {
        const char *name = m->functions[n].name;
        size_t h;
        bool present = false;
        if (!name) continue;
        h = (size_t)(name_hash(name) & (function_table.nslots - 1));
        while (function_table.slots[h] >= 0) {
            if (strcmp(m->functions[function_table.slots[h]].name, name) == 0) { present = true; break; }
            h = (h + 1) & (function_table.nslots - 1);
        }
        if (!present) function_table.slots[h] = (int32_t)n;
    }
    return true;
}

static const R6Function *find_function(const R6Module *m, const char *name) {
    size_t n, h;
    if (function_table.nslots) {
        h = (size_t)(name_hash(name) & (function_table.nslots - 1));
        while (function_table.slots[h] >= 0) {
            const R6Function *f = &m->functions[function_table.slots[h]];
            if (strcmp(f->name, name) == 0) return f;
            h = (h + 1) & (function_table.nslots - 1);
        }
        return NULL;
    }
    for (n = 0; n < m->function_count; ++n)
        if (m->functions[n].name && strcmp(m->functions[n].name, name) == 0)
            return &m->functions[n];
    return NULL;
}

/* D-M1-99: every function the module defines is emitted as `o_<name>`, the
 * spelling the language's C backend gives it, so that no user name meets a
 * runtime, CRT or Win32 symbol. The entry keeps `orbit_main`, which the
 * wrapper `main` calls. Externs keep their own names. The assembler's label
 * is 160 bytes and prefixes `__r6_unwind_` to it, so a name over
 * R6_USER_NAME_MAX is refused instead of truncated. */
#define R6_USER_NAME_MAX 140
#define R6_SYMBOL_CAP (R6_USER_NAME_MAX + 8)
static const char *user_symbol(const R6Module *m, const char *name, char *buf, size_t cap) {
    const R6Function *fn = find_function(m, name);
    if (!fn || fn->is_extern) return name;
    if (fn->is_export) return name;
    if (strcmp(name, "main") == 0) return "orbit_main";
    snprintf(buf, cap, "o_%s", name);
    return buf;
}

/* D-M1-65: a function is emitted when main reaches it. Roots are the
 * non-extern main and any route handler. An edge is a call target or a
 * Symbol operand that names a function, which is how a function value is
 * bound (load_const, decl_var, store_var, arg). Externs are never emitted,
 * so their bodies add no edges. Every function is still validated. */
static bool mark_live_functions(R6Module *m) {
    size_t *stack;
    size_t top = 0, n;
    stack = (size_t *)malloc((m->function_count ? m->function_count : 1) * sizeof *stack);
    if (!stack) return false;
    for (n = 0; n < m->function_count; ++n) {
        R6Function *f = &m->functions[n];
        f->live = false;
        if (!f->is_extern && (f->has_route || f->is_export || strcmp(f->name, "main") == 0)) {
            f->live = true;
            stack[top++] = n;
        }
    }
    while (top) {
        const R6Function *f = &m->functions[stack[--top]];
        size_t pc;
        for (pc = 0; pc < f->instruction_count; ++pc) {
            const R6Instruction *i = &f->instructions[pc];
            unsigned k;
            for (k = 0; k < 3; ++k) {
                const R6Value *v = &i->operands[k];
                const R6Function *target;
                R6Function *mutable_target;
                bool names_function = v->kind == R6_VAL_SYMBOL ||
                    (v->kind == R6_VAL_STRING && i->opcode == R6_OP_CALL && k == 0);
                if (!names_function || !v->as.string) continue;
                target = find_function(m, v->as.string);
                if (!target || target->live) continue;
                mutable_target = &m->functions[target - m->functions];
                mutable_target->live = true;
                if (!mutable_target->is_extern) stack[top++] = (size_t)(target - m->functions);
            }
        }
    }
    free(stack);
    return true;
}

/* Function values carry the pinned builder's signature spelling
 * `fn(p1;...;pn)->ret` (fnTypeSpellingFor, builder.orb:2202-2215); a bare
 * function name in value position binds it (buildCallArgValue,
 * builder.orb:1245-1252) and the call is checked against the spelling
 * (buildFnVarCall, builder.orb:3646-3662). Only the corpus-measured scalar
 * signatures are admitted: scalar parameters, one `;`-separated token each,
 * and a scalar return. Everything else fails closed. */
#define R6_FN_SPELLING_CAP 2048

static bool is_fn_type(const char *t) {
    return t && strncmp(t, "fn(", 3) == 0;
}

static bool fn_scalar_token(const char *s, size_t len) {
    return (len == 3 && memcmp(s, "int", 3) == 0) ||
           (len == 4 && memcmp(s, "bool", 4) == 0) ||
           (len == 5 && memcmp(s, "float", 5) == 0) ||
           (len == 6 && memcmp(s, "string", 6) == 0);
}

/* Validates a spelling and copies its return token to `ret`; a zero-parameter
 * signature is `fn()->ret`. NULL reports a shape outside the measured subset. */
static bool fn_type_ok(const char *t, char *ret, size_t cap, size_t *param_count) {
    const char *arrow, *p;
    size_t n = 0;
    if (!t || strncmp(t, "fn(", 3) != 0) return false;
    arrow = strstr(t + 3, ")->");
    if (!arrow || (arrow > t + 3 && arrow[-1] == ';')) return false;
    for (p = t + 3; p < arrow;) {
        const char *semi = memchr(p, ';', (size_t)(arrow - p));
        const char *end = semi ? semi : arrow;
        if (!fn_scalar_token(p, (size_t)(end - p))) return false;
        ++n;
        if (!semi) break;
        p = end + 1;
    }
    if (!fn_scalar_token(arrow + 3, strlen(arrow + 3))) return false;
    if (strlen(arrow + 3) + 1 > cap) return false;
    strcpy(ret, arrow + 3);
    if (param_count) *param_count = n;
    return true;
}

/* k-th parameter token; only called on spellings fn_type_ok accepted. */
static bool fn_type_param(const char *t, size_t k, char *out, size_t cap) {
    const char *arrow = strstr(t + 3, ")->"), *p;
    size_t n = 0;
    if (!arrow) return false;
    for (p = t + 3; p < arrow;) {
        const char *semi = memchr(p, ';', (size_t)(arrow - p));
        const char *end = semi ? semi : arrow;
        if (n == k) {
            size_t len = (size_t)(end - p);
            if (len + 1 > cap) return false;
            memcpy(out, p, len);
            out[len] = 0;
            return true;
        }
        ++n;
        if (!semi) break;
        p = end + 1;
    }
    return false;
}

/* The spelling fnTypeSpellingFor would mint for this module function. `main`
 * is an entry point, never a value (the pinned sema rejects it, n77). */
static bool fn_value_spelling(const R6Module *m, const char *name, char *out, size_t cap) {
    const R6Function *fn = find_function(m, name);
    size_t i, at = 0;
    int n;
    if (!fn || !fn->name || !valid_symbol(fn->name) || !fn->return_type ||
        strcmp(fn->name, "main") == 0)
        return false;
    n = snprintf(out, cap, "fn(");
    if (n < 0 || (size_t)n >= cap) return false;
    at = (size_t)n;
    for (i = 0; i < fn->param_count; ++i) {
        if (!fn->param_types || !fn->param_types[i]) return false;
        n = snprintf(out + at, cap - at, "%s%s", i ? ";" : "", fn->param_types[i]);
        if (n < 0 || (size_t)n >= cap - at) return false;
        at += (size_t)n;
    }
    n = snprintf(out + at, cap - at, ")->%s", fn->return_type);
    return n >= 0 && (size_t)n < cap - at;
}

/* A Symbol value in load_const/decl_var names a module function: the symbol
 * must resolve, must not be `main`, must spell a scalar signature, and (when
 * `want` is not NULL) the spelling must equal the expected register type. */
static bool fn_value_bind(const R6Module *m, const R6Instruction *i,
                          const char *sym, const char *want,
                          char *out, size_t cap, R6Diagnostic *d) {
    const R6Function *fn = find_function(m, sym);
    char ret[16];
    if (!fn || !fn->name || !valid_symbol(fn->name))
        return fail(d, i, "CGEN", "function value %s is not declared in the module",
                    sym ? sym : "<null>");
    if (strcmp(fn->name, "main") == 0)
        return fail(d, i, "CGEN", "main is an entry point and cannot be used as a function value");
    if (!fn_value_spelling(m, sym, out, cap) || !fn_type_ok(out, ret, sizeof ret, NULL))
        return fail(d, i, "CGEN", "function value %s has a signature outside the M1 native subset", sym);
    if (want && !same_type(out, want))
        return fail(d, i, "CGEN", "function value %s is a %s, not %s", sym, out, want);
    return true;
}

/* Spec 2.1 records that IRv1 has no dedicated constructor for bool and float
 * literals: the builder renders them as raw `Sym` text ("true", "1.5"). A
 * fraction or an exponent is required, so a bare integer never reads as a
 * float. */
static bool float_literal_text(const char *s) {
    bool fraction = false, exponent = false;
    if (!s || !*s) return false;
    if (*s == '-') ++s;
    if (*s < '0' || *s > '9') return false;
    while (*s >= '0' && *s <= '9') ++s;
    if (*s == '.') {
        fraction = true;
        ++s;
        if (*s < '0' || *s > '9') return false;
        while (*s >= '0' && *s <= '9') ++s;
    }
    if (*s == 'e' || *s == 'E') {
        exponent = true;
        ++s;
        if (*s == '+' || *s == '-') ++s;
        if (*s < '0' || *s > '9') return false;
        while (*s >= '0' && *s <= '9') ++s;
    }
    return *s == '\0' && (fraction || exponent);
}

/* A Symbol in load_const is a function value (D-M1-14) or one of the two
 * literal forms spec 2.1 renders as raw text (D-M1-19). The pinned corpus is
 * unambiguous - all 412 load_const Symbols land in a register typed bool
 * (318), float (93) or fn(...) (1) - so the text grammar decides. Returns 2
 * (float, `out` filled), 1 (bool, `out` filled), 0 (function value) or -1
 * (numeric text whose value is not finite, which spec V-08 forbids). */
static int sym_literal_kind(const R6Value *v, R6Value *out) {
    const char *s;
    char *end;
    double d;
    if (v->kind != R6_VAL_SYMBOL || !v->as.string) return 0;
    s = v->as.string;
    if (strcmp(s, "true") == 0 || strcmp(s, "false") == 0) {
        memset(out, 0, sizeof *out);
        out->kind = R6_VAL_BOOL;
        out->as.boolean = (s[0] == 't');
        return 1;
    }
    if (!float_literal_text(s)) return 0;
    d = strtod(s, &end);
    if (*end) return 0;              /* not fully numeric: a function value */
    if (!isfinite(d)) return -1;     /* spec V-08: floats must be finite */
    memset(out, 0, sizeof *out);
    out->kind = R6_VAL_FLOAT64;
    memcpy(&out->as.f64_bits, &d, sizeof d);
    return 2;
}

/* A parameter is loadable by name. The pinned builder emits `arg` to bind each
 * parameter and reads it in the body with `load_var <name>`; the C backend
 * types that read from `paramTypes` after matching `func.params`
 * (c_backend.orb:777-802) and the `arg` binding emits the parameter itself
 * (:2860-2873), so a `load_var` of a parameter is that same value. Parameters
 * resolve before locals, in the backend's order (D-M1-22). */
static bool param_info(const R6Function *f, const char *name, size_t *index,
                       const char **type) {
    size_t p;
    if (!f || !name || !f->param_count || !f->params || !f->param_types) return false;
    for (p = 0; p < f->param_count; ++p) {
        if (f->params[p] && strcmp(f->params[p], name) == 0) {
            if (index) *index = p;
            if (type) *type = f->param_types[p];
            return true;
        }
    }
    return false;
}

/* Locals mirror decl_var order. A Symbol initializer (`val f = inc`) types
 * the local with the function's signature spelling (fn_value_spelling). */
static bool local_info(const R6Module *m, const R6Function *f, const char *name,
                       size_t *index, const char **type, char *buf, size_t cap) {
    const R6Instruction *i;
    const char *t;
    int32_t k;
    if (!name || local_table.f != f) return false;
    k = local_table_find(f, name);
    if (k < 0) return false;
    i = &f->instructions[local_table.first_pc[k]];
    if (i->operands[1].kind == R6_VAL_SYMBOL) {
        char ret[16];
        if (!fn_value_spelling(m, i->operands[1].as.string, buf, cap) ||
            !fn_type_ok(buf, ret, sizeof ret, NULL)) return false;
        t = buf;
    } else if (!value_type(f, &i->operands[1], &t)) {
        return false;
    }
    if (index) *index = (size_t)k;
    if (type) *type = t;
    return true;
}

static const R6Model *find_model(const R6Module *m, const char *name) {
    size_t n;
    if (!m || !name) return NULL;
    for (n = 0; n < m->model_count; ++n)
        if (m->models[n].name && strcmp(m->models[n].name, name) == 0)
            return &m->models[n];
    return NULL;
}

static bool is_model_type(const R6Module *m, const char *t) {
    return t && find_model(m, t) != NULL;
}

/* M1 model fields cover the scalar storage the pinned C backend synthesizes
 * for a model struct: orbit_int (4/4), orbit_bool (1/1), orbit_float (8/8)
 * and orbit_string (8/8 pointer). Anything else stays fail-closed. */
/* D-M1-39: an OrbitList* member (c_backend.orb:478). D-M1-56: a model member
 * is its `Name*` and a union member its cell, both one pointer word. */
static bool field_is_word(const R6Module *m, const char *t) {
    return is_list(t) || is_void_ptr(t) || is_model_type(m, t);
}

/* D-M1-84: the C backend declares a register it cannot type as `void*` and
 * converts at the use with casts through `uintptr_t`. A word in R6 is the
 * 64 bits of an `orbit_int` or a pointer, so the conversions that need no
 * instruction are int to pointer, pointer to int and pointer to pointer
 * (D-M1-94). A bool, float or result is not a
 * pointer word and has no measured conversion. */
static bool is_union_name(const R6Module *m, const char *t);
static bool pointer_word_type(const R6Module *m, const char *t) {
    return is_string(t) || is_list(t) || is_void_ptr(t) || is_model_type(m, t) ||
           is_union_name(m, t);
}

static bool word_convertible(const R6Module *m, const char *from, const char *to) {
    if (!from || !to) return false;
    if (same_type(from, to) || int_widens_to_float(from, to)) return true;
    if (is_void_ptr(to)) return is_int(from) || pointer_word_type(m, from);
    if (is_void_ptr(from)) return is_int(to) || pointer_word_type(m, to);
    return false;
}

static bool model_field_size(const R6Module *m, const char *t, size_t *size, size_t *align) {
    if (is_int(t)) { *size = 8; *align = 8; return true; }
    if (is_bool(t)) { *size = 1; *align = 1; return true; }
    if (is_float(t) || is_string(t)) { *size = 8; *align = 8; return true; }
    if (field_is_word(m, t)) { *size = 8; *align = 8; return true; }
    return false;
}

/* C layout of a synthesized model struct: fields in declaration order with
 * natural alignment, struct size rounded up to the widest member (sizeof(M)
 * at the pin). Returns the offset of field `want` and the total size; the
 * total is exact for any `want` in [0, field_count]. */
static bool model_field_layout(const R6Module *m, const R6Model *model, size_t want,
                               size_t *offset, size_t *total) {
    size_t i, off = 0, align_max = 1, want_off = 0;
    for (i = 0; i < model->field_count; ++i) {
        size_t size, align;
        if (!model_field_size(m, model->fields[i].type_name, &size, &align))
            return false;
        off = (off + align - 1) / align * align;
        if (i == want) want_off = off;
        off += size;
        if (align > align_max) align_max = align;
    }
    *offset = want_off;
    *total = (off + align_max - 1) / align_max * align_max;
    return true;
}

/* The union declaring a variant name, or NULL. The loader rejects a variant
 * name declared by more than one union, so this is unambiguous. */
static const R6Type *find_union_variant(const R6Module *m, const char *variant,
                                        size_t *variant_index) {
    size_t n, v;
    if (!m || !variant) return NULL;
    for (n = 0; n < m->type_count; ++n) {
        const R6Type *t = &m->types[n];
        if (!t->kind || strcmp(t->kind, "union_type") != 0) continue;
        for (v = 0; v < t->variant_count; ++v)
            if (t->variants[v] && strcmp(t->variants[v], variant) == 0) {
                if (variant_index) *variant_index = v;
                return t;
            }
    }
    return NULL;
}

/* D-M1-38: an opaque read (elemKind 0) is orbit_list_read_word, a bare word
 * the pinned backend keeps in a uintptr_t register and converts where it is
 * used. R6 needs the type at the read, so it takes it from the one use the
 * conversion is a C parameter conversion: the word is the only reader of an
 * `arg` that a direct call to a declared function consumes, with no label or
 * jump in between. Only string (the pointer bits) and int (the word) are
 * measured; every other use leaves the read untyped. */
static const char *bound_local_type(const R6Function *f, size_t at);

static const char *opaque_read_type(const R6Module *m, const R6Function *f, size_t at) {
    int32_t reg = f->instructions[at].dest;
    size_t pc, use = f->instruction_count, above = 0;
    for (pc = 0; pc < f->instruction_count; ++pc) {
        const R6Instruction *i = &f->instructions[pc];
        unsigned k;
        if (pc != at && i->has_dest && i->dest == reg) return NULL;
        for (k = 0; k < 3; ++k)
            if (i->operands[k].kind == R6_VAL_REG && i->operands[k].as.i32 == reg) {
                if (use != f->instruction_count) return NULL;
                use = pc;
            }
    }
    if (use == f->instruction_count || use <= at) return NULL;
    /* D-M1-89: `val b = bs.get(i)` binds the word to a local. The C declares
     * the local `uintptr_t` and reads it back as the type of the loads; the
     * word takes that type when it is an int or a string. */
    if (f->instructions[use].opcode == R6_OP_DECL_VAR) {
        const char *bound = bound_local_type(f, at);
        return is_int(bound) || is_string(bound) ? bound : NULL;
    }
    /* D-M1-90: the word is the receiver of a list read (the C casts it to
     * `const OrbitList*`) or an operand of int arithmetic whose result is an
     * int (the C adds `uintptr_t` words and casts to `orbit_int`; the
     * word is the int). */
    {
        const R6Instruction *u = &f->instructions[use];
        if ((u->opcode == R6_OP_LIST_GET || u->opcode == R6_OP_LIST_LEN) &&
            u->operands[0].kind == R6_VAL_REG && u->operands[0].as.i32 == reg)
            return "list";
        if ((u->opcode == R6_OP_ADD || u->opcode == R6_OP_SUB || u->opcode == R6_OP_MUL) &&
            u->has_dest && is_int(type_at(f, u->dest)))
            return "int";
    }
    /* D-M1-51: a comparison with an int. The pin compares the word as
     * uintptr_t (eq/ne) or intptr_t (lt/le/gt/ge), and an int element is
     * stored as the 64-bit value, so the word compares the same way. A
     * comparison with a string compares pointers in the pin and stays
     * unknown. */
    switch (f->instructions[use].opcode) {
    case R6_OP_EQ: case R6_OP_NE: case R6_OP_LT: case R6_OP_LE:
    case R6_OP_GT: case R6_OP_GE: {
        const R6Instruction *u = &f->instructions[use];
        const R6Value *other = (u->operands[0].kind == R6_VAL_REG &&
                                u->operands[0].as.i32 == reg) ? &u->operands[1] : &u->operands[0];
        if (other->kind == R6_VAL_INT) return "int";
        if (other->kind == R6_VAL_REG && other->as.i32 != reg &&
            is_int(type_at(f, other->as.i32)))
            return "int";
        return NULL;
    }
    default:
        break;
    }
    if (f->instructions[use].opcode != R6_OP_ARG || f->instructions[use].has_dest)
        return NULL;
    for (pc = use + 1; pc < f->instruction_count; ++pc) {
        const R6Instruction *i = &f->instructions[pc];
        if (i->opcode == R6_OP_LABEL || i->opcode == R6_OP_JUMP ||
            i->opcode == R6_OP_JUMP_IF_FALSE)
            return NULL;
        if (i->opcode == R6_OP_ARG && !i->has_dest) {
            ++above;
        } else if (i->opcode == R6_OP_CALL) {
            const R6Function *callee;
            const char *name = ir_name(&i->operands[0]), *type;
            size_t arity;
            if (i->operands[1].kind != R6_VAL_INT || i->operands[1].as.i64 < 0) return NULL;
            arity = (size_t)i->operands[1].as.i64;
            if (arity <= above) {
                above -= arity;
                continue;
            }
            if (i->operands[0].kind != R6_VAL_STRING || !name ||
                strcmp(name, "print") == 0 || find_model(m, name) ||
                find_union_variant(m, name, NULL) ||
                (callee = find_function(m, name)) == NULL ||
                callee->param_count != arity)
                return NULL;
            type = callee->param_types[arity - 1 - above];
            return is_string(type) || is_int(type) ? type : NULL;
        }
    }
    return NULL;
}

/* D-M1-43: every `try` emits a result_unwrap, and the builder leaves its
 * destination `unknown` (the pin writes `r_N = r.value` into a uintptr_t
 * word). R6 types it from its use. With no reader the word is never
 * observed, and it is copied as a void* word. When the one reader is the
 * decl_var of a local, the destination takes the type the builder gave every
 * load_var of that local, which is the annotation `val x: T = try ...`. Any
 * other use, or loads that disagree, leaves it unknown. */
static const char *bound_local_type(const R6Function *f, size_t at) {
    int32_t reg = f->instructions[at].dest;
    size_t pc, use = f->instruction_count;
    const char *name, *type = NULL;
    for (pc = 0; pc < f->instruction_count; ++pc) {
        const R6Instruction *i = &f->instructions[pc];
        unsigned k;
        if (pc != at && i->has_dest && i->dest == reg) return NULL;
        for (k = 0; k < 3; ++k)
            if (i->operands[k].kind == R6_VAL_REG && i->operands[k].as.i32 == reg) {
                if (use != f->instruction_count) return NULL;
                use = pc;
            }
    }
    if (use == f->instruction_count) return "void*";
    if (f->instructions[use].opcode != R6_OP_DECL_VAR ||
        f->instructions[use].operands[1].kind != R6_VAL_REG ||
        (name = ir_name(&f->instructions[use].operands[0])) == NULL)
        return NULL;
    for (pc = 0; pc < f->instruction_count; ++pc) {
        const R6Instruction *i = &f->instructions[pc];
        const char *t;
        if (i->opcode != R6_OP_LOAD_VAR || !i->has_dest) continue;
        if (!ir_name(&i->operands[0]) || strcmp(ir_name(&i->operands[0]), name) != 0) continue;
        t = type_at(f, i->dest);
        if (!t || strcmp(t, "unknown") == 0) continue;
        if (type && strcmp(type, t) != 0) return NULL;
        type = t;
    }
    return type;
}

/* Whether any instruction after `at` reads its destination register. */
static bool has_reader(const R6Function *f, size_t at) {
    size_t pc;
    unsigned k;
    for (pc = at + 1; pc < f->instruction_count; ++pc)
        for (k = 0; k < 3; ++k)
            if (f->instructions[pc].operands[k].kind == R6_VAL_REG &&
                f->instructions[pc].operands[k].as.i32 == f->instructions[at].dest)
                return true;
    return false;
}

static const char *unwrap_use_type(const R6Function *f, size_t at) {
    const char *type = bound_local_type(f, at);
    return is_void_ptr(type) || is_int(type) || is_float(type) || is_string(type) ? type : NULL;
}

/* D-M1-47: a match arm binding (`Circle(r) => ...`) is a union_get_data whose
 * only reader is the decl_var of `r`, and the builder leaves its destination
 * `unknown`. It takes the type of r's loads, as an unwrap does; a payload
 * nobody reads, or a type outside the payloads D-M1-12 admits, stays unknown. */
static const char *payload_use_type(const R6Module *m, const R6Function *f, size_t at) {
    const char *type = bound_local_type(f, at);
    if (is_int(type) || is_bool(type) || is_string(type)) return type;
    /* D-M1-54: a model or union payload binds a local the builder types from
       the pattern; bound_local_type answers void* also for a read with no
       reader, which D-M1-47 keeps refused. */
    if (type && find_model(m, type)) return type;
    return is_void_ptr(type) && has_reader(f, at) ? type : NULL;
}

static bool resolve_pack_field(const R6Module *m, const R6Function *f,
                               const R6Instruction *i, bool *is_pack,
                               const R6Type **out_type, size_t *out_variant,
                               size_t *out_index, size_t *out_offset,
                               size_t *out_total, R6Diagnostic *d);

/* D-M1-53: since D-F0-15 a multi-payload arm reads its data word only through
 * `load_field <Union>_<Variant>._i` after the tag test, and the builder
 * leaves the word and every part `unknown`. The word is the pack pointer when
 * each of its readers is such a pack reference. */
static const char *pack_word_type(const R6Module *m, const R6Function *f, size_t at) {
    int32_t reg = f->instructions[at].dest;
    size_t pc, readers = 0;
    for (pc = 0; pc < f->instruction_count; ++pc) {
        const R6Instruction *i = &f->instructions[pc];
        unsigned k;
        if (pc != at && i->has_dest && i->dest == reg) return NULL;
        for (k = 0; k < 3; ++k) {
            const R6Type *pack;
            size_t variant, index, offset, total;
            bool is_pack = false;
            R6Diagnostic scratch;
            if (i->operands[k].kind != R6_VAL_REG || i->operands[k].as.i32 != reg) continue;
            if (i->opcode != R6_OP_LOAD_FIELD || k != 0) return NULL;
            resolve_pack_field(m, f, i, &is_pack, &pack, &variant, &index,
                               &offset, &total, &scratch);
            if (!is_pack) return NULL;
            ++readers;
        }
    }
    return readers ? "void*" : NULL;
}

/* `<Type>_TAG_<variant>` is the bare ordinal constant the pinned builder
 * loads (load_var) for tag comparisons. A union and an enumeration spell it
 * the same way and both use the variant's declaration index as the value. */
static bool tag_constant(const R6Module *m, const char *name, int32_t *value) {
    const char *mark, *variant;
    const R6Type *t = NULL;
    size_t n, v, owner_len;
    if (!m || !name) return false;
    mark = strstr(name, "_TAG_");
    if (!mark || mark == name || !mark[5]) return false;
    owner_len = (size_t)(mark - name);
    variant = mark + 5;
    for (n = 0; n < m->type_count; ++n) {
        const R6Type *cand = &m->types[n];
        if (strlen(cand->name) == owner_len && memcmp(cand->name, name, owner_len) == 0) {
            t = cand;
            break;
        }
    }
    if (!t || !t->kind ||
        (strcmp(t->kind, "union_type") != 0 && strcmp(t->kind, "enumeration") != 0))
        return false;
    for (v = 0; v < t->variant_count; ++v)
        if (t->variants[v] && strcmp(t->variants[v], variant) == 0) {
            *value = (int32_t)v;
            return true;
        }
    return false;
}

/* richVariants spelling: payload type names joined by tabs ("" = unit). */
static size_t union_payload_count(const char *rich) {
    size_t count = 1;
    const char *p;
    if (!rich || !rich[0]) return 0;
    for (p = rich; *p; ++p)
        if (*p == '\t') ++count;
    return count;
}

static bool union_payload_part(const char *rich, size_t index, char *out, size_t cap) {
    size_t start = 0, i = 0, n = rich ? strlen(rich) : 0;
    if (cap == 0 || n == 0) return false;
    for (;;) {
        if (i == n || rich[i] == '\t') {
            size_t len = i - start;
            if (index == 0) {
                if (len >= cap) return false;
                memcpy(out, rich + start, len);
                out[len] = '\0';
                return true;
            }
            --index;
            start = i + 1;
        }
        if (i == n) return false;
        ++i;
    }
}

static bool is_union_name(const R6Module *m, const char *t) {
    size_t n;
    if (!m || !t || !m->types) return false;
    for (n = 0; n < m->type_count; ++n)
        if (m->types[n].name && m->types[n].kind &&
            strcmp(m->types[n].kind, "union_type") == 0 &&
            strcmp(m->types[n].name, t) == 0)
            return true;
    return false;
}

/* D-M1-54: a payload that names a declared model or union is the pointer the
 * pin's mapTypeToC gives it (`Name*`, c_backend.orb:511-527), one 8-byte word
 * in the data word or the pack. */
static bool payload_is_word(const R6Module *m, const char *part) {
    return find_model(m, part) != NULL || is_union_name(m, part);
}

/* The register spelling a payload reads into: the scalar itself, the model
 * name, or `void*` for a union cell (D-M1-47). NULL outside the vocabulary. */
static const char *payload_reg_type(const R6Module *m, const char *part) {
    if (is_int(part) || is_bool(part) || is_string(part)) return part;
    if (find_model(m, part)) return part;
    if (is_union_name(m, part)) return "void*";
    return NULL;
}

/* The measured payload vocabulary: int, bool, string and, from D-M1-54,
 * declared models and unions. Single float payloads truncate through
 * (uintptr_t) in the pin, so they stay fail-closed. */
static bool union_payload_supported(const R6Module *m, const char *part) {
    return payload_reg_type(m, part) != NULL;
}

/* C layout of the synthesized `<union>_<variant>` pack struct the pin fills
 * through the `<U>_<V>_pack` helper: payload parts in declaration order with
 * natural alignment. */
static bool union_pack_layout(const R6Module *m, const R6Type *type, size_t variant,
                              size_t want, size_t *offset, size_t *total) {
    size_t i, off = 0, align_max = 1, want_off = 0;
    if (!type || variant >= type->variant_count) return false;
    for (i = 0; i < union_payload_count(type->rich_variants[variant]); ++i) {
        char part[64];
        size_t size, align;
        if (!union_payload_part(type->rich_variants[variant], i, part, sizeof part))
            return false;
        if (payload_is_word(m, part)) {
            size = 8;
            align = 8;
        } else if (!model_field_size(m, part, &size, &align)) {
            return false;
        }
        off = (off + align - 1) / align * align;
        if (i == want) want_off = off;
        off += size;
        if (align > align_max) align_max = align;
    }
    *offset = want_off;
    *total = (off + align_max - 1) / align_max * align_max;
    return true;
}

/* D-M1-62: `s.length`/`s.len` on a string is a load_field the pin lowers to
 * `orbit_string_len(s)` when the receiver is a string (c_backend.orb load_field
 * arm, `fname == "length" || fname == "len"`). */
static bool string_length_field(const R6Function *f, const R6Instruction *i) {
    const char *ref;
    if (i->opcode != R6_OP_LOAD_FIELD || i->operands[0].kind != R6_VAL_REG ||
        !is_string(type_at(f, i->operands[0].as.i32)))
        return false;
    if (i->operands[1].kind != R6_VAL_SYMBOL && i->operands[1].kind != R6_VAL_STRING)
        return false;
    ref = i->operands[1].as.string;
    return ref && (strcmp(ref, "length") == 0 || strcmp(ref, "len") == 0);
}

/* Resolve the `field` or `owner.field` reference of load_field/store_field.
 * The pinned builder qualifies the owner whenever the receiver type is known
 * (fieldOwnerForObject) and scans the module for the last model with the
 * field only for untyped receivers (findFieldOwner); R6 requires the
 * receiver to be a typed model register, so an explicit owner decides and a
 * bare name resolves in the receiver's own model. */
static bool resolve_field(const R6Module *m, const R6Function *f,
                          const R6Instruction *i,
                          const R6Model **out_model, size_t *out_index,
                          R6Diagnostic *d) {
    const R6Value *ref_value = &i->operands[1];
    const R6Value *rec_value = &i->operands[0];
    const char *ref, *receiver, *fname, *dot;
    const R6Model *model = NULL;
    size_t k, len = 0;

    if (ref_value->kind != R6_VAL_SYMBOL && ref_value->kind != R6_VAL_STRING)
        return fail(d, i, "CGEN", "%s requires a Symbol or String field reference",
                    r6_opcode_name(i->opcode));
    ref = ref_value->as.string;
    if (!ref || !*ref)
        return fail(d, i, "CGEN", "%s requires a non-empty field reference",
                    r6_opcode_name(i->opcode));
    if (rec_value->kind != R6_VAL_REG ||
        !(receiver = type_at(f, rec_value->as.i32)) || !is_model_type(m, receiver))
        return fail(d, i, "CGEN", "%s requires a register receiver of a declared model type",
                    r6_opcode_name(i->opcode));
    dot = strchr(ref, '.');
    fname = dot ? dot + 1 : ref;
    if (dot) {
        len = (size_t)(dot - ref);
        if (len == 0 || !*fname)
            return fail(d, i, "CGEN", "malformed field reference %s", ref);
        for (k = 0; k < m->model_count && !model; ++k) {
            const char *name = m->models[k].name;
            if (name && strlen(name) == len && memcmp(name, ref, len) == 0)
                model = &m->models[k];
        }
        if (!model)
            return fail(d, i, "CGEN", "field reference owner %.*s is not a declared model",
                        (int)len, ref);
        if (strlen(receiver) != len || memcmp(receiver, ref, len) != 0)
            return fail(d, i, "CGEN", "field reference owner %.*s does not match receiver type %s",
                        (int)len, ref, receiver);
    } else {
        model = find_model(m, receiver);
    }
    for (k = 0; k < model->field_count; ++k)
        if (model->fields[k].name && strcmp(model->fields[k].name, fname) == 0) {
            *out_model = model;
            *out_index = k;
            return true;
        }
    return fail(d, i, "CGEN", "model %s has no field %s", model->name, fname);
}

/* A multi-payload variant binds its values by reading `<union>_<variant>._i`
 * off the void* data pointer (union_get_data); the pin synthesizes one pack
 * struct per multi-payload variant. Returns false only on a hard error;
 * `*is_pack` stays false for references that are not pack fields, so the
 * caller falls back to model resolution. */
static bool resolve_pack_field(const R6Module *m, const R6Function *f,
                               const R6Instruction *i, bool *is_pack,
                               const R6Type **out_type, size_t *out_variant,
                               size_t *out_index, size_t *out_offset,
                               size_t *out_total, R6Diagnostic *d) {
    const R6Value *ref_value = &i->operands[1];
    const R6Value *rec_value = &i->operands[0];
    const char *ref, *dot, *fname, *receiver;
    const R6Type *pack = NULL;
    size_t owner_len, matches = 0, matched_variant = 0, k, field_index;
    char *end = NULL;
    *is_pack = false;
    if (ref_value->kind != R6_VAL_SYMBOL && ref_value->kind != R6_VAL_STRING)
        return true;
    ref = ref_value->as.string;
    if (!ref || !(dot = strchr(ref, '.')) || dot == ref || !dot[1])
        return true;
    owner_len = (size_t)(dot - ref);
    fname = dot + 1;
    for (k = 0; k < m->type_count; ++k) {
        const R6Type *t = &m->types[k];
        size_t v, name_len;
        if (!t->kind || strcmp(t->kind, "union_type") != 0) continue;
        for (v = 0; v < t->variant_count; ++v) {
            char joined[512];
            if (!t->variants[v]) continue;
            name_len = strlen(t->name) + 1 + strlen(t->variants[v]);
            if (name_len >= sizeof joined) continue;
            snprintf(joined, sizeof joined, "%s_%s", t->name, t->variants[v]);
            if (owner_len == name_len && memcmp(joined, ref, owner_len) == 0) {
                pack = t;
                matched_variant = v;
                ++matches;
            }
        }
    }
    if (matches == 0) return true;
    *is_pack = true;
    if (matches > 1)
        return fail(d, i, "CGEN", "pack field reference %.*s is ambiguous", (int)owner_len, ref);
    if (union_payload_count(pack->rich_variants[matched_variant]) < 2)
        return fail(d, i, "CGEN", "%s names a union variant without packed payloads", ref);
    if (fname[0] != '_' || fname[1] == '\0')
        return fail(d, i, "CGEN", "malformed pack field reference %s", ref);
    field_index = (size_t)strtoul(fname + 1, &end, 10);
    if (!end || *end)
        return fail(d, i, "CGEN", "malformed pack field reference %s", ref);
    if (field_index >= union_payload_count(pack->rich_variants[matched_variant]))
        return fail(d, i, "CGEN", "union %s variant %s has no payload field %s",
                    pack->name, pack->variants[matched_variant], fname);
    if (rec_value->kind != R6_VAL_REG ||
        !(receiver = type_at(f, rec_value->as.i32)) || !is_void_ptr(receiver))
        return fail(d, i, "CGEN", "pack field reference %s requires a void* data receiver", ref);
    if (!union_pack_layout(m, pack, matched_variant, field_index, out_offset, out_total))
        return fail(d, i, "CGEN", "union %s variant %s payloads are outside the M1 native subset",
                    pack->name, pack->variants[matched_variant]);
    *out_type = pack;
    *out_variant = matched_variant;
    *out_index = field_index;
    return true;
}

static bool has_label(const R6Function *f, int32_t label) {
    size_t n;
    for (n = 0; n < f->instruction_count; ++n)
        if (f->instructions[n].opcode == R6_OP_LABEL &&
            f->instructions[n].operands[0].kind == R6_VAL_LABEL &&
            f->instructions[n].operands[0].as.i32 == label) return true;
    return false;
}

static bool valid_dest(const R6Function *f, const R6Instruction *i,
                       bool required, R6Diagnostic *d) {
    if (required && !i->has_dest)
        return fail(d, i, "CGEN", "%s requires a destination", r6_opcode_name(i->opcode));
    if (i->has_dest && !type_at(f, i->dest))
        return fail(d, i, "CGEN", "%s destination register is out of range", r6_opcode_name(i->opcode));
    return true;
}

static bool validate_value_for_register(const R6Function *f, const R6Value *v,
                                        int32_t reg, const R6Instruction *i,
                                        R6Diagnostic *d) {
    const char *got, *want = type_at(f, reg);
    if (!value_type(f, v, &got))
        return fail(d, i, "CGEN", "%s has an unsupported value operand", r6_opcode_name(i->opcode));
    if (!same_type(got, want))
        return fail(d, i, "CGEN", "%s value type %s does not match register type %s",
                    r6_opcode_name(i->opcode), got, want ? want : "<invalid>");
    if (v->kind == R6_VAL_STRING && v->string_len != strlen(v->as.string))
        return fail(d, i, "CGEN", "string literal contains NUL and cannot be passed through the C ABI");
    return true;
}

static bool is_string_reg(const R6Function *f, const R6Value *v) {
    return v->kind == R6_VAL_REG && is_string(type_at(f, v->as.i32));
}

static bool is_int_reg(const R6Function *f, const R6Value *v) {
    return v->kind == R6_VAL_REG && is_int(type_at(f, v->as.i32));
}

static bool validate_function(const R6Module *m, const R6Function *f,
                              R6Diagnostic *d) {
    size_t pc, arg_count = 0;
    const R6Instruction *arg_values[256];
    if (!valid_symbol(f->name))
        return fail(d, NULL, "CGEN", "function name is not a supported native symbol");
    if (!f->is_extern && strlen(f->name) > R6_USER_NAME_MAX)
        return fail(d, NULL, "CGEN", "function name %.24s... is longer than %d bytes",
                    f->name, R6_USER_NAME_MAX);
    if (!f->return_type)
        return fail(d, NULL, "CGEN", "function %s has no return type", f->name);
    if (f->is_extern) {
        /* V-18/E-09: an extern body, if present, is ignored and never emitted. */
        if (f->param_count > 256 || (f->param_count && !f->param_types))
            return fail(d, NULL, "CGEN", "extern function %s has unsupported parameter metadata", f->name);
        for (pc = 0; pc < f->param_count; ++pc)
            if (!is_scalar(f->param_types[pc]) &&
                !(is_void_ptr(f->param_types[pc]) &&
                  (extern_takes_object(f->name) || extern_object_symbol(f->name))) &&
                !(is_list(f->param_types[pc]) && extern_object_symbol(f->name)))
                return fail(d, NULL, "CGEN", "extern parameter type is unsupported in M1");
        /* D-M1-48: a runtime extern may also return an OrbitList* word or an
           OrbitResult through the sret pointer. */
        if (strcmp(f->return_type, "void") != 0 && !is_scalar(f->return_type) &&
            !is_list(f->return_type) && !is_result(f->return_type) &&
            !(is_void_ptr(f->return_type) &&
              (extern_takes_object(f->name) || extern_object_symbol(f->name))))
            return fail(d, NULL, "CGEN", "extern return type is unsupported in M1");
        return true;
    }
    if (f->param_count && (!f->param_types || !f->params))
        return fail(d, NULL, "CGEN", "function %s has missing parameter metadata", f->name);
    if (f->param_count > 256)
        return fail(d, NULL, "CGEN", "function %s exceeds the 256-parameter Win64 limit", f->name);
    /* D-M1-61: a void main is `int orbit_main` returning 0 in the pin
       (c_backend.orb: isMain sets retType "int"). */
    if (strcmp(f->name, "main") == 0 &&
        (f->param_count != 0 ||
         !(is_int(f->return_type) || strcmp(f->return_type, "void") == 0)))
        return fail(d, NULL, "CGEN", "main must have no parameters and return int or void in M1");
    /* D-M1-40: a model parameter is the `Name*` the pin passes (mapTypeToC),
     * a word the prologue spills with movq like any non-float parameter.
     * D-M1-47: a union parameter is the same word, its D-M1-12 cell. */
    for (pc = 0; pc < f->param_count; ++pc)
        if (!(is_scalar(f->param_types[pc]) || is_list(f->param_types[pc]) ||
              is_model_type(m, f->param_types[pc]) || is_void_ptr(f->param_types[pc]) ||
              is_rtarena(f->param_types[pc]) ||
              (is_result(f->param_types[pc]) && strcmp(f->name, "main") != 0)))
            return fail(d, NULL, "CGEN", "parameter type %s is unsupported in M1", f->param_types[pc]);
    /* D-M1-44: a model return is the `Name*` the pin returns (mapTypeToC),
     * one word in rax like any non-float return. D-M1-55: a void* return (a
     * union cell, D-M1-47) is the same word. */
    if (is_fn_type(f->return_type))
        return fail(d, NULL, "CGEN", "returning function values is not supported in M1");
    for (pc = 0; pc < f->instruction_count; ++pc) {
        const R6Instruction *i = &f->instructions[pc];
        const char *a = NULL, *b = NULL;
        char fn_buf[R6_FN_SPELLING_CAP], local_buf[R6_FN_SPELLING_CAP];
        switch (i->opcode) {
        case R6_OP_LOAD_CONST: {
            R6Value lit;
            int literal;
            if (!valid_dest(f, i, true, d)) return false;
            literal = sym_literal_kind(&i->operands[0], &lit);
            if (literal < 0)
                return fail(d, i, "CGEN", "float literal %s is not finite",
                            i->operands[0].as.string);
            if (literal > 0) {
                /* Spec 2.1: a bool or float literal arrives as `Sym` text and
                   must still match the destination register's type. */
                if (!validate_value_for_register(f, &lit, i->dest, i, d)) return false;
            } else if (is_null_symbol(&i->operands[0])) {
                /* D-M1-59: `NULL` is the zero word of a void* register. */
                if (!is_void_ptr(type_at(f, i->dest)))
                    return fail(d, i, "CGEN", "the NULL literal requires a void* destination");
            } else if (i->operands[0].kind == R6_VAL_SYMBOL) {
                /* Function value: the symbol names a module function whose
                   signature spelling must equal the destination's type. */
                if (!fn_value_bind(m, i, i->operands[0].as.string, type_at(f, i->dest),
                                   fn_buf, sizeof fn_buf, d)) return false;
            } else if (!validate_value_for_register(f, &i->operands[0], i->dest, i, d)) return false;
            break;
        }
        case R6_OP_DECL_VAR: case R6_OP_STORE_VAR: {
            const char *name = ir_name(&i->operands[0]), *vt = NULL, *lt = NULL;
            if (!name || !*name)
                return fail(d, i, "CGEN", "%s requires a named local", r6_opcode_name(i->opcode));
            if (i->operands[1].kind == R6_VAL_SYMBOL) {
                /* `val f = inc` binds a function value: the builder stores a
                   bare function name as a Symbol initializer (buildVarDecl
                   via buildCallArgValue); a plain assignment loads it into a
                   register first, so store_var keeps requiring a register. */
                if (i->opcode != R6_OP_DECL_VAR)
                    return fail(d, i, "CGEN", "store_var requires a register value");
                if (!fn_value_bind(m, i, i->operands[1].as.string, NULL, fn_buf, sizeof fn_buf, d))
                    return false;
                vt = fn_buf;
            } else if (!value_type(f, &i->operands[1], &vt)) {
                return fail(d, i, "CGEN", "%s has an unsupported value operand", r6_opcode_name(i->opcode));
            }
            if (!(is_scalar(vt) || is_list(vt) || is_void_ptr(vt) || is_rtarena(vt) ||
                  is_model_type(m, vt) || is_fn_type(vt) || (is_result(vt) && i->operands[1].kind == R6_VAL_REG)) ||
                !local_info(m, f, name, NULL, &lt, local_buf, sizeof local_buf) || !same_type(vt, lt))
                return fail(d, i, "CGEN", "%s requires a declared local and a matching scalar, list, void*, model, or function value register",
                            r6_opcode_name(i->opcode));
            break;
        }
        case R6_OP_LOAD_VAR: {
            const char *name = ir_name(&i->operands[0]), *lt = NULL;
            const R6Global *global = name ? find_global(m, name) : NULL;
            int64_t ignored;
            int32_t tag_value = 0;
            /* A parameter resolves first, in the backend's order
               (c_backend.orb:788-794), and carries paramTypes[p] (D-M1-22). */
            bool param = param_info(f, name, NULL, &lt);
            bool local = !param && name &&
                         local_info(m, f, name, NULL, &lt, local_buf, sizeof local_buf);
            bool tag = !param && !local && !global && tag_constant(m, name, &tag_value);
            if (!valid_dest(f, i, true, d) || !name ||
                (!param && !local && !tag && (!global || !global_integer(global, &ignored))) ||
                ((param || local) ? (!same_type(lt, type_at(f, i->dest)) ||
                          !(is_scalar(lt) || is_list(lt) || is_void_ptr(lt) || is_rtarena(lt) ||
                            is_model_type(m, lt) || is_fn_type(lt) || is_result(lt))) :
                 tag ? !is_int(type_at(f, i->dest)) :
                       !same_type(global_type(global), type_at(f, i->dest))))
                return fail(d, i, "CGEN", "load_var requires a supported local, parameter, declared global, or tag constant");
            break;
        }
        case R6_OP_LIST_CREATE: {
            const char *capacity_type = NULL;
            /* D-M1-30: elemKind 0 is ORBIT_ELEM_OPAQUE, the state the runtime
               itself creates (`orbit_list_create` is
               `orbit_list_create_typed(..., ORBIT_ELEM_OPAQUE)`,
               collections.c:170). The first typed push or set adopts the kind
               (collections.c:102-116) and a read accepts an opaque list
               (:118-128), so the create only needs an 8-byte slot, a capacity
               and a list destination. Kinds 1..4 keep their exact mapping and
               kind 5 is the ref kind of D-M1-57. */
            if (!valid_dest(f, i, true, d) || !is_list(type_at(f, i->dest)) ||
                i->operands[0].kind != R6_VAL_INT || i->operands[0].as.i64 != 8 ||
                !(i->elem_kind == 0 || i->elem_kind == 5 || elem_kind_type(i->elem_kind)) ||
                !value_type(f, &i->operands[1], &capacity_type) || !is_int(capacity_type))
                return fail(d, i, "CGEN", "list_create requires an 8-byte slot size, a kind in 0..5, a list destination, and an int capacity");
            break;
        }
        case R6_OP_LIST_PUSH: {
            const char *vt = i->operands[1].kind == R6_VAL_REG ? type_at(f, i->operands[1].as.i32) : NULL;
            if (i->operands[0].kind != R6_VAL_REG ||
                !is_list(type_at(f, i->operands[0].as.i32)) ||
                !vt || !(elem_matches(m, i->elem_kind, vt) ||
                         (i->elem_kind == 3 && is_int(vt)) ||
                         (i->elem_kind == 0 && opaque_word_type(m, vt))) ||
                (i->has_dest && !is_void(type_at(f, i->dest))))
                return fail(d, i, "CGEN", "list_push requires a list register and a value register matching elemKind 1..5");
            break;
        }
        case R6_OP_LIST_LEN:
            /* D-M1-29: `.len()` on a string reaches this opcode too, and the
               pinned arm asks isStringOperand first and lowers it to
               orbit_string_len (c_backend.orb:3615-3621). Both receivers keep
               an int destination. */
            if (!valid_dest(f, i, true, d) || !is_int(type_at(f, i->dest)) ||
                i->operands[0].kind != R6_VAL_REG ||
                (!is_list(type_at(f, i->operands[0].as.i32)) &&
                 !is_string(type_at(f, i->operands[0].as.i32))))
                return fail(d, i, "CGEN", "list_len requires a list or string register and an int destination");
            break;
        case R6_OP_LIST_GET:
            if (!valid_dest(f, i, true, d) ||
                !(elem_matches(m, i->elem_kind, type_at(f, i->dest)) ||
                  (i->elem_kind == 0 && opaque_word_type(m, type_at(f, i->dest))) ||
                  (i->elem_kind == 0 &&
                   same_type(opaque_read_type(m, f, pc), type_at(f, i->dest)))) ||
                i->operands[0].kind != R6_VAL_REG ||
                !is_list(type_at(f, i->operands[0].as.i32)) ||
                i->operands[1].kind != R6_VAL_REG || !is_int(type_at(f, i->operands[1].as.i32)))
                return fail(d, i, "CGEN", "list_get requires a list register, an int index register, and an elemKind matching the destination");
            break;
        case R6_OP_LIST_SET: {
            const char *vt = i->operands[2].kind == R6_VAL_REG ? type_at(f, i->operands[2].as.i32) : NULL;
            if (i->operands[0].kind != R6_VAL_REG ||
                !is_list(type_at(f, i->operands[0].as.i32)) ||
                i->operands[1].kind != R6_VAL_REG || !is_int(type_at(f, i->operands[1].as.i32)) ||
                !vt || !(elem_matches(m, i->elem_kind, vt) ||
                         (i->elem_kind == 0 && opaque_word_type(m, vt))) ||
                (i->has_dest && !is_void(type_at(f, i->dest))))
                return fail(d, i, "CGEN", "list_set requires list/index registers, an elemKind-matching value register, and an optional void destination");
            break;
        }
        case R6_OP_LIST_POP:
            if (!valid_dest(f, i, true, d) ||
                !(elem_matches(m, i->elem_kind, type_at(f, i->dest)) ||
                  (i->elem_kind == 0 && opaque_word_type(m, type_at(f, i->dest)))) ||
                i->operands[0].kind != R6_VAL_REG ||
                !is_list(type_at(f, i->operands[0].as.i32)))
                return fail(d, i, "CGEN", "list_pop requires a list register and an elemKind matching the destination");
            break;
        case R6_OP_LOAD_FIELD: {
            const R6Model *model = NULL;
            const R6Type *pack = NULL;
            size_t index = 0, offset = 0, total = 0, pack_variant = 0, pack_index = 0;
            bool is_pack = false;
            char part[64];
            if (!valid_dest(f, i, true, d)) return false;
            if (string_length_field(f, i)) {
                if (!is_int(type_at(f, i->dest)))
                    return fail(d, i, "CGEN", "a string length read requires an int destination");
                break;
            }
            if (!resolve_pack_field(m, f, i, &is_pack, &pack, &pack_variant, &pack_index,
                                    &offset, &total, d))
                return false;
            if (!is_pack && i->operands[0].kind == R6_VAL_REG &&
                is_void_ptr(type_at(f, i->operands[0].as.i32))) {
                /* D-M1-86: a keyed lookup on an OrbitObject word, IR_SPEC 1.4.
                   A key without text has no spelling the getter can take. */
                const R6Value *key = &i->operands[1];
                if (!key->as.string || !*key->as.string || key->string_len != strlen(key->as.string))
                    return fail(d, i, "CGEN", "an object field read requires a non-empty key without NUL");
                if (!object_getter(type_at(f, i->dest)))
                    return fail(d, i, "CGEN", "an object field read requires an int, float, bool, string, object or list destination");
                break;
            }
            if (is_pack) {
                if (!union_payload_part(pack->rich_variants[pack_variant], pack_index, part, sizeof part) ||
                    !same_type(type_at(f, i->dest), payload_reg_type(m, part)))
                    return fail(d, i, "CGEN", "load_field destination type does not match packed payload %s",
                                pack->variants[pack_variant]);
                break;
            }
            if (!resolve_field(m, f, i, &model, &index, d))
                return false;
            if (!same_type(type_at(f, i->dest), model->fields[index].type_name))
                return fail(d, i, "CGEN", "load_field destination type does not match field %s",
                            model->fields[index].name);
            break;
        }
        case R6_OP_STORE_FIELD: {
            const R6Model *model = NULL;
            size_t index = 0;
            const char *vt = NULL;
            if (i->has_dest)
                return fail(d, i, "CGEN", "store_field does not write a destination");
            if (!resolve_field(m, f, i, &model, &index, d)) return false;
            if (!value_type(f, &i->operands[2], &vt) ||
                !same_type(vt, model->fields[index].type_name))
                return fail(d, i, "CGEN", "store_field value type does not match field %s",
                            model->fields[index].name);
            if (i->operands[2].kind == R6_VAL_STRING &&
                (!i->operands[2].as.string ||
                 i->operands[2].string_len != strlen(i->operands[2].as.string)))
                return fail(d, i, "CGEN", "string literal contains NUL and cannot be passed through the C ABI");
            break;
        }
        case R6_OP_UNION_GET_TAG: {
            const char *src = NULL;
            if (!valid_dest(f, i, true, d) || !operand_reg(f, i, 0, &src, d) ||
                !is_void_ptr(src) || !is_int(type_at(f, i->dest)))
                return fail(d, i, "CGEN", "union_get_tag requires a void* register operand and an int destination");
            break;
        }
        case R6_OP_UNION_GET_DATA: {
            const char *src = NULL;
            const char *dt = type_at(f, i->dest);
            if (!valid_dest(f, i, true, d) || !operand_reg(f, i, 0, &src, d) ||
                !is_void_ptr(src) ||
                !(is_void_ptr(dt) || is_int(dt) || is_bool(dt) || is_string(dt) ||
                  is_model_type(m, dt)))
                return fail(d, i, "CGEN", "union_get_data requires a void* register operand and a non-float payload destination");
            break;
        }
        case R6_OP_COPY:
            /* D-M1-93: the pin widens an int stored into a float local or
               field by copying the int register into a float register, and
               its C assignment converts the value. */
            if (i->operands[0].kind == R6_VAL_REG && type_at(f, i->dest) &&
                is_float(type_at(f, i->dest)) &&
                i->operands[0].as.i32 >= 0 && i->operands[0].as.i32 < f->register_count &&
                is_int(type_at(f, i->operands[0].as.i32))) {
                if (!valid_dest(f, i, true, d)) return false;
                break;
            }
            if (!valid_dest(f, i, true, d) ||
                !validate_value_for_register(f, &i->operands[0], i->dest, i, d)) return false;
            if (!is_scalar(type_at(f, i->dest)) && !is_rtarena(type_at(f, i->dest)) &&
                !(is_result(type_at(f, i->dest)) && i->operands[0].kind == R6_VAL_REG))
                return fail(d, i, "CGEN", "copy currently supports scalar and result registers only");
            if (i->operands[0].kind == R6_VAL_STRING &&
                type_at(f, i->dest) && is_string(type_at(f, i->dest)))
                return fail(d, i, "CGEN", "copy from string literal is not supported; load_const handles literals");
            break;
        case R6_OP_ADD:
            if (!valid_dest(f, i, true, d) || !operand_value(f, i, 0, &a, d) ||
                !operand_value(f, i, 1, &b, d)) return false;
            if (is_string(a) != is_string(b)) {
                /* D-M1-23: one side is a string and the other is spread
                   through the runtime converters, so the result is a string
                   and the spread side must be a scalar a converter covers.
                   A pointer, list or model operand has no measured converter
                   and stays fail closed. */
                const char *spread = is_string(a) ? b : a;
                if (!is_string(type_at(f, i->dest)) ||
                    !(is_int(spread) || is_float(spread) || is_bool(spread)))
                    return fail(d, i, "CGEN",
                                "add spread requires a string destination and an int, float, or bool scalar operand");
                break;
            }
            if (is_string(a)) {
                /* D-M1-23: string concatenation. The pinned backend keeps the
                   arena in a file-scope global for every function; R6 reads it
                   from main's parameter slot or from r6_arena. */
                if (!is_string(type_at(f, i->dest)))
                    return fail(d, i, "CGEN",
                                "string add requires a string destination");
                break;
            }
            if (!word_arith_ok(a, b, type_at(f, i->dest)) &&
                (!same_type(a, b) || !same_type(a, type_at(f, i->dest)) ||
                 !(is_int(a) || is_float(a))))
                return fail(d, i, "CGEN", "add supports matching int or float operands");
            break;
        case R6_OP_SUB: case R6_OP_MUL: case R6_OP_DIV: case R6_OP_MOD:
            if (!valid_dest(f, i, true, d) || !operand_value(f, i, 0, &a, d) ||
                !operand_value(f, i, 1, &b, d)) return false;
            if (!((i->opcode == R6_OP_SUB || i->opcode == R6_OP_MUL) &&
                  word_arith_ok(a, b, type_at(f, i->dest))) &&
                (!same_type(a, b) || !same_type(a, type_at(f, i->dest)) ||
                 !(is_int(a) || (i->opcode != R6_OP_MOD && is_float(a)))))
                return fail(d, i, "CGEN", "sub/mul/div require matching int/float operands; mod requires int");
            break;
        case R6_OP_EQ:
            if (!valid_dest(f, i, true, d) || !operand_value(f, i, 0, &a, d) ||
                !operand_value(f, i, 1, &b, d)) return false;
            if (!is_bool(type_at(f, i->dest)) ||
                !(comparison_types_ok(a, b, true) || word_compare_ok(m, a, b)))
                return fail(d, i, "CGEN", "eq and ne require int, float, bool, or string operands and a bool destination; a string is compared with strcmp only against another string");
            break;
        case R6_OP_NE: case R6_OP_LT: case R6_OP_LE: case R6_OP_GT: case R6_OP_GE:
            if (!valid_dest(f, i, true, d) || !operand_value(f, i, 0, &a, d) ||
                !operand_value(f, i, 1, &b, d)) return false;
            if (!is_bool(type_at(f, i->dest)) ||
                !(comparison_types_ok(a, b, i->opcode == R6_OP_NE) ||
                  (i->opcode == R6_OP_NE && word_compare_ok(m, a, b))))
                return fail(d, i, "CGEN", "relational ops require int, float, or bool operands and a bool destination; ne also takes strings");
            break;
        case R6_OP_AND_OP: case R6_OP_OR_OP:
            if (!valid_dest(f, i, true, d) || !operand_reg(f, i, 0, &a, d) ||
                !operand_reg(f, i, 1, &b, d)) return false;
            if (!is_bool(a) || !is_bool(b) || !is_bool(type_at(f, i->dest)))
                return fail(d, i, "CGEN", "and_op/or_op require bool operands and destination");
            break;
        case R6_OP_NOT_OP:
            if (!valid_dest(f, i, true, d) || !operand_reg(f, i, 0, &a, d)) return false;
            if (!is_bool(a) || !is_bool(type_at(f, i->dest)))
                return fail(d, i, "CGEN", "not_op requires bool operand and destination");
            break;
        case R6_OP_NEG:
            if (!valid_dest(f, i, true, d) || !operand_reg(f, i, 0, &a, d)) return false;
            if (!(is_int(a) || is_float(a)) || !same_type(a, type_at(f, i->dest)))
                return fail(d, i, "CGEN", "neg requires matching int or float operand and destination");
            break;
        case R6_OP_BIT_AND: case R6_OP_BIT_OR: case R6_OP_BIT_XOR:
        case R6_OP_SHL: case R6_OP_SHR:
            if (!valid_dest(f, i, true, d) || !operand_value(f, i, 0, &a, d) ||
                !operand_value(f, i, 1, &b, d)) return false;
            if (!is_int(a) || !is_int(b) || !is_int(type_at(f, i->dest)))
                return fail(d, i, "CGEN", "%s requires int operands and an int destination",
                            r6_opcode_name(i->opcode));
            break;
        case R6_OP_BIT_NOT:
            if (!valid_dest(f, i, true, d) || !operand_value(f, i, 0, &a, d)) return false;
            if (!is_int(a) || !is_int(type_at(f, i->dest)))
                return fail(d, i, "CGEN", "bit_not requires an int operand and an int destination");
            break;
        /* D-M1-106: the byte opcodes of a runtime module. Every operand is a
           register: an inline literal has no scratch slot here. */
        case R6_OP_STR_LEN:
            if (!m->runtime_module || !valid_dest(f, i, true, d) ||
                !is_int(type_at(f, i->dest)) ||
                !is_string_reg(f, &i->operands[0]))
                return fail(d, i, "CGEN", "str_len requires a string register and an int destination");
            break;
        case R6_OP_BYTE_LOAD:
            if (!m->runtime_module || !valid_dest(f, i, true, d) ||
                !is_int(type_at(f, i->dest)) ||
                !is_string_reg(f, &i->operands[0]) || !is_int_reg(f, &i->operands[1]))
                return fail(d, i, "CGEN", "byte_load requires a string register, an int index register and an int destination");
            break;
        case R6_OP_BYTE_STORE:
            if (!m->runtime_module || i->has_dest ||
                !is_string_reg(f, &i->operands[0]) || !is_int_reg(f, &i->operands[1]) ||
                !is_int_reg(f, &i->operands[2]))
                return fail(d, i, "CGEN", "byte_store requires a string register, an int index register and an int value register, and no destination");
            break;
        /* D-M1-110 */
        case R6_OP_STR_OFFSET:
            if (!m->runtime_module || !valid_dest(f, i, true, d) ||
                !is_string(type_at(f, i->dest)) ||
                !is_string_reg(f, &i->operands[0]) || !is_int_reg(f, &i->operands[1]))
                return fail(d, i, "CGEN", "str_offset requires a string register, an int offset register and a string destination");
            break;
        case R6_OP_BYTES_COPY:
            if (!m->runtime_module || i->has_dest ||
                !is_string_reg(f, &i->operands[0]) || !is_string_reg(f, &i->operands[1]) ||
                !is_int_reg(f, &i->operands[2]))
                return fail(d, i, "CGEN", "bytes_copy requires a string destination register, a string source register and an int count register, and no destination");
            break;
        case R6_OP_BYTES_ALLOC:
            if (!m->runtime_module || !valid_dest(f, i, true, d) ||
                !is_string(type_at(f, i->dest)) || !is_int_reg(f, &i->operands[0]))
                return fail(d, i, "CGEN", "bytes_alloc requires an int length register and a string destination");
            if (i->operands[1].kind != R6_VAL_NONE &&
                !(i->operands[1].kind == R6_VAL_REG &&
                  is_rtarena(type_at(f, i->operands[1].as.i32))))
                return fail(d, i, "CGEN", "bytes_alloc operand2 must be an RtArena register");
            break;
        case R6_OP_JUMP_IF_FALSE:
            if (i->operands[0].kind != R6_VAL_REG ||
                !is_bool(type_at(f, i->operands[0].as.i32)))
                return fail(d, i, "CGEN", "jump_if_false requires a bool register condition");
            if (i->operands[1].kind != R6_VAL_LABEL ||
                !has_label(f, i->operands[1].as.i32))
                return fail(d, i, "CGEN", "jump_if_false target label is undefined");
            break;
        case R6_OP_JUMP:
            if (i->operands[0].kind != R6_VAL_LABEL ||
                !has_label(f, i->operands[0].as.i32))
                return fail(d, i, "CGEN", "jump target label is undefined");
            break;
        case R6_OP_LABEL:
            if (i->operands[0].kind != R6_VAL_LABEL)
                return fail(d, i, "CGEN", "label requires a label operand");
            { size_t k; for (k = 0; k < pc; ++k)
                if (f->instructions[k].opcode == R6_OP_LABEL &&
                    f->instructions[k].operands[0].as.i32 == i->operands[0].as.i32)
                    return fail(d, i, "CGEN", "duplicate label %d", i->operands[0].as.i32); }
            break;
        case R6_OP_ARG:
            if (i->has_dest) {
                int64_t ix;
                if (i->operands[0].kind != R6_VAL_INT)
                    return fail(d, i, "CGEN", "arg parameter binding requires an integer index");
                ix = i->operands[0].as.i64;
                if (ix < 0 || (size_t)ix >= f->param_count ||
                    !valid_dest(f, i, true, d))
                    return fail(d, i, "CGEN", "arg parameter index/destination is invalid");
                if (!same_type(type_at(f, i->dest), f->param_types[ix]))
                    return fail(d, i, "CGEN", "arg binding type does not match parameter %d", (int)ix);
            } else {
                const char *t;
                if (arg_count == sizeof arg_values / sizeof arg_values[0])
                    return fail(d, i, "CGEN", "more than 256 pending call arguments are unsupported");
                if (!value_type(f, &i->operands[0], &t) ||
                    !(is_scalar(t) || is_list(t) || is_model_type(m, t) || is_void_ptr(t) ||
                      is_rtarena(t) ||
                      (is_result(t) && i->operands[0].kind == R6_VAL_REG)))
                    return fail(d, i, "CGEN", "arg value type is unsupported");
                if (i->operands[0].kind == R6_VAL_REG && !type_at(f, i->operands[0].as.i32))
                    return fail(d, i, "CGEN", "arg register is out of range");
                if (i->operands[0].kind == R6_VAL_REG &&
                    !validate_value_for_register(f, &i->operands[0], i->operands[0].as.i32, i, d)) return false;
                if (i->operands[0].kind == R6_VAL_STRING && !i->operands[0].as.string)
                    return fail(d, i, "CGEN", "null string literal");
                if (i->operands[0].kind == R6_VAL_STRING &&
                    i->operands[0].string_len != strlen(i->operands[0].as.string))
                    return fail(d, i, "CGEN", "string literal contains NUL and cannot be passed through the C ABI");
                arg_values[arg_count++] = i;
            }
            break;
        case R6_OP_CALL: {
            const R6Function *callee;
            const R6Model *model = i->operands[0].kind == R6_VAL_STRING
                                       ? find_model(m, i->operands[0].as.string) : NULL;
            size_t k, arity, base;
            if (model) {
                /* The pinned builder emits model construction as a call whose
                   target is a String value (ir_string(instance)) carrying the
                   field values as already-emitted args; a named call carries
                   the callee name in the same String form (spec 3.1 step 2). */
                if (i->operands[1].kind != R6_VAL_INT || i->operands[1].as.i64 < 0)
                    return fail(d, i, "CGEN", "call arity must be a non-negative Int");
                arity = (size_t)i->operands[1].as.i64;
                if (arity > arg_count)
                    return fail(d, i, "CGEN", "call arity exceeds the pending argument count");
                base = arg_count - arity;
                if (!valid_dest(f, i, true, d) || !same_type(type_at(f, i->dest), model->name))
                    return fail(d, i, "CGEN", "model constructor call requires a destination of type %s",
                                model->name);
                if (arity != model->field_count)
                    return fail(d, i, "CGEN", "model constructor %s takes %zu field value(s); call passes %zu",
                                model->name, model->field_count, arity);
                for (k = 0; k < arity; ++k) {
                    const char *t;
                    size_t size, align;
                    R6Value *v = (R6Value *)&arg_values[base + k]->operands[0];
                    if (!model_field_size(m, model->fields[k].type_name, &size, &align))
                        return fail(d, i, "CGEN", "model %s field %s has unsupported type %s",
                                    model->name, model->fields[k].name, model->fields[k].type_name);
                    if (!value_type(f, v, &t) || !same_type(t, model->fields[k].type_name))
                        return fail(d, i, "CGEN", "model constructor argument %zu does not match field %s",
                                    k, model->fields[k].name);
                    if (v->kind == R6_VAL_STRING &&
                        (!v->as.string || v->string_len != strlen(v->as.string)))
                        return fail(d, i, "CGEN", "string literal contains NUL and cannot be passed through the C ABI");
                }
                arg_count -= arity;
                break;
            }
            /* D-M1-26: the builtin `print` is intercepted by NAME before any
               declaration is resolved, exactly as the pinned lowering does
               (c_backend.orb:2977-3005): a module may declare `extern fn
               print(...)` (3 corpus programs do) and the call still lowers to
               the printf forms, never to the declared symbol. feBuildTimeCallType
               types it void (:3911). The destination, when the builder
               allocated one, is never stored -- the builder types it int from
               the extern's return and no corpus program reads it -- so any
               valid destination register is accepted here. */
            if (i->operands[0].kind == R6_VAL_STRING &&
                strcmp(i->operands[0].as.string, "print") == 0) {
                if (i->operands[1].kind != R6_VAL_INT || i->operands[1].as.i64 < 0)
                    return fail(d, i, "CGEN", "call arity must be a non-negative Int");
                arity = (size_t)i->operands[1].as.i64;
                if (arity > arg_count)
                    return fail(d, i, "CGEN", "call arity exceeds the pending argument count");
                if (i->has_dest && !valid_dest(f, i, true, d)) return false;
                if (arity == 1) {
                    /* The single argument selects the reduction, so its type
                       must be one the lowering measures: a string through
                       "%s", a float through the runtime formatter, and
                       int/bool through "%lld". D-M1-91: a void* register
                       prints as `(long long)word`, the whole 64-bit word. Any
                       other class fails here rather than reaching the
                       emitter. */
                    const char *at = NULL;
                    if (!value_type(f, &arg_values[arg_count - 1]->operands[0], &at) ||
                        !(is_int(at) || is_bool(at) || is_string(at) || is_float(at) ||
                          (is_void_ptr(at) && arg_values[arg_count - 1]->operands[0].kind == R6_VAL_REG)))
                        return fail(d, i, "CGEN",
                                    "print argument has no measured printable type");
                    arg_count -= 1;
                } else {
                    /* c_backend.orb:2999-3004: any other arity pops every
                       pending argument and emits the no-op. */
                    arg_count = 0;
                }
                break;
            }
            /* Direct named call: the callee name arrives as a String (the form
               the pinned builder emits for plain functions, externs and
               builtins alike, spec 3.1 step 2) or as a Symbol. Model
               construction is handled above and union-variant construction
               below, so a String that names a declared function is the call. */
            callee = (i->operands[0].kind == R6_VAL_SYMBOL || i->operands[0].kind == R6_VAL_STRING) &&
                             valid_symbol(i->operands[0].as.string)
                         ? find_function(m, i->operands[0].as.string) : NULL;
            /* D-M1-29: a String target the module does not declare but the
               runtime headers do (the builder omits those declarations). The
               Symbol form stays a declaration lookup: the builder emits the
               intrinsic calls as String targets only. */
            if (!callee && i->operands[0].kind == R6_VAL_STRING)
                callee = find_intrinsic(i->operands[0].as.string);
            if (callee) {
                if (strcmp(callee->name, "main") == 0)
                    return fail(d, i, "CGEN", "main is an entry point and cannot be called from IR");
                if (callee->param_count && !callee->param_types)
                    return fail(d, i, "CGEN", "call target %s has missing parameter metadata", callee->name);
                if (i->operands[1].kind != R6_VAL_INT || i->operands[1].as.i64 < 0)
                    return fail(d, i, "CGEN", "call arity must be a non-negative Int");
                arity = (size_t)i->operands[1].as.i64;
                if (arity > arg_count)
                    return fail(d, i, "CGEN", "call arity exceeds the pending argument count");
                if (arity != callee->param_count)
                    return fail(d, i, "CGEN", "call to %s passes %zu argument(s); declaration requires %zu",
                                callee->name, arity, callee->param_count);
                base = arg_count - arity;
                for (k = 0; k < arity; ++k) {
                    const char *t;
                    R6Value *v = (R6Value *)&arg_values[base + k]->operands[0];
                    if (!value_type(f, v, &t) ||
                        !word_convertible(m, t, callee->param_types[k]))
                        return fail(d, i, "CGEN", "call argument %zu type does not match %s", k, callee->name);
                    if (is_result(t) && (callee != find_function(m, callee->name) || callee->is_extern))
                        return fail(d, i, "CGEN", "a result argument of %s is outside the subset: only a declared Orbit function takes one",
                                    callee->name);
                }
                if (is_void(callee->return_type)) {
                    /* Spec 3.1 step 7: a void callee needs no destination; the
                       pinned builder still allocates one typed `void`, and R6
                       accepts both forms (a non-void dest would be a value
                       nobody may read). */
                    if (i->has_dest && !is_void(type_at(f, i->dest)))
                        return fail(d, i, "CGEN", "void call destination must be a void register");
                } else if (!i->has_dest ||
                           !(same_type(type_at(f, i->dest), callee->return_type) ||
                             ((is_void_ptr(callee->return_type) || is_void_ptr(type_at(f, i->dest))) &&
                              word_convertible(m, callee->return_type, type_at(f, i->dest)))))
                    return fail(d, i, "CGEN", "call destination does not match return type of %s", callee->name);
                if (is_result(callee->return_type) &&
                    (callee != find_function(m, callee->name) ||
                     (callee->is_extern && !extern_needs_arena(callee->name))))
                    return fail(d, i, "CGEN", "a result returned by %s is outside the subset: only a declared Orbit function or an arena-taking runtime extern returns one",
                                callee->name);
                arg_count -= arity; /* E-05: consume exactly the last O2 pending args. */
                break;
            }
            if (i->operands[0].kind == R6_VAL_SYMBOL)
                return fail(d, i, "CGEN", "call target %s is not declared in the module",
                            i->operands[0].as.string);
            if (i->operands[0].kind == R6_VAL_STRING) {
                /* Variant construction: the pinned builder emits `call` with
                   the bare variant name as a String target, arity = payload
                   count, and a fresh void* destination. */
                size_t variant_index = 0;
                const R6Type *variant_type =
                    find_union_variant(m, i->operands[0].as.string, &variant_index);
                const char *rich;
                size_t parts;
                if (!variant_type)
                    return fail(d, i, "CGEN", "call target %s is not a declared function, model, or union variant",
                                i->operands[0].as.string);
                if (i->operands[1].kind != R6_VAL_INT || i->operands[1].as.i64 < 0)
                    return fail(d, i, "CGEN", "call arity must be a non-negative Int");
                arity = (size_t)i->operands[1].as.i64;
                if (arity > arg_count)
                    return fail(d, i, "CGEN", "call arity exceeds the pending argument count");
                base = arg_count - arity;
                if (!valid_dest(f, i, true, d) || !is_void_ptr(type_at(f, i->dest)))
                    return fail(d, i, "CGEN", "union constructor call requires a void* destination");
                rich = variant_type->rich_variants[variant_index];
                parts = union_payload_count(rich);
                if (arity != parts)
                    return fail(d, i, "CGEN", "union variant %s takes %zu payload value(s); call passes %zu",
                                variant_type->variants[variant_index], parts, arity);
                for (k = 0; k < arity; ++k) {
                    char part[64];
                    const char *t;
                    R6Value *v = (R6Value *)&arg_values[base + k]->operands[0];
                    if (!union_payload_part(rich, k, part, sizeof part))
                        return fail(d, i, "CGEN", "union variant %s payloads are outside the M1 native subset",
                                    variant_type->variants[variant_index]);
                    if (!value_type(f, v, &t) || !same_type(t, payload_reg_type(m, part)))
                        return fail(d, i, "CGEN", "union constructor argument %zu does not match payload %s", k, part);
                    if (v->kind == R6_VAL_STRING &&
                        (!v->as.string || v->string_len != strlen(v->as.string)))
                        return fail(d, i, "CGEN", "string literal contains NUL and cannot be passed through the C ABI");
                }
                arg_count -= arity;
                break;
            }
            if (i->operands[0].kind == R6_VAL_REG) {
                /* Indirect call through a function value (buildFnVarCall,
                   builder.orb:3646-3662): the callee register's spelling is
                   the checked signature; arity rides operand2 (V-13). */
                const char *ct = type_at(f, i->operands[0].as.i32);
                char fret[16], fparam[16];
                size_t fparams = 0;
                if (!ct || !fn_type_ok(ct, fret, sizeof fret, &fparams))
                    return fail(d, i, "CGEN", "call through register %d requires a function value signature",
                                i->operands[0].as.i32);
                if (i->operands[1].kind != R6_VAL_INT || i->operands[1].as.i64 < 0)
                    return fail(d, i, "CGEN", "call arity must be a non-negative Int");
                arity = (size_t)i->operands[1].as.i64;
                if (arity > arg_count)
                    return fail(d, i, "CGEN", "call arity exceeds the pending argument count");
                if (arity != fparams)
                    return fail(d, i, "CGEN", "function value call passes %zu argument(s); the signature takes %zu",
                                arity, fparams);
                base = arg_count - arity;
                for (k = 0; k < arity; ++k) {
                    const char *at;
                    R6Value *av = (R6Value *)&arg_values[base + k]->operands[0];
                    if (!fn_type_param(ct, k, fparam, sizeof fparam) ||
                        !value_type(f, av, &at) || !same_type(at, fparam))
                        return fail(d, i, "CGEN", "function value call argument %zu does not match the signature", k);
                }
                if (!i->has_dest || !same_type(type_at(f, i->dest), fret))
                    return fail(d, i, "CGEN", "function value call destination does not match return type %s", fret);
                arg_count -= arity;
                break;
            }
            return fail(d, i, "CGEN", "call requires a direct native symbol or a declared model constructor");
        }
        case R6_OP_RET:
            /* D-M1-28: a void function returns without reading its operand.
               The pinned lowering emits a bare `return;` for both ret forms
               once the return type is void (c_backend.orb:3435-3445 and
               :3466-3471), so a carried value is admitted and ignored. */
            /* D-M1-49: a bare ret in a value function returns the zero of
               the type (`return 0;`, `0.0` or `NULL`, c_backend.orb ret arm).
               A result has no zero in that arm (`return NULL;` is not an
               OrbitResult), so it stays refused. */
            if (strcmp(f->return_type, "void") != 0 &&
                (!i->operands[0].kind || i->operands[0].kind == R6_VAL_NONE) &&
                !is_result(f->return_type)) {
                break;
            }
            if (strcmp(f->return_type, "void") != 0) {
                if (!i->operands[0].kind || i->operands[0].kind == R6_VAL_NONE ||
                    !value_type(f, &i->operands[0], &a) ||
                    !(same_type(a, f->return_type) ||
                      (ref_elem_type(m, f->return_type) && null_register(f, &i->operands[0]))))
                    return fail(d, i, "CGEN", "ret value does not match function return type");
                /* D-M1-43: a result is copied through the sret pointer, so
                   only a result register can be returned. */
                if (is_result(a) && i->operands[0].kind != R6_VAL_REG)
                    return fail(d, i, "CGEN", "a returned result must be a result register");
                if (i->operands[0].kind == R6_VAL_REG && !type_at(f, i->operands[0].as.i32))
                    return fail(d, i, "CGEN", "ret register is out of range");
            }
            break;
        case R6_OP_RESULT_OK:
            if (!valid_dest(f, i, true, d) || !is_result(type_at(f, i->dest)) ||
                !value_type(f, &i->operands[0], &a) ||
                !(is_int(a) || is_float(a) || is_bool(a) || is_string(a)))
                return fail(d, i, "CGEN", "result_ok requires a result destination and an int, float, bool, or string operand");
            if (i->operands[0].kind == R6_VAL_STRING &&
                i->operands[0].string_len != strlen(i->operands[0].as.string))
                return fail(d, i, "CGEN", "string literal contains NUL and cannot be passed through the C ABI");
            break;
        case R6_OP_RESULT_ERR:
            if (!valid_dest(f, i, true, d) || !is_result(type_at(f, i->dest)))
                return fail(d, i, "CGEN", "result_err requires a result destination");
            if (i->operands[1].kind == R6_VAL_NONE) {
                /* Single-argument form: O1 carries the error code. */
                if (!value_type(f, &i->operands[0], &a) || !is_int(a))
                    return fail(d, i, "CGEN", "result_err without a message requires an int code operand");
            } else {
                if (!value_type(f, &i->operands[0], &a) || !is_string(a))
                    return fail(d, i, "CGEN", "result_err message operand must be a string");
                if (i->operands[0].kind == R6_VAL_STRING &&
                    i->operands[0].string_len != strlen(i->operands[0].as.string))
                    return fail(d, i, "CGEN", "string literal contains NUL and cannot be passed through the C ABI");
                if (!value_type(f, &i->operands[1], &b) || !is_int(b))
                    return fail(d, i, "CGEN", "result_err code operand must be an int");
            }
            break;
        case R6_OP_RESULT_IS_OK:
            if (!valid_dest(f, i, true, d) || !is_bool(type_at(f, i->dest)) ||
                i->operands[0].kind != R6_VAL_REG ||
                !is_result(type_at(f, i->operands[0].as.i32)))
                return fail(d, i, "CGEN", "result_is_ok requires a result register operand and a bool destination");
            break;
        case R6_OP_RESULT_UNWRAP:
            if (!valid_dest(f, i, true, d) ||
                i->operands[0].kind != R6_VAL_REG ||
                !is_result(type_at(f, i->operands[0].as.i32)) ||
                !(is_int(type_at(f, i->dest)) || is_float(type_at(f, i->dest)) ||
                  is_string(type_at(f, i->dest)) || is_list(type_at(f, i->dest)) ||
                  (is_void_ptr(type_at(f, i->dest)) &&
                   same_type(unwrap_use_type(f, pc), "void*"))))
                return fail(d, i, "CGEN", "result_unwrap requires a result register operand and an int, float, string or list destination, or a void* one nothing reads");
            break;
        case R6_OP_RESULT_UNWRAP_ERR:
            if (!valid_dest(f, i, true, d) || !is_string(type_at(f, i->dest)) ||
                i->operands[0].kind != R6_VAL_REG ||
                !is_result(type_at(f, i->operands[0].as.i32)))
                return fail(d, i, "CGEN", "result_unwrap_err requires a result register operand and a string destination");
            break;
        case R6_OP_NOP:
        case R6_OP_BEGIN_BLOCK:
        case R6_OP_END_BLOCK:
            /* No observable effect at the pin: nop appends nothing and the
               blocks render as C braces (c_backend.orb:2592, 2835-2842). */
            break;
        default:
            return fail(d, i, "CGEN", "unsupported opcode %s at instruction %zu",
                        r6_opcode_name(i->opcode), pc);
        }
    }
    if (arg_count)
        return fail(d, NULL, "CGEN", "function %s ends with %zu pending arg instruction(s)", f->name, arg_count);
    return true;
}

static void emit_string(FILE *o, const char *s, unsigned long long id) {
    const unsigned char *p = (const unsigned char *)s;
    fputs("\t.section .rdata,\"dr\"\n", o);
    size_t col = 0;
    fprintf(o, ".LCr6_%llu:\n\t.byte ", id);
    while (*p) {
        /* D-M1-101: the assembler reads lines of at most 1023 bytes. */
        fprintf(o, "%u", (unsigned)*p++);
        if (++col == 64) {
            fputs("\n\t.byte ", o);
            col = 0;
        } else {
            fputc(',', o);
        }
    }
    fputs("0\n\t.text\n", o);
}

static void emit_load_arg(FILE *o, const R6Function *f, const R6Value *v,
                          const char *type, size_t index, unsigned long long id);

/* D-M1-94: an int is 64 bits. `movq $imm` encodes a sign-extended imm32, so a
 * value outside that range goes through `movabsq`, which the assembler
 * encodes into %rax only. */
static bool fits_imm32(int64_t v) {
    return v >= INT32_MIN && v <= INT32_MAX;
}

static void emit_int_rax(FILE *o, int64_t v) {
    fprintf(o, fits_imm32(v) ? "\tmovq $%lld, %%rax\n" : "\tmovabsq $%lld, %%rax\n",
            (long long)v);
}

static void emit_int_slot(FILE *o, int64_t v, unsigned slot) {
    if (fits_imm32(v)) {
        fprintf(o, "\tmovq $%lld, -%u(%%rbp)\n", (long long)v, slot);
        return;
    }
    emit_int_rax(o, v);
    fprintf(o, "\tmovq %%rax, -%u(%%rbp)\n", slot);
}

/* D-M1-95: a division or remainder with a divisor that is not a constant
 * tests for a zero divisor and for INT64_MIN / -1, and branches to a block
 * after the function body. The block loads the file, the line and the kind
 * (0 division by zero, 1 overflow) into %rcx, %rdx and %r8 and calls
 * `orbit_div_error`, which prints the message and exits with status 2. */
typedef struct {
    size_t pc;
    unsigned long long file_id;
    int64_t line;
} DivTrap;

static DivTrap *div_traps;
static size_t div_trap_count, div_trap_cap;

/* The IR `line` of a div or mod; 0 when the emitter left it out, which the
 * runtime prints as `at <file>` with no line. */
static int64_t div_trap_line(const R6Instruction *i) {
    return i->source_line;
}

static bool defer_div_trap(size_t pc, unsigned long long file_id, int64_t line) {
    if (div_trap_count == div_trap_cap) {
        size_t cap = div_trap_cap ? div_trap_cap * 2 : 16;
        DivTrap *grown = (DivTrap *)realloc(div_traps, cap * sizeof *grown);
        if (!grown) return false;
        div_traps = grown;
        div_trap_cap = cap;
    }
    div_traps[div_trap_count].pc = pc;
    div_traps[div_trap_count].file_id = file_id;
    div_traps[div_trap_count].line = line;
    ++div_trap_count;
    return true;
}

static void emit_div_traps(FILE *o, size_t fn_index) {
    size_t k;
    for (k = 0; k < div_trap_count; ++k) {
        const DivTrap *t = &div_traps[k];
        int kind;
        for (kind = 0; kind < 2; ++kind)
            fprintf(o, ".Lr6_f%zu_%s_%zu:\n\tleaq .LCr6_%llu(%%rip), %%rcx\n"
                       "\tmovq $%lld, %%rdx\n\tmovq $%d, %%r8\n"
                       "\tcall orbit_div_error\n\tud2\n",
                    fn_index, kind ? "dov" : "dz", t->pc, t->file_id,
                    (long long)t->line, kind);
    }
}

/* An int argument of a float parameter: the C backend casts each argument to
 * the parameter's C type, so the callee receives `(orbit_float)(x)`. */
static void emit_load_arg_as(FILE *o, const R6Function *f, const R6Value *v,
                             const char *type, const char *param_type, size_t index,
                             unsigned long long id) {
    static const char *xmm[] = {"%xmm0", "%xmm1", "%xmm2", "%xmm3"};
    if (!int_widens_to_float(type, param_type)) {
        emit_load_arg(o, f, v, type, index, id);
        return;
    }
    if (v->kind == R6_VAL_INT) {
        R6Value w = *v;
        double x = (double)v->as.i64;
        w.kind = R6_VAL_FLOAT64;
        memcpy(&w.as.f64_bits, &x, sizeof x);
        emit_load_arg(o, f, &w, "float", index, id);
        return;
    }
    if (index >= 4) {
        fprintf(o, "\tcvtsi2sdq -%u(%%rbp), %%xmm15\n\tmovsd %%xmm15, %u(%%rsp)\n",
                (unsigned)(v->as.i32 + 1) * 8, (unsigned)(32 + (index - 4) * 8));
        return;
    }
    fprintf(o, "\tcvtsi2sdq -%u(%%rbp), %s\n", (unsigned)(v->as.i32 + 1) * 8, xmm[index]);
}

static void emit_load_arg(FILE *o, const R6Function *f, const R6Value *v,
                          const char *type, size_t index, unsigned long long id) {
    static const char *gp[] = {"%rcx", "%rdx", "%r8", "%r9"};
    static const char *xmm[] = {"%xmm0", "%xmm1", "%xmm2", "%xmm3"};
    (void)f;
    if (index >= 4) {
        unsigned off = (unsigned)(32 + (index - 4) * 8);
        if (is_float(type)) {
            if (v->kind == R6_VAL_REG) fprintf(o, "\tmovsd -%u(%%rbp), %%xmm15\n", (unsigned)(v->as.i32 + 1) * 8);
            else fprintf(o, "\tmovabsq $0x%016llx, %%rax\n\tmovq %%rax, %%xmm15\n", (unsigned long long)v->as.f64_bits);
            fprintf(o, "\tmovsd %%xmm15, %u(%%rsp)\n", off);
        } else {
            if (v->kind == R6_VAL_REG) fprintf(o, "\tmovq -%u(%%rbp), %%rax\n", (unsigned)(v->as.i32 + 1) * 8);
            else if (v->kind == R6_VAL_STRING) fprintf(o, "\tleaq .LCr6_%llu(%%rip), %%rax\n", id);
            else if (v->kind == R6_VAL_INT) emit_int_rax(o, v->as.i64);
            else fprintf(o, "\tmovl $%d, %%eax\n", v->as.boolean ? 1 : 0);
            fprintf(o, "\tmovq %%rax, %u(%%rsp)\n", off);
        }
        return;
    }
    if (is_float(type)) {
        if (v->kind == R6_VAL_REG) fprintf(o, "\tmovsd -%u(%%rbp), %s\n", (unsigned)(v->as.i32 + 1) * 8, xmm[index]);
        else fprintf(o, "\tmovabsq $0x%016llx, %%rax\n\tmovq %%rax, %s\n", (unsigned long long)v->as.f64_bits, xmm[index]);
    } else {
        if (v->kind == R6_VAL_REG) fprintf(o, "\tmovq -%u(%%rbp), %s\n", (unsigned)(v->as.i32 + 1) * 8, gp[index]);
        else if (v->kind == R6_VAL_STRING) fprintf(o, "\tleaq .LCr6_%llu(%%rip), %s\n", id, gp[index]);
        else if (v->kind == R6_VAL_INT) {
            if (fits_imm32(v->as.i64))
                fprintf(o, "\tmovq $%lld, %s\n", (long long)v->as.i64, gp[index]);
            else {
                emit_int_rax(o, v->as.i64);
                fprintf(o, "\tmovq %%rax, %s\n", gp[index]);
            }
        }
        else fprintf(o, "\tmovl $%d, %s\n", v->as.boolean ? 1 : 0, index == 0 ? "%ecx" : index == 1 ? "%edx" : index == 2 ? "%r8d" : "%r9d");
    }
}

static size_t max_outgoing_args(const R6Module *m, const R6Function *f) {
    size_t pc, pending = 0, maximum = 0;
    for (pc = 0; pc < f->instruction_count; ++pc) {
        const R6Instruction *i = &f->instructions[pc];
        if (i->opcode == R6_OP_ARG && !i->has_dest) ++pending;
        else if (i->opcode == R6_OP_CALL) {
            size_t arity = i->operands[1].kind == R6_VAL_INT && i->operands[1].as.i64 >= 0
                               ? (size_t)i->operands[1].as.i64 : pending;
            /* D-M1-27: an arena-taking extern carries the arena first, so its
               outgoing tail is one slot longer than the IR arity. The IR arity
               is still what is consumed from the pending list. */
            size_t slots = arity;
            if ((i->operands[0].kind == R6_VAL_STRING || i->operands[0].kind == R6_VAL_SYMBOL) &&
                i->operands[0].as.string && call_needs_arena(m, i->operands[0].as.string))
                ++slots;
            if ((i->operands[0].kind == R6_VAL_STRING || i->operands[0].kind == R6_VAL_SYMBOL) &&
                i->operands[0].as.string && find_function(m, i->operands[0].as.string) &&
                is_result(find_function(m, i->operands[0].as.string)->return_type))
                ++slots;
            if (slots > maximum) maximum = slots;
            pending = arity <= pending ? pending - arity : 0;
        } else if (i->opcode == R6_OP_LIST_CREATE && maximum < 5) {
            /* OrbitResult sret pointer plus four list_create_typed arguments. */
            maximum = 5;
        } else if ((i->opcode == R6_OP_LIST_GET || i->opcode == R6_OP_LIST_POP) &&
                   maximum < 6) {
            /* orbit_list_index_error has six positional Win64 arguments. */
            maximum = 6;
        }
    }
    return maximum;
}

static bool uses_result_scratch(const R6Function *f) {
    size_t pc;
    for (pc = 0; pc < f->instruction_count; ++pc)
        if (f->instructions[pc].opcode == R6_OP_LIST_CREATE ||
            f->instructions[pc].opcode == R6_OP_LIST_PUSH ||
            f->instructions[pc].opcode == R6_OP_LIST_SET ||
            f->instructions[pc].opcode == R6_OP_LIST_POP) return true;
    return false;
}

/* The 24-byte OrbitResult scratch (sret target of every list call) lives
 * directly below the register/parameter/local slots, never inside them: a
 * callee's sret store would otherwise clobber a live slot parked in the
 * last 24 bytes of the slot range. The returned value is the distance from
 * %rbp to the buffer base, so the block covers distances
 * [slots * 8 + 1, slots * 8 + 24] (field offsets: ok +0, error_code +4,
 * error_msg +8, value +16, i.e. `scratch - 16` is the value word). */
static unsigned result_scratch_offset(const R6Function *f, bool is_main) {
    size_t slots = (size_t)f->register_count + hidden_slots(f, is_main) +
                   f->param_count + local_count(f);
    return (unsigned)(slots * 8 + 24);
}

/* The slot below the registers that holds a hidden first C argument: the
 * arena for main, and (D-M1-43) the sret pointer of a function that returns
 * an OrbitResult. main returns int, so the two never meet. */
static size_t hidden_slots(const R6Function *f, bool is_main) {
    return is_main || is_result(f->return_type) ? 1 : 0;
}

static bool has_result_registers(const R6Function *f) {
    size_t k;
    for (k = 0; k < f->register_type_count; ++k)
        if (is_result(f->register_types[k])) return true;
    return false;
}

/* D-M1-74: a result parameter arrives as the pointer Win64 passes for a
 * 24-byte struct and is copied into a cell of its own at entry. The cells of
 * the parameters follow those of the locals. */
static bool has_result_params(const R6Function *f) {
    size_t k;
    for (k = 0; k < f->param_count; ++k)
        if (is_result(f->param_types[k])) return true;
    return false;
}

static int32_t param_cell_register(const R6Function *f, size_t param) {
    return (int32_t)(f->register_count + local_count(f) + param);
}

/* Result registers hold a private 24-byte OrbitResult cell below the list
 * scratch area. The returned value is the distance from %rbp to the cell
 * base (field offsets: ok +0, error_code +4, error_msg +8, value +16).
 * Cells never overlap: cell(reg) covers distances [D-23, D] with
 * D = slots * 8 + 24 + 24 * (reg + 1), and the scratch area covers
 * [slots * 8 + 1, slots * 8 + 24]. */
static unsigned result_cell_offset(const R6Function *f, bool is_main, int32_t reg) {
    size_t slots = (size_t)f->register_count + hidden_slots(f, is_main) +
                   f->param_count + local_count(f);
    return (unsigned)(slots * 8 + 24 + 24u * (unsigned)(reg + 1));
}

/* Copies one 24-byte OrbitResult cell to another; both are distances from
 * %rbp to the cell base, as result_cell_offset returns them. */
static void emit_cell_copy(FILE *o, unsigned from, unsigned to) {
    unsigned k;
    for (k = 0; k < 24; k += 8)
        fprintf(o, "\tmovq -%u(%%rbp), %%rax\n\tmovq %%rax, -%u(%%rbp)\n", from - k, to - k);
}

/* D-M1-21: the operation opcodes whose value operands may carry an inline
 * literal, and how many of their operands are values. This is exactly the
 * set the pinned corpus produces (2661 operands across 79/94 documents,
 * kinds Int/String/Bool); a literal in any other operation still fails
 * closed through operand_reg(). */
static unsigned value_operand_count(R6Opcode op) {
    switch (op) {
    case R6_OP_ADD: case R6_OP_SUB: case R6_OP_MUL: case R6_OP_DIV:
    case R6_OP_MOD: case R6_OP_EQ: case R6_OP_NE: case R6_OP_LT:
    case R6_OP_LE: case R6_OP_GT: case R6_OP_GE:
    case R6_OP_BIT_AND: case R6_OP_BIT_OR: case R6_OP_BIT_XOR:
    case R6_OP_SHL: case R6_OP_SHR:
        return 2;
    case R6_OP_BIT_NOT:
        return 1;
    default:
        return 0;
    }
}

static bool uses_literal_scratch(const R6Function *f) {
    size_t pc;
    for (pc = 0; pc < f->instruction_count; ++pc) {
        const R6Instruction *i = &f->instructions[pc];
        unsigned n, count = value_operand_count(i->opcode);
        for (n = 0; n < count; ++n)
            if (i->operands[n].kind != R6_VAL_REG) return true;
    }
    return false;
}

/* The bytes the list scratch and the result cells reserve below the register
 * slots; mirrors the frame arithmetic of emit_function(). */
static size_t result_area_bytes(const R6Function *f) {
    if (!uses_result_scratch(f) && !has_result_registers(f) && !has_result_params(f)) return 0;
    /* D-M1-45: one cell per register and one per local, so a result local
       has its own 24 bytes at cell index register_count + local index.
       D-M1-74: a function with a result parameter adds one cell per parameter. */
    return 24 + (has_result_registers(f) || has_result_params(f)
                     ? 24 * ((size_t)f->register_count + local_count(f) +
                             (has_result_params(f) ? f->param_count : 0)) : 0);
}

/* The two 8-byte immediate slots an operation with an inline literal needs,
 * placed below the result area so no existing offset moves and no cell or
 * scratch block is overlapped. `n` is the operand index (0 or 1). */
static unsigned literal_scratch_offset(const R6Function *f, bool is_main, unsigned n) {
    size_t slots = (size_t)f->register_count + hidden_slots(f, is_main) +
                   f->param_count + local_count(f);
    return (unsigned)(slots * 8 + result_area_bytes(f) + 8u * (n + 1));
}

/* D-M1-23: two 8-byte slots for the string a scalar operand of a string `add`
 * spreads into. They sit below the D-M1-21 immediate slots, so no existing
 * offset moves and no register, local or scratch block is overlapped. */
static bool uses_spread_scratch(const R6Function *f) {
    size_t pc;
    for (pc = 0; pc < f->instruction_count; ++pc) {
        const R6Instruction *i = &f->instructions[pc];
        if (i->opcode != R6_OP_ADD) continue;
        if (is_string(operand_type_of(f, i, 0)) !=
            is_string(operand_type_of(f, i, 1))) return true;
    }
    return false;
}

static unsigned spread_scratch_offset(const R6Function *f, bool is_main,
                                      unsigned n) {
    size_t slots = (size_t)f->register_count + hidden_slots(f, is_main) +
                   f->param_count + local_count(f);
    unsigned used = (unsigned)(slots * 8 + result_area_bytes(f));
    if (uses_literal_scratch(f)) used += 16;
    return used + 8u * (n + 1);
}

/* The distance from %rbp to the word holding operand `n` of a D-M1-21
 * operation: the register's own slot, or the scratch slot its literal was
 * materialized into. */
static unsigned operand_slot(const R6Function *f, bool is_main,
                             const R6Instruction *i, unsigned n) {
    if (i->operands[n].kind == R6_VAL_REG)
        return (unsigned)(i->operands[n].as.i32 + 1) * 8;
    return literal_scratch_offset(f, is_main, n);
}

/* Writes the immediate slots of an operation before the operation reads
 * them, with the same materialization load_const/decl_var already use. */
static void materialize_literals(FILE *o, const R6Function *f,
                                 const R6Instruction *i, bool is_main,
                                 unsigned long long *string_id) {
    unsigned n, count = value_operand_count(i->opcode);
    for (n = 0; n < count; ++n) {
        const R6Value *v = &i->operands[n];
        unsigned slot = literal_scratch_offset(f, is_main, n);
        if (v->kind == R6_VAL_REG) continue;
        if (v->kind == R6_VAL_STRING) {
            emit_string(o, v->as.string, *string_id);
            fprintf(o, "\tleaq .LCr6_%llu(%%rip), %%rax\n\tmovq %%rax, -%u(%%rbp)\n",
                    (*string_id)++, slot);
        } else if (v->kind == R6_VAL_INT)
            emit_int_slot(o, v->as.i64, slot);
        else
            fprintf(o, "\tmovl $%d, -%u(%%rbp)\n", v->as.boolean ? 1 : 0, slot);
    }
}

/* D-M1-23: the executable arena. `main` receives it as its first ABI parameter
 * (the pinned backend's `_init_arena`) and every other function reads the
 * file-scope global R6 mirrors in r6_arena. */
static void emit_load_arena(FILE *o, const R6Function *f, bool is_main,
                            const char *reg) {
    if (is_main)
        fprintf(o, "\tmovq -%u(%%rbp), %s\n",
                (unsigned)(f->register_count + 1) * 8, reg);
    else
        fprintf(o, "\tmovq r6_arena(%%rip), %s\n", reg);
}

/* D-M1-23: spreads one scalar operand of a string `add` into a string with the
 * converter the pinned backend's stringifyConcatOperand emits
 * (c_backend.orb:4347-4384): int -> orbit_int_to_string_selfhost (arena-free,
 * it allocates in the runtime's global arena), bool -> orbit_bool_to_string(
 * arena, v), float -> orbit_float_to_string(arena, v). The result lands in
 * `slot` so the concat reads it from memory and the source word stays live in
 * its own slot. */
static unsigned emit_spread(FILE *o, const R6Function *f, bool is_main,
                            const char *type, unsigned from, unsigned slot) {
    if (is_int(type)) {
        fprintf(o, "\tmovq -%u(%%rbp), %%rcx\n"
                   "\tcall orbit_int_to_string_selfhost\n", from);
    } else if (is_float(type)) {
        emit_load_arena(o, f, is_main, "%rcx");
        fprintf(o, "\tmovsd -%u(%%rbp), %%xmm1\n"
                   "\tcall orbit_float_to_string\n", from);
    } else {
        emit_load_arena(o, f, is_main, "%rcx");
        fprintf(o, "\tmovq -%u(%%rbp), %%rdx\n"
                   "\tcall orbit_bool_to_string\n", from);
    }
    fprintf(o, "\tmovq %%rax, -%u(%%rbp)\n", slot);
    return slot;
}

/* The pinned C backend assigns the executable arena to a file-scope global
 * (`OrbitArena* arena = NULL;` + `arena = _init_arena;` in orbit_main) and
 * every non-main function that creates a list or allocates a model instance
 * reads it from there. R6 mirrors that contract with the private r6_arena
 * slot in .data, so the global is only emitted when some non-main function
 * needs it. D-M1-23 adds the string `add`: its concatenation and its spread
 * converters take the same arena. */
static bool module_needs_arena_global(const R6Module *m) {
    size_t n;
    for (n = 0; n < m->function_count; ++n) {
        const R6Function *f = &m->functions[n];
        size_t pc;
        if (f->is_extern || !f->live || !f->name || strcmp(f->name, "main") == 0) continue;
        for (pc = 0; pc < f->instruction_count; ++pc) {
            const R6Instruction *i = &f->instructions[pc];
            if (i->opcode == R6_OP_LIST_CREATE) return true;
            if (i->opcode == R6_OP_ADD && is_string(type_at(f, i->dest))) return true;
            if (i->opcode == R6_OP_CALL && i->operands[0].kind == R6_VAL_STRING &&
                (find_model(m, i->operands[0].as.string) ||
                 find_union_variant(m, i->operands[0].as.string, NULL))) return true;
            /* D-M1-26: the builtin `print` reads the arena for its single
             * float argument (orbit_float_to_string(arena, v)). The name is
             * intercepted before any declaration (a declared extern `print`
             * is still the builtin), so no function lookup here; conservative
             * for the string and int forms, which do not touch the arena but
             * cost nothing. */
            if (i->opcode == R6_OP_CALL && i->operands[0].kind == R6_VAL_STRING &&
                strcmp(i->operands[0].as.string, "print") == 0 &&
                !find_model(m, i->operands[0].as.string) &&
                !find_union_variant(m, i->operands[0].as.string, NULL)) return true;
            /* D-M1-27: an arena-taking extern reads the arena in every
             * function, including main, so the file-scope mirror must exist. */
            if (i->opcode == R6_OP_CALL &&
                (i->operands[0].kind == R6_VAL_STRING || i->operands[0].kind == R6_VAL_SYMBOL) &&
                i->operands[0].as.string && call_needs_arena(m, i->operands[0].as.string)) return true;
        }
    }
    return false;
}

/* Shared inline read sequence for list_get/list_pop, mirroring the pinned
 * ORBIT_LIST_READ prologue: null list/data and kind checks followed by the
 * slot decode for the element kind. list_pop computes the index as
 * `len - 1` (with the empty-list index_error the pin emits first) and calls
 * orbit_list_pop after the read, so the slot is still in range. */
static void emit_list_read(FILE *o, const R6Function *f, size_t fn_index,
                           size_t pc, unsigned list_off, unsigned index_off,
                           unsigned dest_off, int kind, int decode, bool is_pop,
                           unsigned long long where_id, unsigned long long file_id) {
    unsigned scratch = result_scratch_offset(f, strcmp(f->name, "main") == 0);
    const char *tag = is_pop ? "list_pop" : "list_get";
    char null_list[112], null_data[112], out_of_range[112], kind_error[112];
    char index_error[112], kind_ok[112], done[112], empty[112], len_zero[112], pop[112];
    snprintf(null_list, sizeof null_list, ".Lr6_f%zu_%s_null_list_%zu", fn_index, tag, pc);
    snprintf(null_data, sizeof null_data, ".Lr6_f%zu_%s_null_data_%zu", fn_index, tag, pc);
    snprintf(out_of_range, sizeof out_of_range, ".Lr6_f%zu_%s_oob_%zu", fn_index, tag, pc);
    snprintf(kind_error, sizeof kind_error, ".Lr6_f%zu_%s_kind_error_%zu", fn_index, tag, pc);
    snprintf(index_error, sizeof index_error, ".Lr6_f%zu_%s_index_error_%zu", fn_index, tag, pc);
    snprintf(kind_ok, sizeof kind_ok, ".Lr6_f%zu_%s_kind_ok_%zu", fn_index, tag, pc);
    snprintf(done, sizeof done, ".Lr6_f%zu_%s_done_%zu", fn_index, tag, pc);
    snprintf(empty, sizeof empty, ".Lr6_f%zu_%s_empty_%zu", fn_index, tag, pc);
    snprintf(len_zero, sizeof len_zero, ".Lr6_f%zu_%s_len_zero_%zu", fn_index, tag, pc);
    snprintf(pop, sizeof pop, ".Lr6_f%zu_%s_pop_%zu", fn_index, tag, pc);
    if (is_pop) {
        /* _pl = (orbit_int)orbit_list_len(list); if (_pl <= 0) index_error. */
        fprintf(o, "\tmovq -%u(%%rbp), %%r10\n\tmovl $0, %%eax\n"
                   "\ttestq %%r10, %%r10\n\tje %s\n\tmovq 8(%%r10), %%rax\n"
                   "%s:\n\ttestq %%rax, %%rax\n\tjle %s\n"
                   "\tmovq %%rax, %%r11\n\tmovl $1, %%ecx\n\tsubq %%rcx, %%r11\n"
                   "\tmovq 0(%%r10), %%r9\n\ttestq %%r9, %%r9\n\tje %s\n",
                list_off, len_zero, len_zero, empty, null_data);
    } else {
        fprintf(o, "\tmovq -%u(%%rbp), %%r10\n\tmovq -%u(%%rbp), %%r11\n"
                   "\ttestq %%r10, %%r10\n\tje %s\n\tmovq 0(%%r10), %%r9\n"
                   "\ttestq %%r9, %%r9\n\tje %s\n\tmovq 8(%%r10), %%r8\n"
                   "\tcmpq %%r8, %%r11\n\tjae %s\n",
                list_off, index_off, null_list, null_data, out_of_range);
    }
    /* An opaque read skips the kind check, as orbit_list_checked does. */
    if (kind != 0)
        fprintf(o, "\tcmpl $%d, 32(%%r10)\n\tje %s\n\tcmpl $0, 32(%%r10)\n\tjne %s\n",
                kind, kind_ok, kind_error);
    fprintf(o, "%s:\n\tmovq 24(%%r10), %%rax\n\timulq %%r11, %%rax\n"
               "\taddq %%rax, %%r9\n\tmovq 0(%%r9), %%rax\n",
            kind_ok);
    switch (decode) {
    case 1: /* string and ref (D-M1-57): the slot word is the pointer. */
        fprintf(o, "\tmovq %%rax, -%u(%%rbp)\n", dest_off);
        break;
    case 3: /* float: the slot word holds the binary64 bits. */
        fprintf(o, "\tmovq %%rax, %%xmm0\n\tmovsd %%xmm0, -%u(%%rbp)\n", dest_off);
        break;
    case 4: /* bool: decode `word ? true : false`. */
        fprintf(o, "\ttestq %%rax, %%rax\n\tsetne %%al\n\tmovzbl %%al, %%eax\n"
                   "\tmovq %%rax, -%u(%%rbp)\n", dest_off);
        break;
    default: /* int: the slot word is the value (D-M1-94). */
        fprintf(o, "\tmovq %%rax, -%u(%%rbp)\n", dest_off);
        break;
    }
    fprintf(o, "\tjmp %s\n", is_pop ? pop : done);
    if (!is_pop)
        fprintf(o, "%s:\n\tmovl $0, %%r8d\n\tmovl $0, %%r9d\n\tjmp %s\n",
                null_list, index_error);
    fprintf(o, "%s:\n\tmovq 8(%%r10), %%r8\n\tmovl 32(%%r10), %%r9d\n\tjmp %s\n",
            null_data, index_error);
    if (!is_pop)
        fprintf(o, "%s:\n\tmovl 32(%%r10), %%r9d\n\tjmp %s\n", out_of_range, index_error);
    if (kind != 0)
        fprintf(o, "%s:\n\tmovl 32(%%r10), %%ecx\n\tmovl $%d, %%edx\n"
                   "\tcall orbit_list_kind_error\n\tud2\n",
                kind_error, kind);
    if (is_pop)
        fprintf(o, "%s:\n\tmovl $0, %%r11d\n\tmovl $0, %%r8d\n\tmovl $0, %%r9d\n"
                   "\tjmp %s\n",
                empty, index_error);
    fprintf(o, "%s:\n\tleaq .LCr6_%llu(%%rip), %%rcx\n\tmovq %%r11, %%rdx\n"
               "\tleaq .LCr6_%llu(%%rip), %%rax\n\tmovq %%rax, 32(%%rsp)\n"
               "\tmovq $0, 40(%%rsp)\n\tcall orbit_list_index_error\n\tud2\n",
            index_error, (unsigned long long)where_id, (unsigned long long)file_id);
    if (is_pop)
        fprintf(o, "%s:\n\tleaq -%u(%%rbp), %%rcx\n\tmovq -%u(%%rbp), %%rdx\n"
                   "\tcall orbit_list_pop\n",
                pop, scratch, list_off);
    fprintf(o, "%s:\n", done);
}

/* D-M1-70: true when an instruction other than the comparison at `cmp_pc` and
 * the jump after it names the comparison's register. */
static bool register_named_elsewhere(const R6Function *f, size_t cmp_pc) {
    const R6Instruction *cmp = &f->instructions[cmp_pc];
    size_t k;
    int n;
    for (k = 0; k < f->instruction_count; ++k) {
        const R6Instruction *other = &f->instructions[k];
        if (k == cmp_pc || k == cmp_pc + 1) continue;
        if (other->has_dest && other->dest == cmp->dest) return true;
        for (n = 0; n < 3; ++n)
            if (other->operands[n].kind == R6_VAL_REG && other->operands[n].as.i32 == cmp->dest)
                return true;
    }
    return false;
}

/* D-M1-66: the jump that leaves when the comparison before a jump_if_false is
 * false. The comparison already left its flags and stores its result, so the
 * jump reads the flags instead of loading the stored result and comparing it
 * with zero. Float comparisons are not flag-based (parity) and keep the load.
 * A string `eq` or `ne` leaves the flags of `testl` on the `strcmp` result
 * only when D-M1-70 drops the stored result. NULL when the jump follows
 * anything else. */
static const char *fused_false_jump(const R6Function *f, size_t pc) {
    const R6Instruction *jump = &f->instructions[pc];
    const R6Instruction *cmp;
    const char *t0, *t1;
    if (pc == 0 || jump->operands[0].kind != R6_VAL_REG) return NULL;
    cmp = &f->instructions[pc - 1];
    if (!cmp->has_dest || cmp->dest != jump->operands[0].as.i32) return NULL;
    if (cmp->opcode != R6_OP_EQ && cmp->opcode != R6_OP_NE && cmp->opcode != R6_OP_LT &&
        cmp->opcode != R6_OP_LE && cmp->opcode != R6_OP_GT && cmp->opcode != R6_OP_GE)
        return NULL;
    t0 = operand_type_of(f, cmp, 0);
    t1 = operand_type_of(f, cmp, 1);
    if (is_float(t0) || is_float(t1)) return NULL;
    if ((cmp->opcode == R6_OP_EQ || cmp->opcode == R6_OP_NE) && is_string(t0) && is_string(t1) &&
        register_named_elsewhere(f, pc - 1))
        return NULL;
    switch (cmp->opcode) {
    case R6_OP_EQ: return "jne";
    case R6_OP_NE: return "je";
    case R6_OP_LT: return "jge";
    case R6_OP_LE: return "jg";
    case R6_OP_GT: return "jle";
    default: return "jl";
    }
}

/* D-M1-70: a comparison whose only reader is the fused jump after it needs
 * no stored result. True when the instruction after `cmp_pc` is that jump and
 * no other instruction of the function names the comparison's register. */
static bool flags_only_compare(const R6Function *f, size_t cmp_pc) {
    if (cmp_pc + 1 >= f->instruction_count) return false;
    if (f->instructions[cmp_pc + 1].opcode != R6_OP_JUMP_IF_FALSE ||
        !fused_false_jump(f, cmp_pc + 1))
        return false;
    return !register_named_elsewhere(f, cmp_pc);
}

static void emit_compare_result(FILE *o, const char *setcc, unsigned off, bool flags_only) {
    if (!flags_only)
        fprintf(o, "\t%s %%al\n\tmovzbl %%al, %%eax\n\tmovl %%eax, -%u(%%rbp)\n", setcc, off);
}

/* D-M1-72: ZF is set when two strings are equal. Most string comparisons of
 * the compiler differ in the first byte, so that byte and the pointers are
 * tested before the call to strcmp. A NULL operand faults on the byte load,
 * as strcmp does. */
static void emit_string_equality(FILE *o, size_t fn_index, size_t pc, unsigned aoff, unsigned boff) {
    fprintf(o, "\tmovq -%u(%%rbp), %%rcx\n\tmovq -%u(%%rbp), %%rdx\n"
               "\tmovzbl (%%rcx), %%eax\n\tcmpb (%%rdx), %%al\n\tjne .Lr6_f%zu_streq_%zu\n"
               "\ttestq %%rax, %%rax\n\tje .Lr6_f%zu_streq_%zu\n"
               "\tcmpq %%rcx, %%rdx\n\tje .Lr6_f%zu_streq_%zu\n"
               "\tcall strcmp\n\tcmpl $0, %%eax\n.Lr6_f%zu_streq_%zu:\n",
            aoff, boff, fn_index, pc, fn_index, pc, fn_index, pc, fn_index, pc);
}

/* D-M1-81, D-M1-96: the divisor of the `div` or `mod` at `pc` when it is an
 * inline int or a register that the straight-line code before it set with
 * `load_const`. A label between the definition and the use ends the search. */
static bool constant_register(const R6Function *f, size_t pc, int32_t reg, int depth, int64_t *out) {
    size_t k;
    for (k = pc; k-- > 0;) {
        const R6Instruction *p = &f->instructions[k];
        if (p->opcode == R6_OP_LABEL) return false;
        if (p->has_dest && p->dest == reg) {
            if (p->opcode == R6_OP_LOAD_CONST && p->operands[0].kind == R6_VAL_INT) {
                *out = p->operands[0].as.i64;
                return true;
            }
            if (p->opcode == R6_OP_NEG && depth < 2 && p->operands[0].kind == R6_VAL_REG &&
                constant_register(f, k, p->operands[0].as.i32, depth + 1, out)) {
                *out = (int64_t)(UINT64_C(0) - (uint64_t)*out);
                return true;
            }
            return false;
        }
    }
    return false;
}

static bool constant_divisor(const R6Function *f, size_t pc, int64_t *out) {
    const R6Value *v = &f->instructions[pc].operands[1];
    if (v->kind == R6_VAL_INT) {
        *out = v->as.i64;
        return true;
    }
    return v->kind == R6_VAL_REG && constant_register(f, pc, v->as.i32, 0, out);
}

/* D-M1-96: signed 64-bit magic number and shift for a divisor of two or more
 * in magnitude that is not a power of two (Hacker's Delight, figure 10-1 with
 * the word size at 64). */
static void signed_magic(int64_t d, int64_t *mul, int *shift) {
    const uint64_t two63 = UINT64_C(1) << 63;
    uint64_t ad = d < 0 ? UINT64_C(0) - (uint64_t)d : (uint64_t)d;
    uint64_t t = two63 + ((uint64_t)d >> 63);
    uint64_t anc = t - 1 - t % ad;
    uint64_t q1 = two63 / anc, r1 = two63 - q1 * anc;
    uint64_t q2 = two63 / ad, r2 = two63 - q2 * ad;
    uint64_t delta;
    int p = 63;
    do {
        ++p;
        q1 = 2 * q1;
        r1 = 2 * r1;
        if (r1 >= anc) {
            ++q1;
            r1 -= anc;
        }
        q2 = 2 * q2;
        r2 = 2 * r2;
        if (r2 >= ad) {
            ++q2;
            r2 -= ad;
        }
        delta = ad - r2;
    } while (q1 < delta || (q1 == delta && r1 == 0));
    *mul = (int64_t)(q2 + 1);
    if (d < 0) *mul = (int64_t)(UINT64_C(0) - (uint64_t)*mul);
    *shift = p - 64;
}

/* D-M1-96: n / d or n % d for a constant d other than 0, -1 and INT64_MIN,
 * with the truncation of C. The result is left in %rax. The magic path keeps n
 * in %rcx and multiplies with the one-operand imulq, whose high half lands in
 * %rdx. */
static void emit_division_by_constant(FILE *o, bool remainder, int64_t d, unsigned aoff) {
    uint64_t ad = d < 0 ? UINT64_C(0) - (uint64_t)d : (uint64_t)d;
    if (ad == 1) {
        if (remainder) fputs("\tmovq $0, %rax\n", o);
        else fprintf(o, "\tmovq -%u(%%rbp), %%rax\n", aoff);
        return;
    }
    if ((ad & (ad - 1)) == 0) {
        unsigned k = 0;
        while ((UINT64_C(1) << k) != ad) ++k;
        fprintf(o, "\tmovq -%u(%%rbp), %%rax\n\tmovq %%rax, %%rcx\n\tsarq $63, %%rcx\n"
                   "\tshrq $%u, %%rcx\n\taddq %%rcx, %%rax\n\tsarq $%u, %%rax\n",
                aoff, 64 - k, k);
        if (remainder)
            fprintf(o, "\tshlq $%u, %%rax\n\tnegq %%rax\n\taddq -%u(%%rbp), %%rax\n", k, aoff);
        else if (d < 0) fputs("\tnegq %rax\n", o);
    } else {
        int64_t mul;
        int shift;
        signed_magic(d, &mul, &shift);
        fprintf(o, "\tmovq -%u(%%rbp), %%rcx\n\tmovabsq $%lld, %%rax\n\timulq %%rcx\n",
                aoff, (long long)mul);
        if (d > 0 && mul < 0) fputs("\taddq %rcx, %rdx\n", o);
        if (d < 0 && mul > 0) fputs("\tsubq %rcx, %rdx\n", o);
        if (shift) fprintf(o, "\tsarq $%d, %%rdx\n", shift);
        fputs("\tmovq %rdx, %rax\n\tshrq $63, %rax\n\taddq %rax, %rdx\n", o);
        if (remainder)
            fprintf(o, "\tmovabsq $%lld, %%rax\n\timulq %%rdx, %%rax\n"
                       "\tsubq %%rax, %%rcx\n\tmovq %%rcx, %%rax\n", (long long)d);
        else fputs("\tmovq %rdx, %rax\n", o);
    }
}

static bool emit_function(FILE *o, const R6Module *m, const R6Function *f,
                          size_t fn_index, R6Diagnostic *d) {
    size_t pc, pending = 0;
    const R6Instruction *args[256];
    unsigned long long arg_string_ids[256];
    unsigned long long string_id = ((unsigned long long)fn_index) << 32;
    bool is_main = strcmp(f->name, "main") == 0;
    char symbuf[R6_SYMBOL_CAP];
    const char *native_name = user_symbol(m, f->name, symbuf, sizeof symbuf);
    size_t outgoing = max_outgoing_args(m, f);
    size_t slots = (size_t)f->register_count + hidden_slots(f, is_main) +
                   f->param_count + local_count(f);
    size_t used = slots * 8;
    used += result_area_bytes(f);
    if (uses_literal_scratch(f)) used += 16; /* two immediate slots, D-M1-21 */
    if (uses_spread_scratch(f)) used += 16;  /* two spread slots, D-M1-23 */
    size_t frame = (used + 32 + (outgoing > 4 ? (outgoing - 4) * 8 : 0) + 15) & ~(size_t)15;
    if (f->is_extern) return true;
    /* D-M1-105: a helper of a runtime module is file-local; only the
       exports are visible to the linker. */
    if (m->runtime_module && !f->is_export)
        fprintf(o, "\t.text\n\t.def %s; .scl 3; .type 32; .endef\n", native_name);
    else
        fprintf(o, "\t.text\n\t.globl %s\n\t.def %s; .scl 2; .type 32; .endef\n", native_name, native_name);
    fprintf(o, "\t.seh_proc %s\n%s:\n\tpushq %%rbp\n\t.seh_pushreg %%rbp\n\tmovq %%rsp, %%rbp\n\t.seh_setframe %%rbp, 0\n", native_name, native_name);
    /* D-M1-79: a frame of one page or more is touched page by page, top
       down, before rsp moves; regalloc.c may add up to 14 pushes, so the
       probe covers 128 bytes more. */
    if (frame >= 4096) fprintf(o, "\tmovl $%zu, %%eax\n\tcall ___chkstk_ms\n", frame + 128);
    if (frame) fprintf(o, "\tsubq $%zu, %%rsp\n\t.seh_stackalloc %zu\n", frame, frame);
    fputs("\t.seh_endprologue\n", o);
    /* D-M1-68: where the plain 8-byte slots end and where the literal and
       spread scratch slots lie, for regalloc.c; it removes the line. */
    fprintf(o, "\t# r6.slots %zu %zu %zu\n", slots * 8, slots * 8 + result_area_bytes(f) + 8, used);
    if (!is_main && is_result(f->return_type))
        fprintf(o, "\tmovq %%rcx, -%u(%%rbp)\n", (unsigned)(f->register_count + 1) * 8);
    if (is_main) {
        fprintf(o, "\tmovq %%rcx, -%u(%%rbp)\n", (unsigned)(f->register_count + 1) * 8);
        if (module_needs_arena_global(m))
            fputs("\tleaq r6_arena(%rip), %rax\n\tmovq %rcx, 0(%rax)\n", o);
    }
    for (pc = 0; pc < f->param_count; ++pc) {
        const char *t = f->param_types[pc];
        size_t abi_index = pc + hidden_slots(f, is_main);
        unsigned home = (unsigned)(f->register_count + hidden_slots(f, is_main) + pc + 1) * 8;
        if (abi_index >= 4) {
            unsigned incoming = (unsigned)(48 + (abi_index - 4) * 8);
            if (is_float(t))
                fprintf(o, "\tmovsd %u(%%rbp), %%xmm15\n\tmovsd %%xmm15, -%u(%%rbp)\n", incoming, home);
            else
                fprintf(o, "\tmovq %u(%%rbp), %%rax\n\tmovq %%rax, -%u(%%rbp)\n", incoming, home);
        } else if (is_float(t)) fprintf(o, "\tmovsd %%xmm%zu, -%u(%%rbp)\n", abi_index, home);
        else fprintf(o, "\tmovq %s, -%u(%%rbp)\n", abi_index == 0 ? "%rcx" : abi_index == 1 ? "%rdx" : abi_index == 2 ? "%r8" : "%r9", home);
        if (is_result(t)) {
            /* The home slot holds the caller's pointer; the cell holds the
               copy the C callee owns. */
            unsigned cell = result_cell_offset(f, is_main, param_cell_register(f, pc));
            fprintf(o, "\tmovq -%u(%%rbp), %%r11\n", home);
            unsigned kk;
            for (kk = 0; kk < 24; kk += 8)
                fprintf(o, "\tmovq %u(%%r11), %%rax\n\tmovq %%rax, -%u(%%rbp)\n", kk, cell - kk);
        }
    }
    div_trap_count = 0;
    for (pc = 0; pc < f->instruction_count; ++pc) {
        const R6Instruction *i = &f->instructions[pc];
        const R6Value *v = &i->operands[0];
        const char *t;
        unsigned off = i->has_dest ? (unsigned)(i->dest + 1) * 8 : 0;
        switch (i->opcode) {
        case R6_OP_DECL_VAR: case R6_OP_STORE_VAR: {
            size_t local_index;
            const char *local_type;
            char local_buf[R6_FN_SPELLING_CAP];
            unsigned local_off;
            local_info(m, f, ir_name(&i->operands[0]), &local_index, &local_type,
                       local_buf, sizeof local_buf);
            local_off = (unsigned)(f->register_count + hidden_slots(f, is_main) +
                                   f->param_count + local_index + 1) * 8;
            if (i->operands[1].kind == R6_VAL_SYMBOL)
                /* `val f = inc`: the local word is the function address. */
                fprintf(o, "\tleaq %s(%%rip), %%rax\n\tmovq %%rax, -%u(%%rbp)\n",
                        user_symbol(m, i->operands[1].as.string, symbuf, sizeof symbuf),
                        local_off);
            else if (i->operands[1].kind == R6_VAL_REG && is_result(local_type)) {
                unsigned from = result_cell_offset(f, is_main, i->operands[1].as.i32);
                unsigned to = result_cell_offset(f, is_main,
                                                 (int32_t)(f->register_count + local_index));
                emit_cell_copy(o, from, to);
            } else if (i->operands[1].kind == R6_VAL_REG) {
                if (is_float(local_type))
                    fprintf(o, "\tmovsd -%u(%%rbp), %%xmm0\n\tmovsd %%xmm0, -%u(%%rbp)\n",
                            (unsigned)(i->operands[1].as.i32 + 1) * 8, local_off);
                else
                    fprintf(o, "\tmovq -%u(%%rbp), %%rax\n\tmovq %%rax, -%u(%%rbp)\n",
                            (unsigned)(i->operands[1].as.i32 + 1) * 8, local_off);
            } else if (i->operands[1].kind == R6_VAL_STRING) {
                /* The builder inlines a literal initializer (buildVarDecl):
                   `val s = "x"`, `val n = 7`, `val b = true`. The corpus
                   carries 299 of them across 46 documents (D-M1-19). */
                emit_string(o, i->operands[1].as.string, string_id);
                fprintf(o, "\tleaq .LCr6_%llu(%%rip), %%rax\n\tmovq %%rax, -%u(%%rbp)\n",
                        string_id++, local_off);
            } else if (i->operands[1].kind == R6_VAL_FLOAT64)
                fprintf(o, "\tmovabsq $0x%016llx, %%rax\n\tmovq %%rax, -%u(%%rbp)\n",
                        (unsigned long long)i->operands[1].as.f64_bits, local_off);
            else if (i->operands[1].kind == R6_VAL_INT)
                emit_int_slot(o, i->operands[1].as.i64, local_off);
            else
                fprintf(o, "\tmovl $%d, -%u(%%rbp)\n", i->operands[1].as.boolean ? 1 : 0, local_off);
            break;
        }
        case R6_OP_LOAD_VAR: {
            size_t local_index, param_index;
            const char *local_type, *param_type;
            char local_buf[R6_FN_SPELLING_CAP];
            unsigned local_off;
            const char *name = ir_name(&i->operands[0]);
            const R6Global *global = find_global(m, name);
            if (param_info(f, name, &param_index, &param_type)) {
                /* The parameter's home slot, exactly as its `arg` binding
                   writes and reads it (D-M1-22). */
                unsigned home = (unsigned)(f->register_count + hidden_slots(f, is_main) +
                                           param_index + 1) * 8;
                if (is_result(param_type))
                    emit_cell_copy(o, result_cell_offset(f, is_main, param_cell_register(f, param_index)),
                                   result_cell_offset(f, is_main, i->dest));
                else if (is_float(param_type))
                    fprintf(o, "\tmovsd -%u(%%rbp), %%xmm15\n\tmovsd %%xmm15, -%u(%%rbp)\n",
                            home, off);
                else
                    fprintf(o, "\tmovq -%u(%%rbp), %%rax\n\tmovq %%rax, -%u(%%rbp)\n",
                            home, off);
            } else if (local_info(m, f, name, &local_index, &local_type, local_buf, sizeof local_buf)) {
                local_off = (unsigned)(f->register_count + hidden_slots(f, is_main) +
                                       f->param_count + local_index + 1) * 8;
                if (is_result(local_type))
                    emit_cell_copy(o, result_cell_offset(f, is_main,
                                                         (int32_t)(f->register_count + local_index)),
                                   result_cell_offset(f, is_main, i->dest));
                else if (is_float(local_type))
                    fprintf(o, "\tmovsd -%u(%%rbp), %%xmm0\n\tmovsd %%xmm0, -%u(%%rbp)\n",
                            local_off, off);
                else
                    fprintf(o, "\tmovq -%u(%%rbp), %%rax\n\tmovq %%rax, -%u(%%rbp)\n",
                            local_off, off);
            } else if (global) {
                int64_t value;
                global_integer(global, &value);
                if (is_bool(global->type_name))
                    fprintf(o, "\tmovl $%d, -%u(%%rbp)\n", (int)value, off);
                else
                    emit_int_slot(o, value, off);
            } else {
                int32_t tag_value = 0;
                tag_constant(m, name, &tag_value);
                fprintf(o, "\tmovq $%d, -%u(%%rbp)\n", tag_value, off);
            }
            break;
        }
        case R6_OP_LOAD_CONST: {
            /* Spec 2.1: a Symbol that is a bool or float literal (D-M1-19)
               lowers exactly like the literal, not like a function value. */
            R6Value lit;
            const R6Value *cv = v;
            int literal = sym_literal_kind(v, &lit);
            if (literal < 0)
                return fail(d, i, "CGEN", "float literal %s is not finite", v->as.string);
            if (literal > 0) cv = &lit;
            if (is_null_symbol(cv)) {
                fprintf(o, "\tmovq $0, -%u(%%rbp)\n", off);
            } else if (cv->kind == R6_VAL_STRING) {
                emit_string(o, cv->as.string, string_id);
                fprintf(o, "\tleaq .LCr6_%llu(%%rip), %%rax\n\tmovq %%rax, -%u(%%rbp)\n", string_id++, off);
            } else if (cv->kind == R6_VAL_SYMBOL) {
                /* Function value: the loaded word is the function address. */
                fprintf(o, "\tleaq %s(%%rip), %%rax\n\tmovq %%rax, -%u(%%rbp)\n",
                        user_symbol(m, cv->as.string, symbuf, sizeof symbuf), off);
            } else if (cv->kind == R6_VAL_FLOAT64) {
                fprintf(o, "\tmovabsq $0x%016llx, %%rax\n\tmovq %%rax, -%u(%%rbp)\n", (unsigned long long)cv->as.f64_bits, off);
            } else if (cv->kind == R6_VAL_INT) emit_int_slot(o, cv->as.i64, off);
            else fprintf(o, "\tmovl $%d, -%u(%%rbp)\n", cv->as.boolean ? 1 : 0, off);
            break;
        }
        case R6_OP_LIST_CREATE: {
            unsigned scratch = result_scratch_offset(f, is_main);
            unsigned arena_off = (unsigned)(f->register_count + 1) * 8;
            unsigned success_id = (unsigned)pc * 2;
            unsigned done_id = success_id + 1;
            emit_load_arg(o, f, &i->operands[1], "int", 3, 0);
            fprintf(o, "\tleaq -%u(%%rbp), %%rcx\n", scratch);
            if (is_main) fprintf(o, "\tmovq -%u(%%rbp), %%rdx\n", arena_off);
            else fputs("\tmovq r6_arena(%rip), %rdx\n", o);
            fprintf(o, "\tmovl $8, %%r8d\n\tmovl $%d, 32(%%rsp)\n"
                       "\tcall orbit_list_create_typed\n\tcmpb $1, -%u(%%rbp)\n"
                       "\tje .Lr6_f%zu_list_create_ok_%u\n\tmovq $0, -%u(%%rbp)\n"
                       "\tjmp .Lr6_f%zu_list_create_done_%u\n"
                       ".Lr6_f%zu_list_create_ok_%u:\n\tmovq -%u(%%rbp), %%rax\n"
                       "\tmovq %%rax, -%u(%%rbp)\n.Lr6_f%zu_list_create_done_%u:\n",
                    i->elem_kind, scratch, fn_index, success_id, off,
                    fn_index, done_id, fn_index, success_id, scratch - 16, off,
                    fn_index, done_id);
            break;
        }
        case R6_OP_LIST_PUSH: {
            unsigned scratch = result_scratch_offset(f, is_main);
            unsigned list_off = (unsigned)(i->operands[0].as.i32 + 1) * 8;
            unsigned value_off = (unsigned)(i->operands[1].as.i32 + 1) * 8;
            const char *callee = i->elem_kind == 1 ? "orbit_list_push_string" :
                                 i->elem_kind == 2 ? "orbit_list_push_int" :
                                 i->elem_kind == 3 ? "orbit_list_push_float" :
                                 i->elem_kind == 5 ? "orbit_list_push_ref" :
                                                     "orbit_list_push_bool";
            if (i->elem_kind == 0) {
                /* D-M1-58: orbit_list_push copies the word at the address. */
                fprintf(o, "\tleaq -%u(%%rbp), %%rcx\n\tmovq -%u(%%rbp), %%rdx\n"
                           "\tleaq -%u(%%rbp), %%r8\n\tcall orbit_list_push\n",
                        scratch, list_off, value_off);
                break;
            }
            fprintf(o, "\tleaq -%u(%%rbp), %%rcx\n\tmovq -%u(%%rbp), %%rdx\n",
                    scratch, list_off);
            if (i->elem_kind == 3 && is_int(type_at(f, i->operands[1].as.i32)))
                fprintf(o, "\tcvtsi2sdq -%u(%%rbp), %%xmm2\n", value_off);
            else if (i->elem_kind == 3)
                fprintf(o, "\tmovsd -%u(%%rbp), %%xmm2\n", value_off);
            else if (i->elem_kind == 4)
                fprintf(o, "\tmovl -%u(%%rbp), %%r8d\n", value_off);
            else
                fprintf(o, "\tmovq -%u(%%rbp), %%r8\n", value_off);
            fprintf(o, "\tcall %s\n", callee);
            break;
        }
        case R6_OP_LIST_LEN: {
            unsigned list_off = (unsigned)(i->operands[0].as.i32 + 1) * 8;
            unsigned null_id = (unsigned)pc * 2;
            unsigned done_id = null_id + 1;
            /* D-M1-29: a string receiver is the same opcode in the pin, and
               its arm lowers it to orbit_string_len (c_backend.orb:3615-3621),
               which the header defines inline as `s ? strlen(s) : 0`
               (orbit_api.h:411-413). Emit that body; the archive has no
               orbit_string_len symbol to call. */
            if (is_string(type_at(f, i->operands[0].as.i32))) {
                fprintf(o,
                        "\tmovq -%u(%%rbp), %%rcx\n"
                        "\tmovl $0, %%eax\n"
                        "\ttestq %%rcx, %%rcx\n"
                        "\tje .Lr6_f%zu_list_len_string_null_%u\n"
                        "\tcall strlen\n"
                        ".Lr6_f%zu_list_len_string_null_%u:\n"
                        "\tmovq %%rax, -%u(%%rbp)\n",
                        list_off, fn_index, null_id, fn_index, null_id, off);
                break;
            }
            /* The length is an `int` register: `OrbitList.len` is a size_t
               (orbit_api.h:219) and every consumer of an int slot reads all
               eight bytes (`lt`/`le`/`gt`/`ge` compare with `cmpq`, D-M1-25),
               so the store must be 64-bit. A 32-bit `movl` left the high half
               stale and made `for x, i in xs` over-run its bound. */
            fprintf(o, "\tmovl $0, %%eax\n\tcmpq -%u(%%rbp), %%rax\n"
                       "\tje .Lr6_f%zu_list_len_null_%u\n\tmovq -%u(%%rbp), %%rax\n"
                       "\tmovq 8(%%rax), %%rax\n\tmovq %%rax, -%u(%%rbp)\n"
                       "\tjmp .Lr6_f%zu_list_len_done_%u\n"
                       ".Lr6_f%zu_list_len_null_%u:\n\tmovq $0, -%u(%%rbp)\n"
                       ".Lr6_f%zu_list_len_done_%u:\n",
                    list_off, fn_index, null_id, list_off, off,
                    fn_index, done_id, fn_index, null_id, off, fn_index, done_id);
            break;
        }
        case R6_OP_LIST_GET: {
            unsigned list_off = (unsigned)(i->operands[0].as.i32 + 1) * 8;
            unsigned index_off = (unsigned)(i->operands[1].as.i32 + 1) * 8;
            unsigned long long where_id = string_id++;
            unsigned long long file_id = string_id++;
            emit_string(o, is_main ? "orbit_main" : f->name, where_id);
            emit_string(o, "<orbit-ir>", file_id);
            /* D-M1-38: an opaque read decodes as the type its use gave it. */
            emit_list_read(o, f, fn_index, pc, list_off, index_off, off,
                           i->elem_kind,
                           i->elem_kind == 5 ? 1 :
                           i->elem_kind ? i->elem_kind : is_int(type_at(f, i->dest)) ? 2 : 1,
                           false, where_id, file_id);
            break;
        }
        case R6_OP_LIST_SET: {
            unsigned scratch = result_scratch_offset(f, is_main);
            unsigned list_off = (unsigned)(i->operands[0].as.i32 + 1) * 8;
            unsigned index_off = (unsigned)(i->operands[1].as.i32 + 1) * 8;
            unsigned value_off = (unsigned)(i->operands[2].as.i32 + 1) * 8;
            const char *callee = i->elem_kind == 1 ? "orbit_list_set_string" :
                                 i->elem_kind == 2 ? "orbit_list_set_int" :
                                 i->elem_kind == 3 ? "orbit_list_set_float" :
                                 i->elem_kind == 5 ? "orbit_list_set_ref" :
                                                     "orbit_list_set_bool";
            fprintf(o, "\tleaq -%u(%%rbp), %%rcx\n\tmovq -%u(%%rbp), %%rdx\n"
                       "\tmovq -%u(%%rbp), %%r8\n",
                    scratch, list_off, index_off);
            if (i->elem_kind == 0) {
                fprintf(o, "\tleaq -%u(%%rbp), %%r9\n\tcall orbit_list_set\n", value_off);
                break;
            }
            if (i->elem_kind == 3)
                fprintf(o, "\tmovsd -%u(%%rbp), %%xmm3\n", value_off);
            else if (i->elem_kind == 4)
                fprintf(o, "\tmovl -%u(%%rbp), %%r9d\n", value_off);
            else
                fprintf(o, "\tmovq -%u(%%rbp), %%r9\n", value_off);
            fprintf(o, "\tcall %s\n", callee);
            break;
        }
        case R6_OP_LIST_POP: {
            unsigned list_off = (unsigned)(i->operands[0].as.i32 + 1) * 8;
            unsigned long long where_id = string_id++;
            unsigned long long file_id = string_id++;
            emit_string(o, is_main ? "orbit_main" : f->name, where_id);
            emit_string(o, "<orbit-ir>", file_id);
            emit_list_read(o, f, fn_index, pc, list_off, 0, off,
                           i->elem_kind,
                           i->elem_kind == 5 ? 1 :
                           i->elem_kind ? i->elem_kind : is_int(type_at(f, i->dest)) ? 2 : 1,
                           true, where_id, file_id);
            break;
        }
        case R6_OP_COPY:
            if (is_result(type_at(f, i->dest)))
                emit_cell_copy(o, result_cell_offset(f, is_main, i->operands[0].as.i32),
                               result_cell_offset(f, is_main, i->dest));
            else if (is_float(type_at(f, i->dest)) && i->operands[0].kind == R6_VAL_REG &&
                     is_int(type_at(f, i->operands[0].as.i32)))
                fprintf(o, "\tcvtsi2sdq -%u(%%rbp), %%xmm0\n\tmovsd %%xmm0, -%u(%%rbp)\n",
                        (unsigned)(i->operands[0].as.i32 + 1) * 8, off);
            else if (is_float(type_at(f, i->dest)) && i->operands[0].kind == R6_VAL_REG)
                fprintf(o, "\tmovsd -%u(%%rbp), %%xmm0\n\tmovsd %%xmm0, -%u(%%rbp)\n",
                        (unsigned)(i->operands[0].as.i32 + 1) * 8, off);
            else if (i->operands[0].kind == R6_VAL_REG)
                fprintf(o, "\tmovq -%u(%%rbp), %%rax\n\tmovq %%rax, -%u(%%rbp)\n",
                        (unsigned)(i->operands[0].as.i32 + 1) * 8, off);
            else if (i->operands[0].kind == R6_VAL_BOOL)
                fprintf(o, "\tmovl $%u, -%u(%%rbp)\n", i->operands[0].as.boolean ? 1u : 0u, off);
            else if (i->operands[0].kind == R6_VAL_INT)
                emit_int_slot(o, i->operands[0].as.i64, off);
            else if (i->operands[0].kind == R6_VAL_FLOAT64)
                fprintf(o, "\tmovabsq $0x%016llx, %%rax\n\tmovq %%rax, -%u(%%rbp)\n",
                        (unsigned long long)i->operands[0].as.f64_bits, off);
            else
                return fail(d, i, "CGEN", "copy from this scalar literal kind is unsupported");
            break;
        case R6_OP_ADD: {
            /* D-M1-21: an operand may be an inline literal; it is written to
               its scratch slot first, so the op reads slots either way.
               D-M1-23: with a string on exactly one side, the scalar side is
               spread through the runtime converters; with strings on both
               sides the operation is a concatenation. */
            unsigned aoff = operand_slot(f, is_main, i, 0);
            unsigned boff = operand_slot(f, is_main, i, 1);
            materialize_literals(o, f, i, is_main, &string_id);
            t = type_at(f, i->dest);
            if (is_string(t)) {
                const char *t0 = operand_type_of(f, i, 0);
                const char *t1 = operand_type_of(f, i, 1);
                unsigned s1 = aoff, s2 = boff;
                if (!is_string(t0))
                    s1 = emit_spread(o, f, is_main, t0, aoff,
                                     spread_scratch_offset(f, is_main, 0));
                if (!is_string(t1))
                    s2 = emit_spread(o, f, is_main, t1, boff,
                                     spread_scratch_offset(f, is_main, 1));
                emit_load_arena(o, f, is_main, "%rcx");
                fprintf(o, "\tmovq -%u(%%rbp), %%rdx\n\tmovq -%u(%%rbp), %%r8\n"
                           "\tcall orbit_string_concat\n\tmovq %%rax, -%u(%%rbp)\n",
                        s1, s2, off);
                break;
            }
            if (is_float(t)) {
                fprintf(o, "\tmovsd -%u(%%rbp), %%xmm0\n\taddsd -%u(%%rbp), %%xmm0\n\tmovsd %%xmm0, -%u(%%rbp)\n",
                        aoff, boff, off);
            } else {
                /* D-M1-94: an int and a word are both 64 bits. */
                fprintf(o, "\tmovq -%u(%%rbp), %%rax\n\taddq -%u(%%rbp), %%rax\n"
                           "\tmovq %%rax, -%u(%%rbp)\n", aoff, boff, off);
            }
            break;
        }
        case R6_OP_SUB: case R6_OP_MUL: case R6_OP_DIV: case R6_OP_MOD: {
            unsigned aoff = operand_slot(f, is_main, i, 0);
            unsigned boff = operand_slot(f, is_main, i, 1);
            const char *t0 = operand_type_of(f, i, 0);
            int64_t divisor = 0;
            materialize_literals(o, f, i, is_main, &string_id);
            if (is_float(t0)) {
                const char *ins = i->opcode == R6_OP_SUB ? "subsd" :
                                  i->opcode == R6_OP_MUL ? "mulsd" : "divsd";
                fprintf(o, "\tmovsd -%u(%%rbp), %%xmm0\n\t%s -%u(%%rbp), %%xmm0\n\tmovsd %%xmm0, -%u(%%rbp)\n",
                        aoff, ins, boff, off);
            } else if (i->opcode == R6_OP_SUB || i->opcode == R6_OP_MUL) {
                fprintf(o, "\tmovq -%u(%%rbp), %%rax\n\t%s -%u(%%rbp), %%rax\n"
                           "\tmovq %%rax, -%u(%%rbp)\n",
                        aoff, i->opcode == R6_OP_SUB ? "subq" : "imulq", boff, off);
            } else if ((i->opcode == R6_OP_DIV || i->opcode == R6_OP_MOD) &&
                       constant_divisor(f, pc, &divisor) && divisor != 0 && divisor != -1 &&
                       divisor != INT64_MIN) {
                emit_division_by_constant(o, i->opcode == R6_OP_MOD, divisor, aoff);
                fprintf(o, "\tmovq %%rax, -%u(%%rbp)\n", off);
            } else {
                unsigned long long file_id = string_id++;
                emit_string(o, m->source_name, file_id);
                if (!defer_div_trap(pc, file_id, div_trap_line(i)))
                    return fail(d, i, "CGEN", "out of memory");
                fprintf(o, "\tmovq -%u(%%rbp), %%rcx\n\ttestq %%rcx, %%rcx\n"
                           "\tje .Lr6_f%zu_dz_%zu\n\tcmpq $-1, %%rcx\n"
                           "\tjne .Lr6_f%zu_dok_%zu\n"
                           "\tmovabsq $-9223372036854775808, %%rax\n"
                           "\tcmpq -%u(%%rbp), %%rax\n\tje .Lr6_f%zu_dov_%zu\n"
                           ".Lr6_f%zu_dok_%zu:\n"
                           "\tmovq -%u(%%rbp), %%rax\n\tcqto\n\tidivq %%rcx\n"
                           "\tmovq %s, -%u(%%rbp)\n",
                        boff, fn_index, pc, fn_index, pc, aoff, fn_index, pc,
                        fn_index, pc, aoff,
                        i->opcode == R6_OP_DIV ? "%rax" : "%rdx", off);
            }
            break;
        }
        case R6_OP_EQ: {
            unsigned aoff = operand_slot(f, is_main, i, 0);
            unsigned boff = operand_slot(f, is_main, i, 1);
            const char *t1;
            materialize_literals(o, f, i, is_main, &string_id);
            t = operand_type_of(f, i, 0);
            t1 = operand_type_of(f, i, 1);
            if (is_string(t) && is_string(t1)) {
                emit_string_equality(o, fn_index, pc, aoff, boff);
                emit_compare_result(o, "sete", off, flags_only_compare(f, pc));
            } else if (is_float(t) || is_float(t1)) {
                fprintf(o, "\tmovsd -%u(%%rbp), %%xmm0\n\tucomisd -%u(%%rbp), %%xmm0\n\tsete %%al\n\tsetnp %%dl\n\tandb %%dl, %%al\n\tmovzbl %%al, %%eax\n\tmovl %%eax, -%u(%%rbp)\n",
                        aoff, boff, off);
            } else if (is_bool(t) || is_bool(t1)) {
                /* D-M1-31: a bool slot holds its value in the low byte; the
                   upper half can be stale because the producers store with
                   32-bit writes. The pinned arm compares the C value, so the
                   bool side is normalized to a zero-extended 0/1 before the
                   compare instead of reading the whole slot. */
                if (is_bool(t) && is_bool(t1))
                    fprintf(o, "\tmovzbl -%u(%%rbp), %%eax\n\tmovzbl -%u(%%rbp), %%edx\n\tcmpq %%rdx, %%rax\n",
                            aoff, boff);
                else if (is_bool(t))
                    fprintf(o, "\tmovzbl -%u(%%rbp), %%eax\n\tcmpq -%u(%%rbp), %%rax\n",
                            aoff, boff);
                else
                    fprintf(o, "\tmovzbl -%u(%%rbp), %%eax\n\tmovq -%u(%%rbp), %%rdx\n\tcmpq %%rax, %%rdx\n",
                            boff, aoff);
                emit_compare_result(o, "sete", off, flags_only_compare(f, pc));
            } else {
                fprintf(o, "\tmovq -%u(%%rbp), %%rax\n\tcmpq -%u(%%rbp), %%rax\n", aoff, boff);
                emit_compare_result(o, "sete", off, flags_only_compare(f, pc));
            }
            break;
        }
        case R6_OP_NE: case R6_OP_LT: case R6_OP_LE: case R6_OP_GT: case R6_OP_GE: {
            const char *setcc;
            unsigned aoff = operand_slot(f, is_main, i, 0);
            unsigned boff = operand_slot(f, is_main, i, 1);
            const char *t0 = operand_type_of(f, i, 0);
            const char *t1 = operand_type_of(f, i, 1);
            materialize_literals(o, f, i, is_main, &string_id);
            if (i->opcode == R6_OP_NE && is_string(t0) && is_string(t1)) {
                emit_string_equality(o, fn_index, pc, aoff, boff);
                emit_compare_result(o, "setne", off, flags_only_compare(f, pc));
            } else if (is_float(t0) || is_float(t1)) {
                setcc = i->opcode == R6_OP_NE ? "setne" :
                        i->opcode == R6_OP_LT ? "setb" :
                        i->opcode == R6_OP_LE ? "setbe" :
                        i->opcode == R6_OP_GT ? "seta" : "setae";
                fprintf(o, "\tmovsd -%u(%%rbp), %%xmm0\n\tucomisd -%u(%%rbp), %%xmm0\n\t%s %%al\n",
                        aoff, boff, setcc);
                if (i->opcode == R6_OP_NE)
                    fprintf(o, "\tsetp %%dl\n\torb %%dl, %%al\n");
                else if (i->opcode == R6_OP_LT || i->opcode == R6_OP_LE)
                    fprintf(o, "\tsetnp %%dl\n\tandb %%dl, %%al\n");
                fprintf(o, "\tmovzbl %%al, %%eax\n\tmovl %%eax, -%u(%%rbp)\n", off);
            } else if (is_bool(t0) || is_bool(t1)) {
                /* D-M1-31: the same low-byte normalization as eq. The int
                   side stays in the compare's destination so `setl`/`setg`
                   keep their direction. */
                setcc = i->opcode == R6_OP_NE ? "setne" :
                        i->opcode == R6_OP_LT ? "setl" :
                        i->opcode == R6_OP_LE ? "setle" :
                        i->opcode == R6_OP_GT ? "setg" : "setge";
                if (is_bool(t0) && is_bool(t1))
                    fprintf(o, "\tmovzbl -%u(%%rbp), %%eax\n\tmovzbl -%u(%%rbp), %%edx\n\tcmpq %%rdx, %%rax\n",
                            aoff, boff);
                else if (is_bool(t0))
                    fprintf(o, "\tmovzbl -%u(%%rbp), %%eax\n\tcmpq -%u(%%rbp), %%rax\n",
                            aoff, boff);
                else
                    fprintf(o, "\tmovzbl -%u(%%rbp), %%eax\n\tmovq -%u(%%rbp), %%rdx\n\tcmpq %%rax, %%rdx\n",
                            boff, aoff);
                emit_compare_result(o, setcc, off, flags_only_compare(f, pc));
            } else {
                setcc = i->opcode == R6_OP_NE ? "setne" :
                        i->opcode == R6_OP_LT ? "setl" :
                        i->opcode == R6_OP_LE ? "setle" :
                        i->opcode == R6_OP_GT ? "setg" : "setge";
                fprintf(o, "\tmovq -%u(%%rbp), %%rax\n\tcmpq -%u(%%rbp), %%rax\n", aoff, boff);
                emit_compare_result(o, setcc, off, flags_only_compare(f, pc));
            }
            break;
        }
        case R6_OP_AND_OP: case R6_OP_OR_OP:
            fprintf(o, "\tcmpl $0, -%u(%%rbp)\n\tsetne %%al\n\tcmpl $0, -%u(%%rbp)\n\tsetne %%dl\n\t%s %%dl, %%al\n\tmovzbl %%al, %%eax\n\tmovl %%eax, -%u(%%rbp)\n",
                    (unsigned)(i->operands[0].as.i32 + 1) * 8,
                    (unsigned)(i->operands[1].as.i32 + 1) * 8,
                    i->opcode == R6_OP_AND_OP ? "andb" : "orb", off);
            break;
        case R6_OP_NOT_OP:
            fprintf(o, "\tcmpl $0, -%u(%%rbp)\n\tsete %%al\n\tmovzbl %%al, %%eax\n\tmovl %%eax, -%u(%%rbp)\n",
                    (unsigned)(i->operands[0].as.i32 + 1) * 8, off);
            break;
        case R6_OP_NEG:
            if (is_float(type_at(f, i->operands[0].as.i32)))
                fprintf(o, "\tmovsd -%u(%%rbp), %%xmm0\n\tmovabsq $-9223372036854775808, %%rax\n\tmovq %%rax, %%xmm1\n\txorpd %%xmm1, %%xmm0\n\tmovsd %%xmm0, -%u(%%rbp)\n",
                        (unsigned)(i->operands[0].as.i32 + 1) * 8, off);
            else
                /* D-M1-94: -INT64_MIN wraps to itself. */
                fprintf(o, "\tmovq -%u(%%rbp), %%rax\n\tnegq %%rax\n"
                           "\tmovq %%rax, -%u(%%rbp)\n",
                        (unsigned)(i->operands[0].as.i32 + 1) * 8, off);
            break;
        case R6_OP_BIT_AND: case R6_OP_BIT_OR: case R6_OP_BIT_XOR: {
            /* D-M1-85, D-M1-94: the operation is on the 64-bit words. */
            unsigned aoff = operand_slot(f, is_main, i, 0);
            unsigned boff = operand_slot(f, is_main, i, 1);
            materialize_literals(o, f, i, is_main, &string_id);
            fprintf(o, "\tmovq -%u(%%rbp), %%rax\n\tmovq -%u(%%rbp), %%rcx\n"
                       "\t%s %%rcx, %%rax\n\tmovq %%rax, -%u(%%rbp)\n",
                    aoff, boff, i->opcode == R6_OP_BIT_AND ? "andq" :
                                i->opcode == R6_OP_BIT_OR ? "orq" : "xorq", off);
            break;
        }
        case R6_OP_SHL: case R6_OP_SHR: {
            /* The count is `& 63`, which the hardware applies to a 64-bit
               shift; shr is arithmetic (D-M1-94). */
            unsigned aoff = operand_slot(f, is_main, i, 0);
            unsigned boff = operand_slot(f, is_main, i, 1);
            materialize_literals(o, f, i, is_main, &string_id);
            fprintf(o, "\tmovq -%u(%%rbp), %%rcx\n\tmovq -%u(%%rbp), %%rax\n"
                       "\t%s %%cl, %%rax\n\tmovq %%rax, -%u(%%rbp)\n",
                    boff, aoff, i->opcode == R6_OP_SHL ? "shlq" : "sarq", off);
            break;
        }
        case R6_OP_BIT_NOT: {
            unsigned aoff = operand_slot(f, is_main, i, 0);
            materialize_literals(o, f, i, is_main, &string_id);
            fprintf(o, "\tmovq -%u(%%rbp), %%rax\n\tnotq %%rax\n"
                       "\tmovq %%rax, -%u(%%rbp)\n", aoff, off);
            break;
        }
        /* D-M1-106. str_len keeps the null test of orbit_string_len (a NULL
           string has length 0); the byte accesses do not test anything. */
        case R6_OP_STR_LEN:
            fprintf(o, "\tmovq -%u(%%rbp), %%rcx\n\tmovl $0, %%eax\n"
                       "\ttestq %%rcx, %%rcx\n\tje .Lr6_f%zu_str_len_%zu\n"
                       "\tcall strlen\n.Lr6_f%zu_str_len_%zu:\n"
                       "\tmovq %%rax, -%u(%%rbp)\n",
                    operand_slot(f, is_main, i, 0), fn_index, pc, fn_index, pc, off);
            break;
        /* D-M1-110. No allocation and no check: the pointer moves by i bytes.
           bytes_copy passes memcpy a positive count only. */
        case R6_OP_STR_OFFSET:
            fprintf(o, "\tmovq -%u(%%rbp), %%rax\n\tmovq -%u(%%rbp), %%rcx\n"
                       "\taddq %%rcx, %%rax\n\tmovq %%rax, -%u(%%rbp)\n",
                    operand_slot(f, is_main, i, 0), operand_slot(f, is_main, i, 1), off);
            break;
        case R6_OP_BYTES_COPY:
            fprintf(o, "\tmovq -%u(%%rbp), %%rax\n\ttestq %%rax, %%rax\n"
                       "\tjle .Lr6_f%zu_bytes_copy_%zu\n"
                       "\tmovq -%u(%%rbp), %%rcx\n\tmovq -%u(%%rbp), %%rdx\n"
                       "\tmovq -%u(%%rbp), %%r8\n\tcall memcpy\n"
                       ".Lr6_f%zu_bytes_copy_%zu:\n",
                    operand_slot(f, is_main, i, 2), fn_index, pc,
                    operand_slot(f, is_main, i, 0), operand_slot(f, is_main, i, 1),
                    operand_slot(f, is_main, i, 2), fn_index, pc);
            break;
        case R6_OP_BYTE_LOAD:
            fprintf(o, "\tmovq -%u(%%rbp), %%rax\n\tmovq -%u(%%rbp), %%rcx\n"
                       "\taddq %%rcx, %%rax\n\tmovzbl (%%rax), %%eax\n"
                       "\tmovq %%rax, -%u(%%rbp)\n",
                    operand_slot(f, is_main, i, 0), operand_slot(f, is_main, i, 1), off);
            break;
        case R6_OP_BYTE_STORE:
            fprintf(o, "\tmovq -%u(%%rbp), %%rax\n\tmovq -%u(%%rbp), %%rcx\n"
                       "\taddq %%rcx, %%rax\n\tmovq -%u(%%rbp), %%rdx\n"
                       "\tmovb %%dl, (%%rax)\n",
                    operand_slot(f, is_main, i, 0), operand_slot(f, is_main, i, 1),
                    operand_slot(f, is_main, i, 2));
            break;
        case R6_OP_BYTES_ALLOC:
            /* The global arena, not r6_arena: the object may be linked into a
               program the C backend built. The terminating byte is written
               before the destination store, so a destination that shares
               the length slot cannot corrupt the address. */
            /* D-M1-108: an RtArena operand is the arena of the caller; NULL
               falls back to the global arena. */
            if (i->operands[1].kind == R6_VAL_REG)
                fprintf(o, "\tmovq -%u(%%rbp), %%rcx\n\ttestq %%rcx, %%rcx\n"
                           "\tjne .Lr6_f%zu_bytes_arena_%zu\n\tcall orbit_arena_get_global\n"
                           "\tmovq %%rax, %%rcx\n.Lr6_f%zu_bytes_arena_%zu:\n"
                           "\tmovq -%u(%%rbp), %%rdx\n\tleaq 1(%%rdx), %%rdx\n"
                           "\tcall orbit_alloc\n\tmovq -%u(%%rbp), %%rcx\n"
                           "\taddq %%rax, %%rcx\n\tmovb $0, (%%rcx)\n"
                           "\tmovq %%rax, -%u(%%rbp)\n",
                        operand_slot(f, is_main, i, 1), fn_index, pc, fn_index, pc,
                        operand_slot(f, is_main, i, 0), operand_slot(f, is_main, i, 0), off);
            else
            fprintf(o, "\tcall orbit_arena_get_global\n\tmovq %%rax, %%rcx\n"
                       "\tmovq -%u(%%rbp), %%rdx\n\tleaq 1(%%rdx), %%rdx\n"
                       "\tcall orbit_alloc\n\tmovq -%u(%%rbp), %%rcx\n"
                       "\taddq %%rax, %%rcx\n\tmovb $0, (%%rcx)\n"
                       "\tmovq %%rax, -%u(%%rbp)\n",
                    operand_slot(f, is_main, i, 0), operand_slot(f, is_main, i, 0), off);
            break;
        case R6_OP_JUMP_IF_FALSE: {
            const char *fused = fused_false_jump(f, pc);
            if (fused)
                fprintf(o, "\t%s .Lr6_f%zu_l%d\n", fused, fn_index, i->operands[1].as.i32);
            else
                fprintf(o, "\tcmpl $0, -%u(%%rbp)\n\tje .Lr6_f%zu_l%d\n",
                        (unsigned)(v->as.i32 + 1) * 8, fn_index,
                        i->operands[1].as.i32);
            break;
        }
        case R6_OP_JUMP:
            fprintf(o, "\tjmp .Lr6_f%zu_l%d\n", fn_index, i->operands[0].as.i32);
            break;
        case R6_OP_LABEL:
            fprintf(o, ".Lr6_f%zu_l%d:\n", fn_index, v->as.i32);
            break;
        case R6_OP_ARG:
            if (i->has_dest) {
                int idx = v->as.i32;
                unsigned home = (unsigned)(f->register_count + hidden_slots(f, is_main) + idx + 1) * 8;
                if (is_result(f->param_types[idx]))
                    emit_cell_copy(o, result_cell_offset(f, is_main, param_cell_register(f, (size_t)idx)),
                                   result_cell_offset(f, is_main, i->dest));
                else if (is_float(f->param_types[idx]))
                    fprintf(o, "\tmovsd -%u(%%rbp), %%xmm15\n\tmovsd %%xmm15, -%u(%%rbp)\n", home, off);
                else fprintf(o, "\tmovq -%u(%%rbp), %%rax\n\tmovq %%rax, -%u(%%rbp)\n", home, off);
            } else { args[pending] = i; arg_string_ids[pending++] = ULLONG_MAX; }
            break;
        case R6_OP_LOAD_FIELD: {
            const R6Model *model = NULL;
            const R6Field *field;
            const R6Type *pack = NULL;
            size_t index = 0, offset = 0, total = 0, pack_variant = 0, pack_index = 0;
            bool is_pack = false;
            char part[64];
            if (string_length_field(f, i)) {
                unsigned null_id = (unsigned)pc * 2;
                fprintf(o, "\tmovq -%u(%%rbp), %%rcx\n\tmovl $0, %%eax\n"
                           "\ttestq %%rcx, %%rcx\n\tje .Lr6_f%zu_field_len_null_%u\n"
                           "\tcall strlen\n.Lr6_f%zu_field_len_null_%u:\n"
                           "\tmovq %%rax, -%u(%%rbp)\n",
                        (unsigned)(i->operands[0].as.i32 + 1) * 8, fn_index, null_id,
                        fn_index, null_id, off);
                break;
            }
            if (!resolve_pack_field(m, f, i, &is_pack, &pack, &pack_variant, &pack_index,
                                    &offset, &total, d))
                return false;
            if (!is_pack && i->operands[0].kind == R6_VAL_REG &&
                is_void_ptr(type_at(f, i->operands[0].as.i32))) {
                unsigned long long key_id = string_id++;
                const char *dt = type_at(f, i->dest);
                emit_string(o, i->operands[1].as.string, key_id);
                fprintf(o, "\tmovq -%u(%%rbp), %%rcx\n\tleaq .LCr6_%llu(%%rip), %%rdx\n\tcall %s\n",
                        (unsigned)(i->operands[0].as.i32 + 1) * 8, key_id, object_getter(dt));
                if (is_float(dt))
                    fprintf(o, "\tmovsd %%xmm0, -%u(%%rbp)\n", off);
                else if (is_bool(dt))
                    fprintf(o, "\tmovzbl %%al, %%eax\n\tmovq %%rax, -%u(%%rbp)\n", off);
                else
                    fprintf(o, "\tmovq %%rax, -%u(%%rbp)\n", off);
                break;
            }
            if (is_pack) {
                if (!union_payload_part(pack->rich_variants[pack_variant], pack_index, part, sizeof part))
                    return fail(d, i, "CGEN", "union %s variant %s payloads are outside the M1 native subset",
                                pack->name, pack->variants[pack_variant]);
                fprintf(o, "\tmovq -%u(%%rbp), %%r11\n", (unsigned)(i->operands[0].as.i32 + 1) * 8);
                if (is_bool(part))
                    fprintf(o, "\tmovzbl %zu(%%r11), %%eax\n\tmovq %%rax, -%u(%%rbp)\n", offset, off);
                else
                    fprintf(o, "\tmovq %zu(%%r11), %%rax\n\tmovq %%rax, -%u(%%rbp)\n", offset, off);
                break;
            }
            if (!resolve_field(m, f, i, &model, &index, d)) return false;
            field = &model->fields[index];
            if (!model_field_layout(m, model, index, &offset, &total))
                return fail(d, i, "CGEN", "model %s field %s has unsupported type %s",
                            model->name, field->name, field->type_name);
            fprintf(o, "\tmovq -%u(%%rbp), %%r11\n", (unsigned)(i->operands[0].as.i32 + 1) * 8);
            if (is_float(field->type_name))
                fprintf(o, "\tmovsd %zu(%%r11), %%xmm0\n\tmovsd %%xmm0, -%u(%%rbp)\n", offset, off);
            else if (is_bool(field->type_name))
                fprintf(o, "\tmovzbl %zu(%%r11), %%eax\n\tmovq %%rax, -%u(%%rbp)\n", offset, off);
            else
                fprintf(o, "\tmovq %zu(%%r11), %%rax\n\tmovq %%rax, -%u(%%rbp)\n", offset, off);
            break;
        }
        case R6_OP_STORE_FIELD: {
            const R6Model *model = NULL;
            const R6Field *field;
            const R6Value *val = &i->operands[2];
            size_t index = 0, offset = 0, total = 0;
            if (!resolve_field(m, f, i, &model, &index, d)) return false;
            field = &model->fields[index];
            if (!model_field_layout(m, model, index, &offset, &total))
                return fail(d, i, "CGEN", "model %s field %s has unsupported type %s",
                            model->name, field->name, field->type_name);
            fprintf(o, "\tmovq -%u(%%rbp), %%r11\n", (unsigned)(i->operands[0].as.i32 + 1) * 8);
            if (is_float(field->type_name)) {
                if (val->kind == R6_VAL_REG)
                    fprintf(o, "\tmovsd -%u(%%rbp), %%xmm0\n", (unsigned)(val->as.i32 + 1) * 8);
                else
                    fprintf(o, "\tmovabsq $0x%016llx, %%rax\n\tmovq %%rax, %%xmm0\n",
                            (unsigned long long)val->as.f64_bits);
                fprintf(o, "\tmovsd %%xmm0, %zu(%%r11)\n", offset);
            } else if (is_string(field->type_name)) {
                if (val->kind == R6_VAL_REG)
                    fprintf(o, "\tmovq -%u(%%rbp), %%rax\n", (unsigned)(val->as.i32 + 1) * 8);
                else {
                    emit_string(o, val->as.string, string_id);
                    fprintf(o, "\tleaq .LCr6_%llu(%%rip), %%rax\n", string_id++);
                }
                fprintf(o, "\tmovq %%rax, %zu(%%r11)\n", offset);
            } else if (field_is_word(m, field->type_name)) {
                fprintf(o, "\tmovq -%u(%%rbp), %%rax\n\tmovq %%rax, %zu(%%r11)\n",
                        (unsigned)(val->as.i32 + 1) * 8, offset);
            } else if (is_bool(field->type_name)) {
                if (val->kind == R6_VAL_REG)
                    fprintf(o, "\tcmpl $0, -%u(%%rbp)\n\tsetne %%al\n\tmovb %%al, %zu(%%r11)\n",
                            (unsigned)(val->as.i32 + 1) * 8, offset);
                else
                    fprintf(o, "\tmovb $%d, %zu(%%r11)\n", val->as.boolean ? 1 : 0, offset);
            } else {
                if (val->kind == R6_VAL_REG)
                    fprintf(o, "\tmovq -%u(%%rbp), %%rax\n", (unsigned)(val->as.i32 + 1) * 8);
                else
                    emit_int_rax(o, val->as.i64);
                fprintf(o, "\tmovq %%rax, %zu(%%r11)\n", offset);
            }
            break;
        }
        case R6_OP_UNION_GET_TAG:
            /* The pin reads the cell as { int tag @0; void* data @8; } off
               orbit_alloc, with the tag stored as the variant ordinal. */
            fprintf(o, "\tmovq -%u(%%rbp), %%r11\n\tmovslq 0(%%r11), %%rax\n\tmovq %%rax, -%u(%%rbp)\n",
                    (unsigned)(i->operands[0].as.i32 + 1) * 8, off);
            break;
        case R6_OP_UNION_GET_DATA:
            fprintf(o, "\tmovq -%u(%%rbp), %%r11\n\tmovq 8(%%r11), %%rax\n\tmovq %%rax, -%u(%%rbp)\n",
                    (unsigned)(i->operands[0].as.i32 + 1) * 8, off);
            break;
        case R6_OP_CALL: {
            const R6Model *model = i->operands[0].kind == R6_VAL_STRING
                                       ? find_model(m, i->operands[0].as.string) : NULL;
            size_t k, arity = (size_t)i->operands[1].as.i64, base = pending - arity;
            for (k = 0; k < arity; ++k) {
                const R6Value *av = &args[base + k]->operands[0];
                if (av->kind == R6_VAL_STRING) {
                    arg_string_ids[base + k] = string_id;
                    emit_string(o, av->as.string, string_id++);
                }
            }
            if (model) {
                /* Inline orbit_model_<M>_create: allocate sizeof(M) in the
                   same arena the pin keeps in its OrbitArena global, then
                   store each field with its C width (orbit_int truncates to
                   32 bits, orbit_bool stores a 0/1 byte, orbit_float a
                   double, orbit_string a pointer). */
                size_t offset = 0, total = 0;
                if (is_main)
                    fprintf(o, "\tmovq -%u(%%rbp), %%rcx\n", (unsigned)(f->register_count + 1) * 8);
                else
                    fputs("\tmovq r6_arena(%rip), %rcx\n", o);
                if (!model_field_layout(m, model, 0, &offset, &total))
                    return fail(d, i, "CGEN", "model %s has an unsupported field type", model->name);
                fprintf(o, "\tmovl $%zu, %%edx\n\tcall orbit_alloc\n\tmovq %%rax, %%r10\n", total);
                for (k = 0; k < arity; ++k) {
                    const R6Value *av = &args[base + k]->operands[0];
                    const char *ft = model->fields[k].type_name;
                    if (!model_field_layout(m, model, k, &offset, &total))
                        return fail(d, i, "CGEN", "model %s has an unsupported field type", model->name);
                    if (is_float(ft)) {
                        if (av->kind == R6_VAL_REG)
                            fprintf(o, "\tmovsd -%u(%%rbp), %%xmm0\n", (unsigned)(av->as.i32 + 1) * 8);
                        else
                            fprintf(o, "\tmovabsq $0x%016llx, %%rax\n\tmovq %%rax, %%xmm0\n",
                                    (unsigned long long)av->as.f64_bits);
                        fprintf(o, "\tmovsd %%xmm0, %zu(%%r10)\n", offset);
                    } else if (is_string(ft)) {
                        if (av->kind == R6_VAL_REG)
                            fprintf(o, "\tmovq -%u(%%rbp), %%rax\n", (unsigned)(av->as.i32 + 1) * 8);
                        else
                            fprintf(o, "\tleaq .LCr6_%llu(%%rip), %%rax\n", arg_string_ids[base + k]);
                        fprintf(o, "\tmovq %%rax, %zu(%%r10)\n", offset);
                    } else if (field_is_word(m, ft)) {
                        fprintf(o, "\tmovq -%u(%%rbp), %%rax\n\tmovq %%rax, %zu(%%r10)\n",
                                (unsigned)(av->as.i32 + 1) * 8, offset);
                    } else if (is_bool(ft)) {
                        if (av->kind == R6_VAL_REG)
                            fprintf(o, "\tcmpl $0, -%u(%%rbp)\n\tsetne %%al\n\tmovb %%al, %zu(%%r10)\n",
                                    (unsigned)(av->as.i32 + 1) * 8, offset);
                        else
                            fprintf(o, "\tmovb $%d, %zu(%%r10)\n", av->as.boolean ? 1 : 0, offset);
                    } else {
                        if (av->kind == R6_VAL_REG)
                            fprintf(o, "\tmovq -%u(%%rbp), %%rax\n", (unsigned)(av->as.i32 + 1) * 8);
                        else
                            emit_int_rax(o, av->as.i64);
                        fprintf(o, "\tmovq %%rax, %zu(%%r10)\n", offset);
                    }
                }
                fprintf(o, "\tmovq %%r10, -%u(%%rbp)\n", off);
                pending = base;
                break;
            }
            if (i->operands[0].kind == R6_VAL_STRING &&
                strcmp(i->operands[0].as.string, "print") == 0 &&
                !find_union_variant(m, i->operands[0].as.string, NULL)) {
                /* D-M1-26: the builtin `print` (c_backend.orb:2977-3005).
                 * Exactly one argument prints one line: a string via the C
                 * "%s" form, a float via the runtime's own formatter, and any
                 * other scalar (int/bool) via "%lld". 0 or >1 arguments emit
                 * no code at all, exactly like the pinned no-op comment
                 * (c_backend.orb:3003). The void destination is never stored.
                 * The calls reach the msvcrt printf with the variable-argument
                 * ABI: every argument rides the integer registers (RCX/%rdx)
                 * and AL is set to 0 (no vector registers) for the varargs
                 * call. */
                if (arity == 1) {
                    const R6Value *av = &args[base]->operands[0];
                    const char *at = NULL;
                    unsigned long long fmt;
                    value_type(f, av, &at);
                    if (is_string(at)) {
                        /* D-M1-102: the pin's `orbit_print_string(s)` (string,
                         * newline, flush) in place of printf("%s\n", s). */
                        emit_load_arg(o, f, av, "string", 0, arg_string_ids[base]);
                        fputs("\tcall orbit_print_string\n", o);
                        pending = base;
                        break;
                    } else if (is_float(at)) {
                        /* printf("%s\n", orbit_float_to_string(arena, v)) */
                        emit_load_arena(o, f, is_main, "%rcx");
                        emit_load_arg(o, f, av, "float", 1, 0);
                        fputs("\tcall orbit_float_to_string\n\tmovq %rax, %rdx\n", o);
                        fmt = string_id++;
                        emit_string(o, "%s\n", fmt);
                    } else {
                        /* printf("%lld\n", (long long)v) */
                        fmt = string_id++;
                        emit_string(o, "%lld\n", fmt);
                        emit_load_arg(o, f, av, at, 1, 0);
                    }
                    fprintf(o, "\tleaq .LCr6_%llu(%%rip), %%rcx\n\tmovl $0, %%eax\n\tcall printf\n",
                            fmt);
                }
                /* The pinned lowering pops one argument for the printing form
                   and every pending argument for the no-op (c_backend.orb:
                   2999-3004); validation books the same counts. */
                pending = arity == 1 ? base : 0;
                break;
            }
            if (i->operands[0].kind == R6_VAL_SYMBOL || i->operands[0].kind == R6_VAL_STRING) {
                /* Direct named call (spec 3.1 step 2): the target String names
                   a declared function. Model construction is handled above and
                   union-variant construction below; validate() has already
                   established which of the three the name is. */
                const R6Function *named = find_function(m, i->operands[0].as.string);
                if (!named && i->operands[0].kind == R6_VAL_STRING)
                    named = find_intrinsic(i->operands[0].as.string);
                if (named) {
                    /* D-M1-29: orbit_string_len is ORBIT_INLINE in the runtime
                       header (orbit_api.h:411-413), so the archive never had a
                       symbol for the C backend to call. Lower it here as
                       `s ? (orbit_int)strlen(s) : 0`, which is the header's
                       own body. */
                    if (strcmp(named->name, "orbit_string_len") == 0) {
                        unsigned null_id = (unsigned)pc * 2 + 1;
                        const R6Value *av = &args[base]->operands[0];
                        emit_load_arg(o, f, av, "string", 0,
                                      arg_string_ids[base] == ULLONG_MAX ? 0 : arg_string_ids[base]);
                        fprintf(o,
                                "\tmovl $0, %%eax\n"
                                "\ttestq %%rcx, %%rcx\n"
                                "\tje .Lr6_f%zu_strlen_null_%u\n"
                                "\tcall strlen\n"
                                ".Lr6_f%zu_strlen_null_%u:\n",
                                fn_index, null_id, fn_index, null_id);
                        if (i->has_dest && !is_void(type_at(f, i->dest)))
                            fprintf(o, "\tmovq %%rax, -%u(%%rbp)\n", off);
                        pending = base;
                        break;
                    }
                    /* D-M1-27: an enumerated runtime extern takes the arena as
                       its first C parameter although the IR declaration does
                       not repeat it (c_backend.orb:3097-3104 prepends it
                       when the callee takes one; the extern family is
                       `functionNeedsArena` at :4416-4437). Load it into %rcx
                       and shift every IR argument one slot; the float
                       arguments keep the xmm sequence because the arena is an
                       integer argument. */
                    bool arena_first = named->is_extern &&
                                       extern_needs_arena(named->name);
                    /* D-M1-43: an OrbitResult return is 24 bytes, so Win64
                       passes the caller's cell as a hidden first argument. */
                    bool sret_first = is_result(named->return_type);
                    /* D-M1-48: a runtime extern that returns a result takes
                       the sret pointer first and the arena second
                       (`OrbitResult f(OrbitArena*, ...)`). */
                    size_t hidden = (arena_first ? 1u : 0u) + (sret_first ? 1u : 0u);
                    if (arena_first) emit_load_arena(o, f, is_main, sret_first ? "%rdx" : "%rcx");
                    if (sret_first)
                        fprintf(o, "\tleaq -%u(%%rbp), %%rcx\n",
                                result_cell_offset(f, is_main, i->dest));
                    for (k = 0; k < arity; ++k) {
                        const R6Value *av = &args[base + k]->operands[0];
                        const char *at;
                        value_type(f, av, &at);
                        if (is_result(at)) {
                            unsigned cell = result_cell_offset(f, is_main, av->as.i32);
                            size_t ai = k + hidden;
                            if (ai >= 4)
                                fprintf(o, "\tleaq -%u(%%rbp), %%rax\n\tmovq %%rax, %u(%%rsp)\n",
                                        cell, (unsigned)(32 + (ai - 4) * 8));
                            else
                                fprintf(o, "\tleaq -%u(%%rbp), %s\n", cell,
                                        ai == 0 ? "%rcx" : ai == 1 ? "%rdx" : ai == 2 ? "%r8" : "%r9");
                            continue;
                        }
                        emit_load_arg_as(o, f, av, at, named->param_types[k], k + hidden,
                                         arg_string_ids[base + k] == ULLONG_MAX ? 0 : arg_string_ids[base + k]);
                    }
                    fprintf(o, "\tcall %s\n",
                            find_function(m, named->name)
                                ? user_symbol(m, named->name, symbuf, sizeof symbuf)
                                : intrinsic_symbol(named->name));
                    if (i->has_dest && !is_void(type_at(f, i->dest)) && !sret_first) {
                        if (is_float(type_at(f, i->dest))) fprintf(o, "\tmovsd %%xmm0, -%u(%%rbp)\n", off);
                        else if (is_bool(type_at(f, i->dest)))
                            /* D-M1-42: a C bool returns in al only; the
                               rest of rax is whatever the callee left. */
                            fprintf(o, "\tmovzbl %%al, %%eax\n\tmovq %%rax, -%u(%%rbp)\n", off);
                        else fprintf(o, "\tmovq %%rax, -%u(%%rbp)\n", off);
                    }
                    pending = base;
                    break;
                }
            }
            if (i->operands[0].kind == R6_VAL_STRING) {
                /* Variant construction: a 16-byte cell { int tag; void* data }
                   off orbit_alloc. The data word is the raw payload value
                   (exactly (void*)(uintptr_t)(arg)) or a pointer to the pack
                   struct of a multi-payload variant. */
                size_t variant_index = 0;
                const R6Type *variant_type =
                    find_union_variant(m, i->operands[0].as.string, &variant_index);
                const char *rich;
                size_t parts;
                if (!variant_type)
                    return fail(d, i, "CGEN", "call target %s is not a declared function, model, or union variant",
                                i->operands[0].as.string);
                rich = variant_type->rich_variants[variant_index];
                parts = union_payload_count(rich);
                if (parts >= 2) {
                    size_t offset = 0, total = 0;
                    if (!union_pack_layout(m, variant_type, variant_index, 0, &offset, &total))
                        return fail(d, i, "CGEN", "union %s variant %s payloads are outside the M1 native subset",
                                    variant_type->name, variant_type->variants[variant_index]);
                    if (is_main)
                        fprintf(o, "\tmovq -%u(%%rbp), %%rcx\n", (unsigned)(f->register_count + 1) * 8);
                    else
                        fputs("\tmovq r6_arena(%rip), %rcx\n", o);
                    fprintf(o, "\tmovl $%zu, %%edx\n\tcall orbit_alloc\n\tmovq %%rax, -%u(%%rbp)\n\tmovq %%rax, %%r10\n",
                            total, off);
                    for (k = 0; k < parts; ++k) {
                        const R6Value *av = &args[base + k]->operands[0];
                        char part[64];
                        if (!union_pack_layout(m, variant_type, variant_index, k, &offset, &total) ||
                            !union_payload_part(rich, k, part, sizeof part))
                            return fail(d, i, "CGEN", "union %s variant %s payloads are outside the M1 native subset",
                                        variant_type->name, variant_type->variants[variant_index]);
                        if (is_string(part) || payload_is_word(m, part)) {
                            if (av->kind == R6_VAL_REG)
                                fprintf(o, "\tmovq -%u(%%rbp), %%rax\n", (unsigned)(av->as.i32 + 1) * 8);
                            else
                                fprintf(o, "\tleaq .LCr6_%llu(%%rip), %%rax\n", arg_string_ids[base + k]);
                            fprintf(o, "\tmovq %%rax, %zu(%%r10)\n", offset);
                        } else if (is_bool(part)) {
                            if (av->kind == R6_VAL_REG)
                                fprintf(o, "\tcmpl $0, -%u(%%rbp)\n\tsetne %%al\n\tmovb %%al, %zu(%%r10)\n",
                                        (unsigned)(av->as.i32 + 1) * 8, offset);
                            else
                                fprintf(o, "\tmovb $%d, %zu(%%r10)\n", av->as.boolean ? 1 : 0, offset);
                        } else {
                            if (av->kind == R6_VAL_REG)
                                fprintf(o, "\tmovq -%u(%%rbp), %%rax\n", (unsigned)(av->as.i32 + 1) * 8);
                            else
                                emit_int_rax(o, av->as.i64);
                            fprintf(o, "\tmovq %%rax, %zu(%%r10)\n", offset);
                        }
                    }
                }
                if (is_main)
                    fprintf(o, "\tmovq -%u(%%rbp), %%rcx\n", (unsigned)(f->register_count + 1) * 8);
                else
                    fputs("\tmovq r6_arena(%rip), %rcx\n", o);
                fprintf(o, "\tmovl $16, %%edx\n\tcall orbit_alloc\n\tmovl $%zu, 0(%%rax)\n", variant_index);
                if (parts == 1) {
                    const R6Value *av = &args[base]->operands[0];
                    char part[64];
                    if (!union_payload_part(rich, 0, part, sizeof part))
                        return fail(d, i, "CGEN", "union %s variant %s payloads are outside the M1 native subset",
                                    variant_type->name, variant_type->variants[variant_index]);
                    if (av->kind == R6_VAL_REG)
                        fprintf(o, "\tmovq -%u(%%rbp), %%rdx\n", (unsigned)(av->as.i32 + 1) * 8);
                    else if (is_string(part))
                        fprintf(o, "\tleaq .LCr6_%llu(%%rip), %%rdx\n", arg_string_ids[base]);
                    else if (av->kind == R6_VAL_INT && fits_imm32(av->as.i64))
                        fprintf(o, "\tmovq $%lld, %%rdx\n", (long long)av->as.i64);
                    else if (av->kind == R6_VAL_INT) {
                        /* %rax holds the cell: park it while the wide value
                           goes through movabsq. */
                        fputs("\tmovq %rax, %r10\n", o);
                        emit_int_rax(o, av->as.i64);
                        fputs("\tmovq %rax, %rdx\n\tmovq %r10, %rax\n", o);
                    }
                    else
                        fprintf(o, "\tmovl $%d, %%edx\n", av->as.boolean ? 1 : 0);
                } else if (parts == 0) {
                    fputs("\tmovl $0, %edx\n", o);
                } else {
                    fprintf(o, "\tmovq -%u(%%rbp), %%rdx\n", off);
                }
                fprintf(o, "\tmovq %%rdx, 8(%%rax)\n\tmovq %%rax, -%u(%%rbp)\n", off);
                pending = base;
                break;
            }
            if (i->operands[0].kind == R6_VAL_REG) {
                /* Indirect call through a function value (buildFnVarCall):
                   the callee word is loaded after the argument registers so
                   the load cannot clobber them; %r10 is volatile scratch. */
                unsigned callee_off = (unsigned)(i->operands[0].as.i32 + 1) * 8;
                for (k = 0; k < arity; ++k) {
                    const R6Value *av = &args[base + k]->operands[0];
                    const char *at;
                    value_type(f, av, &at);
                    emit_load_arg(o, f, av, at, k,
                                  arg_string_ids[base + k] == ULLONG_MAX ? 0 : arg_string_ids[base + k]);
                }
                fprintf(o, "\tmovq -%u(%%rbp), %%r10\n\tcallq *%%r10\n", callee_off);
                if (i->has_dest) {
                    if (is_float(type_at(f, i->dest))) fprintf(o, "\tmovsd %%xmm0, -%u(%%rbp)\n", off);
                    else fprintf(o, "\tmovq %%rax, -%u(%%rbp)\n", off);
                }
                pending = base;
                break;
            }
            return fail(d, i, "CGEN", "call requires a direct native symbol or a declared model constructor");
        }
        case R6_OP_RET:
            /* D-M1-28: a void function returns without reading the operand,
               exactly like the pinned lowering. */
            if (is_result(f->return_type) && v->kind == R6_VAL_REG) {
                unsigned cell = result_cell_offset(f, is_main, v->as.i32);
                fprintf(o, "\tmovq -%u(%%rbp), %%rax\n"
                           "\tmovq -%u(%%rbp), %%r10\n\tmovq %%r10, 0(%%rax)\n"
                           "\tmovq -%u(%%rbp), %%r10\n\tmovq %%r10, 8(%%rax)\n"
                           "\tmovq -%u(%%rbp), %%r10\n\tmovq %%r10, 16(%%rax)\n",
                        (unsigned)(f->register_count + 1) * 8, cell, cell - 8, cell - 16);
            } else if (strcmp(f->return_type, "void") != 0 && v->kind != R6_VAL_NONE) {
                value_type(f, v, &t);
                if (v->kind == R6_VAL_REG && is_float(t)) fprintf(o, "\tmovsd -%u(%%rbp), %%xmm0\n", (unsigned)(v->as.i32 + 1) * 8);
                else if (v->kind == R6_VAL_REG) fprintf(o, "\tmovq -%u(%%rbp), %%rax\n", (unsigned)(v->as.i32 + 1) * 8);
                else if (v->kind == R6_VAL_INT) emit_int_rax(o, v->as.i64);
                else if (v->kind == R6_VAL_BOOL) fprintf(o, "\tmovl $%d, %%eax\n", v->as.boolean ? 1 : 0);
                else if (v->kind == R6_VAL_FLOAT64) fprintf(o, "\tmovabsq $0x%016llx, %%rax\n\tmovq %%rax, %%xmm0\n", (unsigned long long)v->as.f64_bits);
                else return fail(d, i, "CGEN", "returning string literals is unsupported");
            } else if (strcmp(f->return_type, "void") != 0 || is_main) {
                if (is_float(f->return_type)) fputs("\txorpd %xmm0, %xmm0\n", o);
                else fputs("\tmovl $0, %eax\n", o);
            }
            fputs("\tleave\n\tret\n", o);
            break;
        case R6_OP_RESULT_OK: {
            unsigned cell = result_cell_offset(f, is_main, i->dest);
            value_type(f, v, &t);
            if (is_float(t)) {
                /* MS x64 argument registers are positional: the hidden sret
                   pointer is argument 1 (RCX), so the float value of argument
                   2 goes to XMM1, not XMM0. The pinned archive reads XMM1. */
                if (v->kind == R6_VAL_REG)
                    fprintf(o, "\tmovsd -%u(%%rbp), %%xmm1\n", (unsigned)(v->as.i32 + 1) * 8);
                else
                    fprintf(o, "\tmovabsq $0x%016llx, %%rax\n\tmovq %%rax, %%xmm1\n",
                            (unsigned long long)v->as.f64_bits);
                fprintf(o, "\tleaq -%u(%%rbp), %%rcx\n\tcall orbit_result_ok_float\n", cell);
            } else {
                if (v->kind == R6_VAL_REG) {
                    if (is_bool(t))
                        fprintf(o, "\tmovl -%u(%%rbp), %%edx\n", (unsigned)(v->as.i32 + 1) * 8);
                    else
                        fprintf(o, "\tmovq -%u(%%rbp), %%rdx\n", (unsigned)(v->as.i32 + 1) * 8);
                } else if (v->kind == R6_VAL_STRING) {
                    emit_string(o, v->as.string, string_id);
                    fprintf(o, "\tleaq .LCr6_%llu(%%rip), %%rdx\n", string_id++);
                } else if (v->kind == R6_VAL_INT) {
                    fprintf(o, "\tmovabsq $%lld, %%rax\n\tmovq %%rax, %%rdx\n", (long long)v->as.i64);
                } else {
                    fprintf(o, "\tmovl $%d, %%edx\n", v->as.boolean ? 1 : 0);
                }
                fprintf(o, "\tleaq -%u(%%rbp), %%rcx\n\tcall orbit_result_ok\n", cell);
            }
            break;
        }
        case R6_OP_RESULT_ERR: {
            unsigned cell = result_cell_offset(f, is_main, i->dest);
            if (i->operands[1].kind == R6_VAL_NONE) {
                /* Single-argument form: O1 is the error code, message NULL. */
                if (v->kind == R6_VAL_REG)
                    fprintf(o, "\tmovq -%u(%%rbp), %%rdx\n", (unsigned)(v->as.i32 + 1) * 8);
                else {
                    emit_int_rax(o, v->as.i64);
                    fputs("\tmovq %rax, %rdx\n", o);
                }
                fputs("\tmovl $0, %r8d\n", o);
            } else {
                if (v->kind == R6_VAL_REG)
                    fprintf(o, "\tmovq -%u(%%rbp), %%r8\n", (unsigned)(v->as.i32 + 1) * 8);
                else {
                    emit_string(o, v->as.string, string_id);
                    fprintf(o, "\tleaq .LCr6_%llu(%%rip), %%r8\n", string_id++);
                }
                if (i->operands[1].kind == R6_VAL_REG)
                    fprintf(o, "\tmovq -%u(%%rbp), %%rdx\n", (unsigned)(i->operands[1].as.i32 + 1) * 8);
                else {
                    emit_int_rax(o, i->operands[1].as.i64);
                    fputs("\tmovq %rax, %rdx\n", o);
                }
            }
            fprintf(o, "\tleaq -%u(%%rbp), %%rcx\n\tcall orbit_result_err\n", cell);
            break;
        }
        case R6_OP_RESULT_IS_OK: {
            unsigned cell = result_cell_offset(f, is_main, i->operands[0].as.i32);
            fprintf(o, "\tmovl -%u(%%rbp), %%eax\n\tmovzbl %%al, %%eax\n\tmovl %%eax, -%u(%%rbp)\n",
                    cell, off);
            break;
        }
        case R6_OP_RESULT_UNWRAP: {
            unsigned cell = result_cell_offset(f, is_main, i->operands[0].as.i32);
            if (is_float(type_at(f, i->dest)))
                fprintf(o, "\tleaq -%u(%%rbp), %%rcx\n\tcall orbit_result_get_float\n\tmovsd %%xmm0, -%u(%%rbp)\n",
                        cell, off);
            else
                fprintf(o, "\tmovq -%u(%%rbp), %%rax\n\tmovq %%rax, -%u(%%rbp)\n", cell - 16, off);
            break;
        }
        case R6_OP_RESULT_UNWRAP_ERR: {
            unsigned cell = result_cell_offset(f, is_main, i->operands[0].as.i32);
            unsigned long long empty_id = string_id++;
            emit_string(o, "", empty_id);
            fprintf(o, "\tmovq -%u(%%rbp), %%rax\n\ttestq %%rax, %%rax\n"
                       "\tjne .Lr6_f%zu_result_unwrap_err_%zu\n"
                       "\tleaq .LCr6_%llu(%%rip), %%rax\n"
                       ".Lr6_f%zu_result_unwrap_err_%zu:\n"
                       "\tmovq %%rax, -%u(%%rbp)\n",
                    cell - 8, fn_index, pc, empty_id, fn_index, pc, off);
            break;
        }
        case R6_OP_NOP:
        case R6_OP_BEGIN_BLOCK:
        case R6_OP_END_BLOCK:
            break; /* no machine code: same no-op as the pinned C backend */
        default:
            return fail(d, i, "CGEN", "unsupported opcode %s", r6_opcode_name(i->opcode));
        }
    }
    emit_div_traps(o, fn_index);
    fprintf(o, "\t.seh_endproc\n");
    return !ferror(o);
}

/* D-M1-87: the pin's main opens the database and creates one table per model
 * with fields when the module calls a database builtin (c_backend.orb,
 * generateModelDDL: `<lowercased name>s`, int and bool INTEGER, float REAL,
 * the rest TEXT, `id` the primary key). D-M1-92: the same function then runs
 * the module's migrations, `orbit_run_migrations(_orbit_migs, N)`, and exits 1
 * when it answers false. */
static bool module_uses_db(const R6Module *m) {
    size_t n;
    for (n = 0; n < m->function_count; ++n)
        if (m->functions[n].is_extern && m->functions[n].name &&
            strncmp(m->functions[n].name, "orbit_db_", 9) == 0)
            return true;
    return false;
}

static void emit_db_text(FILE *o, const char *text, unsigned long long id) {
    emit_string(o, text, id);
}

/* D-M1-102: the start-up pieces of docs/STARTUP.md in the pin, decided from
 * the callee names of the module as c_backend.orb `moduleCallsStartup` does. */
static bool callee_has_prefix(const char *name, const char *prefix) {
    return strncmp(name, prefix, strlen(prefix)) == 0;
}

static bool module_calls_startup(const R6Module *m, int what) {
    size_t n, pc;
    for (n = 0; n < m->function_count; ++n) {
        const R6Function *f = &m->functions[n];
        for (pc = 0; pc < f->instruction_count; ++pc) {
            const R6Instruction *i = &f->instructions[pc];
            const char *name;
            if (i->opcode != R6_OP_CALL) continue;
            if (i->operands[0].kind != R6_VAL_STRING && i->operands[0].kind != R6_VAL_SYMBOL) continue;
            name = i->operands[0].as.string;
            if (!name) continue;
            if (what == 0 && (callee_has_prefix(name, "orbit_os_argc") ||
                              callee_has_prefix(name, "orbit_os_argv")))
                return true;
            if (what == 1 && (strcmp(name, "print") == 0 ||
                              callee_has_prefix(name, "orbit_os_write_std")))
                return true;
            if (what == 2 && (strcmp(name, "orbit_file_upload_save") == 0 ||
                              callee_has_prefix(name, "orbit_http_") ||
                              callee_has_prefix(name, "orbit_response_") ||
                              callee_has_prefix(name, "orbit_route_") ||
                              callee_has_prefix(name, "orbit_send_") ||
                              callee_has_prefix(name, "orbit_kynx_") ||
                              callee_has_prefix(name, "orbit_pulse_") ||
                              callee_has_prefix(name, "orbit_ledger_")))
                return true;
        }
    }
    return false;
}

bool r6_module_uses_args(const R6Module *m) { return module_calls_startup(m, 0); }

/* A database program counts as a network program: the SQLite progress handler
 * lives in kynx.c. */
bool r6_module_uses_network(const R6Module *m) {
    return module_uses_db(m) || module_calls_startup(m, 2);
}

/* D-M1-104: a module that reads argv or links the network modules (a database
 * program does) keeps the MinGW start-up and its `main`. Any other module has
 * R6's own process entry `orbit_start`, which needs nothing from the CRT. */
bool r6_module_native_entry(const R6Module *m) {
    /* Migrations are database work: the module keeps the main that refuses
     * them without an orbit_db_* extern. */
    return !m->migration_count && !r6_module_uses_args(m) &&
           !r6_module_uses_network(m);
}

static bool emit_entry_wrapper(FILE *o, const R6Module *m, R6Diagnostic *d) {
    bool db = module_uses_db(m);
    if (r6_module_native_entry(m)) {
        /* msvcrt.dll initializes itself when the loader maps it. The entry
         * only names the application type, which msvcrt reads to choose where
         * its fatal diagnostics go, and ends in `exit`, which flushes stdio and
         * runs the atexit table before ExitProcess. */
        fputs("\t.text\n\t.globl orbit_start\n"
              "\t.def orbit_start; .scl 2; .type 32; .endef\n"
              "\t.seh_proc orbit_start\norbit_start:\n"
              "\tpushq %rbp\n\t.seh_pushreg %rbp\n"
              "\tmovq %rsp, %rbp\n\t.seh_setframe %rbp, 0\n"
              "\tsubq $48, %rsp\n\t.seh_stackalloc 48\n"
              "\t.seh_endprologue\n"
              "\tmovl $1, %ecx\n\tcall __set_app_type\n"
              "\tcall orbit_crash_handler_install\n", o);
        if (module_calls_startup(m, 1)) fputs("\tcall orbit_console_init\n", o);
        fputs("\tmovl $131072, %ecx\n\tcall orbit_arena_create\n"
              "\tmovq %rax, -8(%rbp)\n"
              "\tmovq %rax, %rcx\n\tcall orbit_main\n"
              "\tmovl %eax, -16(%rbp)\n"
              "\tmovq -8(%rbp), %rcx\n\tcall orbit_arena_destroy\n"
              "\tmovl -16(%rbp), %ecx\n\tcall exit\n\tud2\n"
              "\t.seh_endproc\n", o);
        return !ferror(o);
    }
    unsigned long long base = 1ULL << 62, next = base + 1;
    char ddl[8][1024];
    unsigned long long ddl_id[8];
    unsigned long long mig_id = 0;
    size_t ddl_count = 0, k, j;
    /* The pin opens the database for migrations alone too, but the driver
       links the DB archive by the orbit_db_* externs, so a migration-only
       module has no measured link and stays refused. */
    if (m->migration_count && !db)
        return fail(d, NULL, "CGEN", "a module with migrations that calls no orbit_db_* builtin is outside the M1 native subset");
    if (db) {
        emit_db_text(o, ".orbit/data/app.db", base);
        for (k = 0; k < m->model_count; ++k) {
            const R6Model *model = &m->models[k];
            size_t len = 0;
            if (!model->field_count) continue;
            if (ddl_count == 8)
                return fail(d, NULL, "CGEN", "a database module with more than 8 models is outside the M1 native subset");
            len += (size_t)snprintf(ddl[ddl_count], sizeof ddl[0], "CREATE TABLE IF NOT EXISTS ");
            for (j = 0; model->name[j] && len + 4 < sizeof ddl[0]; ++j) {
                char c = model->name[j];
                ddl[ddl_count][len++] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
            }
            len += (size_t)snprintf(ddl[ddl_count] + len, sizeof ddl[0] - len, "s (");
            for (j = 0; j < model->field_count && len + 128 < sizeof ddl[0]; ++j) {
                const char *ft = model->fields[j].type_name;
                len += (size_t)snprintf(ddl[ddl_count] + len, sizeof ddl[0] - len, "%s%s %s%s",
                                        j ? ", " : "", model->fields[j].name,
                                        is_int(ft) || is_bool(ft) ? "INTEGER" : is_float(ft) ? "REAL" : "TEXT",
                                        strcmp(model->fields[j].name, "id") == 0 ? " PRIMARY KEY" : "");
            }
            if (j != model->field_count || len + 3 >= sizeof ddl[0])
                return fail(d, NULL, "CGEN", "model %s is too large for the database table text", model->name);
            snprintf(ddl[ddl_count] + len, sizeof ddl[0] - len, ");");
            ddl_id[ddl_count] = next++;
            emit_db_text(o, ddl[ddl_count], ddl_id[ddl_count]);
            ++ddl_count;
        }
        if (m->migration_count) {
            /* The array `_orbit_migs` of the pin's block: one pointer slot per
               migration in .data, filled by main before the call. */
            mig_id = next;
            for (k = 0; k < m->migration_count; ++k) {
                if (!m->migrations[k])
                    return fail(d, NULL, "CGEN", "migration %zu has no text", k);
                emit_db_text(o, m->migrations[k], next++);
            }
            fputs("\t.section .data,\"dw\"\n", o);
            for (k = 0; k < m->migration_count; ++k)
                fprintf(o, "r6_migration_%zu:\n\t.byte 0,0,0,0,0,0,0,0\n", k);
            fputs("\t.text\n", o);
        }
    }
    fputs("\t.text\n\t.globl main\n"
          "\t.def main; .scl 2; .type 32; .endef\n"
          "\t.seh_proc main\nmain:\n"
          "\tpushq %rbp\n\t.seh_pushreg %rbp\n"
          "\tmovq %rsp, %rbp\n\t.seh_setframe %rbp, 0\n"
          "\tsubq $48, %rsp\n\t.seh_stackalloc 48\n"
          "\t.seh_endprologue\n"
          , o);
    /* D-M1-37, D-M1-102: the pinned main stores argc/argv only when the module
     * reads them; an unconditional reference links os.o. */
    if (r6_module_uses_args(m))
        fputs("\tleaq _orbit_argc(%rip), %rax\n\tmovl %ecx, 0(%rax)\n"
              "\tleaq _orbit_argv(%rip), %rax\n\tmovq %rdx, 0(%rax)\n", o);
    fputs("\tcall orbit_crash_handler_install\n", o);
    if (module_calls_startup(m, 1)) fputs("\tcall orbit_console_init\n", o);
    if (db) {
        fprintf(o, "\tleaq .LCr6_%llu(%%rip), %%rcx\n\tcall orbit_db_init\n", base);
        for (k = 0; k < ddl_count; ++k)
            fprintf(o, "\tleaq .LCr6_%llu(%%rip), %%rcx\n\tcall orbit_db_exec_ddl\n", ddl_id[k]);
        if (m->migration_count) {
            for (k = 0; k < m->migration_count; ++k)
                fprintf(o, "\tleaq .LCr6_%llu(%%rip), %%rax\n"
                           "\tleaq r6_migration_%zu(%%rip), %%rcx\n"
                           "\tmovq %%rax, 0(%%rcx)\n", mig_id + k, k);
            /* `if (!orbit_run_migrations(...)) exit(1)`; the C writes the
               message to stderr first, which the verdict does not compare. */
            fprintf(o, "\tleaq r6_migration_0(%%rip), %%rcx\n"
                       "\tmovl $%zu, %%edx\n\tcall orbit_run_migrations\n"
                       "\tcmpb $0, %%al\n\tjne .Lr6_migrations_ok\n"
                       "\tmovl $1, %%ecx\n\tcall exit\n"
                       ".Lr6_migrations_ok:\n", m->migration_count);
        }
    }
    fputs("\tmovl $131072, %ecx\n\tcall orbit_arena_create\n"
          "\tmovq %rax, -8(%rbp)\n"
          "\tmovq %rax, %rcx\n\tcall orbit_main\n"
          "\tmovl %eax, -16(%rbp)\n"
          "\tmovq -8(%rbp), %rcx\n\tcall orbit_arena_destroy\n"
          "\tmovl -16(%rbp), %eax\n\tleave\n\tret\n"
          "\t.seh_endproc\n", o);
    return !ferror(o);
}

/* ---- D-M1-20: recovering a register type the builder left `unknown` ------
 *
 * The pinned builder serializes the module before the C backend's
 * inferRegisterTypes runs (c_backend.orb:674), so a register it could not
 * type is written as the literal text `unknown` and the module check below
 * refuses it. That is the widest wall the emitted corpus hits: 84 of the 90
 * documents carry `unknown`, 6735 slots in all, and 47 documents stop exactly
 * there. Spec 2.3 makes registerTypes advisory - the language recovers those
 * slots from the defining instruction, that recovery is part of the contract,
 * and its rules are the authority mirrored here.
 *
 * Three passes, like the original, because a rule can only fire once its
 * operands are known. Two differences, both narrowing what R6 accepts:
 *
 *   * the language's registers are untyped uintptr_t words, so its arithmetic
 *     rules are loose (`add` is "string" when an operand is a string and
 *     "int" otherwise; sub/mul/div/mod/neg are always "int"). R6 emits typed
 *     x64 and needs the real type, so it infers "float" when an arithmetic
 *     operand is float, "int" when both operands are int, and leaves the slot
 *     unknown when they do not agree.
 *   * a rule the language resolves through its string/float spreading passes
 *     (an enum tag constant, `result_unwrap`), through a call target that is
 *     not declared in the module, or into a type the native backend has no
 *     register for (`map`) leaves the slot unknown, which keeps the
 *     fail-closed rejection.
 *
 * A replaced slot is reallocated: the loader frees every register type with
 * free() (ir.c). The replacement happens in place and before any other
 * reader, so the module check and the emitter both see the recovered type and
 * no lookup path changes. */

static char *dup_type_text(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* A value's type where any literal is allowed. NULL for a Symbol (D-M1-19:
 * its text is a literal only when the declared type says so) and for a
 * register whose slot is still unknown. */
static const char *infer_value_type(const R6Function *f, const R6Value *v) {
    switch (v->kind) {
    case R6_VAL_INT: return "int";
    case R6_VAL_FLOAT64: return "float";
    case R6_VAL_BOOL: return "bool";
    case R6_VAL_STRING: return "string";
    case R6_VAL_REG: return type_at(f, v->as.i32);
    default: return NULL;
    }
}

/* "float" when either side is float, "int" when both are int, NULL when
 * neither holds (including when a side is still unknown). */
static const char *infer_arith_type(const char *a, const char *b) {
    if (is_float(a) || is_float(b)) return "float";
    if (is_int(a) && is_int(b)) return "int";
    return NULL;
}

/* The dest classes the opcode alone decides, mirroring the language's
 * eq/ne/.../map_has -> bool and result_unwrap_err -> string rules. */
static const char *infer_cls_type(R6Opcode op) {
    switch (op) {
    case R6_OP_EQ: case R6_OP_NE: case R6_OP_LT: case R6_OP_LE:
    case R6_OP_GT: case R6_OP_GE: case R6_OP_AND_OP: case R6_OP_OR_OP:
    case R6_OP_NOT_OP: case R6_OP_RESULT_IS_OK: case R6_OP_MAP_HAS:
        return "bool";
    case R6_OP_RESULT_UNWRAP_ERR:
        return "string";
    default:
        return NULL;
    }
}

static const char *infer_dest_type(const R6Module *m, const R6Function *f,
                                   const R6Instruction *i,
                                   char *buf, size_t cap) {
    const R6Value *o1 = &i->operands[0], *o2 = &i->operands[1];
    const char *cls = infer_cls_type(i->opcode);
    if (cls) return cls;
    switch (i->opcode) {
    case R6_OP_LOAD_CONST:
        if (o1->kind == R6_VAL_SYMBOL) {
            const char *s = o1->as.string;
            return s && (strcmp(s, "true") == 0 || strcmp(s, "false") == 0)
                       ? "bool" : NULL;
        }
        return infer_value_type(f, o1);
    case R6_OP_LOAD_VAR: {
        const char *name = ir_name(o1), *type = NULL;
        const R6Global *g;
        int32_t ordinal;
        if (!name) return NULL;
        if (param_info(f, name, NULL, &type)) return type;
        if (local_info(m, f, name, NULL, &type, buf, cap)) return type;
        if ((g = find_global(m, name)) != NULL) return global_type(g);
        /* `<Type>_TAG_<variant>` is the ordinal constant the builder loads as
         * an int, for a union and for an enumeration alike (D-M1-35). */
        if (tag_constant(m, name, &ordinal)) return "int";
        return NULL;
    }
    case R6_OP_ADD: {
        const char *t1 = infer_value_type(f, o1), *t2 = infer_value_type(f, o2);
        if (is_string(t1) || is_string(t2)) return "string";
        return infer_arith_type(t1, t2);
    }
    case R6_OP_SUB: case R6_OP_MUL: case R6_OP_DIV: case R6_OP_MOD:
        return infer_arith_type(infer_value_type(f, o1),
                                infer_value_type(f, o2));
    case R6_OP_NEG: {
        const char *t = infer_value_type(f, o1);
        return is_float(t) ? "float" : is_int(t) ? "int" : NULL;
    }
    case R6_OP_COPY:
        return infer_value_type(f, o1);
    case R6_OP_ARG:
        if (o1->kind == R6_VAL_INT && o1->as.i64 >= 0 &&
            (size_t)o1->as.i64 < f->param_count)
            return f->param_types[o1->as.i64];
        return NULL;
    case R6_OP_CALL: {
        const char *name = ir_name(o1);
        const R6Function *callee;
        const R6Model *model;
        if (!name) return NULL;    /* indirect: no spelling to read */
        if ((model = find_model(m, name)) != NULL) return model->name;
        /* A variant constructor lands in the union cell D-M1-12 defines. */
        if (find_union_variant(m, name, NULL) != NULL) return "void*";
        /* D-M1-26: the builtin `print` returns void (feBuildTimeCallType,
         * c_backend.orb:3917) and is intercepted by name before any declared
         * function of the same name, exactly like the pinned lowering
         * (:2977-3005). */
        if (o1->kind == R6_VAL_STRING && strcmp(name, "print") == 0) return "void";
        if ((callee = find_function(m, name)) != NULL) return callee->return_type;
        if (o1->kind == R6_VAL_STRING &&
            (callee = find_intrinsic(name)) != NULL) return callee->return_type;
        return NULL;               /* builtin or alias: fail closed */
    }
    case R6_OP_LOAD_FIELD: {
        const R6Model *model;
        const R6Type *pack;
        size_t index, variant, offset, total;
        bool is_pack = false;
        R6Diagnostic scratch;
        if (string_length_field(f, i)) return "int";
        if (resolve_pack_field(m, f, i, &is_pack, &pack, &variant, &index,
                               &offset, &total, &scratch) && is_pack) {
            /* D-M1-53: a pack part reads as its declared payload type. */
            if (!union_payload_part(pack->rich_variants[variant], index, buf, cap))
                return NULL;
            return payload_reg_type(m, buf);
        }
        if (is_pack) return NULL;
        if (!resolve_field(m, f, i, &model, &index, &scratch)) return NULL;
        return model->fields[index].type_name;
    }
    case R6_OP_LIST_CREATE: return "list";
    case R6_OP_STR_LEN: case R6_OP_BYTE_LOAD: return "int";
    case R6_OP_BYTES_ALLOC: case R6_OP_STR_OFFSET: return "string";
    case R6_OP_LIST_LEN: return "int";
    case R6_OP_LIST_GET:
    case R6_OP_LIST_POP:
        if (i->elem_kind == 0 && i->opcode == R6_OP_LIST_GET)
            return opaque_read_type(m, f, (size_t)(i - f->instructions));
        if (i->elem_kind == 5) {
            /* D-M1-57: a ref read bound to a local takes the type of its loads. */
            size_t at = (size_t)(i - f->instructions);
            const char *type = bound_local_type(f, at);
            if (!type || !ref_elem_type(m, type)) return NULL;
            return !is_void_ptr(type) || has_reader(f, at) ? type : NULL;
        }
        return elem_kind_type(i->elem_kind);
    case R6_OP_MAP_KEYS: return "list";
    case R6_OP_RESULT_UNWRAP:
        return unwrap_use_type(f, (size_t)(i - f->instructions));
    case R6_OP_UNION_GET_DATA: {
        const char *type = payload_use_type(m, f, (size_t)(i - f->instructions));
        return type ? type : pack_word_type(m, f, (size_t)(i - f->instructions));
    }
    case R6_OP_UNION_GET_TAG: return "int";   /* D-M1-12's ordinal tag */
    default: return NULL;
    }
}

static void infer_register_types(const R6Module *m, R6Function *f) {
    char buf[R6_FN_SPELLING_CAP];
    size_t pass;
    if (!f || !f->register_types || !f->register_type_count) return;
    for (pass = 0; pass < 3; ++pass) {
        size_t pc;
        bool changed = false;
        for (pc = 0; pc < f->instruction_count; ++pc) {
            const R6Instruction *i = &f->instructions[pc];
            const char *type;
            char *copy;
            if (!i->has_dest || i->dest < 0 ||
                (size_t)i->dest >= f->register_type_count)
                continue;
            if (!f->register_types[i->dest] ||
                !(strcmp(f->register_types[i->dest], "unknown") == 0 ||
                  strcmp(f->register_types[i->dest], "void*") == 0))
                continue;
            type = infer_dest_type(m, f, i, buf, sizeof buf);
            if (!type || strcmp(type, "unknown") == 0 ||
                strcmp(type, f->register_types[i->dest]) == 0) continue;
            copy = dup_type_text(type);
            if (!copy) return;     /* out of memory: keep the fail-closed state */
            free(f->register_types[i->dest]);
            f->register_types[i->dest] = copy;
            changed = true;
        }
        if (!changed) break;
    }
    /* D-M1-46: the builder reserves registers it never uses (a match arm
     * allocates one per pattern). The pin declares such a slot as an unused
     * `uintptr_t r_N;`, so no type is observable; R6 gives it the word type. */
    {
        size_t reg;
        for (reg = 0; reg < f->register_type_count; ++reg) {
            size_t pc2;
            bool used = false;
            char *copy;
            if (!f->register_types[reg] || strcmp(f->register_types[reg], "unknown") != 0)
                continue;
            for (pc2 = 0; pc2 < f->instruction_count && !used; ++pc2) {
                const R6Instruction *i = &f->instructions[pc2];
                unsigned k;
                if (i->has_dest && i->dest == (int32_t)reg) used = true;
                for (k = 0; k < 3; ++k)
                    if (i->operands[k].kind == R6_VAL_REG && i->operands[k].as.i32 == (int32_t)reg)
                        used = true;
            }
            if (used || (copy = dup_type_text("void*")) == NULL) continue;
            free(f->register_types[reg]);
            f->register_types[reg] = copy;
        }
    }
}

static bool codegen_module(FILE *out, R6Module *module, R6Diagnostic *diag) {
    size_t n;
    bool main_found = false;
    if (diag) memset(diag, 0, sizeof *diag);
    if (!out || !module) return fail(diag, NULL, "CGEN", "null output or module");
    if (module->function_count && !module->functions)
        return fail(diag, NULL, "CGEN", "module has missing function storage");
    if (module->global_count && !module->globals)
        return fail(diag, NULL, "CGEN", "module has missing global storage");
    /* D-M1-32: after the storage guards, so a module the loader cannot produce
     * still fails closed instead of dereferencing an absent function array. */
    if (!function_table_build(module)) return fail(diag, NULL, "CGEN", "out of memory indexing functions");
    canonicalize_list_types(module);
    canonicalize_enum_types(module);
    canonicalize_union_types(module);
    canonicalize_result_types(module);
    for (n = 0; n < module->global_count; ++n) {
        size_t prior;
        int64_t ignored;
        if (!global_integer(&module->globals[n], &ignored))
            return fail(diag, NULL, "CGEN", "global %s is outside M1 literal int/bool support",
                        module->globals[n].name ? module->globals[n].name : "<unnamed>");
        for (prior = 0; prior < n; ++prior)
            if (strcmp(module->globals[n].name, module->globals[prior].name) == 0)
                return fail(diag, NULL, "CGEN", "duplicate global %s", module->globals[n].name);
    }
    if (module->type_count && !module->types)
        return fail(diag, NULL, "CGEN", "module has missing type storage");
    for (n = 0; n < module->type_count; ++n) {
        const R6Type *type = &module->types[n];
        /* D-M1-35: an enumeration carries no payloads. Its variants become
         * `typedef enum` members, so a rich variant would be silently dropped
         * by the C backend; the subset fails closed instead. */
        bool is_enum = type->kind && strcmp(type->kind, "enumeration") == 0;
        const char *what = is_enum ? "enumeration" : "union";
        size_t v;
        if (!type->name || !valid_symbol(type->name))
            return fail(diag, NULL, "CGEN", "type name is not a supported native symbol");
        if (!type->kind ||
            (strcmp(type->kind, "union_type") != 0 && strcmp(type->kind, "enumeration") != 0))
            return fail(diag, NULL, "CGEN", "M1 codegen does not support type kind %s",
                        type->kind && type->kind[0] ? type->kind : "<missing>");
        if (type->variant_count < 1 || type->variant_count != type->rich_variant_count ||
            (type->variant_count && (!type->variants || !type->rich_variants)))
            return fail(diag, NULL, "CGEN", "%s %s variant metadata is inconsistent",
                        what, type->name);
        for (v = 0; v < type->variant_count; ++v) {
            size_t parts = union_payload_count(type->rich_variants[v]), pi;
            if (!type->variants[v] || !type->variants[v][0] || !valid_symbol(type->variants[v]))
                return fail(diag, NULL, "CGEN", "%s %s has an invalid variant name",
                            what, type->name);
            if (is_enum) {
                if (parts != 0)
                    return fail(diag, NULL, "CGEN",
                                "enumeration %s variant %s carries a payload, which the M1 native subset does not support",
                                type->name, type->variants[v]);
                continue;
            }
            if (parts > 4)
                return fail(diag, NULL, "CGEN", "union %s variant %s payload arity is outside the M1 native subset",
                            type->name, type->variants[v]);
            for (pi = 0; pi < parts; ++pi) {
                char part[64];
                if (!union_payload_part(type->rich_variants[v], pi, part, sizeof part) ||
                    !union_payload_supported(module, part))
                    return fail(diag, NULL, "CGEN", "union %s variant %s payloads are outside the M1 native subset",
                                type->name, type->variants[v]);
            }
        }
    }
    if (module->model_count && !module->models)
        return fail(diag, NULL, "CGEN", "module has missing model storage");
    for (n = 0; n < module->model_count; ++n) {
        const R6Model *model = &module->models[n];
        size_t k;
        if (!model->name || !valid_symbol(model->name))
            return fail(diag, NULL, "CGEN", "model name is not a supported native symbol");
        if (model->field_count && !model->fields)
            return fail(diag, NULL, "CGEN", "model %s has missing field storage", model->name);
        for (k = 0; k < model->field_count; ++k) {
            size_t prior, size, align;
            if (!model->fields[k].name || !model->fields[k].name[0] ||
                !model->fields[k].type_name || !model->fields[k].type_name[0])
                return fail(diag, NULL, "CGEN", "model %s has a field without name or type", model->name);
            for (prior = 0; prior < k; ++prior)
                if (strcmp(model->fields[prior].name, model->fields[k].name) == 0)
                    return fail(diag, NULL, "CGEN", "model %s has duplicate field %s",
                                model->name, model->fields[k].name);
            if (!model_field_size(module, model->fields[k].type_name, &size, &align))
                return fail(diag, NULL, "CGEN", "model %s field %s has unsupported type %s",
                            model->name, model->fields[k].name, model->fields[k].type_name);
        }
    }
    for (n = 0; n < module->function_count; ++n) {
        R6Function *fm = &module->functions[n];   /* D-M1-20 rewrites types */
        const R6Function *f = fm;
        size_t k;
        if (!f->name) return fail(diag, NULL, "CGEN", "function has null name");
        if (!local_table_build(f)) return fail(diag, NULL, "CGEN", "out of memory indexing locals of %s", f->name);
        if (strcmp(f->name, "main") == 0 && !f->is_extern) main_found = true;
        for (k = 0; k < n; ++k)
            if (strcmp(module->functions[k].name, f->name) == 0)
                return fail(diag, NULL, "CGEN", "duplicate function %s", f->name);
        if (f->register_count < 0 || (size_t)f->register_count != f->register_type_count)
            return fail(diag, NULL, "CGEN", "function %s has inconsistent register types", f->name);
        if (f->register_type_count && !f->register_types)
            return fail(diag, NULL, "CGEN", "function %s has missing register type metadata", f->name);
        /* D-M1-20: recover the slots the builder serialized before it could
         * type them; a slot no rule explains stays `unknown` and is refused
         * by the loop below. */
        infer_register_types(module, fm);
        for (k = 0; k < f->register_type_count; ++k) {
            char fret[16];
            const char *rt = f->register_types[k];
            if (is_fn_type(rt)) {
                /* Function values: the measured scalar signature subset. */
                if (!fn_type_ok(rt, fret, sizeof fret, NULL))
                    return fail(diag, NULL, "CGEN", "register type %s is unsupported in M1", rt);
                continue;
            }
            if (!is_register_type(rt) && !is_void_ptr(rt) && !is_rtarena(rt) &&
                !is_model_type(module, rt))
                return fail(diag, NULL, "CGEN", "register type %s is unsupported in M1", rt);
        }
        if (!validate_function(module, f, diag)) return false;
    }
    if (!main_found && !module->runtime_module)
        return fail(diag, NULL, "CGEN", "module must define one non-extern main");
    if (!mark_live_functions(module)) return fail(diag, NULL, "CGEN", "out of memory marking reachable functions");
    /* D-M1-105: the archive member may be linked into a program the C backend
       built, which has no r6_arena; the operations that read it are refused. */
    if (module->runtime_module && module_needs_arena_global(module))
        return fail(diag, NULL, "CGEN", "a runtime module cannot use the executable arena");
    if (module_needs_arena_global(module))
        fputs("\t.section .data,\"dw\"\nr6_arena:\n\t.byte 0,0,0,0,0,0,0,0\n\t.text\n", out);
    for (n = 0; n < module->function_count; ++n) {
        if (!module->functions[n].live) continue;
        if (!local_table_build(&module->functions[n]))
            return fail(diag, NULL, "CGEN", "out of memory indexing locals of %s", module->functions[n].name);
        if (!emit_function(out, module, &module->functions[n], n, diag)) return false;
    }
    if (!module->runtime_module && !emit_entry_wrapper(out, module, diag)) return false;
    return !ferror(out);
}

bool r6_codegen_win64(FILE *out, R6Module *module, R6Diagnostic *diag) {
    bool ok;
    local_table_release();
    function_table_release();
    ok = codegen_module(out, module, diag);
    local_table_release();
    function_table_release();
    return ok;
}
