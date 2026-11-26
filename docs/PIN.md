# Pinned language inputs

R6 pins one `orbit-lang` commit and the inputs that make its evidence
reproducible. The commit is on the public remote
`https://github.com/joacokhzyx/orbit-lang`. A local checkout is an observation
of one machine; the commit is the pin.

| Input | Value | Evidence and status |
|---|---|---|
| Orbit repository | `C:\Users\Usuario\Downloads\orbit-lang` (read-only to R6) | Local checkout; do not write there |
| Orbit commit | `31591e66edf21871f4d18e246700900ee6dc0402` (`feature/0.1.0-stability`, tag `v0.1.0-rc.5`) | D-F0-22: 335 commits after `01e31e8` (D-F0-21) in 15 merges. The frontend learns four more typing cases and the unknown census falls 18 to 3 of 149684 instructions; `runtime/database.c` includes the header it calls; `docs/IR_SPEC.md` stops counting §7's opcodes in V-03. The corpus is 152 documents, 151 of them executable |
| Orbit IR version | IRv1, published by `31591e6` | `docs/IR_SPEC.md` blob `2e4b305fa1` (was `5db1c1db` at `01e31e8`): §1 keeps its 66-opcode inventory, `unknown` written as `void*`; `int` is 64 bits, `shl` and `shr` mask the count with 63, and §1.5 defines the checked `div` and `mod` that read `sourceName` and the instruction `line`. §7 is new: 11 opcodes a runtime module may carry, the `runtimeModule` and `export` fields, and the `RtPtr`/`RtArena` types. Validity rule V-03 names §7 instead of counting it. `ir_emit.orb` blob `adba026f0c` |
| IR emitter (in-repo) | `scripts/measure.py emit-ir` at `31591e6`, blob `f1db0d216f` | Discovers 127 suite programs, `imports.support.orb` and 24 std programs (`scripts/measure.py:3623`). The standalone `scripts/emit_ir_corpus.py` of the previous pin was folded in at `e8e1e19`, so `run_corpus.py` asks the pin which entry point it carries (D-M1-141). The corpus emitted from `C:\Temp\orbit-pin-31591e6` is 152 documents with `filesSha256 51d5a2e9` |
| IR emitter binary (oracle) | `C:\Temp\fp-d55c456.exe` | SHA-256 `d3aaf818be62ccb33f942e5e567855b34ca158087bcfaef6f255c2fecb182291`, 904192 bytes, `orbit 0.1.0`. Built by `scripts/trust.py verify --cc gcc --emit-fixed-point` in `C:\Temp\orbit-pin-d55c456`, which reports `Finished verify: 4/4 pass` and chain2 and chain3 C equal to `PUBLISHED_C`. The name is a scratch name, not a pin. `git diff --stat d55c456 31591e6` changes `README.md` and `docs/IR_SPEC.md` only, so the binary of `rc.4` builds `rc.5` unchanged. **The bytes are build-site dependent**: this hash identifies this build, not the pin (D-F0-11). It builds all 151 corpus programs. The pin makes native the default backend on Windows, so the harnesses set `ORBIT_BACKEND=c` for every oracle build (D-F0-20) |
| Emitted IR corpus | `tests/corpus/manifest.json` with `baseline.json` and `run_corpus.py` | 151 executable plus 1 support, 152 documents, 7201698 bytes, `filesSha256 51d5a2e93cbfca8a7f41d8b5ab7196eb1df16cd36e9d1b421c563154ed2d1886`, per-file SHA-256 in the manifest, `languageCommit 31591e66edf21871f4d18e246700900ee6dc0402`. Measured at this pin: 151 build and 151 match over 151 programs, ratchet `r6BuildOk 151`, `match 151`, `unknownRegisterTypes` in 3 of the 152 documents, `opcodes never in a matching program: 0/45`. The previous pin refused one program, `system_request_classes`, because its runtime archive predated `system_http_requests_ok` (D-M1-140); the archive rebuilt from this pin carries the symbol and the refusal is gone. The IR is regenerated, not committed (D-M1-17). `axes-baseline.json` still names the previous corpus (`corpusFilesSha256 a533ace1.` over 143 programs) and was not re-taken here: `--check-axes` is not part of the corpus gate and re-taking it costs five runs of every program |
| Compiler IR census | `tests/compiler-ir/baseline.json` with `run_census.py` | 29 modules of `compiler/**/*.orb` emitted at the pin, `irFilesSha256 6844e59a.`; 1 builds. `compiler/selfhost/lexer_trace.orb` is the one module R6 builds; 27 refuse with `module must define one non-extern main` because they are libraries. `compiler/host/main.orb` emits and fails at link: it calls `orbit_ui_caps`, `orbit_ui_width`, `orbit_ui_anim_start`, `orbit_ui_anim_stage`, `orbit_ui_anim_stop` and `orbit_ui_text_size`, which `runtime/console.c` defines only under `ORBIT_WITH_EXEC` (`runtime/console.c:49-50`), and the plain archive deliberately omits them so a program that prints does not carry them. The compiler's own recipe compiles the runtime with that define. This is a gap in the archive recipe, not in codegen, and it is the one row the ratchet records as lower than the previous pin's 2 (D-F0-22). The ratchet is one-way over `r6BuildOk` (D-M1-34) |
| Self-host canonical | `compiler/selfhost/stage3.exe.c` at `31591e6`, blob `cd553b3d22`; `PUBLISHED_C` in `scripts/trust.py` | `940265C21BE022DA64E33330AFAA9B8C65D88A4D38940FB98117221A45E27704`. The file moved at `rc.3` (`3531d75`) and has not changed since. `scripts/trust.py verify` prints `Verified chain2 C fixed point ... ok` and `chain3 ... ok` with that hash, and `seed C matches published contract` |
| Runtime ABI header | `runtime/orbit_api.h` at `31591e6`, blob `90cc1c8ff0` (was `e90288a8.`) | The last commit to touch it is `125b7be`. `orbit_int` is `int64_t`, `orbit_streq` is `orbit_ct_equal`, and the three HTTP limit functions are declared. `orbit_print` is still `void orbit_print(const char* str)` |
| Runtime umbrella header | `runtime/runtime.h` at `31591e6`, blob `216a88acf5` (was `ffbd6a94.`) | The last commit to touch it is `d31a945`, which dropped the admission layer's codename. The R6 runtime archive compiles through this header, so the row is pinned from D-F0-11 on |
| Runtime build recipe | `scripts/mklib_runtime.py` at `31591e6`, blob `d31d847a75` (was `eae20666.`) | One archive member per runtime module: each `<m>.c` compiles on its own with `-DORBIT_RUNTIME_MODULE`. `--out <absolute path>` writes the plain archive (26 members) and `--db --out <absolute path>` the database one (28 members). `--cc C:\w64devkit\bin\gcc.exe`. Changed at this pin: `--self-test` compiles every `DB_MODULES` member alone, because `--db` is the only recipe that does and a call into a module whose header is not included stays invisible until then. The recipe writes a `.stamp` file beside the archive, which only the language reads. On Linux, `scripts/build-runtime-linux.sh <pinned-checkout> [output-dir]` calls the script with `--cc gcc` and `--db`, and defaults the output directory to `.r6-build/runtime-linux-x64` |
| Runtime library, Windows x64 | Built from the pinned recipe to `.r6-build/runtime-win64/libruntime_net.a` (ignored, not committed) | Rebuilt from `C:\Temp\orbit-pin-31591e6`: 26 members, 180906 bytes. The database variant (28 members, 212782 bytes) is built by `scripts/build-runtime-db.py` (D-M1-87): the plain members plus `database` and `auth`, and `sqlite3.lib` copied from `runtime/vendor/win-x64`. `nm -g --defined-only` lists `system_http_requests_ok` as `T` in both. **Not byte-reproducible** (D-F0-11). A database executable needs `sqlite3.dll` beside it or on `PATH` |
| Runtime library, Linux x64 | Built with `scripts/build-runtime-linux.sh /mnt/c/Temp/orbit-pin-31591e6` to `.r6-build/runtime-linux-x64/` (ignored, not committed) | The wrapper calls the pin's `scripts/mklib_runtime.py --cc gcc` and adds `--db` for the database archive. Built in WSL2 Ubuntu with GCC 15.2.0 and `/usr/bin/ld` (GNU ld.bfd): 26 members plain, 28 database, and `nm -g --defined-only` finds `system_http_requests_ok` in the plain archive. **Not byte-reproducible** (D-F0-11), like the Windows archives |
| Runtime source | `runtime/arena.c` at `31591e6`, blob `f42f1887e5` | The arena core; the extensions are `runtime/arena_*.c`. The last commit to touch it is `e8a8302` |
| Normative backend reference | `compiler/emit/c_backend.orb` at `31591e6`, blob `df2ff93019` | The start-up rules of `docs/STARTUP.md` (`moduleCallsStartup`) and `orbit_print_string` for a string `print`. It names every user symbol `o_<name>` (`cUserName`) and keeps `orbit_main` for the entry. Line citations in `docs/DECISIONS.md` before D-F0-20 are anchored at earlier pins; read them with `git cat-file blob <that pin>:compiler/emit/c_backend.orb` |
| IR emitter source | `compiler/emit/ir_emit.orb` at `31591e6`, blob `adba026f0c` | The module `sourceName` and the instruction `line` that the checked division reads |
| Source corpus | 151 executable programs: 127 suite programs and 24 std programs; the suite also has `tests/suite/imports.support.orb`, which the corpus counts as its one support member | Pinned inventory. `scripts/measure.py:3623-3626` states the rule: every `.orb` under the two directories minus `*.support.orb` is executable |
| Toolchain | GCC 16.2.0, target `x86_64-w64-mingw32`, on Windows x64; GCC 15.2.0 on Linux x64 | Windows uses w64devkit; Linux uses WSL2 Ubuntu and GNU ld.bfd. The WSL image runs the stock `6.18.33.2-microsoft-standard-WSL2` kernel: a local `.wslconfig` named a custom kernel under `C:\Temp` that no longer exists, and it was commented out on 2026-11-08 |

