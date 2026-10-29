# Decision log

All entries are R6 proposals/decisions scoped to the commit in [PIN.md](PIN.md).
They do not alter Orbit. Dates use the user's timezone (America/Buenos_Aires).

## D-F0-01 — Pin language inputs

- Date: 2026-10-14.
- Decision: pin `orbit-lang` at `5c01cea68f125f06dd3ac82a61776827bdb0c6f3`; never follow its branch automatically.
- Alternatives: track `main` (rejected: moving IR/ABI invalidates differential evidence).
- Evidence: `git rev-parse HEAD`; hashes and local toolchain are recorded in [PIN.md](PIN.md).

## D-F0-02 — Accept the R6 IRv1 contract

- Date: 2026-10-14.
- Decision: user accepted the JSON format, five manual fixtures, target/DB policy, and fail-closed rule on 2026-10-14. R6 may implement only this local version-1 contract; Orbit still has no `--emit-ir`, published spec or serialized corpus at the pin. Reject unsupported versions and UNSPECIFIED behavior.
- Alternatives: wait for an Orbit emitter (rejected for M1 only, since user explicitly approved the proposal); infer additional semantics (rejected: fail closed).
- Evidence: explicit approval in this task; enum/structure at `compiler/ir.orb:3-105`; `docs/F0_REVIEW.md`.
- Status: **accepted for R6 implementation; not an upstream Orbit publication**.
- Note (2026-10-15): the serialization half of this entry is superseded by
  D-F0-06 (the language now publishes `docs/IR_SPEC.md`); the semantic contract
  and the fail-closed rule stand unchanged.

## D-F0-03 — DB excluded until a published compatible runtime exists

- Date: 2026-10-14.
- Decision: R6 does not claim DB coverage through M2. DB tests are explicitly excluded from R6's differential corpus until M3 unless Orbit publishes and pins a DB-capable runtime variant with matching ABI. Do not fabricate a local variant or shim. The exclusion manifest must enumerate affected tests and count them; that count is pending a reproducible IR corpus.
- Alternatives: link `libruntime_net.a` and silently omit DB (rejected: header says net variant has no DB symbols); build or shim DB locally (rejected: violates runtime ownership).
- Evidence: `runtime/orbit_api.h:106` rejects `ORBIT_WITH_DB` for the net-only library; C backend lowers `db_*` at `c_backend.orb:3539-3596`. The pinned candidate M2 corpus has 89 executable programs (67 suite, 22 std) plus one suite support helper; no source-detectable DB/auth cases were found. This does not replace post-IR differential accounting.
- Status: **accepted by user 2026-10-14**.

## D-F0-04 — Target matrix

- Date: 2026-10-14.
- Decision: user confirmed Windows x86-64 first, Linux x86-64 second, macOS ARM64 from M3, and DB out of corpus until M3 absent official compatible runtime. Single-thread only through M3; no performance measures before M4.
- Alternatives: promise all platforms immediately (rejected: target runtime/toolchain ABI not verified).
- Evidence: local GCC target `x86_64-w64-mingw32`; user confirmed Linux second and macOS ARM64 from M3; Linux runtime build not available on this host.
- Status: **accepted by user 2026-10-14**; Linux runtime verification remains pending.

## D-F0-05 — Unknown semantics fail closed

- Date: 2026-10-14.
- Decision: every opcode/platform/version not explicitly supported must produce a diagnostic and nonzero exit; no stub, invented result, or fallback. Single-thread only through M2.
- Alternatives: copy the C backend's default `Unknown opcode` comment (rejected: it silently continues).
- Evidence: backend fallback at `compiler/c_backend.orb:3602-3604`; user accepted explicit error policy 2026-10-14.
- Status: **accepted by user 2026-10-14**.

## D-M1-01 — Lower pinned literal globals as immediate loads

- Date: 2026-10-14.
- Decision: Win64 M1 accepts only globals with valid identifiers, type `int` or `bool`, and decimal `IRValue.String` values in int32 range (bool only 0/1). `load_var` resolves a local first, then a validated module global. Other global forms fail with a diagnostic.
- Alternatives: emit mutable data sections for all globals (rejected: the pinned builder turns literal globals into C preprocessor defines); coerce unsupported values (rejected: semantic invention).
- Evidence: pinned `builder.orb:2288-2304,2333-2372` and `c_backend.orb:1481-1505`; fixture `14-global-constants` differential pass, and full manual fixture set 14/14.

## D-M1-02 — Use the target CRT for string equality

- Date: 2026-10-14.
- Decision: lower string `eq`/`ne` to direct `strcmp` calls using Win64 C ABI. Keep ordered string comparisons unsupported.
- Alternatives: compare pointers (rejected: Orbit compares string contents); copy a string-comparison routine into R6 (rejected: the target CRT already owns this primitive and libc behavior must not be forked).
- Evidence: pinned `c_backend.orb:2552-2566` emits `strcmp(a,b)==0/!=0`; `tests/suite/strings_eq.orb`; R6 fixture `15-string-equality` and full 15/15 differential gate.

## D-M1-03 — Lower Win64 arguments beyond positional registers to stack slots

- Date: 2026-10-14.
- Decision: support up to 256 scalar parameters/arguments. Reserve 32 bytes of shadow space plus eight bytes per outgoing argument after position 4; read callee positions 5+ from the Win64 stack argument area. Float values retain their binary64 bits in the positional slot.
- Alternatives: retain the four-argument limit (rejected: the target ABI and existing native lowering already define stack slots); shift float parameters by type (rejected: Win64 slots are positional).
- Evidence: native differential fixtures `16-call-stack-args` and `17-call-stack-float`; full manual gate 17/17.

## D-M1-04 — Call official runtime for int-list creation and push

- Date: 2026-10-14.
- Decision: implement only `list_create`/`list_push`/`list_len` for int lists in `main`. Use official `orbit_list_create_typed` and `orbit_list_push_int`; honor Win64 `OrbitResult` sret and its pinned header layout. Lower the header's inline `orbit_list_len` as a load of the declared `OrbitList.len` member.
- Alternatives: reimplement list allocation/push/storage (rejected: duplicates runtime ownership); accept all element kinds (rejected: typed helpers and failure paths are not yet lowered); skip the `OrbitResult.ok` field (rejected: invents failure behavior).
- Evidence: pinned builder `builder.orb:4731-4749`; C lowering `c_backend.orb:3387-3421`; ABI `runtime/orbit_api.h:179-184,217-224,361-376,517-519`; fixture `18-int-list-literal-len`; full manual differential gate 18/18.

## D-M1-05 — Lower checked int-list reads with official runtime errors

- Date: 2026-10-14.
- Decision: support `list_get` for int lists in `main` only. Check null list/data, unsigned bounds, and the pinned element-kind rule (int or opaque); call the official index/kind error routines on failure.
- Alternatives: unchecked pointer arithmetic (rejected: changes runtime failure behavior); duplicate the collection runtime (rejected: runtime owns these semantics); support other kinds (rejected: their representation/lowering is not covered).
- Evidence: pinned `c_backend.orb:495-502` maps int reads to `orbit_list_read_int`; `runtime/orbit_api.h:217-224,358-360,422-468,485-490`; `runtime/collections.c:78-100`; fixtures `19-int-list-get` and `20-int-list-get-oob`, full manual differential gate 20/20.

## D-F0-06 — Adopt the language-side IRv1 textual format

- Date: 2026-10-15.
- Decision: R6 adopts `docs/IR_SPEC.md` from the Orbit checkout (blob
  `110ce439be8969d0e97986ad5eaa0cf0db92b1af` at HEAD `0837185`, "language-side
  proposal for R6") as its loader contract, superseding the serialization of
  `docs/IR_SPEC_DRAFT.md`, including the spec's own precedence rule ("where
  this document and the draft disagree, this document prevails"). The loader,
  the fixtures and the validation placement (V-08/V-10/V-13/V-14/V-16, E-09)
  migrate as listed in `docs/IR_SPEC_ALIGNMENT.md`; the differential gate must
  stay green (20/20) with the migrated fixtures before anything else moves.
- Alternatives: keep the R6 draft serialization (rejected: the language now
  publishes a spec that declares itself prevalent; R6 would stay on an
  unpublished local shape with different key names); declare an IRv2 combining
  both (rejected: a third format to publish for no measured gain; the missing
  NaN-literal expressiveness goes to the language as question P2 instead).
- Evidence: user decision 2026-10-15; difference list, side-by-side format and
  migration plan in `docs/IR_SPEC_ALIGNMENT.md`; loader behaviour read from
  `src/loader/ir.c` (`parse_function`, `parse_instruction`, `parse_route`,
  `validate`, `r6_ir_parse`); the 20 current fixtures use only finite float
  literals, so no measured evidence is lost by V-08.
- Pin: unchanged at `5c01cea68f125f06dd3ac82a61776827bdb0c6f3`. The move happens
  only when the emitter and the serialized corpus exist and are verified, with
  new hashes recorded in `PIN.md` (D-F0-01 policy).
- Status: **accepted by user 2026-10-15; migration landed (see D-F0-07)**.

## D-F0-07 — IRv1 migration outcome: loader, fixtures and call arity

- Date: 2026-10-15.
- Decision: the migration of D-F0-06 landed as one atomic change (commit
  `06d7d2c`, `feat: adopt language-side IRv1 format (20/20 diff)`):
  - `src/loader/ir.c` implements the spec serialization exactly: root
    `irVersion`+"module" (2 keys), `opcode`/`dest`/`operand1..3`/`elemKind`,
    values `{"kind":K,"value":V}` (`{"kind":"None"}` with no payload), flat
    `routeMethod`..`routeLimitBurst` (E-10 coherence), `registerCount` ==
    `registerTypes` length (V-11), `paramTypes` (V-12), ints and floats as JSON
    numbers. The draft document is rejected with `E_FORMAT` and a migration
    hint; no dual acceptance.
  - Loader gains the spec's missing validations: V-06/V-07/V-08 (fraction and
    exponent rejected in integer contexts; non-finite floats rejected), V-10
    (dest required for value-producing opcodes and non-void calls), V-13
    (`call` consumes exactly its `operand2` pending args), V-14 (jump targets
    must exist), V-16 (route coherence), V-18 (an extern body is ignored) and
    the V-01 elision marker.
  - `src/codegen-x64/win64.c` consumes the operand2 arity for direct calls
    (last-N pending args, E-05) and no longer rejects an extern body; it keeps
    the R6 rule arity == declared parameter count for the direct calls it
    supports and still fails closed (`CGEN`) on leftover pending args.
- Alternatives: validate `call` arity only in codegen (rejected: V-13 is a
  loader rule and the diagnostic must name the field); accept the C backend's
  silent tolerance of leftover pending args (rejected: R6 does not emit a
  program whose call arguments it cannot account for; the builder never
  produces one); keep route functions accepted (rejected: Kynx routes need
  `OrbitResponse*` plumbing outside the native subset -> explicit
  `E_UNSUPPORTED`).
- Codes pinned for the negative suite: structural shape and kind mismatches ->
  `E_SCHEMA`; magnitude/range and non-finite floats -> `E_RANGE`; unknown
  opcode names -> `E_OPCODE`; draft documents -> `E_FORMAT`; unsupported
  opcodes/routes -> `E_UNSUPPORTED`.
- Evidence: gate `python tests/diff/run_diff.py` **20/20** with exit/stdout
  byte-identical to the pre-migration baseline (`.r6-build/baseline-20.txt`
  vs `.r6-build/migrated-20.txt`); the spec's own §5.2 minimal example builds
  and runs (exit 0); the draft form of `01-int-call` exits 1 with
  `error[E_FORMAT]`; no `.orb` file changed; driver build clean under
  `-Werror` (SHA-256
  `CAD71E9FD9786867ECD91F69EED76E54858372833095F20B82BF9541D53AA932`);
  `.gitattributes` pins LF for `*.json`/`*.orb` so a fresh checkout satisfies
  V-01 regardless of `core.autocrlf`.
- Status: **accepted (implementation evidence, not an upstream publication)**.

## D-M1-06 — Loader contract suite as the second gate

- Date: 2026-10-15.
- Decision: `tests/reject/` is the repo's second gate, committed with the
  migration: 47 rejection cases covering every applicable V-01..V-19 rule and
  E-01/E-02/E-08/E-10/E-11/E-12 edge cases (each case cites its rule in
  `manifest.json`), plus 4 acceptance controls. A rejection must exit 1 with
  exactly one `r6: <file>:<line>:<col>: error[CODE]: <msg>` line carrying the
  expected code; an acceptance control must build and, when declared, run with
  byte-compared stdout (the corpus `01-int-call` prints `M1-add\r\n\r\n` and
  exits 42, matching the differential baseline).
- Alternatives: one shell script per case (rejected: a manifest + single
  runner keeps the rule → code → exit mapping in one auditable place);
  asserting only exit codes (rejected: fail-closed means the diagnostic names
  the violated rule — codes are contract, not presentation); folding the
  rejections into the differential gate (rejected: different oracle — the C
  backend has no opinion on loader rejections, and `.orb` counterparts cannot
  express malformed IR).
- Byte-exact cases (BOM, missing trailing LF, invalid UTF-8, elision marker,
  `1e3`/`1e999` forms, duplicate keys) are rendered by `make_cases.py`, which
  also renders `manifest.json`; `--check` re-verifies the tree. E-05/E-07/E-09
  are exercised as acceptance controls (`ok-call-arity-suffix`,
  `ok-untargeted-label`, `ok-extern-body`); E-03/E-04/E-06 require phase C
  decisions because R6's strict `registerTypes` typing and runtime alias set
  reject them in codegen.
- Evidence: `python tests/reject/run_reject.py` → **51/51** (47 reject + 4
  accept) on 2026-10-15; `make_cases.py --check` renders the tree identically;
  differential gate still **20/20** with the suite in place; driver SHA-256
  `CAD71E9FD9786867ECD91F69EED76E54858372833095F20B82BF9541D53AA932`.
- Status: **accepted**.

## D-M1-07 — `nop`/`begin_block`/`end_block` admitted as no-ops

- Date: 2026-10-15.
- Decision: the loader admits `nop`, `begin_block` and `end_block`; codegen
  lowers all three to no machine code. Ground at the pin:
  `c_backend.orb:2592` appends an empty string for `nop`;
  `c_backend.orb:2835-2842` renders the blocks as the C braces `{`/`}` (pure C
  scoping decoration: no observable runtime effect for the IR the pin can
  compile; R6 registers are function-wide stack slots and it emits nothing);
  `cteval.orb:953,998` produces `nop` with `dest -1` while folding dead
  arguments/instructions; `isNopOrControl` (`cteval.orb:223-229`) classifies
  `nop`/`begin_block`/`end_block` as control, so folding never removes them.
- Producers: the pin's frontend emits neither block marker anywhere
  (`git grep 5c01cea -- compiler/` shows only the `ir.orb` enum, `cteval` and
  `c_backend`), so the fixture is hand-written like cases 06–20 and the oracle
  is the behavior of the equivalent `.orb`.
- Evidence: fixture `21-nop-blocks` (`nop` plus `begin_block`/`end_block`
  around an add; `.orb` uses an `if true` block) → differential **21/21** on
  2026-10-15 (exit 7, empty stdout, identical to the oracle); loader suite
  still **51/51**; table updated in `docs/proposed-ir/m1/README.md`.
- Status: **accepted**.

## D-M1-08 — `result_*` opcodes lowered against the official runtime

- Date: 2026-10-15.
- Decision: the loader admits `result_ok`, `result_err`, `result_is_ok`,
  `result_unwrap` and `result_unwrap_err`; codegen lowers them against the
  pinned runtime. Each `result` register owns a private 24-byte `OrbitResult`
  cell below the list scratch area (`ok` +0, `error_code` +4, `error_msg` +8,
  `value` +16 — the layout `runtime/orbit_api.h` declares; `sizeof` 24 and the
  offsets were verified against the pinned header), and the frame grows by
  `24 * registerCount` when any register is typed `result`.
- Lowering (read from `c_backend.orb:3458-3511` and confirmed against the
  archive): `result_ok` calls `orbit_result_ok` with the `(void*)(uintptr_t)`
  word cast for int/bool/string payloads, or `orbit_result_ok_float` for float
  payloads; `result_err` calls `orbit_result_err(code, msg)`, with the
  single-operand form (`operand2` None) passing the code and a NULL message;
  `result_is_ok` reads the `.ok` byte; `result_unwrap` reads `.value` as a
  word, or calls `orbit_result_get_float` for float destinations (the payload
  travels as the IEEE-754 bit pattern, per the runtime's own comment);
  `result_unwrap_err` reads `.error_msg` with the backend's `?: ""`
  empty-string fallback.
- ABI note: MS x64 argument registers are positional — argument N selects
  RCX/RDX/R8/R9 or XMM(N-1) by position — and the hidden sret pointer of a
  struct-returning function is argument 1, so the float parameter of
  `orbit_result_ok_float` is passed in XMM1, not XMM0 (disassembly of the
  pinned archive confirms it). The same rule applies to future sret runtime
  calls with float parameters (e.g. `orbit_list_push_float`). The positional
  order also fixes `orbit_result_err`: sret RCX, code RDX, message R8.
- Fail-closed boundaries: construction destinations must be typed `result`
  and observation destinations `bool`/`int`/`float`/`string`; operands are
  result registers only; `result_ok` payloads are int/float/bool/string
  (NUL-bearing string literals stay rejected as everywhere else); `ret` of a
  `result` register is rejected with CGEN (the sret return contract is not
  modelled); `copy`/`decl_var`/`store_var`/`arg` of `result` registers keep
  failing as before; `unknown` registers remain unsupported at module level.
- Producers: the pin's frontend emits these opcodes for `ok(...)`, `err(...)`
  and `try ... catch` (`parser.orb:450-459`; `builder.orb:3117,3166,4215,4234,
  4831,4842,4854`); the fixture is hand-written like cases 06–21 and the
  oracle is the equivalent `.orb` (exit 46).
- Evidence: fixture `22-result-ops` → differential **22/22** on 2026-10-15
  (exit 46, empty stdout); loader suite **51/51**; table updated in
  `docs/proposed-ir/m1/README.md`.
- Status: **accepted**.

## D-M1-09 — `list_set`/`list_pop`, list kinds 1–4 and the arena outside `main`

- Date: 2026-10-15.
- Decision: the loader admits `list_set` and `list_pop`; the six list opcodes
  (`list_create`, `list_push`, `list_len`, `list_get`, `list_set`, `list_pop`)
  now validate `elemKind` 1–4. The numbers map 1:1 to the runtime's
  string/int/float/bool element kinds and to an R6 register type of the same
  name; kind 0 (opaque) and kind 5 (ref) stay CGEN fail-closed, as does any
  list operand or destination whose register type does not match the kind.
- Lowering, read from `c_backend.orb:3387-3433` and `listWriteCall`
  (`c_backend.orb:520-550`): `list_set` is a direct
  `orbit_list_set_{string,int,float,bool}` call (sret RCX, list RDX, index R8,
  value R9/XMM3) with no prologue checks — the runtime validates. `list_push`
  passes the value per kind (string/int as the register word, float in XMM,
  bool normalized to a clean 0/1 word with a 32-bit move). `list_pop`
  reproduces the pinned prologue: `_pl = (orbit_int)orbit_list_len(list)` (a
  NULL list reads as length 0) with `if (_pl <= 0)
  orbit_list_index_error(where, 0, 0, 0, file, line)`, then the read at
  `_pl - 1` and `orbit_list_pop(...)` into the shared scratch. The read keeps
  the pinned null-data diagnostic (`index_error(where, _pl - 1, len, kind)`);
  only its out-of-range branch is omitted as unreachable (`_pl - 1 < len` by
  construction). `where` is `"orbit_main"` in main and the function name
  elsewhere, matching the `__func__` the generated C passes.
- Arena: the pin assigns the executable arena to a file-scope global
  (`OrbitArena* arena = NULL;` overwritten with the init arena inside
  `orbit_main`) and every non-main function that creates a list reads it
  before `orbit_list_create_typed` (`c_backend.orb:3391`). R6 mirrors that
  contract with a private 8-byte slot `r6_arena` in `.data` (section flags
  `0xC0500040u`: read+write+align16+initialized, the `.rdata` flags plus the
  write bit), written from RCX by the `orbit_main` prologue and read from the
  global by any non-main function that needs it; the slot is emitted only when
  such a function exists. `string_add` outside `main` stays CGEN fail-closed
  even though the global would now make it lowerable (not measured; boundary
  kept from D-F0-06).
- Encoder: `subq reg,reg` (opcode 0x29, the `addq`/`cmpq` operand pattern) and
  `jle rel32` (0x0F 0x8E) join the supported subset — `_pl <= 0` and `_pl - 1`
  have no other encoding with the admitted forms. The existing `movq reg,reg`
  encoder silently dropped REX.R/REX.B and is fixed now that a lowering is the
  first to use `%r10`/`%r11` in a register-to-register move (`movq %rax,%r11`
  used to encode `mov %rax,%rbx`, and the pop read decoded the wrong slot).
- Frame layout: the 24-byte list/result scratch now sits below the
  register/parameter/local slots (base at `slots * 8 + 24`, block
  `[slots * 8 + 1, slots * 8 + 24]`), never overlapping them. The SSA-shaped
  fixtures 18–22 never parked a live slot in the old overlap, but
  `23-list-set-pop` does (its `bs`/`bp` locals) and the sret store corrupted
  the list the pop then read. Result cells (D-M1-08) shift by the same 16
  bytes; frame size is derived from the same slot/scratch/cell ends plus the
  shadow space and any stack arguments.
- Producers: the pin's frontend emits `list_set`/`list_pop` for indexed
  assignment and `.pop()` (`builder.orb:3908-4033, 4540-4560`; literal lists
  become create+pushes); the fixture is hand-written like cases 06–22 and the
  oracle is the equivalent `.orb`.
- Evidence: fixture `23-list-set-pop` → differential **23/23** on 2026-10-15
  (exit 135, empty stdout); loader suite **51/51**; table updated in
  `docs/proposed-ir/m1/README.md`.
- Status: **accepted**.

## D-M1-10 — `map_*` stays outside the native subset

- Date: 2026-10-15.
- Decision: the loader keeps rejecting the six map opcodes (`map_create`,
  `map_set`, `map_get`, `map_has`, `map_delete`, `map_keys`) with
  `E_UNSUPPORTED`. The family is closed with evidence instead of lowered: no
  document that carries a live map value can be accepted without depending on
  an open gap, and three of the six lowerings are ill-formed against the
  contractual runtime.
- Producers (spec §1.1): only `.has`/`.delete`/`.keys` emit opcodes
  (`builder.orb:4051-4077`); `map_create`/`map_set`/`map_get` have no producer
  at the pin, so no corpus program (§5.5) can contain them.
- Lowering (§1.2 rows 116-121) against the contractual runtime (§4,
  `orbit_api.h:378-382,534-556`), measured with the fixed-point oracle
  (`orbit-fp-26cfb87400.exe`) and gcc from w64devkit:
  - `map_create` → `orbit_map_create(O1)`: one argument where the header
    requires `(arena, value_size)`. The sibling call-path emission fails the
    same way: declaring `extern fn orbit_map_create(value_size: int)
    -> map<string, int>` and calling it with `8` emits
    `r_0 = (void*)(uintptr_t)(orbit_map_create(arena, (orbit_int)(uintptr_t)(8)));`
    and gcc rejects it — "aggregate value used where an integer was expected".
  - `map_keys` → `orbit_map_keys(O1)`: the arena argument is missing — gcc,
    "too few arguments to function 'orbit_map_keys'; expected 2, have 1".
  - `map_delete` → the builder types the destination `void`
    (`builder.orb:4066`) while the arm assigns the `OrbitResult` to it; the
    produced C names an undeclared register — gcc, "'r_5' undeclared".
  - `map_has` → `orbit_map_has(O1, O2)` is the one well-formed arm (inline,
    `orbit_api.h:553-556`), and a probe program does run end to end: declare
    the extern with `-> result`, `try orbit_map_create(8)`, then `.has("k")`
    answers `false` on the empty map and `true` after
    `orbit_map_set(map, "k", "v")`.
  - `map_set`/`map_get` → well-formed text (`orbit_map_set(O1, O2, O3)`,
    `orbit_map_get(O1, O2)`) but producer-less, and reachable only with a live
    map value.
- The blocking dependency is the map value itself: the only working creation
  path at the pin is the `orbit_map_create` call, which works exclusively
  because the C backend prepends `arena` (`functionNeedsArena`,
  `c_backend.orb:4220`). That list is gap **U-04** (spec §3.1.9/§6): its
  membership is UNSPECIFIED and "the loader rejects any document that depends
  on it". IRv1 carries no value that yields the arena, so R6 cannot lower the
  call faithfully either, and the extern surface does not open a way around
  it: R6's whitelist already refuses `-> result` / `-> map<...>` extern
  declarations (non-scalar return types, `win64.c:241-242`).
- Consequence: no fixture 24 is added; the table in
  `docs/proposed-ir/m1/README.md` records the closure, and `tests/reject/`
  pins one rejection per opcode (cases `x-map-create` … `x-map-keys`).
- Evidence: oracle probes p7-p12 (scratch `.r6-build/map-probes/`, 2026-10-15);
  loader suite **57/57**; differential gate unchanged at **23/23**.
- Status: **accepted**.
- Note (2026-10-15): "no fixture 24" means no map fixture; the number was later
  used by the model family (D-M1-11).

## D-M1-11 — Model constructors and field access inside the measured subset

- Date: 2026-10-15.
- Decision: the loader admits `load_field`/`store_field` and codegen lowers a
  model only in the shape the pinned builder emits: construction is a `call`
  whose target is a `String` naming the model, arity equal to the field count
  and field values in declaration order (`buildObjectLiteralAsModel`,
  `builder.orb:1206-1233`); reads and writes require a receiver register whose
  declared type is a model name, and the reference resolves inside that model.
  Fields are limited to `int`/`bool`/`float`/`string`. Model validation now
  runs at module level (duplicate fields, unsupported field types, register
  types), replacing the blanket CGEN refusal of `models`; `types`/`migrations`
  stay rejected. Everything else stays fail-closed: model values crossing a
  function boundary (return, parameter, `arg`), field access on non-model
  receivers (string/JSON/object paths included), collection fields, duplicate
  field names, invalid model names and constructor arity/target errors
  (CGEN), plus duplicate model names or a model name colliding with a function
  name (`E_DUPLICATE` at load).
- Producer shape: annotated receivers qualify references through
  `fieldOwnerForObject` (`builder.orb:1983`). The pinned C backend synthesizes
  the struct plus `orbit_model_M_create` (fields as positional parameters,
  `arena` prepended by the backend, `c_backend.orb:1631-1635`) with
  `int`/`bool`/`float`/`string` members at 4/1/8/8 bytes; R6 reproduces that C
  layout (offsets in declaration order, size rounded to the widest member
  alignment).
- Lowering: the constructor arm inlines `orbit_model_M_create` — arena from
  the `orbit_main` slot in `main` or the `r6_arena` global elsewhere
  (D-M1-09), `orbit_alloc(arena, sizeof(M))` (`runtime/orbit_api.h:334`), one
  store per field at its C offset; the destination receives the pointer.
  `load_field` sign-extends `int` (`movslq`), zero-extends `bool` (`movzbl`)
  and moves `float`/`string` as 8-byte values (`movsd`/`movq`); `store_field`
  writes with the C width (`movl` truncates `int`, `setne`+`movb` normalizes
  `bool`). A bare reference resolves in the receiver's model; an `Owner.`
  prefix must name the receiver's model. The pin's fallback for a reference
  with no owner (`findFieldOwner`, `c_backend.orb:4406-4415`, a last-wins scan
  over a module-wide field-name table) is deliberately not reproduced: R6
  fails closed when the receiver's model lacks the field instead of guessing
  across models.
- Encoder: `movb %reg8,mem` (8-bit GPR ids 0-3 only, so `%spl`-style names
  cannot silently encode `%ah`), `movb $imm,mem`, `movzbl mem,%r32` and
  `movslq mem,%r64` join `asm_win64.c`.
- Boundary pinned in `tests/reject/` (rule D-M1-11): non-model receiver,
  missing field, owner/receiver mismatch, destination or value type differing
  from the field type, `store_field` with a destination, constructor arity
  mismatch, unsupported field type, non-`Sym`/`String` reference (`E_SCHEMA`),
  unknown constructor target, duplicate model name, model↔function collision
  (`E_DUPLICATE`); acceptance control `ok-model-ctor`.
- Evidence: fixture `24-model-fields` → differential **24/24** on 2026-10-15
  (exit 42, stdout `seed\nmut\nf-ok\n`); loader suite **69/69** (64 rejections
  + 5 acceptance controls); table updated in `docs/proposed-ir/m1/README.md`.
- Status: **accepted**.

## D-M1-12 — Union tag dispatch and payload packs inside the measured subset

- Date: 2026-10-15.
- Decision: the loader admits `union_get_tag`/`union_get_data`, and codegen
  lowers a declared type only when `kind` is `union_type` and the program
  stays inside the measured union subset: tag constants `<Union>_TAG_<Variant>`
  carry the variant ordinal and load as `Sym`/`String`; construction is a
  `call` whose target is a `String` naming the variant, arity equal to the
  payload count and a fresh `void*` destination; the value is a 16-byte cell
  `{ int tag @0; void* data @8 }` off `orbit_alloc`, whose data word holds the
  single payload (null for a unit variant) or a pointer to the synthesized
  `<Union>_<Variant>` pack struct when the payload is a comma list of at most
  two `int`/`bool`/`string` parts. `union_create`, `switch_op` and `type_info`
  stay `E_UNSUPPORTED`: they have no producer in the builder, `switch_op`
  lowers to an empty switch skeleton and `type_info` to a comment (U-12).
  Everything else stays fail-closed (CGEN): float or named payloads, payload
  arity > 2, kinds other than `union_type`, tag loads that do not name a
  declared variant, `union_get_tag`/`union_get_data` sources that are not
  `void*` registers or destinations outside
  `int`/`void*`/`bool`/`string`/model, pack references that do not resolve to
  a multi-payload variant or field of the receiver, constructor arity/target
  errors, and `void*` at a function boundary (returns and parameters stay
  scalar/model-only). The loader adds `E_DUPLICATE` for duplicate variant
  names, a variant name declared by more than one type, and colliding variant
  names or tag constants against functions, models or globals; `E_SCHEMA`
  when `variants` and `richVariants` lengths disagree.
- Producer shape: match arms emit `union_get_tag` and `union_get_data`
  (`builder.orb:3326-3336`), bind the payload — `decl_var` from the data word
  for one payload, `load_field` on `<Union>_<Variant>._i` for packs
  (`builder.orb:3338-3388`) — and then compare the tag with a `load_var`
  of the `<Union>_TAG_<Variant>` symbol plus `eq` + `jump_if_false`
  (`builder.orb:3390-3428`); the payload-less variant used as a value builds
  a real node through `call` with a `String` target and arity 0
  (`builder.orb:4677-4690`); the C backend synthesizes the cell
  (`c_backend.orb:105-109`) and one pack function per multi-payload variant
  with positional `_i` fields (`c_backend.orb:150-160`). Pin note: that
  emission puts each arm's binding prologue before the arm's tag test, so a
  pack binding reached while the scrutinee still carries the unit data word
  (null) faults in the oracle; fixture 25 orders the unit arm before the pack
  arm — the order the corpus already uses — and both sides then agree. The
  cell layout is locally emitted text (U-14), and the admitted subset never
  lets a union cell cross a function boundary, so no document depends on it
  as an external contract.
- Lowering: constructor → `movl $16,%edx` + `orbit_alloc`, tag ordinal
  stored at offset 0 (`movl $idx,0(%rax)`), data word at offset 8: a single
  payload is the raw value word (`movl`/`movq` from the frame, `leaq` for
  string literals), a unit variant stores `movl $0,%edx`, and multi-payload
  variants first `orbit_alloc` the pack struct, store each part with its C
  width (`movl`/`movb`/`movq`, declaration order) and then write the pack
  pointer into the cell. `union_get_tag` lowers to `movslq 0(cell)` and
  `union_get_data` to `movq 8(cell)`; pack reads (`load_field` on
  `<Union>_<Variant>._i`, `void*` receiver) lower to
  `movslq`/`movzbl`/`movq` of the pack field; a tag constant lowers to
  `movq $ordinal` in the destination's int slot. `store_field` stays
  model-only (no pack path).
- Encoder: unchanged — `movl $imm,%r32`, `movl $imm,mem`, `movq $imm,mem`
  and the `movslq`/`movzbl`/`movq` memory loads already covered every
  emitted form.
- Boundary pinned in `tests/reject/` (rule D-M1-12): `union_create`,
  `switch_op`, `type_info` (`E_UNSUPPORTED`); float payloads, payload
  arity > 2, non-`union_type` kinds, `union_get_tag` destination not `int`,
  non-`void*` tag/data sources, unknown tag constants, missing or
  single-payload pack fields, constructor arity (CGEN); duplicate variant
  names, cross-type variant names, tag/function-name collisions
  (`E_DUPLICATE`); `variants`/`richVariants` length mismatch (`E_SCHEMA`);
  acceptance control `ok-union-ctor`.
- Evidence: fixture `25-union-tag-data` → differential **25/25** on
  2026-10-15 (exit 85, stdout `u-ok\n`); loader suite **86/86** (80
  rejections + 6 acceptance controls); table updated in
  `docs/proposed-ir/m1/README.md`.
- Note (D-M1-35, 2026-10-22): `enumeration` is lowered as an `int` too, so
  "non-`union_type` kinds" in the boundary above now means every kind other
  than `union_type` and `enumeration`. The control `x-union-kind-enumeration`
  becomes the acceptance control `ok-enum-kind-unit`; `x-union-kind-sum`
  (filed under D-M1-35) keeps the kind gate pinned.
- Status: **accepted**.

## D-M1-13 — `alloc`/`free` stay outside the native subset

- Date: 2026-10-15.
- Decision: the loader keeps rejecting `alloc` and `free` with
  `E_UNSUPPORTED`. Both exist only in the opcode enum and the C lowerings —
  no producer anywhere in `compiler/*.orb` at the pin (spec §1.1) — so no
  corpus program (§5) can carry them and no `.orb` can back a differential
  fixture that executes them with observable effect. The family is closed
  with evidence instead of lowered, mirroring the producer-less opcodes
  already refused (`switch_op`/`type_info`/`union_create`, D-M1-12).
- `free` (§1.2 row 107): the observed lowering is the comment
  `/* free - arena managed */` and never frees (`c_backend.orb:3293-3296`);
  the spec's audit note keeps its semantics `UNSPECIFIED` (U-09) and states
  the no-op is NOT semantics, so admitting it would invent behavior.
  `cteval.orb:49,86` only classifies it (impure / side-effecting); nothing
  emits it.
- `alloc` (§1.2 row 106): the observed lowering is
  `D ← orbit_arena_alloc(arena, O1)`, with `sizeof(void*)` when O1 is absent
  (`c_backend.orb:3280-3292`), but the destination is a raw pointer with no
  consumer inside the measured subset: no admitted opcode reads through it
  (union cells and packs only come from the admitted constructors), `void*`
  values cannot cross a function boundary (D-M1-11/D-M1-12 fail closed on
  returns, parameters and `arg`), and no oracle program can pin the
  default-size branch of a producer-less opcode. A fixture could only drive
  it as an inert side effect, adding surface the corpus can never hit.
- Consequence: no fixture is added; `tests/reject/` pins one rejection per
  opcode (cases `x-alloc-opcode` and `x-free-opcode`, both `E_UNSUPPORTED`)
  and the closure is recorded in `docs/proposed-ir/m1/README.md`.
- Evidence: loader suite **88/88** (82 rejections + 6 acceptance controls);
  differential gate unchanged at **25/25**; lines cited from the pinned
  checkout at `5c01cea`.
- Status: **accepted**.

## D-M1-14 — Function values and indirect calls inside the measured subset

- Date: 2026-10-15.
- Decision: the native subset admits function values without capture and the
  indirect `call` through them, mirroring the pinned builder: a bare function
  name in value position serializes as `ir_symbol`
  (`buildCallArgValue`, `builder.orb:1235`); `val f = inc` / `var h = inc`
  binds it with `decl_var` + Symbol initializer (`buildVarDecl`,
  `builder.orb:2509`); the signature spelling is `fn(p1;...;pn)->ret`
  (`fnTypeSpellingFor`, `builder.orb:2202`); and `f(args)` lowers as pending
  `arg` instructions, a `load_var` of the callee register, and a `call` with
  the callee in `operand1` and the arity in `operand2` (`buildFnVarCall`,
  `builder.orb:3646`). `tests/suite/function_pointers.orb` is the measured
  corpus test; the fixture covers its int/string/two-param/nullary and
  reassignment shapes.
- Admitted: register types whose `fn(...)` spelling is well-formed with scalar
  parameter and return tokens (`int`/`bool`/`float`/`string`; zero parameters
  spell `fn()->ret`); Symbol initializers in `decl_var` (the local is typed by
  the spelling) and in `load_const` (the spelling must equal the destination
  register type); `store_var` rebinding through a register word; indirect
  calls checked against the register's spelling (exact arity, each pending
  argument's type, destination type) and emitted as a word load plus
  `callq *%r10` (`win64.c:1933`, encoder `asm_win64.c:217`). Fn values may
  name any module function with a scalar spelling except `main`
  (`sema.orb:3365` — "Cannot use 'main' as a value"); extern functions are
  measured in position: the oracle accepts `orbit_string_at` as a value and
  calls it through the binding with identical exit to R6 (fixture
  `26-fn-values`, subtest `extern_value`).
- Rejected fail closed: undeclared symbols, `main`, non-scalar spellings
  (void return, list/model parameters — `CGEN`), destination type mismatches,
  malformed or non-scalar `fn(...)` register types (module register-type
  gate), `store_var` with a Symbol initializer (the builder loads the symbol
  into a register first), fn-typed function returns (`win64.c:647`), calls
  through a non-`fn` register (`win64.c:1003`), arity different from the
  spelling's parameter count (`win64.c:1011`), argument/destination type
  mismatches, and a call consuming more pending arguments than exist (V-13,
  `E_SCHEMA`, generic rule). String concatenation inside callees stays under
  the D-M1-09 boundary and is not re-litigated here
  (`greet`/`shout`/`whisper` remain outside).
- Consequence: fixture `26-fn-values` (exit 42, stdout `fn-ok`), 13 rejection
  pins (twelve `CGEN` + one V-13) and the acceptance controls
  `ok-fn-value`/`ok-fn-extern`; differential gate **26/26**; loader suite
  **104/104** (96 rejections + 8 acceptance controls).
- Evidence: builder/sema cites from the pinned checkout at `5c01cea`;
  `tests/suite/function_pointers.orb`; the oracle and R6 build and run the
  fixture pair with identical exit/stdout, including the extern binding
  (oracle probe: `orbit_string_at("abc", 1) == 98`).
- Status: **accepted**.

## D-M1-15 — Register-valued globals stay rejected

- Date: 2026-10-15.
- Decision: module globals keep admitting only the D-M1-01 literal forms; a
  global whose value is anything else — `ir_register` included — keeps
  failing with `global %s is outside M1 literal int/bool support`
  (`win64.c:2093`), now pinned as case `x-global-register`.
- Why it stays out: the builder does contain a path that would mint
  `IRGlobal(name, ir_register(valueReg))` for a non-literal top-level
  `const` (`builder.orb:2377`), but the module sema refuses every non-literal
  top-level declaration first (`E1007`, `sema.orb:1824-1831`; accepted shapes
  in `isTopLevelConstantExpr`, `sema.orb:1840`), so no corpus program can
  produce one. Admitting the shape would invent storage semantics the module
  contract never defines (a global pointing into frame state).
- Consequence: no fixture is added. Together with D-M1-14 this closes C8
  (globals + indirect calls).
- Evidence: loader suite **104/104** (96 rejections + 8 acceptance controls);
  differential gate **26/26**; cites from the pinned checkout at `5c01cea`.
- Status: **accepted**.

## D-M1-16 — The spec edge cases E-03/E-04/E-06 stay fail-closed

- Date: 2026-10-16.
- Decision: the three spec edge cases the phase-B notes left open stay
  outside the native subset and keep failing in codegen, pinned as
  `x-list-get-kind-ref` (E-03), `x-list-create-kind-opaque` (E-04) and
  `x-print-two-args` (E-06), all `CGEN`.
- E-03 (`elemKind 5` on `list_get` with `registerTypes[D] == "unknown"`; the
  spec table asks to accept, "ref elements keep unknown/void*"): kind 5 is
  the builder's reference kind (`isReferenceKindName`, `builder.orb:1346-1353`;
  sliced in `elemKindCode`, `builder.orb:1356-1363`) and
  `registerTypeForElemKind` maps it to `"unknown"` (`builder.orb:1380-1386`).
  R6 has no `unknown`/`void*` value registers — D-M1-09 already keeps kinds
  0/5 CGEN fail-closed and D-M1-12 keeps `void*` at function boundaries out —
  and the spec's own audit leaves the kind-5 predicate unaudited (U-01), so
  admitting the shape would invent pointer-list semantics; the module
  register-type gate reports `register type unknown is unsupported in M1`.
- E-04 (`elemKind 0` on `list_create`, "untyped accessor + void* destination"):
  kind 0 is the builder's could-not-tell fallback — no element type exists to
  type an accessor or a destination with — and the shape keeps failing with
  `list_create requires an 8-byte slot size, a kind in 1..4, ...`.
- E-06 (`print` call with 2 pending args; the spec says "accept the IR; the
  lowering emits `/* print */` and drops them"): no Orbit program can produce
  it — the pinned sema refuses `print(a, b)` with `E1003` ("print takes
  exactly 1 argument, got 2.", `sema.orb:3577-3584`) — and the reference
  lowering pops the values into a comment (`c_backend.orb:2797-2802`; U-17
  records the multi-arg shape as "a comment"). The only behavior would be
  dropping values, so R6 keeps refusing (`call to orbit_print passes 2
  argument(s); declaration requires 1`).
- Alternatives: implementing the shapes (rejected: admits untyped/ref
  register values and a value-dropping print — new contract semantics outside
  the measured subset); leaving the divergence unrecorded (rejected: the spec
  asks to accept all three, so the boundary must be explicit and pinned).
- Consequence: the phase-B warnings in `tests/reject/README.md` and D-M1-06
  are resolved; loader suite **107/107** (99 rejections + 8 acceptance
  controls); differential gate unchanged at **26/26**.
- Evidence: the three cases run red on the committed driver with exactly one
  `error[CGEN]` line each; `make_cases.py --check` without drift; builder,
  sema and backend cites read from the pinned checkout at `5c01cea`; the
  spec edge table read at blob `110ce439…`.
- Status: **accepted**.

## D-F0-08 — Language-side answers to P1–P5 and emitter authorization

- Date: 2026-10-16.
- Decision: the language side answered the five questions that blocked the
  emitter (`IR_SPEC_ALIGNMENT.md` §6) and authorized the phase-D branch. The
  emitter serializes `registerTypes` **before** `inferRegisterTypes` (the
  builder model; §5.1 defines the text form from the in-memory model); a
  non-finite `ir_float` fails with a diagnostic (V-08, fail closed; no bit
  encoding for now); unknown-key rejection applies to the whole document
  (root and inner levels); §3.1/§3.2 are **normative for the audited rules**
  (aliases, arity minimums, `print` shapes, returns) with U-04/U-05/U-17
  explicitly open; the serialized corpus is emitted by the language and R6
  pins it as a hash manifest plus a regeneration recipe (same pattern as
  `runtime/orbit_api.h` and the runtime archive).
- Alternatives: serializing after `inferRegisterTypes` (rejected: couples the
  corpus to the unpinned U-10 rewrite rules and is not additive); a bit
  encoding for NaN/Infinity (rejected while no program needs it); root-only
  key rejection (rejected: silently accepts inner typos); C-backend
  observation without normativity (rejected: leaves R6 no contract to
  implement against); committing the corpus in either repository (rejected:
  it is an emitted artifact, reproducible from the pinned emitter).
- Consequence: phase D is unblocked. The corpus may legitimately carry
  `unknown` register types; R6's strict register-type contract rejects some of
  those programs today and they are expected in the D3 missing list, to be
  resolved by later dated decisions. Re-pin only after branch, corpus and
  manifest exist with their gates green.
- Evidence: answers registered 2026-10-16 in `IR_SPEC_ALIGNMENT.md` §6; branch
  `r6-emit-ir` from `5c01cea` in the separate clone (user checkout untouched).
- Status: **accepted by user 2026-10-16**.

## D-M1-17 - The language-emitted corpus is pinned as a measurement (2/89)

- Date: 2026-10-16.
- Decision: phase D pins the language-emitted corpus under `tests/corpus/`
  (`manifest.json` with a per-file sha256 plus one `filesSha256`, the runner
  `run_corpus.py`, and `baseline.json`), and treats the corpus differential as
  a **measurement, not a gate**: only corpus drift from the pin, and a
  `--check` regression below the baseline (`r6BuildOk`/`match`), can fail it.
  Failing corpus programs are the number being measured, never a failure.
- Pin: 90 members (89 programs + `imports.support.orb`), 5334302 bytes,
  `filesSha256 10f6e7d97977aaeec3018b9f8694e78dc7fee3ae614fa16cf4db27cc10b7f89b`,
  emitted by the pinned fixed-point `orbit 0.1.0-rc.2`
  (`1e6a378a6a67a6bfdd36df127584f49c54405538`, binary sha256
  `805cf61894ca83aec0afa1fe5a80a652a0cdc3e3e88a2e6be6a6abdf6e2e07dc`) through
  the language's `scripts/emit_ir_corpus.py`; regenerating and comparing the
  hashes is the contract, the `.ir` files are never committed.
- Measurement 2026-10-16: the oracle builds and runs 89/89; R6 builds **2/89**
  and matches **2/89** (identical exit + stdout bytes); 87 refusals, all
  `error[CGEN]`, none `E_*` (the loader admits the whole corpus). The two
  matching programs are degenerate - one `ret` with an `Int` literal and no
  registers (`bom_utf8` exit 0, `nesting_legal` exit 9) - and 40 of the 41
  opcodes present in the corpus never appear in a program R6 completed.
- First refusal per program (by opcode where the diagnostic carries a
  position, by module scope otherwise): module-level 63, `load_var` 11, `call`
  6, `decl_var` 6, `load_const` 1. Dominant message: `register type unknown is
  unsupported in M1` (45).
- The corpus uses the kinds `Int`/`String`/`Sym`/`Bool`/`Register`/`Label`/
  `None` and **never** `Float` (0 occurrences), which exposes three gaps,
  recorded here as **open contract questions** - not decided, and not relaxed
  on the fly:
  1. `unknown` in `registerTypes` in **84/90** documents, a direct consequence
     of serializing before `inferRegisterTypes` (P1 in D-F0-08); R6's strict
     register-type contract (D-M1-09) is the first refusal in 45 programs.
     Relaxing it means admitting value registers whose type is unknown at
     emission time, so it needs its own dated decision plus a definition of
     what may then be lowered.
  2. `call` with O1 `String` in **2917 calls across 74/90** documents (the
     remaining 6 are indirect through `Register`; there is not one `Sym`).
     Spec §3.1 step 2 is normative for the audited call rules (D-F0-08 P4):
     "`call` reads O1: `String`/`Sym` -> named call". R6 resolves `Sym` only
     and routes `String` to variant construction (D-M1-12), so it refuses
     calls to functions the same document declares (`print`, `system_pid`,
     `system_uptime`, `imported_value`). The spec-ordered fix is to resolve
     `String` as a named call first and keep variant construction as the
     fallback; that changes a measured lowering, so it needs its own entry.
  3. float/bool literals as `Sym`: **412 `load_const` with `Sym` across 36/90**
     documents (`0.0`, `1.5`, `true`, `false`). Spec §2.1 already records
     `Float` as "no dedicated constructor observed" and renders `Sym` raw in C,
     so literal text legitimately arrives as `Sym`; R6 reads every `Sym` in
     `load_const` as a function value (D-M1-14). Disambiguating by text
     grammar (numeric/bool literal vs identifier) is a new loader/codegen rule
     and needs its own dated decision.
- Alternatives: not measuring the corpus (rejected: the pin exists to be
  measured); a hard gate on `match` (rejected: the subset gap is precisely the
  measurement, and a red gate would hide the number); committing the emitted
  `.ir` files (rejected: emitted artifact, reproducible from the pin);
  relaxing the three gaps now (rejected: each widens the native subset and
  needs its own contract decision).
- Consequence: D3 closes with the corpus pinned and measured; the three gaps
  are the priority-ordered backlog for extending the native subset, and the
  re-pin (D4) is the next phase. `unknown` stays rejected until the user rules
  on it.
- Evidence: full run `--check --write-baseline` exits 0 with `ratchet holds:
  r6BuildOk 2 -> 2, match 2 -> 2`; per-case detail kept by `--json`; drift and
  tamper detection exercised (exit 1); the generator's `--self-test` 20/20 and
  `dev.py all` 19/19 in the language clone; in R6 the differential gate stays
  **26/26** and the loader suite **107/107**.
- Status: **accepted** (P5 authorization, 2026-10-16); the three gaps stay
  open, to be decided separately.

## D-M1-18 — The builder's direct-call form: a `String` target is a named call

- Date: 2026-10-16.
- Decision: `call` resolves `operand1` exactly as spec §3.1 step 2 states
  ("`call` reads O1: `String`/`Sym` -> named call"). A `String` target is now
  resolved in the order declared function -> declared model constructor
  (D-M1-11) -> declared union variant (D-M1-12); a `Sym` target keeps the
  named-call reading only. That closes gap 2 of D-M1-17 with the user's
  explicit ruling (P2 answer, D-F0-08): the builder's form is normative, so the
  `String` reading is the primary one and variant construction becomes the
  fallback instead of the only reading.
- Why the order is unambiguous: the census of the pinned corpus (D-M1-17,
  `1e6a378`, `filesSha256 10f6e7d9…`) finds **2917 `call` with O1 `String`
  across 74/90 documents and not one `Sym`** (the remaining 6 calls are
  indirect through `Register`). Resolving those 2917 names inside their own
  documents gives **2227 declared functions**, 177 declared models, 104
  declared union variants and **409 names with no declaration at all**
  (runtime symbols: `orbit_string_at` 130, `print` 121, `orbit_string_len` 78,
  `orbit_string_slice` 38, `orbit_object_*` 24, `orbit_db_*` 4,
  `orbit_string_find` 3, `system_*` 8) with **zero name hitting more than one
  of the three namespaces**. The 409 unresolved names are the builtin/alias
  workstream (spec §3.1 steps 4-6) and stay fail closed until their own
  decision; the module gate already forbids a model/function name collision
  (E_DUPLICATE, case `x-model-function-collision`).
- Also decided here, same rule and same source: spec §3.1 step 7 says a
  destination is not *needed* for a void callee, not that it is forbidden. The
  pinned builder always allocates one and types it `void` (**205 void calls
  across 14 documents carry a `void`-typed destination; none omits it**), so a
  void call may now carry a destination whose declared register type is
  `void`; any other destination type on a void call stays `CGEN`, and the
  emitter does not write the unspecified return word into a `void` slot. The
  `dest:-1` form of step 7 stays admitted (equality, not precedence).
- Admitted: `String` target naming a declared function — the whole named-call
  body the `Symbol` path always used (exact arity against the declaration,
  per-argument type equality, destination equal to the return type, `main`
  refused as an entry point), now shared by both value kinds; `String` naming
  a model or a union variant exactly as D-M1-11/D-M1-12 defined; the
  `void`-typed destination for a void callee.
- Rejected fail closed: a `String` naming nothing keeps the pre-existing
  `call target %s is not a declared function, model, or union variant`
  (pinned by `x-ctor-unknown-model`); arity different from the declaration,
  argument type mismatch, destination mismatch and a non-`void` destination on
  a void call are new `CGEN` pins; a `Symbol` naming no function keeps
  `call target %s is not declared in the module`. Invalid symbol text is
  never looked up as a function.
- Consequence: fixture `27-call-string-target` (`String` targets, inline `arg`
  literal and a `void`-typed destination, exit 42, stdout `string-target`),
  five rejection pins (`x-call-string-target-{arity,arg-type,dest,main}` and
  `x-call-void-dest-type`, all `CGEN`) plus the controls `ok-call-void-dest`
  and fixture 27, which is also registered as an acceptance case with
  `run_exit`/`run_stdout`. Differential gate **26/26 -> 27/27**; loader suite
  **107/107 -> 114/114** (104 rejections + 10 acceptance controls);
  `make_cases.py --check` without drift.
- Fixture fidelity: the call shapes in `27` mirror the builder (String targets,
  a `void`-typed destination, a constant folded into `arg`) while the callee
  body stays inside the measured subset — the builder's own body for `v + v`
  carries an `unknown` destination, which is gap 1 of D-M1-17 and is still
  rejected.
- Evidence: corpus census recomputed from the pinned corpus; oracle and R6
  build and run fixture 27 and `ok-call-void-dest` with identical
  exit/stdout; the five rejections run red on the committed driver with
  exactly one `error[CGEN]` line each; the corpus measurement after the change
  is **3/89** (2/89 before) with `tests/suite/imports.orb` moving from
  first-refusal-at-`call` to a full match and the ratchet re-taken at
  `r6BuildOk 3` / `match 3`; recorded in `tests/corpus/baseline.json` and
  `docs/M1_REPORT.md`.
- Status: **accepted**.

## D-M1-19 — Literal forms: raw `Sym` text in `load_const` and inline `decl_var` initializers

- Date: 2026-10-16.
- Decision (the authorized gap, spec §2.1): `load_const` classifies a `Sym`
  value by its **text grammar**, not by position. `"true"`/`"false"` is the
  bool literal; a numeric text with a **fraction or an exponent** is the float
  literal (finite value required, spec V-08); anything else keeps the
  function-value reading of D-M1-14. A bare integer is deliberately *not* a
  float literal (`"7"` stays a function name), so the reading is deterministic
  and fail closed. The classified literal is then checked against the
  destination register exactly like the native `Bool`/`Float` kinds, so a bool
  literal in an `int` register is still `CGEN`.
- Why text grammar is the source: spec §2.1 records that IRv1 has no dedicated
  constructor for bool and float literals, and the pinned builder renders them
  as raw `Sym` text. The census of the pinned corpus (`1e6a378`,
  `filesSha256 10f6e7d9.`) finds **412 `load_const` with a `Sym` value across
  36/90 documents**: `true` 206, `false` 112, 28 distinct float spellings 93
  (`0.0`, `1.5`, `2.5e3`, `1.5E-3`, `100000000000000000000.0`) and exactly
  **one function value** (`whisper`). Text grammar and destination type agree
  on all 412 (bool 318, float 93, `fn(string)->string` 1), and `Sym` appears in
  no other value slot of the corpus (`load_var`/`decl_var`/`store_var` names
  and `load_field`/`store_field` refs are not values).
- Decision (second gap, found by measuring the same family): the
  `decl_var`/`store_var` emitter assumed the initializer was always a register
  and copied a register slot, but the builder inlines a **literal
  initializer** for `val n = 7`, `val s = "x"`, `val b = true`
  (`buildVarDecl`). The corpus carries **299 of them across 46 documents (Int
  212, String 53, Bool 34)** and none of the three documents that currently
  match R6 has one, so the measurement could not see it while the lowering was
  a silent miscompile: the local received the words of an unrelated register.
  The emit now materializes the constant into the local slot for
  `Int`/`Bool`/`Float64`/`String`, keeps the register copy for `Register` and
  keeps the `leaq` for a function `Symbol`; the loader side already typed
  literal initializers (`value_type`), so no gate was relaxed. `store_var`
  keeps accepting a literal for symmetry with `decl_var` (no producer either
  way; the corpus only ever stores registers there: 534 sites).
- Reported, not fixed (outside this decision; one part is a semantics
  question): a **literal operand inside a binary op** (`eq`/`ne`/`add`/… with
  an `Int`/`Bool`/`String` immediate) is still rejected with `operand 2 must
  be a valid register`. The census is **2622 such operands across 76/90
  documents** (`eq` 884, `add` 737, `ne` 474, `sub` 123, `ge` 84, `lt` 83,
  `gt` 71, `le` 66, `mul` 39, `div` 35, `mod` 26), and `add` with a `String`
  operand (376 sites) is string concatenation, i.e. the U-17 boundary rather
  than a literal form. Both need their own decision; fixture 29 deliberately
  compares two locals so it stays inside the measured subset.
- Admitted: `load_const` with a `Sym` that is `true`/`false` or a finite
  float text, with a matching destination type; `decl_var`/`store_var` with an
  inline `Int`/`Bool`/`Float`/`String` initializer.
- Rejected fail closed (new pins, all `CGEN`):
  `x-sym-literal-float-dest-type` (float text in an `int` register),
  `x-sym-literal-bool-dest-type`, `x-sym-literal-not-finite` (`1e999` is
  numeric text but its value is not finite, spec V-08),
  `x-sym-integer-text-is-a-symbol` (a bare integer is not a float literal) and
  `x-decl-var-literal-type` (a local keeps the type of its first `decl_var`);
  the function-value rejections of D-M1-14 still hold because non-literal text
  resolves as a function name.
- Consequence: fixtures `28-sym-literals` (`Sym` bool/float literals plus the
  builder's own `decl_var` stream, exit 42) and `29-decl-var-literals` (inline
  `Int`/`String`/`Bool` initializers read back through `load_var`, exit 42,
  stdout `ok`), both registered as acceptance cases; three new controls
  (`ok-sym-bool-literal`, `ok-sym-float-literal`, `ok-decl-var-literal`).
  Differential gate **27/27 -> 29/29**; loader suite **114/114 -> 124/124**
  (109 rejections + 15 acceptance controls); `make_cases.py --check` without
  drift.
- Fixture fidelity: the instruction streams of `28` and `29` are the pinned
  emitter's own output for the paired `.orb`; only `registerTypes` is
  completed to the concrete types, because the emitter serializes before
  `inferRegisterTypes` and leaves `unknown` (gap 1 of D-M1-17, D-M1-20).
  Fixture `28` is the witness for the second gap: before the fix the
  differential read `gcc exit=42, r6 exit=7`.
- Evidence: corpus census recomputed from the pinned corpus; oracle and R6
  build and run fixtures 28/29 and the three controls with identical
  exit/stdout; the five rejections run red on the committed driver with
  exactly one `error[CGEN]` line each; the corpus measurement stays **3/89**
  (the module-level and `unknown` gates dominate) but `load_const` disappears
  as a first refusal - the document that stopped there now advances to `ret` -
  and the ratchet is re-taken at `r6BuildOk 3` / `match 3` in
  `tests/corpus/baseline.json`.
- Status: **accepted**.

## D-M1-20 - Register types left `unknown`: a mirror of the language backend

- Date: 2026-10-16. Authorized gap: D-M1-17 gap 1 (spec §2.3: `registerTypes`
  is advisory, the backend derives it).
- Decision: before the module is validated, R6 walks every function and
  **recovers the type of a register declared `unknown` from its defining
  instruction**, mirroring the rules of `inferRegisterTypes`
  (`c_backend.orb:674-1148`) restricted to the opcodes of the native subset, as
  a three-pass fixed point like the original. Only the literal text `unknown`
  is replaced: a declared type is never touched, and a register no rule
  explains **stays `unknown` and stays refused** by the module check, with the
  same message and code (`CGEN`).
- Why infer instead of tolerating `unknown`: the language backend declares
  every register as an untyped `uintptr_t` word (`c_backend.orb:3741-3750`) and
  only casts at the point of use, so it can emit with a `unknown` slot; R6
  emits **typed** x64 and must choose `addl`/`addsd`, so it needs the real
  type. Accepting `unknown` without one would invent semantics (forbidden);
  inferring it is not, because the rule is the same rule the oracle uses to
  emit the code R6 is compared against.
- Mirrored rules (only over `unknown`, three passes): `load_const` by value
  (`Int`/`Float`/`Bool`/`String`; a `Sym` only as `true`/`false` -> bool);
  `load_var` -> the homonymous parameter type, else the type of the first
  `decl_var`/global of that name (`local_info` already resolves the
  initializer: a typed register or a D-M1-19 inline literal), else a
  `<Union>_TAG_<Variant>` constant as `int` (D-M1-12); `add` -> `string` when
  either operand is one, else arithmetic; `sub`/`mul`/`div`/`mod` ->
  arithmetic; `neg` -> the operand's type; `eq`/`ne`/`lt`/`le`/`gt`/`ge`/
  `and_op`/`or_op`/`not_op`/`result_is_ok`/`map_has` -> `bool`;
  `result_unwrap_err` -> `string`; `copy` -> the source type; `arg` -> the
  parameter type of the index; `call` -> the callee's `returnType` / a model
  name (constructor, D-M1-11) / `void*` (variant constructor, the D-M1-12
  union cell); `load_field` -> the receiver model's field type
  (`resolve_field`); `list_create` -> `list`; `list_len` -> `int`; `list_get`
  -> the `elemKind` type (1-4); `map_keys` -> `list`; `union_get_tag` -> `int`
  (the D-M1-12 ordinal tag).
- Two deliberate narrowings against `inferRegisterTypes`, both *shrinking*
  what R6 accepts:
  1. **Arithmetic**: the original is `int`-biased because its registers carry
     no type (`add` is `string` or `int`; `sub`/`mul`/`div`/`mod`/`neg` are
     always `int`). R6 infers `float` when an operand is float, `int` when both
     are int, and leaves the slot `unknown` when they do not agree (an operand
     still untyped, typically) - it never guesses.
  2. **No native type**: what the original resolves through its string/float
     spreading passes, or what the native backend has no register for, leaves
     the slot `unknown`: an **enum** tag constant (`TokType_TAG_*`, the bulk of
     the leftovers; no access type outside `union`), `result_unwrap` (the
     original does not infer it either), `union_get_data` (payload outside the
     subset D-M1-12 already closed), a `call` target not declared in the module
     (builtins/aliases, the D-M1-18 front) and a `map` (no register type; the
     family is closed by D-M1-10). Also a `load_var` whose name is not a
     parameter, a local, a global, or a declared union tag.
- Measured evidence (pinned corpus `10f6e7d9`, `1e6a378`): of the **6735
  `unknown` registers in 84/90 documents**, the pass recovers **5941 (88.2%)**
  and **leaves 40 of those 84 documents with no `unknown` at all**; 794
  survive, headed by `load_var` 492 (mostly the enum tags), `call` 131,
  `result_unwrap` 71, `list_get` 32, `load_field` 21, `union_get_data` 20. The
  attribution is a measurement, not a gate (`tests/corpus/`).
- Gate evidence (same tree): differential **29/29 -> 31/31** with fixtures
  30/31; loader **124/124 -> 137/137** (113 rejections + 24 controls);
  `make_cases.py --check` with no drift (131 case files + manifest);
  `migrate-m1-fixtures.py --check` 20/20 (28-31 skip, newer than `06d7d2c`).
  Corpus re-measured: `r6BuildOk` **3 -> 6** and `match` **3 -> 6`;
  `tests/suite/model_fields.orb` (exit 7), `tests/suite/model_two_fields.orb`
  and `tests/suite/unary_neg.orb` (exit 2) go from refusal to a full
  byte-for-byte coincidence with the oracle. The ratchet is re-taken in
  `tests/corpus/baseline.json` (`r6BuildOk 6`, `match 6`).
- Fixtures 30/31 are the pinned emitter's **verbatim stream with its `unknown`
  slots included** - the opposite of 28/29, which complete the types - and that
  is the point: the second gate now exercises the inference against the real
  oracle. The edge is pinned in `tests/reject/` by the refusals
  `x-infer-arith-unknown-operand`, `x-infer-unresolved-call`,
  `x-infer-unknown-load-var`, `x-infer-result-unwrap` (rule D-M1-20) and the
  controls `ok-infer-eq-bool` (5), `ok-infer-load-var` (3),
  `ok-infer-neg-chain` (5), `ok-infer-call-return` (7), `ok-infer-arg-param`
  (33), `ok-infer-list-create` (8), `ok-infer-union-tag` (0).
- Contract surface: `r6_codegen_win64` becomes `R6Module *` and rewrites
  `register_types` **in place** before any other reader (the loader keeps
  owning the memory: each replaced entry is `free()`d and replaced by a heap
  copy). The parser and the loader are untouched, and a register that is still
  `unknown` produces the same diagnostic as before.
- Measured next wall: none of the 4 documents that still stop at `unknown`
  (`test_regex`, `float_literals`, `if_expression_return`,
  `list_index_out_of_range`) is recoverable by these rules. The dominant front
  after D-M1-20 is the **literal operand inside a binary operation** already
  reported by D-M1-19 (2622 sites in 76/90).
- Status: **accepted**.

## D-F0-09 — Local re-pin: the language pin stays at `5c01cea` and the emitter is an added local input

- Date: 2026-10-16.
- Decision: D4 records the pin without publishing anything. The public
  language/runtime pin stays at `5c01cea` (still reproducible from the upstream
  repository; the runtime and header blobs are unchanged by the branch) and
  `PIN.md` **adds** the emitter as a local input: branch `r6-emit-ir` at
  `1e6a378a6a67` in the separate clone
  `C:\Users\Usuario\Downloads\orbit-lang-emit-ir` (three additive commits on
  the pin, `ed17dac`/`a610905`/`1e6a378`), with the oracle binary hash
  (`805cf618…`), the corpus manifest (`filesSha256 10f6e7d9…`) and the promoted
  self-host canonical (`32FE98D7…`). The entry states explicitly that the
  branch is **not merged and not pushed**.
- Alternatives: moving the language commit pin itself to `1e6a378` (rejected:
  that commit exists only in the local clone, so the pin would stop being
  publicly reproducible while publishing nothing new, since the branch is
  additive); pushing the branch to a remote (rejected by the user 2026-10-16:
  no production upload); committing the emitted corpus or a git bundle into R6
  (rejected: the corpus is an emitted artifact reproducible from the pinned
  emitter, D-F0-08, and a bundle would add binaries to the repository).
- Consequence: R6's documented pin now covers the emitter the corpus was
  measured against, entirely offline. The trade is that the emitter pin is
  **local**: it stops resolving if the clone is deleted, and making it public is
  a separate, later decision (a push or a bundle would only add provenance; it
  does not change what R6 verifies).
- Evidence: the emitter rows in `docs/PIN.md`; branch and commit verified in the
  clone (`git rev-parse r6-emit-ir` -> `1e6a378…`, tree clean, full clone);
  corpus re-checked against the pin (`run_corpus.py --check`: `filesSha256`
  matches, ratchet `r6BuildOk 6 -> 6`, `match 6 -> 6`); the emitter binary
  SHA-256 is `805cf6…`; both R6 gates are green on the same tree.
- Status: **accepted by user 2026-10-16**.

## D-F0-10 — Re-pin to the merged language: `main` `299eb09`, spec blob `16f26a9d`, corpus `93+1`

- Date: 2026-10-16.
- Decision: the user merged the emitter branch into the `orbit-lang` `main`
  locally (no push), so the language pin moves from `5c01cea` to the merge
  commit `299eb094d51098945a6618a60ba2efc02dfe59d9`, whose second parent is the
  emitter tip `1e6a378…`. `PIN.md` re-pins every language-side input to it: the
  spec blob `16f26a9dc2707466dd2d2e8e7695aa28db3bbc9b` (was `110ce439…`), the
  emitter as a first-class in-repo input (fixed point
  `orbit-fp-26cfb87400.exe`, sha256 `36c42e20…`), the designated corpus
  (`93+1` = 71 suite + 22 std, `filesSha256 0106d531…`) and the promoted
  self-host canonical (`PUBLISHED_C 15341D24…`, was `32FE98D7…`). The runtime
  rows move with the commit: `runtime/orbit_api.h` blob `27fc52b8…` (was
  `6a29a5a0…`), recipe blob `86858c14…` (was `7a5e008…`) and the rebuilt
  Windows archive `BFEE5395…` (was `3FC8FBE3…`).
- Why the spec blob moved: the merge surfaced a drift that predated it — the
  language had added four `tests/suite` programs after `docs/IR_SPEC.md` was
  written, so §5.5 still said `67+1` while the tree held `71+1`, and R6's
  emitter self-test asserted the spec's number (`alive_check` went red on the
  merged tree). The user chose to reconcile upward (§5.5 and
  `scripts/emit_ir_corpus.py` to `93+1`) over keeping the spec as the pin's
  frozen snapshot.
- Alternatives: keeping the language commit at `5c01cea` and only recording the
  merged emitter (rejected: incoherent once the emitter lives in `main`, and the
  corpus/emitter pin would name a commit that is not the tree measured);
  re-pinning without rebuilding the runtime archive (rejected: the runtime
  source moved, so the pin would name `299eb09` while linking a `5c01cea`
  archive, which `PIN.md` forbids); keeping the corpus at `89+1` by excluding
  the four new tests from discovery (rejected: the corpus is the discovered test
  tree, not a hand-picked set — D-M1-17).
- Consequence: the emitter pin is now reproducible from the user's own
  repository (`299eb09` is on `main`), so the local-only caveat of D-F0-09 no
  longer applies; **nothing was pushed** to any remote. The previous pin's
  measurements stay in `M1_REPORT.md` and `tests/corpus/README.md` as the
  `1e6a378` record; the corpus re-measurement at `299eb09` is `7/93` (from
  `6/89`), a ratchet increase.
- Evidence: merged tree clean at `299eb09` (`dev.py all` **21/21**, canonical
  re-promoted to `15341D24…`, `emit_ir_corpus.py --self-test` **20/20**, real
  emission **94/94**, `filesSha256 0106d531…`); R6 differential **31/31**
  against the merged oracle and the rebuilt archive; loader suite **137/137**;
  `make_cases.py --check` no drift; `migrate-m1-fixtures.py --check` 20/20;
  corpus `run_corpus.py --check` ratchet `r6BuildOk 7 -> 7`, `match 7 -> 7`.
- Status: **accepted by user 2026-10-16**.

## D-F0-11 — Re-pin to `main` `862f257` (H-ARENA, Oracle removal, loader fix, `compiler/` reorg): all IR-neutral

- Date: 2026-10-17.
- Decision: the language `main` moved four commits past the D-F0-10 pin and R6
  re-pins once, after the last of them: `e75f2c7` (H-ARENA: the ledger's memory
  columns measure the request, not a prediction), `8b9107a` (remove the Arena
  Oracle, whose last consumer was the ledger), `0a145cc` (loader: identify a
  module by its file, not by the spelling that reached it) and `862f257` (split
  `compiler/` by stage into `front/ ir/ tir/ emit/ host/ tools/`). `PIN.md`
  re-pins to `862f257352f5bb7f5ce57c12d387a3ff8bc65f40`: the spec blob
  `371f873e…` (was `16f26a9d…`; path mentions only), the emitter fixed point
  rebuilt in place (`orbit-fp-26cfb87400.exe`, sha256 `10a16956…`), the corpus
  with `languageCommit 862f257` and `filesSha256 0106d531…` **unchanged**, the
  self-host canonical `PUBLISHED_C 9906A6F7…` (through `ACB0245D…` at H-ARENA),
  and the runtime rows: `runtime/orbit_api.h` blob `bf098c54…` (was
  `27fc52b8…`), `runtime/runtime.h` blob `ffbd6a94…` (new row), recipe blob
  `86858c14…` (unchanged), Windows archive rebuilt from the pinned recipe.
- Why one pin and not several: H-ARENA, the Oracle removal and the reorg are all
  class (ii) for R6 — emission, runtime and file layout, no IR change — so
  pinning after each would have moved `PIN.md` repeatedly for the same corpus.
  The pin is taken after the reorg, the last commit of the tramo.
- Why the corpus did not move: measured, not assumed. R6 regenerated the corpus
  with its own fixed point at `299eb09`, `8b9107a` and `862f257`; all three give
  `filesSha256 0106d531…` (94 documents, 5419454 bytes). The literal operands
  found in binary ops are produced by the IRBuilder (`builder.orb`
  `buildCallArgValue`), not by `ir_emit.orb`, so moving the emitter file into
  `compiler/emit/` cannot change the emitted bytes — and it did not.
- Not purely additive in the runtime: `orbit_ledger_exit` gained a
  `const OrbitArena*` parameter and two arena accessors
  (`orbit_arena_active_bytes`, `orbit_arena_overflowed`) were added, and
  `runtime/oracle.c` + `runtime/oracle.h` were deleted. R6 emits no ledger call,
  so the differential is unaffected; the rows are recorded because the pin names
  a header the R6 archive compiles against.
- Finding (pin discipline): **no SHA-256 of a locally built binary is a stable
  anchor.** Both the emitter binary and the runtime archive/member object embed
  the absolute directory they were built in (measured: `…\runtime/json.c` is a
  literal string inside `orbit_lib.o`, and the fixed point contains its worktree
  path), and `ar` adds container metadata. Two R6 rebuilds of the archive in the
  same directory agree on the member object and disagree on the archive;
  rebuilds in different directories disagree on both, while the **324 defined
  symbols** are identical. `PIN.md` now anchors on commit + canonical +
  recipe/source blobs + corpus `filesSha256` + symbol set, and marks artifact
  hashes as observations of one build. The archive hash recorded by D-F0-10
  (`BFEE5395…`) was the same kind of observation.
- Alternatives: re-pinning at `8b9107a` and again after the reorg (rejected: two
  pins for one corpus, and one pin was what the user asked for); adopting the
  language's own archive hash `2ffe2301…` as the pin (rejected: it is a
  build-site observation, and R6 must link an archive built from the pinned
  recipe); keeping the `299eb09` fixed point because "behaviour is the same"
  (rejected: the pin must name the tree it was built from, and the documented
  recipe points at `orbit-fp-26cfb87400.exe`).
- Consequence: the differential oracle and the corpus emitter are `862f257`; the
  runtime archive in `.r6-build` was rebuilt from it. **Nothing was pushed** to
  any remote (R6 has none configured; the language `main` is 16 commits ahead of
  `origin`). D-F0-10 remains the record of the merge pin.
- Evidence: R6's own fixed point, built from `862f257` in a fresh worktree
  (`orbit-verify-862f257`), converges to `9906A6F7…`, equal to the language's
  `PUBLISHED_C`; the corpus regenerated at `862f257` gives
  `filesSha256 0106d531…`; differential **31/31** against that oracle; loader
  **137/137**; `make_cases.py --check` no drift; corpus `run_corpus.py --check`
  ratchet `r6BuildOk 7 -> 7`, `match 7 -> 7`; the runtime archive rebuilt from
  the pinned recipe reports 324 defined symbols with `orbit_print`,
  `orbit_alloc`, `orbit_result_ok` and `orbit_string_concat` present and
  `orbit_handle_request` absent.
- Status: **applied 2026-10-17** (re-pin requested by the user).

## D-M1-21 - The inline literal operand: materialized into a frame scratch slot

- Date: 2026-10-17. Authorized gap: the "measured next wall" D-M1-20 names
  (the literal operand inside a binary operation), spec §2.3.
- Decision: `add`/`sub`/`mul`/`div`/`mod`/`eq`/`ne`/`lt`/`le`/`gt`/`ge` accept an
  inline literal as operand1 and/or operand2. Admitted kinds: `Int`, `String`,
  `Bool`. The per-opcode **type rule is unchanged**: the literal is typed by the
  same `value_type` a register goes through, so a mismatched literal type is
  still `CGEN` (a `String` literal in `sub`, an `Int` literal in `eq` against a
  string register, ...). Everything else keeps failing closed: an inline
  `Float` gets a dedicated `CGEN`, a literal in an operation outside the eleven
  keeps the `operand_reg` refusal, and a register with no type keeps its own.
- Why those kinds: measured, not assumed. The pinned corpus carries **2661
  inline-literal operand sites across 79/94 documents**, kinds `Int` 1692,
  `String` 572, `Bool` 397, and **no `Float` at all**. The builder inlines
  whatever value it already holds (`builder.orb` `buildCallArgValue`), and a
  float value reaches a binary op as a register, because the emitter renders
  float literals through `load_const`/`decl_var`. A `Float` immediate therefore
  has no producer in this position and stays refused rather than generalised.
- Why those opcodes: the same census - the eleven are exactly the operations
  that carry an inline literal in the corpus, and exactly the operations R6
  implements with both operands as values. `and_op`/`or_op`/`not_op`/`neg`
  carry none (the builder folds or materialises those operands), so they keep
  failing closed through `operand_reg`.
- Reference (normative, the oracle's own rule): the pinned C backend types an
  inline literal exactly like a register. `irVerifierOperandType`
  (`compiler/emit/c_backend.orb:1176-1198`) maps `IRValue.Int` -> `"int"`,
  `Float` -> `"float"`, `Bool` -> `"bool"`, `String` -> `"string"` (the mirror
  R6's `value_type` already implemented for D-M1-20); `isStringOperand`
  (`:3887`) treats an inline `IRValue.String` as a string operand;
  `verifyarithBinop` (`:1323`) and `verifycmpBinop` (`:1356`) type both operands
  through it; `add`'s inference (`:804`) and its emit (`:1051`) test
  `isStringOperand` on both sides. R6 mirrors that rule; it invents nothing.
- Mechanism: two 8-byte immediate slots below the result area. The bytes the
  list scratch and the result cells reserve are factored out of the frame
  arithmetic (`result_area_bytes()`), so `used` grows by 16 only when a
  function actually needs the slots and **no existing offset moves**;
  `literal_scratch_offset()` places them at `slots * 8 + result_area + 8 * (n + 1)`
  (above every cell, below nothing else in use). `materialize_literals()` writes
  the operand with the same materialization `load_const`/`decl_var` already use
  (`movq $imm` for `Int`, `movl $0/1` for `Bool`, `emit_string` + `leaq .LCr6_*`
  for `String`) and `operand_slot()` then returns either the register's slot or
  the scratch slot, so every operation stays a plain slot read. Validation swaps
  `operand_reg` for `operand_value` in those eleven cases only.
- Measured effect (pinned corpus `0106d531`, oracle `orbit-fp-26cfb87400.exe`):
  first refusals whose opcode is one of the eleven fall **24 -> 6**, and the 6
  are not the literal form: four are the D-M1-20 residue (a register operand or
  destination still `unknown`: `for_index_labels`, `integer_literals`,
  `interpolation`, `if_expression_value`), one is `peephole_folds`'s
  `add` of a `String` literal with an `Int` literal - the language's
  **string-spreading** conversion, which R6 does not implement - and one is
  `list_element_type`'s `ne` over a bool, which R6 refuses by rule.
  `r6BuildOk` **7 -> 21** and `match` **7 -> 21**: `arith`, `bool_and_or`,
  `compound_assignment`, `div_mod_signed`, `division`, `for_range`, `if_branch`,
  `model_mutation`, `numeric_literals`, `peephole_int_bool_only`, `strings_eq`,
  `ternary`, `while_break`, `while_counter` go from refusal to a byte-for-byte
  coincidence with the oracle. **Zero documents regressed** (0 lost).
- Measured next fronts (questions to the language, not admissions): the
  string-spreading `add` (what text does the oracle produce for the non-string
  side?) and `ne` over `bool` (the reference accepts it; R6's `ne` rule admits
  int/float/string only). Both are reported through the channel and neither is
  implementable in R6 without a rule.
- Gate evidence: differential **31/31 -> 32/32** (fixture 32 is the pinned
  emitter's own stream for its paired `.orb`, `unknown` slots included, exit 42
  and stdout `lit-ok\r\n\r\n`); loader **137/137 -> 142/142** (116 rejections +
  26 controls); `make_cases.py --check` no drift (135 case files + manifest);
  `migrate-m1-fixtures.py --check` 20/20 (32 skips, newer than `06d7d2c`); the
  corpus ratchet is re-taken in `tests/corpus/baseline.json` (`r6BuildOk 21`,
  `match 21`).
- Negative controls (the form admits nothing beyond the measurement):
  `x-lit-float-operand` (an inline `Float` is refused),
  `x-lit-and-op-operand` (a literal outside the eleven operations is refused),
  `x-lit-type-mismatch` (admitting the form does not admit a mismatched type).
  Positive control `ok-lit-operands` (exit 5): a frame that reserves the list
  result scratch and the immediate slots at the same time, which is the
  offset arithmetic this decision introduces.
- Status: **accepted**.

## D-F0-12 - Re-pin to the language `main` at `ce87d508` (five commits, runtime-only)

- Date: 2026-10-17. Requested by the user (the pin moves as its own dated
  round, never inside a lowering round).
- Decision: the pin moves from `862f257` to
  `ce87d508e8bdb7a1a0059726d3f45ecc73f8f5e3` (`main`, not pushed, not in any
  remote). Tramo: `8878225` (runtime: hold the hot retention at the request's
  footprint, not a fixed 256 KB) -> `5cfc12b` / `8f7acbf` (perf docs) ->
  `5ea4366` / `ce87d50` (C-cache harness fixes).
- What did **not** move, measured at the new commit:
  - `compiler/` is untouched by the tramo (`git diff --name-only 862f257..ce87d508
    -- compiler/` is empty), so the fixed-point binary is **byte-identical**:
    `C:\Temp\orbit-fp-26cfb87400.exe`, sha256 `10a16956…`, unchanged, and the
    oracle of the differential and the corpus is the same binary D-F0-11 pinned.
    The canonical is unchanged too: `compiler/selfhost/stage3.exe.c` at HEAD has
    sha256 `9906A6F7…`, equal to `PUBLISHED_C`.
  - The emitted corpus is **byte-identical**: a full regenerate-and-verify run
    gives `filesSha256 0106d531cc10f2edb02097db2babdef4291f845a03be85dcfee47c3d91b5bfb9`
    (94 documents, 5419454 bytes) with every per-file `sha256` matching
    `tests/corpus/manifest.json`; oracle `93/93` build and run.
  - Every previously pinned blob is unchanged: `docs/IR_SPEC.md 371f873e…`,
    `docs/architecture/TYPED_IR.md c5558d55…`, `runtime/orbit_api.h bf098c54…`,
    `runtime/runtime.h ffbd6a94…`, `scripts/mklib_runtime.py 86858c14…`.
- What moved: `runtime/arena.c` (`f9a31e5c…`, +26/-3: the hot retention follows
  the request's footprint instead of a fixed 256 KB) and the harness
  (`scripts/orbit_ccache.py b14f0df7…`, `scripts/alive_check.py 3fb63b8c…`,
  `scripts/runtime_c_tests.py cb5e2f6a…`). The runtime archive was rebuilt from
  the new tree with the pinned recipe: one member, **324 defined symbols, an
  identical set in both directions** against the `862f257` build, with
  `orbit_print`, `orbit_alloc`, `orbit_result_ok`, `orbit_string_concat` present
  and `orbit_handle_request` absent. Its sha256 `86e63f0d…` is an *observation*
  of this build (the previous one was `6ebfc05d…`); per D-F0-11 no archive hash
  is an anchor.
- New blob pins recorded (named by path before, cited by line by decisions):
  `compiler/emit/c_backend.orb 2d64c8de…` (the normative reference D-M1-19/
  D-M1-20/D-M1-21 cite), `compiler/emit/ir_emit.orb 54f0601b…`,
  `scripts/emit_ir_corpus.py 057887fd…`.
- Observation, recorded and deliberately **not** part of the pin: the
  checkout's working tree is dirty while this re-pin is taken
  (`M scripts/_common.py`, `M scripts/alive_check.py`, `?? scripts/cc_cached.py`
  — a concurrent session's C-cache work, all class (ii) `scripts/**`, none of it
  under `compiler/`). The pin rests on the commit, and `git show ce87d508:<path>`
  yields the pinned bytes, so the dirty harness does not enter it. It did break
  the oracle build mid-round, though: an `ORBIT_CC` wrapper spelled as two quoted
  tokens, which the pinned fixed point cannot spawn (Windows
  `ERROR_INVALID_NAME`), produced whole-corpus `oracle-fail` runs with
  `r6BuildOk 0`. The ratchet caught exactly that and refused to write a baseline.
  The D-M1-21 measurement and the gates below were taken with `ORBIT_CCACHE=0`,
  the harness's own single-run escape hatch.
- Gates re-run on the new pin with the rebuilt runtime archive: differential
  **32/32**; loader **142/142** (116 rejections + 26 controls);
  `make_cases.py --check` no drift; `migrate-m1-fixtures.py --check` 20/20;
  corpus ratchet **holds 21 -> 21** (`r6BuildOk` and `match` unchanged, oracle
  green).
- Status: **applied**.

## D-M1-22 - `load_var` of a parameter: the value of its `arg` binding

- Date: 2026-10-17. Authorized gap: the measured next wall. After D-M1-21 the
  largest first refusal of the pinned corpus is `load_var` (**23 of 93
  programs**), and all 23 read a **parameter by name**: the pinned builder
  binds each parameter with `arg` and reads it in the body with
  `load_var <name>`.
- Decision: `load_var` whose O1 names a **parameter of the enclosing function**
  is admitted. The destination is typed `paramTypes[p]` and the lowered value is
  the parameter itself - the same slot that parameter's `arg` binding reads.
  Name resolution inside a function is, in this order: **parameter**, then
  `decl_var` local, then declared global, then union tag constant. Everything
  else is unchanged: a name that is none of those keeps its refusal; a name that
  is a parameter of *another* function keeps it (parameters are function
  scoped, they are not module names); and a destination whose type differs from
  `paramTypes[p]` keeps it.
- Reference (normative, the oracle's own rule), all read from the pinned blob
  `compiler/emit/c_backend.orb 0c9dd786…` at `17ed624` (re-anchored from
  `2d64c8de…` at `ce87d508` by D-F0-13):
  - the emission of `load_var` (`:2600-2653`) takes the name text and, when the
    variable's own type is unknown or empty, **falls back to `func.params` and
    takes `paramTypes[p]`** (`:2625-2633`); the cast that follows is chosen from
    that type (`:2635-2651`), and a `Sym` renders as the name verbatim
    (`generateValue`, `:IRValue.Sym -> v.value`). So a `load_var` of a parameter
    is its own C variable, cast by `paramTypes[p]`.
  - the type inference (`:777-802`) consults `func.params` **before** the
    locals and infers `params.get(p) == vn -> paramTypes.get(p)`; that is the
    pass that decides the register type, and the order R6 mirrors.
  - the `arg` binding (`:2862-2875`) emits `reg[dest] = params[index]`, i.e. the
    same value, so the two instructions are the same source.
  - `docs/IR_SPEC.md` (blob `371f873e…`) §1.2 states the same rule: `load_var`
    is `D <- variable named by O1`, and `arg` with `D` is `Int O1 is a parameter
    index, D <- parameter name`. Its own internal line cites are against an older
    revision of the backend than the pinned blob, so the pinned lines above are
    what is auditable here.
- Why the whole family at once: measured, not assumed. Decoding every one of the
  23 first refusals at its diagnostic offset (R6 reports the byte offset into
  the single-line IR JSON) gives **23/23 parameter reads** and nothing else:
  parameter types `int` 16, `string` 5, `float` 1, `bool` 1, across 14 distinct
  functions (`intToString` 8, `getEnv` 2, `addSeconds` 2, and ten singletons).
  There is no other `load_var` form behind the wall, so one rule clears it.
- Mechanism: R6 already spills parameter `p` to its home slot
  `(registerCount + isMain + p + 1) * 8` in the prologue and reads that slot for
  the `arg` binding, and its inference already resolved the name
  (`param_info` is now the single definition of that lookup, extracted from the
  inference pass). The change is therefore a param branch in the `load_var`
  validation and a param branch in its emission that copies the home slot into
  the destination (the `movq`/`movsd` pair the `arg` binding already uses). No
  frame arithmetic changes and no other offset moves.
- Measured effect (pinned corpus `0106d531`, oracle `orbit-fp-26cfb87400.exe`):
  first refusals whose opcode is `load_var` fall **23 -> 0**; `r6BuildOk` and
  `match` go **21 -> 23**. `tests/suite/float_boundaries.orb` (float parameter)
  and `tests/suite/fn_recursive.orb` (int parameter) go from refusal to a
  byte-for-byte coincidence with the oracle. **Zero documents regressed**. The
  remaining 21 of those 23 programs advance to their next wall, which is what the
  new histogram measures; the walls are conjunctive, so clearing one family does
  not by itself clear a program.
- Gate evidence: differential **33/33** (fixture 33 is the pinned emitter's own
  stream for its paired `.orb`, reading int/string/bool/float parameters by
  name, exit 42, stdout empty); loader **146/146** (116 rejections + 30
  controls, +2 rejections and +3 positive controls of D-M1-22);
  `make_cases.py --check` no drift (138 case files + manifest);
  `migrate-m1-fixtures.py --check` 20/20; the corpus ratchet is re-taken in
  `tests/corpus/baseline.json` (`r6BuildOk 23`, `match 23`).
- Negative controls: `x-load-var-foreign-param` (a name that is a parameter of
  another function, not of the enclosing one, keeps the refusal - parameters are
  not module names) and `x-load-var-param-mismatch` (loading a parameter into a
  differently typed register keeps it). Positive control `ok-load-var-param`
  (exit 42): a body that never reads the bound register still sees the argument,
  which is the equivalence this decision rests on. The pre-existing
  `x-infer-unknown-load-var` (an undeclared name stays unknown) still holds.
- Diagnostic wording: the message becomes `load_var requires a supported local,
  parameter, declared global, or union tag constant` so it names the source it
  now admits. The rejection code (`CGEN`) and the fail-closed contract are
  unchanged; the historical message quoted in the D-M1-20/21 measurements is
  left as that run's record.
- Status: **accepted**.

## D-M1-23 - The string `add`: spread on one side, concat on both, and the non-main arena

- Date: 2026-10-18. Round 4 of the corpus backlog (`p4`). Chosen by the
  measurement, not by taste: after D-M1-22 the largest *actionable* first
  refusal was the `add` of `String` + `Int` (`peephole_folds`'s rule) plus the
  `string add outside main` refusal that capped `interpolation`,
  `integer_literals` and `function_pointers` (7 of 7 `add` refusals).
- Admission: `add` with a `string` on **exactly one** side now spreads the
  other operand (an `int`, `float` or `bool` scalar) through the runtime
  converter and concatenates; and `add` of two strings is now lowered in
  **every** function, not only in `main`.
- Reference (pin `17ed624`, read with `git cat-file blob`, never from the
  working tree):
  - `compiler/emit/c_backend.orb:2664-2670` - the `IROpcode.add` arm: if either
    operand is a string operand, both sides go through
    `stringifyConcatOperand` and the result is
    `orbit_string_concat(arena, s1, s2)`.
  - `:4347-4384` - `stringifyConcatOperand`: a string operand is used raw, any
    other operand is spread by its inferred type.
  - `:3887-3914` - `isStringOperand` decides "is a string" from
    `registerTypes`/`strRegs`/`strVars`, which is the type R6's own `type_at`
    already carries.
  - `:193` and `:2227` give the prototype and the definition of the emitted
    `orbit_main`, whose first ABI parameter is `OrbitArena* arena`. At
    `ce87d508` the header also declared a file-scope `extern OrbitArena*
    arena;` (`:387`) and `orbit_main` wrote `arena = _init_arena;` (`:2242`);
    `17ed624` deleted both lines and every emitted function takes
    `OrbitArena* arena` as its first parameter. R6 keeps its arena in the
    `r6_arena` slot and passes it explicitly, so its programs do not read a
    global.
  - `runtime/selfhost.c:170` - `orbit_int_to_string_selfhost(orbit_int)` is
    arena-free (it uses `orbit_arena_get_global()`); `runtime/collections.c:716`
    `orbit_int_to_string(arena, v)`, `:730` `orbit_float_to_string(arena, v)`,
    `:815` `orbit_bool_to_string(arena, v)`, `:831`
    `orbit_string_concat(arena, a, b)`; declarations in
    `runtime/orbit_api.h:388-391`.
- Semantics admitted: the spread is emitted with the converter the pinned
  backend's `stringifyConcatOperand` picks for the operand's inferred type -
  `int` -> `orbit_int_to_string_selfhost` (value in RCX), `bool` ->
  `orbit_bool_to_string` (arena RCX, value RDX), `float` ->
  `orbit_float_to_string` (arena RCX, value XMM1) - matching the ABI the earlier
  rounds already pinned. The spread result lands in one of two 8-byte slots
  placed below the D-M1-21 immediate slots, so no existing offset moves and no
  register, local or scratch block is overlapped.
- Fail closed: refused with `CGEN`
  - `add spread requires a string destination and an int, float, or bool scalar operand`
    - a spread whose scalar side is a pointer, list or model (no measured
      converter) and a non-string destination;
  - `string add requires a string destination`;
  - matching int/float operands keep their previous rule; an inline `Float`
    literal on a spread side stays refused (the builder never emits one there).
- Negative controls (`tests/reject/`): rejects `x-add-spread-dest`,
  `x-add-spread-pointer`, `x-add-spread-float-literal`, `x-add-string-dest`;
  acceptance `ok-add-string-spread` (a non-main int spread and a non-main bool
  spread, exit 42); fixture `34-string-spreading-add` (`.orb`/`.json`, emitted
  with the pinned emitter, exit 42 / `[spread]\r\n`). The single message above
  covers both clauses, so each control is built to trip exactly one:
  `x-add-spread-dest` keeps a valid int scalar and declares no string
  destination, and `x-add-spread-pointer` declares a **string** destination
  (`["string", "string", "void*"]`), so the first clause is provably satisfied
  there and the scalar guard is the only clause that can refuse it - while
  `ok-add-string-spread` shows the same string destination accepting an int and
  a bool spread.
- Effect: 23/93 -> 27/93, `r6BuildOk` 23 -> 27 and `match` 23 -> 27 (the string
  fix plus D-M1-24/D-M1-25). Four programs cross to a byte-identical match:
  `interpolation`, `integer_literals` and `function_pointers` through this
  admission, and `for_index_labels` through D-M1-24/D-M1-25 (it still needs this
  admission to build). The `add` first-refusal wall falls 7 -> 0.
- Status: **applied**.

## D-M1-24 - list read labels: the id carries the function ordinal

- Date: 2026-10-18. Found while measuring the round-4 result (D-M1-23), and
  **pre-existing**: present in the working tree at `432c205`, introduced with
  the list opcodes and never exercised, because no fixture and no corpus
  program reached the assembler through it until round 4 cleared the wall in
  front of `for_index_labels`.
- Symptom: a module with a `list_get`/`list_pop` in a **non-main** function
  fails at assembly with `error[ASSEMBLER]: duplicate assembler label
  '.LCr6_0'`.
- Root cause: the private label id `string_id` is initialised once per function
  as `((unsigned long long)fn_index) << 32` (`win64.c:1635`), so the **function
  ordinal lives in the high 32 bits** and the low 32 bits are the per-function
  string ordinal. Every other consumer keeps all 64 bits (`emit_string` takes
  `unsigned long long`, `materialize_literals` takes `unsigned long long *`,
  `arg_string_ids` is `unsigned long long[256]`), but the two list-read sites
  stored the id in 32-bit locals, `unsigned where_id = string_id++;`,
  truncating the function ordinal: every function emitted `.LCr6_0`,
  `.LCr6_1`, ... from zero, so the second function to read a list collided with
  the first. A module that only reads lists in `main` never collides (the
  ordinal stays 0 and is defined once), which is why the fixtures 19/20 and the
  whole M1 corpus before round 4 never exposed it.
- Fix: `emit_list_read`'s `where_id`/`file_id` parameters and the two call
  sites (`list_get`, `list_pop`) widen to `unsigned long long`. No other
  truncation remains (audited: no `unsigned <x> = string_id` left in the file).
- Reference: this is an R6-internal contract, not a language admission - a
  private assembler label must be defined once. The evidence is the emitted
  assembly carrying two `.LCr6_0:` definitions and the assembler's own refusal;
  the 64-bit convention the neighbouring code already follows is the contract
  that was violated, so no spec line applies.
- Effect: `for_index_labels`'s duplicate-label refusal disappears when the
  program reaches the assembler (measured in the D-M1-23 round). No other
  program changes: the label **set** only gains the high bits it lost, so labels
  become unique - never fewer, never different in kind. No frame arithmetic,
  offset or opcode path is touched.
- Status: **applied**.

## D-M1-25 - `list_len`: the length slot is written 64-bit

- Date: 2026-10-18. Found while measuring the round-4 result, and
  **pre-existing**: present in the working tree at `432c205`, introduced with
  the list opcodes. The label fix (D-M1-24) is what let the corpus program
  `for_index_labels` compile, exposing it.
- Symptom: `for x, i in xs` over-runs its bound. R6 built the program and the
  runtime aborted inside the element read with `orbit: list index out of range /
  index 3 / length 3 / elements int / in index_binding`; the oracle returns 0.
- Root cause: `R6_OP_LIST_LEN` stored the length with a 32-bit `movl` (and the
  null branch with `movl $0`) into an `int` slot, while every consumer of an
  `int` slot reads all eight bytes - `lt`/`le`/`gt`/`ge` compare with `cmpq`
  (the int arm of the relational emission), `eq`/`ne` likewise. `OrbitList.len`
  is a `size_t` (`runtime/orbit_api.h:219`) and `orbit_list_len` returns
  `size_t` (`runtime/orbit_api.h:519-521`), so the eight-byte load the emission
  already used is right and only the store was narrow: the high half of the slot
  kept whatever the frame last held, and `i < len` compared `i` against that
  garbage bound.
- Fix: the non-null branch stores with `movq` (the value the `movq 8(%rax)`
  load already produced) and the null branch with `movq $0`. No other 32-bit
  store targets an `int` slot: the remaining ones (bool literals, bool locals,
  comparison results, `result_is_ok`) all feed `cmpl`, which reads four bytes,
  and every `int` producer is already 64-bit (`movq`/`addq`/`movslq`).
- Reference: the pinned runtime's own types - `OrbitList.len` is `size_t`
  (`orbit_api.h:219`), `orbit_list_len` returns `size_t` (`:519-521`) - and R6's
  int convention (a 64-bit slot, which `load_const`, `add` and the `list_get`
  element decode already assume). No spec line applies; the invariant is that a
  slot has one width.
- Regression: fixture `35-list-len-bound` (a list-length-bounded indexed loop in
  a **non-main** function, with no builtin call and no string `add`) is exit 42
  on both sides; it also pins D-M1-24.
- Effect: `for_index_labels` matches the oracle (exit 0). Nothing else moves: the
  fix only stops discarding the high half of a correct 64-bit load, so it cannot
  turn a refusal into a build nor change a value that was already right.
- Status: **applied**.

## D-M1-26 - The builtin `print`: one line per call, destination never stored

- Date: 2026-10-19. Round 5 (`p5`) of the corpus backlog. Chosen by the
  measurement, not by taste: `print` is the most frequent call target in the
  corpus (134 call sites across 32/93 programs) and the first refusal of the
  nine programs whose remaining wall was a call to it.
- Admission: a `call` whose target is the **String** `print` is the builtin, not
  a declared function. The destination is never stored; exactly one pending
  argument prints one line, selected by the argument's class; any other arity is
  the pinned no-op and consumes every pending argument.
- Reference (pin `17ed624`, read with `git cat-file blob`):
  - `compiler/emit/c_backend.orb:3917` - `feBuildTimeCallType` types `print`
    `void`.
  - `:2977-3005` - the lowering dispatches on `calleeName == "print"` **before**
    resolving any declaration: one argument is a string through
    `print("%s", arg)`, a float goes through
    `orbit_float_to_string(arena, v)` and `printf("%s\n", ...)`, and any other
    argument is `printf("%lld\n", (long long)(arg))`.
  - `:2999-3004` - any other arity pops **every** pending argument and emits
    only an empty statement.
  - `:3887-3914` and `isFloatArgReg` - the class that selects the reduction is
    the register type the builder already recorded, not the spelling of the
    operand.
  - `runtime/orbit_api.h:293` - `print(...)` is a macro over `printf` plus a
    newline and an `fflush`; no `print` symbol exists, so a call emitted to that
    name is the LINK failure the earlier measurement recorded.
- Semantics admitted: `int`/`bool` -> `%lld`, `string` -> `%s`, `float` ->
  `orbit_float_to_string` (arena in RCX, value in XMM1) then `%s`. Inline
  literals are admitted in the classes the builder emits in this position (Int,
  String, Float, Bool); `arg_string_ids` already carries the label of an inline
  String. The destination register is admitted but never written: the builder
  types it `int` from a declared `extern fn print(...)` in three corpus programs
  (8 call sites) and `unknown` elsewhere, and no corpus program reads it after
  the call (audited).
- The interception is by name, before the declared-function lookup, exactly like
  the pinned lowering. `extern fn print(string) -> int` therefore does not
  shadow the builtin, which is what those three corpus programs rely on.
- Fail closed: the admission is one name in one form.
  - `x-builtin-name-outside-print`: `println` is still an unknown target.
  - `x-print-symbol-target`: a `Sym("print")` target is not the builtin (the
    builder emits String targets), so it stays a declared-function lookup.
  - The structural arity checks are the loader's (V-13/E-05); the `v13-arity-*`
    cases already pin them.
- Controls: fixture `36-print-builtin` (string/int/float in main, exit 0,
  `hello\r\n42\r\n1.5\r\n`), `ok-print-noop` (arity 0 and arity 2 with two
  pending arguments, exit 7, no output), `ok-print-inline-literals`,
  `ok-print-nonmain-float` (the arena outside `main`) and
  `ok-print-shadowed-extern` (an `extern fn print` returning `int`, destination
  never stored).
- Effect: with this admission the eight programs in the round-5 report cleared
  the unknown-register wall. Three of them (`test_assert`, `test_convert`,
  `test_hash`) then built but did not match, which is how D-M1-27 was found.
- Status: **applied**.

## D-M1-27 - Arena-injected runtime externs: the arena is the first C argument

- Date: 2026-10-19. Round 5 (`p5`), found by D-M1-26's own measurement:
  `test_assert`, `test_convert` and `test_hash` built for the first time and
  then faulted (`0xC0000005`) or printed `assert: zero -- got "", want "0"`,
  because R6 called `orbit_int_to_string` with the IR arity while its C
  signature takes the arena first.
- Symptom: `orbit_int_to_string(0)` returns `""`; `orbit_sha256_hex` and
  `orbit_hmac_sha256_base64url` fault. The IR declares `extern fn
  orbit_int_to_string(v: int) -> string` (one parameter); the runtime declares
  `orbit_string orbit_int_to_string(OrbitArena* arena, orbit_int value)`
  (`runtime/orbit_api.h:388`).
- Root cause: the pinned C backend prepends `arena` when the callee takes one
  (`c_backend.orb:3097-3104`). At `ce87d508`, where this decision measured it,
  that was exactly the extern family `functionNeedsArena` enumerates
  (`:4416-4437`); at `17ed624` `calleeTakesArena` also answers true for every
  non-extern function the module emits (`:4447-4458`). R6 emitted the IR
  arguments from slot 0, so the callee read the integer as the arena pointer and
  the next register as the value.
- Admission: a call to a **declared extern** whose name is in that enumerated
  family loads the executable arena into RCX and shifts every IR argument one
  position. The shift is positional for both register files: Win64 assigns
  argument *i* to the *i*-th integer and *i*-th SSE register, which
  `orbit_float_to_string(arena, v)` pins (the value rides XMM1).
- Reference: `c_backend.orb:4416-4437` (`functionNeedsArena`, copied verbatim),
  `:3097-3104` (the prepend); `runtime/orbit_api.h:388-389` (the two
  conversions) and `runtime/collections.c:716`/`:730` (their definitions take
  the arena). At `ce87d508` the arena was also a file-scope global of the
  generated header (`:387`), the one D-M1-23 mirrors; `17ed624` deleted that
  line and every emitted function takes `OrbitArena* arena`, so a call outside
  `main` passes the caller's arena instead of setting a module-wide flag.
- Fail closed: the injection requires the name **and** a declared extern. A bare
  String target with that spelling stays an unknown target
  (`x-arena-extern-undeclared`). A local function cannot carry an arena-family
  name as a control: the runtime already defines the symbol, so the link
  rejects the module before any injection could be observed - recorded here
  rather than pretended as a case.
- Controls: fixture `37-arena-extern-call` (`orbit_int_to_string(42)` and
  `(-7)` printed, exit 0, `42\r\n-7\r\n`) and `ok-arena-extern-float`
  (`orbit_float_to_string(2.5)`, pinning the XMM1 positional shift).
- Effect: the three programs D-M1-26 exposed now build **and** match the oracle.
  Round 5 closes at **35/93 match, 35/93 build** (from 27/27), with no program
  building-but-differing and no `LINK` refusal left in the corpus; the
  `r6BuildOk`/`match` ratchet is re-taken at 35.
- Status: **applied**.

## D-M1-28 - A void function returns without reading the value its `ret` carries

- Date: 2026-10-19. Round 6 (`p6`). `void_return_void_call` was the whole
  `ret` wall that was not the by-design `result` return: the language spells a
  tail call over a void callee as `return f()`, and the builder carries the
  call's destination register in `operand1`.
- Symptom: R6 refused it with `void ret must not carry a value`, so
  `return target()` in a void function never built.
- Reference (pin `17ed624`, read with `git cat-file blob`):
  - `compiler/emit/c_backend.orb:3435-3445` - the register form of `ret`: when
    the current function's return type is `void` the lowering emits
    `return;` and never reads the register. Its own comment records why: the
    call was already emitted as a statement, so the register names no live
    value and `return (void*)(r_0);` does not even compile.
  - `:3466-3471` - the non-register form shares the same branch and the same
    `return;`.
  - `:3426`, `:3457` - `main` is decided before the declared type, but R6
    already requires `main` to have zero parameters and an `int` return
    (`win64.c:779-780`), so the `void` arm is unreachable for `main`.
- Admission: a `ret` in a function typed `void` accepts any `operand1` and
  ignores it: the emitter loads nothing and emits `leave; ret`. The value is
  not type-checked and its register is not required to exist, because the pin
  never reads either.
- Fail closed: the non-void direction is untouched. A value of another type is
  still `ret value does not match function return type` (`x-ret-type-mismatch`),
  a register that does not exist is still `ret register is out of range`, and
  returning a `result` is still refused. A `void main` was already refused, so
  the admission cannot swallow the `isMainFuncName` branch of the pin.
- Controls: fixture `38-void-ret-value` (a `return target()` tail call chained
  through two void functions plus the statement form, exit 42, `T\r\nT\r\n`)
  and `ok-void-ret-value` (hand-built: the `ret` carries the `int` destination
  of the void `print` call, which would be a mismatch if it were read).
- Effect: `void_return_void_call` matches the oracle; the `ret` first-refusal
  drops **2 -> 1** (the remaining one is the by-design `result` return) and the
  corpus ratchet is re-taken at 36/93.
- Status: **applied**.

## D-M1-29 - The runtime string helpers the builder calls without declaring

- Date: 2026-10-19. Round 7 (`p7`). Two related first refusals, both about the
  same three runtime helpers:
  - `call target orbit_string_len is not a declared function, model, or union
    variant` (5 programs), `... orbit_string_slice ...` (2), `...
    orbit_string_at ...` (2). The builder emits these as `call` with a String
    target and **no declaration** in the module, because the pinned C backend
    sees the prototypes through the runtime headers it includes.
  - `list_len requires a list register and an int destination` (2:
    `strings_concat`, `strings_escape`): `.len()` on a string reaches the
    `list_len` **opcode** when the builder did not know the receiver's type.
- Reference (pin `17ed624`, read with `git cat-file blob`):
  - `runtime/orbit_api.h:385` - `orbit_int orbit_string_at(orbit_string,
    orbit_int)`.
  - `:386` - `orbit_string orbit_string_slice(OrbitArena*, orbit_string,
    orbit_int, orbit_int)`; `orbit_string_slice` is in `functionNeedsArena`
    (`c_backend.orb:4416-4437`), so D-M1-27's table prepends the arena.
  - `:411-413` - `ORBIT_INLINE orbit_int orbit_string_len(orbit_string s) {
    return s ? (orbit_int)strlen(s) : 0; }`. It is a header inline, not an
    exported symbol: `nm libruntime_net.a` lists `orbit_string_at` and
    `orbit_string_slice` as `T` and has no `orbit_string_len` at all, so the
    length cannot be a plain call.
  - `c_backend.orb:3615-3621` - the `list_len` opcode arm asks
    `isStringOperand` first and emits `orbit_string_len((orbit_string)(...))`,
    falling back to `orbit_list_len` only otherwise. Same helper, second door.
  - `:3193-3194` - the call form's argument cast for the same family.
- Admission:
  - A table of three signatures (name, parameters, return type) - `len(string)
    -> int`, `at(string, int) -> int`, `slice(string, int, int) -> string` -
    consulted **only** when the module does not declare the name and **only**
    for the builder's String call target. A Symbol target stays a declaration
    lookup, so the table cannot become an alias surface.
  - The arena, where the C signature has one, comes from D-M1-27's existing
    name table, so `slice` gets it in `%rcx` and every IR argument shifts.
  - `orbit_string_len` is lowered inline to the header's own body
    (`s ? (orbit_int)strlen(s) : 0`), in the call form and in the `list_len`
    opcode form, because there is no symbol to call.
- Fail closed: an undeclared String name outside the table is still `call
  target ... is not a declared function`; a Symbol target never resolves here;
  the header's arity is enforced; a `list_len` receiver that is neither a list
  nor a string is still refused.
- Controls: fixture `39-string-intrinsics` (`.len()`, `.at()` and `.slice()`
  through parameters, exit 0, `5`/`90`/`bc`), `ok-list-len-string` (hand-built:
  the opcode form on a string register, exit 3), `x-intrinsic-symbol-target`
  (Symbol form stays undeclared), `x-intrinsic-arity` (arity 2 against a
  one-parameter header) and `x-list-len-bool` (a bool receiver is still
  refused).
- Effect: measured in two steps because the second door surfaced only after
  the first opened. The intrinsics alone moved the corpus **36 -> 37**
  (`if_expression_else_if` matches; the nine first-walls are gone, no
  regression), and the `list_len` string arm then flips `strings_concat` and
  `strings_escape` for **39/93 match, 39/93 build**. The ratchet is re-taken
  at 39.
- Status: **applied**.

## D-M1-30 - An empty list literal is created opaque (`elemKind 0`)

- Date: 2026-10-19. Round 8 (`p8`). First refusal: `list_create requires an
  8-byte slot size, a kind in 1..4, a list destination, and an int capacity`
  (5 programs: `list_push`, `list_index_out_of_range`,
  `list_element_type_annotation`, `test_process`, `test_process_exit`). All
  five create with `elemKind 0` and a destination typed `list`: that is the
  builder's empty list literal (`val xs = []`), whose element type is decided
  by the first push, not by the literal itself.
- Reference (pin `17ed624`, `git cat-file blob`):
  - `compiler/emit/c_backend.orb:3586-3591` - the `list_create` arm forwards
    `instr.elemKind` straight to
    `orbit_list_create_typed(arena, sizeof(void*), capacity, kind)`; kind 0 is
    a value it passes through, not a refusal.
  - `runtime/collections.c:170` - `orbit_list_create` itself is
    `orbit_list_create_typed(..., ORBIT_ELEM_OPAQUE)`: opaque is the runtime's
    own starting state, not an invalid element type.
  - `runtime/collections.c:102-116` (header `orbit_api.h:415-423`) -
    `orbit_list_adopt_kind` makes the first typed push or set establish the
    kind, and reports a conflict if a later one disagrees.
  - `runtime/collections.c:118-128` (header `orbit_api.h:424-428`) -
    `orbit_list_expect_kind` accepts an opaque list because "the caller's
    static type is the only account of what is in there".
- Admission: `list_create` accepts `elemKind 0` as well as 1..4. The emitted
  call already forwards the kind, so no lowering changes; the destination must
  still be a list, the slot size 8, and the capacity an int register or
  literal.
- Fail closed: kind 5 (ref) stays refused at the create - the native subset
  has no slot decoder for it - and the opaque **accessor** stays refused:
  `list_get`, `list_set` and `list_push` still require a concrete kind 1..4, so
  a `list_get` with `elemKind 0` and an `unknown` destination stays a `CGEN`
  refusal, as does the `register type unknown` gate behind it. The three
  boundaries are pinned as `x-list-get-kind-opaque`,
  `x-list-set-kind-opaque` and `x-list-push-kind-opaque`; E-03 is the kind 5
  read (`x-list-get-kind-ref`) and is a separate case.
- Supersedes D-M1-16 / E-04 for the create only. That decision read the draft
  spec's edge table ("untyped accessor + void* destination") as one case; the
  corpus shows the builder separates the two: the **create** is
  what an empty literal emits and what the runtime adopts, while the
  **accessor** is the half with no native type. `x-list-create-kind-opaque` is
  therefore reclassified as the accept control `ok-list-create-opaque`, and the
  rejection is re-pinned on kind 5 as `x-list-create-kind-ref`; E-03
  (`x-list-get-kind-ref`) and E-06 (`x-print-two-args`) keep their rejections
  unchanged. No control is deleted without being re-pinned.
- Controls: fixture `40-list-create-opaque` (the `filled` function of
  `tests/suite/list_push.orb`: create with kind 0, push 41 and 7 with kind 2,
  two typed reads; exit 0), `ok-list-create-opaque` (same round trip hand-built,
  exit 41), `x-list-create-kind-ref` (kind 5 still `CGEN`), and
  `x-list-get-kind-opaque`, `x-list-set-kind-opaque`,
  `x-list-push-kind-opaque` for the accessor boundary.
- Effect: corpus **39 -> 41/93** (`list_index_out_of_range` and
  `test_process_exit` match the oracle; the other three create programs advance
  with no regression: two reach `register type unknown` and one reaches a
  `list_push` with `elemKind 0`), and the ratchet is re-taken at 41.
- Status: **applied**.

## D-M1-31 - Comparisons follow the pinned arms (bool operands, word eq/ne)

- Date: 2026-10-20. First refusal: `ne requires matching int/float/string
  operands; relational ops require int/float and bool destination` in three
  programs - `if_expression_value` at `c > 4` (bool against int),
  `list_element_type` at `flags.get(0) != true` (bool against bool) and
  `peephole_folds` at `n == "5"` (int against string).
- Reference (pin `17ed624`, `git cat-file blob`):
  - `compiler/emit/c_backend.orb:2747-2760` (`eq`) and `:2761-2774` (`ne`):
    two strings compare with `strcmp`; either side float promotes both
    operands to `orbit_float`; otherwise the two values compare as
    `uintptr_t`. A bool has no arm of its own - it is one of the words - and
    `n == "5"` reaches the `uintptr_t` arm, which compares 5 with the
    literal's address and answers false.
  - `compiler/emit/c_backend.orb:2783-2814` (`lt`, `le`, `gt`, `ge`): no
    string arm; a float pair compares in double; otherwise `intptr_t`. The
    comment at `:2775-2782` states why the relational ops are signed.
- Admission: `eq`, `ne`, `lt`, `le`, `gt` and `ge` accept `bool` operands. A
  string is a `strcmp` operand only against another string; in `eq`/`ne` a
  string against an int is the `uintptr_t` word compare. The destination
  stays a bool.
- Lowering: a bool slot holds its value in the low byte. `load_const` and the
  comparison results store it with 32-bit writes (`movl`), so the upper half
  can be stale; a bool list read writes all 64 bits. The comparison normalizes
  a bool side with `movzbl` before the compare, which is the pinned C value
  `(uintptr_t)(bool)`. An int side stays in the compare's destination so
  `setl`/`setg` keep their direction.
- Fail closed, measured as negative controls:
  - a float next to an int stays refused (`x-compare-float-int`): the pinned
    arm writes `(orbit_float)(v)` and R6 has no int-to-double conversion, so
    admitting the pair would compare an integer bit pattern with a double.
  - a string in a relational op stays refused (`x-relational-string`): the pin
    has no string arm there, and `intptr_t` on two addresses is not a
    comparison the corpus measures.
  - a bool against a string stays refused: the pin would cast the pointer to
    the word, but no corpus program compares them and the byte normalization
    above has no meaning for a pointer.
- Controls: fixture `41-bool-and-word-comparisons` (bool `gt` against an int,
  bool `ne`/`eq`, int/string `eq`/`ne`, string `ne`, float `ne`; exit 0),
  `ok-compare-bool-word` (`true > 0`, exit 41), `ok-compare-string-word`
  (`5 == "5"` is false, exit 41) and `ok-compare-int-bool` (`1 > false`,
  exit 41).
- Effect: corpus **41 -> 43/93** (`if_expression_value` and `peephole_folds`
  match the oracle; `list_element_type` advances from the `ne` to
  `register type unknown` with no regression), and the ratchet is re-taken at
  43. The first refusals `gt`, `ne` and `eq` drop to zero.
- Status: **applied**.

## D-M1-32 - A list spelling is the list pointer (parameters, returns, registers)

- Date: 2026-10-20. First refusal: `parameter type list<int> is unsupported in
  M1` in `list_param_element_types`, `parameter_type_fallback` and
  `test_time_ns`; `parameter type list<string> is unsupported in M1` in
  `std_lists_generic`, `test_collections` and `test_collections_boxed`; and
  `parameter type list is unsupported in M1` in `call_arg_types` (seven
  programs). `generic_functions` stopped on the neighbouring
  `arg value type is unsupported in M1`, a list passed as a call argument.
- Reference (pin `17ed624`, `git cat-file blob`):
  - `compiler/emit/c_backend.orb:478` answers `OrbitList*` for `list`, `List`
    and `OrbitList*`.
  - `compiler/emit/c_backend.orb:479-482` says why: "A list described together
    with its element type is still a list. The suffix names what is IN it,
    which is a property of the values, not of the pointer, so the C type does
    not change."
  - `compiler/emit/c_backend.orb:483-499` applies that rule to any
    parameterised spelling at any depth, and takes the base to decide:
    `list`/`List` -> `OrbitList*`, `map`/`Map` -> `OrbitMap*`. The comment names
    `map<string, list<int>>` itself.
  - `compiler/emit/c_backend.orb:606-611` reads each element kind through its
    own runtime call (`orbit_list_read_int`, `orbit_list_read_string`, ...),
    so the element type travels with the operation and not with the pointer.
- Admission: a parameter type, a return type and a register type may spell a
  list as `list`, `List`, or `list<...>`/`List<...>` at any depth. `r6_codegen_win64`
  rewrites those spellings to `list` in the three ABI positions before any
  check reads them, freeing the string the module owns. The rewrite runs after
  the module-storage guards, so a module the loader cannot produce still fails
  closed instead of dereferencing an absent function array. A pending `arg` may
  carry a list, and a call passes it as one pointer word. Nothing else is
  rewritten: a model, a union, a result, a function type and a `map` keep their
  spelling.
- Lowering: a list parameter arrives as the first pointer-sized word in its GP
  register; `list_get`, `list_set`, `list_push` and `list_len` dispatch on the
  instruction's `elemKind` exactly as before. The prologue moves a list like
  any non-float word.
- Fail closed, measured as negative controls:
  - a `map<string, int>` parameter stays refused (`x-param-map-spelling`): the
    same parameterised syntax with a different base, and the pin answers
    `OrbitMap*` for it, a type R6 does not lower.
  - an extern parameter stays scalar (`x-extern-list-param`): the pin types an
    extern prototype from the same C map, but R6 lowers externs from the scalar
    subset only, and no corpus extern takes a list. The refusal keeps its
    message, `extern parameter type is unsupported in M1`.
  - a function-value spelling `fn(list)->int` stays refused
    (`x-fn-type-nonscalar`, D-M1-14): the canonicalizer rewrites a whole type
    spelling, never a token inside `fn(...)`.
- Controls: fixture `42-list-parameter-spellings` (`list<int>` parameter,
  `list<string>` parameter, `list<int>` return, a call result typed
  `list<int>`, and a `list<list<int>>` parameter whose body measures its
  length; exit 0), `ok-param-list-spelling` (41 travels in through a
  `list<int>` parameter and back out as the element; exit 41) and
  `ok-list-capital-spelling` (a `List`-typed register created, pushed, and
  measured; exit 41). `x-param-map-spelling` and `x-extern-list-param` are the
  rejections.
- Effect: corpus **43 -> 44/93** match and **43 -> 45/93** build.
  `list_param_element_types` matches the oracle. `test_time_ns` builds and now
  differs at its own assert 16: `2147483647 + 1` wraps to int32 in the pin and
  stays 64-bit in R6 (`sinceNsBetween(0, pastCap)` answers 2147483648 instead
  of -1). That is the first `r6-differs` in the corpus, and it is the int-width
  wall, not this admission. `generic_functions` leaves the `arg` wall for
  `register type unknown`; `std_lists_generic` and `test_collections` pass the
  parameter gate and stop at `register type unknown`; `test_collections_boxed`
  stops at `list_push` kind 0; `call_arg_types` and `parameter_type_fallback`
  stop at their model parameters (`Box`, `Point`). First refusals: module-level
  37 -> 35, `list_push` 1 -> 2.
- The `List` register position has no corpus program behind it:
  `type_spellings` stops at an earlier `decl_var` (line 1, column 381 in both
  measurements), so `ok-list-capital-spelling` is that position's only evidence.
- Status: **applied**.

## D-M1-33 - A top-level constant is a #define whose text is the type

- Date: 2026-10-20. First refusal: `global CHAR_SPACE is outside M1 literal
  int/bool support` in `global_const` and `ast_function_body_is_a_block`, and
  `global WALK_MAX_DEPTH is outside M1 literal int/bool support` in `test_fs`
  and `test_imports`.
- Reference (pin `17ed624`, `git cat-file blob`):
  - `compiler/emit/c_backend.orb:398-401` and `:1635` write
    `#define <name> <text>` into the preamble for every module global.
  - `compiler/emit/c_backend.orb:1649-1653` (`cDefineLiteral`) returns the
    value's `String` payload unchanged. `:1641-1648` says the builder stores the
    literal in an `IRValue.String` so a register cannot land in a define.
  - `compiler/emit/c_backend.orb:2406-2415` says a top-level name is that
    define, and that declaring a variable under it breaks the preprocessor.
  - The corpus writes `typeName: "unknown"` and the decimal text in `value` for
    all 21 globals.
- Admission: a global whose `typeName` is `unknown` or empty and whose value is
  a `String` of an optional `-` and decimal digits inside int32 is an `int`
  constant. An absent `typeName` is not admitted here: the loader rejects it
  with `E_SCHEMA` before codegen. A declared `int` or `bool` keeps its rule.
  `global_type` answers the declared type, else `int` when `global_integer`
  holds, else NULL. `load_var` of a global already emitted the value as an
  immediate, which is the same substitution the `#define` performs.
- Fail closed: a declared non-scalar type is refused whatever the text
  (`x-global-declared-string`); an `unknown` text that is not a decimal integer
  is refused (`x-global-unknown-text`); a text outside int32 is refused on the
  same parse (`x-global-int-overflow`, `x-global-int-underflow`). `long` is 32
  bits on this target, so `strtol` saturates at `LONG_MAX`/`LONG_MIN` without
  leaving int32: the parse is `strtoll` with `ERANGE`, and the boundary values
  `2147483647` and `-2147483648` still build and match the oracle. `0x20` and
  `+5` are C int literals the pin would accept and R6 refuses: stated gaps, not
  divergences.
- Controls: fixture `43-global-constant-type` (`val SPACE = 32`,
  `const LIMIT = 3`, `val NEG = -2`; printed, compared and added; stdout
  `32\r\n3\r\n-2\r\n`, exit 0) and `ok-global-unknown-type` (43 + -2 = 41 by
  exit). `x-global-unknown-text`, `x-global-declared-string`,
  `x-global-int-overflow` and `x-global-int-underflow` are the rejections.
  The two edges inside int32 are pinned too: `ok-global-int32-max`
  (`2147483647` reaches exit 2147483647) and `ok-global-int32-min`
  (`-2147483648` reaches exit `0x80000000`).
- Evidence (pin `filesSha256 0106d531`): match **44 -> 45/93** and build
  **45 -> 46/93**. `global_const` now matches. `ast_function_body_is_a_block`
  advances to `union ASTNode variant Program payloads are outside the M1 native
  subset`, `test_fs` to `extern return type is unsupported in M1` and
  `test_imports` to `register type unknown is unsupported in M1`. First
  refusals: module-level 35 -> 34; `register type unknown` 12 -> 13; `extern
  return type` 2 -> 3; the four `global ... is outside M1 literal int/bool
  support` messages disappear. Ratchet re-taken at 46/45.
- Status: **applied**.

## D-F0-13 - Re-pin to 17ed624: corpus 97+1, canonical 6C26A19E

- Date: 2026-10-21. Pin: `17ed6243844eb043f08cf9163cb60ea9cb8c6a63` (`main`,
  **not pushed**), 70 commits after `ce87d508` (D-F0-12).
- What moved: `compiler/emit/c_backend.orb` (`2d64c8de` -> `0c9dd786`),
  `compiler/tir/lower.orb`, `compiler/tools/doctor.orb`, the regenerated
  `compiler/selfhost/stage3.exe.c` (`5852fe69`), `runtime/arena.c`
  (`59f12e24`, request-boundary OOM recovery), `runtime/orbit_api.h`
  (`523f2913`, six new request-OOM symbols), `runtime/file.c`,
  `runtime/http.c`, `runtime/kynx.c` and `scripts/emit_ir_corpus.py`
  (`057887fd` -> `d3da729c`, 74 suite + 23 std). `compiler/emit/ir_emit.orb`
  is unchanged (`54f0601b`), so the serializer did not move.
- Canonical: `python scripts/dev.py fp` in a tree materialized at the commit
  converges and reports "matches canonical".
  `PUBLISHED_C = 6C26A19ED12A5D6ABAD240B922FA9C3E1D939DE16D998363CA7CF816FB0FF6DE`
  (was `9906A6F7...`), equal to `sha256(compiler/selfhost/stage3.exe.c)` at
  the commit. The fixed point is `C:\Temp\orbit-fp-08a471904d.exe`, 2573312
  bytes, sha256 `7e1f9c307f96415fb6ee9198b241eb0b5bcccabc6634fbe9571cf9bd39dcc896`,
  `orbit 0.1.0-rc.2`.
- Corpus: **97 executable + 1 support = 98 documents**, 5932267 bytes,
  `filesSha256 e8586b9af976d18c3fdb1d4071a5431dc60da49223a6a09e9e38a02a9b96db0e`
  (was `0106d531...`), `languageCommit 17ed624`. Emission from a `git archive`
  extraction and from a `git clone` at the same commit produced the same
  `filesSha256`, so the bytes are a function of the commit, not of the tree.
- Measurement: **47 build / 46 match over 97 programs** (was 46/45 over 93);
  91/98 documents carry an `unknown` register type (was 87/94). Ratchet
  re-taken at `r6BuildOk 47`, `match 46`.
- Runtime archive rebuilt from the commit with an absolute `--out`:
  `nm --defined-only` reports **337 defined symbols** (324 at `ce87d508`);
  observation sha256 `58ff94a2381b33cb2dc2e8aa5deb0889b301b611bd742f12efc49f5152bbf97e`,
  140814 bytes. A relative `--out` resolves against the pinned tree and fails
  at `ar`; the recipe `86858c14` is unchanged.
- Fixture retired: the pin rejects using an extern function as a value
  (`Semantic error: Cannot use extern function 'orbit_string_at' as a value
  ... [in extern_value]`), so `26-fn-values.orb` no longer builds with the
  oracle and cannot sit in a differential. The pair is renamed to
  `26-fn-values.{orb,json}.skip`; the IR keeps its pinned acceptance control
  `ok-fn-extern` (D-M1-14, exit 98). Differential 43 -> 42 cases.
- Gates: differential **42/42**; loader **193/193** (140 rejections + 53
  controls); `tests/reject/make_cases.py --check` 175 files without drift;
  `scripts/migrate-m1-fixtures.py --check` 20/20; corpus `--check` holds the
  ratchet at 47/46.
- Citations: D-M1-19 to D-M1-33 cite `c_backend.orb` by line. The 130
  citations those entries, `CONTRACT.md`, `IR_SPEC_DRAFT.md` and the corpus
  README carry are reprinted against `0c9dd786` while the entries keep their
  original evidence pin: 113 shifted (+2 below old 2911, +6 above it), 10
  rewritten in place, 5 pointing at constructs the new blob does not have, 2
  unchanged. The gone ones are stated as history instead of renumbered: the
  file-scope `extern OrbitArena* arena;` (`:387`) and `_init_arena` with
  `arena = _init_arena;` (`:2242`). Two pre-existing mis-points now name the
  construct they describe: `inferRegisterTypes` is `:674-1148` and the `nop`
  arm is `:2592`.
- Status: **applied**.

## D-M1-34 - The compiler's own IR is the second measured target

- Date: 2026-10-22. First measurement. The corpus covers 97 programs and says
  nothing about the program the backend exists for. Every module under
  `compiler/**/*.orb` is emitted from the pin and R6's first refusal on each is
  recorded, so a round can report its effect on both targets.
- Instrument: `tests/compiler-ir/run_census.py` runs
  `<compiler> build <module> --emit-ir <out>` with `ORBIT_SKIP_CC=1` from a
  checkout at the pin, then runs R6 with the same `{ir}`/`{exe}` template the
  corpus harness takes. It groups the cases by their first refusal, writes the
  full report with `--json`, and holds `tests/compiler-ir/baseline.json` as a
  one-way ratchet over `r6BuildOk`, keyed by `irFilesSha256` and
  `languageCommit`. Nothing fails because R6 builds fewer modules than exist.
- Measurement (pin `17ed624`): **26 modules, 97 638 934 bytes,
  `irFilesSha256 858dad12…`, 0 build**. Re-taken at the pin `33abf7a` by
  D-F0-14: **97 699 992 bytes, `irFilesSha256 34e710ca…`, 0 build**, the same
  six first refusals. Six first refusals:

| First refusal | Modules | Which |
|---|---|---|
| `union ASTNode variant Program payloads are outside the M1 native subset` | 9 | `emit/c_backend`, `front/ast`, `front/parser`, `front/resolver`, `front/sema`, `ir/builder`, `tir/entry`, `tir/lower`, `tools/doctor` |
| `M1 codegen does not support type kind enumeration` | 8 | `front/lexer`, `front/token`, `host/cluster`, `host/main`, `host/pipeline`, `host/start`, `selfhost/lexer_trace`, `tools/fmt` |
| `union IRValue variant Int payloads are outside the M1 native subset` | 4 | `emit/ir_emit`, `ir/cteval`, `ir/ir`, `ir/optimizer` |
| `extern return type is unsupported in M1` | 2 | `host/extern`, `tools/palette` |
| `module must define one non-extern main` | 2 | `emit/route_runtime`, `front/literal` |
| `model TirFunction field parameters has unsupported type list` | 1 | `tir/format` |

- The last two rows are not subset walls. `route_runtime.orb` and
  `literal.orb` are library modules with no `main`, so they build only inside an
  amalgamation.
- The order is not the size of the group: `host/main.orb` is the entry point of
  the compiler and sits in the enumeration group, so that wall is met first.
  `main.ir` is 16 187 421 bytes, 1 012 functions, 16 globals, 95 models and 4
  types: one enumeration (`TokType`, a flat `variants` list of 90 names) and
  three unions (`ASTNode` 45, `IROpcode` 66, `IRValue` 8).
- The documents are the pin's own sources. The emitter *binary* may carry the
  fix `33abf7a`; the tree must not, because `emit/ir_emit.orb` is itself one of
  the 26 modules and the amalgamations include it. A tree with the fix applied
  emits its own document, not the pin's: 97 692 498 bytes instead of
  97 638 934, and a different hash.
- The emission needed the language fix `33abf7a`, which appends the document
  into one `orbit_cbuf` instead of `out = out + piece`. Without it the 16.2 MB
  document of `host/main.orb` does not finish, past 4.5 GB. The fix is byte
  neutral for the *corpus*: the 98 documents keep `filesSha256 e8586b9a…` file
  by file, because `tests/suite` and `tests/std` do not include
  `emit/ir_emit.orb`. It is not neutral for the *census*: that module is one of
  the 26, so a tree carrying the fix emits its own document. The census
  baseline is the pin's sources, never a patched tree.
- A language commit that changes `compiler/` changes the documents and the
  hash, so a pin move re-takes this baseline in the same act, as the corpus
  baseline does.
- Status: **applied**.



## D-F0-14 - Re-pin to 33abf7a: the emitter appends instead of copying

- Date: 2026-10-22. Pin: `33abf7ab762607c117d01d06bee8584a2e3781c1` (`main`,
  **not pushed**), 2 commits after `17ed624` (D-F0-13).
- What moved: `9268b8f` "doctor: report an annotated var nothing reassigns"
  (`compiler/tools/doctor.orb`, `docs/DOCTOR.md`, the `d013` golden,
  `compiler/emit/c_backend.orb` and the regenerated `compiler/selfhost/stage3.exe.c`)
  and `33abf7a` "emit: append the IR document into a buffer instead of copying
  it" (`compiler/emit/ir_emit.orb`, `compiler/selfhost/stage3.exe.c`,
  `scripts/baselines/unknown_count.json`, `scripts/verify_seed.py`).
  `runtime/` and `scripts/emit_ir_corpus.py` are untouched.
- `c_backend.orb` moves `0c9dd786` -> `2eacd0c5`, four lines: `var errs: list`
  and `var warns: list` become `val` in `verifyModuleTypes` (`:1391-1392`). The
  hunk replaces two lines with two, so the 130 citations re-anchored at D-F0-13
  keep their line numbers; the blob has to be cited as `2eacd0c5` from now on.
- Why: `emitIRv1` accumulated the document with `out = out + piece`. The `+`
  copies the prefix and the arena never frees, so emission was quadratic in the
  document size. Measured on the pin's sources, one module at a time: lexer
  679 MB -> 14 MB, parser 1 411 MB -> 140 MB, `host/main.orb` did not finish
  past 4.5 GB and now emits in 2.62 s with a peak of 1 000 MB. The census
  (D-M1-34) needs it: 8 of the 26 modules exceed the machine without it.
- Canonical: `python scripts/dev.py fp` in a tree materialized at the commit
  converges and reports "matches canonical".
  `PUBLISHED_C = 138FBDF4DDE2D90C8C9E7543F9E1AF4662A60D52A4ED946E91A99E1DFCD06193`
  (was `6C26A19E...`), equal to `sha256(compiler/selfhost/stage3.exe.c)` at the
  commit. The fixed point is `C:\Temp\orbit-fp-8f487a3e9f.exe`, 2571776 bytes,
  sha256 `82826f899541f8b81fc47ac395b08b02ec1137fa4aca012bd2ba851757a570a9`,
  `orbit 0.1.0-rc.2`. `verify_seed.py` reports 4/4.
- Corpus: **unchanged**: 97 executable + 1 support = 98 documents, 5932267
  bytes, `filesSha256 e8586b9af976d18c3fdb1d4071a5431dc60da49223a6a09e9e38a02a9b96db0e`.
  The fix is byte-neutral for the corpus because `tests/suite` and `tests/std`
  do not include `emit/ir_emit.orb`, and the doctor commit only adds a
  diagnostic; the re-emitted manifest differs only in its
  `emitter` block (`C:\Temp\orbit-fp-8f487a3e9f.exe`, `languageCommit 33abf7a`).
  The census is the opposite case: `emit/ir_emit.orb` is one of the 26 modules,
  so the document of the module and of every amalgamation that includes it is
  now the pin's own fixed source.
- Census re-emitted at the commit: **26 modules, 97 699 992 bytes,
  `irFilesSha256 34e710ca...`, 0 build**; the six first refusals keep their
  sizes (9/8/4/2/2/1). `host/main.orb` is 16 198 821 bytes (was 16 187 421):
  11 400 bytes more source in one function, and 1 012 functions, 16 globals,
  95 models and 4 types unchanged. Ratchet re-taken.
- Gates: differential **42/42**; loader **195/195** (140 rejections + 55
  controls); `tests/reject/make_cases.py --check` 177 files without drift;
  `migrate-m1-fixtures.py --check` 20/20; corpus **47 build / 46 match over 97
  programs**, ratchet re-taken at `r6BuildOk 47`, `match 46`; census ratchet
  holds.
- A pin move is one commit: PIN.md, this entry, the regenerated corpus manifest
  and both baselines describe one tree.
- Status: **applied**.

## D-M1-35 - An enumeration is an int whose tag constant is the variant index

- Date: 2026-10-22. Pin: `33abf7a` (D-F0-14). The pinned C backend lowers an
  `enumeration` to `typedef enum { Name_TAG_Variant = <index> }` in declaration
  order plus `#define Name_COUNT n` (`c_backend.orb:1692-1706`), and its
  verifier accepts a comparison between two enums or between an enum and an
  `int`/`usize` without a warning (`:1362-1363`, `isEnumType` at `:442-460`
  names the kind, `sum`, `union` and the fixed spellings `TokType`/`TokTypeTag`).
  The type has no runtime representation of its own, so R6 lowers it as an
  `int`. A field of an enum type is 4 bytes with alignment 4 in the C struct,
  the same size and alignment `model_field_size` gives an `int`.
- Shape at the pin: the type entry is `{"name","kind":"enumeration",
  "variants":[names],"richVariants":["",...],"methods":[]}`, one empty rich
  variant per name, and the builder writes the constant of a variant as a
  `load_var` named `Name_TAG_Variant`. The register type of that load is the
  enum's name in the compiler's own front (`front/token.ir`) and `unknown`
  elsewhere (the fixture); `infer_dest_type` gives the `unknown` slots `int`.
  The loader already accepted that entry; the refusal was codegen's kind gate.
- R6: `canonicalize_enum_types` rewrites every use of an enumeration name to
  `int` before any check reads it -- `paramTypes`, `returnType`,
  `registerTypes`, model field types and global `typeName` -- the way
  `canonicalize_list_types` already rewrites the list spellings (D-M1-32). The
  type table keeps the name, because that name is the key that resolves
  `Name_TAG_Variant` to its index; `tag_constant` (the former
  `union_tag_constant`) accepts both `union_type` and `enumeration`.
- Fail closed: the kind gate names `enumeration` and `union_type` and keeps
  refusing every other kind (`x-union-kind-sum`). An enumeration variant that
  carries a payload is refused (`x-enum-payload`): the C backend would drop it
  in silence. A tag constant that names no declared variant stays an undeclared
  symbol (`x-enum-undeclared-tag`). The D-M1-12 control
  `x-union-kind-enumeration` becomes the acceptance control `ok-enum-kind-unit`
  because that document now builds.
- Fixture `44-enum-tag-constant`: the `.orb` and the IR the pin emits from it.
  It reads a variant back through an enum constant, a model field, a parameter
  and a return type, and compares eight times: four `eq` in the two callees and
  four `ne` in `main`. Oracle and R6 both print `blue\r\nred\r\n2\r\nred\r\n`
  and exit 0.
- Measured: differential **42/42 -> 43/43**; loader **195/195 -> 199/199** (142
  rejections + 57 controls; `make_cases.py --check` 180 files without drift);
  corpus **47 build / 46 match over 97 programs**, ratchet holds at the same
  numbers and its baseline file is untouched; census **0 -> 0 build**, and the
  `enumeration` group of 8 is gone: `union ASTNode` 9 -> 14, `union IRValue`
  4 -> 4, `extern return type` 2 -> 3, `no non-extern main` 2 -> 2,
  `register type unknown` 0 -> 2 (`front/lexer`, `front/token`: a register slot
  that no instruction writes and none reads), `model field list` 1 -> 1. The
  census ratchet is re-taken; the corpus ratchet is only re-checked.
- Not measured, not claimed: an enumeration as a list element kind, an enum
  inside a union payload, the kind `sum`, the `Name_COUNT` macro, an enum in an
  extern signature and an enum-typed global. The last two are admitted by the
  rewrite and have no control: an enum is an int at the ABI, and the global path
  accepts decimal text through `global_integer` as it does for any int.
- Status: **applied**.

## D-M1-36 - An int is 32 bits, and the slot keeps the sign

- Date: 2026-10-22. Pin: `33abf7a` (D-F0-14). Found by D-M1-32's measurement:
  `test_time_ns` built and differed at its assert 16, with
  `sinceNsBetween(0, pastCap)` answering 2147483648 where the oracle answers
  -1.
- Reference (pin `33abf7a`, read with `git cat-file blob`):
  - `runtime/orbit_api.h` declares `typedef int orbit_int;`. An Orbit `int` is
    a C `int`, 32 bits wide, and the arithmetic the pinned backend emits is
    that C expression.
  - Fixture `45-int-overflow` is the measurement of the width: the pin builds
    its `.orb` and exits 42, while R6 before this round answers 10 at the same
    program's first probe.
- Root cause: `win64.c` emitted integer arithmetic at 64 bits - `addq` for
  `add`, `subq`/`imulq` for `sub`/`mul` - so a result past int32 stayed in the
  slot, and every consumer that compares with `cmpq` (the convention D-M1-25
  records) read the 64-bit value. Two boundary sites carried the same defect: a
  `call` whose destination is an `int` stored all of `%rax`, while a C `int`
  returns in `%eax` and the 32-bit write zeroes the upper half; and an inline
  `Int` literal argument was loaded with `movl` into the argument register, so
  `-1` crossed a call as 4294967295.
- Admission: none. The loader suite does not move, because the round corrects
  the lowering of opcodes that were already admitted. No new control is added,
  and 199/199 is the evidence that nothing new is admitted.
- Fix: `add`, `sub` and `mul` are `addl`/`subl`/`imull` followed by
  `movslq %eax, %rax`; a call whose destination type is `int` sign-extends
  `%eax` before the store; an inline int literal argument is `movl` followed by
  `movslq` into the argument register. `div`, `mod`, `list_get` and
  `load_field` already used the 32-bit form.
- The slot convention does not change: an `int` slot holds the sign-extended
  32-bit value in its eight bytes, so `cmpq`, the slot copies and the int
  parameters keep working. Bool slots keep their 32-bit writes; `list_len`
  keeps its 64-bit store because `OrbitList.len` is a `size_t` (D-M1-25).
- `neg` stays `negq`. A 32-bit negation differs only for `-INT32_MIN`, which is
  undefined in C; the oracle folds that comparison under the no-overflow
  assumption (measured: the probe `.orb` exits 11), so no fixture can pin it
  and the round leaves the instruction alone.
- Fixture `45-int-overflow` covers `add` (`bump(2147483647)` must be negative),
  `mul` (`sq(65536)` must be 0), `sub` (`over - 1` must be 2147483647), the
  call result (the `bump`/`sq`/`id`/`sum4` destinations) and the literal
  argument (`id(-1)` must be negative, and `sum4(1, 2, 3, -4)` must be 2, which
  also measures the fourth argument register, `%r9d`). Oracle and R6 both exit
  42. The first probe cannot use `main`'s return value alone: the process exit
  keeps only the low 32 bits, so the wrap has to be observed by a comparison.
- Effect: differential **43/43 -> 44/44**; loader **199/199** unchanged;
  corpus **47 build / 46 match -> 47 build / 47 match over 97 programs**, with
  exactly one verdict moving (`tests/std/test_time_ns.orb`: `r6-differs` ->
  `match`) and the other 97 cases unchanged; census **0 -> 0 build**, 26
  modules, 97 699 992 bytes, `irFilesSha256 34e710ca.`, the same six first
  refusals. The corpus ratchet is re-taken at `match 47`; the census baseline is
  untouched.
- Not measured: an `int` operation whose result the C source leaves
  unobservable, the `float` and `bool` paths (both unchanged), and any width
  other than 32. No corpus program multiplies past int32, so the `imull` form
  rests on the fixture.
- Status: **applied**.

## D-M1-37 - The entry point stores argc and argv for the runtime

- Date: 2026-10-26. Pin: `33abf7a` (D-F0-14). Found while measuring an opaque
  list read: `tests/std/test_process.orb` built under R6 and exited 4, at
  `if argCount() < 1 { return 4 }`, where the oracle exits 0.
- Reference (pin `33abf7a`, read with `git cat-file blob`): the C `main` the
  pinned backend writes for a program without routes starts with
  `_orbit_argv = argv;` and `_orbit_argc = argc;` (`c_backend.orb:302-306`).
  `runtime/os.c:371-378` defines both globals, initialized to `NULL` and `0`,
  and `orbit_os_argc`/`orbit_os_argv` read them.
- Root cause: R6's entry wrapper called `orbit_arena_create` and `orbit_main`
  and never wrote the two globals. Every program saw `argc` 0 and an empty
  `argv`.
- Fix: the wrapper stores `%ecx` into `_orbit_argc` and `%rdx` into
  `_orbit_argv` before its first call. Each store is a `leaq sym(%rip)`
  followed by a store through the base register, the RIP-relative form the
  internal assembler already encodes.
- Admission: none. The loader suite gains one acceptance control that runs the
  fixture; no rejection moves.
- Fixture `46-process-argc`: two externs, `orbit_os_argc` and the
  arena-taking `orbit_os_argv` (D-M1-27). It checks `argc >= 1`, a non-empty
  `argv[0]` and an empty `argv[argc]`, and prints `argc`. Oracle and R6 both
  print `1\r\n` and exit 42. The driver built from `01037c1` builds the same
  IR and the program exits 10, at the first check.
- Not changed: the pinned `main` also sets the Windows console to UTF-8 and
  enables ANSI processing when stdout is a console. That code runs only on a
  console handle, the gates read stdout through a pipe, and no measurement can
  observe it. The database setup the pinned `main` adds for a module with
  migrations stays outside the subset, which refuses migrations.
- Status: **applied**.

## D-M1-38 - An opaque list read takes the type of the parameter it feeds

- Date: 2026-10-26. Pin: `33abf7a` (D-F0-14). After D-M1-36 the widest first
  refusal in the corpus was `register type unknown is unsupported in M1`, 12
  programs. In 11 of them the `unknown` slot is the destination of a
  `list_get` with `elemKind 0`: a read from a list the builder could not type,
  such as the `parts: list` parameter of the std `join`
  (`std/string/string.orb:144`).
- Reference (pin `33abf7a`, read with `git cat-file blob`): `listReadCall`
  lowers kind 0 to `orbit_list_read_word` (`c_backend.orb:605-613`).
  `runtime/orbit_api.h:492-497` returns the slot word unchanged and skips the
  kind check for `ORBIT_ELEM_OPAQUE`. The backend's inference gives such a read
  `string` only when the receiver register is a string list
  (`c_backend.orb:957-983`); otherwise the register stays a `uintptr_t` word
  and C converts it where it is used.
- Measured uses: of the 18 opaque reads in the 12 programs, every one has a
  single reader. 13 are the argument of a direct call to a declared function
  whose parameter is `string` (`orbit_cbuf_append_selfhost`, `assertEqStr`,
  `matchSeq`, `validSeq`, `verifyHs256`); 3 are a `ne` against a literal and 2
  are a `list_push` into another opaque list.
- Decision: a `list_get` with `elemKind 0` whose destination is `unknown` takes
  the type of the parameter its value is passed to, when all of these hold:
  the destination register has one writer and one reader; the reader is an
  `arg` with no destination; the `call` that consumes that `arg` is a direct
  call by `String` name to a declared function (not `print`, a model or a union
  variant) whose arity equals its parameter count; no `label`, `jump` or
  `jump_if_false` lies between the `arg` and the `call`; and the parameter type
  is `string` or `int`. The `call` is found by counting the pending `arg`s
  after the read, as `max_outgoing_args` does. A destination declared with a
  type must agree with the same rule, or `list_get` refuses it.
- Why it is the oracle's semantics: C converts the `uintptr_t` word to the
  parameter type at the call. For `orbit_string` the word is the pointer. For
  `orbit_int` the conversion keeps the low 32 bits, and the R6 int slot holds
  them sign-extended (D-M1-36). The R6 read is the typed read with the kind
  check removed, so the null, bounds and index-error paths are unchanged.
- Not admitted: `float` and `bool` parameters (C converts a word to `double`
  numerically, and the subset has not measured a `bool` word), a comparison
  use, a second use, an indirect call, and a `list_push` of the word into
  another list. Those reads stay `unknown`, with the same diagnostic.
- Fixture `47-opaque-list-read`: `firstLen(parts: list)` passes `parts.get(0)`
  to `strLen(s: string)`, and `pick(nums: list, at: int)` passes
  `nums.get(at)` to `twice(n: int)` for 7, -5 and 2147483647 (the last wraps to
  -2). Oracle and R6 both print `5\r\n` and exit 42.
- Controls in `tests/reject/`: `x-opaque-read-float-param`,
  `x-opaque-read-compare`, `x-opaque-read-two-uses` and
  `x-opaque-read-declared-mismatch` refuse; `ok-opaque-read-int-param` (exit 9)
  and the fixture accept.
- Measured: differential **45/45 -> 46/46**; loader **200/200 -> 206/206** (146
  rejections + 60 controls; `make_cases.py --check` 185 files without drift);
  corpus **47 build / 47 match -> 49 build / 49 match over 97 programs**.
  `tests/std/test_regex.orb` and `tests/std/test_process.orb` go from refusal
  to a byte-for-byte match; `test_process` needed D-M1-37 first. Five more
  programs (`test_bytes`, `test_crypto`, `test_io`, `test_json`,
  `test_string`) move their first refusal from `register type unknown` to
  `returning result values is not supported in M1`. The ratchet is re-taken at
  `r6BuildOk 49`, `match 49`. Census **0 -> 0 build**, the same six first
  refusals.
- Status: **applied**.

## D-M1-39 - A list field is an eight-byte list pointer

- Date: 2026-10-26. Pin: `33abf7a` (D-F0-14). The census refused
  `compiler/tir/format.orb` at `model TirFunction field parameters has
  unsupported type list`, and three corpus programs (`generic_models`,
  `generic_nested_argument`, `model_literal_fields`) stopped at a model field of
  type `list<int>`, `list<list<float>>` or `list<string>`.
- Reference (pin `33abf7a`): `mapTypeToC` gives every list spelling the same C
  type, `OrbitList*` (`c_backend.orb:478`, `:489-496`), and a synthesized model
  struct declares each field with that map. A list field is a pointer member:
  8 bytes, alignment 8.
- Decision: `canonicalize_list_types` (D-M1-32) also rewrites model field types
  to `list`. `model_field_size` gives `list` 8/8. `load_field` copies the
  pointer into the slot, and `store_field` and the constructor copy the slot
  into the member. The existing checks require a `list` value register for
  each, so a string or int value stays refused.
- The field shares the list, as the pointer does in C: a `push` through
  `bag.items` is visible through the local the list came from.
- Fixture `48-model-list-field`: a model with an `int` and two list fields,
  built from a local list and a literal, pushed through the field, read back
  through typed locals, and replaced with a new list. Oracle and R6 both print
  `bc\r\n` and exit 42.
- Not measured, and not in the fixture on purpose: an element read straight off
  the field (`bag.names.get(1)`). The pin emits it with `elemKind 0`, and its
  oracle prints `140697716711947` for `print(bag.names.get(1))`, the pointer
  formatted as an int, instead of `bc`. That is a defect of the language's
  builder, reported on the language side; R6 keeps those reads `unknown`
  (D-M1-38 types an opaque read only by a parameter it feeds).
- Controls: `x-model-map-field` and `x-model-list-field-store-string` refuse.
  The D-M1-11 control `x-model-field-unsupported-type` used a `list` field; it
  now uses a model-typed field, which stays refused.
- Measured: differential **46/46 -> 47/47**; loader **206/206 -> 209/209** (148
  rejections + 61 controls; `make_cases.py --check` 187 files without drift);
  corpus **49 build / 49 match**, unchanged: the three programs move to their
  next refusal, a model parameter (`Box_int`, `Point`) or an `unknown`
  register. Census **0 -> 0 build**; `compiler/tir/format.orb` moves from the
  list field to `returning model or void* values is not supported in M1`. The
  census baseline is re-taken for that refusal; the corpus baseline is not.
- Status: **applied**.

## D-M1-40 - A model parameter is the model pointer

- Date: 2026-10-26. Pin: `33abf7a` (D-F0-14). After D-M1-39, four corpus
  programs stopped at `parameter type <Model> is unsupported in M1` (`Point`,
  `Alpha`, `Box`, `Trace`), and a fifth (`generic_models`) at `Box_int`.
- Reference (pin `33abf7a`): `mapTypeToC` gives a declared model `Name*`
  (`c_backend.orb:524-526`). A model value is a pointer, and a parameter of a
  model type receives that pointer.
- Decision: a declared function admits a model-typed parameter, and an `arg`
  admits a model-typed value. The prologue already spills every non-float
  parameter with `movq`, from a register or from the stack, and
  `emit_load_arg` already moves a non-float slot with `movq`. The existing
  call check requires each argument to have the callee's parameter type, so a
  model `A` passed where `B` is declared stays refused.
- The callee shares the instance: a `store_field` through the parameter is
  visible to the caller, as the pointer is in C.
- Not admitted: a model parameter of an extern (no runtime function takes one
  in the subset), a model return (`returning model or void* values`, its own
  front), and a union or `object` parameter.
- Fixture `49-model-parameter`: `sum(p)`, `shift(p, dx)` writing `p.x` that
  `main` reads back, `fifth(1, 2, 3, 4, p)` with the model in the stack slot
  of the fifth argument, and `name(p)` returning the string field. Oracle and
  R6 both print `origin\r\n` and exit 42.
- Controls: `x-model-param-extern` and `x-model-param-other-model` refuse.
- Measured: differential **47/47 -> 48/48**; loader **209/209 -> 212/212** (150
  rejections + 62 controls; `make_cases.py --check` 189 files without drift);
  corpus **49 build / 49 match -> 52 build / 52 match over 97 programs**:
  `if_expression_no_phantom_branch`, `model_field_owner_typing` and
  `parameter_type_fallback` match byte for byte; `call_arg_types` and
  `generic_models` move to `returning model or void* values`, and
  `model_literal_fields` to `register type object`. The ratchet is re-taken at
  52/52. Census **0/26**, unchanged.
- Status: **applied**.

## D-M1-41 - `movslq` carries REX.R and REX.B

- Date: 2026-10-26. Pin: `33abf7a` (D-F0-14). Found while building a
  five-argument call: the R6 binary crashed with `0xC0000005` outside gdb and
  ran correctly inside it.
- Root cause: `asm_win64.c` encoded the register form of `movslq` with a fixed
  `0x48` prefix. A source or destination in `%r8`-`%r15` lost its high bit, so
  `movslq %r9d, %r9` became `movslq %ecx, %rcx` and `movslq %r8d, %r8` became
  `movslq %eax, %rax`. D-M1-36 emits `movl $n, %r8d` / `movslq %r8d, %r8` for
  an inline int literal argument in position 2 or 3. The extension then
  truncated the first argument in `%rcx` to its sign-extended low 32 bits. A
  pointer in `%rcx` below `0x80000000` survived, which is why gdb, whose
  stack sits low, hid it.
- Fix: the prefix is `0x48 | REX.R(dest) | REX.B(src)`. `negq` and the
  `movq` GPR-to-XMM form had the same fixed prefix and get the same bits; R6
  emits both only with low registers today, so neither changes any output.
- Byte evidence: nine forms (`movslq` across `%r8d`, `%r9d`, `%r11d`, `%ecx`
  and `%eax` sources and destinations, `negq %rax`/`%r10`, `movq %rax, %xmm0`
  and `movq %r10, %xmm15`) assembled by `tests/coff/asm_probe.c` and by GNU
  `as` disassemble identically with `objdump -d`. GNU `as` is only the
  reference for the bytes; no R6 program goes through it.
- Fixture `50-literal-args-r8-r9`: `mix("abc", 1, 2, -3)` puts a string
  pointer from `.rdata` in `%rcx` and literals in `%r8`/`%r9`, and
  `spread(5, 6, "four", -7)` puts the pointer in `%r8`. Oracle and R6 both
  print `65\r\n` and exit 42. The driver built with the previous encoder
  crashes on the same IR with `0xC0000005`.
- Admission: none. The loader suite gains the fixture as an acceptance control.
- Measured: differential **48/48 -> 49/49**; loader **212/212 -> 213/213**
  (150 rejections + 63 controls); corpus **52 build / 52 match**, no verdict
  moves; census **0/26**, ratchet holds.
- Status: **applied**.

## D-M1-42 - A C `bool` return is read from `al`

- Date: 2026-10-26. Pin: `33abf7a` (D-F0-14). Found while building
  `tests/std/test_string.orb` for a result decision: R6 exited 5 at
  `if assertTrue(startsWith("hi", "hello")) { return 5 }`, where the oracle
  exits 0.
- Reference (pin `33abf7a`): `runtime/orbit_api.h:118` declares
  `typedef bool orbit_bool`, and `:405` declares
  `bool orbit_string_starts_with(...)`. The Win64 ABI returns a C `bool` in
  `al` and leaves the rest of `rax` undefined.
- Root cause: a direct call with a `bool` destination stored all of `rax` in
  the slot, and every bool consumer compares the slot's low 32 bits with zero.
  A `false` with nonzero bits above `al` read as `true`. A function R6 emits
  writes the whole of `eax`, so only runtime calls were exposed.
- Fix: a `bool` destination takes `movzbl %al, %eax` before the store.
- Fixture `51-c-bool-return`: three runtime predicates
  (`orbit_string_starts_with`, `orbit_string_ends_with`,
  `orbit_string_contains`) answer `false` and `true`, and the last `true` is
  printed. Oracle and R6 both print `1\r\n` and exit 42. The driver without
  the fix exits 10, at the first `false`.
- Admission: none. The loader suite gains the fixture as an acceptance control.
- Measured: differential **49/49 -> 50/50**; loader **213/213 -> 214/214**
  (150 rejections + 64 controls); corpus **52 build / 52 match**, no verdict
  moves; census **0/26**, ratchet holds.
- Status: **applied**.

## D-M1-43 - A result crosses a call, and `try` types its unwrap

- Date: 2026-10-26. Pin: `33abf7a` (D-F0-14). After D-M1-38,
  `returning result values is not supported in M1` was the first refusal of 7
  corpus programs (`float_results`, `test_codec`, `test_bytes`, `test_crypto`,
  `test_io`, `test_json`, `test_string`).
- Reference (pin `33abf7a`): `mapTypeToC` gives `result` the C type
  `OrbitResult` (`c_backend.orb:502`), a 24-byte struct (`ok`, `error_code`,
  `error_msg`, `value`; `runtime/orbit_api.h:180-185`). A function returns it
  by value (`return r_N;`, `:3423`), and a register holds it by value. The
  `result_unwrap` a `try` emits is `r_N = r.value;` into a `uintptr_t` word, or
  `orbit_result_get_float` when the destination is `float` (`:3683-3700`).
- Two rules, in one decision because every `try` over a returned result emits
  both: the call and the unwrap. Neither can be measured alone.
  1. **The call.** Win64 returns a 24-byte struct through a hidden pointer in
     `rcx`, and the IR arguments move one position, as the arena does for
     D-M1-27 externs. The callee spills `rcx` into the slot below its
     registers that `main` uses for its arena (`hidden_slots`), and its `ret`
     copies the 24-byte cell through that pointer and returns it in `rax`. The
     caller passes the destination register's result cell. A parameter in ABI
     position 4 or later is read from the stack by its ABI position, not by its
     IR index. Only a declared, non-extern Orbit function returns a result;
     externs already fail with `extern return type`.
  2. **The unwrap.** The builder leaves the destination `unknown` (D-M1-20 left
     it there). With no reader, the word is never observed and is typed
     `void*`. When the one reader is the `decl_var` of a local, the
     destination takes the type of that local's `load_var` destinations, which
     the builder writes from the annotation `val x: T = try ...`; those loads
     must agree and the type must be `int`, `float` or `string`. Any other use
     stays `unknown`.
- Fixture `52-result-return`: `half` returns `ok(n / 2)` or `err("odd", 3)`,
  `scale` an `ok` float, `greet` an `ok` string, and `pick` takes five `int`s,
  so its fifth argument sits on the stack behind the hidden pointer. `main`
  binds `int`, `float` and `string` values through `try`, and prints two
  `catch e` messages. Oracle and R6 both print `odd\r\nhi r6\r\nsum off\r\n`
  and exit 42.
- Controls: `x-unwrap-local-loads-disagree` refuses; `ok-result-unwrap-unread`
  (exit 1) and the fixture accept. The D-M1-20 control `x-infer-result-unwrap`
  used an unwrap nothing reads, which this decision admits; it now uses an
  unwrap read by `ret`, which stays refused.
- Language defects met while writing the fixture, reported on the language
  side and kept out of it: an `int` literal argument to a function that
  contains `try`/`catch` is emitted as `(void*)(uintptr_t)(84)` and gcc rejects
  the C (`-Wint-conversion`); two `catch e` in one function are a duplicate
  symbol.
- Measured: differential **50/50 -> 51/51**; loader **214/214 -> 217/217**
  (151 rejections + 66 controls; `make_cases.py --check` 191 files without
  drift); corpus **52 build / 52 match -> 56 build / 56 match over 97
  programs**: `float_results`, `test_codec`, `test_crypto` and `test_string`
  match byte for byte (`test_string` needed D-M1-42); `test_bytes` and
  `test_io` move to `returning model or void* values`, `test_json` to
  `extern return type`. The ratchet is re-taken at 56/56. Census **0/26**,
  unchanged.
- Status: **applied**.

## D-M1-44 - A model return is the model pointer

- Date: 2026-10-26. Pin: `33abf7a` (D-F0-14). After D-M1-43,
  `returning model or void* values is not supported in M1` was the first
  refusal of five corpus programs (`call_arg_types`, `generic_models`,
  `color_smoke`, `test_bytes`, `test_io`) and of `compiler/tir/format.orb`.
- Reference (pin `33abf7a`): `mapTypeToC` gives a declared model `Name*`
  (`c_backend.orb:524-526`), and a function returns it as that pointer.
- Decision: a function may return a model. The `ret` of a non-float register
  already moves the slot into `rax`, and a call already stores `rax` into a
  non-float, non-int, non-bool destination, so only the signature check
  changes. The existing checks require the `ret` value and the call
  destination to have the declared model type. A `void*` return (the D-M1-12
  union cell) stays refused, now with the message
  `returning void* values is not supported in M1`.
- Fixture `53-model-return`: a factory, an identity that mutates its argument
  and returns it (both names see the change), a nested
  `total(same(make(1, 1)))`, and a model returned from a five-argument call.
  Oracle and R6 both print `wide\r\nmade\r\n` and exit 42.
- Controls: `x-return-union-cell` refuses.
- Measured: differential **51/51 -> 52/52**; loader **217/217 -> 219/219** (152
  rejections + 67 controls; `make_cases.py --check` 192 files without drift);
  corpus **56 build / 56 match -> 60 build / 60 match over 97 programs**:
  `generic_models`, `color_smoke`, `test_bytes` and `test_io` match byte for
  byte, and `call_arg_types` moves to `call argument 0 type does not match
  takesFloat`. The ratchet is re-taken at 60/60. Census **0/26**:
  `compiler/tir/format.orb` moves to `list_push requires a list register and a
  value register matching elemKind 1..4`; the census baseline is re-taken.
- Status: **applied**.

## D-M1-45 - A result local is a 24-byte cell

- Date: 2026-10-26. Pin: `33abf7a` (D-F0-14). After D-M1-44, six corpus
  programs stopped at `decl_var requires a declared local and a matching
  scalar, list, void*, model, or function value register`: a local of type
  `result` (`val value: result = ok(41)`, `var out: result = ...`).
- Reference (pin `33abf7a`): `hoistedCType` declares a result local as
  `OrbitResult` (`c_backend.orb:4195`), and `storeCastFor` stores into it
  without a cast (`:4214`): the local holds the 24-byte struct by value.
- Decision: a result local gets its own 24-byte cell in the area that already
  holds one cell per register, at cell index `register_count + local index`.
  `decl_var` and `store_var` copy the value register's cell into it, and
  `load_var` copies it into the destination register's cell, eight bytes at a
  time. The value must be a register (the builder never writes a result
  literal), and the existing type checks keep a value of another type out.
- Fixture `54-result-local`: `ratio` returns a `val bad: result` or
  `val good: result`; `label` declares `var out: result`, reassigns it twice
  and returns it; `main` reads the values through `try` and three error
  messages through `catch`. Oracle and R6 both print
  `zero\r\nsmall\r\nbig\r\nnegative\r\n` and exit 42.
- Controls: `x-result-local-store-int` refuses.
- Measured: differential **52/52 -> 53/53**; loader **219/219 -> 221/221** (153
  rejections + 68 controls; `make_cases.py --check` 193 files without drift);
  corpus **60 build / 60 match -> 63 build / 63 match over 97 programs**:
  `result_bind`, `result_catch` and `result_values` match byte for byte;
  `result_binding` moves to `parameter type result`, `result_try` to
  `register type unknown`, `type_spellings` to `register type Result`. The
  ratchet is re-taken at 63/63. Census **0/26**, unchanged.
- Status: **applied**.

## D-M1-46 - A register no instruction touches is a word

- Date: 2026-10-26. Pin: `33abf7a` (D-F0-14). The builder reserves registers
  it never writes or reads: a `match` arm over a union allocates one per
  pattern, and the census reported the same slot in `compiler/front/lexer.orb`
  and `compiler/front/token.orb` (D-M1-35). Each is serialized `unknown`, and
  R6 refused the whole function at the register-type gate.
- Reference (pin `33abf7a`): the C backend declares every register as a
  `uintptr_t r_N;` word (`c_backend.orb:3741-3750`), so an unused slot is a
  declaration nothing reads and has no observable type.
- Decision: after the D-M1-20 passes, a register still `unknown` that no
  instruction names as destination or operand is typed `void*`, the word type.
  A slot that is read is not dead: if nothing writes it, it stays `unknown`
  and refused.
- Controls: `ok-dead-unknown-slot` (exit 7) accepts two dead slots;
  `x-unknown-slot-read-unwritten` refuses a slot that `ret` reads and nothing
  writes. No fixture: the pin emits dead slots only in programs whose other
  instructions need D-M1-47, which carries the fixture.
- Measured: differential **53/53**, unchanged; loader **221/221 -> 223/223**
  (154 rejections + 69 controls; `make_cases.py --check` 195 files without
  drift); corpus **63 build / 63 match**, unchanged, because the four corpus
  programs with dead slots (`match`, `member_resolution`,
  `union_bare_variant_pattern`, `union_match_exhaustive`) stop first at a
  union register type. Census **0/26**: `front/lexer` and `front/token` move
  from `register type unknown` to `ret value does not match function return
  type`; the census baseline is re-taken.
- Status: **applied**.

## D-M1-47 - A union type is its cell pointer, and a match arm binds its payload

- Date: 2026-10-26. Pin: `33abf7a` (D-F0-14). Four corpus programs stopped at
  `register type Shape` or `register type OptInt`: the name of a declared
  union used as a register, parameter or return type.
- Reference (pin `33abf7a`): a union is `typedef struct Name Name;`
  (`c_backend.orb:1678-1686`) and `Name*` in every value position
  (`mapTypeToC`, `:524-526`), the cell D-M1-12 already lowers as a `void*`
  word. A match arm `Circle(r) => ...` reads the cell's data word
  (`union_get_data`, `:3721-3724`) into a register the builder leaves
  `unknown`, then binds it with `decl_var r`.
- Decision, three parts that the same programs need together:
  1. `canonicalize_union_types` rewrites a declared union name in parameter,
     return and register types to `void*`. The type table keeps the name,
     which resolves `Name_TAG_Variant`.
  2. A declared function admits a `void*` parameter and an `arg` admits a
     `void*` value; both are the word every non-float parameter already is.
     A `void*` return stays refused (D-M1-44), and an extern still takes
     scalars only.
  3. A `union_get_data` destination that is `unknown` takes the type of the
     local it initializes, read off that local's `load_var` destinations, as
     D-M1-43 does for `result_unwrap`; the type must be a payload D-M1-12
     admits (`int`, `bool`, `string`). A payload no local binds stays
     `unknown`.
- Fixture `55-union-parameter`: a four-variant union (`Circle(int)`,
  `Square(int)`, `Label(string)`, `Empty`) passed to `area` and `describe`,
  whose matches bind `int` and `string` payloads and use a wildcard arm.
  Oracle and R6 both print `shape orbit\r\n` and exit 42. A unit variant is
  constructed as `Empty()`; a bare `Empty` is `Undeclared identifier` at the
  pin.
- Controls: `x-union-param-extern` and `x-union-payload-unread` refuse.
- Measured: differential **53/53 -> 54/54**; loader **223/223 -> 226/226** (156
  rejections + 70 controls; `make_cases.py --check` 197 files without drift);
  corpus **63 build / 63 match -> 64 build / 64 match over 97 programs**:
  `match` matches byte for byte; `member_resolution` moves to `ret value does
  not match function return type`, `union_bare_variant_pattern` to `returning
  void* values`, `union_match_exhaustive` to `register type unknown`. The
  ratchet is re-taken at 64/64. Census **0/26**, unchanged groups; the census
  baseline is re-taken with the same counts.
- Status: **applied**.

## D-M1-48 - A runtime extern returns a result or a list

- Date: 2026-10-26. Pin: `33abf7a` (D-F0-14). `extern return type is
  unsupported in M1` was the first refusal of four std programs (`test_fs`,
  `test_http_client`, `test_json`, `test_random`) and of three census modules
  (`host/extern`, `selfhost/lexer_trace`, `tools/palette`).
- Reference (pin `33abf7a`, `runtime/orbit_api.h`): the four result externs
  the corpus declares are `OrbitResult orbit_json_parse(OrbitArena*, const
  char*)` (`:589`), `orbit_file_read` (`:599`), `orbit_random_bytes` (`:619`)
  and `orbit_http_client_fetch` (`:722`); all four take the arena first
  (`functionNeedsArena`, `c_backend.orb:4416-4437`). The list externs return
  `OrbitList*` (`orbit_file_list_dir`, `:601`).
- Decision: an extern may return `list` (one word in `rax`) or `result`. A
  result extern must be in the arena-taking runtime family: Win64 passes the
  caller's result cell in `rcx`, the arena moves to `rdx`, and the IR
  arguments start at position 2. The `result_unwrap` rules of D-M1-43 apply
  unchanged. An `object` return stays refused.
- Fixture `56-extern-result-list`: `orbit_file_read` of a missing file takes
  the `catch` path, `orbit_random_bytes(16)` takes the `ok` path, and
  `orbit_file_list_dir` of a missing directory returns an empty list. Oracle
  and R6 both print `read failed\r\n0\r\n` and exit 42. The fixture does not
  print the random value: it is raw bytes in a `string`, and `len()` is
  `strlen`, which stops at the first zero byte (it printed `1` in one run).
- Controls: `x-extern-result-no-arena` and `x-extern-return-object` refuse.
- Measured: differential **54/54 -> 55/55**; loader **226/226 -> 229/229** (158
  rejections + 71 controls; `make_cases.py --check` 199 files without drift);
  corpus **64 build / 64 match -> 65 build / 65 match over 97 programs**:
  `test_http_client` matches byte for byte; `test_fs` moves to `list_push`,
  `test_json` to `extern parameter type`, `test_random` to `register type
  unknown`. The ratchet is re-taken at 65/65. Census **0/26**: the
  `extern return type` group of 3 is gone; `no non-extern main` goes 2 -> 3,
  `ret value does not match` 2 -> 3 and `register type unknown` 0 -> 1. The
  census baseline is re-taken.
- Status: **applied**.

## D-M1-49 - A bare `ret` in a value function returns the zero of the type

- Date: 2026-10-26. Pin: `33abf7a` (D-F0-14). `ret value does not match
  function return type` stopped three census modules and, after D-M1-47,
  `tests/suite/member_resolution.orb`: a `ret` with no operand at the end of an
  `int` function, after a `match` whose arms all return.
- Reference (pin `33abf7a`): the `ret` arm of the C backend lowers a `None`
  operand to `return 0;` for `int`, `bool`, an enum and `main`, `return 0.0;`
  for `float`, `return;` for `void`, and `return NULL;` for any other type.
- Decision: a bare `ret` in a value function returns `0` in `eax` (which also
  clears `rax`, the `NULL` of a pointer return) or `0.0` in `xmm0` for a
  `float` function. A `result` function stays refused: `return NULL;` is not
  an `OrbitResult` value.
- Fixture `57-bare-return`: `pos`, `half`, `name` and `flag` return only inside
  an `if`, so their fall-through is the bare `ret` (one per function in the
  IR). `pos(-5)` is 0, `half(-1.0)` is 0.0, `flag(2)` is false, and
  `name(1)` prints `one`. Oracle and R6 both print `one\r\n` and exit 42. The
  language accepts a value function that can fall off its end without a
  diagnostic; that is reported on the language side.
- Controls: `x-bare-ret-result` refuses.
- Measured: differential **55/55 -> 56/56**; loader **229/229 -> 231/231** (159
  rejections + 72 controls; `make_cases.py --check` 200 files without drift);
  corpus **65 build / 65 match**, unchanged: `member_resolution` moves to
  `register type unknown`. Census **0/26**: the `ret value does not match`
  group of 3 is gone; `no non-extern main` goes 3 -> 5 and `main must have no
  parameters and return int` appears with 1. The census baseline is re-taken.
- Status: **applied**.

## D-M1-50 - `Result` and `OrbitResult` are the result type

- Date: 2026-10-26. Pin: `33abf7a` (D-F0-14). `tests/suite/type_spellings.orb`
  stopped at `register type Result is unsupported in M1`.
- Reference (pin `33abf7a`): `mapTypeToC` answers `OrbitResult` for `result`,
  `Result` and `OrbitResult` alike (`c_backend.orb:502`), and the program's own
  comment records that the capitalised container spellings are the same types.
- Decision: `canonicalize_result_types` rewrites `Result` and `OrbitResult` to
  `result` in parameter, return and register types, as D-M1-32 does for the
  list spellings. Any other spelling stays refused.
- Fixture `58-result-spelling`: a function declared `-> Result`, a local
  `val good: Result`, and two `Result` locals in `main` read through `try` and
  `catch e`. The emitted IR carries `Result` in its register and return types.
  Oracle and R6 both print `neg\r\n` and exit 42.
- Controls: `x-result-spelling-other` (`RESULT`) refuses.
- Measured: differential **56/56 -> 57/57**; loader **231/231 -> 233/233** (160
  rejections + 73 controls; `make_cases.py --check` 201 files without drift);
  corpus **65 build / 65 match**, unchanged: `type_spellings` moves to
  `register type unknown`, an opaque list read compared with an int (D-M1-51).
  Census **0/26**, ratchet holds.
- Status: **applied**.

## D-M1-51 - An opaque list read compared with an int is an int

- Date: 2026-10-26. Pin: `33abf7a` (D-F0-14). D-M1-38 typed an opaque read
  (`elemKind 0`) only by a parameter it feeds. Two corpus programs
  (`list_push`, `type_spellings`) stopped at an opaque read whose one use is
  `ne` against an `int` literal.
- Reference (pin `33abf7a`): `eq` and `ne` compare two words as
  `(uintptr_t)a == (uintptr_t)b`, and `lt`/`le`/`gt`/`ge` as `intptr_t`
  (`c_backend.orb:2747-2790`), unless both sides are strings (`strcmp`) or one
  is a float. An `int` element is stored sign-extended into its slot word
  (`orbit_list_push_int`), so its low 32 bits sign-extended compare the same
  way as the word.
- Decision: an opaque read whose one reader is `eq`, `ne`, `lt`, `le`, `gt`
  or `ge`, and whose other operand is an `Int` literal or an `int` register,
  is typed `int`. A comparison with a string compares pointers in the pin, not
  content, and stays `unknown`; so does any other use.
- Fixture `59-opaque-compare`: three functions take `xs: list` and compare
  `xs.get(i)` with an `int` parameter (`>`), an `int` parameter (`==`) and
  `-1`, over `[4, -1, 9, 2147483647, -7]`. Every `list_get` in the IR has
  `elemKind 0`. Oracle and R6 both print `3\r\n` and exit 42.
- Controls: the D-M1-38 control `x-opaque-read-compare` compared with `3`,
  which this decision admits; it now compares with the string `"x"` and stays
  refused.
- Measured: differential **57/57 -> 58/58**; loader **233/233 -> 234/234** (160
  rejections + 74 controls; `make_cases.py --check` 201 files without drift);
  corpus **65 build / 65 match -> 67 build / 67 match over 97 programs**:
  `list_push` and `type_spellings` match byte for byte. The ratchet is
  re-taken at 67/67. Census **0/26**, ratchet holds.
- Status: **applied**.

## D-M1-52 - An int negates at 32 bits

- Date: 2026-10-26. Pin: `33abf7a` (D-F0-14). Found while measuring the
  corpus at orbit-lang `d58d647` for the next pin: `tests/suite/int_wrap_32.orb`
  built under R6 and exited 1 where the oracle exits 0, at
  `add(2147483647, 1) != -2147483648`.
- Root cause: the builder lowers the literal `-2147483648` as
  `load_const -2147483648` (the literal `2147483648` wrapped to 32 bits)
  followed by `neg`. R6 negated with `negq`, so the slot held `+2147483648`;
  the oracle's C negates an `orbit_int` and answers `-2147483648`.
- This corrects D-M1-36, which kept `neg` at `negq` because a probe showed the
  oracle folding a comparison on `-INT32_MIN` under its no-overflow assumption.
  When the operand arrives through a register, as here, the oracle's result is
  the 32-bit wrap.
- Fix: an `int` `neg` is `negl %eax` followed by `movslq %eax, %rax`, the form
  D-M1-36 gave `add`, `sub` and `mul`. The encoder gains `negl` (`F7 /3`, with
  REX.B for `%r8d`-`%r15d`); `tests/coff/asm_probe.c` and GNU `as` disassemble
  `negl` on `%eax`, `%ecx`, `%r9d` and `%r11d` to the same bytes. A float `neg`
  is unchanged.
- Fixture `60-int-neg-wrap`: `add(2147483647, 1)` compared with a local and a
  literal `-2147483648`, `neg(low)` equal to `low`, `neg(-5)` and
  `neg(2147483647)`. Oracle and R6 both print `-2147483648\r\n` and exit 42;
  the driver without the fix exits 10.
- Admission: none. The loader suite gains the fixture as an acceptance control.
- Measured: differential **58/58 -> 59/59**; loader **234/234 -> 235/235** (160
  rejections + 75 controls); corpus at the pin **67 build / 67 match**, no
  verdict moves; census **0/26**, ratchet holds.
- Status: **applied**.

## D-F0-15 - Re-pin to a5cdf77: the first public pin, typed registers

- Date: 2026-10-26. Moves the pin from `33abf7a` (D-F0-14) to orbit-lang
  `main` `a5cdf779ddc0ba37f72eb0a9dcfd750044b3430c`, 43 commits. The commit is on
  the public remote `https://github.com/joacokhzyx/orbit-lang`
  (`3724b20..a5cdf77` pushed on 2026-10-26), so the corpus and the gates can be
  reproduced from public bytes for the first time.
- What moved, from four merges:
  - `corpus-growth`: 25 suite programs (models, unions, results, strings,
    lists, memory churn, compiler-shaped programs).
  - `build-speed`: the bootstrap compiles at `-O1`, stops at iteration 1 when
    it equals the canonical, and shards the compile; no compiler source moved.
  - `lang-fixes`: eight compiler fixes with nine suite programs. Two change
    the IR: `&&`/`||` lower to `copy`, `jump_if_false`, `jump` and `label`
    around the right operand (`and_op`/`or_op` have no producer), and a
    multi-payload `match` reads its fields after the tag test.
  - `typed-ir`: the builder writes the register types it proves. The corpus
    goes from 8045 `unknown` register destinations to 425
    (`scripts/ir_unknown_census.py`).
- Inputs at the pin (`docs/PIN.md`): fixed point
  `C:\Temp\orbit-fp-b0d70c396d.exe`, built from `C:\Temp\orbit-pin-a5cdf77-git`,
  converges to `PUBLISHED_C D78411C0.` (was `138FBDF4.`); with it the pinned tree
  passes `tests/suite` 109/109 and `tests/std` 23/23. The runtime archive
  rebuilt from the pinned recipe defines 337 symbols, the same exported names
  as before. The corpus regenerates to 133 documents, 6808416 bytes,
  `filesSha256 91341401.`.
- Measurement before the move: at the intermediate `d58d647` the corpus built
  90 of 132 programs and one differed (`int_wrap_32`), which D-M1-52 fixed
  before this commit. At `a5cdf77` no program differs.
- Ratchets: the corpus baseline is re-taken at `r6BuildOk 91`, `match 91`.
  `run_corpus.py --check --write-baseline` refuses to write across corpora
  (`the baseline was taken against a different corpus`), so the baseline is
  written from that run's `--json` report with the same fields the script
  writes, and a second `--check` run against it prints `ratchet holds: 91 -> 91`.
  The census baseline is re-taken: 27 modules (one more at the pin), 0 build.
- The CI workflow's `ORBIT_PIN` moves with the pin, and every command in
  `AGENTS.md`, `README.md`, `CONTRIBUTING.md` and `tests/corpus/README.md` points
  at the new fixed point and checkout.
- Measured: differential **59/59** against the new oracle; loader **235/235**;
  `make_cases.py --check` without drift; corpus **67 build / 67 match over 97
  -> 91 build / 91 match over 132 programs**, none differing; census **0 build
  over 26 -> 0 build over 27 modules**, first refusals `union ASTNode` 14,
  `no non-extern main` 6, `union IRValue` 4, `list_push` 1, `main must have no
  parameters` 1, `register type unknown` 1.
- Status: **applied**.

## D-M1-53 - A multi-payload arm reads its pack

- Date: 2026-10-27. Pin: `a5cdf77` (D-F0-15).
- Root cause: since D-F0-15 a multi-payload `match` arm reads its data word
  only through `load_field <Union>_<Variant>._i`, after the tag test. The
  builder leaves the `union_get_data` destination and every part `unknown`,
  so `tests/suite/union_multi_payload_unit_variant.orb` stopped at
  `register type unknown`.
- Decision: a `union_get_data` destination whose every reader is a
  `load_field` naming a pack field of a declared union is the pack pointer
  (`void*`). A pack `load_field` destination takes the part's declared type,
  inside the D-M1-12 payload vocabulary (`int`, `bool`, `string`). The lowering
  is the D-M1-12 one; only the types are inferred.
- Boundary: a data word with any other reader, and a word read through a field
  reference that names no pack, keep no type and are refused (CGEN,
  `x-pack-word-other-reader`, `x-pack-word-model-reader`). A read nothing binds
  stays refused (D-M1-47, `x-union-payload-unread`).
- Fixture `61-union-pack-arm`: arms with `int, int`, `int, string` and
  `bool, int` packs and a unit arm. Oracle and R6 both print `pack\r\n` and
  exit 42.
- Measured: differential **59/59 -> 60/60**; loader **235/235 -> 238/238**
  (162 rejections + 76 controls); corpus **91 -> 92 build / 92 match over 132
  programs** (`union_multi_payload_unit_variant`), none differing; census
  **0/27**, ratchet holds.
- Status: **applied**.

## D-F0-16 - Re-pin to ca3eb91: typed unwraps, payloads and list reads

- Date: 2026-10-27. Moves the pin from `a5cdf77` (D-F0-15) to orbit-lang
  `main` `ca3eb9103dfcff03f11831eddb8be98bc521b787`, four commits, pushed to
  `https://github.com/joacokhzyx/orbit-lang` (`a5cdf77..ca3eb91`).
- What moved: the `typed-ir-2` merge. The builder types a `try` unwrap from
  the annotation of the `val` it feeds, a single `int`/`bool`/`string` union
  payload from the variant, and a list read whose element is a list or a
  declared model. The C backend casts the word for those registers. The
  corpus goes from 425 `unknown` register destinations to 341
  (`scripts/ir_unknown_census.py`). The serializer (`ir_emit.orb`), the corpus
  script and `runtime/` did not move, so the runtime archive is kept.
- Inputs at the pin (`docs/PIN.md`): fixed point
  `C:\Temp\orbit-fp-33ea94e9ec.exe`, built from `C:\Temp\orbit-pin-ca3eb91-git`,
  converges to `PUBLISHED_C BA1304FD.` (was `D78411C0.`); with it the pinned tree
  passes `tests/suite` 109/109 and `tests/std` 23/23. The corpus regenerates
  to 133 documents, 6808178 bytes, `filesSha256 3f20ced0.`.
- Ratchets: the corpus baseline is written by `run_corpus.py --write-baseline`
  at the pin, and a `--check` run against it holds at 95 -> 95. The census
  baseline is re-taken: 27 modules, 0 build, the same first refusals.
- Measured: differential **60/60** against the new oracle; loader **238/238**;
  corpus **92 build / 92 match -> 95 build / 95 match over 132 programs**
  (`result_chain`, `result_try`, `union_match_exhaustive`), none differing;
  census **0 build over 27 modules**.
- Status: **applied**.

## D-M1-54 - A model or union payload is a pointer word

- Date: 2026-10-27. Pin: `ca3eb91` (D-F0-16).
- Root cause: D-M1-12 admitted `int`, `bool` and `string` payloads only. A
  payload naming a model (`Deposit(Account, int)`) or a union
  (`Apply(Op, int)`, `Program(ProgramNode)` in `ASTNode`) was the first
  refusal of `union_model_payload` and `union_multi_payload` in the corpus and
  of 14 compiler modules in the census (`union ASTNode variant Program`).
- Reference (pin `ca3eb91`): `mapTypeToC` gives a capitalised name `Name*`
  (`c_backend.orb:511-527`). A single payload is stored in the data word as
  `(void*)(uintptr_t)(x + 0)`, and a pack field holds the pointer
  (`c_backend.orb:103-131`).
- Decision: a payload part that names a declared model or union is an 8-byte,
  8-aligned pointer word. The constructor stores it with `movq`, a pack read
  loads it with `movq`, and its register type is the model name or `void*` for
  a union cell (D-M1-47). A match binding of a model payload takes the type of
  the local's loads, as D-M1-47 does for scalars.
- Boundary: a payload naming no declared model or union stays refused
  (`x-union-payload-undeclared`), and an `int` passed for a model payload is
  refused (`x-union-payload-model-mismatch`). Float payloads and arity above 2
  stay refused (D-M1-12).
- Fixture `62-union-named-payload`: a model in a pack (`Deposit`), a model
  alone (`Open`) and a union in a pack (`Apply(Op, int)`) with a nested match.
  The mutation through the payload is visible to the caller. Oracle and R6
  both print `named\r\n` and exit 42.
- Measured: differential **60/60 -> 61/61**; loader **238/238 -> 241/241**
  (164 rejections + 77 controls); corpus **95 -> 97 build / 97 match over 132
  programs** (`union_model_payload`, `union_multi_payload`), none differing;
  census **0/27**: the 14 `ASTNode` modules move to
  `model FieldInitNode field value has unsupported type ASTNode` and the four
  `IRValue` modules to `model IRInstruction field opcode has unsupported type
  IROpcode`.
- Status: **applied**.

## D-M1-55 - A union return is its cell word

- Date: 2026-10-27. Pin: `ca3eb91` (D-F0-16).
- Root cause: D-M1-44 admitted model returns and kept a `void*` (union cell)
  return refused, without a measured reason. `returning void* values is not
  supported in M1` was the first refusal of `mini_parser`,
  `union_bare_variant_pattern` and `union_payload_return` in the corpus; 41
  compiler functions return `ASTNode` and 9 return `IRValue`.
- Reference (pin `ca3eb91`): `mapTypeToC` gives a union `Name*`, a pointer
  returned in `rax`.
- Decision: a function may return `void*`, the D-M1-12 cell word of a union.
  The `ret` of a word register already moves the slot into `rax`, a call
  already stores `rax` into a `void*` destination, and a bare `ret` returns 0
  (D-M1-49), the pin's `NULL`. Only the signature check changes.
- Control: `x-return-union-cell` is retargeted from D-M1-44: a union function
  whose `ret` returns an `int` register is refused.
- Fixture `63-union-return`: a factory over three variants, a function that
  re-builds or returns its argument, nested `grow(grow(make(...)))`. Oracle and
  R6 both print `shapes\r\n` and exit 42.
- Measured: differential **61/61 -> 62/62**; loader **241/241 -> 242/242**
  (164 rejections + 78 controls); corpus **97 -> 100 build / 100 match over
  132 programs** (`mini_parser`, `union_bare_variant_pattern`,
  `union_payload_return`), none differing; census **0/27**, ratchet holds.
- Status: **applied**.

## D-M1-56 - A model or union field is a pointer word

- Date: 2026-10-27. Pin: `ca3eb91` (D-F0-16).
- Root cause: model fields were `int`, `bool`, `float`, `string` and `list`
  (D-M1-11, D-M1-39). After D-M1-54 the first refusal of 14 compiler modules
  was `model FieldInitNode field value has unsupported type ASTNode`, of four
  more `model IRInstruction field opcode has unsupported type IROpcode`, and of
  the corpus program `model_nested` a model field of model type.
- Reference (pin `ca3eb91`): `mapTypeToC` gives a model or union field
  `Name*` (`c_backend.orb:511-527`), one pointer member of the struct.
- Decision: a field that names a declared model is an 8-byte, 8-aligned
  pointer member; a field that names a declared union takes the cell spelling
  `void*` (D-M1-47) and the same layout. The constructor, `load_field` and
  `store_field` move it with `movq`. A self-referential field (`Box.p: Box`) is
  one such pointer.
- Controls: `x-model-field-unsupported-type` is retargeted to a field naming no
  declared type; `x-model-field-model-store-int` and
  `x-model-field-union-ctor-int` refuse an `int` written into a model field and
  passed for a union field.
- Fixture `64-model-word-fields`: a segment with two `Point` fields and an
  `Expr` field, a `BinNode` holding two `Expr`s, reads through `s.to.x`,
  rewrites of both fields and a recursive evaluation. Oracle and R6 both print
  `ab\r\n` and exit 42.
- Measured: differential **62/62 -> 63/63**; loader **242/242 -> 245/245**
  (166 rejections + 79 controls); corpus **100 -> 101 build / 101 match over
  132 programs** (`model_nested`), none differing; census **0/27**, first
  refusals `register type unknown` 10, `list_push` 9, `no non-extern main` 7,
  `main must have no parameters` 1.
- Status: **applied**.

## D-M1-57 - A ref list holds pointer words

- Date: 2026-10-27. Pin: `ca3eb91` (D-F0-16).
- Root cause: `elemKind 5` (ref) was outside the subset (D-M1-16 E-03,
  D-M1-30): a list of lists, models or unions could not be created, pushed or
  read. `list_element_type` stopped at `register type unknown` on its ref read,
  and the compiler's own IR (`compiler/host/main.orb`) carries 145 ref reads
  (144 `list_get`, one `list_pop`) and 50 ref writes (48 `list_push`, two
  `list_set`).
- Reference (pin `ca3eb91`): the C backend reads a ref element with
  `orbit_list_read_ref`, which checks `ORBIT_ELEM_REF` (5) and returns the slot
  word, and writes it with `orbit_list_push_ref`/`orbit_list_set_ref`
  (`c_backend.orb:605-662`; `orbit_api.h:521-525`; `collections.c:272-276,449`).
- Decision: `list_create` admits kind 5. A ref element is a pointer word: a
  `list`, a declared model or a `void*` union cell. `list_get` and `list_pop`
  with kind 5 use the inline read of D-M1-38 with the kind check against 5 and
  store the slot word; `list_push` and `list_set` call
  `orbit_list_push_ref`/`orbit_list_set_ref`. A ref read the builder leaves
  `unknown` takes the type of the local it is bound to, as D-M1-43 does for an
  unwrap; a read nothing loads stays unknown.
- Controls: `x-list-create-kind-ref` is retargeted from D-M1-30 to an `int`
  pushed into a ref list, and `x-list-get-ref-into-int` refuses a ref read into
  an `int` destination.
- Fixture `65-ref-lists`: a `list<Item>` pushed, read in a callee, replaced
  with `set` and popped, and a `list<Tok>` of union cells read and matched.
  Oracle and R6 both print `a\r\n` and exit 42.
- Measured: differential **63/63 -> 64/64**; loader **245/245 -> 247/247**
  (167 rejections + 80 controls); corpus **101 -> 103 build / 103 match over
  132 programs** (`list_element_type`, `test_collections_boxed`), none
  differing; census **0/27**, first refusals `register type unknown` 11,
  `list_push` 8, `no non-extern main` 7, `main must have no parameters` 1.
- Status: **applied**.

## D-M1-58 - An untyped list moves bare words

- Date: 2026-10-27. Pin: `ca3eb91` (D-F0-16).
- Root cause: an `elemKind 0` list admitted only its create (D-M1-30) and a
  read typed from its use (D-M1-38, D-M1-51). A push or set of an untyped
  element, and a read into a destination the builder already typed, were
  refused. `generic_nested_argument` stopped at its untyped read into a list,
  and the compiler's IR has 125 untyped pushes and four untyped sets
  (`compiler/host/main.orb`), most from model fields declared bare `list`.
- Reference (pin `ca3eb91`): an untyped push is `uintptr_t _tmp =
  (uintptr_t)(v); orbit_list_push(list, &_tmp)`, a set the same through
  `orbit_list_set`, and a read `orbit_list_read_word` converted by
  `generateRegAssign`: `(orbit_int)(uintptr_t)` for an int, `(orbit_string)` for
  a string and `(void*)(uintptr_t)` for a pointer (`c_backend.orb:605-640`).
- Decision: an untyped element is a word of type `int`, `string`, `list`,
  model or `void*`. A push or set passes the address of the value's slot to
  `orbit_list_push`/`orbit_list_set`; an R6 int slot already holds the
  sign-extended value, which is the pin's `(uintptr_t)` of an `orbit_int`. A
  read or pop into such a destination uses the opaque read of D-M1-38 and
  decodes an int from the low 32 bits and every other word as itself. `bool`
  and `float` keep out: their conversions through the word are not measured.
- Controls: the three D-M1-30 controls `x-list-get-kind-opaque`,
  `x-list-set-kind-opaque` and `x-list-push-kind-opaque` are retargeted to a
  bool read, a bool set and a float push, which stay refused.
- Fixture `66-opaque-lists`: a list of lists pushed and read untyped, and a
  model with two bare `list` fields filled with strings and ints, read through
  a string parameter and int comparisons, and rewritten with `set`. Oracle and
  R6 both print `second\r\n` and exit 42; the driver at D-M1-57 refuses it at
  the first untyped push.
- Measured: differential **64/64 -> 65/65**; loader **247/247 -> 248/248**
  (167 rejections + 81 controls); corpus **103 -> 104 build / 104 match over
  132 programs** (`generic_nested_argument`), none differing; census **0/27**,
  first refusals `register type unknown` 17, `no non-extern main` 8,
  `main must have no parameters` 1, `register type NULL` 1.
- Status: **applied**.

## D-M1-59 - NULL is the zero word and pointer words compare

- Date: 2026-10-27. Pin: `ca3eb91` (D-F0-16).
- Root cause: the builder loads the `NULL` literal as `load_const Sym:"NULL"`
  into a register typed `"NULL"`. R6 read the symbol as a function value and
  refused the type. The compiler's IR at orbit-lang `bd60022` has 108 such
  loads: 101 feed an `eq`/`ne` against a model, a union or a list, 6 a `ret`
  and one a `list_push`. `eq`/`ne` between two pointer words were refused
  too; `compiler/ir/cteval.orb` stopped at `register type NULL`.
- Reference (pin `ca3eb91`): `mapTypeToC` declares `"NULL"` as `void*`
  (`c_backend.orb:472`), the literal is the C `NULL`, and `eq`/`ne` compare
  `(uintptr_t)a == (uintptr_t)b` when neither side is a string or a float.
- Decision: the register spelling `"NULL"` is `void*`, and `load_const NULL`
  stores the zero word. `eq` and `ne` admit two pointer words (a `list`, a
  model or a `void*` cell) and lower to the 64-bit `cmpq` the int compare
  already uses. A function returning a model, a list or a union may return a
  register whose only definition is `load_const NULL`.
- Controls: `x-null-into-int` (NULL into an `int` register),
  `x-eq-word-int` (a model compared with an `int`) and
  `x-ret-cell-from-model-fn` (a union cell returned from a model function)
  stay refused.
- Fixture `67-null-words`: a linked list ended by `NULL` from a model
  function, walked with `cur != NULL`, and models compared with `==`. Oracle
  and R6 both print `null\r\n` and exit 42. Building the fixture found an
  oracle defect: a local named `mid` holding a model is assigned through
  `(orbit_int)(uintptr_t)` and the walk faults, so the fixture names it
  `second`.
- Measured: differential **65/65 -> 66/66**; loader **248/248 -> 252/252**
  (170 rejections + 82 controls); corpus **104 build / 104 match over 132
  programs**, no verdict moves; census **0/27**, `compiler/ir/cteval.orb`
  moves from `register type NULL` to `register type unknown`.
- Status: **applied**.

## D-M1-60 - indexOf and find call orbit_string_indexOf

- Date: 2026-10-27. Pin: `ca3eb91` (D-F0-16).
- Root cause: the builder emits `s.indexOf(t)` as a call to
  `orbit_string_indexOf`, `s.find(t)` as `orbit_string_find` or a bare `find`,
  and declares none of them. R6 refused them as undeclared targets: 61
  functions of the compiler's IR at orbit-lang `bd60022`, and the corpus
  program `user_fn_named_find`.
- Reference (pin `ca3eb91`): `orbit_int orbit_string_indexOf(orbit_string,
  orbit_string)` (`orbit_api.h:413`), defined in the archive. The C backend
  renames `orbit_string_find`, and `find` or `indexOf` when no user function
  has the name, to it (`c_backend.orb:3001-3006`).
- Decision: the D-M1-29 intrinsic table gains `orbit_string_indexOf`,
  `orbit_string_find`, `find` and `indexOf`, each `(string, string) -> int`, and
  all four call `orbit_string_indexOf`. A module function with one of these
  names is resolved from its declaration first, as for every intrinsic. The
  `int` return is sign-extended from `eax` (D-M1-36).
- Controls: `x-index-of-int-arg` (an `int` argument) and `x-find-arity` (one
  argument) stay refused.
- Fixture `68-string-index`: `indexOf` found and not found, `find`, a call
  through a function, and an empty haystack. Oracle and R6 both print
  `found\r\n` and exit 42.
- Measured: differential **66/66 -> 67/67**; loader **252/252 -> 255/255**
  (172 rejections + 83 controls); corpus **104 -> 105 build / 105 match over
  132 programs** (`user_fn_named_find`), none differing; census **0/27**.
- Status: **applied**.

## D-M1-61 - A void main exits 0

- Date: 2026-10-27. Pin: `ca3eb91` (D-F0-16).
- Root cause: R6 required `main` to return `int`. The compiler's own entry
  points declare `fn main()`: `compiler/host/main.orb` and
  `compiler/selfhost/lexer_trace.orb`, whose first refusal was
  `main must have no parameters and return int in M1`.
- Reference (pin `ca3eb91`): the C backend emits every `main` as
  `int orbit_main(OrbitArena* arena)` (`isMain` sets `retType "int"`), and a
  `return` in a void `main` becomes `return 0;`; measured on a void main with
  an early `return`, the oracle exits 0.
- Decision: `main` may return `void`. Every `ret` in a void `main` leaves 0
  in `eax`, which the entry wrapper turns into the exit code. `main` still
  takes no parameters.
- Controls: `x-main-returns-string` and `x-main-with-param` stay refused.
- Fixture `69-void-main`: a void `main` that calls a recursive function and
  prints. Oracle and R6 both print `void main\r\n` and exit 0.
- Measured: differential **67/67 -> 68/68**; loader **255/255 -> 258/258**
  (174 rejections + 84 controls); corpus **105 build / 105 match over 132
  programs**, no verdict moves; census **0 -> 1 build over 27 modules**:
  `compiler/selfhost/lexer_trace.orb` builds. Run from the pinned tree, the
  R6 build and the oracle build print the same 400 token lines of
  `compiler/host/main.orb` byte for byte and both exit 0; the R6 executable is
  310769 bytes, the oracle's 442368. The census baseline is re-taken at 1.
- Status: **applied**.

## D-M1-62 - s.length on a string is its byte length

- Date: 2026-10-27. Pin: `ca3eb91` (D-F0-16).
- Root cause: the builder emits `s.length` (and `s.len` without a call) as
  `load_field <s> "length"`, and R6 accepted `load_field` only on a model
  receiver. In the compiler's IR at orbit-lang `cf770ed` it was the first
  refusal of `tirEscape` (`compiler/tir/format.orb`).
- Reference (pin `ca3eb91`): the C backend lowers a `load_field` named
  `length` or `len` on a string receiver to `orbit_string_len(s)`, whose header
  body is `s ? strlen(s) : 0` (`orbit_api.h:411-413`; D-M1-29 lowers the same
  body for `list_len` on a string).
- Decision: a `load_field` whose receiver is a string register and whose field
  is `length` or `len` reads `strlen` of the pointer, 0 for a null pointer,
  into an `int` destination. A destination the builder leaves `unknown` takes
  `int`. Any other field on a string stays refused.
- Controls: `x-string-length-into-string` (a `string` destination) and
  `x-string-field-size` (the field `size`) stay refused.
- Fixture `70-string-length-field`: `s.length` in a `while` bound that counts
  quotes with `at`, compared with a literal, and on the empty string. Oracle and
  R6 both print `length\r\n` and exit 42.
- Measured: differential **68/68 -> 69/69**; loader **258/258 -> 261/261**
  (176 rejections + 85 controls); corpus **105 build / 105 match over 132
  programs**, none differing; `member_resolution` moves from its `.length`
  read to `call target system_uptime`. Census **1/27**, ratchet holds.
- Status: **applied**.

## D-M1-63 - The assembler grows its tables

- Date: 2026-10-27. Pin: `ca3eb91` (D-F0-16).
- Root cause: `asm_win64.c` kept labels (2048), procedures (256), fixups
  (2048) and `.globl` names (512) in fixed arrays and found a label by a linear
  scan. `compiler/host/main.orb` at orbit-lang `0cfce74` (1038 functions)
  passed validation and stopped at `line 18625: too many assembler relocations
  (limit 2048)`.
- Decision: the four tables are heap arrays that double when full, and labels
  and globals are found through an open-addressing FNV-1a hash of the name.
  The instruction encoding and the order of labels, and so the object bytes,
  do not change. The `Asm` state no longer sits on the stack.
- Measured: loader **261/261**, differential **69/69**, corpus **105 build /
  105 match**, census **1/27**, all unchanged. Built from orbit-lang `0cfce74`
  (not the pin), `compiler/host/main.orb` emitted by that tree's fixed point
  builds under R6 in 42 s into a 3357756-byte executable. That executable:
  - prints `orbit 0.1.0-rc.2`;
  - emits the same IR for `tests/suite/union_model_payload.orb` as the oracle;
  - translates `compiler/host/main.orb` to C byte-identical to the oracle's
    (6212022 bytes, sha256 `54f0b797…`, the tree's `PUBLISHED_C`), in 9345 ms
    against the oracle's 5423 ms;
  - emits IR for `compiler/host/main.orb` identical to the IR it was built
    from, and R6 builds that IR into a byte-identical executable (sha256
    `2b8cb011…`).
- Status: **applied**. The census reaches `compiler/host/main.orb` at the
  D-F0-17 pin (`0cfce74`).

## D-F0-17 - Re-pin to 0cfce74: R6 builds the Orbit compiler

- Date: 2026-10-27. Moves the pin from `ca3eb91` (D-F0-16) to orbit-lang
  `main` `0cfce745cd8f3b566a58e5ce9f8fc3c51194ca84`, 22 commits, pushed to
  `https://github.com/joacokhzyx/orbit-lang` (`ca3eb91..0cfce74`).
- What moved, in four merges:
  - `typed-ir-3`: model and union payload reads, union and list fields,
    `orbit_string_len`/`indexOf` calls and string concatenation are typed.
  - `typed-ir-4`: the compiler's own lists declare `list<T>`; the IR of
    `compiler/host/main.orb` has 0 `unknown` register destinations (3446 at
    `ca3eb91`).
  - `typed-ir-5`: a local re-declared with another type gets a new name, so
    every local has one type; `ScheduleDeclNode.handler` is an `ASTNode`; and
    three emitter defects found by R6 fixtures are fixed with regression tests
    (a variant named `Tag`, a local ending in `id` cast through `orbit_int`, a
    constant fold that dropped a call returning `NULL`).
  - `sema-upper-check`: `checkNode` no longer compares two string addresses
    with `>=`.
- The serializer changed by one parameter annotation (`ir_emit.orb` blob
  `b52333e7`); the IR format is unchanged. `runtime/` and the corpus script did
  not move, so the runtime archive is kept.
- Inputs at the pin (`docs/PIN.md`): fixed point
  `C:\Temp\orbit-fp-5d5aa27fa7.exe`, built from `C:\Temp\orbit-pin-0cfce74-git`,
  converges to `PUBLISHED_C 54F0B797.`; with it the pinned tree passes
  `tests/suite` 112/112 and `tests/std` 23/23. The corpus regenerates to 136
  documents, 6829729 bytes, `filesSha256 0f96a33f.`.
- Ratchets: the corpus baseline is written by `--write-baseline` at the pin and
  a `--check` run holds at 109 -> 109. The census baseline is re-taken.
- Measured: differential **69/69** against the new oracle; loader **261/261**;
  corpus **105 build / 105 match over 132 -> 109 build / 109 match over 135
  programs**, none differing; census **1 -> 2 build over 27 modules**: both
  entry points build, `compiler/host/main.orb` (the whole compiler) and
  `compiler/selfhost/lexer_trace.orb`. The other 25 modules are libraries whose
  only refusal is `module must define one non-extern main`.
- Status: **applied**.

## D-M1-64 - R6 builds the compiler in 5 s

- Date: 2026-10-27. Pin: `0cfce74` (D-F0-17).
- Root cause: `r6 build` took 41 s on `compiler/host/main.orb` (17.6 MB of IR,
  1038 functions). Timed per phase with a build of the driver that reads
  `clock()` between phases, the cost sat in six places:
  - `r6_coff_write` advanced its cursor by `count * 10` bytes of relocations
    after it had replaced `count` with the section's file offset. The object
    was 211 MB for 3.4 MB of content and was written zero by zero with
    `fputc`. The linker accepted the gaps.
  - The relocation lists were found by scanning every relocation once per
    section, in three places.
  - `finalize_object` looked up each external symbol by comparing it with
    every symbol already in the object.
  - `local_count` and `local_info` compared every `decl_var` with all the ones
    before it, and the frame helpers called `local_count` for each
    instruction.
  - The JSON tree took one `malloc` and one `free` per node, and `jfree`
    walked it at the end.
  - Validation found the callee of each `call` by scanning the functions, and
    each label by scanning the instructions.
- Decision:
  - The relocation cursor advances by the count. One counting sort groups the
    relocations by section.
  - The assembler indexes the object's symbols by name in a hash table sized
    once for the symbols and fixups.
  - `win64.c` keeps one hash table of the locals of the current function.
    `codegen_module` builds it per function and `r6_codegen_win64` releases it.
  - The loader allocates the JSON tree from 1 MiB chunks, frees them in one
    pass, and copies plain runs of a string in one step. Validation looks up
    functions and labels through hash tables built once.
  - No fixed array and no limit is added. A failed allocation is a `CGEN` or
    `E_MEMORY` diagnostic.
- Order of diagnostics: the duplicate-function check still runs first and the
  duplicate-label check still runs in instruction order. A pure speed change
  needs no new fixture.
- Measured on `compiler/host/main.orb` from `0cfce74`, driver built without
  optimization, both runs back to back on a loaded machine:

  | Phase | Before | After |
  |---|---|---|
  | read | 16 ms | 21 ms |
  | JSON load and validation | 4619 ms | 1451 ms |
  | register inference and validation | 1099 ms | 454 ms |
  | assembler text | 8092 ms | 1116 ms |
  | assembling | 15381 ms | 949 ms |
  | COFF write | 11181 ms | 48 ms |
  | `ld` | 266 ms | 259 ms |
  | total | 40.9 s | 4.7 s |

  On an idle machine the same build takes 20.6 s before and 2.4 s after. The
  object file shrinks from 211508235 to 3454987 bytes.
- Evidence that the output is unchanged: the executable of `main.ir` is
  byte-identical (sha256 `2b8cb011…`, 3357756 bytes) and so are the 178
  executables built from the 69 fixtures and the 109 corpus programs that
  build (the sha256 of each compared before and after, with the same 26
  refusals and messages). `objdump -dr -t` of `19-int-list-get.o` differs only in the
  section file offsets. That compiler translates `compiler/host/main.orb` to
  the C of the oracle (6212022 bytes, `54f0b797…`), emits IR identical to the
  IR it was built from, and R6 builds that IR into the same executable.
- Gates: differential **69/69**, loader **261/261**, corpus **109 build /
  109 match over 135**, ratchet held, census **2/27**.
- Status: **applied**.

## D-M1-65 - Only the functions main reaches are emitted

- Date: 2026-10-27. Pin: `0cfce74` (D-F0-17).
- Root cause: `emit_function` ran for every function of the module. The
  compiler's IR carries functions no call reaches, and each one costs code, a
  `.pdata` entry and the runtime members it references.
- Decision:
  - A function is live when the non-extern `main` or a route handler reaches
    it. An edge is a `call` target (Symbol or String) or a Symbol operand that
    names a function. That second form binds a function value in
    `load_const`, `decl_var`, `store_var` and `arg`.
  - Externs add no edges, because they are never emitted.
  - Unreachable functions are still validated. A construct the subset refuses
    is refused wherever it sits. The case `x-dead-function-validated` pins it.
  - `R6Function.live` carries the result from `mark_live_functions` to the
    emission loop and to `module_needs_arena_global`, so the arena global
    exists only when an emitted function needs it.
  - `find_function` reads a hash table of the function names, built once per
    module. The first function of a name wins, as before.
- Fixtures:
  - `71-dead-functions` (oracle and R6 print `live\r\n` and exit 42): `double`
    is reached by a call, `bump` only as a function value, `triple` through a
    call chain. Three functions that call each other or scan a string are
    unreachable. `nm` of the R6 executable lists the first four and none of
    the others.
  - `ok-dead-undefined-call` is the negative control: two unreachable
    functions, one binding the other as a value, call an extern no archive
    defines. Before this decision the build ended in `error[LINK]` with
    `undefined reference to no_such_runtime_symbol`. Now it builds and exits 7.
  - `ok-fn-value` (existing) keeps a function reached only by its Symbol.
- Measured: `compiler/host/main.orb` builds to 3343316 bytes, from 3357756.
  Over the 70 fixtures and 135 corpus programs, 179 executables build and 21
  change: 42720297 bytes become 42577405. `71-dead-functions` goes from
  236641 to 235057 bytes. The compiler built this way translates
  `compiler/host/main.orb` to the C of the oracle (6212022 bytes,
  `54f0b797…`), emits IR identical to `main.ir`, and R6 builds that IR into the
  same executable (sha256 `6275d24e…`).
- Gates: differential **70/70**, loader **264/264** (177 rejections + 87
  controls), corpus **109 build / 109 match over 135**, ratchet held, census
  **2/27**.
- Status: **applied**.

## D-M1-66 - A branch on a comparison reads the flags

- Date: 2026-10-27. Pin: `0cfce74` (D-F0-17).
- Root cause: `eq`, `ne`, `lt`, `le`, `gt` and `ge` store a 0/1 result with
  `setcc`, and the `jump_if_false` right after them loaded that slot to
  compare it with zero: `cmpl $0, -N(%rbp)` and `je`. The slot was just
  written and the flags of the comparison were still live. In the asm of
  `compiler/host/main.orb` 7826 of those pairs follow a store.
- Decision:
  - When the instruction before a `jump_if_false` is a comparison whose
    destination is the tested register, the jump is the inverse condition
    (`jne`, `je`, `jge`, `jg`, `jle`, `jl`) and reads the flags. The
    comparison still stores its result, so a later read of the register is
    unchanged.
  - Float comparisons (parity), string `eq`/`ne` (the result of `strcmp`) and
    every other producer of the tested register keep the load.
  - `asm_win64.c` encodes `jl`, `jge` and `jg` and keeps its four existing
    jumps in one table. The bytes of those four do not change.
- Fixture `72-compare-branch` (oracle and R6 print `599\r\n` and exit 42): the
  six comparisons on unequal and equal operands, a stored comparison read
  after its branch, a branch on a comparison that is not the one before it,
  and float, string and bool comparisons followed by a branch. Its asm keeps
  the load for the stored, float and string cases and drops it for the rest.
- Measured: the asm of `compiler/host/main.orb` goes from 297829 to 294546
  instructions and `cmpl $0` + `je` pairs from 7826 to 4543. The self-compile
  of `compiler/host/main.orb` by the R6-built compiler takes a median of 5.02 s
  against 5.11 s before, nine interleaved runs each, which is inside the run
  to run spread. The oracle takes 2.31 s. The compiler built this way
  translates `compiler/host/main.orb` to the C of the oracle (`54f0b797…`),
  emits IR identical to `main.ir`, and R6 builds that IR into the same
  executable (sha256 `2eb8a03c…`).
- Gates: differential **71/71**, loader **265/265** (177 rejections + 88
  controls), corpus **109 build / 109 match over 135**, ratchet held, census
  **2/27**.
- Status: **applied**.

## D-M1-67 - A load right after the store of its slot reads the register

- Date: 2026-10-27. Pin: `0cfce74` (D-F0-17).
- Root cause: every IR register lives in a stack slot, so the generator stores
  a result and the next instruction loads the same slot. In the asm of
  `compiler/host/main.orb` 9224 `movq %rax, M` lines are followed by
  `movq M, %rax`, 5643 by `movq M, %r11` and 2578 by `movq M, %rcx`. The load
  waits for the store to forward.
- Decision:
  - A new stage, `peephole.c`, rewrites the asm text between the generator and
    the assembler. For two adjacent lines `movq %R, M` and `movq M, %S` it
    drops the second when `S` is `R` and replaces it with `movq %R, %S`
    otherwise. The store stays, so every later read of the slot is unchanged.
  - Only adjacent lines match. A label, a `call` or any other line between
    them ends the match, and a load of another slot ends it too.
  - Only 64-bit general registers match. A 32-bit load clears the upper half
    and an xmm load clears the upper lane, which a dropped load would not.
  - The stage rewrites `.r6-build/r6-<pid>.s` in place and fails closed with
    `error[PEEPHOLE]`. The comment in `win64.c` that ruled out peephole
    rewriting now points here.
- Fixtures: no new one. The rewrite applies to every function of every
  fixture and every corpus program. In the asm of the compiler 17904 adjacent
  store and load pairs name the same slot and 18989 name another slot, which
  must stay. The differential (71 programs), the corpus (109 programs) and
  the self-compile of the compiler are the controls for both.
- Measured: the asm of `compiler/host/main.orb` goes from 294546 to 285322
  instructions. The self-compile of `compiler/host/main.orb` by the R6-built
  compiler takes a median of 4.39 s against 5.02 s, nine interleaved runs each
  (minimum 4.22 s against 4.91 s). The oracle takes 2.31 s. The executable
  shrinks from 3323348 to 3234260 bytes. The compiler translates
  `compiler/host/main.orb` to the C of the oracle (`54f0b797…`), emits IR
  identical to `main.ir`, and R6 builds that IR into the same executable
  (sha256 `6025ca1f…`).
- Gates: differential **71/71**, loader **265/265**, corpus **109 build / 109
  match over 135**, ratchet held, census **2/27**.
- Status: **applied**.

## D-M1-68 - The hottest frame slots live in callee-saved registers

- Date: 2026-10-27. Pin: `0cfce74` (D-F0-17).
- Root cause: every IR register lives in an 8-byte frame slot, and D-M1-67
  removed only the reload on the line after a store. In the asm of
  `compiler/host/main.orb` 159741 lines still name a slot. Loop counters,
  parameters and literal scratch slots are stored and loaded on every
  iteration.
- Decision:
  - A new stage, `regalloc.c`, runs after `peephole.c` on the text of each
    function. It gives slots to rbx, rsi, rdi and r12-r15, which every callee
    preserves, and rewrites the slot operand of each reference.
  - `win64.c` writes `# r6.slots A B C` after `.seh_endprologue`: the end of
    the plain 8-byte slots and the first and last byte of the literal and
    spread scratch slots. `regalloc.c` removes the line. A slot outside those
    ranges, such as a result cell or the list scratch, stays in memory.
  - A slot is a candidate when every reference to it is `movq`, `movl`,
    `cmpl`, `cmpb`, `cmpq`, `addl`, `subl`, `imull`, `addq`, `subq`, `imulq`,
    `movzbl`, `movslq`, `idivl` or `idivq` with the slot as the memory
    operand. A `leaq` of the slot, a `movsd` or any other form keeps it in
    memory.
  - A slot holds its register from its first to its last reference in text
    order. A slot whose first reference is a read is live from the start of
    the function. A slot that is live where a backward jump lands stays live
    to that jump, so a loop keeps the values it reads again. Two slots whose
    intervals do not overlap share a register.
  - A linear scan hands out the seven registers in order of first reference.
    When all are busy, the slot with the smaller weight loses. A reference
    weighs 8 to the power of its loop depth, up to depth 4. A register is kept
    only when the slots on it weigh at least 6, because it costs a push and a
    reload.
  - A function pushes the registers it uses after `movq %rsp, %rbp`, so
    every other slot moves down by 8 bytes per register and the frame grows
    by 8 when the count is odd. Each `leave` is preceded by `movq` reloads
    from the push area. A function that names one of the seven registers,
    returns without `leave`, or jumps to a label it does not define is left
    as it is. The stage fails closed with `error[REGALLOC]`.
  - `asm_win64.c` accepts a register where a memory operand may stand for the
    forms above, `pushq` of the seven registers, `movq $imm, %reg`, and the
    REX prefix of `%sil`, `%dil` and `%r8b` to `%r15b`. It records each push
    and writes a `UWOP_PUSH_NONVOL` code for it between the allocation and the
    frame-pointer codes. `setcc` now rejects a byte register above `%bl`,
    which it encoded as `%ah` to `%bh`.
- Fixture `73-register-slots` (oracle and R6 print nine integer lines and exit
  42): nine accumulators and nine parameters live across a loop, recursion
  with two live results, nested loops, a value written late in the previous
  iteration, a return from inside a loop, a float and a bool beside integers,
  and the address of slots passed by `list_push`.
- Measured: the lines of the asm of `compiler/host/main.orb` that name a slot
  go from 159741 to 58103. Instructions go from 285322 to 310835, because of
  4800 pushes and the reloads before each `leave`. The executable shrinks from
  3234260 to 2926548 bytes. The self-compile of `compiler/host/main.orb` by
  the R6-built compiler takes a median of 2.60 s against 3.87 s, nine
  interleaved runs each (minimum 2.57 s against 3.78 s). The oracle takes
  2.09 s. Machine load differs between this measurement and the one in
  D-M1-67; compare only the pair inside each decision. A minimum weight of 3 gives 2.64 s and 24 gives 2.79 s against
  2.58 s at 6, five and seven runs.
- Encoding check: `objdump -d` of the object R6 writes for the asm of the
  compiler matches the object GAS writes for the same text, 310835
  instructions (the 1001 function headings that `objdump` also prints are not
  counted), and the unwind data (`.xdata`, 1427 lines) is identical.
- The compiler built this way translates `compiler/host/main.orb` to the C of
  the oracle (`54f0b797…`), emits IR identical to `main.ir`, and R6 builds
  that IR into the same executable (sha256 `59fbfe5d…`).
- Gates: differential **72/72**, loader **266/266** (177 rejections + 89
  controls), corpus **109 build / 109 match over 135**, ratchet held, census
  **2/27**.
- Status: **applied**.

## D-M1-69 - The arena loads with one RIP-relative movq

- Date: 2026-10-27. Pin: `0cfce74` (D-F0-17).
- Root cause: a function other than `main` read the arena with
  `leaq r6_arena(%rip), %rax` and `movq 0(%rax), %reg`. The asm of
  `compiler/host/main.orb` holds 4532 such pairs, and each wrote `%rax` for
  nothing.
- Decision:
  - `win64.c` emits `movq r6_arena(%rip), %reg` at the five places that load
    the arena (`emit_load_arena`, `list_create`, the model allocation and the
    two union allocations). The store of the arena in `main` keeps `leaq`.
  - `asm_win64.c` encodes `movq sym(%rip), %r64` as REX.W 8B /r with a REL32
    relocation to the symbol, as `leaq` does. A destination that is not a
    64-bit general register is a diagnostic.
- Fixtures: no new one. The `movq` appears in the asm of six fixtures with
  `%rcx` and five with `%rdx`, and all pass the differential. The other
  registers of `emit_load_arena` use the same line of code.
- Encoding check: `asm_probe` on `movq r6_arena(%rip), %rcx|%rdx|%r8|%rax`
  and GNU `as` on the same text give identical `objdump -dr` output (bytes
  `48 8b 0d`, `48 8b 15`, `4c 8b 05`, `48 8b 05`, `IMAGE_REL_AMD64_REL32`).
- Measured: the asm of the compiler goes from 310835 to 306303 instructions
  and the executable from 2926548 to 2908116 bytes. The self-compile of
  `compiler/host/main.orb` takes a median of 2.60 s against 2.14 s for the
  oracle, nine interleaved runs (minimum 2.54 s against 2.10 s). D-M1-68
  measured 2.60 s as well, so the time does not move.
- The compiler built this way translates `compiler/host/main.orb` to the C of
  the oracle (`54f0b797…`), emits IR identical to `main.ir`, and R6 builds
  that IR into the same executable (sha256 `7dad2f06…`).
- Gates: differential **72/72**, loader **266/266**, corpus **109 build / 109
  match over 135**, ratchet held, census **2/27**.
- Status: **applied**.

## D-M1-70 - A comparison read only by its branch stores no result

- Date: 2026-10-27. Pin: `0cfce74` (D-F0-17).
- Root cause: D-M1-66 let the branch read the flags but kept the comparison's
  `setcc`, `movzbl` and `movl` to its slot. In the asm of
  `compiler/host/main.orb` 9884 instructions, a third of them in the loop
  tests of the compiler, write a result no instruction reads.
- Decision:
  - `flags_only_compare` in `win64.c` is true when the instruction after an
    `eq`, `ne`, `lt`, `le`, `gt` or `ge` is the `jump_if_false` that
    `fused_false_jump` fuses, and no other instruction of the function names
    the comparison's register as operand or destination.
  - The integer and bool comparisons then stop after `cmpq`, and the jump
    reads the flags. Float and string comparisons are not fused and keep the
    result.
  - Any other read of the register keeps the three instructions. The check is
    one scan of the function per comparison that is followed by a fused jump.
- Fixture `74-flags-only-compare` (oracle and R6 print `15139\r\n` and exit
  42): comparisons that only branch, inside and outside a `while`, and three
  that keep the result because another branch, an argument of a call or an
  assignment reads it. The asm of `pick` holds exactly those three `setcc`.
- Measured: the asm of the compiler goes from 306303 to 296419 instructions
  and the executable from 2908116 to 2878420 bytes. The self-compile of
  `compiler/host/main.orb` takes a median of 2.57 s against 2.60 s before and
  2.13 s for the oracle, nine interleaved runs each (minimum 2.53 s against
  2.54 s), which is inside the run to run spread.
- The compiler built this way translates `compiler/host/main.orb` to the C of
  the oracle (`54f0b797…`), emits IR identical to `main.ir`, and R6 builds
  that IR into the same executable (sha256 `08e8213a…`).
- Gates: differential **73/73**, loader **266/266**, corpus **109 build / 109
  match over 135**, ratchet held, census **2/27**.
- Status: **applied**.

## D-M1-71 - A string comparison read only by its branch tests strcmp directly

- Date: 2026-10-27. Pin: `0cfce74` (D-F0-17).
- Root cause: a string `eq` or `ne` called `strcmp`, stored the result,
  compared the slot with zero, stored the 0/1 and the `jump_if_false` loaded
  it again. The asm of `compiler/host/main.orb` holds 2901 `call strcmp`, and
  the lookup loops of the compiler run them.
- Decision:
  - When D-M1-70 finds that no other instruction names the register of a
    string `eq` or `ne`, the comparison ends with `call strcmp` and
    `cmpl $0, %eax`, and `fused_false_jump` returns `jne` for `eq` and `je`
    for `ne`. Otherwise the comparison and the jump are unchanged.
  - `register_named_elsewhere` is the scan of D-M1-70, now shared by both.
- Fixture `75-string-compare-branch` (oracle and R6 print `135\r\n` and exit
  42): `==` and `!=` that only branch, in a function and in a `while` over a
  `list<string>`, and the same two comparisons kept in a `val` read by three
  branches. The asm of `grade` holds two `strcmp` followed by `cmpl $0, %eax`
  and two followed by the stored `sete` and `setne`.
- Measured: the asm of the compiler goes from 296419 to 287746 instructions,
  1723 of the 2901 `strcmp` calls now end in `cmpl $0, %eax`, and the
  executable goes from 2878420 to 2851796 bytes. The self-compile of
  `compiler/host/main.orb` takes a median of 2.52 s against 2.57 s before and
  2.13 s for the oracle, nine interleaved runs each (minimum 2.47 s against
  2.53 s).
- The compiler built this way translates `compiler/host/main.orb` to the C of
  the oracle (`54f0b797…`), emits IR identical to `main.ir`, and R6 builds
  that IR into the same executable (sha256 `90f0341a…`).
- Gates: differential **74/74**, loader **266/266**, corpus **109 build / 109
  match over 135**, ratchet held, census **2/27**.
- Status: **applied**.

## D-M1-72 - A string equality tests the first byte and the pointers before it calls strcmp

- Date: 2026-10-27. Pin: `0cfce74` (D-F0-17).
- Root cause: the self-compile of `compiler/host/main.orb` makes 147423232
  string equality calls. Counted with a probe in place of `strcmp` in the C of
  the compiler, 90.2% of them differ in the first byte (132998610), 0.8% are
  the same pointer (1143316), 9.0% share the first byte and need the full
  comparison (13281305, of which 826027 are equal), and none compare two empty
  strings. A sampler put `strcmp` at about 16% of the run.
- Decision: a string `eq` or `ne` loads the first byte of the left operand,
  compares it with the first byte of the right operand and leaves with ZF
  clear when they differ. Equal first bytes of zero, or the same pointer, leave with
  ZF set. Anything else calls `strcmp`. D-M1-70 and D-M1-71 are unchanged: the
  result is still ZF, read by the fused jump or by `sete` and `setne`.
- NULL: `strcmp` faults on a NULL operand and so does the byte load, so a
  program that compares a NULL string stops in the oracle and in R6. The first
  load is the left operand and the compare reads the right one before any
  branch, so neither operand is skipped.
- The assembler gains `cmpb mem, %al` (opcode `3A /r`).
- Oracle: `gcc -O1` compiles `strcmp(a, b) == 0` to `call strcmp`,
  `testl %eax, %eax`, `sete`, with no inline test of the first byte. The
  comparison costs the oracle one library call each time, so the first-byte
  test makes R6 do less work there than the oracle does.
- Fixture `76-string-equality-fast-path` (oracle and R6 print `97\r\n` and
  exit 42): the same pointer, a first byte that differs, equal contents behind
  two pointers, a shared prefix in both directions, two empty strings and an
  empty string against a longer one, each in the branch-only form and the
  stored form.
- Measured: the asm of the compiler goes from 287746 to 306871 instructions
  and the executable from 2851796 to 3047590 bytes. The self-compile of
  `compiler/host/main.orb` takes a median of 2.77 s against 3.15 s before and
  2.66 s for the oracle, nine interleaved runs each on a busy machine (minimum
  2.65 s against 2.94 s and 2.49 s). `strcmp` falls from about 16% to 3% of the
  samples. The rest of the msvcrt time is `memmove` (10.7%) and `strlen`
  (4.6%), both called from the runtime and from `.len()` in the same way the
  oracle calls them; R6 emits no direct `memcpy`.
- The compiler built this way translates `compiler/host/main.orb` to the C of
  the oracle (`54f0b797…`), emits IR identical to `main.ir`, and R6 builds
  that IR into the same executable.
- Gates: differential **75/75**, loader **266/266**, corpus **109 build / 109
  match over 135**, ratchet held, census **2/27**.
- Status: **applied**.

## D-M1-73 - An int argument of a float parameter widens at the call

- Date: 2026-10-27. Pin: `0cfce74` (D-F0-17).
- Root cause: `suite/call_arg_types` passes `takesFloat(2)`. The call validator
  required the argument type to equal the parameter type and refused with
  `call argument 0 type does not match takesFloat`. The C backend casts each
  argument to the parameter's C type, so the callee receives
  `(orbit_float)(2)`; the test comment says "an int widens to a float in the
  C backend".
- Decision:
  - A call to a declared function or runtime extern accepts an `int` argument
    for a `float` parameter. An int literal becomes the double's bits at
    compile time. An int register is converted with `cvtsi2sdq` into the
    parameter's xmm register, or into `xmm15` and then the stack slot from the
    fifth argument.
  - No other pair widens. A bool, a string or a float argument for another
    parameter type, and a float for an int parameter, keep the refusal.
  - The assembler gains `cvtsi2sdq mem, %xmm` (`F2 REX.W 0F 2A /r`).
- Out of scope: an arithmetic operation on an int and a float register
  (`add supports matching int or float operands`) and the store of a float into
  an int local (`suite/assignment_var_store`, see below).
- Fixture `77-int-arg-float-param` (oracle and R6 print
  `1.5\r\n3.5\r\n8.0\r\n-2.5\r\n22.5\r\n27.0\r\n` and exit 42): a literal, a
  register, an expression, a negative value, and a fifth argument on the
  stack. Negative controls `x-call-bool-arg-to-float-param`,
  `x-call-string-arg-to-float-param`, `x-call-float-arg-to-int-param`.
- `suite/assignment_var_store` is not an R6 gap. The source says
  `var ratio: float = 1`; the IR carries `decl_var ratio Int:1` with no type,
  and the C hoists `orbit_int ratio`, so `ratio = 2.5` stores 2 and the
  float register that reads it back is `(orbit_float)(ratio)`. The oracle
  exits 0 only because `3 > 2.0` still holds. Matching it needs a float to int
  truncation on `store_var` and an int to float conversion on `load_var`
  that the IR types contradict. The defect belongs to the emitter.
- Gates: differential **76/76**, loader **270/270**, corpus **109 build / 109
  match over 135 -> 110 / 110**, census **2/27**, self-host C
  `54f0b797...` from the IR of `compiler/host/main.orb`.
- Status: **applied**.

## D-M1-74 - A union variant carries up to four payloads

- Date: 2026-10-27. Pin: `0cfce74` (D-F0-17).
- Root cause: `suite/union_nested_match` declares `Bin(Op, int, int)`. The
  module check refused any variant with more than two payloads
  (`union Expr variant Bin payload arity is outside the M1 native subset`).
  The pack layout, the constructor, the match reads and the `Op*` word of a
  union payload already handle any count; the limit of two was the measured
  range of D-M1-53, not a layout rule.
- Decision: the limit is four. The C backend packs the payloads in
  declaration order with natural alignment (`Expr_Bin { Op* _0; orbit_int _1;
  orbit_int _2; }`), the layout `union_pack_layout` already computes. Five
  payloads keep the refusal, since no fixture measures them.
- Fixture `78-union-wide-pack` (oracle and R6 print `9 5 20 0 wide 41 narrow
  -23 103 997` one per line and exit 42): `Bin(Op, int, int)`, a four-payload
  `Cell(int, bool, string, int)` and `Span(bool, int, int, bool)` with padding
  between the bool and the int, and a nested match on the `Op` payload.
- The control `x-union-payload-arity` held three ints; it now holds five and
  cites this decision.
- Gates: differential **77/77**, loader **271/271**, corpus **110 build / 110 match over 135 -> 111 / 111**, census **2/27**,
  self-host C `54f0b797...` from the IR of `compiler/host/main.orb`.
- Status: **applied**.

## D-M1-75 - A result passes as an argument to a declared function

- Date: 2026-10-27. Pin: `0cfce74` (D-F0-17).
- Root cause: `suite/result_binding` and `suite/result_string_value` call
  `fn unwrap(r: result)`. The parameter check refused `result`
  (`parameter type result is unsupported in M1`). The C signature is
  `orbit_int unwrap(OrbitArena* arena, OrbitResult r)`: a 24-byte struct,
  which Win64 passes as a pointer to a caller-made copy.
- Decision:
  - A declared Orbit function other than `main` may take `result` parameters.
    The caller passes `leaq` of the result register's cell in the ABI slot of
    that argument, in a register or on the stack. The callee copies the 24
    bytes at the pointer into a cell of its own at entry, before any other
    instruction, so the caller's cell is not aliased once the callee runs.
  - The cells of the parameters follow those of the locals
    (`register_count + locals + param`). `arg` with a destination and
    `load_var` of the parameter copy that cell.
  - A runtime extern still takes scalars only, and a result argument goes to
    a declared Orbit function only. A result literal, an int for a result
    parameter and a result for an int parameter are refused.
- Fixture `79-result-parameter` (oracle and R6 print `41 -1 42 102 9 41 51 9
  82 8 41` one per line and exit 42): a parameter read twice, one after
  another parameter, one on the stack as the fifth argument, a result relayed
  to two functions and the result of a call passed in place.
- Gates: differential **78/78**, loader **275/275**, corpus **111 build / 111 match over 135 -> 113 / 113**, census **2/27**,
  self-host C `54f0b797...` from the IR of `compiler/host/main.orb`.
- Status: **applied**.

## D-M1-76 - A copy moves a result register into a result register

- Date: 2026-10-27. Pin: `0cfce74` (D-F0-17).
- Root cause: `suite/result_stage2` binds `val r = if flag { make_success() }
  else { make_failure() }`. Each branch ends in `copy` from the call's result
  register into the register the binding reads. The opcode accepted scalar
  registers only (`copy currently supports scalar registers only`).
- Decision: `copy` from a result register into a result register copies the
  24-byte cell. Any other pairing keeps the refusal: the existing check of the
  value type against the register type refuses an int into a result and a
  result into an int.
- Fixture `80-result-branch-value` (oracle and R6 print `41 13 41 13 5` one
  per line and exit 42): an `if` value with two call branches, and a nested
  choice whose last branch is `ok(5)`.
- Gates: differential **79/79**, loader **278/278**, corpus **113 build / 113 match over 135 -> 114 / 114**, census **2/27**,
  self-host C `54f0b797...` from the IR of `compiler/host/main.orb`.
- Status: **applied**.

## D-M1-77 - An int pushed into a list<float> converts to a float

- Date: 2026-10-27. Pin: `0cfce74` (D-F0-17).
- Root cause: `suite/list_element_type_annotation` pushes `xs.push(1)` into a
  `list<float>`. The push carries elemKind 3 and an int register, and R6
  refused it (`list_push requires a list register and a value register
  matching elemKind 1..5`). The oracle C is
  `orbit_list_push_float(list, (orbit_float)(r_2))`.
- Decision: `list_push` with elemKind 3 accepts an int register and converts it
  with `cvtsi2sdq` into `xmm2`. A bool or a string for elemKind 3 and a float
  for elemKind 2 keep the refusal. `list_set` is unchanged.
- Fixture `81-float-list-int-push` (oracle and R6 print `4 1.0 7.0 2.5 14.0`
  one per line and exit 42): a literal, a register, an expression and a float
  pushed into one list.
- `suite/list_element_type_annotation` still does not build, and the cause is
  in the emitter. In `rebindingForgetsTheAdoptedType`, after `slot = []`,
  `slot.push("now a string")` is emitted with elemKind 2 and a string value
  register, and the oracle C is `orbit_list_push_int(list,
  (orbit_int)(uintptr_t)(r_10))`: it stores a 32-bit truncation of a string
  pointer. The test passes only because it reads the length. The element kind
  of the push should be 1. R6 refuses the pair, as `x-list-push-*` pins.
- Gates: differential **80/80**, loader **281/281**, corpus **114 build / 114 match over 135 -> 114 / 114**, census **2/27**,
  self-host C `54f0b797...` from the IR of `compiler/host/main.orb`.
- Status: **applied**.

## D-M1-78 - An object is a pointer word for the JSON getters

- Date: 2026-10-27. Pin: `0cfce74` (D-F0-17).
- Root cause: `std/test_json` imports `std/json/json.orb`, which declares
  `extern fn orbit_object_get_int(obj: object, key: string) -> int` and ten
  more getters, and binds `val o: object = try parseJson(...)`. The extern
  check refused the parameter (`extern parameter type is unsupported in M1`).
  The C type of `object` is `OrbitObject*`, a pointer.
- Decision:
  - `object` is spelled `void*` after loading, the way a union is (D-M1-47),
    so a register, a local, a parameter and a return of that type are the
    pointer word every `void*` already is.
  - An extern may take or return a `void*` only if it is one of the 11 getters
    of `std/json/json.orb`: `orbit_object_get_{int,float,bool,string,object}`,
    `orbit_json_array_len` and `orbit_json_array_get_{int,float,bool,string,
    object}`. Any other extern with an object keeps the refusal.
- Fixture `82-json-object` (oracle and R6 print `api 42 1.5 1 7 6 b 9 0` one
  per line and exit 42): `try` binds the parsed object, a function takes and
  returns one, each getter is read, and a missing key reads as the default.
  `std/test_json` itself builds and matches.
- Gates: differential **81/81**, loader **284/284**, corpus **114 build / 114 match over 135 -> 115 / 115**, census **2/27**,
  self-host C `54f0b797...` from the IR of `compiler/host/main.orb`.
- Status: **applied**.

## D-M1-79 - A frame of one page or more is probed before rsp moves

- Date: 2026-10-27. Pin: `0cfce74` (D-F0-17).
- Root cause: `emit_function` reserved every frame with a plain `subq` and the
  image commits 4 KB of stack (`SizeOfStackCommit` 0x1000). Windows grows the
  stack one guard page at a time, so an access more than one page below the
  committed end is an access violation. The compiler's largest frame is
  29 544 B. A call pushes its return address at the new `rsp`, which is the far
  end of the frame.
- Measured before the change: the fixture `83-stack-probe` (`deep`, 4000
  registers = 32 000 B, 8 deep) exits 139 with the driver of `18b25e7`. A frame
  of 1400 registers (11 200 B) did not fault, and 2000 registers (16 000 B)
  did.
- Decision: a frame of 4096 B or more is preceded by `movl $<frame+128>, %eax`
  and `call ___chkstk_ms` (libgcc, already on the link line as `-lgcc`). The
  routine touches each page from the current `rsp` down and preserves every
  register and the flags. The 128 bytes cover the callee-saved pushes that
  `regalloc.c` adds. The call precedes `subq`, so the unwind codes are those of
  D-M1-68 and the allocation still uses `UWOP_ALLOC_LARGE`.
- Fixture `83-stack-probe` (hand-written IR, `deep` with `registerCount` 4000;
  oracle and R6 exit 8): `deep` writes the last register first and recurses 8
  times.
- Gates: differential **82/82**, loader **284/284**, corpus **115 build / 115
  match over 135 -> 115 / 115**, self-host C `54f0b797...` from the IR of
  `compiler/host/main.orb`.
- Status: **applied**.

## D-M1-80 - Float slots live in xmm6-xmm14

- Date: 2026-10-27. Pin: `0cfce74` (D-F0-17).
- Root cause: every float went through its frame slot. The float benchmark of
  `C:\Temp\pa\bench\floats.orb` (2e8 iterations of two dependent
  multiply-add chains, 1e8 calls of a Horner polynomial) ran 5.23 s against
  0.60 s for the oracle, because each operation stored and reloaded its value
  through memory. The compiler has 22 `movsd` in 307 000 instructions, so its
  own code does not change.
- Decision:
  - `regalloc.c` classifies each slot by the lines that name it. `movsd`,
    `addsd`, `subsd`, `mulsd`, `divsd` and `ucomisd` make it a float slot;
    `movq` against a general register is valid for both classes; `movl`,
    `cmpl`, `addl` and the other integer forms make it an integer slot. A
    slot with both kinds of use stays in memory.
  - A float slot takes xmm6-xmm14 by the same interval scan as the integer
    slots, and keeps the same weight threshold. xmm0-xmm3 and xmm15 stay the
    generator's scratch registers; a function that names xmm6-xmm14 is not
    touched. `movsd slot, %xmmN` becomes `movaps %xmmR, %xmmN`.
  - The functions that use xmm registers allocate `16 * n` bytes right after
    `pushq %rbp`, set the frame register to the area's top with
    `leaq A(%rsp), %rbp`, and save the registers with `movaps` at
    `0..16n(%rsp)`. The unwind info carries `UWOP_ALLOC_*` for the area,
    `UWOP_SET_FPREG` with frame offset `A / 16` (at most 240 B, so at most 15
    registers) and one `UWOP_SAVE_XMM128` per register. The slot area and the
    callee-saved pushes lie below it; `leave` is preceded by the `movaps`
    restores.
  - The assembler gains `movaps` (`0F 28`, `0F 29`), `movq %xmm, %gpr`
    (`66 REX.W 0F 7E`), xmm register sources for `movsd`, `addsd`..`divsd` and
    `ucomisd`, REX for xmm8-xmm15, a general register source for `cvtsi2sdq`,
    and the directives `.seh_xmmarea`, `.seh_savexmm` and
    `.seh_setframe %rbp, N`.
- `objdump -p` of the fixture's image decodes the codes as
  `save xmm6 at rsp + 0x0` ... `FPReg: rbp = rsp + 0x70` for a function that
  saves seven registers. No test unwinds through an R6 frame, so the restore of
  xmm registers by the unwinder is read from that decoding and not run.
- Fixture `84-float-registers` (oracle and R6 print `119.125 204.02347564697266
  1336.0 13.375 1.5 0 0 -1.5 1 2.25 inf` one per line and exit 42): six float
  arguments, twelve live floats in one loop, two floats live across a
  recursive call, a NaN compare and a negation.
- The oracle has a defect that the fixture avoids: `val r = deep(n - 1, x)`
  declares `uintptr_t r` in the C for a recursive call that returns a float,
  and the oracle then truncates the value (202.5 for the same program). R6
  follows the IR type. The fixture adds the call result in one expression.
- Benchmark `floats.ir`, best of 5: 5.23 s -> 1.26 s. `fib.ir`: unchanged
  within noise.
- Gates: differential **83/83**, loader **284/284**, corpus **115 build / 115
  match over 135 -> 115 / 115**, self-host C `54f0b797...` from the IR of
  `compiler/host/main.orb`.
- Status: **applied**.

## D-M1-81 - Division and remainder by a constant are a multiply and shifts

- Date: 2026-10-27. Pin: `0cfce74` (D-F0-17).
- Root cause: every `div` and `mod` was `cltd` + `idivl`, about 25 cycles. The
  benchmark `ints.orb` (1e9 iterations with `i % 3` and `s % 1000`) ran 10.41 s
  against 5.16 s for the oracle, whose gcc replaces the division by a multiply.
- Decision:
  - A divisor is constant when it is an inline int, or a register whose nearest
    earlier definition in the straight-line code (no `label` between) is a
    `load_const` or a `neg` of one. A divisor of 0, -1 or INT_MIN keeps
    `idivl`, with its trap on INT_MIN / -1; the oracle's compiler refuses
    `n / 0` at compile time.
  - A divisor of 1 is a copy (`div`) or zero (`mod`). A power
    of two `2^k` is `(n + ((n >> 31) >>> (32 - k))) >> k`, negated for a
    negative divisor. Any other divisor uses the signed 32-bit magic number and
    shift of Hacker's Delight 10-1: `imulq` of the sign-extended `n` by the
    magic, `sarq $32`, a correction by `n` when the magic and the divisor differ
    in sign, `sarl`, and the sign bit added.
  - A remainder is `n - q * d` with `imull`, so it keeps the sign of `n`.
  - The assembler gains `sarl`, `shrl`, `shll` and `sarq` with an 8-bit count
    (`C1 /7`, `/5`, `/4`).
- Measured outside R6 with `C:\Temp\r6-w\divtest.c`, which runs the same
  sequence in C against `/` and `%` of gcc: 329 divisors (31 fixed, 298
  pseudo-random across all bit lengths), the nine edge values, 200 000 random
  dividends, the multiples of each divisor and their neighbours: 0
  disagreements.
- Fixture `85-div-by-constant` (oracle and R6 print `0`, `1300628442`,
  `-1507943223`, `1507943225`, `-25` one per line and exit 42): 40 constant
  divisors, positive and negative, powers of two, 2147483647, 1073741825, each
  compared with a division whose divisor arrives as a parameter, over the edge
  values, -300..300 and 3000 steps of a 32-bit linear congruential generator.
- Benchmark `ints.ir`, best of 5: 10.41 s -> 3.70 s.
- Gates: differential **84/84**, loader **284/284**, corpus **115 build / 115
  match over 135 -> 115 / 115**, self-host C `54f0b797...` from the IR of
  `compiler/host/main.orb`.
- Status: **applied**.

## D-M1-82 - Stores nothing reads and register copies are removed after allocation

- Date: 2026-10-27. Pin: `0cfce74` (D-F0-17).
- Root cause: the generator stores every IR value in a slot and moves every
  operand through `rax`. The assembler text of `compiler/host/main.orb` has
  306 941 instructions (`grep -c '^\t[a-z]'`) against 146 418 for gcc -O1, of
  which 81 000 are `rax` moves.
- Decision:
  - `regalloc.c` counts the reads of each slot. A slot that is written and
    never read is not a register candidate, and its stores are dropped
    (306 941 -> 288 636 instructions). A slot whose address is taken or whose
    reference the assembler cannot encode is not touched, as before.
  - `peephole.c` gains `r6_peephole_final`, run after `r6_regalloc_win64`. For
    each function it computes, per line, the general registers read and fully
    written, follows labels and jumps to the fixed
    point of liveness, and rewrites:
    - `movq A, %X` + `movq %X, B` into `movq A, B` when X is dead (also
      `movl`);
    - `movq A, %X` + `cmpq B, %X` into `cmpq B, A`, and + `testq %X, %X` into
      `testq A, A`;
    - `movq $imm, %R` into the one reader of R in the next four lines
      (`cmpq`, `movq`, `addl`, `subl`, `cmpl`, `imull` against `%eax`) when R
      is dead after it;
    - `movq %S, %D` by renaming the readers of D to S in an eight-line window
      when D is dead after the last one and S is not written meanwhile;
    - a definition of a register nothing reads, from an immediate, a register,
      or a frame, stack or symbol operand (a load that can fault, such as
      `movq 8(%r11), %rax`, stays);
    - `jmp L` followed by `L:`, and the instructions after `ret`, `jmp` and
      `ud2` up to the next label.
    A line the pass does not know reads every register. `ret` reads rax
    and the callee-saved registers; a call reads rcx, rdx, r8 and r9 and
    writes the volatile registers.
  - The assembler gains `cmpq`, `addl` and `subl` with an immediate against a
    register or memory operand (`83 /ext ib`, `81 /ext id`) and `imull $imm,
    %eax` (`6B`, `69`).
- A first version of the copy renaming treated `sete %al` as not writing rax,
  so a copy of rax was read after the flag byte changed. All 83 fixtures
  passed and the R6-built compiler stopped with `list element type mismatch`;
  only the self-host check found it. `writes_register` now marks 8-bit writes,
  `set*` and read-modify-write operands, and fixture `86-register-cleanup`
  reaches the forms around it.
- Fixture `86-register-cleanup` (oracle and R6 print `149 371 1180 1 1010 100
  332 111 118 7` one per line and exit 42): a bool kept across a second
  compare, a loop that returns from inside, `break`, `continue`, a clamp with
  three returns and a Collatz loop.
- Measured on `compiler/host/main.orb`, before D-M1-79 and after this
  decision: instructions 306 871 -> 232 557, `.text` 1 413 488 B -> 1 159 600
  B, image 3 047 590 B -> 2 793 126 B. The R6-built compiler compiling
  `compiler/host/main.orb` to C (`ORBIT_SKIP_CC=1`, interleaved with the
  oracle, 7 and 9 runs): median 3.38 s against 3.24 s (ratio 1.042) before,
  2.33 s against 2.46 s (ratio 0.945) after.
- Benchmarks: see D-M1-83.
- Gates: differential **85/85**, loader **284/284**, corpus **115 build / 115
  match over 135 -> 115 / 115**, self-host C `54f0b797...` from the IR of
  `compiler/host/main.orb`.
- Status: **applied**.

## D-M1-83 - Float constants load from .rdata and xmm moves are fused

- Date: 2026-10-27. Pin: `0cfce74` (D-F0-17).
- Root cause: after D-M1-80 a float constant was still built per use with
  `movabsq $bits, %rax` + `movq %rax, %xmmK`, and each operation went through
  xmm0: `movaps %xmm7, %xmm0`, `mulsd %xmm9, %xmm0`, `movaps %xmm0, %xmm9`.
  `floats.ir` ran 1.26 s against 0.60 s for the oracle.
- Decision: the liveness of `r6_peephole_final` (D-M1-82) covers xmm0-xmm15 and
  the pass adds:
  - `movabsq $0x<bits>, %D` + `movq %D, %xmmK` with D dead becomes
    `movsd .LCr6f<n>(%rip), %xmmK`, with the 8 bytes in a `.rdata` block at
    the end of the function; equal constants in one function share a label;
  - `movsd .LCr6f<n>(%rip), %K` + `addsd|subsd|mulsd|divsd|ucomisd %K, %X`
    with K dead becomes the operation with the memory operand;
  - `movaps A, %X` + `OP B, %X` + `movaps %X, D` with X dead becomes
    `movaps A, D` + `OP B, D`, unless B names D or X;
  - `movaps %S, %D` renames the readers of D to S as it does for general
    registers, and a dead `movaps` or `movsd` between registers goes;
  - `movq %X, %X` and `movaps %X, %X` go.
  A call reads xmm0-xmm3 and writes xmm0-xmm5; `ret` reads xmm0 and
  xmm6-xmm15, so the restores of the callee-saved registers stay.
- The assembler accepts `label(%rip)` as the source of `movsd` and of
  `addsd`..`divsd`, `ucomisd`.
- Fixture `87-float-cleanup` (oracle and R6 print `-86.53966155315554
  -10.924478458669366 153.57059251167735 -3.5207706118582354 2.25 0.0 0.0
  -0.0 inf 0.0 0` one per line and exit 42): `a = c - a` and `b = b / a` where
  the destination is the right operand, a square, a swap through a temporary,
  a constant on the left, negation of zero, 1e308 * 10 and 4.9e-324 / 2.
- Benchmark `floats.ir`, best of 5: 1.26 s -> 0.82 s.
- Totals of D-M1-79..83 on a machine with nothing else running, best of 5 per
  program (`C:\Temp\pa\bench`, same exit code as the oracle's build):

  | program | gcc -O1 | `18b25e7` | D-M1-83 |
  |---|---|---|---|
  | `fib.ir` | 0.333 s | 0.461 s | 0.406 s |
  | `ints.ir` | 4.169 s | 9.559 s | 2.229 s |
  | `floats.ir` | 0.487 s | 4.086 s | 0.742 s |
  | `lists.ir` | 0.565 s | 0.647 s | 0.589 s |
  | `sa.ir` | 0.545 s | 0.523 s | 0.505 s |
  | `sb.ir` | 0.276 s | 0.270 s | 0.280 s |
  | `sc.ir` | 1.404 s | 1.416 s | 1.378 s |

- Totals for `compiler/host/main.orb`: instructions 306 871 -> 230 156,
  `.text` 1 413 488 B -> 1 152 368 B, image 3 047 590 B -> 2 785 958 B. The
  R6-built compiler compiling `compiler/host/main.orb` to C
  (`ORBIT_SKIP_CC=1`, interleaved with the oracle's compiler, median of 9
  runs): 2.15 s against 2.28 s, ratio 0.940; at `18b25e7` the ratio was 1.042
  (3.38 s against 3.24 s).
- Gates: differential **86/86**, loader **284/284**, corpus **115 build / 115
  match over 135 -> 115 / 115**, self-host C `54f0b797...` from the IR of
  `compiler/host/main.orb`.
- Status: **applied**.

## D-M1-84 - A void* register is recovered like an unknown one and a call converts through the word

- Date: 2026-10-27. Pin: `0cfce74` (D-F0-17). The decision lands before the
  pin move it prepares: orbit-lang `e34833a` (branch `ir-complete`,
  `docs/IR_SPEC.md` section 2.3) writes `void*` where the emitter wrote
  `unknown`, because that is what `mapTypeToC` declares for a value the
  compiler cannot type.
- Root cause: the corpus emitted at `e34833a` has 292 `void*` register types.
  R6 recovers an `unknown` slot from its defining instruction (D-M1-20,
  D-M1-47, D-M1-53, D-M1-57) and treated a `void*` slot as typed, so every
  recovery stopped. `r6 build` of that corpus built 108 of 135 programs, and 11
  programs that build at `0cfce74` stopped: `test_bytes`, `test_crypto`,
  `test_io`, `test_json`, `test_string` (a `list_get` of elemKind 0 passed to a
  string parameter) and the six `union_*` programs (a payload read into a
  `void*` destination). The C passes such a value to a parameter or stores it
  with a cast through `uintptr_t`.
- Decision:
  - `infer_register_types` recovers a `void*` destination by the same rules as
    an `unknown` one and keeps the slot when no rule yields another type.
  - A named call converts each argument to the parameter type with the casts the
    C backend writes: an `int`, a `string`, a `list`, a model or a union passes
    to a `void*` parameter unchanged, because an `int` slot already holds the
    sign-extended word; a `void*` word passes to an `int` parameter as its low
    32 bits sign-extended (`movslq`), and to a `string`, `list`, model or union
    parameter unchanged.
  - A call whose callee returns `void*` stores into an `int` destination with
    the same truncation, and a call whose callee returns `int` stores into a
    `void*` destination sign-extended.
  - A `bool`, a `float` and a `result` have no measured conversion to or from a
    `void*` and stay refused.
- Controls in `tests/reject/`: `ok-voidptr-retype-load-var` (exit 3),
  `ok-voidptr-call-roundtrip` (42), `ok-voidptr-call-dest-int` (9);
  `x-voidptr-retype-unresolved`, `x-voidptr-arg-bool-param` and
  `x-bool-arg-voidptr-param` stay `CGEN`.
- Measured: `r6 build` of the corpus emitted at `e34833a` builds 108 -> 125 of
  135 programs. The differential of that corpus matches every program that
  builds. At `0cfce74`: differential **86/86**, loader **290/290**, corpus **115
  build / 115 match over 135 -> 115 / 115**, self-host C `54f0b797...`, census 2 of 27.
- Status: **applied**.

## D-F0-18 - Re-pin to e34833a: complete, typed IR

- Date: 2026-10-27. Moves the pin from `0cfce74` (D-F0-17) to orbit-lang
  `main` `e34833af5a8fae76bd12c1fdb1ac65be6b002659`, 26 commits, pushed to
  `https://github.com/joacokhzyx/orbit-lang`.
- What moved, in four merges:
  - `ir-complete`: six bitwise opcodes (`bit_and`, `bit_or`, `bit_xor`, `shl`,
    `shr`, `bit_not`; `docs/IR_SPEC.md` section 1.3: a 32-bit `orbit_int`
    result, a shift count `& 31`, `shr` arithmetic); an extern entry for every
    runtime symbol a module calls (`system_*`, `orbit_db_*`, `orbit_object_*`,
    `orbit_string_*`; section 3.3); object field reads typed by their getter
    (section 1.4); `unknown` written as `void*`; `string_raw` written as
    `string`; fixes for `var x: float = 1` and for a rebound list.
  - `slim-compiler`: functions no entry point reaches are pruned before the C
    step.
  - `fast-builds` and `ship-archive`: the compiler links the prebuilt runtime
    archive by default and the release ships it.
- `runtime/` did not move; `scripts/mklib_runtime.py` adds a stamp file the
  language reads. The runtime archive is rebuilt from the pin and its symbol
  listing is identical to the previous one. The R6 driver does not change.
- Inputs at the pin (`docs/PIN.md`): fixed point
  `C:\Temp\orbit-fp-c9c04bc393.exe` (sha256 `60862644...`), built from
  `C:\Temp\orbit-pin-e34833a-git`; `PUBLISHED_C 80A61B67.`. The corpus
  regenerates to 136 documents, 6850719 bytes, `filesSha256 1f2c44a4.`. The C
  that the R6-built compiler writes for `compiler/host/main.orb` has the sha256
  `80A61B67A236903FA4894A6159363889C8D332B9BA7FF0022A90E02B5558D713`.
- The pin move commit carries D-M1-84 (the driver at the previous commit).
  Ten programs of the 135 stay refused at this pin, each with its first
  refusal: `bitwise_operators`, `test_bitwise`, `test_imports` (`E_OPCODE`, the
  six bitwise opcodes); `object_literals`, `object_literal_cross_function`,
  `object_literal_missing_key`, `model_literal_fields` (`load_field requires a
  register receiver of a declared model type`); `arena_sink_scope`,
  `peephole_no_fold_side_effects` (`LINK`, `orbit_db_query_all` undefined);
  `test_random` (a `result_unwrap` into a `list`).
- Ratchets: the corpus baseline is taken at the pin (`r6BuildOk 125`, `match
  125`; the previous baseline names another corpus, so `--check` cannot hold it
  across the move) and so is the census baseline.
- Measured: differential **86/86** against the new oracle; loader **290/290**;
  corpus **115 build / 115 match over 135 -> 125 build / 125 match over 135
  programs**, none differing; census **2 build over 27 -> 2 build over 28
  modules**; self-host C `80A61B67...` from the IR of `compiler/host/main.orb`.
- Status: **applied**.

## D-M1-85 - The six bitwise opcodes

- Date: 2026-10-27. Pin: `e34833a` (D-F0-18).
- Root cause: the loader stopped at `E_OPCODE` on `bit_and`, `bit_or`,
  `bit_xor`, `shl`, `shr` and `bit_not` (indexes 60-65, `docs/IR_SPEC.md`
  section 1.3). Three corpus programs carry them: `bitwise_operators`,
  `test_bitwise` and `test_imports`.
- Decision:
  - The loader admits the six names. Each needs a destination.
  - Operands and destination are `int`; a float, bool, string or `void*`
    operand and a non-int destination stay `CGEN`. An inline `Int` literal is
    admitted as an operand, like the arithmetic opcodes (D-M1-21).
  - The C casts each operand to `uintptr_t` and the result to `orbit_int`, so
    the low 32 bits of the operation, sign-extended, are the slot. `bit_and`,
    `bit_or` and `bit_xor` are `andl`/`orl`/`xorl` on `%eax` with a slot
    operand; `bit_not` is `notl`; `shl` and `shr` load the count into `%ecx`
    and run `shll %cl` or `sarl %cl`, whose hardware mask is the `& 31` of the
    spec. `shr` is arithmetic because the C shifts the sign-extended 64-bit
    word and keeps the low 32 bits.
  - The assembler encodes `andl`, `orl`, `xorl` (slot or register to `%eax`),
    `notl %reg` and `shll`/`sarl %cl, %reg`; `regalloc.c` treats `andl`,
    `orl` and `xorl` like `addl`.
- Fixture `88-bitwise-ops` (oracle and R6 print the same 30 lines and exit 42):
  and/or/xor/not on 0, -1, `INT_MIN` and `INT_MAX`; counts 0, 31, 32, 33, -1
  and -31; `-8 >> 1`, `-1 >> 31`; literal and register operands; C precedence.
  Controls in `tests/reject/`: `ok-bitwise-literal-operands` (exit 14),
  `x-bitwise-float-operand`, `x-shift-bool-destination` and
  `x-bit-not-string-operand` stay `CGEN`.
- Corpus: `test_bitwise` builds and matches. `test_imports` stops at a
  `result_unwrap` into a `list` (next refusal) and `bitwise_operators` stops at
  a `list_get` of a `list<list<int>>`. Each is a separate decision.
- Gates: differential **87/87**, loader **294/294**, corpus **125 build / 125
  match over 135 -> 126 / 126**, census 2 build over 28 modules, self-host C
  `80A61B67...` from the IR of `compiler/host/main.orb`.
- Status: **applied**.

## D-M1-86 - Object literals and keyed object reads

- Date: 2026-10-27. Pin: `e34833a` (D-F0-18).
- Root cause: four corpus programs (`object_literals`,
  `object_literal_cross_function`, `object_literal_missing_key`,
  `model_literal_fields`) stopped at `load_field requires a register receiver
  of a declared model type`. The IR builds an object with a `call` of the extern
  `orbit_object_create`, fills it with `orbit_object_set_<kind>` calls and reads
  it with `load_field` (`docs/IR_SPEC.md` sections 1.4 and 3.3).
- Decision:
  - The loader's canonical spelling already turns the IR type `object` into
    `void*`, the `OrbitObject*` word. The creator and the six setters are the
    only externs outside the scalar set that may declare a `void*` (or, for
    `orbit_object_set_list`, a list) parameter and a `void*` return.
  - A `load_field` whose receiver is a `void*` register and whose reference is
    not a `<Union>_<Variant>._i` pack reference is a keyed read. The key is the
    text of the `Sym` or `String` operand; an empty key or one with a NUL is
    `CGEN`. The destination type names the getter: `int` is
    `orbit_object_get_int` (result sign-extended from `eax`), `float`
    `orbit_object_get_float` (`xmm0`), `bool` `orbit_object_get_bool` (`al`
    zero-extended), `string` `orbit_object_get_string`, `void*`
    `orbit_object_get_object` and `list` `orbit_object_get_list` (`rax`). Any
    other destination type is `CGEN`; the untyped `orbit_object_get_raw` form
    has no corpus program and stays refused.
  - A key never stored reads as the zero of the getter, which is the runtime's
    behaviour and not R6's.
- Fixture `89-object-literals` (oracle and R6 print the same 17 lines and exit
  42): four value kinds, bool keys named `ok` and `err`, a nested object, a
  list value read back with a declared type, three missing-key reads, an object
  passed to and returned from a function, and a model whose field names equal
  object keys. Controls in `tests/reject/`: `ok-object-set-get` (exit 5);
  `x-object-read-result-destination`, `x-object-read-empty-key` and
  `x-object-extern-other-symbol` stay `CGEN`.
- Oracle note: reading a missing `float` key into a typed local
  (`val x: float = o.get("k")`) segfaults in the oracle's binary, so the
  fixture does not read one. It is a language-side defect, not an R6 one.
- Gates: differential **88/88**, loader **298/298**, corpus **126 build / 126
  match over 135 -> 130 / 130**, census 2 build over 28 modules, self-host C
  `80A61B67...`.
- Status: **applied**.

## D-M1-87 - Database programs link the DB variant of the runtime

- Date: 2026-10-27. Pin: `e34833a` (D-F0-18).
- Root cause: `arena_sink_scope` and `peephole_no_fold_side_effects` call
  `Item.all()` and `Item.where(...)`, which are `orbit_db_query_all` and
  `orbit_db_query_where_p` externs (IR_SPEC 3.3). `ld` reported
  `undefined reference to orbit_db_query_all` because the net archive defines no
  database symbols. The language links such a program against the whole runtime
  compiled with `-DORBIT_WITH_DB`, `-I<runtime>/vendor` and
  `<runtime>/vendor/win-x64/sqlite3.lib` (`compiler/host/pipeline.orb`).
- Decision:
  - `scripts/build-runtime-db.py --orbit-root <pin> --cc <gcc>` compiles the
    translation unit of the pinned `scripts/mklib_runtime.py` with one more
    define, `ORBIT_WITH_DB`, into `.r6-build/runtime-win64/libruntime_net_db.a`
    and copies `sqlite3.lib` beside it. Neither file is committed. CI and
    `scripts/ci.ps1` build or require them.
  - The driver links the DB archive and `sqlite3.lib` instead of the plain
    archive when the module declares an extern named `orbit_db_*`.
  - The entry wrapper does what the pin's `main` does for such a module: it
    calls `orbit_db_init(".orbit/data/app.db")` and one `orbit_db_exec_ddl("CREATE
    TABLE IF NOT EXISTS <lowercased name>s (...)")` per model with fields (int
    and bool `INTEGER`, float `REAL`, anything else `TEXT`, the field `id` the
    primary key; `generateModelDDL` and `modelTableName`). More than 8 models
    and any `migrations` entry are `CGEN`: the migration runner has no lowering.
  - The executable finds `sqlite3.dll` as the oracle's does, beside the exe or on
    `PATH`. `tests/diff/run_diff.py` puts `<orbit-root>/runtime/vendor/win-x64`
    first on `PATH` and runs both executables with that environment, as
    `run_corpus.py` already does.
- Fixture `90-db-link` (oracle and R6 print `2 2 0 2` and exit 42): all, where
  and create on a model with four field kinds, an empty model, run where
  `.orbit/data` does not exist, so the open fails on both sides with the same
  message on stderr. Both corpus programs were also run by hand with
  `.orbit/data` present: stdout and exit equal, and both sides create
  `app.db`. Controls: `ok-db-link` (build only) and `x-db-migrations`
  (`CGEN`).
- Gates: differential **89/89**, loader **300/300**, corpus **130 build / 130
  match over 135 -> 132 / 132**, census 2 build over 28 modules, self-host C
  `80A61B67...`.
- Status: **applied**.

## D-M1-88 - A result unwrapped into a list

- Date: 2026-10-27. Pin: `e34833a` (D-F0-18).
- Root cause: `test_random` and `test_imports` unwrap `randomBytes(16)`, whose
  value is the `OrbitList*` of `orbit_random_bytes`, into a `list` register.
  `result_unwrap` admitted an int, float or string destination only, and the
  IR types this destination `list` (it was `unknown` before D-F0-18).
- Decision: a `list` destination joins the word copy of the int and string
  unwraps (`movq` of the value cell). The C writes
  `r_n = (void*)(uintptr_t)(r_m.value)` into an `OrbitList*` register, which is
  the same word. A bool, a model and a result destination stay `CGEN`.
- Fixture `91-result-list` (oracle and R6 print `16 16 0 1` and exit 42): a
  16-entry and an empty list from `orbit_random_bytes`, a walk over the first
  and a refused negative count.
  Control: `x-result-unwrap-bool-destination` stays `CGEN`.
- Corpus: `test_imports` builds and matches. `test_random` moves to its next
  refusal, a `load_var` of a local bound to an untyped `list_get` read
  (`val b = bs.get(i)`), which is a separate decision.
- Gates: differential **90/90**, loader **301/301**, corpus **132 build / 132
  match over 135 -> 133 / 133**, census 2 build over 28 modules, self-host C
  `80A61B67...`.
- Status: **applied**.

## D-M1-89 - An untyped list element bound to a local

- Date: 2026-10-27. Pin: `e34833a` (D-F0-18).
- Root cause: in `test_random`, `val b = bs.get(i)` reads an element of a list
  the builder cannot type (`list_get` elemKind 0, destination `void*`) and binds
  it with `decl_var`; the loads of `b` are `int`. The recovery of D-M1-51 and
  D-M1-60 knew a comparison or an argument as the only use, not a bound local,
  so the first refusal was `load_var requires a supported local`.
- Decision: when the only use of the `void*` element is a `decl_var`, the
  element takes the type of the loads of that local (`bound_local_type`, the
  rule D-M1-47 uses for unwraps), if that type is `int` or `string`. The C
  declares the local as a word and reads it as `(orbit_int)(uintptr_t)`, so an
  element that fits 32 bits reads the same. An element wider than 32 bits is out
  of evidence. Loads that disagree leave the element `void*` and the local is
  refused.
- Fixture `92-opaque-local` (oracle and R6 print `64 1` and exit 42): 64 random
  bytes read with `val b = bs.get(i)`, a range check and a threshold count.
  Control: `x-opaque-local-disagreeing-loads` stays `CGEN`.
- Corpus: `test_random` builds and matches.
- Gates: differential **91/91**, loader **302/302**, corpus **133 build / 133
  match over 135 -> 134 / 134**, census 2 build over 28 modules, self-host C
  `80A61B67...`.
- Status: **applied**.

## D-M1-90 - Elements of nested lists

- Date: 2026-10-27. Pin: `e34833a` (D-F0-18).
- Root cause: `bitwise_operators` reads `xs[0][0]` of a `list<list<int>>` and
  `ys[0][0][0]` of a `list<list<list<int>>>`. The builder types the first read
  `list` and every read below it `void*` (a `list_get` of elemKind 0), and the
  word is then the receiver of the next read or an operand of an `add` whose
  destination is `int`. R6 recovered a `void*` element from a comparison, an
  argument or a bound local only, so the first refusal was `list_get requires
  a list register`.
- Decision: the only use of an untyped element decides its type.
  - A `list_get` or `list_len` that takes it as receiver makes it a `list`; the
    C casts the word to `const OrbitList*`.
  - An `add`, `sub` or `mul` with an `int` destination makes it an `int`; the C
    operates on `uintptr_t` words and narrows to `orbit_int`, and the 32-bit
    instruction on the low halves gives the same slot.
  - A word that feeds another word-typed `add` (a chain of three terms) or a
    `print` stays `void*` and refused: the C prints the word as `%lld`, which
    has no measured lowering here.
- Fixture `93-nested-lists` (oracle and R6 print `3` and exit 42): reads at
  depth two and three, sums, a difference and a product, and a length of an
  inner list. Control: `x-opaque-word-add-string-destination` stays `CGEN`.
- Corpus: `bitwise_operators` builds and matches; the corpus is complete.
- Gates: differential **92/92**, loader **303/303**, corpus **134 build / 134
  match over 135 -> 135 / 135**, census 2 build over 28 modules, self-host C
  `80A61B67...`.
- Status: **applied**.

## D-M1-91 - Arithmetic over words and a printed word

- Date: 2026-10-28. Pin: `e34833a` (D-F0-18).
- Root cause: D-M1-90 left two uses of an untyped element refused: a word that
  feeds a second word-typed `add` (a sum of three terms whose intermediate
  register is `void*`) and a word passed to `print`. No corpus program reaches
  either. The pin's C is clear for both: `add`, `sub` and `mul` compute
  `(uintptr_t)a OP (uintptr_t)b` and store it with the cast of the destination
  register (`c_backend.orb`, `IROpcode.add`/`sub`/`mul`; `generateRegAssign`),
  and `print` of a register that is neither a string nor a float writes
  `printf("%lld\n", (long long)(r))`.
- Decision:
  - `add`, `sub` and `mul` accept operands that are `int` or `void*`, at least
    one of them `void*`, with an `int` or `void*` destination. A `void*`
    destination keeps the 64-bit result (`addq`, `subq`, `imulq` on the slots).
    An `int` destination keeps the low 32 bits, which is the 32-bit instruction
    of the typed rule. `div` and `mod` over words stay refused: the pin divides
    `(orbit_int)` casts, not words.
  - `print` accepts a `void*` register and writes its 64-bit word with `%lld`.
  - The untyped-element recovery of D-M1-89 and D-M1-90 reads an `int` as the
    low 32 bits, sign-extended: `emit_list_read` decodes an `int` destination
    with `movslq`, which is the C cast `(orbit_int)(uintptr_t)`. No change.
- Fixture `94-word-chain` (hand-written IR; oracle and R6 print `8` and exit
  42): a sum of three elements whose intermediate register is `void*`, and a
  printed element. Controls in `tests/reject/`: `ok-word-arith-print` (exit 7,
  prints `2147483648`: a 64-bit sum, a 64-bit difference and product, an `int`
  destination that wraps to `-2147483648`); `x-word-arith-bool-operand` and
  `x-word-div-word-destination` stay `CGEN`.
- Gates: differential **93/93**, loader **306/306** (`make_cases.py --check`
  clean). The `94-word-chain` IR is hand-written, not emitter output.
- Status: **applied**.

## D-M1-92 - Migrations of a database module

- Date: 2026-10-28. Pin: `e34833a` (D-F0-18).
- Root cause: D-M1-87 refused every module with `migrations`. The pin's
  `generateModelDDL` (`c_backend.orb`) appends, after the model tables, a block
  `const char* _orbit_migs[] = {...}; if (!orbit_run_migrations(_orbit_migs, N))
  { fprintf(stderr, ...); exit(1); }`. `orbit_run_migrations` (`runtime/database.c`)
  creates `_orbit_migrations` and runs each statement once, by position, in a
  transaction.
- Decision: in `main`, after the model `orbit_db_exec_ddl` calls, R6 stores the
  address of each migration text in a `.data` array, calls
  `orbit_run_migrations(array, N)` and calls `exit(1)` when the answer is
  false. The message on stderr is not part of the verdict. A module with
  migrations and no `orbit_db_*` extern stays `CGEN`: the driver links the DB
  archive by those externs (D-M1-87) and the link of a migration-only module
  has no measurement.
- Fixture `95-db-migrations` (oracle and R6 exit 1, empty stdout): the harness
  has no `.orbit/data`, so the open fails, the runner answers false and `main`
  exits before `orbit_main`. With the directory present and `sqlite3.dll` on
  `PATH`, the oracle and R6 both print `466` and exit 42 on a first run and on
  a rerun (compared by hand, 2026-10-28).
  Controls in `tests/reject/`: `ok-db-migrations` (builds, two migrations and an
  `orbit_db_*` extern); `x-db-migrations-no-db-extern` stays `CGEN`. The case
  `x-db-migrations` of D-M1-87 is replaced by these two.
- Gates: differential **94/94**, loader **307/307**, corpus **135 build / 135
  match over 135 -> 135 / 135** (ratchet holds), self-host C `80A61B67...`
  (`selfhost2.ps1` prints SELFHOST OK).
- Status: **applied**.

## D-M1-93 - An int register copied into a float register

- Date: 2026-10-28. Pin: `e34833a` (D-F0-18); the IR that needs it is the one
  of orbit-lang `6a9ddaf`.
- Root cause: orbit-lang `eb54e08` converts an int stored into a float local or
  float field by copying the int register into a float register (the mechanism
  of `39a6d18` for a float declaration), and `992b12b` documents in IR_SPEC that
  this `copy` converts the value, as the C assignment does. The validation of
  `copy` required the operand type to equal the destination type, so
  `tests/suite/float_local_int_assign` from the `6a9ddaf` corpus failed with
  `CGEN` ("copy value type int does not match register type float").
- Decision: a `copy` whose operand is an `int` register and whose destination
  is a `float` register is lowered as `cvtsi2sdq` from the int slot into
  `%xmm0` and a `movsd` into the float slot. Every other pair of differing
  kinds stays `CGEN`.
- Fixture `96-int-copy-to-float` (prints `3.5`, exit 42; its `.json` is the IR
  the `6a9ddaf` fixed point emits). Controls in `tests/reject/`:
  `ok-copy-int-into-float` (prints `-7.0`, exit 0); `x-copy-bool-into-float`
  stays `CGEN`.
- Gates at `e34833a`: differential **95/95**, loader **309/309**, corpus
  **135 build / 135 match over 135 -> 135 / 135**, self-host C `80A61B67...`
  (`scripts/selfhost.ps1` prints SELFHOST OK). At the `6a9ddaf` corpus the
  driver builds 137 of 137 programs.
- Status: **applied**.

## D-F0-19 - Re-pin to 6a9ddaf: the open language bugs closed

- Date: 2026-10-28. Moves the pin from `e34833a` (D-F0-18) to orbit-lang
  `main` `6a9ddaf7066d396fe9b25a902e67ca2ee4701872`, 11 commits in the merge
  `pending-fixes`.
- What moved:
  - `eb54e08` and `992b12b`: an int stored into a float local or field is
    copied into a float register and the `copy` converts the value;
    `docs/IR_SPEC.md` says so in the `copy` row (blob `49aaa478`).
  - `df18940`: an object read in arithmetic is typed by its getter, and a read
    with no known getter is a compile error, so no program reads through
    `orbit_object_get_raw`.
  - `23908f4`, `16a7b93`, `26ef097`, `f4b706d`: doctor fixes and the suite
    programs that expected them.
  - `d783438` and `9bb9dda`: the suite gains `float_local_int_assign` and
    `object_float_missing_key` (114 programs).
- `runtime/` and `scripts/mklib_runtime.py` did not move
  (`git diff e34833a 6a9ddaf -- runtime scripts/mklib_runtime.py` is empty).
  The runtime archive is rebuilt from the pin: 141360 bytes, 337 symbols as
  before. The R6 driver does not change; the lowering that the pin needs is
  D-M1-93, committed before this move.
- Inputs at the pin (`docs/PIN.md`): fixed point
  `C:\Temp\orbit-fp-c41ad248ab.exe` (sha256 `426dc84e...`, 2711040 bytes),
  built from `C:\Temp\orbit-pin-6a9ddaf-git`; `PUBLISHED_C C1176CF4.`. The
  corpus regenerates to 138 documents, 6872621 bytes, `filesSha256 741900ad.`:
  two documents are new and `user_fn_named_like_compiler_helper` changed
  (11675 to 12545 bytes). The C that the R6-built compiler writes for
  `compiler/host/main.orb` has the sha256
  `C1176CF45FD3B1D72A24956AD873D882CFB97EF6A9E4B0196C7074105E63E7CD`.
- Ratchets: the corpus baseline is taken at the pin (`r6BuildOk 137`, `match
  137`; the previous baseline names another corpus, so `--check` cannot hold it
  across the move), and so are the census baseline and `axes-baseline.json`
  (137 programs, best of 5).
- Measured: differential **95/95** against the new oracle; loader **309/309**
  (205 rejections, 104 acceptance controls); corpus **135 build / 135 match
  over 135 -> 137 build / 137 match over 137 programs**, none differing;
  census **2 build over 28 modules**, unchanged; self-host C `C1176CF4...`
  from the IR of `compiler/host/main.orb`.
- Status: **applied**.

## D-M1-94 - An int is 64 bits

- Date: 2026-10-28. Pin: `e34833a` (D-F0-18). Status: **applied** against the
  language branch `int64` (orbit-lang `d536223`, not pinned yet). Measured with
  that tree's fixed point: `run_diff.py` 99/99, `run_reject.py` 332/332, corpus
  140 match + 3 whose oracle fails to build (D-M1-97), self-host C
  `DD649139...` equal to `PUBLISHED_C`.
- Reason: the language makes `orbit_int` an `int64_t` with two's complement
  arithmetic and defined wrap (Phase 1b). Arithmetic, comparison, negation and
  the six bitwise opcodes are 64-bit; a shift count is taken `& 63` and `shr`
  is arithmetic; list elements, result payloads and model fields of type `int`
  are eight bytes; `print` of an int writes the 64-bit value.
- Decision:
  - The loader keeps an `Int` value in `as.i64` and accepts INT64_MIN to
    INT64_MAX. A value past that range is `E_RANGE`, as past INT32 was.
  - `add`, `sub`, `mul` and `neg` are `addq`, `subq`, `imulq` and `negq` on the
    slots. The `movslq` that D-M1-36 and D-M1-52 added after each is gone, and
    the typed rule and the word rule of D-M1-91 are one rule.
  - An int immediate that fits a sign-extended 32 bits is `movq $imm`; any other
    goes through `movabsq` into `%rax` (`emit_int_rax`, `emit_int_slot`). The
    same holds for an inline argument, a returned literal, a result payload, a
    field initializer and a global (`global_integer` parses 64 bits).
  - `bit_and`, `bit_or` and `bit_xor` are `andq`, `orq`, `xorq`; `shl` and `shr`
    are `shlq %cl` and `sarq %cl`; `bit_not` is `notq`. D-M1-85 keeps its
    typing rules and loses its 32-bit narrowing.
  - A call whose destination or callee is `int` stores all of `%rax`; an `int`
    argument of a `void*` parameter and a `void*` argument of an `int` parameter
    are the same word, so the truncation of D-M1-84 and the low-32-bit decode of
    D-M1-89 and D-M1-91 go. An opaque list element decodes with `movq`.
  - `model_field_size` gives an `int` eight bytes with alignment eight, so the
    layout of a model and of a union pack follows. An enumeration field is
    rewritten to `int` before the layout (D-M1-35) and takes eight bytes too.
  - `cvtsi2sdq` was already 64-bit. The print format `%lld` and its
    full-register load do not change.
- Superseded: D-M1-36 (32-bit slot), D-M1-52 (`neg` at 32 bits), the width
  statement of D-M1-85, the truncations of D-M1-84, D-M1-89 and D-M1-91, and the
  4-byte enumeration field of D-M1-35.
- Not changed: the union tag stays a 4-byte write and read at offset 0 of the
  16-byte cell; `list_len` keeps its 64-bit store; a bool keeps its 32-bit write.
- Closed at `bf38b38` (D-F0-20): the runtime returns `orbit_int`;
  `68-string-index` passes.
- The `int` fixtures `06`, `45`, `47`, `59`, `60`, `85` and `88` and the
  controls of `tests/reject/` that encode INT32 limits change with this
  decision.
- Verified at the pin `bf38b38` by D-F0-20: run_diff.py 100/100, run_reject.py
  341/341.

## D-M1-95 - Division by zero and INT64_MIN / -1 stop the program

- Date: 2026-10-28. Pin: `e34833a` (D-F0-18). Status: **applied** against the
  language branch `int64` (`d536223`); the file and line arrive in the IR
  (D-M1-97). Counts as in D-M1-94.
- Reason: the language makes an integer `/` or `%` by zero, and `INT64_MIN` by
  `-1`, a runtime stop: `orbit: division by zero at <file>:<line>` or `orbit:
  integer overflow in division at <file>:<line>` on stderr, exit status 2.
  Float division stays IEEE. Before this decision `idivl` raised a hardware
  exception (STATUS_INTEGER_DIVIDE_BY_ZERO) in R6 programs.
- Decision:
  - An integer `div` or `mod` whose divisor is not a constant other than 0 or -1
    is `cqto` and `idivq` preceded by two checks. `testq` of the divisor and
    `je` to the zero block; `cmpq $-1` of the divisor and, when equal, a
    `cmpq` of the dividend against `INT64_MIN` and `je` to the overflow block.
  - The two blocks follow the function body, after the last `ret`. Each one is:

        leaq .LCr6_<id>(%rip), %rcx    # "<orbit-ir>"
        movq $<line>, %rdx
        movq $<kind>, %r8              # 0 division by zero, 1 overflow
        call orbit_div_error
        ud2

    The runtime function `void orbit_div_error(const char *file, int64_t line,
    int64_t kind)` does not return. It belongs to the language branch; a module
    that divides and links an archive without it fails with `error[LINK]`.
  - The line is `div_trap_line(i)`, one function in `win64.c`. D-M1-97 feeds it
    from the IR.
  - A constant divisor of 0 or -1 keeps this path, so the trap fires at run
    time; the multiply path of D-M1-96 never sees them.
- Fixtures: `97-div-zero` and `98-div-overflow` (exit 2, empty stdout, message
  on stderr, which the differential does not compare).
- Closed by D-M1-97: the line travels in the IR, the runtime exports
  `orbit_div_error`, and `mod` shares the messages of `div`.
- Verified at the pin `bf38b38` by D-F0-20: run_diff.py 100/100, run_reject.py
  341/341.

## D-M1-96 - Division and remainder by a constant at 64 bits

- Date: 2026-10-28. Pin: `e34833a` (D-F0-18). Status: **applied** against the
  language branch `int64` (`d536223`). Counts as in D-M1-94.
- Reason: D-M1-81 replaced `idivl` by a 32-bit multiply and shifts. With a
  64-bit int the divisor, the magic number and the product are 64-bit.
- Decision:
  - A divisor is constant under the rule of D-M1-81. A divisor of 0, -1 or
    INT64_MIN keeps the checked `idivq` of D-M1-95. A divisor of 1 is a copy
    (`div`) or zero (`mod`).
  - A power of two `2^k` is `(n + ((n >> 63) >>> (64 - k))) >> k`, negated for a
    negative divisor, with `sarq`, `shrq` and `addq`. The remainder is
    `n - (q << k)`.
  - Any other divisor uses the signed 64-bit magic number of Hacker's Delight
    10-1 with a word size of 64 (`signed_magic`, shift `p - 64`). `movabsq` loads
    the magic into `%rax`, the one-operand `imulq %rcx` leaves the high half of
    `n * magic` in `%rdx`, `addq` or `subq` of `n` corrects a magic of the other
    sign, `sarq` shifts by `p - 64`, and `shrq $63` adds the sign bit. The
    remainder is `n - q * d` with the two-operand `imulq`.
  - The peephole models the one-operand `imulq` (read of `%rax` and the operand,
    write of `%rax` and `%rdx`), as it models `idivq`.
- Measured outside R6 with `tests/div/magic64.c`, which runs the emitted
  sequence in C with `__int128` for the high half against the `/` and `%` of
  gcc: 4000 divisors (edge values, their multiples, random of every bit
  length), 3000 dividends each (the edge values, the divisor and its
  neighbours, multiples of the divisor, random of every bit length): 11 751 000
  pairs, 0 disagreements.
- Fixture: `99-div-by-constant-64` (constant divisors against a divisor that
  arrives as a parameter, over 64-bit edge values).
- Verified at the pin `bf38b38` by D-F0-20: run_diff.py 100/100, run_reject.py
  341/341.

## D-M1-97 - Source name and line in the IR, and the stack reserve

- Date: 2026-10-28. Pin: `e34833a` (D-F0-18). Status: **applied** against the
  language branch `int64` (`d536223`, `docs/IR_SPEC.md` section 1.5 and V-20).
- Reason: the language puts the base name of the root file in the module as
  `sourceName` and the Orbit line in each `div` and `mod` as `line`, so the
  trap of D-M1-95 can name its origin. Its runtime links every Windows
  executable with a 16 MiB stack, because 64-bit frames overflowed the 2 MiB
  default.
- Decision:
  - The module accepts an optional string `sourceName`; an absent one is `""`.
    An instruction accepts an optional `line`, a positive integer, on `div` and
    `mod` only. Any other opcode, zero, a negative number, a fraction or a
    string is `E_SCHEMA` or `E_RANGE`. Optional, not required, because the 99
    fixtures and the 275 loader cases predate the field; the runtime prints the
    bare message when both are absent.
  - The trap block of D-M1-95 loads `sourceName` and `line` instead of
    `<orbit-ir>` and 0.
  - The driver passes `--stack 16777216` to `ld`.
- Language-side finding, not worked around: the suite programs
  `division_checked_values`, `remainder_by_zero` and
  `remainder_int_min_overflow` define a user function `remainder`, which
  collides with the libm declaration in `math.h`, so their C build fails
  (`previous declaration of 'remainder'`). R6 builds them and prints
  `orbit: division by zero at remainder_by_zero.orb:11` and
  `orbit: integer overflow in division at remainder_int_min_overflow.orb:10`,
  exit 2. The fix is a user-symbol prefix in the C backend (Phase 4).
  Closed at `bf38b38` by `c-mangle` and D-M1-99; all 143 programs match.
- Controls: `v20-line-on-add`, `v20-line-zero`, `v20-line-negative`,
  `v20-line-string`, `v20-line-fraction`, `v20-sourcename-number` (rejected);
  `ok-div-trap-line`, `ok-mod-trap-line`, `ok-div-trap-no-source` (stderr
  compared through the new `run_stderr` key of `run_reject.py`).
- Measured: `run_diff.py` 99/99, `run_reject.py` 332/332, `make_cases.py
  --check` clean, self-host `DD649139...`. The corpus emitted by the branch has
  143 programs: 140 match, 3 fail in the oracle for the collision above.
- Verified at the pin `bf38b38` by D-F0-20: run_diff.py 100/100, run_reject.py
  341/341.

## D-M1-98 - Runtime directory and temporary directory of the driver

- Date: 2026-10-28.
- Reason: the language installs R6 as `bin\r6.exe` with the runtime archives in
  `lib\` and runs it from the user's project. The driver resolved the archives
  and its `.s`/`.o` temporaries under `.r6-build` in the current directory,
  which wrote into the user's project and found no archives there.
- Decision:
  - Runtime directory, in order: `--runtime-dir <dir>`, `R6_RUNTIME_DIR`,
    `.r6-build/runtime-win64` relative to the current directory. The directory
    holds `libruntime_net.a`, `libruntime_net_db.a` and `sqlite3.lib`.
  - A missing archive is `error[LINK]: runtime archive not found: <full path>`,
    checked before `ld` runs. `sqlite3.lib` is checked only for a module with an
    `orbit_db_*` extern.
  - Temporaries: `R6_TMPDIR` (created if absent), else `GetTempPathA`. Names
    stay `r6-<pid>.s` and `r6-<pid>.o`; a good link deletes them, a failed one
    keeps them.
- Not changed: the repo's scripts and harnesses pass no flag or variable, so
  they still use `.r6-build/runtime-win64`. None reads the temporaries.
- Controls: `link-runtime-dir-empty`, `link-runtime-env-empty` (rejected, LINK);
  three builds of `ok-link-runtime-dir` through the flag, the variable and the
  flag over a wrong variable. `run_reject.py` cases carry optional `args` and
  `env`.

## D-M1-99 - User symbols are emitted as `o_<name>`

- Date: 2026-10-28.
- Reason: the language's C backend emits every user function as `o_<name>`
  (orbit-lang `tests/suite/c_names_user_defined.orb`). R6 emitted the IR name,
  so a user function named `system_uptime` or `current_lease` met the runtime
  archive's unprefixed export and the link reported a multiple definition. The
  same collision held for any libc, CRT or Win32 name (`printf`, `remainder`).
- Decision:
  - Every non-extern function is defined, called and bound as a value under
    `o_<name>`. The runtime's own names start with `orbit_`, which `o_`
    cannot spell, and `o_` is the prefix of the C backend.
  - The entry stays `orbit_main`, called by the wrapper `main` that the CRT
    link needs. Externs keep their names.
  - Models, unions and globals emit no symbol in the object, so they need no
    prefix. The `where` strings of runtime errors keep the IR name.
  - A function name over 140 bytes is `error[CGEN]`: the assembler label is 160
    bytes and carries `__r6_unwind_` and `o_`, and truncation would merge two
    names.
- Controls: `101-user-symbol-prefix` (user functions `printf`, `remainder`,
  `system_uptime`, `current_lease`, `orbit_main_like`, one bound as a value;
  differential against the oracle; the unprefixed driver fails it with
  `multiple definition of system_uptime`), `ok-user-name-max` (140 bytes
  accepted) and `cgen-user-name-too-long` (141 bytes rejected).

## D-M1-100 - Version handshake: `r6 --version`

- Date: 2026-10-28.
- Reason: the language runs `r6.exe` for `--backend=native`. A stale r6 that
  predates a change of the IR or of the command line hung a build instead of
  failing. The language needs to read what an executable speaks before it hands
  it a module.
- Decision: `r6 --version` (also `-V`) prints exactly one line,
  `r6 <semver> ir-protocol <N>`, to stdout and exits 0. Currently
  `r6 0.1.0 ir-protocol 3` (D-M1-107). `--help` lists it.
- The protocol number N covers what the language and r6 agree on:
  - the command line: `build <ir> -o <exe>`, `--runtime-dir`, the exit codes
    and the one-line diagnostic;
  - the IR the language emits and r6 accepts: `irVersion`, the opcode set, the
    fields r6 reads (`line`, `sourceName`), the width of `int` and the typing
    of registers;
  - the runtime archive ABI r6 links against.
- N bumps when a change makes an executable that lacks it build wrong or fail
  on a module the other side produces: a new or removed flag, a new field or
  opcode the language starts to emit, a change of the width or meaning of a
  type, a runtime symbol r6 starts to call. A fix inside r6 that leaves these
  untouched changes the semver only.
- History: 1 is every r6 before this decision (32-bit `int`, no `sourceName`,
  no `--runtime-dir`, no handshake; such an r6 has no `--version` and exits 2).
  2 is the 64-bit `int` with the division stop (D-M1-94, D-M1-95), the source
  name and line (D-M1-97), the runtime directory (D-M1-98) and `o_` symbols
  (D-M1-99).
  3 adds `build --object`, the module field `runtimeModule`, the function
  field `export` and the four byte opcodes (D-M1-105, D-M1-106, D-M1-107).
- The language refuses an r6 whose protocol it does not know.

## D-M1-101 - String literals over 64 bytes span several `.byte` lines

- Date: 2026-10-28.
- Reason: at the pin `bf38b38` the compiler's usage text for `orbit build` is
  a 280-byte string literal. `emit_string` wrote it as one `.byte` line of 1058
  bytes and the assembler refuses lines over 1023 bytes, so
  `compiler/host/main.orb` stopped building (`error[ASSEMBLER]: assembler line
  305047 exceeds 1023 bytes`) and the census ratchet fell from 2 to 1 build.
- Decision: `emit_string` ends the line after 64 bytes and opens a new `.byte`
  line. The bytes and their order are unchanged. The assembler limit is not
  raised: a line limit is what keeps its fixed buffers bounded.
- Control: `ok-long-string` (a 300-byte literal printed whole). The driver
  without the change fails it with the assembler error above.

## D-F0-20 - Re-pin to bf38b38: 64-bit int, `o_` symbols, native default

- Date: 2026-10-28. Moves the pin from `6a9ddaf` (D-F0-19) to orbit-lang
  `main` `bf38b38866fa89728d0236c2f17ab7872230d6a4`, 27 commits in seven merges.
- What moved:
  - `int64`: `orbit_int` is `int64_t`, `div` and `mod` are checked and read the
    module `sourceName` and the instruction `line`, and `shl`/`shr` mask the
    count with 63. R6 lowers this since D-M1-94 to D-M1-97.
  - `c-mangle`: the C backend emits every user symbol as `o_<name>`. R6 does
    the same since D-M1-99. `c_names_user_defined` joins the suite.
  - `security`: `Model.where` binds its parameters, secrets compare in
    constant time (`orbit_ct_equal` replaces `orbit_streq`) and the HTTP
    request line, headers and body are capped before allocation.
  - `native-backend`: `orbit build --backend=native` and `ORBIT_BACKEND` run
    R6, and native is the default on Windows.
  - Suite: eight programs join (`c_names_user_defined`, `division_by_zero`,
    `division_checked_values`, `division_int_min_overflow`,
    `float_call_result_binding`, `int_wrap_64`, `remainder_by_zero`,
    `remainder_int_min_overflow`) and two leave (`int_wrap_32`,
    `namespace_user_names`): 120 suite and 23 std programs.
- `scripts/mklib_runtime.py` did not move; `runtime/` changed in 20 files. The
  runtime archive is rebuilt from the pin: `orbit_lib.o` defines 343 symbols
  (337 before), 145160 bytes.
- Harness: with native the default, the oracle build of `run_diff.py` and
  `run_corpus.py` would run r6 and compare R6 with itself. Both set
  `ORBIT_BACKEND=c` for the oracle. Without it a build against a stale r6 did
  not finish in 300 s.
- R6 changes the pin needed: D-M1-101. The usage text of `orbit build` is a
  280-byte literal and the assembler refused its 1058-byte `.byte` line, which
  dropped the census from 2 to 1 build.
- Inputs at the pin (`docs/PIN.md`): fixed point
  `C:\Temp\orbit-fp-099b002148.exe` (sha256 `7c511c23...`, 947200 bytes),
  built from `C:\Temp\orbit-pin-bf38b38-git`; `PUBLISHED_C 70129F41.` (was
  `C1176CF4.`). The corpus regenerates to 144 documents, 6992300 bytes,
  `filesSha256 94025f2e.`; all 136 documents that remain change hash.
- Ratchets: the corpus baseline is taken at the pin (`r6BuildOk 143`, `match
  143`; the previous baseline names another corpus), and so are the census
  baseline (27 modules, 2 build) and `axes-baseline.json` (143 programs, best
  of 5).
- Measured: differential **100/100** against the new oracle; loader
  **341/341** (215 rejections, 126 acceptance controls); corpus **137 build /
  137 match over 137 -> 143 build / 143 match over 143 programs**, none
  differing; census **2 build over 27 modules**; self-host C `70129F41...` from
  the IR of `compiler/host/main.orb`, equal to `PUBLISHED_C`.
- Status: **applied**.

## D-M1-102 - Start-up per program, as docs/STARTUP.md of the pin

- Date: 2026-10-28. Pin: `bf38b38` (D-F0-20), measured against the runtime
  archive of `cc6b167` built with `scripts/mklib_runtime.py` (19 members).
- Reason: the pin splits the runtime archive into one member per module and
  states in `docs/STARTUP.md` what the entry of a program must do. R6 emitted
  `_orbit_argv` and `_orbit_argc` in every `main` and lowered `print` of a string
  to `printf`. The first reference links `os.o`, `collections.o` and `advapi32`;
  the second links `__mingw_pformat`. `hello` (`print("hello")`) was 66 512 bytes
  of `.text` and imported ADVAPI32.
- Decision:
  - The names come from the `call` instructions of every function, with the
    prefixes of `moduleCallsStartup` in `compiler/emit/c_backend.orb`.
  - `main` stores `_orbit_argc` and `_orbit_argv` only when a call starts with
    `orbit_os_argc` or `orbit_os_argv`.
  - `main` calls `orbit_crash_handler_install` first, always.
  - `main` calls `orbit_console_init` when the module calls `print` or
    `orbit_os_write_std*`.
  - `print` of a string is `orbit_print_string(s)`. Int, bool and float keep
    `printf`, as the pin does.
  - The driver passes `-lws2_32` only for a module with a call that starts with
    `orbit_http_`, `orbit_response_`, `orbit_route_`, `orbit_send_`,
    `orbit_kynx_`, `orbit_pulse_` or `orbit_ledger_`, a call to
    `orbit_file_upload_save`, or an `orbit_db_*` extern. A database program is a
    network program because the progress handler lives in `kynx.c`.
- Measured: `hello` `.text` 66 512 -> 13 216 bytes, `.rdata` 6 632 -> 3 080,
  `.bss` 11 376 -> 8 768, DLLs ADVAPI32, KERNEL32, msvcrt -> KERNEL32, msvcrt.
  The C backend at `cc6b167` prints 13 072 / 3 064 / 8 736 for the same program.
  `run_diff.py` 100/100 and `run_reject.py` 341/341 with the new archives.
- Fixtures: `46-process-argc` is the positive control for the argv rule; every
  other fixture builds without the reference.

## D-M1-103 - The database archive is the pin's per-module archive

- Date: 2026-10-28. Pin: `bf38b38` (D-F0-20); the runtime of `cc6b167` is the
  first one to need it.
- Reason: `scripts/build-runtime-db.py` compiled `LIB_TU`, the one translation
  unit of `scripts/mklib_runtime.py`, with `-DORBIT_WITH_DB`. The language at
  `cc6b167` has no `LIB_TU`: each module is `runtime/<m>.h` and `<m>.c`, compiled
  with `-DORBIT_RUNTIME_MODULE`, and `mklib_runtime.py --db` writes the archive
  of 21 members (19 plain, plus `database` and `auth`).
- Decision: the script runs `mklib_runtime.py --db --quiet --cc <cc> --out
  <absolute path>` from the checkout and copies `sqlite3.lib` from
  `runtime/vendor/win-x64`. R6 keeps no copy of the recipe. A change of the
  pin's member list reaches R6 through the pin alone.
- Measured: the script writes a 21-member archive; `90-db-link` and
  `95-db-migrations` pass against it (`run_diff.py --case`, 2/2).

## D-M1-104 - R6 owns the process entry of a program that needs no MinGW start-up

- Date: 2026-10-28. Pin: `bf38b38` (D-F0-20), measured against the runtime
  archive of `01e31e8` (25 members).
- Reason: `crt2.o`, `crtbegin.o`, `crtend.o`, `libmingw32` and `libmingwex`
  start-up code is 6 400 bytes of `.text` in `int main(void){return 0;}`. After
  D-M1-102 `hello` was 8 064 bytes of `.text` with the arena split of `01e31e8`
  and the CRT in it. Criterion 4 asks for 8 192 or fewer.
- Decision, the entry rule: a module has R6's entry `orbit_start` when it has no
  `orbit_os_argc*`/`orbit_os_argv*` call, no network call or `orbit_db_*` extern
  (D-M1-102), and no migrations. Any other module keeps `main` and the MinGW
  start-up objects, as before.
  - `orbit_start` calls `__set_app_type(1)` (console application, which msvcrt
    reads to choose where fatal diagnostics go), then the sequence of D-M1-102,
    `orbit_main`, `orbit_arena_destroy`, and ends in msvcrt `exit(code)`, which
    flushes stdio and runs the atexit table. `ExitProcess` alone would drop
    buffered `printf` output.
  - The driver links with `-e orbit_start --subsystem console
    --disable-auto-import --disable-runtime-pseudo-reloc` and without the
    start-up objects, so a runtime member that needed a pseudo-relocation is a
    link error, not a silent miss. The 16 MiB stack of D-M1-97 is a PE header
    field and stays.
  - `atexit` is not in any import library: `crt2.o` defines it, and libmingwex's
    gdtoa (float formatting) calls it. The driver names `msvcrt.dll` on the ld
    command line, which imports `atexit` from the DLL; the executable shows
    `msvcrt.dll` twice in its import directory.
  - After the link the driver reads the section table of the executable. A
    `.ctors`, `.dtors`, `.init_array`, `.fini_array` or `.preinit_array` section
    is `error[LINK]: the runtime needs the MinGW start-up`, and the executable
    is deleted. Constructors only run in that start-up. Thread-local storage
    stays: the loader reads the TLS directory, and the arena uses it.
  - Not done, and why: the MinGW start-up stays for argv (the pin's `os.c`
    reads `_orbit_argv`, filled by `__getmainargs` in `crt2.o`), for network and
    database programs, and for a module with migrations. `--gc-sections` was not
    tried: R6's object has one `.text` section, the pin builds the archive
    without `-ffunction-sections`, and `hello` is already under the target.
- Measured (`hello`, `print("hello")`, link against the `01e31e8` archive):

  | | `.text` | `.rdata` | `.data` | `.bss` | DLLs | build ms | run ms |
  |---|---|---|---|---|---|---|---|
  | R6 before D-M1-102 | 64 080 | 6 632 | 608 | 11 344 | ADVAPI32, KERNEL32, msvcrt | 62 | 10.5 |
  | R6 after D-M1-102 | 8 064 | 2 792 | 192 | 8 768 | KERNEL32, msvcrt | 55 | 10.0 |
  | R6 after D-M1-104 | 1 792 | 400 | 16 | 8 352 | KERNEL32, msvcrt | 45 | 10.0 |
  | C backend, `-O1 -s` | 7 904 | 2 776 | 192 | 8 704 | KERNEL32, msvcrt | 1 200 | 10.1 |

  Best of 7 for build and run; the run time is the process start of PowerShell
  around a program that does nothing else.
- Controls: `46-process-argc` and the two database fixtures keep `main` and the
  MinGW start-up; every other fixture runs on `orbit_start`. The loader suite
  keeps `x-db-migrations-no-db-extern` (a migrations module does not take the new
  entry and is still refused).
- Measured: corpus 143 build / 143 match over 143 programs (an earlier link
  without `atexit` printed 135, with eight float programs crashing); `run_diff.py`
  100/100; `run_reject.py` 341/341.

## D-F0-21 - Re-pin to 01e31e8: per-module runtime and per-program start-up

- Date: 2026-10-28. Moves the pin from `bf38b38` (D-F0-20) to orbit-lang `main`
  `01e31e882610cea21fbfbf4fe02fee00051fab2e`, 8 commits in two merges.
- What moved:
  - `lean-runtime` (`cc6b167`): each runtime module is `runtime/<m>.h` plus
    `<m>.c`, compiled with `-DORBIT_RUNTIME_MODULE`. `scripts/mklib_runtime.py`
    writes one archive member per module and `LIB_TU` is gone. `docs/STARTUP.md`
    states the entry rules R6 mirrors (D-M1-102).
  - `arena-split` (`01e31e8`): `arena` is a core member of 1 144 bytes of
    `.text` and seven extensions (`arena_alloc`, `arena_oom`, `arena_reset`,
    `arena_checkpoint`, `arena_child`, `arena_stats`, `arena_global`);
    `string_pool` leaves the member list.
  - The archive has 25 members (27 with `--db`).
- R6 changes the pin needed: D-M1-102 (start-up per program), D-M1-103 (the
  database archive from the pin's recipe) and D-M1-104 (R6's own process entry).
  All three are committed before this one.
- Inputs at the pin (`docs/PIN.md`): fixed point
  `C:\Temp\orbit-fp-ee60c8af43.exe` (sha256 `9de8324a...`, 2 720 256 bytes),
  built from `C:\Temp\orbit-pin-01e31e8-git`; `PUBLISHED_C E794E2DB.` (was
  `70129F41.`). The corpus regenerates to 144 documents, 6 992 633 bytes,
  `filesSha256 a533ace1.`; the program list is the one of D-F0-20.
- Ratchets: the corpus baseline stays at `r6BuildOk 143`, `match 143`; the
  census baseline is re-taken (27 modules, 2 build, `irFilesSha256 61e61734.`)
  and so is `axes-baseline.json` (143 programs, best of 5).
- Measured: differential **100/100** against the new oracle; loader **341/341**;
  corpus **143 build / 143 match over 143 programs**, none differing; census
  **2 build over 27 modules**; self-host C `E794E2DB...` from the IR of
  `compiler/host/main.orb`, equal to `PUBLISHED_C`. `hello` is 1 792 bytes of
  `.text` in R6 and 7 904 in the C backend (D-M1-104).
- Status: **applied**.

## D-M1-105 - Runtime modules: `runtimeModule`, `export` and `build --object`

- Date: 2026-10-28. Pin unchanged (`01e31e8`, D-F0-21).
- Reason: the Orbit runtime, written in C, will be ported to Orbit one module at
  a time. A ported module is an Orbit file without `main` whose `export fn`
  functions keep their C names and join the runtime archive. R6 must compile such
  a module to an object, not to an executable.
- Decision, the IR (admitted by the loader, `src/loader/ir.c`):
  - `module.runtimeModule` is an optional boolean, default false. `function.export`
    is an optional boolean, default false. Both are absent from every module the
    pin emits, so an absent field is the old behaviour.
  - `export: true` outside a runtime module is `E_SCHEMA`. So is `export` on an
    extern function, an exported name that is not a C identifier
    (`[A-Za-z_][A-Za-z0-9_]*`), a non-boolean `export` or `runtimeModule`, and a
    runtime module that defines `main` (an extern `main` declaration is not a
    definition).
- Decision, the code (`src/codegen-x64/win64.c`):
  - An exported function's symbol is its IR name, with no `o_` prefix (D-M1-99
    keeps `o_` for every other definition). It is global.
  - A function of a runtime module that is not exported keeps `o_<name>` and is
    file-local (no `.globl`), so two members of the archive can carry a helper of
    the same name. `nm` prints it as `t`.
  - Exported functions are the roots of reachability (D-M1-65); a runtime module
    has no `main` and no entry wrapper, so no `orbit_main`, `orbit_start` or
    `main` symbol is emitted.
  - The arena of an executable is the `r6_arena` mirror that `main` fills
    (D-M1-23). A runtime module is archived into programs the C backend also
    links, where `r6_arena` does not exist. The operations that read it (lists,
    model and union cells, string `add`, `print`, arena-taking externs) are
    `error[CGEN]: a runtime module cannot use the executable arena`.
- Decision, the driver (`src/driver/r6.c`): `r6 build --object <in.ir> -o <out.o>`
  runs load, codegen, peephole, regalloc, asm and coff, writes the object to the
  `-o` path and does not run `ld` or look for the runtime archive. `--object`
  with an ordinary module, and a runtime module without `--object`, are
  `error[CLI]` with exit 1: the module is what disagrees with the flag, which is
  an input error and not a misspelled command line.
- Controls (`tests/reject/`): ten rejections `rt-*` (export outside a runtime
  module, main in a runtime module, two non-identifier names, an exported extern,
  two non-boolean fields, both `CLI` cases and the arena refusal) and
  `ok-runtime-module-object`, which builds two exports and a local helper. The
  object's symbols were read with `nm`: `rtmod_probe_add` and
  `rtmod_probe_twice_sum` are `T`, `o_rtmod_twice` is `t`.
- Status: **applied**.

## D-M1-106 - Byte opcodes of a runtime module: `str_len`, `byte_load`, `byte_store`, `bytes_alloc`

- Date: 2026-10-28. Pin unchanged (`01e31e8`, D-F0-21).
- Reason: a runtime module written in Orbit reads and writes the bytes of a
  string. The IR has no opcode for that, and a C-string runtime function such as
  an upper-casing helper needs all four.
- Decision, admission: the four opcodes are admitted only when
  `module.runtimeModule` is true. In an ordinary module they are
  `E_UNSUPPORTED: opcode is admitted only in a runtime module`, so a module the
  pin emits today cannot reach them.
- Decision, semantics (`src/codegen-x64/win64.c`). Every operand is a register:
  an inline literal is `error[CGEN]`, because these opcodes have no scratch slot
  for one.

  | Opcode | dest | operands | Lowering |
  |---|---|---|---|
  | `str_len` | int | string | `strlen`; a NULL string is 0, as `orbit_string_len` of the pin |
  | `byte_load` | int | string, int index | `movzbl (s+i)`: zero-extended, no bounds check |
  | `byte_store` | none (`-1`) | string, int index, int value | `movb %dl, (s+i)`: the low 8 bits of the value |
  | `bytes_alloc` | string | int n | `orbit_alloc(orbit_arena_get_global(), n + 1)`, byte `n` set to 0 |

  - `byte_store` with a destination is `E_SCHEMA`; the other three without one
    are `E_SCHEMA` too.
  - `bytes_alloc` calls the global arena and not `r6_arena`: the object may be
    linked into a program built by the C backend (D-M1-105). Both symbols are
    declared in `runtime/arena.h` of the pin (`void* orbit_alloc(OrbitArena*,
    size_t)`, `OrbitArena* orbit_arena_get_global(void)`) and live in the
    `arena_alloc` and `arena_global` members of the archive. The byte after the
    allocation is written, and nothing else is cleared.
  - The assembler gets no new instruction: the lowering uses `movzbl (%rax)`,
    `movb %dl, (%rax)`, `movb $0, (%rcx)`, `leaq 1(%rdx), %rdx` and the address
    `addq %rcx, %rax`, which `asm_win64.c` already encodes.
- Controls: `rt-*-ordinary-module` (four opcodes refused outside a runtime
  module), `rt-byte-store-with-dest`, `rt-byte-load-missing-dest`,
  `rt-str-len-int-operand`, `rt-byte-load-string-index`,
  `rt-byte-load-literal-index`, `rt-byte-store-string-value` and
  `rt-bytes-alloc-int-dest` refuse the wrong operand or destination;
  `ok-runtime-module-bytes` builds all four in a runtime module.
- End to end (`python tests/runtime-module/run_runtime_module.py`): the runtime
  module `probe-rt.json` exports `rtmod_probe_upper(s: string) -> string` and
  `rtmod_probe_sum(s: string) -> int`. The script builds it with `--object`,
  checks with `nm` that both are `T` and that no `main`, `orbit_main` or
  `orbit_start` exists, shows that `probe-main.json` fails with `error[LINK]`
  while the member is missing, adds the object to a temporary copy of
  `libruntime_net.a` with `ar`, links the program and compares its output. It
  printed `HELLO, ORBIT 42!`, an empty line and `468` (the byte sum of `h` and
  the two bytes of `é`, 104 + 195 + 169, which a sign-extending load would turn
  into 104 - 61 - 87 = -44), exit 0.
- Not done: a literal operand, a bounds check, a `bytes_alloc` that clears the
  bytes, and any other string operation. A new opcode is a new decision.
- Status: **applied**.

## D-M1-107 - `ir-protocol 3`

- Date: 2026-10-28. Pin unchanged (`01e31e8`, D-F0-21).
- Reason: D-M1-100 bumps the protocol when the IR gains a field or an opcode the
  language starts to emit, or when the command line gains a flag. D-M1-105 and
  D-M1-106 add one flag (`--object`), two fields (`runtimeModule`, `export`) and
  four opcodes. A language that emits them must be able to refuse an r6 built
  before them, which reads the field as an unknown key (`E_SCHEMA`) and the
  opcodes as `E_OPCODE`.
- Decision: `r6 --version` prints `r6 0.1.0 ir-protocol 3`. The semver stays.
- What the bump does not touch: the pinned compiler `01e31e8` requires
  `ir-protocol 2`. `tests/diff/run_diff.py`, `tests/corpus/run_corpus.py`,
  `tests/compiler-ir/run_census.py` and `scripts/selfhost.ps1` call `r6.exe build`
  directly and never read the version, so they are unaffected. A build through
  `orbit build --backend=native` with the pinned fixed point refuses this r6
  until the language moves to protocol 3.
- Status: **applied**.

## D-M1-108 - `RtArena`: the arena a runtime function allocates from

- Date: 2026-10-28. Pin unchanged (`01e31e8`, D-F0-21).
- Reason: several runtime string functions take `OrbitArena* arena` as their
  first C parameter, and a program passes the arena of the request. A runtime
  module that allocated from the global arena (D-M1-106) would keep every
  per-request string until the process ends.
- Decision, the type: `RtArena` is an opaque 64-bit pointer, passed like any
  reference in the integer argument register or slot. It is admitted as a
  parameter type and a register type of a runtime module only. In any function
  of an ordinary module (including an extern declaration) it is
  `E_SCHEMA: type RtArena is admitted only in a runtime module`. A runtime
  module can pass an `RtArena` register as a call argument. It has no
  literal, no comparison and no conversion.
- Decision, `bytes_alloc`: the optional `operand2` is an `RtArena` register. With
  it the lowering is `orbit_alloc(a ? a : orbit_arena_get_global(), n + 1)` and
  the NUL byte; without it the allocation is the global arena (D-M1-106). An
  operand2 that is a literal or a register of another type is `CGEN`.
- Test hook: R6 prepends the arena to an extern only for the names in
  `extern_needs_arena` (D-M1-27). `rtmod_probe_lower` is added to that list so
  that an ordinary program can reach an arena-first export; no pinned runtime
  symbol has that name. The names the runtime really defines (for example
  `orbit_string_slice`) are already in the list.
- Controls: `rt-rtarena-param-ordinary-module`,
  `rt-rtarena-register-ordinary-module`, `rt-rtarena-extern-ordinary-module`,
  `rt-bytes-alloc-arena-int`, `rt-bytes-alloc-arena-string`,
  `rt-bytes-alloc-arena-literal`; the acceptance control is
  `ok-runtime-module-arena`.
- End to end: `tests/runtime-module` adds the export
  `rtmod_probe_lower(a: RtArena, s: string) -> string`, which forwards `a` to a
  file-local helper that allocates with it. The ordinary program calls it as an
  arena-taking extern and prints `hello, orbit 42!` for `Hello, ORBIT 42!`. The
  script also checks that the object references `orbit_alloc` and
  `orbit_arena_get_global`.
- Not measured: the NULL branch of `a ? a : global`, because no IR value of
  type `RtArena` is NULL.
- Status: **applied**.

## D-M1-109 - `RtArena` through locals: `load_var`, `decl_var`, `store_var`, `copy`

- Date: 2026-10-28. Pin unchanged (`01e31e8`, D-F0-21).
- Reason: the language does not reference a parameter register. It emits
  `arg`, then `load_var` of the parameter name wherever the body reads it
  (`tests/runtime-module/emitted/arena.json`, emitted by the runtime-module
  branch of orbit-lang). D-M1-108 admitted `RtArena` in parameters, registers
  and call arguments only, so that IR was `error[CGEN]: load_var requires a
  supported local, parameter, ...`.
- Decision: an `RtArena` is a 64-bit reference like a list or a model pointer in
  `load_var` of a parameter or a local, `decl_var`, `store_var` and `copy`. The
  types must match as for the other references: an int register does not copy
  into an `RtArena` register and an int local does not load into one. The
  loader still admits the spelling in runtime modules only (D-M1-108), so an
  ordinary module has no way to reach these paths.
- Test hook: `rtmod_probe_copy_in` joins `rtmod_probe_lower` in
  `extern_needs_arena`, for the same reason (D-M1-108).
- Inputs: `tests/runtime-module/emitted/arena.json` and `upper.json` are the
  IR of the fixed point `1769474c12` of orbit-lang `runtime-module` for
  `tests/runtime_module/arena.orb` and `upper.orb`; `REBUILD.txt` there gives the
  command. They are committed inputs, not regenerated here.
- Controls: `rt-rtarena-copy-from-int` and
  `rt-rtarena-decl-int-local-load-as-rtarena` refuse the mismatch;
  `ok-runtime-module-arena-var`, `emitted/arena.json` and `emitted/upper.json`
  build. The end-to-end script builds both emitted files, adds the arena one to
  the archive, and the ordinary program prints `copy me` through it.
- Status: **applied**.

## D-M1-110 - `str_offset` and `bytes_copy`

- Date: 2026-10-28. Pin unchanged (`01e31e8`, D-F0-21).
- Reason: the Orbit port of `orbit_string_trim` went from 37 ms to 98 ms because
  it copies byte by byte where the C function calls `memcpy`. Slice, concat and
  replace need the same two operations.
- Decision, admission: both opcodes follow D-M1-106. They are admitted only in a
  runtime module (`E_UNSUPPORTED: opcode is admitted only in a runtime module`
  elsewhere), every operand is a register (an inline literal is `CGEN`), and
  a type that does not match is `CGEN`.

  | Opcode | dest | operands | Lowering |
  |---|---|---|---|
  | `str_offset` | string | string s, int i | `s + i`: no allocation, no check |
  | `bytes_copy` | none (`-1`) | string dst, string src, int n | `memcpy(dst, src, n)` of msvcrt when `n > 0`; a count of 0 or below copies nothing |

  `bytes_copy` with a destination and `str_offset` without one are `E_SCHEMA`.
  The count is tested as a signed 64-bit value before the call, so a negative
  count never reaches `memcpy` as a huge `size_t`.
- Controls: `rt-str-offset-ordinary-module`, `rt-bytes-copy-ordinary-module`,
  `rt-bytes-copy-with-dest`, `rt-str-offset-missing-dest`, the literal operand
  of each, and the wrong operand or destination type of each refuse the input;
  `ok-runtime-module-slice` builds both.
- End to end: `rtmod_probe_slice(a: RtArena, s, start, len)` copies a middle
  slice into a buffer from `bytes_alloc`; the program prints `Orbit` for
  `("hello, Orbit 42", 7, 5)`. `rtmod_probe_guard` writes `x` into a buffer and
  calls `bytes_copy` with a count of -5; the program prints `x`. `rtmod_probe_slice`
  joins the `extern_needs_arena` test hook (D-M1-108).
- Status: **applied**.

## D-M1-111 - `eq` and `ne` of a string or an `RtArena` with `NULL`

- Date: 2026-10-28. Pin unchanged (`01e31e8`, D-F0-21).
- Reason: orbit-lang `ef7e751` no longer retypes the register of the `NULL`
  literal as `string`, so `s == NULL` reaches R6 as a string register against a
  `NULL` register. R6 read it as a string comparison and refused it
  (`runtime/orb/strings.orb`, 11 string and 3 `RtArena` comparisons).
- Decision: in `eq` and `ne`, a string register or an `RtArena` register
  against the register of the `NULL` literal is a comparison of two 64-bit
  words, in either order, in every module. No `strcmp` call. The register of the
  literal is the one `canonical_union_spelling` types `void*` and that is
  defined exactly once, by `load_const NULL` (`null_register`, D-M1-59). A
  string against a string stays `strcmp`.
- Not admitted, and `error[CGEN]`: an ordering (`lt`, `le`, `gt`, `ge`) against
  `NULL`, a string against any other `void*` word, and a `NULL` register
  assigned twice.
- Controls: `x-string-lt-null`, `x-string-eq-voidptr-param` and
  `x-string-eq-null-register-redefined` refuse the neighbours;
  `ok-runtime-module-null-compare` builds the four forms; `ok-string-null-compare`
  runs an ordinary program (`"here"` and `""` are not NULL, byte-compared output)
  and `ok-string-null-argument` shows NULL equal to NULL. Neither is a
  differential fixture: the pinned C backend lowers `s == NULL` to `strcmp` and
  the oracle crashes with 0xC0000005 even on a non-NULL string (`run_diff.py
  --no-oracle-cache` on such a fixture), so `run_diff.py` stays 100/100.
  `tests/runtime-module/emitted/strings.json` is the IR of
  `runtime/orb/strings.orb` from the fixed point `1769474c12` of orbit-lang
  `runtime-module`, and `scripts/mklib_runtime.py --r6` builds 26 of 26 members
  with it.
- Status: **applied**.

## D-M1-112 - `result_ok` and `result_err` write the `OrbitResult` in place

- Date: 2026-10-28. Pin unchanged (`01e31e8`, D-F0-21).
- Reason: orbit-lang removes `runtime/types.c` (the five `OrbitResult`
  constructors become `static inline`), so an R6 program must not reference
  `orbit_result_ok`, `orbit_result_ok_float` or `orbit_result_err`.
- Decision: `result_ok` and `result_err` store the four fields of the
  24-byte result cell directly: `bool ok` at +0 (written as a 32-bit word),
  `int error_code` at +4, `const char *error_msg` at +8, `void *value` at +16.
  Layout checked against `runtime/orbit_api.h` of orbit-lang `develop`.
  `result_ok` writes the value operand as its 64-bit pattern: an int, a pointer,
  a string address, a bool zero-extended or a float's IEEE-754 bits, which is
  what `orbit_result_ok_float` boxed. `result_err` writes the code as 32 bits,
  the message (or NULL) and a NULL value. Every operand form the lowering
  admitted stays admitted; no new input is accepted.
- Evidence of the removal: `22-result-ops.json` rebuilt as a runtime module
  (`runtimeModule: true`, `main` renamed and exported) has no `orbit_result_ok`,
  `orbit_result_ok_float` or `orbit_result_err` in `nm`; the only result symbol
  it references is `orbit_result_get_float`, which `result_unwrap` of a float
  still calls.
- Status: **applied**.

## D-M1-113 - `RtPtr` and `reinterpret`

- Date: 2026-10-28. Pin unchanged (`01e31e8`, D-F0-21).
- Reason: the Orbit port of the list functions (orbit-lang `runtime/orb/lists.orb`)
  reads and writes the fields of `OrbitList`, so a runtime module needs a pointer
  that is not a string. Amendment 2 of the runtime design names it `RtPtr`.
- Decision, the type: `RtPtr` is an opaque 64-bit pointer, passed like any
  reference in the integer argument register or slot. It is admitted as a
  parameter type (including the parameters of an extern declaration, next to
  `RtArena`), a register type and a call argument of a runtime module only. In
  any function of an ordinary module, extern declarations included, it is
  `E_SCHEMA: type RtPtr is admitted only in a runtime module`. It travels through
  `load_var`, `decl_var`, `store_var` and `copy` as `RtArena` does (D-M1-109).
  It has no literal, no return type and no conversion except `reinterpret`.
- Decision, comparison: `eq` and `ne` of an `RtPtr` register against the register
  of the `NULL` literal are a comparison of two 64-bit words, in either order
  (D-M1-111). An ordering against `NULL`, a comparison with a `void*` parameter
  or with a string stays `error[CGEN]`.
- Decision, `reinterpret`: dest `RtPtr` with a string register operand, or dest
  string with an `RtPtr` register operand. The lowering is a 64-bit copy. Same
  admission as D-M1-106: runtime modules only (`E_UNSUPPORTED` elsewhere), a
  register operand (an inline literal is `CGEN`), a destination (`E_SCHEMA`).
  Any other pairing, including string to string and `RtPtr` to `RtPtr`, is `CGEN`.
- Decision, `str_offset`: also takes an `RtPtr` register and produces an `RtPtr`.
  A string offsets to a string and an `RtPtr` to an `RtPtr`; the two do not mix.
- Controls: `rt-rtptr-param-ordinary-module`, `rt-rtptr-register-ordinary-module`,
  `rt-rtptr-extern-ordinary-module`, `rt-reinterpret-ordinary-module`,
  `rt-reinterpret-missing-dest`, five wrong pairings or operands of
  `reinterpret`, two wrong pairings of `str_offset`, `rt-rtptr-copy-from-int`,
  `rt-rtptr-decl-int-local-load-as-rtptr`, `rt-rtptr-lt-null`,
  `rt-rtptr-eq-voidptr-param`, `rt-rtptr-eq-string`,
  `rt-bytes-alloc-arena-rtptr` and `rt-rtptr-extern-argument-int` refuse the
  input; `ok-runtime-module-rtptr` builds the admitted forms.
- Status: **applied**.

## D-M1-114 - `mem_load` and `mem_store`

- Date: 2026-10-28. Pin unchanged (`01e31e8`, D-F0-21).
- Reason: `OrbitList` is `{data, len, capacity, elem_size, elem_kind, arena}`. The
  list functions of orbit-lang `runtime/orb/lists.orb` read and write those fields
  through an `RtPtr` (D-M1-113).
- Decision: both opcodes follow D-M1-106. Runtime modules only (`E_UNSUPPORTED`
  elsewhere), every operand a register (an inline literal is `CGEN`).

  | Opcode | dest | operands | `elemKind` 8 | `elemKind` 4 |
  |---|---|---|---|---|
  | `mem_load` | int, or `RtPtr` with `elemKind` 8 | `RtPtr` p, int off | `movq (p+off)` | `movslq (p+off)`: sign-extended |
  | `mem_store` | none (`-1`) | `RtPtr` p, int off, int or `RtPtr` value | `movq` of the word | `movl` of the low 32 bits |

  The address is `p + off` with `off` in bytes. No null test and no bounds test.
  `mem_store` with a destination is `E_SCHEMA`, as `byte_store`. `elemKind` of a
  memory opcode is an integer in 0..8 (`E_RANGE` above); of any other opcode it
  stays 0..5. An `elemKind` other than 8 or 4, a string or int pointer operand, a
  string or bool destination, and a pointer moved with `elemKind` 4 are `CGEN`.
  `elemKind` on an opcode that is neither a list nor a memory opcode stays
  `E_SCHEMA`.
- Controls: `rt-mem-load-ordinary-module`, `rt-mem-store-ordinary-module`,
  `rt-mem-store-with-dest`, `rt-mem-load-missing-dest`,
  `rt-mem-elemkind-out-of-range`, `rt-elemkind-8-on-list-create` and fourteen
  wrong operands, destinations or kinds refuse the input;
  `ok-runtime-module-mem` builds both widths and the pointer form.
- Status: **applied**.

## D-M1-115 - `float_bits` and `bits_float`

- Date: 2026-10-28. Pin unchanged (`01e31e8`, D-F0-21).
- Reason: `orbit_list_push_float` and `orbit_list_get_float` store a float in the
  64-bit element word as its IEEE-754 bit pattern. The Orbit port has to move
  that pattern between a float register and an int register without a numeric
  conversion.
- Decision: both opcodes follow D-M1-106 (runtime modules only, registers only,
  a destination required). `float_bits` takes a float register and produces an
  int register, `bits_float` takes an int register and produces a float
  register. A float slot holds its bit pattern, so both lower to a 64-bit copy
  (`movq`); no `cvtsi2sd`, no `cvttsd2si`. Any other operand or destination type
  and an inline literal are `CGEN`.
- Controls: `rt-float-bits-ordinary-module`, `rt-bits-float-ordinary-module`,
  `rt-float-bits-missing-dest`, `rt-bits-float-missing-dest` and six wrong
  operands or destinations refuse the input; `ok-runtime-module-float-bits`
  builds. The round trip of the pattern of 1.5 and -0.0 runs in
  `tests/runtime-module` (D-M1-117).
- Status: **applied**.

## D-M1-116 - `result_ok` of an `RtPtr`, `result_err` with the code first, an export that returns `result`

- Date: 2026-10-28. Pin unchanged (`01e31e8`, D-F0-21).
- Reason: the list functions return `OrbitResult` (`orbit_list_create_typed`,
  `orbit_list_pop`, `orbit_list_get`) and build it with the inline form of
  D-M1-112. The runtime design names the forms `rt_ok(word: int)`,
  `rt_ok_ptr(p: RtPtr)` and `rt_err(code: int, msg: string)`.
- Decision, `result_ok`: the operand may also be an `RtPtr` register. Its 64-bit
  word is the `value` field, as an int is. An `RtArena`, a `void*` and a model
  register stay `CGEN`.
- Decision, `result_err`: a runtime module may give the code first and the
  message second, both registers (`rt_err(code, msg)` lowers to operand1 = code,
  operand2 = message). The order of every other module, message first and code
  second, and the one-operand form (code only) are unchanged; the two orders are
  told apart by the type of operand1, which is an int register only in the
  code-first form. An ordinary module that gives `(int, string)` stays
  `CGEN: result_err message operand must be a string`.
- Decision, export: an `export fn` with return type `result` returns the
  `OrbitResult` by value through the hidden return pointer, as any function that
  returns a result (D-M1-43). `ok-runtime-module-result` builds one.
- Controls: `rt-result-err-code-first-ordinary-module`,
  `rt-result-err-code-first-literal-message`,
  `rt-result-err-code-first-int-message`, `rt-result-ok-rtarena` and
  `rt-result-ok-int-dest` refuse the input; `ok-runtime-module-result` builds
  `result_ok` of an RtPtr and of an int and `result_err` in three forms.
- Status: **applied**.

## D-M1-117 - end-to-end run of the memory, float-bit and result opcodes

- Date: 2026-10-28. Pin unchanged (`01e31e8`, D-F0-21).
- Reason: D-M1-112 to D-M1-116 are admitted by rejection and acceptance cases that
  only build. `tests/runtime-module` runs them.
- Decision: `probe-rt.json` gains three exports.
  - `rtmod_probe_cell(a: RtArena, x: int, mode: int) -> result` allocates 40 bytes
    with `bytes_alloc`, stores `x` as a word at +0, -3 as 32 bits at +8 and the
    cell's own address at +24, and reads `x` back through the stored pointer and
    -3 back sign-extended. A negative `x` returns `result_err` with code 9 and
    the message `negative`; mode 1 returns `result_ok` of the pointer; otherwise
    `result_ok` of `x + (-3)`.
  - `rtmod_probe_float(a: RtArena, y: float) -> result` stores `float_bits(y)`,
    loads it, converts it with `bits_float` and returns it in `result_ok`.
  - `rtmod_probe_bits(y: float) -> int` returns `float_bits(y)`.
- Output added to the program: `2`, `negative`, `1`, `-2.5`, `4609434218613702656`
  (the pattern of 1.5) and `-9223372036854775808` (the pattern of -0.0).
- The script also requires that the object has no undefined `orbit_result_*`
  symbol.
- Test hook: `rtmod_probe_cell` and `rtmod_probe_float` join the arena-first names
  of `extern_needs_arena` (D-M1-108), which is also what admits a `result`
  returned by an extern; no pinned runtime symbol has those names.
- Status: **applied**.
