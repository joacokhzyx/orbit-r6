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
  lowering pops the values into a comment (`c_backend.orb:2795-2800`; U-17
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
  (`c_backend.orb:562-1030`) restricted to the opcodes of the native subset, as
  a three-pass fixed point like the original. Only the literal text `unknown`
  is replaced: a declared type is never touched, and a register no rule
  explains **stays `unknown` and stays refused** by the module check, with the
  same message and code (`CGEN`).
- Why infer instead of tolerating `unknown`: the language backend declares
  every register as an untyped `uintptr_t` word (`c_backend.orb:3735-3744`) and
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