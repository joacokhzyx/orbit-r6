# Pinned language inputs

R6 pins one `orbit-lang` commit and the inputs that make its evidence
reproducible. The commit is on the public remote
`https://github.com/joacokhzyx/orbit-lang`. A local checkout is an observation
of one machine; the commit is the pin.

| Input | Value | Evidence and status |
|---|---|---|
| Orbit repository | `C:\Users\Usuario\Downloads\orbit-lang` (read-only to R6) | Local checkout; do not write there |
| Orbit commit | `01e31e882610cea21fbfbf4fe02fee00051fab2e` (`main`) | D-F0-21: 8 commits after `bf38b38` (D-F0-20) in two merges. The runtime archive is one member per module, a program starts only what it uses, and `arena` is a core member plus seven extensions. The corpus is 143 executable documents plus one support module |
| Orbit IR version | IRv1, published by `main` at `01e31e8` | `docs/IR_SPEC.md` blob `5db1c1db` (was `49aaa478`): 66 opcodes, `unknown` written as `void*`; `int` is 64 bits, `shl` and `shr` mask the count with 63, and section 1.5 defines the checked `div` and `mod` that read `sourceName` and the instruction `line`. `ir_emit.orb` blob `e5e77a05`. Both unchanged since `bf38b38` |
| IR emitter (in-repo) | `scripts/emit_ir_corpus.py` at `01e31e8`, blob `488a689b` | Discovers 120 suite programs, `imports.support.orb` and 23 std programs. The corpus emitted from `C:\Temp\orbit-pin-01e31e8-git` is 144 documents with `filesSha256 a533ace1` |
| IR emitter binary (oracle) | `C:\Temp\orbit-fp-ee60c8af43.exe` | SHA-256 `9de8324a82b638ecc864a00ab9490efe80bb8c3c1d5170bc5c55ea59a5c344ea`, 2720256 bytes, `orbit 0.1.0-rc.2`; fixed-point build of `01e31e8` in `C:\Temp\orbit-pin-01e31e8-git`. The name is `orbit-fp-` plus the first 10 hex digits of `sha256(checkout path)`, so a rebuild in place overwrites it. **The bytes are build-site dependent**: this hash identifies this build, not the pin. It builds all 143 corpus programs. The pin makes native the default backend on Windows, so the harnesses set `ORBIT_BACKEND=c` for every oracle build (D-F0-20) |
| Emitted IR corpus | `tests/corpus/manifest.json` with `baseline.json` and `run_corpus.py` | 143 executable plus 1 support, 144 documents, 6992633 bytes, `filesSha256 a533ace164a28bed32dba3d3df900c3e8811b69ce8d6829d8503e132c0f71e03`, per-file SHA-256 in the manifest, `languageCommit 01e31e882610cea21fbfbf4fe02fee00051fab2e`. Measured at this pin as 143 build and 143 match over 143 programs, ratchet `r6BuildOk 143`, `match 143`. `axes-baseline.json` is re-taken on the same corpus. The IR is regenerated, not committed (D-M1-17) |
| Compiler IR census | `tests/compiler-ir/baseline.json` with `run_census.py` | 27 modules of `compiler/**/*.orb` emitted at the pin, `irFilesSha256 61e61734.`; 2 build. The entry points `compiler/host/main.orb` and `compiler/selfhost/lexer_trace.orb` build; the other 25 are libraries whose only refusal is `module must define one non-extern main`. The ratchet is one-way over `r6BuildOk` (D-M1-34) |
| Self-host canonical | `compiler/selfhost/stage3.exe.c` at `01e31e8`, blob `3850e41d.`; `PUBLISHED_C` in `scripts/verify_seed.py` | `E794E2DBF070AD5EA04DC2617E888694D37069896AC3B67BE2A22A9D857E92B1`. The C that the R6-built compiler writes for `compiler/host/main.orb` has this SHA-256. `scripts/selfhost.ps1` and `scripts/selfhost.sh` print SELFHOST OK with it. The canonical moved at this pin (was `70129F41.`) |
| Runtime ABI header | `runtime/orbit_api.h` at `01e31e8`, blob `e90288a8.` (was `315d34d3.`) | `orbit_int` is `int64_t` (was `int`), `orbit_streq` becomes `orbit_ct_equal`, and three HTTP limit functions are declared: `orbit_http_limits_reload`, `orbit_http_parse_error_response`, `orbit_http_buffer_full_response`. `orbit_print` is still `void orbit_print(const char* str)` |
| Runtime umbrella header | `runtime/runtime.h` at `17ed624`, blob `ffbd6a94ca4c52d0cee9f768f2fcb7bb0c1f1622` | Unchanged since `862f257`. It loses its `#include "oracle.c"` when that file is deleted (`runtime/oracle.c` and `runtime/oracle.h`, −218 lines). The R6 runtime archive compiles through this header, so the row is pinned from D-F0-11 on |
| Runtime build recipe | `scripts/mklib_runtime.py` at `01e31e8`, blob `eae20666.` (was `ec192790.`) | One archive member per runtime module: each `<m>.c` compiles on its own with `-DORBIT_RUNTIME_MODULE`. `--out <absolute path>` writes the plain archive (25 members) and `--db --out <absolute path>` the database one (27 members). `--cc C:\w64devkit\bin\gcc.exe`. The recipe writes a `.stamp` file beside the archive, which only the language reads. On Linux, `scripts/build-runtime-linux.sh <pinned-checkout> [output-dir]` calls the script with `--cc gcc` and `--db`, and defaults the output directory to `.r6-build/runtime-linux-x64` |
| Runtime library, Windows x64 | Built from the pinned recipe to `.r6-build/runtime-win64/libruntime_net.a` (ignored, not committed) | Rebuilt from `C:\Temp\orbit-pin-01e31e8-git`: 25 members, 174332 bytes, and `nm -g --defined-only` lists 268 symbols of types T, D, B, R and C. **Not byte-reproducible** (D-F0-11). The database variant (27 members) is built by `scripts/build-runtime-db.py` (D-M1-87): the plain members plus `database` and `auth`, and `sqlite3.lib` copied from `runtime/vendor/win-x64`. A database executable needs `sqlite3.dll` beside it or on `PATH` |
| Runtime library, Linux x64 | Built with `scripts/build-runtime-linux.sh <pinned-checkout> [output-dir]` to `.r6-build/runtime-linux-x64/` (ignored, not committed) | The wrapper calls the pin's `scripts/mklib_runtime.py --cc gcc` and adds `--db` for the database archive. The plain archive has 25 members and the database one 27, counted with `ar t`. Built in WSL2 Ubuntu with GCC 15.2.0 and `/usr/bin/ld` (GNU ld.bfd). **Not byte-reproducible** (D-F0-11), like the Windows archives |
| Runtime source | `runtime/arena.c` at `01e31e8`, blob `389e6cb773adc6219f86204216a0870da2cfe212` (was `a9c12c78`) | The arena core; the extensions are `runtime/arena_*.c` |
| Normative backend reference | `compiler/emit/c_backend.orb` at `01e31e8`, blob `147f2582` (was `b17b716d`) | The start-up rules of `docs/STARTUP.md` (`moduleCallsStartup`) and `orbit_print_string` for a string `print`. It names every user symbol `o_<name>` (`cUserName`) and keeps `orbit_main` for the entry. Line citations in `docs/DECISIONS.md` before D-F0-20 are anchored at earlier pins; read them with `git cat-file blob <that pin>:compiler/emit/c_backend.orb` |
| IR emitter source | `compiler/emit/ir_emit.orb` at `01e31e8`, blob `e5e77a05` | The module `sourceName` and the instruction `line` that the checked division reads. Unchanged since `bf38b38` |
| Source corpus | 143 executable programs: 120 suite programs and 23 std programs; the suite also has one `.support.orb` helper excluded by `scripts/test_suite.py:59` | Pinned inventory; the same 143 programs are the corpus |
| Toolchain | GCC 16.2.0, target `x86_64-w64-mingw32`, on Windows x64; GCC 15.2.0 on Linux x64 | Windows uses w64devkit; Linux uses WSL2 Ubuntu and GNU ld.bfd |

