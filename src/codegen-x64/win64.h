#ifndef R6_CODEGEN_WIN64_H
#define R6_CODEGEN_WIN64_H

#include "../loader/ir.h"

#include <stdio.h>

/* Emit GNU assembler x86-64 Windows ABI text. Returns false and fills diag
 * for every unsupported operation or invalid codegen-level combination.
 *
 * The module is mutable: D-M1-20 recovers register types the builder
 * serialized as `unknown` (infer_register_types), in place, before anything
 * else reads them. */
bool r6_codegen_win64(FILE *out, R6Module *module, R6Diagnostic *diag);

/* Start-up decisions of docs/STARTUP.md in the pin (D-M1-102): the module reads
 * argv, or links the network modules (a database program does). */
bool r6_module_uses_args(const R6Module *module);
bool r6_module_uses_network(const R6Module *module);

/* D-M1-104: the module gets R6's own process entry `orbit_start` and is
 * linked without the MinGW start-up objects. */
bool r6_module_native_entry(const R6Module *module);

#endif
