#ifndef R6_IR_H
#define R6_IR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    R6_OP_NOP, R6_OP_LOAD_CONST, R6_OP_LOAD_VAR, R6_OP_STORE_VAR,
    R6_OP_DECL_VAR, R6_OP_ADD, R6_OP_SUB, R6_OP_MUL, R6_OP_DIV,
    R6_OP_MOD, R6_OP_EQ, R6_OP_NE, R6_OP_LT, R6_OP_LE, R6_OP_GT,
    R6_OP_GE, R6_OP_AND_OP, R6_OP_OR_OP, R6_OP_NOT_OP, R6_OP_NEG,
    R6_OP_BEGIN_BLOCK, R6_OP_END_BLOCK, R6_OP_CALL, R6_OP_RET,
    R6_OP_JUMP, R6_OP_JUMP_IF_FALSE, R6_OP_ALLOC, R6_OP_FREE,
    R6_OP_DB_GET, R6_OP_DB_SET, R6_OP_DB_ALL, R6_OP_DB_WHERE,
    R6_OP_HTTP_RESPONSE, R6_OP_LABEL, R6_OP_COPY, R6_OP_ARG,
    R6_OP_LOAD_FIELD, R6_OP_STORE_FIELD, R6_OP_SWITCH_OP,
    R6_OP_TYPE_INFO, R6_OP_LIST_CREATE, R6_OP_LIST_PUSH,
    R6_OP_LIST_GET, R6_OP_LIST_SET, R6_OP_LIST_LEN, R6_OP_LIST_POP,
    R6_OP_MAP_CREATE, R6_OP_MAP_SET, R6_OP_MAP_GET, R6_OP_MAP_HAS,
    R6_OP_MAP_DELETE, R6_OP_MAP_KEYS, R6_OP_RESULT_OK,
    R6_OP_RESULT_ERR, R6_OP_RESULT_UNWRAP, R6_OP_RESULT_IS_OK,
    R6_OP_RESULT_UNWRAP_ERR, R6_OP_UNION_CREATE,
    R6_OP_UNION_GET_TAG, R6_OP_UNION_GET_DATA, R6_OP_BIT_AND,
    R6_OP_BIT_OR, R6_OP_BIT_XOR, R6_OP_SHL, R6_OP_SHR, R6_OP_BIT_NOT,
    R6_OP_STR_LEN, R6_OP_BYTE_LOAD, R6_OP_BYTE_STORE, R6_OP_BYTES_ALLOC,
    R6_OP_STR_OFFSET, R6_OP_BYTES_COPY,
    R6_OP_REINTERPRET, R6_OP_MEM_LOAD, R6_OP_MEM_STORE,
    R6_OP_COUNT
} R6Opcode;

typedef enum { R6_VAL_NONE, R6_VAL_INT, R6_VAL_FLOAT64, R6_VAL_STRING,
               R6_VAL_SYMBOL, R6_VAL_BOOL, R6_VAL_REG, R6_VAL_LABEL } R6ValueKind;
typedef struct {
    R6ValueKind kind;
    size_t string_len;
    union { int64_t i64; int32_t i32; uint64_t f64_bits; char *string; bool boolean; } as;
} R6Value;

typedef struct {
    R6Opcode opcode;
    bool has_dest;
    int32_t dest;
    R6Value operands[3];
    int32_t elem_kind;
    size_t line, column;
    int64_t source_line; /* Orbit line of a div or mod; 0 when absent */
} R6Instruction;

typedef struct { char *name; char *type_name; } R6Field;
typedef struct {
    char *name; size_t field_count; R6Field *fields;
    size_t type_param_count; char **type_params;
} R6Model;
typedef struct {
    char *name, *kind; size_t variant_count, rich_variant_count, method_count;
    char **variants, **rich_variants, **methods;
} R6Type;
typedef struct {
    char *method, *path; bool has_limit;
    int64_t limit_rate, limit_window_ms, limit_burst;
} R6Route;
typedef struct {
    char *name; size_t param_count; char **params, **param_types;
    size_t instruction_count; R6Instruction *instructions;
    int32_t register_count; size_t register_type_count; char **register_types;
    char *return_type; bool is_extern, has_route; R6Route route;
    bool live; /* set by codegen (D-M1-65): reachable from main */
    bool is_export; /* D-M1-105: runtime-module function whose symbol is its IR name */
} R6Function;
typedef struct { char *name; R6Value value; char *type_name; } R6Global;
typedef struct {
    size_t function_count; R6Function *functions;
    size_t global_count; R6Global *globals;
    size_t model_count; R6Model *models;
    size_t type_count; R6Type *types;
    size_t migration_count; char **migrations;
    char *source_name; /* base name of the root file; "" when absent */
    bool runtime_module; /* D-M1-105: archive member of the runtime, no main */
} R6Module;

typedef struct { size_t line, column; char code[16]; char message[256]; } R6Diagnostic;

/* On failure, returns false and fills diagnostic. Output is always initialized. */
bool r6_ir_parse(const char *data, size_t length, R6Module *out, R6Diagnostic *diagnostic);
void r6_module_free(R6Module *module);
const char *r6_opcode_name(R6Opcode opcode);

#endif
