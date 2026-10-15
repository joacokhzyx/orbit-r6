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
- Status: **accepted by user 2026-10-15; migration pending, code unchanged**.