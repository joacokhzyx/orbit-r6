# Pinned language inputs

| Input | Value | Evidence / status |
|---|---|---|
| Orbit repository | `C:\Users\Usuario\Downloads\orbit-lang` (read-only to R6) | Local checkout inspected; do not write there. |
| Orbit commit | `ca3eb9103dfcff03f11831eddb8be98bc521b787` (`main`) | On the public remote `https://github.com/joacokhzyx/orbit-lang` (`git push` `a5cdf77..ca3eb91` on 2026-10-27). Four commits after `a5cdf77` (D-F0-15): the `typed-ir-2` merge types `try` unwraps from their annotation, single-payload union data reads from the variant, and list reads whose element is a list or a declared model. `compiler/emit/c_backend.orb`, `compiler/ir/builder.orb`, `compiler/selfhost/stage3.exe.c`, `scripts/verify_seed.py` and `docs/IR_SPEC.md` moved; `runtime/`, `compiler/emit/ir_emit.orb`, `scripts/emit_ir_corpus.py` and `scripts/mklib_runtime.py` did not. Moved from `a5cdf77` by D-F0-16. |
| Orbit IR version | IRv1, published by `main` at `ca3eb91` | Same serializer as D-F0-15 (`ir_emit.orb` blob `a26ace90`). No opcode or field changes; `registerTypes` carries more proven types: 341 `unknown` register destinations over the 133-document corpus, 425 at `a5cdf77` (`scripts/ir_unknown_census.py`). |
| IR emitter (in-repo) | `C:\Users\Usuario\Downloads\orbit-lang`, branch `main`, commit `ca3eb9103dfcff03f11831eddb8be98bc521b787` | `scripts/emit_ir_corpus.py` (blob `e7a4dbe4.`) discovers 109 suite programs, `imports.support.orb` and 23 std programs; the corpus emitted from `C:\Temp\orbit-pin-ca3eb91-git` (a `git clone` at the commit) is 133 documents, `filesSha256 3f20ced0.`. Public (D-F0-16). |
| IR emitter binary (oracle) | `C:\Temp\orbit-fp-33ea94e9ec.exe` | SHA-256 `e24f5c378c2e193cf68658a00fe04556ddf1fa10b71c87b9f44ae223408b46f6`; 864768 bytes; `orbit 0.1.0-rc.2`; fixed-point build of the compiler sources at `ca3eb91` (`python scripts/dev.py fp` in `C:\Temp\orbit-pin-ca3eb91-git`; "converged (matches canonical)"). The name is `orbit-fp-` + the first 10 hex of `sha256(checkout path)`, so a rebuild in place overwrites it. **The bytes are build-site dependent**: this hash identifies *this* build, not the pin; the reproducible anchors are the commit and the canonical. With it the pinned tree passes `tests/suite` 109/109 and `tests/std` 23/23. Oracle of the differential, the corpus and the census. |
| Emitted IR corpus (R6 pin) | `tests/corpus/manifest.json` with `baseline.json` and `run_corpus.py` | 132 executable + 1 support = 133 documents, 6808178 bytes, `filesSha256 3f20ced0bdcd64d5ae96330b347bf17eef11bb8e239882c75f742e7311e3afa0`, per-file SHA-256 in the manifest, `languageCommit ca3eb9103dfcff03f11831eddb8be98bc521b787`; regenerate with `run_corpus.py --orbit-root <tree at the pin>`. Measured, not committed as `.ir` (D-M1-17); measured at this pin as **95 build / 95 match over 132 programs**, none differing (ratchet `r6BuildOk 95`, `match 95`, D-F0-16). |
| Compiler IR census (R6, D-M1-34) | `tests/compiler-ir/baseline.json` with `run_census.py` | 27 modules of `compiler/**/*.orb` emitted at the pin with `--emit-ir`, `irFilesSha256 dfd37c4f.`; **0 build**. First refusals: `union ASTNode` 14, `no non-extern main` 6, `union IRValue` 4, `list_push` 1, `main must have no parameters` 1, `register type unknown` 1. Regenerate with `run_census.py --compiler <fp> --tree <tree at the pin>`; the ratchet is one-way over `r6BuildOk`. |
| Self-host canonical (in-repo) | `compiler/selfhost/stage3.exe.c` at `ca3eb91` (blob `a7dedf3f.`); `PUBLISHED_C` in `scripts/verify_seed.py` | `BA1304FDDD26F58844667123BE82597455094DD8E169CA4A7E6D799A513C843C` (the file's own sha256 at the commit); `dev.py fp` converged and reported "matches canonical". The canonical moved at this pin (was `D78411C0.` at `a5cdf77`). |
| Runtime ABI header | `runtime/orbit_api.h` at `ca3eb91` (blob `5757f48f.`) | Unchanged since D-F0-15. `orbit_print` is `void orbit_print(const char* str)`. |
| Runtime umbrella header | `runtime/runtime.h` at `17ed624` (blob unchanged since `862f257`) | Committed blob `ffbd6a94ca4c52d0cee9f768f2fcb7bb0c1f1622`. Loses its `#include "oracle.c"` when that file is deleted (`runtime/oracle.c` + `runtime/oracle.h`, −218 lines). The R6 runtime archive compiles through this header, so the row is pinned from D-F0-11 on. |
| Runtime build recipe | `scripts/mklib_runtime.py` at `17ed624` (blob unchanged since `299eb09`) | Committed blob `86858c142759ba539e10e6880467f9372e7b617c`. Tracked by the pin; it builds one unity runtime TU with `ORBIT_WITH_NET`, `ORBIT_LIB_NO_ROUTER`, `ORBIT_PERF_IMPL`; `--out` supports an R6-local artifact path (the script itself writes only there plus its own temp dir, so `--out` keeps the checkout untouched). `--cc C:\w64devkit\bin\gcc.exe`. |
| Runtime library, Windows x64 | Built from the official pinned recipe/source to `.r6-build/runtime-win64/libruntime_net.a` (local ignored build output; not committed) | `runtime/` and `scripts/mklib_runtime.py` are the same blobs at `ca3eb91` as at `a5cdf77`, so the D-F0-15 archive is kept: one member `orbit_lib.o`, **337 defined symbols** (`nm --defined-only`). **Not byte-reproducible** (D-F0-11). Observation of this build: sha256 `b5a109999dc97fbb911c380bf092f80648ea42769049d2d4ae25b02f9f85f927`, 141360 bytes. |
| Runtime source (implementation) | `runtime/arena.c` at `ca3eb91` | Committed blob `fae06521f3ce4eb3c73f3316cf1347ac7cf91380`, unchanged since D-F0-15. |
| Normative backend reference | `compiler/emit/c_backend.orb` at `ca3eb91` | Committed blob `e1e2d402` (was `39a47614` at `a5cdf77`). Line citations into this file in `docs/DECISIONS.md` before D-F0-16 are anchored at earlier pins and are not re-anchored; read them with `git cat-file blob <that pin>:compiler/emit/c_backend.orb`. |
| IR emitter source | `compiler/emit/ir_emit.orb` at `ca3eb91` | Committed blob `a26ace90`, unchanged since D-F0-14. |
| Corpus emitter script | `scripts/emit_ir_corpus.py` at `ca3eb91` | Committed blob `e7a4dbe438a0621ac0c2c93f49a92916ea92c681`, unchanged since D-F0-15; discovers its members from the directories and asserts 109 suite + 1 support + 23 std. |
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
`main`, so the emitter commit resolves from the user's own checkout. From
D-F0-15 on the pinned commit is on the public remote, so the corpus and the
gates can be reproduced from public bytes. The earlier local-only pin
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

**D-F0-15 (2026-10-26).** Moves the pin from `33abf7a` to `a5cdf77`, 43
commits and the first public pin. The move is not IR-neutral: the corpus
grows to 133 documents (`filesSha256 91341401.`), `&&`/`||` lower to jumps,
a multi-payload match reads after the tag test, and register types drop from
8045 to 425 `unknown` destinations. The fixed point rebuilt from the commit
converges to `PUBLISHED_C D78411C0.` (was `138FBDF4.`) and the runtime archive
rebuilt from the same sources defines 337 symbols. The gates print diff
59/59, loader 235/235, corpus 91 build / 91 match over 132 programs, none
differing (was 67/67 over 97), and census 0 build over 27 modules.

**D-F0-16 (2026-10-27).** Moves the pin from `a5cdf77` to `ca3eb91`, four
commits that type more IR registers (341 `unknown` destinations, was 425). The
serializer and the runtime did not move, so the runtime archive is kept. The
fixed point converges to `PUBLISHED_C BA1304FD.`. The gates print diff 60/60,
loader 238/238, corpus 95 build / 95 match over 132 programs, none differing,
and census 0 build over 27 modules.
