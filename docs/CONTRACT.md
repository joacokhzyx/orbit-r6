# R6 internal contract

Date: 2026-10-14, restated 2026-10-31. Language: the pin in
[`PIN.md`](PIN.md). Accepted IR: IRv1 from `orbit-lang/docs/IR_SPEC.md` at the
pinned commit (D-F0-06; the adopted differences are in
[Spec alignment](#spec-alignment)). This file fixes the interfaces between
loader, codegen, driver and the differential. It does not widen semantics.

## Loader representation (`src/loader/ir.h`)

- `R6Module` keeps functions, globals, models, types and migrations in order.
- Each `R6Function` keeps its name, parameters and types, instructions,
  register count and types, return, extern flag and route.
- Each `R6Instruction` keeps the opcode enum, an optional `dest`, three tagged
  `R6Value`s and `elem_kind`. Numeric types are `int32_t` for register and
  label indices and `int64_t` for literals during range validation; floats are
  stored as `uint64_t` binary64 bits; strings are validated UTF-8 bytes,
  NUL-terminated only when materialized as C strings for the ABI.
- The loader rejects extra or duplicate keys, unknown tags, invalid fields,
  length or type mismatches, invalid UTF-8, out-of-range registers,
  unsupported opcodes and any version other than 1. It does not normalize or
  reorder.
- `R6Module` owns its allocations; `r6_module_free` releases them recursively.
  Parsing JSON into this model runs no callbacks and has no runtime effect.

## Codegen

### Target

The target is x86-64 Win64 or System V, chosen by the host when the driver is
compiled (`src/codegen-x64/target.h` derives `R6_TARGET_WIN64` from `_WIN32`).
One binary emits one target: there is no `--target` and no cross build. The IR,
opcodes and diagnostic codes are identical on both; the object writer and the
linker inputs differ (D-M1-119).

The lowering covers these opcodes, each only for the combinations validated in
codegen:

```text
load_const add sub mul div mod eq ne lt le gt ge jump_if_false label arg
and_op or_op not_op neg copy jump load_var decl_var store_var list_create
list_push list_len list_get call ret
```

An unimplemented opcode produces an error naming it and the instruction.

### Emission

The codegen lowers IR to a temporary internal representation. `asm_win64.c`
encodes x86-64 bytes and the object writer is `coff.c` (AMD64 COFF) on Win64 or
`elf64.c` (ELF64 ET_REL) on System V. It neither generates nor parses C, and it
calls neither GCC nor GAS while compiling a program. GNU `ld` resolves runtime
and CRT symbols. Every IR register lives in an 8-byte stack slot. A jump reads
the flags of the previous comparison (D-M1-66). `peephole.c` drops the reload
of a slot just stored (D-M1-67). `regalloc.c` moves the hottest slots of each
function into registers that survive a call. In Win64 those are seven integer
registers (rbx, rsi, rdi and r12-r15) and nine vector registers. In System V
they are five integer registers (rbx and r12-r15) and no vector register
(D-M1-68, D-M1-119).

`peephole.c` uses per-target use and definition sets for `call`. In System V a
call uses the whole integer argument bank (`rdi`, `rsi`, `rdx`, `rcx`, `r8`,
`r9`) and `xmm0`-`xmm7`. It defines every volatile register and every vector
register. In Win64 it uses the four positional slots and `xmm0`-`xmm3`. That set
preserves argument setup in System V (D-M1-119).

### Values and types

`load_const` lowers `Int`, `String`, `Bool` and `Float` literals and function
values (a `Symbol` naming a module function, D-M1-14). A `Symbol` that is
`true`/`false`, or a numeric text with a fraction or exponent, is the
corresponding bool or float literal (spec §2.1; D-M1-19) and must match the
destination register's type. A non-finite numeric text fails (V-08), and a bare
integer is not a float literal.

Registers typed `unknown`: the builder serializes before `inferRegisterTypes`
(P1), so a register the builder did not type arrives as the literal text
`unknown`. Before validating the module, `r6_codegen_win64` (signature
`R6Module *`; it rewrites `register_types` in place while the loader keeps
ownership) recovers those registers from their defining instruction, mirroring
the `inferRegisterTypes` rules (`c_backend.orb:674`) for the supported opcodes,
in three passes. It replaces only `unknown`, never a declared type, and a
register that no rule explains stays rejected with the same `CGEN`. Decision
D-M1-20.

Integer semantics mirror the pinned backend: `runtime/orbit_api.h:114` declares
`typedef int64_t orbit_int`; `c_backend.orb` lowers `add`, `sub` and `mul`
through `uintptr_t`, `div` and `mod` through signed `orbit_int`, and relational
comparisons through `intptr_t`. R6 reflects those conversions for the measured
subset; cases outside the differential are not generalized.

`sub`, `mul` and `div` also accept binary64 registers and use SSE2; fixture
`09-float-arithmetic` compares exact representable operations against the
reference compiler. The evidence does not extend to NaN or infinity edges.
`ne`, `lt`, `le`, `gt` and `ge` accept int or float. For float,
`10-float-compare-nan` verifies all six comparison predicates with NaN and
signed zero against the pin; `ne` combines `setne` with parity (`setp`), and
`lt`/`le` mask the unordered flag of `ucomisd`. `eq` and `ne` also accept
strings and call the CRT `strcmp`; the pin emits `strcmp(a,b)==0/!=0` for
strings (`c_backend.orb:2552-2566`). Relational string comparisons stay
rejected.

`neg` accepts int (modular negation in the native slot) and float (flips the
sign bit with XOR, preserving the payload); `11-neg` compares finite values with
GCC. Integer overflow is not covered by that fixture.

`copy` moves scalars to slots, and `jump` resolves labels local to a function;
`12-jump-copy` confirms unconditional jump and copy, and `13-while-counter`
verifies a back-edge, a conditional branch and a local int variable against the
pin. `decl_var` and `store_var` support scalar locals and `list` references
declared in the function, initialized or updated from registers or an inline
literal (`val n = 7`, `val s = "x"`, `val b = true`; D-M1-19). A `Symbol`
initializer binds a function value (D-M1-14). `load_var` also reads literal
int/bool globals: the pin serializes those `IRGlobal.value` as decimal String
(`builder.orb:2288-2304,2333-2338`; `c_backend.orb:1481-1505`). Other globals,
parameters by name and captures fail explicitly.

### Lists

The measured subset supports `list<int>` in `main` using `list_create` (8-byte
slots, `elem_kind=2`), int `list_push` and `list_len`. Creation and push call
the pinned runtime's `orbit_list_create_typed` and `orbit_list_push_int`. The C
return `OrbitResult` uses sret; `ok` is at byte 0 and `value` at byte 16 per the
header layout (`runtime/orbit_api.h:179-184`). Length reads `OrbitList.len`
(offset 8; `orbit_api.h:217-224,517-519`), like the official inline accessor.
`list_get` admits int in `main` only; it checks null, unsigned bounds and kind,
and calls the official `orbit_list_index_error` or `orbit_list_kind_error` on
failure (`orbit_api.h:358-360,422-468`). Other kinds, set/pop, helpers outside
`main`, and a list as argument or return fail closed.

### Calls and frames

IR registers are typed stack slots; physical register assignment is not
promised. `arg` arguments queue until `call` and drain at every call; parameter
binding is lowered in the function prologue. Up to 256 scalar parameters are
admitted. Win64 places positions 0-3 in `rcx`, `rdx`, `r8` and `r9` (a float
occupies one of those slots) and the rest in 8-byte slots from `[rsp+32]`. The
callee receives them from `[rbp+48]`, and the caller reserves 32 bytes of shadow
space. System V fills the integer bank `rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9`
and the vector bank `xmm0`-`xmm7` separately, and spills an argument only when
its bank is exhausted. There is no shadow space, and the callee reads stack
arguments from `[rbp+16]`. The outgoing area is reserved per function. The frame
is `(used + shadow + stack + 15) & ~15` on both targets, and `chkstk` is emitted
only in Win64. Direct calls by name (target `String` or `Sym`, resolved to a
declared function, model constructor or union variant, D-M1-18) and indirect
calls through an `fn(...)` register are admitted (D-M1-14); a void callee may
carry a declared `void` destination.

External ABI is the target's C ABI. Win64: the first four arguments use
positional `RCX`/`RDX`/`R8`/`R9` or `XMM0`-`XMM3` according to type. The caller
reserves 32 bytes of shadow space and keeps stack alignment. An integer return
is in `RAX` and a float in `XMM0`. Seven integer and nine vector registers
survive a call. System V: the integer bank `rdi`/`rsi`/`rdx`/`rcx`/`r8`/`r9`
and the vector bank `xmm0`-`xmm7` fill separately; there is no shadow space;
five integer registers (`rbx`, `r12`-`r15`) survive and no vector register; an
integer return is in `RAX` and a float in `XMM0`. `orbit_print` is
`void(const char*)` on both. Win64 links the net-only runtime plus `-lws2_32`.

### Link inputs

Win64: `ld -m i386pep -Bdynamic --stack 16777216`, the module object,
`--start-group libruntime_net.a [sqlite3.lib] [-lws2_32] -lmingw32 -lgcc
-lmingwex -lmsvcrt -lkernel32 -ladvapi32 --end-group` and, depending on the
entry, `crt2.o`/`crtbegin.o`/`crtend.o` or `msvcrt.dll` (D-M1-104). It does not
include `user32`, `shell32` or `pthread`: no runtime member or pinned `extern fn`
references one of their symbols, and each library costs about 1 ms per link
(D-M1-122).

Linux: `ld` receives `-dynamic-linker` with `/lib64/ld-linux-x86-64.so.2` (or
`/lib/ld-linux-x86-64.so.2`), then `crt1.o`, `crti.o`, `crtbegin.o`, the module
object, `--as-needed`, `--start-group libruntime_net.a [-lsqlite3] --end-group`,
`-lc -lm -ldl -lpthread`, `crtend.o` and `crtn.o`. `--as-needed` keeps a
library out of `DT_NEEDED` when no symbol comes from it; symbols and program
output do not change (D-M1-121). `R6_CRT_DIR` replaces the start-up directory
and `R6_LD` the linker; `crtbegin.o` and `crtend.o` are searched under
`/usr/lib/gcc/x86_64-linux-gnu/*/` when the glibc directory does not hold them.
The default runtime directory is `.r6-build/runtime-linux-x64` (D-M1-119).

### Entry point

Linux always keeps `main` and the glibc start-up. It does not use R6's own
`orbit_start` because that calls msvcrt's `__set_app_type` and the driver's
start-up check reads a PE header. Win64 uses `orbit_start` (D-M1-104) when the
module has no argv, network, database or migrations, and `main` otherwise. A
module that keeps `main` emits it as C `main`, and its int32 return determines
the exit status. A missing or duplicate `main` is not supported.

## Driver and diagnostics

- CLI: `r6 [build] <input.ir> -o <output> [--runtime-dir <dir>] [--timings]`.
  The output is a PE `.exe` in Win64 and an ELF executable in System V. The
  runtime directory is `--runtime-dir`, then `R6_RUNTIME_DIR`, then
  `.r6-build/runtime-win64` on Windows or `.r6-build/runtime-linux-x64` on
  Linux. The driver writes temporaries only under its local build or temp
  directory, invokes GNU `ld`, and deletes them on success; it keeps the output
  only when the link succeeded.
- A one-line diagnostic on stderr:
  `r6: <file>:<line>:<col>: error[<code>]: <message>`, with a nonzero exit. JSON
  errors point at the token; semantic and codegen errors point at the
  instruction index and opcode. No fallback, silent truncation or success
  message on stderr.
- The differential harness compiles the `.orb` source with the pin's GCC
  command, runs the GCC and R6 programs with identical stdin, and compares exit
  and stdout as bytes. The program's stderr is captured separately. Any
  difference is a failure.

## Isolation

- `src/loader`: parsing and validation; owns `ir.h`.
- `src/codegen-x64`: emission from `ir.h`; parses no JSON and calls no driver.
- `src/driver`: CLI, temporaries, linker invocation and exit codes.
- `tests/diff`: GCC oracle, execution and exact comparison; it does not change
  fixtures.
- No module creates shims, implements runtime functions or alters
  `orbit-lang`.

Rejected alternative: generating C to delegate lowering (forbidden by the
contract).

## Internal assembler

`win64.c` writes a private GAS-syntax text; `peephole.c` and `regalloc.c`
rewrite it, and `asm_win64.c` encodes it to bytes for `coff.c` (AMD64 COFF) or
`elf64.c` (ELF64 ET_REL). No external assembler reads or emits program
instructions. `asm_win64.c` accepts only the forms the selector emits and
rejects any other instruction, directive or unwind frame; the current list is
the code of `asm_win64.c`, not a document. In Win64 the `.pdata` and `.xdata`
records come from the real offsets of the encoded prologue. The ELF writer
starts from the same object model and emits `.text`, `.rdata` and `.data` by
name, `.note.GNU-stack`, one `.rela.<section>` per section with relocations,
`.symtab` (locals first), `.strtab` and `.shstrtab`. An R6 REL32 is
`R_X86_64_PC32` against a defined symbol and `R_X86_64_PLT32` against an
undefined one, with addend -4; a 32S is `R_X86_64_32S`. The `.seh_*` directives
are parsed and not written: ELF carries no unwind data (D-M1-119).

## Spec alignment

The language publishes `orbit-lang/docs/IR_SPEC.md`; where that document and
the local draft disagreed, the spec prevails, and where the language publishes
something else, the language prevails (D-F0-06). R6 adopts the spec's
serialization exactly and keeps no dual acceptance.

- **Format.** `src/loader/ir.c` accepts `{"irVersion":"1","module":M}`,
  instructions with `opcode`/`dest`/`operand1..3`/`elemKind`, values with
  `kind` and `value` (`{"kind":"None"}` has no payload), `dest` `-1` for no
  destination, flat `route*` fields, and floats as JSON numbers. The draft
  document is rejected with `E_FORMAT` and a migration hint.
- **Loader validations.**
  - V-06 and V-07: JSON integers; a fraction or exponent in an integer context
    is `E_SCHEMA`.
  - V-08: a non-finite value is `E_RANGE`.
  - V-10: `dest` is required by opcode and by a non-void `call`.
  - V-13: arity for O2 is an integer at least 0 and at most the pending
    arguments.
  - V-14: a jump names an existing label.
  - V-16: the route fields are coherent.
  - V-18: an extern body is parsed and ignored.
  - V-01, V-11 and V-12: the elision marker `…[N chars]`, destination and
    register lengths.
- **Fixtures.** The 100 pairs in `tests/diff/fixtures/` use this format, keep
  LF (`.gitattributes`), and compare exit and stdout bytes against the oracle.
  A `call` carries its arity in `operand2`.
- **Codegen consequences.** `call` consumes exactly the last O2 pending
  arguments (E-05) and still requires arity to equal the declared parameters;
  an extern with a body is ignored (E-09); Kynx routes are rejected with
  `E_UNSUPPORTED`, outside the native subset.

## Measured subset

R6 implements strict IRv1 and two targets, Win64 x64 and System V x86-64, one
target per binary (D-M1-119). The admitted opcodes are those in
[Emission](#emission). Calls are direct or indirect and take up to 256 scalar
arguments under the target's convention; every IR slot lives in an 8-byte
frame, and `peephole.c` and `regalloc.c` rewrite the generated text before
encoding.

The pin emits its own corpus with `orbit build --emit-ir`:
`tests/corpus/manifest.json` fixes 144 documents (143 executable and one support
module), 6 992 633 bytes and `filesSha256 a533ace1`, and
`tests/corpus/run_corpus.py --check` requires 143 builds and 143 matches. The
100 pairs in `tests/diff/fixtures/` remain manual differential fixtures; they
record the IR forms and do not depend on the emitter.

Overflow and extreme cases outside the fixtures have no coverage. Fixture 10
confirms IEEE unordered comparison for NaN and `+0.0`/`-0.0` equivalence;
fixture 11 covers finite negation, not every NaN payload. Semantic evidence is
limited to the pinned `c_backend.orb` and these pairs; fixtures 14-18 add
literal int/bool globals, string comparisons, stack arguments and an int list.
Do not extrapolate to every IRv1 edge.

The two measured platforms are Windows x86-64 and Linux x86-64. Linux was
measured in WSL2 Ubuntu with GCC 15.2.0 and GNU ld.bfd. ARM64 macOS is future
work. Threading and benchmarks were not evaluated. There is no cross
compilation: each binary builds the host's target (D-M1-119).
