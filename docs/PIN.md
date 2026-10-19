# Pinned language inputs

| Input | Value | Evidence / status |
|---|---|---|
| Orbit repository | `C:\Users\Usuario\Downloads\orbit-lang` (read-only to R6) | Local checkout inspected; do not write there. |
| Orbit commit | `ce87d508e8bdb7a1a0059726d3f45ecc73f8f5e3` (`main`) | `git rev-parse HEAD` on 2026-10-17, **not pushed** (no remote receives it). Tramo of the previous pin (`862f257`, D-F0-11): `8878225` (runtime: the hot retention follows the request's footprint, not a fixed 256 KB) → `5cfc12b`/`8f7acbf` (perf docs) → `5ea4366`/`ce87d50` (C-cache harness). **`compiler/` is untouched by the tramo** (measured: `git diff --name-only 862f257..ce87d508 -- compiler/` is empty), so the fixed-point binary, the canonical and the emitted corpus are all byte-identical to D-F0-11. The working tree is **dirty** at this pin (`M scripts/_common.py`, `M scripts/alive_check.py`, `?? scripts/cc_cached.py` — a concurrent session's harness work, class (ii)); the pin rests on the commit, not on the working tree. Moved from `862f257` by D-F0-12. |
| Orbit IR version | IRv1, published by `main` at `ce87d508` | The emitter sources are unchanged from `862f257` (no `compiler/` path moved in the tramo), so this is the same IRv1 D-F0-11 pinned. `orbit build --emit-ir` is part of the pinned commit (merged additively; D-F0-08, D-F0-10/D-F0-11/D-F0-12). |
| IR emitter (in-repo) | `C:\Users\Usuario\Downloads\orbit-lang`, branch `main`, commit `ce87d508e8bdb7a1a0059726d3f45ecc73f8f5e3` | The emitter commits `ed17dac` (new `compiler/ir_emit.orb` + `--emit-ir`), `a610905` (`scripts/emit_ir_corpus.py`) and `1e6a378` (self-test) are ancestors of `main`, so the emitter resolves from the user's own repository; the separate clone `orbit-lang-emit-ir` is not part of the pin. `862f257` moved `compiler/ir_emit.orb` to `compiler/emit/ir_emit.orb` and rewrote its imports, but **the emitted bytes did not change** (measured: R6 regenerated the corpus at `299eb09`, `8b9107a`, `862f257` and `ce87d508`, and all four give `filesSha256 0106d531…`). `orbit build --emit-ir` is unchanged and `scripts/emit_ir_corpus.py` was not touched by the reorg. **Local only: not pushed to any remote** (D-F0-10/D-F0-11/D-F0-12). |
| IR emitter binary (oracle) | `C:\Temp\orbit-fp-26cfb87400.exe` | SHA-256 `10a16956709c68c13e949015187634fa48dec2f60b6fdbc4cf73738be43e5379`; 2537984 bytes; `orbit 0.1.0-rc.2`; fixed-point build of compiler sources that are **identical** at `862f257` and `ce87d508` (no `compiler/` path moved), so this is the byte-identical oracle of D-F0-11; it converges to the canonical `9906A6F7…`. The name is `orbit-fp-` + the first 10 hex of `sha256(checkout path)`, so a rebuild in place overwrites it. **The bytes are build-site dependent**: the binary embeds the absolute source path (measured), so this hash identifies *this* build, not the pin; the reproducible anchors are the commit and the canonical. The `languageCommit` the corpus report prints is the *checkout's* HEAD at emission time, which is why the D-F0-12 run prints `ce87d508` with this same binary. Oracle of the differential and of the corpus. |
| Emitted IR corpus (R6 pin) | `tests/corpus/manifest.json` with `baseline.json` and `run_corpus.py` | 93 executable + 1 support = 94 documents, 5419454 bytes, `filesSha256 0106d531cc10f2edb02097db2babdef4291f845a03be85dcfee47c3d91b5bfb9`, per-file SHA-256 in the manifest, `languageCommit ce87d508e8bdb7a1a0059726d3f45ecc73f8f5e3`; regenerate with `run_corpus.py --orbit-root C:\Users\Usuario\Downloads\orbit-lang`. The `filesSha256` is **unchanged** from `299eb09` through `862f257` to `ce87d508`: H-ARENA, the Oracle removal, the loader fix and the `compiler/` reorg (all IR-neutral, measured), and the D-F0-12 tramo too (runtime + harness only). Measured, not committed as `.ir` (D-M1-17); re-measured at the D-F0-11 re-pin as **7/93** (ratchet `r6BuildOk 7`, `match 7`), lifted to **21/93** by D-M1-21 on 2026-10-17 (ratchet `r6BuildOk 21`, `match 21`), and **held at 21/93** across the D-F0-12 re-pin — same `filesSha256` all three times. |
| Self-host canonical (in-repo) | `compiler/selfhost/stage3.exe.c` at `ce87d508` (blob `322337e2…`); `PUBLISHED_C` in `scripts/verify_seed.py` | `9906A6F7A518C8D19F0C384BB31C97CE6601532A7C64463723A7C6C6B88567E3` (the file's own sha256 at HEAD, re-verified); `dev.py all` 21/21. The canonical moved twice inside the earlier tramo (`15341D24…` → `ACB0245D…` at H-ARENA → `9906A6F7…` at the loader fix); the compiler reorg and the D-F0-12 tramo did **not** move it. Independently reproduced by R6: building `862f257` in a fresh worktree converges to `9906A6F7…`. |
| Runtime ABI header | `runtime/orbit_api.h` at `ce87d508` (blob unchanged since `862f257`) | Committed blob `bf098c5476918c1f925eaac12e4ffc0ce84b9482` (was `27fc52b8…`), equal to working-tree bytes. The change from D-F0-11 is **not** purely additive: `orbit_ledger_exit` gained a `const OrbitArena*` parameter and two arena accessors were added (`orbit_arena_active_bytes`, `orbit_arena_overflowed`). `orbit_print` is `void orbit_print(const char* str)` at line 613; `orbit_os_isatty_selfhost(orbit_int fd)` at line 659. |
| Runtime umbrella header | `runtime/runtime.h` at `ce87d508` (blob unchanged since `862f257`) | Committed blob `ffbd6a94ca4c52d0cee9f768f2fcb7bb0c1f1622`. Loses its `#include "oracle.c"` when that file is deleted (`runtime/oracle.c` + `runtime/oracle.h`, −218 lines). The R6 runtime archive compiles through this header, so the row is pinned from D-F0-11 on. |
| Runtime build recipe | `scripts/mklib_runtime.py` at `ce87d508` (blob unchanged since `299eb09`) | Committed blob `86858c142759ba539e10e6880467f9372e7b617c`. Tracked by the pin; it builds one unity runtime TU with `ORBIT_WITH_NET`, `ORBIT_LIB_NO_ROUTER`, `ORBIT_PERF_IMPL`; `--out` supports an R6-local artifact path (the script itself writes only there plus its own temp dir, so `--out` keeps the checkout untouched). `--cc C:\w64devkit\bin\gcc.exe`. |
| Runtime library, Windows x64 | Rebuilt from the official pinned recipe/source at `ce87d508` to `.r6-build/runtime-win64/libruntime_net.a` (local ignored build output; not committed) | One archive member `orbit_lib.o`; `objdump` reports PE x86-64; `nm` reports **324 defined symbols, an identical set in both directions** against the `862f257` build (verified by full set comparison, not by count); `orbit_print`, `orbit_alloc`, `orbit_result_ok`, `orbit_string_concat` present, `orbit_handle_request` absent as required. Build used GCC 16.2.0 `x86_64-w64-mingw32` via `--cc`. **Neither the archive nor the member object is byte-reproducible** (measured, D-F0-11): the compiled unit embeds the absolute build path (`…\runtime/json.c`) and `ar` adds container metadata, so two rebuilds in the same directory agree on the member while rebuilds in different directories do not. The pin therefore rests on the recipe blob, the runtime source blobs and the symbol set — not on archive bytes. Observation of this build: sha256 `86e63f0dfe2bd2c68d7df3408467e2eba449bb3781042bf4586618b27cbcf0e4`, 138954 bytes (the `862f257` build was `6ebfc05d…`, same size). |
| Runtime source (implementation) | `runtime/arena.c` at `ce87d508` | Committed blob `f9a31e5c0a48d1a276f011ef627f7037d16ce745`; the D-F0-12 tramo's only runtime change (+26/−3): the hot retention follows the request's footprint instead of a fixed 256 KB. The headers and the recipe are unchanged, so the archive's **symbol set is unchanged**; only the member's bytes move (see the reproducibility row). |
| Normative backend reference | `compiler/emit/c_backend.orb` at `ce87d508` | Committed blob `2d64c8de6ef27e7ca175c7fb6a46f13e4b731992` (**unchanged** since `862f257`). The decisions that mirror backend rules cite it by line (D-M1-19 `Sym` classification, D-M1-20 `inferRegisterTypes` `562-1030`, D-M1-21 `irVerifierOperandType` `1174-1196` / `isStringOperand` `3881` / `verifyarithBinop` `1321` / `verifycmpBinop` `1354` / `add` `802`+`1049`), so the blob is pinned from D-F0-12 on: a line-number citation is only auditable against a fixed revision. |
| IR emitter source | `compiler/emit/ir_emit.orb` at `ce87d508` | Committed blob `54f0601bc682045c1604d72c75414a5e62195b1b`. Pinned from D-F0-12 on so an emitted-byte claim can be re-derived without guessing which revision produced it. |
| Corpus emitter script | `scripts/emit_ir_corpus.py` at `ce87d508` | Committed blob `057887fd67d4831739e8a92f062008db09302c2a` (path and file unchanged since the emitter branch; D-F0-10/D-F0-11). Pinned from D-F0-12 on. |
| Reproducibility of build artifacts | Measured for the emitter binary and for the runtime archive/member | Both embed the absolute path of the directory they were built in, so no SHA-256 of a local binary or `.a` is a stable anchor. What is pinned: the Orbit commit, the canonical C (`PUBLISHED_C`), the recipe and source blobs, the corpus `filesSha256`, and the symbol set. Any hash of a local artifact in this file is an *observation* of one build. |
| Existing local runtime archive | `.orbit/lib/libruntime_net.a` in the Orbit checkout; untracked | Observed SHA-256 `2ffe2301b75ea86660547de8c2fe4231f740c149be4c43aa93db97877cba22dd` at `862f257` — a different build of the same commit (see the reproducibility row). Untracked and rebuilt by the language's own builds, so this is a historical observation of one build, not a pin. Not used by R6. |
| Runtime library, Linux x64 | Not yet built/verified | No Linux distribution is installed in WSL and no native Linux x64 compiler is available. Must build with the pinned script/source on a Linux x64 toolchain before enabling that platform. |
| Available GCC | GCC 16.2.0, target `x86_64-w64-mingw32` | Local Windows x64 MinGW toolchain; use `-lws2_32` at final Windows link as the pinned header documents. |
| Candidate M2 source corpus | 93 executable cases: 71 suite programs + 22 std programs; suite also has 1 `.support.orb` helper excluded by `scripts/test_suite.py:59` | Pinned checkout inventory. No direct `db(...)`, migration, `orbit_db_*` or auth calls found in these source files; thus 0 currently identified DB cases in this specific source inventory. This is not a measured IR/differential result. |

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
stable reference, so pinned bytes come from `git cat-file blob ce87d508:<path>`
and corpus inputs from a tree materialized at the pin, whatever state the live
checkout is in.