## Reproducibility

A build artifact embeds the absolute path of the directory it was built in, so
no SHA-256 of a local binary or archive is a stable anchor. What is pinned: the
Orbit commit, the canonical C (`PUBLISHED_C`), the recipe and source blobs, the
corpus `filesSha256`, and the symbol set. Every hash of a local artifact in this
file is an observation of one build (D-F0-11).

Pinned bytes come from `git cat-file blob 01e31e8:<path>`, never from a working
tree. The sibling checkout is neither the pin nor a stable reference: it can be
ahead of the pin and dirty. Corpus and census inputs come from a tree
materialized at the pin.

## Moving the pin

A pin move is one commit. It carries a dated entry in
[`DECISIONS.md`](DECISIONS.md), this file, the regenerated
`tests/corpus/manifest.json` and the re-taken `tests/corpus/baseline.json`.
They describe one tree, and a commit that splits them describes a tree that
never existed.

1. Push the new `orbit-lang` commit first. A pin to a commit no remote has
   cannot be reproduced.
2. Materialize the commit in a scratch checkout and build its fixed-point
   compiler.
3. Rebuild the runtime archives and regenerate the corpus against that tree.
4. Re-take the baseline under the new pin and run both gates on both hosts.
5. Record the decision with the new hashes, the corpus inventory and the gate
   counts.

The pin history, including every superseded commit, is in
[`DECISIONS.md`](DECISIONS.md) under D-F0.