## Reproducibility

A build artifact embeds the absolute path of the directory it was built in, so
no SHA-256 of a local binary or archive is a stable anchor. What is pinned: the
Orbit commit, the canonical C (`PUBLISHED_C`), the recipe and source blobs, the
corpus `filesSha256`, and the symbol set. Every hash of a local artifact in this
file is an observation of one build (D-F0-11).

Pinned bytes come from `git cat-file blob 31591e6:<path>`, never from a working
tree. The sibling checkout is neither the pin nor a stable reference: it can be
ahead of the pin and dirty. Corpus and census inputs come from a tree
materialized at the pin.

## Moving the pin

A pin move is one commit. It carries a dated entry in
[`DECISIONS.md`](DECISIONS.md), this file, the workflow pin, the regenerated
`tests/corpus/manifest.json` and the re-taken `tests/corpus/baseline.json`.
Run the compiler census against the same checkout. Record any refusal and
retain its existing baseline when the census would decrease `r6BuildOk`.

1. Push the new `orbit-lang` commit first. A pin to a commit no remote has
   cannot be reproduced.
2. Materialize the commit in a scratch checkout and build its fixed-point
   compiler.
3. Rebuild the runtime archives and regenerate the corpus against that tree.
4. Re-take the corpus baseline under the new pin and run the gates on both hosts.
5. Record the decision with the new hashes, the corpus inventory and the gate
   counts.

The pin history, including every superseded commit, is in
[`DECISIONS.md`](DECISIONS.md) under D-F0.
