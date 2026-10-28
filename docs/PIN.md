# Pinned language inputs

| Input | Value | Evidence / status |
|---|---|---|
| Orbit repository | `C:\Users\Usuario\Downloads\orbit-lang` (read-only to R6) | Local checkout inspected; do not write there. |
| Orbit commit | `01e31e882610cea21fbfbf4fe02fee00051fab2e` (`main`) | On the public remote `https://github.com/joacokhzyx/orbit-lang`. 8 commits after `bf38b38` (D-F0-20) in two merges: `lean-runtime` (one archive member per runtime module, start-up per program, `docs/STARTUP.md`) and `arena-split` (`arena` as a core member plus seven extensions). `runtime/` and `scripts/mklib_runtime.py` moved. Moved from `bf38b38` by D-F0-21. |
| Orbit IR version | IRv1, published by `main` at `01e31e8` | `docs/IR_SPEC.md` blob `5db1c1db` (was `49aaa478`): 66 opcodes, `unknown` written as `void*`; `int` is 64 bits, `shl`/`shr` mask the count with 63, and section 1.5 defines the checked `div`/`mod` that read `sourceName` and the instruction `line`. `ir_emit.orb` blob `e5e77a05`. Both unchanged since `bf38b38`. |
| IR emitter (in-repo) | `C:\Users\Usuario\Downloads\orbit-lang`, branch `main`, commit `01e31e882610cea21fbfbf4fe02fee00051fab2e` | `scripts/emit_ir_corpus.py` (blob `488a689b`, unchanged) discovers 120 suite programs, `imports.support.orb` and 23 std programs; the corpus emitted from `C:\Temp\orbit-pin-01e31e8-git` (a `git clone` at the commit) is 144 documents, `filesSha256 a533ace1`. Public (D-F0-21). |
| IR emitter binary (oracle) | `C:\Temp\orbit-fp-ee60c8af43.exe` | SHA-256 `9de8324a82b638ecc864a00ab9490efe80bb8c3c1d5170bc5c55ea59a5c344ea`; 2720256 bytes; `orbit 0.1.0-rc.2`; fixed-point build of the compiler sources at `01e31e8` in `C:\Temp\orbit-pin-01e31e8-git`. The name is `orbit-fp-` + the first 10 hex of `sha256(checkout path)`, so a rebuild in place overwrites it. **The bytes are build-site dependent**: this hash identifies *this* build, not the pin; the reproducible anchors are the commit and the canonical. It builds all 143 corpus programs. Oracle of the differential, the corpus and the census. **The pin makes native the default backend on Windows**, so the harnesses set `ORBIT_BACKEND=c` for every oracle build (D-F0-20). |
| Emitted IR corpus (R6 pin) | `tests/corpus/manifest.json` with `baseline.json` and `run_corpus.py` | 143 executable + 1 support = 144 documents, 6992633 bytes, `filesSha256 a533ace164a28bed32dba3d3df900c3e8811b69ce8d6829d8503e132c0f71e03`, per-file SHA-256 in the manifest, `languageCommit 01e31e882610cea21fbfbf4fe02fee00051fab2e`; regenerate with `run_corpus.py --orbit-root <tree at the pin>`. Measured, not committed as `.ir` (D-M1-17); measured at this pin as **143 build / 143 match over 143 programs**, none differing (ratchet `r6BuildOk 143`, `match 143`, unchanged from D-F0-20). `axes-baseline.json` is re-taken on the same corpus. |
| Compiler IR census (R6, D-M1-34) | `tests/compiler-ir/baseline.json` with `run_census.py` | 27 modules of `compiler/**/*.orb` emitted at the pin with `--emit-ir`, `irFilesSha256 61e61734.`; **2 build**. Both entry points build: `compiler/host/main.orb` (the whole compiler) and `compiler/selfhost/lexer_trace.orb`. The other 25 modules are libraries: their only refusal is `module must define one non-extern main`, so no function in them is refused. Regenerate with `run_census.py --compiler <fp> --tree <tree at the pin>`; the ratchet is one-way over `r6BuildOk`. |
| Self-host canonical (in-repo) | `compiler/selfhost/stage3.exe.c` at `01e31e8` (blob `3850e41d.`); `PUBLISHED_C` in `scripts/verify_seed.py` | `E794E2DBF070AD5EA04DC2617E888694D37069896AC3B67BE2A22A9D857E92B1`. The C that the R6-built compiler writes for `compiler/host/main.orb` has this sha256 (`scripts/selfhost.ps1` prints SELFHOST OK). The canonical moved at this pin (was `70129F41.`). |
| Runtime ABI header | `runtime/orbit_api.h` at `01e31e8` (blob `e90288a8.`, was `315d34d3.`) | `orbit_int` is `int64_t` (was `int`), `orbit_streq` becomes `orbit_ct_equal`, and three HTTP limit functions are declared (`orbit_http_limits_reload`, `orbit_http_parse_error_response`, `orbit_http_buffer_full_response`). `orbit_print` is still `void orbit_print(const char* str)`. |
| Runtime umbrella header | `runtime/runtime.h` at `17ed624` (blob unchanged since `862f257`) | Committed blob `ffbd6a94ca4c52d0cee9f768f2fcb7bb0c1f1622`. Loses its `#include "oracle.c"` when that file is deleted (`runtime/oracle.c` + `runtime/oracle.h`, −218 lines). The R6 runtime archive compiles through this header, so the row is pinned from D-F0-11 on. |
| Runtime build recipe | `scripts/mklib_runtime.py` at `01e31e8` (blob `eae20666.`, was `ec192790.`) | One archive member per runtime module: each `<m>.c` is compiled on its own with `-DORBIT_RUNTIME_MODULE`. `--out <absolute path>` writes the plain archive (25 members) and `--db --out <absolute path>` the database one (27 members). `--cc C:\w64devkit\bin\gcc.exe`. The recipe also writes a `.stamp` file beside the archive, which only the language reads. |
| Runtime library, Windows x64 | Built from the official pinned recipe/source to `.r6-build/runtime-win64/libruntime_net.a` (local ignored build output; not committed) | Rebuilt from `C:\Temp\orbit-pin-01e31e8-git`: 25 members, 174332 bytes, and `nm -g --defined-only` lists 268 symbols of types T, D, B, R and C. **Not byte-reproducible** (D-F0-11). The DB variant (27 members) is built by `scripts/build-runtime-db.py`. |
| Runtime library, DB variant (R6, D-M1-87) | `scripts/build-runtime-db.py` builds `.r6-build/runtime-win64/libruntime_net_db.a` (local ignored build output; not committed) | `scripts/mklib_runtime.py --db --out <absolute path>` of the pin (D-M1-103): one member per module, the plain ones plus `database` and `auth`, and `sqlite3.lib` copied from `runtime/vendor/win-x64`. The language links the same way (`compiler/host/pipeline.orb`). A DB executable needs `sqlite3.dll` beside it or on `PATH` (`runtime/vendor/win-x64`). Not byte-reproducible. |
| Runtime source (implementation) | `runtime/arena.c` at `01e31e8` | Committed blob `389e6cb773adc6219f86204216a0870da2cfe212` (was `a9c12c78`): the arena core. The extensions are `runtime/arena_*.c`. |
| Normative backend reference | `compiler/emit/c_backend.orb` at `01e31e8` | Committed blob `147f2582` (was `b17b716d`): the start-up rules of `docs/STARTUP.md` (`moduleCallsStartup`) and `orbit_print_string` for a string `print`. It names every user symbol `o_<name>` (`cUserName`) and keeps `orbit_main` for the entry. Line citations into this file in `docs/DECISIONS.md` before D-F0-20 are anchored at earlier pins and are not re-anchored; read them with `git cat-file blob <that pin>:compiler/emit/c_backend.orb`. |
| IR emitter source | `compiler/emit/ir_emit.orb` at `01e31e8` | Committed blob `e5e77a05` (unchanged since `bf38b38`): the module `sourceName` and the instruction `line` that the checked division reads. |
| Corpus emitter script | `scripts/emit_ir_corpus.py` at `01e31e8` | Committed blob `488a689bd1cc7fc4e213b07c503f9c83fecdfdda` (unchanged since `bf38b38`); it discovers its members from the directories (120 suite + 1 support + 23 std at this pin). |
| Reproducibility of build artifacts | Measured for the emitter binary and for the runtime archive/member | Both embed the absolute path of the directory they were built in, so no SHA-256 of a local binary or `.a` is a stable anchor. What is pinned: the Orbit commit, the canonical C (`PUBLISHED_C`), the recipe and source blobs, the corpus `filesSha256`, and the symbol set. Any hash of a local artifact in this file is an *observation* of one build. |
| Existing local runtime archive | `.orbit/lib/libruntime_net.a` in the Orbit checkout; untracked | Observed SHA-256 `2ffe2301b75ea86660547de8c2fe4231f740c149be4c43aa93db97877cba22dd` at `862f257` — a different build of the same commit (see the reproducibility row). Untracked and rebuilt by the language's own builds, so this is a historical observation of one build, not a pin. Not used by R6. |
| Runtime library, Linux x64 | Not yet built/verified | No Linux distribution is installed in WSL and no native Linux x64 compiler is available. A Linux x64 build with the pinned script is required before that platform is enabled. |
| Available GCC | GCC 16.2.0, target `x86_64-w64-mingw32` | Local Windows x64 MinGW toolchain; use `-lws2_32` at final Windows link as the pinned header documents. |
| Candidate M2 source corpus | 143 executable cases: 120 suite programs + 23 std programs; the suite also has 1 `.support.orb` helper excluded by `scripts/test_suite.py:59` | Pinned checkout inventory; the same 143 programs are the corpus (row "Emitted IR corpus"). |

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
IR-neutral. D-F0-12 moves the pin to `ce87d508`: a runtime and harness change that
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

