# Pinned language inputs

| Input | Value | Evidence / status |
|---|---|---|
| Orbit repository | `C:\Users\Usuario\Downloads\orbit-lang` (read-only to R6) | Local checkout inspected; do not write there. |
| Orbit commit | `33abf7ab762607c117d01d06bee8584a2e3781c1` (`main`) | `git rev-parse HEAD` on 2026-10-22, **not pushed** (no remote receives it). Two commits after `17ed624` (D-F0-13): `9268b8f` adds the `d013` doctor diagnostic over an annotated variable nothing reassigns, and `33abf7a` appends the IR document into one buffer instead of copying the prefix. `compiler/emit/c_backend.orb` (`0c9dd786.` -> `2eacd0c5.`), `compiler/tools/doctor.orb`, `docs/DOCTOR.md`, `compiler/selfhost/stage3.exe.c` (`5852fe69.` -> `bf8887c4.`), `scripts/baselines/unknown_count.json` and `scripts/verify_seed.py` moved; `runtime/` and `scripts/emit_ir_corpus.py` did not. The 98 corpus documents are byte-identical, so the pin is corpus-neutral and the compiler's own IR becomes emittable (D-M1-34). Moved from `17ed624` by D-F0-14. |
| Orbit IR version | IRv1, published by `main` at `33abf7a` | `compiler/emit/ir_emit.orb` changed blob (`54f0601b.` -> `a26ace9.`): the serializer appends into one `orbit_cbuf` instead of rebuilding `out = out + piece` at every line. Its output is unchanged: the corpus regenerates to 98 documents and `filesSha256 e8586b9a.` file by file. `c_backend.orb` moved for an unrelated doctor change (`2eacd0c5.`, two in-place lines); `scripts/emit_ir_corpus.py` is `d3da729c.` since D-F0-13. |
| IR emitter (in-repo) | `C:\Users\Usuario\Downloads\orbit-lang`, branch `main`, commit `33abf7ab762607c117d01d06bee8584a2e3781c1` | `compiler/emit/ir_emit.orb` at this pin is the single-buffer emitter; `scripts/emit_ir_corpus.py` lists 74 suite + 23 std programs and the corpus re-emitted from the commit keeps `filesSha256 e8586b9a.` with all 98 documents identical to D-F0-13. Emission from two independently materialized trees (a `git archive` extraction and a `git clone`, both at the commit) produced the same `filesSha256`, so the bytes are a function of the commit, not of the tree. **Local only: not pushed to any remote** (D-F0-10/D-F0-11/D-F0-12/D-F0-13/D-F0-14). |
| IR emitter binary (oracle) | `C:\Temp\orbit-fp-8f487a3e9f.exe` | SHA-256 `82826f899541f8b81fc47ac395b08b02ec1137fa4aca012bd2ba851757a570a9`; 2571776 bytes; `orbit 0.1.0-rc.2`; fixed-point build of the compiler sources at `33abf7a` (`python scripts/dev.py fp` in `C:\Temp\orbit-pin-33abf7a`; converged and matches the canonical `138FBDF4.`; `verify_seed.py` 4/4). The name is `orbit-fp-` + the first 10 hex of `sha256(checkout path)`, so a rebuild in place overwrites it. **The bytes are build-site dependent**: the binary embeds the absolute source path, so this hash identifies *this* build, not the pin; the reproducible anchors are the commit and the canonical. Oracle of the differential, the corpus and the census. |
| Emitted IR corpus (R6 pin) | `tests/corpus/manifest.json` with `baseline.json` and `run_corpus.py` | 97 executable + 1 support = 98 documents, 5932267 bytes, `filesSha256 e8586b9af976d18c3fdb1d4071a5431dc60da49223a6a09e9e38a02a9b96db0e`, per-file SHA-256 in the manifest, `languageCommit 33abf7ab762607c117d01d06bee8584a2e3781c1`; regenerate with `run_corpus.py --orbit-root <tree at the pin>`. The bytes did not move at this pin: the manifest differs from D-F0-13 only in its `emitter` block. Measured, not committed as `.ir` (D-M1-17); re-measured at this pin as **63 build / 63 match over 97 programs** (ratchet `r6BuildOk 63`, `match 63`, D-M1-45). |
| Compiler IR census (R6, D-M1-34) | `tests/compiler-ir/baseline.json` with `run_census.py` | 26 modules of `compiler/**/*.orb` emitted at the pin with `--emit-ir`: 97 699 992 bytes, `irFilesSha256 34e710ca.`; **0 build**. Six first refusals: `union ASTNode` 14, `union IRValue` 4, `extern return type` 3, `no non-extern main` 2, `register type unknown` 2, `model field list` 1. The emission needs the `33abf7a` emitter: 8 of the 26 modules exceed the machine without it. Regenerate with `run_census.py --compiler <fp> --tree <tree at the pin>`; the ratchet is one-way over `r6BuildOk`. |
| Self-host canonical (in-repo) | `compiler/selfhost/stage3.exe.c` at `33abf7a` (blob `bf8887c4.`); `PUBLISHED_C` in `scripts/verify_seed.py` | `138FBDF4DDE2D90C8C9E7543F9E1AF4662A60D52A4ED946E91A99E1DFCD06193` (the file's own sha256 at HEAD); `dev.py fp` converged and reported "matches canonical"; `verify_seed.py` reports 4/4. The canonical moved at this pin (was `6C26A19E.` at `17ed624`): the emitter fix and the doctor diagnostic both change the compiler's own C. |
| Runtime ABI header | `runtime/orbit_api.h` at `17ed624` (blob `523f2913.`) | Seven new declarations, all additive (no existing signature moved): request-boundary OOM recovery (`orbit_request_oom_install`, `orbit_request_oom_arm`, `orbit_request_oom_disarm`, `orbit_request_oom_message`), `orbit_response_or_500`, `orbit_kynx_has_trusted_proxies` and `orbit_kynx_lease_abandon`; it also includes `<setjmp.h>`. Diff against `bf098c54.`: +14 lines, no deletions. `orbit_print` is `void orbit_print(const char* str)`; `orbit_os_isatty_selfhost(orbit_int fd)` unchanged. |
| Runtime umbrella header | `runtime/runtime.h` at `17ed624` (blob unchanged since `862f257`) | Committed blob `ffbd6a94ca4c52d0cee9f768f2fcb7bb0c1f1622`. Loses its `#include "oracle.c"` when that file is deleted (`runtime/oracle.c` + `runtime/oracle.h`, −218 lines). The R6 runtime archive compiles through this header, so the row is pinned from D-F0-11 on. |
| Runtime build recipe | `scripts/mklib_runtime.py` at `17ed624` (blob unchanged since `299eb09`) | Committed blob `86858c142759ba539e10e6880467f9372e7b617c`. Tracked by the pin; it builds one unity runtime TU with `ORBIT_WITH_NET`, `ORBIT_LIB_NO_ROUTER`, `ORBIT_PERF_IMPL`; `--out` supports an R6-local artifact path (the script itself writes only there plus its own temp dir, so `--out` keeps the checkout untouched). `--cc C:\w64devkit\bin\gcc.exe`. |
| Runtime library, Windows x64 | Rebuilt from the official pinned recipe/source at `17ed624` to `.r6-build/runtime-win64/libruntime_net.a` (local ignored build output; not committed) | One archive member `orbit_lib.o`; `objdump` reports PE x86-64; `nm --defined-only` reports **337 defined symbols** (324 at `ce87d508`; the difference is the request-OOM API and its statics); `orbit_print`, `orbit_alloc`, `orbit_result_ok`, `orbit_string_concat` present, `orbit_handle_request` absent as required. Build used GCC 16.2.0 `x86_64-w64-mingw32` via `--cc` and an **absolute** `--out` (the script resolves a relative `--out` against the pinned tree, so a relative path fails at `ar`). **Neither the archive nor the member object is byte-reproducible** (D-F0-11). Observation of this build: sha256 `58ff94a2381b33cb2dc2e8aa5deb0889b301b611bd742f12efc49f5152bbf97e`, 140814 bytes. |
| Runtime source (implementation) | `runtime/arena.c` at `17ed624` | Committed blob `59f12e24fd04a6e67064f17c5ea8a3ec9f3e57cd`; the D-F0-13 tramo adds request-boundary OOM recovery (a `jmp_buf` target armed around one request, `orbit_request_oom_message`) and keeps the overflow-segment allocation. The 64 MB `ORBIT_ARENA_DEFAULT_RESERVE` is unchanged. |
| Normative backend reference | `compiler/emit/c_backend.orb` at `33abf7a` | Committed blob `2eacd0c5` (was `0c9dd786.` at `17ed624`). The delta is two declarations in `verifyModuleTypes` (`var errs: list` -> `val errs: list` and the same for `warns`, `:1391-1392`), with no line added or removed: the 130 citations re-anchored at D-F0-13 keep their line numbers, and only the blob prefix changes. The construct set is otherwise the one D-F0-13 described: no file-scope arena global, every emitted function takes `OrbitArena* arena`, and the router has the 500 fallback. |
| IR emitter source | `compiler/emit/ir_emit.orb` at `33abf7a` | Committed blob `a26ace90` (was `54f0601b.` at `17ed624`). Pinned so an emitted-byte claim can be re-derived without guessing which revision produced it; this is the revision that emits `host/main.orb` in 2.62 s and 1 000 MB of peak. |
| Corpus emitter script | `scripts/emit_ir_corpus.py` at `17ed624` | Committed blob `d3da729cff8c53a8b6f889956d3eaa5080aaa1a5` (was `057887fd.` at `ce87d508`); documents 74 suite programs plus `imports.support.orb` and 23 std programs. Pinned so the 98-document corpus can be regenerated from the commit. |
| Reproducibility of build artifacts | Measured for the emitter binary and for the runtime archive/member | Both embed the absolute path of the directory they were built in, so no SHA-256 of a local binary or `.a` is a stable anchor. What is pinned: the Orbit commit, the canonical C (`PUBLISHED_C`), the recipe and source blobs, the corpus `filesSha256`, and the symbol set. Any hash of a local artifact in this file is an *observation* of one build. |
| Existing local runtime archive | `.orbit/lib/libruntime_net.a` in the Orbit checkout; untracked | Observed SHA-256 `2ffe2301b75ea86660547de8c2fe4231f740c149be4c43aa93db97877cba22dd` at `862f257` — a different build of the same commit (see the reproducibility row). Untracked and rebuilt by the language's own builds, so this is a historical observation of one build, not a pin. Not used by R6. |
| Runtime library, Linux x64 | Not yet built/verified | No Linux distribution is installed in WSL and no native Linux x64 compiler is available. Must build with the pinned script/source on a Linux x64 toolchain before enabling that platform. |
| Available GCC | GCC 16.2.0, target `x86_64-w64-mingw32` | Local Windows x64 MinGW toolchain; use `-lws2_32` at final Windows link as the pinned header documents. |
| Candidate M2 source corpus | 97 executable cases: 74 suite programs + 23 std programs; suite also has 1 `.support.orb` helper excluded by `scripts/test_suite.py:59` | Pinned checkout inventory. No direct `db(...)`, migration, `orbit_db_*` or auth calls found in these source files; thus 0 currently identified DB cases in this specific source inventory. This is not a measured IR/differential result. |

Changing the Orbit pin requires a dated decision in `DECISIONS.md`, with new
header/library hashes and corpus inventory. M2 may not silently include tests
from another commit. Build artifact hashes depend on compiler/archive toolchain
**and on the build directory**; source provenance is established by the pinned
recipe, the runtime source blobs and the symbol set, not by archive byte
equality (D-F0-11).

The emitter is an **in-repo input** (D-F0-10): the language commit pin is
`main`, so the emitter commit resolves from the user's own checkout and the
separate clone is no longer part of the pin. Nothing was pushed: provenance is
local, which does not change what R6 verifies. The earlier local-only pin
(D-F0-09: `5c01cea` + the clone) and the merge pin (D-F0-10: `299eb09`) are
superseded by D-F0-11 (`862f257`), which follows the H-ARENA landing, the
Oracle removal, the loader fix and the `compiler/` reorg — all verified
IR-neutral. D-F0-12 moves the pin to `ce87d508`: a runtime + harness tramo that
leaves `compiler/` untouched, so the fixed-point oracle is byte-identical, the
canonical is unchanged and a full corpus regeneration still gives
`filesSha256 0106d531…`; the runtime archive was rebuilt and its 324-symbol set
is identical, and the dirty working tree observed while taking the pin is
recorded there and is **not** part of it.

**Observation 2026-10-18 (no pin change).** While measuring the round-4 result
the live checkout `C:\Users\Usuario\Downloads\orbit-lang` had **advanced past
the pin** (HEAD `adb8b96`, `ce87d508` still an ancestor) and was dirty
(`M compiler/ir/builder.orb`, `M compiler/selfhost/stage3.exe.c`,
`M docs/IR_SPEC.md`, `M scripts/emit_ir_corpus.py`, `M scripts/verify_seed.py`,
`?? tests/suite/union_bare_variant_pattern.orb`); its `emit_ir_corpus.py` now
lists 96 documents, so `run_corpus.py --orbit-root <that tree>` reports corpus
drift. The fixed-point binary at `C:\Temp\orbit-fp-26cfb87400.exe` was
**rebuilt in place** by the concurrent session (the name is
`sha256(checkout path)`, so it is reused): it is now 2551296 bytes, SHA-256
`09526eff.`, recording checkout `adb8b96`, where the pin recorded 2537984 bytes
and `10a16956.`. Neither change moves the pin. Reproduced from the pinned
commit alone: `git -C <checkout> archive ce87d508` extracted into a scratch
tree, then `emit_ir_corpus.py --compiler <fp> --out <dir>` over that tree
yields exactly 94 documents and `filesSha256 0106d531.` with the current
binary - so the emitted bytes are still a function of the pinned sources, and
the corpus measurement must pass `--orbit-root` a tree materialized at
`ce87d508` (not the advancing working tree). The pin rests on the commit.

The fixed point the 2026-10-18 gates were actually measured with is the
pinned-era build `C:\Temp\orbit-fp-8b0bb38a82.exe` (2537984 bytes, SHA-256
`3a000d74.`), passed to `--compiler`; the pinned sources it was built from were
materialized read-only at `C:\Temp\orbit-pin-ce87d508` and passed to
`--orbit-root`. Both are `C:\Temp` artifacts of the pin, not new pins.

**Observation 2026-10-18, later (no pin change).** The concurrent session
committed its work and the live checkout's working tree went **clean** at
`82cde1a5`, 28 commits past the pin (`ce87d508` still an ancestor). That is the
same lesson seen from its other side: the sibling tree is neither the pin nor a
stable reference, so pinned bytes come from `git cat-file blob 17ed624:<path>`
and corpus inputs from a tree materialized at the pin, whatever state the live
checkout is in.

**D-F0-13 (2026-10-21).** Moves the pin from `ce87d508` to `17ed624`, 70 commits of compiler, runtime, scripts, std, tests and docs. The move is not IR-neutral: `compiler/emit/c_backend.orb` changed blob (`2d64c8de.` -> `0c9dd786.`), `scripts/emit_ir_corpus.py` lists four more programs, and the corpus regenerates to 98 documents with `filesSha256 e8586b9a.` (was `0106d531.`). The fixed point rebuilt from the commit converges to `PUBLISHED_C 6C26A19E.` (was `9906A6F7.`) and the runtime archive rebuilt from the same sources defines 337 symbols (was 324). The differential lost one fixture: the language now rejects using an extern function as a value, so `26-fn-values.orb` cannot be built by the oracle; the pair is retired to `.skip` and its IR keeps a pinned acceptance control (`ok-fn-extern`, exit 98). The gates print diff 42/42, loader 193/193, corpus 47 build / 46 match over 97 programs, and `migrate-m1-fixtures.py --check` 20/20.

**D-F0-14 (2026-10-22).** Moves the pin from `17ed624` to `33abf7a`, two
commits: `9268b8f` adds the `d013` doctor diagnostic over an annotated variable
nothing reassigns, and `33abf7a` replaces the quadratic accumulation in
`emitIRv1` with one append buffer. The move is corpus-neutral: all 98 documents
keep `filesSha256 e8586b9a.`; the manifest differs only in its `emitter` block.
It is not neutral for the compiler's own IR, which is the point: with the fix
8 of the 26 modules stop exceeding the machine, and the census exists at all.
The fixed point rebuilt from the commit converges to `PUBLISHED_C 138FBDF4.`
(was `6C26A19E.`) and the corpus stays at 47 build / 46 match. `runtime/` did
not move, so the 337-symbol archive of D-F0-13 is unchanged.
