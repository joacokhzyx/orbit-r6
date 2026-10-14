# Decision log

All entries are R6 proposals/decisions scoped to the commit in [PIN.md](PIN.md).
They do not alter Orbit. Dates use the user's timezone (America/Buenos_Aires).

## D-F0-01 — Pin language inputs

- Date: 2026-10-14.
- Decision: pin `orbit-lang` at `5c01cea68f125f06dd3ac82a61776827bdb0c6f3`; never follow its branch automatically.
- Alternatives: track `main` (rejected: moving IR/ABI invalidates differential evidence).
- Evidence: `git rev-parse HEAD`; hashes and local toolchain are recorded in [PIN.md](PIN.md).

## D-F0-02 — IRv1 proposal is not yet accepted

- Date: 2026-10-14.
- Decision: keep R6's supported IR range `none` until a human accepts `IR_SPEC_DRAFT.md` or Orbit publishes an equivalent versioned format. Then support only an explicit range and reject versions outside it.
- Alternatives: infer a private format and begin coding (rejected: no `--emit-ir`/spec/corpus at the pin).
- Evidence: search of pinned checkout; enum and structure at `compiler/ir.orb:3-105` are internal and have no serialization/version field.
- Status: **awaiting human review**.

## D-F0-03 — DB excluded until a published compatible runtime exists

- Date: 2026-10-14.
- Decision: R6 does not claim DB coverage through M2. DB tests are explicitly excluded from R6's differential corpus until M3 unless Orbit publishes and pins a DB-capable runtime variant with matching ABI. Do not fabricate a local variant or shim. The exclusion manifest must enumerate affected tests and count them; that count is pending a reproducible IR corpus.
- Alternatives: link `libruntime_net.a` and silently omit DB (rejected: header says net variant has no DB symbols); build or shim DB locally (rejected: violates runtime ownership).
- Evidence: `runtime/orbit_api.h:106` rejects `ORBIT_WITH_DB` for the net-only library; C backend lowers `db_*` at `c_backend.orb:3533-3590`. The pinned candidate M2 corpus has 89 executable programs (67 suite, 22 std) plus one suite support helper; no source-detectable DB/auth cases were found. This does not replace post-IR differential accounting.
- Status: policy recorded; needs human confirmation in the F0 review package.

## D-F0-04 — Target matrix

- Date: 2026-10-14.
- Decision: Windows x86-64 first, Linux x86-64 second; macOS ARM64 out of scope until M3. No multithreaded claims before M3. No performance measurements before M4.
- Alternatives: promise all platforms immediately (rejected: target runtime/toolchain ABI not verified).
- Evidence: local GCC target is `x86_64-w64-mingw32`; Linux toolchain availability not verified; macOS ARM64 explicitly deferred by task contract.
- Status: matrix proposal awaits F0 review; platform code does not exist.

## D-F0-05 — Unknown semantics fail closed

- Date: 2026-10-14.
- Decision: every opcode/platform/version not explicitly supported must produce a diagnostic and nonzero exit; no stub, invented result, or fallback. Single-thread only through M2.
- Alternatives: copy the C backend's default `Unknown opcode` comment (rejected: it silently continues).
- Evidence: backend fallback at `compiler/c_backend.orb:3596-3598`; R6 contract requires explicit errors.
- Status: R6 policy, to be applied after IR approval.