**D-F0-17 (2026-10-27).** Moves the pin from `ca3eb91` to `0cfce74`, 22
commits that type the compiler's own IR. The serializer and the runtime did not
move, so the runtime archive is kept. The fixed point converges to
`PUBLISHED_C 54F0B797.`. The corpus grows to 136 documents. The gates print
diff 69/69, loader 261/261, corpus 109 build / 109 match over 135 programs,
none differing, and census 2 build over 27 modules.

**D-F0-18 (2026-10-27).** Moves the pin from `0cfce74` to `e34833a`, 26 commits
and four merges. The move is not IR-neutral: the corpus stays at 136 documents
and grows to 6850719 bytes (`filesSha256 1f2c44a4.`), the IR gains six opcodes,
every runtime symbol a module calls has an extern entry, and `void*` replaces
`unknown`. `runtime/` did not move, so the runtime archive is rebuilt from the
same sources with the same symbol set. The fixed point converges to
`PUBLISHED_C 80A61B67.` (was `54F0B797.`). The gates print diff 86/86, loader
290/290, corpus 125 build / 125 match over 135 programs, none differing, and
census 2 build over 28 modules.

**D-F0-19 (2026-10-28).** Moves the pin from `e34833a` to `6a9ddaf`, 11 commits
in one merge. The move is not IR-neutral: the corpus grows to 138 documents and
6872621 bytes (`filesSha256 741900ad.`), two suite programs join, and an int
copied into a float register becomes a `copy` that converts (D-M1-93).
`runtime/` did not move, so the runtime archive is rebuilt from the same
sources with the same symbol set. The fixed point converges to
`PUBLISHED_C C1176CF4.` (was `80A61B67.`). The gates print diff 95/95, loader
309/309, corpus 137 build / 137 match over 137 programs, none differing, and
census 2 build over 28 modules.

