# Differential fixtures

Each `.json` is an IRv1 document in the language-side format
(`orbit-lang/docs/IR_SPEC.md` at the pin): root `irVersion` and `module`,
operands `{"kind":K,"value":V}`, `dest` `-1` for no destination, and each
`call`'s arity in `operand2` (V-13). Each `.orb` is the behaviourally equivalent
source for the GCC oracle. The pair is compiled and run by
[`../run_diff.py`](../run_diff.py); exit status and stdout bytes must match.
The files are LF by `.gitattributes` (V-01), and `orbit_print` is the runtime
symbol declared by `runtime/orbit_api.h`.

The pairs are hand-written and predate the emitter, which is why they stay
manual. The cases below record the IR forms the native backend accepts and the
output each one produces. Where the expected stdout is long, the oracle decides
and the catalogue says so.

| Case | Family | Exit | Stdout |
|---|---|---:|---|
| `01-int-call` | Integer sum and a runtime call | 42 | `M1-add\n` |
| `02-string` | String concatenation and a runtime call | 0 | `IRv1\n` |
| `03-float-branch` | binary64 add and equality with a branch | 0 | empty |
| `04-branch` | Conditional jump | 7 | empty |
| `05-call-3args` | Three-argument call | 6 | empty |
| `06-signed-arithmetic` | sub/mul/div/mod with negatives; 64-bit `int` (D-M1-94) | 20 | empty |
| `07-signed-compare` | Signed comparison and branch | 3 | empty |
| `08-bool-logic` | and/or/not on booleans | 9 | empty |
| `09-float-arithmetic` | binary64 sub/mul/div with exact results | 10 | empty |
| `10-float-compare-nan` | Six float comparisons with NaN and signed zero | 0 | empty |
| `11-neg` | Integer and binary64 negation | 0 | empty |
| `12-jump-copy` | Unconditional jump and register copy | 3 | empty |
| `13-while-counter` | Loop with a local variable, back-edge and accumulation | 10 | empty |
| `14-global-constants` | Literal int/bool globals, negative literal and branch | 33 | empty |
| `15-string-equality` | String equality and inequality, including concatenation | 5 | empty |
| `16-call-stack-args` | Fifth and sixth integer arguments on the stack | 11 | empty |
| `17-call-stack-float` | binary64 fifth argument and integer sixth argument | 11 | empty |
| `18-int-list-literal-len` | Int list create, push and length | 3 | empty |
| `19-int-list-get` | Indexed int list read | 20 | empty |
| `20-int-list-get-oob` | Out-of-bounds index through a dynamic function | 2 | empty |
| `21-nop-blocks` | `nop`, `begin_block` and `end_block` with no observable effect | 7 | empty |
| `22-result-ops` | Result ok/err/is_ok/unwrap/unwrap_err with float, int and string payloads; `catch` binding | 46 | empty |
| `23-list-set-pop` | `list_set`, `list_pop` and string/int/float/bool kinds outside `main` | 135 | empty |
| `24-model-fields` | Model constructor and int/float/bool/string field load and store | 42 | `seed\nmut\nf-ok\n` |
| `25-union-tag-data` | Unit, single and multi-payload union constructors; tag/data reads; four exhaustive matches | 85 | `u-ok\n` |
| `26-fn-values` | Function values, indirect call, `var` rebinding and extern binding. Retired from the differential in D-F0-13; the IR stays measured by `ok-fn-extern` | 42 | `fn-ok\n` |
| `27-call-string-target` | Call target `String` with void and int destinations | 42 | `string-target\n` |
| `28-sym-literals` | Bool and float literals as `Sym` text in `load_const` | 42 | empty |
| `29-decl-var-literals` | Inline literal initializers in `decl_var` | 42 | `ok\n` |
| `30-inferred-model-fields` | Emitter stream with six `unknown` registers: model constructor, local, field load and add | 7 | empty |
| `31-inferred-unary-neg` | Emitter stream with four `unknown` registers: neg/add chain | 2 | empty |
| `32-inline-literal-operands` | Inline literals in eleven arithmetic and comparison opcodes, plus string concatenation | 42 | `lit-ok\n\n` |
| `33-parameter-by-name` | Emitter stream reading int/string/bool/float parameters by name | 42 | empty |
| `34-string-spreading-add` | One-sided string add with int/float/bool spread, and concatenation in `main` and a callee | 42 | `[spread]\n` |
| `35-list-len-bound` | Indexed loop over a list outside `main`: 64-bit length and per-function label | 42 | empty |
| `36-print-builtin` | `print` with one string, int or float argument and a void destination | 0 | `hello\n42\n1.5\n` |
| `37-arena-extern-call` | Runtime extern with the arena as its first C parameter | 0 | `42\n-7\n` |
| `38-void-ret-value` | `return f()` of a void callee: the `ret` carries the destination and does not read it | 42 | `T\nT\n` |
| `39-string-intrinsics` | `.len()`, `.at()` and `.slice()` without a module declaration | 0 | `5\n90\nbc\n` |
| `40-list-create-opaque` | Empty list with `elemKind 0`; the first typed push adopts the kind | 0 | empty |
| `41-bool-and-word-comparisons` | `bool` in gt/ne/eq, `int` against `string` by words, float in `ne` | 0 | empty |
| `42-list-parameter-spellings` | `list<int>` and `list<string>` parameters, `list<int>` return, `list<list<int>>` parameter | 0 | empty |
| `43-global-constant-type` | Three module constants with `typeName` `unknown` and decimal text | 0 | `32\r\n3\r\n-2\r\n` |
| `44-enum-tag-constant` | Three-variant enum: tag constants, model field, parameter, return and eight comparisons | 0 | `blue\r\nred\r\n2\r\nred\r\n` |
| `45-int-overflow` | 64-bit `int` without 2^31 overflow, including literals above 32 bits | 42 | empty |
| `46-process-argc` | Entry keeps argc/argv: `orbit_os_argc` and `orbit_os_argv` | 42 | `1\r\n` |
| `47-opaque-list-read` | Opaque read typed by its single use, with int and string consumers | 42 | `5\r\n` |
| `48-model-list-field` | Model `list<int>` and `list<string>` fields: constructor, push and replacement | 42 | `bc\r\n` |
| `49-model-parameter` | Model parameter: field reads, visible mutation, fifth stack argument and string field return | 42 | `origin\r\n` |
| `50-literal-args-r8-r9` | Int literals in r8/r9 with a string pointer; REX.R and REX.B preserved | 42 | `65\r\n` |
| `51-c-bool-return` | Runtime predicates returning C `bool` in `al` | 42 | `1\r\n` |
| `52-result-return` | Result int/float/string returns by hidden pointer, `try`, two catches and a fifth stack argument | 42 | `odd\r\nhi r6\r\nsum off\r\n` |
| `53-model-return` | Model returned as a pointer: factory, mutating identity, nested calls and return after five arguments | 42 | `wide\r\nmade\r\n` |
| `54-result-local` | Result locals declared, reassigned and returned as 24-byte cells | 42 | `zero\r\nsmall\r\nbig\r\nnegative\r\n` |
| `55-union-parameter` | Four-variant union parameter: int and string payload matches, wildcard and unit variant | 42 | `shape orbit\r\n` |
| `56-extern-result-list` | Runtime externs returning result and list | 42 | `read failed\r\n0\r\n` |
| `57-bare-return` | int/float/string/bool functions falling off the end return 0, 0.0 or false | 42 | `one\r\n` |
| `58-result-spelling` | The `Result` spelling in return, local and registers | 42 | `neg\r\n` |
| `59-opaque-compare` | Opaque reads compared with int literals and registers | 42 | `3\r\n` |
| `60-int-neg-wrap` | 64-bit negation: `-INT64_MIN`, `INT64_MAX + 1` and 2^31 as an ordinary value | 42 | `-9223372036854775808\r\n` |
| `61-union-pack-arm` | Multi-payload match arms: the data word is the pack pointer, each field read is typed | 42 | `pack\r\n` |
| `62-union-named-payload` | Payloads that name a model or union: one pointer word in data and pack | 42 | `named\r\n` |
| `63-union-return` | Functions returning a union: the cell returns in `rax`, is rebuilt and passed | 42 | `shapes\r\n` |
| `64-model-word-fields` | Model fields that are a model or union: one pointer word, built, read and rewritten | 42 | `ab\r\n` |
| `65-ref-lists` | Lists of references (`elemKind 5`): models and unions added, read, replaced and removed | 42 | `a\r\n` |
| `66-opaque-lists` | Untyped lists (`elemKind 0`): int, string and list words pushed, replaced and read | 42 | `second\r\n` |
| `67-null-words` | `NULL` is the zero word; `eq`/`ne` between models compares pointers | 42 | `null\r\n` |
| `68-string-index` | `indexOf` and `find` on strings, including the not-found result | 42 | `found\r\n` |
| `69-void-main` | `main` with no return value exits 0 | 0 | `void main\r\n` |
| `70-string-length-field` | `s.length` is a `load_field` giving `strlen`, or 0 for null | 42 | `length\r\n` |
| `71-dead-functions` | Only functions reachable from `main` are emitted; three dead functions are validated and skipped | 42 | `live\r\n` |
| `72-compare-branch` | A jump tests the previous comparison flags instead of reloading the result | 42 | `599\r\n` |
| `73-register-slots` | Hottest slots in callee-saved registers, with recursion, nested loops and mixed types | 42 | long; the oracle decides |
| `74-flags-only-compare` | A comparison read only by the next jump is not stored | 42 | `15139\r\n` |
| `75-string-compare-branch` | A string `==`/`!=` read only by the next jump tests the `strcmp` result | 42 | `135\r\n` |
| `76-string-equality-fast-path` | String equality compares the first byte and the pointers before `strcmp` | 42 | `97\r\n` |
| `77-int-arg-float-param` | An int argument enters a float parameter with the C cast conversion | 42 | `1.5\r\n3.5\r\n8.0\r\n-2.5\r\n22.5\r\n27.0\r\n` |
| `78-union-wide-pack` | Three- and four-payload union variants: padding, nested union payload, five payloads rejected | 42 | long; the oracle decides |
| `79-result-parameter` | A result argument passed by copy in a register and on the stack | 42 | long; the oracle decides |
| `80-result-branch-value` | An `if` used as a value copies the result of the branch that ran | 42 | long; the oracle decides |
| `81-float-list-int-push` | An int pushed to `list<float>` converts; bool and string stay rejected | 42 | `4\r\n1.0\r\n7.0\r\n2.5\r\n14.0\r\n` |
| `82-json-object` | Parsed JSON object bound by `try`, passed, returned and read by getters | 42 | `api\r\n42\r\n1.5\r\n1\r\n7\r\n6\r\nb\r\n9\r\n0\r\n` |
| `83-stack-probe` | A 32 000-byte frame called eight levels deep; `___chkstk_ms` probes the pages | 8 | empty |
| `84-float-registers` | Six float arguments, twelve live floats, recursive calls, NaN comparison and negation | 42 | long; the oracle decides |
| `85-div-by-constant` | `/` and `%` by 40 constants against parameter division, at 32-bit edges | 42 | `0\r\n2593896487739827721\r\n-1507943223\r\n1507943225\r\n-25\r\n` |
| `86-register-cleanup` | Register cleanup across a second compare, loop return, break, continue, clamp and Collatz | 42 | long; the oracle decides |
| `87-float-cleanup` | Destination equals right operand, swap, negative zero, `1e308 * 10`, denormal division | 42 | long; the oracle decides |
| `88-bitwise-ops` | and/or/xor/not on 64-bit words; shift counts masked with 63; literal and register operands | 42 | long; the oracle decides |
| `89-object-literals` | Object literal create and setters, typed getters, nesting, a list inside an object | 42 | long; the oracle decides |
| `90-db-link` | `all`, `where` and `create` link the DB runtime; database opening fails on both sides | 42 | `2\r\n2\r\n0\r\n2\r\n` |
| `91-result-list` | `orbit_random_bytes` returns a result whose value is a list | 42 | `16\r\n16\r\n0\r\n1\r\n` |
| `92-opaque-local` | Untyped list element bound with `val` and read as int | 42 | `64\r\n1\r\n` |
| `93-nested-lists` | `list<list<int>>` and `list<list<list<int>>>` reads with an untyped intermediate | 42 | `3\r\n` |
| `94-word-chain` | Sum of three untyped elements through a `void*` intermediate, then `print` | 42 | `8\r\n` |
| `95-db-migrations` | Model, two migration lines and a query; both sides exit 1 without a database | 1 | empty |
| `96-int-copy-to-float` | An int copied into a float variable converts | 42 | `3.5` |
| `97-div-zero` | Division by zero prints 42, then the runtime message on stderr, and exits 2 | 2 | `42\r\n` |
| `98-div-overflow` | `INT64_MIN` division and modulo edges; `INT64_MIN % -1` reports overflow | 2 | `-4611686018427387904\r\n-2\r\n-9223372036854775807\r\n0\r\n` |
| `99-div-by-constant-64` | `/` and `%` by 44 64-bit constants against parameter division | 42 | `0\r\n-7835353680425529040\r\n-6476564002640978153\r\n6476564002640978155\r\n-25\r\n` |
| `100-int64-wrap` | 64-bit wrap on `+`, `-`, `*` and unary `-`; shifts masked with 63; 3^40 | 42 | 24 lines; the oracle decides |

`map_*` has no fixture (D-M1-10): the builder produces none of the six opcodes,
the lowerings call the runtime with the wrong arity, and every live map value
depends on the arena prepend gap. All six are pinned as `E_UNSUPPORTED`
rejections in `tests/reject/` (cases `x-map-*`).

`alloc` and `free` also have no fixture (D-M1-13): neither has a producer in
the builder, the spec leaves `free` `UNSPECIFIED`, and `alloc` returns a raw
pointer no admitted consumer reads. Both are pinned as `E_UNSUPPORTED`
rejections (`x-alloc-opcode`, `x-free-opcode`).

The measured families have their own borders pinned in `tests/reject/`:
models (D-M1-11), unions (D-M1-12), function values (D-M1-14), literal globals
(D-M1-15), call targets (D-M1-18), builder literals (D-M1-19), inline literal
operands (D-M1-21) and `unknown` register inference (D-M1-20).
