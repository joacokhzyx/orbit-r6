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
  (`compiler/emit/c_backend.orb:1174-1196`) maps `IRValue.Int` -> `"int"`,
  `Float` -> `"float"`, `Bool` -> `"bool"`, `String` -> `"string"` (the mirror
  R6's `value_type` already implemented for D-M1-20); `isStringOperand`
  (`:3881`) treats an inline `IRValue.String` as a string operand;
  `verifyarithBinop` (`:1321`) and `verifycmpBinop` (`:1354`) type both operands
  through it; `add`'s inference (`:802`) and its emit (`:1049`) test
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
  `compiler/emit/c_backend.orb 2d64c8de…` at `ce87d508`:
  - the emission of `load_var` (`:2598-2651`) takes the name text and, when the
    variable's own type is unknown or empty, **falls back to `func.params` and
    takes `paramTypes[p]`** (`:2623-2631`); the cast that follows is chosen from
    that type (`:2633-2649`), and a `Sym` renders as the name verbatim
    (`generateValue`, `:IRValue.Sym -> v.value`). So a `load_var` of a parameter
    is its own C variable, cast by `paramTypes[p]`.
  - the type inference (`:775-800`) consults `func.params` **before** the
    locals and infers `params.get(p) == vn -> paramTypes.get(p)`; that is the
    pass that decides the register type, and the order R6 mirrors.
  - the `arg` binding (`:2860-2873`) emits `reg[dest] = params[index]`, i.e. the
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
- Reference (pinned `ce87d508`, read with `git cat-file blob`, never from the
  working tree):
  - `compiler/emit/c_backend.orb:2662-2668` - the `IROpcode.add` arm: if either
    operand is a string operand, both sides go through
    `stringifyConcatOperand` and the result is
    `orbit_string_concat(arena, s1, s2)`.
  - `:4341-4378` - `stringifyConcatOperand`: a string operand is used raw, any
    other operand is spread by its inferred type.
  - `:3881-3908` - `isStringOperand` decides "is a string" from
    `registerTypes`/`strRegs`/`strVars`, which is the type R6's own `type_at`
    already carries.
  - `:387` - the generated header declares `extern OrbitArena* arena;` (a
    file-scope global); `:193` and `:2219` give the prototype/definition of
    `orbit_main(OrbitArena* _init_arena, ...)` and `:2242` writes
    `arena = _init_arena;` in the prologue. So `main` receives the arena as its
    first ABI parameter and every other function reads the global; R6 mirrors
    the global in `r6_arena` (the D-F0-12 runtime archive) and reads `main`'s
    parameter slot.
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
- Reference (pinned `ce87d508`, read with `git cat-file blob`):
  - `compiler/emit/c_backend.orb:3911` - `feBuildTimeCallType` types `print`
    `void`.
  - `:2971-2999` - the lowering dispatches on `calleeName == "print"` **before**
    resolving any declaration: one argument is a string through
    `print("%s", arg)`, a float goes through
    `orbit_float_to_string(arena, v)` and `printf("%s\n", ...)`, and any other
    argument is `printf("%lld\n", (long long)(arg))`.
  - `:2993-2998` - any other arity pops **every** pending argument and emits
    only an empty statement.
  - `:3881-3908` and `isFloatArgReg` - the class that selects the reduction is
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
- Root cause: the pinned C backend prepends `arena` to every call whose name
  `functionNeedsArena` lists (`c_backend.orb:3091-3098`; predicate at
  `:4410-4431`). R6 emitted the IR arguments from slot 0, so the callee read the
  integer as the arena pointer and the next register as the value.
- Admission: a call to a **declared extern** whose name is in that enumerated
  family loads the executable arena into RCX and shifts every IR argument one
  position. The shift is positional for both register files: Win64 assigns
  argument *i* to the *i*-th integer and *i*-th SSE register, which
  `orbit_float_to_string(arena, v)` pins (the value rides XMM1).
- Reference: `c_backend.orb:4410-4431` (`functionNeedsArena`, copied verbatim),
  `:3091-3098` (the prepend); `runtime/orbit_api.h:388-389` (the two
  conversions) and `runtime/collections.c:716`/`:730` (their definitions take
  the arena). The arena global is the one D-M1-23 already mirrors
  (`c_backend.orb:387`), so a call outside `main` also sets
  `module_needs_arena_global`.
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
- Reference (pinned `ce87d508`, read with `git cat-file blob`):
  - `compiler/emit/c_backend.orb:3429-3439` - the register form of `ret`: when
    the current function's return type is `void` the lowering emits
    `return;` and never reads the register. Its own comment records why: the
    call was already emitted as a statement, so the register names no live
    value and `return (void*)(r_0);` does not even compile.
  - `:3460-3465` - the non-register form shares the same branch and the same
    `return;`.
  - `:3420`, `:3451` - `main` is decided before the declared type, but R6
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
- Reference (pinned `ce87d508`, read with `git cat-file blob`):
  - `runtime/orbit_api.h:385` - `orbit_int orbit_string_at(orbit_string,
    orbit_int)`.
  - `:386` - `orbit_string orbit_string_slice(OrbitArena*, orbit_string,
    orbit_int, orbit_int)`; `orbit_string_slice` is in `functionNeedsArena`
    (`c_backend.orb:4410-4431`), so D-M1-27's table prepends the arena.
  - `:411-413` - `ORBIT_INLINE orbit_int orbit_string_len(orbit_string s) {
    return s ? (orbit_int)strlen(s) : 0; }`. It is a header inline, not an
    exported symbol: `nm libruntime_net.a` lists `orbit_string_at` and
    `orbit_string_slice` as `T` and has no `orbit_string_len` at all, so the
    length cannot be a plain call.
  - `c_backend.orb:3609-3615` - the `list_len` opcode arm asks
    `isStringOperand` first and emits `orbit_string_len((orbit_string)(...))`,
    falling back to `orbit_list_len` only otherwise. Same helper, second door.
  - `:3187-3188` - the call form's argument cast for the same family.
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
- Reference (pinned `ce87d508`, `git cat-file blob`):
  - `compiler/emit/c_backend.orb:3580-3585` - the `list_create` arm forwards
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
- Reference (pinned `ce87d508`, `git cat-file blob`):
  - `compiler/emit/c_backend.orb:2745-2758` (`eq`) and `:2759-2772` (`ne`):
    two strings compare with `strcmp`; either side float promotes both
    operands to `orbit_float`; otherwise the two values compare as
    `uintptr_t`. A bool has no arm of its own - it is one of the words - and
    `n == "5"` reaches the `uintptr_t` arm, which compares 5 with the
    literal's address and answers false.
  - `compiler/emit/c_backend.orb:2781-2812` (`lt`, `le`, `gt`, `ge`): no
    string arm; a float pair compares in double; otherwise `intptr_t`. The
    comment at `:2773-2780` states why the relational ops are signed.
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
- Reference (pinned `ce87d508`, `git cat-file blob`):
  - `compiler/emit/c_backend.orb:476` answers `OrbitList*` for `list`, `List`
    and `OrbitList*`.
  - `compiler/emit/c_backend.orb:477-480` says why: "A list described together
    with its element type is still a list. The suffix names what is IN it,
    which is a property of the values, not of the pointer, so the C type does
    not change."
  - `compiler/emit/c_backend.orb:481-497` applies that rule to any
    parameterised spelling at any depth, and takes the base to decide:
    `list`/`List` -> `OrbitList*`, `map`/`Map` -> `OrbitMap*`. The comment names
    `map<string, list<int>>` itself.
  - `compiler/emit/c_backend.orb:604-609` reads each element kind through its
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
- Reference (pinned `ce87d508`, `git cat-file blob`):
  - `compiler/emit/c_backend.orb:396-399` and `:1633` write
    `#define <name> <text>` into the preamble for every module global.
  - `compiler/emit/c_backend.orb:1647-1651` (`cDefineLiteral`) returns the
    value's `String` payload unchanged. `:1639-1646` says the builder stores the
    literal in an `IRValue.String` so a register cannot land in a define.
  - `compiler/emit/c_backend.orb:2404-2413` says a top-level name is that
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
- Evidence (pin `filesSha256 0106d531`): match **44 -> 45/93** and build
  **45 -> 46/93**. `global_const` now matches. `ast_function_body_is_a_block`
  advances to `union ASTNode variant Program payloads are outside the M1 native
  subset`, `test_fs` to `extern return type is unsupported in M1` and
  `test_imports` to `register type unknown is unsupported in M1`. First
  refusals: module-level 35 -> 34; `register type unknown` 12 -> 13; `extern
  return type` 2 -> 3; the four `global ... is outside M1 literal int/bool
  support` messages disappear. Ratchet re-taken at 46/45.
- Status: **applied**.