**D-F0-20 (2026-10-28).** Moves the pin from `6a9ddaf` to `bf38b38`, 27 commits
in seven merges. The move changes the IR of every program: the corpus is 144
documents and 6992300 bytes (`filesSha256 94025f2e.`), all 136 documents that
remain differ in hash, eight suite programs join and two leave. `int` is 64 bits
and checked division reads `sourceName` and `line` (R6 D-M1-94 to D-M1-97). User
symbols are `o_<name>` (R6 D-M1-99). `runtime/` changed in 20 files (13 outside
the tests), so the runtime archive defines 343 symbols (was 337). The fixed
point converges to `PUBLISHED_C 70129F41.` (was `C1176CF4.`). The gates print
diff 100/100, loader 341/341, corpus 143 build / 143 match over 143 programs,
none differing, and census 2 build over 27 modules. The pin makes native the
default backend on Windows, so `run_diff.py` and `run_corpus.py` set
`ORBIT_BACKEND=c` for the oracle build.

**D-F0-21 (2026-10-28).** Moves the pin from `bf38b38` to `01e31e8`, 8 commits in
two merges. The runtime archive has one member per module (25, and 27 with the
database), `docs/STARTUP.md` states what a program starts, and `arena` is a core
member plus seven extensions. The corpus is 144 documents and 6992633 bytes
(`filesSha256 a533ace1.`) with the same program list. The fixed point converges
to `PUBLISHED_C E794E2DB.` (was `70129F41.`). The gates print diff 100/100,
loader 341/341, corpus 143 build / 143 match over 143 programs, none differing,
census 2 build over 27 modules and SELFHOST OK. R6 needed D-M1-102 to D-M1-104
for this pin.
