# Pinned language inputs

| Input | Value | Evidence / status |
|---|---|---|
| Orbit repository | `C:\Users\Usuario\Downloads\orbit-lang` (read-only to R6) | Local checkout inspected; do not write there. |
| Orbit commit | `a5cdf779ddc0ba37f72eb0a9dcfd750044b3430c` (`main`) | On the public remote `https://github.com/joacokhzyx/orbit-lang` (`git push` `3724b20..a5cdf77` on 2026-10-26; `33abf7a` was already there). 43 commits after `33abf7a` (D-F0-14): three merges add 34 suite programs (`corpus-growth`, `lang-fixes`), eight compiler fixes with regression tests (`lang-fixes`), the `-O1` sharded bootstrap with an early fixed-point stop (`build-speed`), and register types the front end already knows (`typed-ir`). `compiler/emit/c_backend.orb`, `compiler/ir/builder.orb`, `compiler/front/sema.orb`, `compiler/selfhost/stage3.exe.c`, `runtime/arena.c`, `runtime/orbit_api.h`, `scripts/emit_ir_corpus.py` and `docs/IR_SPEC.md` moved; `compiler/emit/ir_emit.orb`, `runtime/runtime.h` and `scripts/mklib_runtime.py` did not. Moved from `33abf7a` by D-F0-15. |
| Orbit IR version | IRv1, published by `main` at `a5cdf77` | Same serializer as D-F0-14 (`ir_emit.orb` blob `a26ace90`). Two shape changes recorded in `docs/IR_SPEC.md` at the pin: `&&`/`||` lower to `copy`, `jump_if_false`, `jump` and `label` around the right operand, so `and_op`/`or_op` have no producer; a multi-payload `match` reads its `load_field`s after the tag test. `registerTypes` carries the types the builder proves: 425 `unknown` register destinations over the 133-document corpus, 8045 at `d58d647` (`scripts/ir_unknown_census.py`). |
| IR emitter (in-repo) | `C:\Users\Usuario\Downloads\orbit-lang`, branch `main`, commit `a5cdf779ddc0ba37f72eb0a9dcfd750044b3430c` | `scripts/emit_ir_corpus.py` (blob `e7a4dbe4.`) discovers 109 suite programs, `imports.support.orb` and 23 std programs; the corpus emitted from `C:\Temp\orbit-pin-a5cdf77-git` (a `git clone` at the commit) is 133 documents, `filesSha256 91341401.`. Public (D-F0-15). |
| IR emitter binary (oracle) | `C:\Temp\orbit-fp-b0d70c396d.exe` | SHA-256 `c0df50b7839569a5af2398a9b33f82c74638e5869d2b843268d4b0a1fc48c00d`; 861696 bytes; `orbit 0.1.0-rc.2`; fixed-point build of the compiler sources at `a5cdf77` (`python scripts/dev.py fp` in `C:\Temp\orbit-pin-a5cdf77-git`; "converged (matches canonical)"). The name is `orbit-fp-` + the first 10 hex of `sha256(checkout path)`, so a rebuild in place overwrites it. **The bytes are build-site dependent**: this hash identifies *this* build, not the pin; the reproducible anchors are the commit and the canonical. With it the pinned tree passes `tests/suite` 109/109 and `tests/std` 23/23. Oracle of the differential, the corpus and the census. |
| Emitted IR corpus (R6 pin) | `tests/corpus/manifest.json` with `baseline.json` and `run_corpus.py` | 132 executable + 1 support = 133 documents, 6808416 bytes, `filesSha256 91341401c99b132df989d50eaf0a3a622f9292707184f085c2d02f2be4a63316`, per-file SHA-256 in the manifest, `languageCommit a5cdf779ddc0ba37f72eb0a9dcfd750044b3430c`; regenerate with `run_corpus.py --orbit-root <tree at the pin>`. Measured, not committed as `.ir` (D-M1-17); measured at this pin as **91 build / 91 match over 132 programs**, none differing (ratchet `r6BuildOk 91`, `match 91`, D-F0-15). |
| Compiler IR census (R6, D-M1-34) | `tests/compiler-ir/baseline.json` with `run_census.py` | 27 modules of `compiler/**/*.orb` emitted at the pin with `--emit-ir`, `irFilesSha256 bcac55b1.`; **0 build**. First refusals: `union ASTNode` 14, `no non-extern main` 6, `union IRValue` 4, `list_push` 1, `main must have no parameters` 1, `register type unknown` 1. Regenerate with `run_census.py --compiler <fp> --tree <tree at the pin>`; the ratchet is one-way over `r6BuildOk`. |
| Self-host canonical (in-repo) | `compiler/selfhost/stage3.exe.c` at `a5cdf77` (blob `00c22e15.`); `PUBLISHED_C` in `scripts/verify_seed.py` | `D78411C08CB3589E66AC89E99A1F9D3352DC56A3D1F5086F95669B03D3EAB08F` (the file's own sha256 at the commit); `dev.py fp` converged and reported "matches canonical". The canonical moved at this pin (was `138FBDF4.` at `33abf7a`): the compiler fixes and the typed IR change the compiler's own C. |
| Runtime ABI header | `runtime/orbit_api.h` at `a5cdf77` (blob `5757f48f.`) | Five lines added against `523f2913.` (D-F0-13), no signature moved; the defined-symbol set of the archive is unchanged (next row). `orbit_print` is `void orbit_print(const char* str)`. |
| Runtime umbrella header | `runtime/runtime.h` at `17ed624` (blob unchanged since `862f257`) | Committed blob `ffbd6a94ca4c52d0cee9f768f2fcb7bb0c1f1622`. Loses its `#include "oracle.c"` when that file is deleted (`runtime/oracle.c` + `runtime/oracle.h`, −218 lines). The R6 runtime archive compiles through this header, so the row is pinned from D-F0-11 on. |
| Runtime build recipe | `scripts/mklib_runtime.py` at `17ed624` (blob unchanged since `299eb09`) | Committed blob `86858c142759ba539e10e6880467f9372e7b617c`. Tracked by the pin; it builds one unity runtime TU with `ORBIT_WITH_NET`, `ORBIT_LIB_NO_ROUTER`, `ORBIT_PERF_IMPL`; `--out` supports an R6-local artifact path (the script itself writes only there plus its own temp dir, so `--out` keeps the checkout untouched). `--cc C:\w64devkit\bin\gcc.exe`. |
| Runtime library, Windows x64 | Rebuilt from the official pinned recipe/source at `a5cdf77` to `.r6-build/runtime-win64/libruntime_net.a` (local ignored build output; not committed) | One archive member `orbit_lib.o`; `nm --defined-only` reports **337 defined symbols**, the same count as D-F0-13; the exported names are the same as the `33abf7a` build, and only a gcc-generated local (`orbit_alloc_try.part.0` -> `orbit_alloc_try.part.0.constprop.0`) differs. Build used GCC 16.2.0 `x86_64-w64-mingw32` via `--cc` and an **absolute** `--out`. **Not byte-reproducible** (D-F0-11). Observation of this build: sha256 `b5a109999dc97fbb911c380bf092f80648ea42769049d2d4ae25b02f9f85f927`, 141360 bytes. |
| Runtime source (implementation) | `runtime/arena.c` at `a5cdf77` | Committed blob `fae06521f3ce4eb3c73f3316cf1347ac7cf91380` (was `59f12e24.`); `runtime/string_pool.c` also moved (pool kept consistent when a grow fails). |
| Normative backend reference | `compiler/emit/c_backend.orb` at `a5cdf77` | Committed blob `39a47614` (was `2eacd0c5.` at `33abf7a`). Line citations into this file in `docs/DECISIONS.md` before D-F0-15 are anchored at earlier pins and are not re-anchored; read them with `git cat-file blob <that pin>:compiler/emit/c_backend.orb`. |
| IR emitter source | `compiler/emit/ir_emit.orb` at `a5cdf77` | Committed blob `a26ace90`, unchanged since D-F0-14. |
| Corpus emitter script | `scripts/emit_ir_corpus.py` at `a5cdf77` | Committed blob `e7a4dbe438a0621ac0c2c93f49a92916ea92c681` (was `d3da729c.`); discovers its members from the directories and asserts 109 suite + 1 support + 23 std. Pinned so the 133-document corpus can be regenerated from the commit. |
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
