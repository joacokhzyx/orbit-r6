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
- Evidence: `runtime/orbit_api.h:106` rejects `ORBIT_WITH_DB` for the net-only library; C backend lowers `db_*` at `c_backend.orb:3533-3590`. The pinned candidate M2 corpus has 89 executable programs (67 suite, 22 std) plus one suite support helper; no source-detectable DB/auth cases were found. This does not replace post-IR differential accounting.
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
- Evidence: backend fallback at `compiler/c_backend.orb:3596-3598`; user accepted explicit error policy 2026-10-14.
- Status: **accepted by user 2026-10-14**.

## D-M1-01 — Lower pinned literal globals as immediate loads

- Date: 2026-10-14.
- Decision: Win64 M1 accepts only globals with valid identifiers, type `int` or `bool`, and decimal `IRValue.String` values in int32 range (bool only 0/1). `load_var` resolves a local first, then a validated module global. Other global forms fail with a diagnostic.
- Alternatives: emit mutable data sections for all globals (rejected: the pinned builder turns literal globals into C preprocessor defines); coerce unsupported values (rejected: semantic invention).
- Evidence: pinned `builder.orb:2288-2304,2333-2372` and `c_backend.orb:1479-1503`; fixture `14-global-constants` differential pass, and full manual fixture set 14/14.

## D-M1-02 — Use the target CRT for string equality

- Date: 2026-10-14.
- Decision: lower string `eq`/`ne` to direct `strcmp` calls using Win64 C ABI. Keep ordered string comparisons unsupported.
- Alternatives: compare pointers (rejected: Orbit compares string contents); copy a string-comparison routine into R6 (rejected: the target CRT already owns this primitive and libc behavior must not be forked).
- Evidence: pinned `c_backend.orb:2550-2564` emits `strcmp(a,b)==0/!=0`; `tests/suite/strings_eq.orb`; R6 fixture `15-string-equality` and full 15/15 differential gate.

## D-M1-03 — Lower Win64 arguments beyond positional registers to stack slots

- Date: 2026-10-14.
- Decision: support up to 256 scalar parameters/arguments. Reserve 32 bytes of shadow space plus eight bytes per outgoing argument after position 4; read callee positions 5+ from the Win64 stack argument area. Float values retain their binary64 bits in the positional slot.
- Alternatives: retain the four-argument limit (rejected: the target ABI and existing native lowering already define stack slots); shift float parameters by type (rejected: Win64 slots are positional).
- Evidence: native differential fixtures `16-call-stack-args` and `17-call-stack-float`; full manual gate 17/17.

## D-M1-04 — Call official runtime for int-list creation and push

- Date: 2026-10-14.
- Decision: implement only `list_create`/`list_push`/`list_len` for int lists in `main`. Use official `orbit_list_create_typed` and `orbit_list_push_int`; honor Win64 `OrbitResult` sret and its pinned header layout. Lower the header's inline `orbit_list_len` as a load of the declared `OrbitList.len` member.
- Alternatives: reimplement list allocation/push/storage (rejected: duplicates runtime ownership); accept all element kinds (rejected: typed helpers and failure paths are not yet lowered); skip the `OrbitResult.ok` field (rejected: invents failure behavior).
- Evidence: pinned builder `builder.orb:4731-4749`; C lowering `c_backend.orb:3381-3415`; ABI `runtime/orbit_api.h:179-184,217-224,361-376,517-519`; fixture `18-int-list-literal-len`; full manual differential gate 18/18.

## D-M1-05 — Lower checked int-list reads with official runtime errors

- Date: 2026-10-14.
- Decision: support `list_get` for int lists in `main` only. Check null list/data, unsigned bounds, and the pinned element-kind rule (int or opaque); call the official index/kind error routines on failure.
- Alternatives: unchecked pointer arithmetic (rejected: changes runtime failure behavior); duplicate the collection runtime (rejected: runtime owns these semantics); support other kinds (rejected: their representation/lowering is not covered).
- Evidence: pinned `c_backend.orb:493-500` maps int reads to `orbit_list_read_int`; `runtime/orbit_api.h:217-224,358-360,422-468,485-490`; `runtime/collections.c:78-100`; fixtures `19-int-list-get` and `20-int-list-get-oob`, full manual differential gate 20/20.

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
  `c_backend.orb:2423` appends an empty string for `nop`;
  `c_backend.orb:2634-2641` renders the blocks as the C braces `{`/`}` (pure C
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
- Lowering (read from `c_backend.orb:3452-3505` and confirmed against the
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
- Lowering, read from `c_backend.orb:3381-3427` and `listWriteCall`
  (`c_backend.orb:518-548`): `list_set` is a direct
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
  before `orbit_list_create_typed` (`c_backend.orb:3385`). R6 mirrors that
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
  `c_backend.orb:4214`). That list is gap **U-04** (spec §3.1.9/§6): its
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
  `arena` prepended by the backend, `c_backend.orb:1629-1633`) with
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
  with no owner (`findFieldOwner`, `c_backend.orb:4400-4409`, a last-wins scan
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
  `/* free - arena managed */` and never frees (`c_backend.orb:3287-3290`);
  the spec's audit note keeps its semantics `UNSPECIFIED` (U-09) and states
  the no-op is NOT semantics, so admitting it would invent behavior.
  `cteval.orb:49,86` only classifies it (impure / side-effecting); nothing
  emits it.
- `alloc` (§1.2 row 106): the observed lowering is
  `D ← orbit_arena_alloc(arena, O1)`, with `sizeof(void*)` when O1 is absent
  (`c_backend.orb:3274-3286`), but the destination is a raw pointer with no
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