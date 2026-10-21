# Pinned language inputs

| Input | Value | Evidence / status |
|---|---|---|
| Orbit repository | `C:\Users\Usuario\Downloads\orbit-lang` (read-only to R6) | Local checkout inspected; do not write there. |
| Orbit commit | `17ed6243844eb043f08cf9163cb60ea9cb8c6a63` (`main`) | `git rev-parse HEAD` on 2026-10-21, **not pushed** (no remote receives it). Tramo of `ce87d508` (D-F0-12): 70 commits. `compiler/` moved: `compiler/emit/c_backend.orb` (`2d64c8de.` -> `0c9dd786.`), `compiler/tir/lower.orb`, `compiler/tools/doctor.orb` and the regenerated `compiler/selfhost/stage3.exe.c` (`5852fe69.`); the canonical moved to `6C26A19E.`. `runtime/` moved: `arena.c` (request-boundary OOM recovery), `file.c`, `http.c`, `kynx.c` and `orbit_api.h` (+6 request-OOM symbols). The corpus grew from 93+1 to 97+1 documents (suite 71 -> 74, std 22 -> 23). Moved from `ce87d508` by D-F0-13. |
| Orbit IR version | IRv1, published by `main` at `17ed624` | `compiler/emit/ir_emit.orb` is byte-identical to `ce87d508` (blob `54f0601b.`), so the serializer did not move. The C backend (`c_backend.orb`, `0c9dd786.`) and the corpus emitter (`scripts/emit_ir_corpus.py`, `d3da729c.`) did change; the corpus was regenerated and its bytes differ from D-F0-12: 97+1 documents, `filesSha256 e8586b9a.` (was `0106d531.`). |
| IR emitter (in-repo) | `C:\Users\Usuario\Downloads\orbit-lang`, branch `main`, commit `17ed6243844eb043f08cf9163cb60ea9cb8c6a63` | The emitter commits `ed17dac`, `a610905` and `1e6a378` are ancestors of `main`, so the emitter resolves from the user's own repository. At this pin `compiler/emit/ir_emit.orb` is unchanged from `862f257` and `scripts/emit_ir_corpus.py` lists 74 suite + 23 std programs. Emission from two independently materialized trees (a `git archive` extraction and a `git clone`, both at the commit) produced the same `filesSha256 e8586b9a.`, so the bytes are a function of the commit, not of the tree. **Local only: not pushed to any remote** (D-F0-10/D-F0-11/D-F0-12/D-F0-13). |
| IR emitter binary (oracle) | `C:\Temp\orbit-fp-08a471904d.exe` | SHA-256 `7e1f9c307f96415fb6ee9198b241eb0b5bcccabc6634fbe9571cf9bd39dcc896`; 2573312 bytes; `orbit 0.1.0-rc.2`; fixed-point build of the compiler sources at `17ed624` (`python scripts/dev.py fp` in a materialized tree; converged and matches the canonical `6C26A19E.`). The name is `orbit-fp-` + the first 10 hex of `sha256(checkout path)`, so a rebuild in place overwrites it. **The bytes are build-site dependent**: the binary embeds the absolute source path, so this hash identifies *this* build, not the pin; the reproducible anchors are the commit and the canonical. Oracle of the differential and of the corpus. |
| Emitted IR corpus (R6 pin) | `tests/corpus/manifest.json` with `baseline.json` and `run_corpus.py` | 97 executable + 1 support = 98 documents, 5932267 bytes, `filesSha256 e8586b9af976d18c3fdb1d4071a5431dc60da49223a6a09e9e38a02a9b96db0e`, per-file SHA-256 in the manifest, `languageCommit 17ed6243844eb043f08cf9163cb60ea9cb8c6a63`; regenerate with `run_corpus.py --orbit-root <tree at the pin>`. The `filesSha256` moved at this pin (was `0106d531.` from `299eb09` through `ce87d508`): the suite grew 71 -> 74 and std 22 -> 23. Measured, not committed as `.ir` (D-M1-17); re-measured at this pin as **47 build / 46 match over 97 programs** (ratchet `r6BuildOk 47`, `match 46`), from 46/45 over 93 at D-F0-12. |
| Self-host canonical (in-repo) | `compiler/selfhost/stage3.exe.c` at `17ed624` (blob `5852fe69.`); `PUBLISHED_C` in `scripts/verify_seed.py` | `6C26A19ED12A5D6ABAD240B922FA9C3E1D939DE16D998363CA7CF816FB0FF6DE` (the file's own sha256 at HEAD); `dev.py fp` converged and reported "matches canonical". The canonical moved at this pin (was `9906A6F7.` at `ce87d508`): the compiler sources changed (`c_backend.orb`, `tir/lower.orb`, `tools/doctor.orb`). |
| Runtime ABI header | `runtime/orbit_api.h` at `17ed624` (blob `523f2913.`) | Seven new declarations, all additive (no existing signature moved): request-boundary OOM recovery (`orbit_request_oom_install`, `orbit_request_oom_arm`, `orbit_request_oom_disarm`, `orbit_request_oom_message`), `orbit_response_or_500`, `orbit_kynx_has_trusted_proxies` and `orbit_kynx_lease_abandon`; it also includes `<setjmp.h>`. Diff against `bf098c54.`: +14 lines, no deletions. `orbit_print` is `void orbit_print(const char* str)`; `orbit_os_isatty_selfhost(orbit_int fd)` unchanged. |
| Runtime umbrella header | `runtime/runtime.h` at `17ed624` (blob unchanged since `862f257`) | Committed blob `ffbd6a94ca4c52d0cee9f768f2fcb7bb0c1f1622`. Loses its `#include "oracle.c"` when that file is deleted (`runtime/oracle.c` + `runtime/oracle.h`, −218 lines). The R6 runtime archive compiles through this header, so the row is pinned from D-F0-11 on. |
| Runtime build recipe | `scripts/mklib_runtime.py` at `17ed624` (blob unchanged since `299eb09`) | Committed blob `86858c142759ba539e10e6880467f9372e7b617c`. Tracked by the pin; it builds one unity runtime TU with `ORBIT_WITH_NET`, `ORBIT_LIB_NO_ROUTER`, `ORBIT_PERF_IMPL`; `--out` supports an R6-local artifact path (the script itself writes only there plus its own temp dir, so `--out` keeps the checkout untouched). `--cc C:\w64devkit\bin\gcc.exe`. |
| Runtime library, Windows x64 | Rebuilt from the official pinned recipe/source at `17ed624` to `.r6-build/runtime-win64/libruntime_net.a` (local ignored build output; not committed) | One archive member `orbit_lib.o`; `objdump` reports PE x86-64; `nm --defined-only` reports **337 defined symbols** (324 at `ce87d508`; the difference is the request-OOM API and its statics); `orbit_print`, `orbit_alloc`, `orbit_result_ok`, `orbit_string_concat` present, `orbit_handle_request` absent as required. Build used GCC 16.2.0 `x86_64-w64-mingw32` via `--cc` and an **absolute** `--out` (the script resolves a relative `--out` against the pinned tree, so a relative path fails at `ar`). **Neither the archive nor the member object is byte-reproducible** (D-F0-11). Observation of this build: sha256 `58ff94a2381b33cb2dc2e8aa5deb0889b301b611bd742f12efc49f5152bbf97e`, 140814 bytes. |
| Runtime source (implementation) | `runtime/arena.c` at `17ed624` | Committed blob `59f12e24fd04a6e67064f17c5ea8a3ec9f3e57cd`; the D-F0-13 tramo adds request-boundary OOM recovery (a `jmp_buf` target armed around one request, `orbit_request_oom_message`) and keeps the overflow-segment allocation. The 64 MB `ORBIT_ARENA_DEFAULT_RESERVE` is unchanged. |
| Normative backend reference | `compiler/emit/c_backend.orb` at `17ed624` | Committed blob `0c9dd786c0286d24009efca5d79494b5161c538b` (was `2d64c8de.` at `ce87d508`). **The construct set changed**: the global `extern OrbitArena* arena;` was removed (every emitted function takes `OrbitArena* arena`), the arena predicate became `calleeTakesArena`, and the router gained a 500 fallback. The line-number citations that decisions D-M1-19 to D-M1-33 wrote against `2d64c8de.` are therefore auditable at `ce87d508` only; a re-anchoring pass against `0c9dd786.` is tracked as separate work (130 citations measured: 65 shifted, 5 rewritten, 1 gone). |
| IR emitter source | `compiler/emit/ir_emit.orb` at `17ed624` | Committed blob `54f0601bc682045c1604d72c75414a5e62195b1b`, unchanged since `862f257`. Pinned so an emitted-byte claim can be re-derived without guessing which revision produced it. |
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
