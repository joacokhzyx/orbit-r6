#include "win64.h"

#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

/* M1 deliberately uses memory-backed IR registers. No register allocation,
 * peephole rewriting, or semantic fallback is performed here. */

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
static bool is_string(const char *t) { return t && strcmp(t, "string") == 0; }
static bool is_bool(const char *t) { return t && strcmp(t, "bool") == 0; }
static bool is_scalar(const char *t) {
    return is_int(t) || is_float(t) || is_string(t) || is_bool(t);
}
static bool same_type(const char *a, const char *b) {
    return a && b && strcmp(a, b) == 0;
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

static const R6Function *find_function(const R6Module *m, const char *name) {
    size_t n;
    for (n = 0; n < m->function_count; ++n)
        if (m->functions[n].name && strcmp(m->functions[n].name, name) == 0)
            return &m->functions[n];
    return NULL;
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

static bool validate_function(const R6Module *m, const R6Function *f,
                              R6Diagnostic *d) {
    size_t pc, arg_count = 0;
    const R6Instruction *arg_values[256];
    if (!valid_symbol(f->name))
        return fail(d, NULL, "CGEN", "function name is not a supported native symbol");
    if (!f->return_type)
        return fail(d, NULL, "CGEN", "function %s has no return type", f->name);
    if (f->is_extern) {
        if (f->instruction_count)
            return fail(d, NULL, "CGEN", "extern function %s must not contain instructions", f->name);
        if (f->param_count > 4 || (f->param_count && !f->param_types))
            return fail(d, NULL, "CGEN", "extern function %s has unsupported parameter metadata", f->name);
        for (pc = 0; pc < f->param_count; ++pc)
            if (!is_scalar(f->param_types[pc]))
                return fail(d, NULL, "CGEN", "extern parameter type is unsupported in M1");
        if (strcmp(f->return_type, "void") != 0 && !is_scalar(f->return_type))
            return fail(d, NULL, "CGEN", "extern return type is unsupported in M1");
        return true;
    }
    if (f->param_count && (!f->param_types || !f->params))
        return fail(d, NULL, "CGEN", "function %s has missing parameter metadata", f->name);
    if (f->param_count > 4)
        return fail(d, NULL, "CGEN", "function %s has more than four parameters (M1 limit)", f->name);
    if (strcmp(f->name, "main") == 0 &&
        (f->param_count != 0 || !is_int(f->return_type)))
        return fail(d, NULL, "CGEN", "main must have no parameters and return int in M1");
    for (pc = 0; pc < f->param_count; ++pc)
        if (!is_scalar(f->param_types[pc]))
            return fail(d, NULL, "CGEN", "parameter type %s is unsupported in M1", f->param_types[pc]);
    for (pc = 0; pc < f->instruction_count; ++pc) {
        const R6Instruction *i = &f->instructions[pc];
        const char *a = NULL, *b = NULL;
        switch (i->opcode) {
        case R6_OP_LOAD_CONST:
            if (!valid_dest(f, i, true, d)) return false;
            if (!validate_value_for_register(f, &i->operands[0], i->dest, i, d)) return false;
            break;
        case R6_OP_COPY:
            if (!valid_dest(f, i, true, d) ||
                !validate_value_for_register(f, &i->operands[0], i->dest, i, d)) return false;
            if (!is_scalar(type_at(f, i->dest)))
                return fail(d, i, "CGEN", "copy currently supports scalar registers only");
            if (i->operands[0].kind == R6_VAL_STRING &&
                type_at(f, i->dest) && is_string(type_at(f, i->dest)))
                return fail(d, i, "CGEN", "copy from string literal is not supported; load_const handles literals");
            break;
        case R6_OP_ADD:
            if (!valid_dest(f, i, true, d) || !operand_reg(f, i, 0, &a, d) ||
                !operand_reg(f, i, 1, &b, d)) return false;
            if (!same_type(a, b) || !same_type(a, type_at(f, i->dest)) ||
                !(is_int(a) || is_float(a) || is_string(a)))
                return fail(d, i, "CGEN", "add supports matching int, float, or string registers only");
            if (is_string(a) && strcmp(f->name, "main") != 0)
                return fail(d, i, "CGEN", "string add outside main requires the Orbit arena parameter contract");
            break;
        case R6_OP_SUB: case R6_OP_MUL: case R6_OP_DIV: case R6_OP_MOD:
            if (!valid_dest(f, i, true, d) || !operand_reg(f, i, 0, &a, d) ||
                !operand_reg(f, i, 1, &b, d)) return false;
            if (!same_type(a, b) || !same_type(a, type_at(f, i->dest)) ||
                !(is_int(a) || (i->opcode != R6_OP_MOD && is_float(a))))
                return fail(d, i, "CGEN", "sub/mul/div require matching int/float operands; mod requires int");
            break;
        case R6_OP_EQ:
            if (!valid_dest(f, i, true, d) || !operand_reg(f, i, 0, &a, d) ||
                !operand_reg(f, i, 1, &b, d)) return false;
            if (!same_type(a, b) || !is_bool(type_at(f, i->dest)) ||
                !(is_int(a) || is_float(a) || is_bool(a)))
                return fail(d, i, "CGEN", "eq supports matching int, float, or bool registers and bool destination only");
            break;
        case R6_OP_NE: case R6_OP_LT: case R6_OP_LE: case R6_OP_GT: case R6_OP_GE:
            if (!valid_dest(f, i, true, d) || !operand_reg(f, i, 0, &a, d) ||
                !operand_reg(f, i, 1, &b, d)) return false;
            if (!(is_int(a) || is_float(a)) || !same_type(a, b) ||
                !is_bool(type_at(f, i->dest)))
                return fail(d, i, "CGEN", "ne/lt/le/gt/ge require matching int/float operands and bool destination");
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
                int32_t ix;
                if (i->operands[0].kind != R6_VAL_INT)
                    return fail(d, i, "CGEN", "arg parameter binding requires an integer index");
                ix = i->operands[0].as.i32;
                if (ix < 0 || (size_t)ix >= f->param_count ||
                    !valid_dest(f, i, true, d))
                    return fail(d, i, "CGEN", "arg parameter index/destination is invalid");
                if (!same_type(type_at(f, i->dest), f->param_types[ix]))
                    return fail(d, i, "CGEN", "arg binding type does not match parameter %d", ix);
            } else {
                const char *t;
                if (arg_count == sizeof arg_values / sizeof arg_values[0])
                    return fail(d, i, "CGEN", "more than 256 pending call arguments are unsupported");
                if (!value_type(f, &i->operands[0], &t) || !is_scalar(t))
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
                if (arg_count > 4)
                    return fail(d, i, "CGEN", "calls with more than four arguments are unsupported in M1");
            }
            break;
        case R6_OP_CALL: {
            const R6Function *callee;
            size_t k;
            if (i->operands[0].kind != R6_VAL_SYMBOL ||
                !valid_symbol(i->operands[0].as.string))
                return fail(d, i, "CGEN", "call requires a direct native symbol");
            callee = find_function(m, i->operands[0].as.string);
            if (!callee) return fail(d, i, "CGEN", "call target %s is not declared in the module", i->operands[0].as.string);
            if (strcmp(callee->name, "main") == 0)
                return fail(d, i, "CGEN", "main is an entry point and cannot be called from IR");
            if (callee->param_count && !callee->param_types)
                return fail(d, i, "CGEN", "call target %s has missing parameter metadata", callee->name);
            if (arg_count != callee->param_count)
                return fail(d, i, "CGEN", "call to %s has %zu arguments; declaration requires %zu",
                            callee->name, arg_count, callee->param_count);
            for (k = 0; k < arg_count; ++k) {
                const char *t;
                R6Value *v = (R6Value *)&arg_values[k]->operands[0];
                if (!value_type(f, v, &t) || !same_type(t, callee->param_types[k]))
                    return fail(d, i, "CGEN", "call argument %zu type does not match %s", k, callee->name);
            }
            if (callee->return_type && strcmp(callee->return_type, "void") == 0) {
                if (i->has_dest) return fail(d, i, "CGEN", "void call cannot have a destination");
            } else if (!i->has_dest || !same_type(type_at(f, i->dest), callee->return_type))
                return fail(d, i, "CGEN", "call destination does not match return type of %s", callee->name);
            arg_count = 0;
            break;
        }
        case R6_OP_RET:
            if (strcmp(f->return_type, "void") == 0) {
                if (i->operands[0].kind != R6_VAL_NONE)
                    return fail(d, i, "CGEN", "void ret must not carry a value");
            } else {
                if (!i->operands[0].kind || i->operands[0].kind == R6_VAL_NONE ||
                    !value_type(f, &i->operands[0], &a) || !same_type(a, f->return_type))
                    return fail(d, i, "CGEN", "ret value does not match function return type");
                if (i->operands[0].kind == R6_VAL_REG && !type_at(f, i->operands[0].as.i32))
                    return fail(d, i, "CGEN", "ret register is out of range");
            }
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
    fprintf(o, ".LCr6_%llu:\n\t.byte ", id);
    while (*p) {
        fprintf(o, "%u,", (unsigned)*p++);
    }
    fputs("0\n\t.text\n", o);
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
            else if (v->kind == R6_VAL_INT) fprintf(o, "\tmovl $%d, %%eax\n", v->as.i32);
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
        else if (v->kind == R6_VAL_INT) fprintf(o, "\tmovl $%d, %s\n", v->as.i32, index == 0 ? "%ecx" : index == 1 ? "%edx" : index == 2 ? "%r8d" : "%r9d");
        else fprintf(o, "\tmovl $%d, %s\n", v->as.boolean ? 1 : 0, index == 0 ? "%ecx" : index == 1 ? "%edx" : index == 2 ? "%r8d" : "%r9d");
    }
}

static bool emit_function(FILE *o, const R6Module *m, const R6Function *f,
                          size_t fn_index, R6Diagnostic *d) {
    size_t pc, pending = 0;
    const R6Instruction *args[256];
    unsigned long long arg_string_ids[256];
    unsigned long long string_id = ((unsigned long long)fn_index) << 32;
    bool is_main = strcmp(f->name, "main") == 0;
    const char *native_name = is_main ? "orbit_main" : f->name;
    size_t frame = ((size_t)f->register_count * 8 + (is_main ? 8 : 0) +
                    f->param_count * 8 + 32 + 15) & ~(size_t)15;
    if (f->is_extern) return true;
    fprintf(o, "\t.text\n\t.globl %s\n\t.def %s; .scl 2; .type 32; .endef\n", native_name, native_name);
    fprintf(o, "\t.seh_proc %s\n%s:\n\tpushq %%rbp\n\t.seh_pushreg %%rbp\n\tmovq %%rsp, %%rbp\n\t.seh_setframe %%rbp, 0\n", native_name, native_name);
    if (frame) fprintf(o, "\tsubq $%zu, %%rsp\n\t.seh_stackalloc %zu\n", frame, frame);
    fputs("\t.seh_endprologue\n", o);
    if (is_main) fprintf(o, "\tmovq %%rcx, -%u(%%rbp)\n", (unsigned)(f->register_count + 1) * 8);
    for (pc = 0; pc < f->param_count; ++pc) {
        const char *t = f->param_types[pc];
        size_t abi_index = pc + (is_main ? 1 : 0);
        unsigned home = (unsigned)(f->register_count + (is_main ? 1 : 0) + pc + 1) * 8;
        if (is_float(t)) fprintf(o, "\tmovsd %%xmm%zu, -%u(%%rbp)\n", abi_index, home);
        else fprintf(o, "\tmovq %s, -%u(%%rbp)\n", abi_index == 0 ? "%rcx" : abi_index == 1 ? "%rdx" : abi_index == 2 ? "%r8" : "%r9", home);
    }
    for (pc = 0; pc < f->instruction_count; ++pc) {
        const R6Instruction *i = &f->instructions[pc];
        const R6Value *v = &i->operands[0];
        const char *t;
        unsigned off = i->has_dest ? (unsigned)(i->dest + 1) * 8 : 0;
        switch (i->opcode) {
        case R6_OP_LOAD_CONST:
            if (v->kind == R6_VAL_STRING) {
                emit_string(o, v->as.string, string_id);
                fprintf(o, "\tleaq .LCr6_%llu(%%rip), %%rax\n\tmovq %%rax, -%u(%%rbp)\n", string_id++, off);
            } else if (v->kind == R6_VAL_FLOAT64) {
                fprintf(o, "\tmovabsq $0x%016llx, %%rax\n\tmovq %%rax, -%u(%%rbp)\n", (unsigned long long)v->as.f64_bits, off);
            } else if (v->kind == R6_VAL_INT) fprintf(o, "\tmovq $%d, -%u(%%rbp)\n", v->as.i32, off);
            else fprintf(o, "\tmovl $%d, -%u(%%rbp)\n", v->as.boolean ? 1 : 0, off);
            break;
        case R6_OP_COPY:
            if (is_float(type_at(f, i->dest)) && i->operands[0].kind == R6_VAL_REG)
                fprintf(o, "\tmovsd -%u(%%rbp), %%xmm0\n\tmovsd %%xmm0, -%u(%%rbp)\n",
                        (unsigned)(i->operands[0].as.i32 + 1) * 8, off);
            else if (i->operands[0].kind == R6_VAL_REG)
                fprintf(o, "\tmovq -%u(%%rbp), %%rax\n\tmovq %%rax, -%u(%%rbp)\n",
                        (unsigned)(i->operands[0].as.i32 + 1) * 8, off);
            else if (i->operands[0].kind == R6_VAL_BOOL)
                fprintf(o, "\tmovl $%u, -%u(%%rbp)\n", i->operands[0].as.boolean ? 1u : 0u, off);
            else if (i->operands[0].kind == R6_VAL_INT)
                fprintf(o, "\tmovq $%d, -%u(%%rbp)\n", i->operands[0].as.i32, off);
            else if (i->operands[0].kind == R6_VAL_FLOAT64)
                fprintf(o, "\tmovabsq $0x%016llx, %%rax\n\tmovq %%rax, -%u(%%rbp)\n",
                        (unsigned long long)i->operands[0].as.f64_bits, off);
            else
                return fail(d, i, "CGEN", "copy from this scalar literal kind is unsupported");
            break;
        case R6_OP_ADD:
            t = type_at(f, i->dest);
            if (is_string(t)) {
                fprintf(o, "\tmovq -%u(%%rbp), %%rcx\n\tmovq -%u(%%rbp), %%rdx\n\tmovq -%u(%%rbp), %%r8\n\tcall orbit_string_concat\n\tmovq %%rax, -%u(%%rbp)\n",
                        (unsigned)(f->register_count + 1) * 8,
                        (unsigned)(i->operands[0].as.i32 + 1) * 8,
                        (unsigned)(i->operands[1].as.i32 + 1) * 8, off);
                break;
            }
            if (is_float(t)) {
                fprintf(o, "\tmovsd -%u(%%rbp), %%xmm0\n\taddsd -%u(%%rbp), %%xmm0\n\tmovsd %%xmm0, -%u(%%rbp)\n",
                        (unsigned)(i->operands[0].as.i32 + 1) * 8,
                        (unsigned)(i->operands[1].as.i32 + 1) * 8, off);
            } else fprintf(o, "\tmovq -%u(%%rbp), %%rax\n\taddq -%u(%%rbp), %%rax\n\tmovq %%rax, -%u(%%rbp)\n",
                           (unsigned)(i->operands[0].as.i32 + 1) * 8,
                           (unsigned)(i->operands[1].as.i32 + 1) * 8, off);
            break;
        case R6_OP_SUB: case R6_OP_MUL: case R6_OP_DIV: case R6_OP_MOD: {
            unsigned aoff = (unsigned)(i->operands[0].as.i32 + 1) * 8;
            unsigned boff = (unsigned)(i->operands[1].as.i32 + 1) * 8;
            if (is_float(type_at(f, i->operands[0].as.i32))) {
                const char *ins = i->opcode == R6_OP_SUB ? "subsd" :
                                  i->opcode == R6_OP_MUL ? "mulsd" : "divsd";
                fprintf(o, "\tmovsd -%u(%%rbp), %%xmm0\n\t%s -%u(%%rbp), %%xmm0\n\tmovsd %%xmm0, -%u(%%rbp)\n",
                        aoff, ins, boff, off);
            } else if (i->opcode == R6_OP_DIV || i->opcode == R6_OP_MOD) {
                fprintf(o, "\tmovl -%u(%%rbp), %%eax\n\tcltd\n\tidivl -%u(%%rbp)\n", aoff, boff);
                fprintf(o, "\tmovslq %s, %s\n\tmovq %s, -%u(%%rbp)\n",
                        i->opcode == R6_OP_DIV ? "%eax" : "%edx",
                        i->opcode == R6_OP_DIV ? "%rax" : "%rdx",
                        i->opcode == R6_OP_DIV ? "%rax" : "%rdx", off);
            } else {
                fprintf(o, "\tmovq -%u(%%rbp), %%rax\n", aoff);
                fprintf(o, "\t%s -%u(%%rbp), %%rax\n", i->opcode == R6_OP_SUB ? "subq" : "imulq", boff);
                fprintf(o, "\tmovq %%rax, -%u(%%rbp)\n", off);
            }
            break;
        }
        case R6_OP_EQ:
            t = type_at(f, i->operands[0].as.i32);
            if (is_float(t)) {
                fprintf(o, "\tmovsd -%u(%%rbp), %%xmm0\n\tucomisd -%u(%%rbp), %%xmm0\n\tsete %%al\n\tsetnp %%dl\n\tandb %%dl, %%al\n\tmovzbl %%al, %%eax\n\tmovl %%eax, -%u(%%rbp)\n",
                        (unsigned)(i->operands[0].as.i32 + 1) * 8,
                        (unsigned)(i->operands[1].as.i32 + 1) * 8, off);
            } else if (is_int(t)) fprintf(o, "\tmovq -%u(%%rbp), %%rax\n\tcmpq -%u(%%rbp), %%rax\n\tsete %%al\n\tmovzbl %%al, %%eax\n\tmovl %%eax, -%u(%%rbp)\n",
                           (unsigned)(i->operands[0].as.i32 + 1) * 8,
                           (unsigned)(i->operands[1].as.i32 + 1) * 8, off);
            else fprintf(o, "\tmovl -%u(%%rbp), %%eax\n\tcmpl -%u(%%rbp), %%eax\n\tsete %%al\n\tmovzbl %%al, %%eax\n\tmovl %%eax, -%u(%%rbp)\n",
                         (unsigned)(i->operands[0].as.i32 + 1) * 8,
                         (unsigned)(i->operands[1].as.i32 + 1) * 8, off);
            break;
        case R6_OP_NE: case R6_OP_LT: case R6_OP_LE: case R6_OP_GT: case R6_OP_GE: {
            const char *setcc;
            unsigned aoff = (unsigned)(i->operands[0].as.i32 + 1) * 8;
            unsigned boff = (unsigned)(i->operands[1].as.i32 + 1) * 8;
            if (is_float(type_at(f, i->operands[0].as.i32))) {
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
            } else {
                setcc = i->opcode == R6_OP_NE ? "setne" :
                        i->opcode == R6_OP_LT ? "setl" :
                        i->opcode == R6_OP_LE ? "setle" :
                        i->opcode == R6_OP_GT ? "setg" : "setge";
                fprintf(o, "\tmovq -%u(%%rbp), %%rax\n\tcmpq -%u(%%rbp), %%rax\n\t%s %%al\n\tmovzbl %%al, %%eax\n\tmovl %%eax, -%u(%%rbp)\n",
                        aoff, boff, setcc, off);
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
                fprintf(o, "\tmovq -%u(%%rbp), %%rax\n\tnegq %%rax\n\tmovq %%rax, -%u(%%rbp)\n",
                        (unsigned)(i->operands[0].as.i32 + 1) * 8, off);
            break;
        case R6_OP_JUMP_IF_FALSE:
            fprintf(o, "\tcmpl $0, -%u(%%rbp)\n\tje .Lr6_f%zu_l%d\n",
                    (unsigned)(v->as.i32 + 1) * 8, fn_index,
                    i->operands[1].as.i32);
            break;
        case R6_OP_JUMP:
            fprintf(o, "\tjmp .Lr6_f%zu_l%d\n", fn_index, i->operands[0].as.i32);
            break;
        case R6_OP_LABEL:
            fprintf(o, ".Lr6_f%zu_l%d:\n", fn_index, v->as.i32);
            break;
        case R6_OP_ARG:
            if (i->has_dest) {
                int idx = v->as.i32;
                unsigned home = (unsigned)(f->register_count + (is_main ? 1 : 0) + idx + 1) * 8;
                if (is_float(f->param_types[idx]))
                    fprintf(o, "\tmovsd -%u(%%rbp), %%xmm15\n\tmovsd %%xmm15, -%u(%%rbp)\n", home, off);
                else fprintf(o, "\tmovq -%u(%%rbp), %%rax\n\tmovq %%rax, -%u(%%rbp)\n", home, off);
            } else { args[pending] = i; arg_string_ids[pending++] = ULLONG_MAX; }
            break;
        case R6_OP_CALL: {
            const R6Function *callee = find_function(m, i->operands[0].as.string);
            size_t k;
            for (k = 0; k < pending; ++k) {
                const R6Value *av = &args[k]->operands[0];
                const char *at; value_type(f, av, &at);
                if (av->kind == R6_VAL_STRING) {
                    arg_string_ids[k] = string_id;
                    emit_string(o, av->as.string, string_id++);
                }
            }
            for (k = 0; k < pending; ++k) {
                const R6Value *av = &args[k]->operands[0];
                const char *at; value_type(f, av, &at);
                emit_load_arg(o, f, av, at, k, arg_string_ids[k] == ULLONG_MAX ? 0 : arg_string_ids[k]);
            }
            fprintf(o, "\tcall %s\n", strcmp(callee->name, "main") == 0 ? "orbit_main" : callee->name);
            if (i->has_dest) {
                if (is_float(type_at(f, i->dest))) fprintf(o, "\tmovsd %%xmm0, -%u(%%rbp)\n", off);
                else fprintf(o, "\tmovq %%rax, -%u(%%rbp)\n", off);
            }
            pending = 0;
            break;
        }
        case R6_OP_RET:
            if (v->kind != R6_VAL_NONE) {
                value_type(f, v, &t);
                if (v->kind == R6_VAL_REG && is_float(t)) fprintf(o, "\tmovsd -%u(%%rbp), %%xmm0\n", (unsigned)(v->as.i32 + 1) * 8);
                else if (v->kind == R6_VAL_REG) fprintf(o, "\tmovq -%u(%%rbp), %%rax\n", (unsigned)(v->as.i32 + 1) * 8);
                else if (v->kind == R6_VAL_INT) fprintf(o, "\tmovl $%d, %%eax\n", v->as.i32);
                else if (v->kind == R6_VAL_BOOL) fprintf(o, "\tmovl $%d, %%eax\n", v->as.boolean ? 1 : 0);
                else if (v->kind == R6_VAL_FLOAT64) fprintf(o, "\tmovabsq $0x%016llx, %%rax\n\tmovq %%rax, %%xmm0\n", (unsigned long long)v->as.f64_bits);
                else return fail(d, i, "CGEN", "returning string literals is unsupported");
            }
            fputs("\tleave\n\tret\n", o);
            break;
        default:
            return fail(d, i, "CGEN", "unsupported opcode %s", r6_opcode_name(i->opcode));
        }
    }
    fprintf(o, "\t.seh_endproc\n");
    return !ferror(o);
}

static void emit_entry_wrapper(FILE *o) {
    fputs("\t.text\n\t.globl main\n"
          "\t.def main; .scl 2; .type 32; .endef\n"
          "\t.seh_proc main\nmain:\n"
          "\tpushq %rbp\n\t.seh_pushreg %rbp\n"
          "\tmovq %rsp, %rbp\n\t.seh_setframe %rbp, 0\n"
          "\tsubq $48, %rsp\n\t.seh_stackalloc 48\n"
          "\t.seh_endprologue\n"
          "\tmovl $131072, %ecx\n\tcall orbit_arena_create\n"
          "\tmovq %rax, -8(%rbp)\n"
          "\tmovq %rax, %rcx\n\tcall orbit_main\n"
          "\tmovl %eax, -16(%rbp)\n"
          "\tmovq -8(%rbp), %rcx\n\tcall orbit_arena_destroy\n"
          "\tmovl -16(%rbp), %eax\n\tleave\n\tret\n"
          "\t.seh_endproc\n", o);
}

bool r6_codegen_win64(FILE *out, const R6Module *module, R6Diagnostic *diag) {
    size_t n;
    bool main_found = false;
    if (diag) memset(diag, 0, sizeof *diag);
    if (!out || !module) return fail(diag, NULL, "CGEN", "null output or module");
    if (module->function_count && !module->functions)
        return fail(diag, NULL, "CGEN", "module has missing function storage");
    if (module->global_count || module->model_count || module->type_count || module->migration_count)
        return fail(diag, NULL, "CGEN", "M1 codegen does not support globals, models, types, or migrations");
    for (n = 0; n < module->function_count; ++n) {
        const R6Function *f = &module->functions[n];
        size_t k;
        if (!f->name) return fail(diag, NULL, "CGEN", "function has null name");
        if (strcmp(f->name, "main") == 0 && !f->is_extern) main_found = true;
        for (k = 0; k < n; ++k)
            if (strcmp(module->functions[k].name, f->name) == 0)
                return fail(diag, NULL, "CGEN", "duplicate function %s", f->name);
        if (f->register_count < 0 || (size_t)f->register_count != f->register_type_count)
            return fail(diag, NULL, "CGEN", "function %s has inconsistent register types", f->name);
        if (f->register_type_count && !f->register_types)
            return fail(diag, NULL, "CGEN", "function %s has missing register type metadata", f->name);
        for (k = 0; k < f->register_type_count; ++k)
            if (!is_scalar(f->register_types[k]))
                return fail(diag, NULL, "CGEN", "register type %s is unsupported in M1", f->register_types[k]);
        if (!validate_function(module, f, diag)) return false;
    }
    if (!main_found) return fail(diag, NULL, "CGEN", "module must define one non-extern main");
    for (n = 0; n < module->function_count; ++n)
        if (!emit_function(out, module, &module->functions[n], n, diag)) return false;
    emit_entry_wrapper(out);
    return !ferror(out);
}
