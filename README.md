# Orbit R6

![Orbit Banner](assets/banner.png)

R6 is a native x86-64 code generator for the Orbit language IR. It reads
the JSON IR that `orbit build --emit-ir` writes and produces a native
executable: a Windows x64 PE on Windows, a Linux x64 ELF on Linux.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

## What R6 is not

- R6 does not lower programs through C. GCC never compiles an R6 program.
- R6 does not use GAS. `src/codegen-x64/win64.c` emits a private assembly text,
  `peephole.c` drops reloads of a slot just stored, `regalloc.c` moves the
  hottest slots into callee-saved registers, `asm_win64.c` encodes it to bytes,
  and `coff.c` writes an AMD64 COFF object on Windows while `elf64.c` writes an
  ELF64 object on Linux.
- GNU `ld` only links. It is the one external tool the program path runs.
- R6 does not guess. An opcode, type, version or platform outside the supported
  set gets one diagnostic and a nonzero exit.
- R6 builds for the host. One binary builds one target, chosen when R6 is
  compiled (`src/codegen-x64/target.h`): Win64 on Windows, System V x86-64 on
  Linux. There is no `--target` flag and no cross build. macOS has no
  measurement.

## Status

Measured on 2026-10-31 at the pin (`01e31e8`, see below), on both hosts:

| Check | Windows x64 | Linux x64 | Kind |
|---|---|---|---|
| Differential (`tests/diff/run_diff.py`) | 100/100 | 100/100 | gate |
| Loader contract suite (`tests/reject/run_reject.py`) | 452/452 | 452/452 | gate |
| Corpus (`tests/corpus/run_corpus.py`) | 143 build / 143 match | 143 build / 143 match | measurement with a one-way ratchet |
| Self-host (`scripts/selfhost.ps1`, `scripts/selfhost.sh`) | SELFHOST OK | SELFHOST OK | measurement |
| Compile time (`tests/perf/run_perf.py`) | baseline in `tests/perf/baseline.json` | baseline in `tests/perf/baseline.json` | measurement with a one-way ratchet |

The loader suite is 452 cases at this pin; `tests/reject/manifest.json` is the
source of that count. The compiler-IR census (`tests/compiler-ir/run_census.py`)
was last measured on 2026-10-28: 2 build over 27 modules, a measurement with a
one-way ratchet. The compile-time baseline holds the driver from before
D-M1-120: on Windows x64 the `-O2` build measures 17.2% less wall over the
corpus and 39.8% less on the self-host R6 build.

R6 builds the Orbit compiler itself (`compiler/host/main.orb`, 1038
functions). The compiler it produces translates its own source to C
byte-identical to the GCC-built compiler, and emits its own IR unchanged, which
R6 builds into a byte-identical executable (`docs/DECISIONS.md` D-M1-63). On
both hosts the generated C hashes to the pin's `PUBLISHED_C`,
`E794E2DBF070AD5EA04DC2617E888694D37069896AC3B67BE2A22A9D857E92B1`.

The supported subset is what `docs/M1_REPORT.md` states as measured. Overflow,
NaN payloads and other operating systems are out of evidence.

Three words recur below. The **oracle** is the pinned `orbit` compiler, which
builds the same program through C and GCC. The **differential** runs a program
built by the oracle and the same program built by R6, and requires identical
exit code and stdout bytes. The **fixed point** is the oracle binary built from
the pinned sources by the language's own bootstrap.

## Prerequisites

- Windows x64 or Linux x64, and Python 3.
- A GCC for the driver build only: w64devkit GCC 16.2.0 on Windows
  (`C:\w64devkit\bin\gcc.exe`), GCC 15.2.0 on Linux.
- The runtime archive, per host. Neither archive is in the repository and
  neither is byte-reproducible; both are rebuilt from the pinned Orbit checkout
  with its `scripts/mklib_runtime.py`. `docs/PIN.md` lists the recipes and the
  Windows symbol set.
  - Windows: `.r6-build\runtime-win64\libruntime_net.a`.
  - Linux: `.r6-build/runtime-linux-x64/libruntime_net.a`, built with
    `./scripts/build-runtime-linux.sh <pinned checkout>`; it has 25 members, 27
    with the database archive.
- For programs that call the database: `python scripts/build-runtime-db.py
  --orbit-root <pinned checkout> --cc C:\w64devkit\bin\gcc.exe` on Windows,
  which writes `libruntime_net_db.a` and `sqlite3.lib` beside the archive
  (D-M1-87). The executable needs `sqlite3.dll` from `runtime/vendor/win-x64`
  on `PATH`. On Linux `scripts/build-runtime-linux.sh` writes both archives,
  and the link adds the system `-lsqlite3` (D-M1-119).
- Windows link inputs: MinGW CRT and library directories from `R6_MINGW_ROOT`
  (default `C:/w64devkit`) and `R6_MINGW_GCC_VERSION` (default `16.2.0`). Only
  `ld.exe` is executed.
- Linux link inputs: glibc `crt1.o`, `crti.o`, `crtbegin.o`, `crtend.o` and
  `crtn.o`, found under `/usr/lib/x86_64-linux-gnu` and
  `/usr/lib/gcc/x86_64-linux-gnu/*/`. `R6_CRT_DIR` overrides the directory and
  `R6_LD` the linker; the default is `ld` on `PATH`.
- For the differential, the corpus and the census: a checkout of `orbit-lang`
  at the pin and the fixed point built from it. The driver build and the loader
  suite need neither.

The absolute paths in this file and in `docs/PIN.md` are observations of one
machine, not requirements.

## Quick start

```powershell
git clone https://github.com/joacokhzyx/orbit-r6
cd orbit-r6
.\scripts\build-r6.ps1                              # -> .r6-build\r6.exe

# The pinned language tree: runtime archive and oracle.
git clone https://github.com/joacokhzyx/orbit-lang C:\Temp\orbit-pin-01e31e8-git
git -C C:\Temp\orbit-pin-01e31e8-git checkout 01e31e8
python C:\Temp\orbit-pin-01e31e8-git\scripts\mklib_runtime.py `
  --cc C:\w64devkit\bin\gcc.exe `
  --out "$PWD\.r6-build\runtime-win64\libruntime_net.a"

python tests/reject/run_reject.py                    # needs no oracle

cd C:\Temp\orbit-pin-01e31e8-git
python scripts/dev.py fp --cc C:\w64devkit\bin\gcc.exe  # builds the fixed point
python scripts/dev.py fp-path                        # prints its path
```

On Linux the same steps with the shell scripts:

```bash
git clone https://github.com/joacokhzyx/orbit-r6
cd orbit-r6
./scripts/build-r6.sh                                # -> .r6-build/r6
./scripts/build-runtime-linux.sh /tmp/orbit-pin-01e31e8-git
python3 tests/reject/run_reject.py                   # needs no oracle
```

Build the same pinned checkout and fixed point; `scripts/selfhost.sh` takes the
checkout, the fixed point and the driver, in that order.

The fixed point is named `orbit-fp-<10 hex>.exe` after a hash of the checkout
path, so `dev.py fp-path` gives the name to pass as `--compiler` below.

## Build

```powershell
.\scripts\build-r6.ps1          # -> .r6-build\r6.exe
```

```bash
./scripts/build-r6.sh           # -> .r6-build/r6
```

Both scripts use `$env:R6_CC`, else `$env:CC`, else `gcc` on `PATH`, with
`-std=c11 -O2 -Wall -Wextra -Werror` (D-M1-120). One binary builds the target of
the host it was compiled on.

## Use

```powershell
.\.r6-build\r6.exe build program.ir -o program.exe [--runtime-dir <dir>] [--timings]
```

```bash
./.r6-build/r6 build program.ir -o program [--runtime-dir <dir>] [--timings]
```

`--runtime-dir <dir>` names the directory holding the runtime archives.
Lookup order (D-M1-98): the flag, then `R6_RUNTIME_DIR`, then
`.r6-build/runtime-win64` on Windows or `.r6-build/runtime-linux-x64` on Linux,
relative to the current directory. A missing archive is `error[LINK]` with the
full path tried.

On Windows a module that reads argv, uses the network or the database, or
declares migrations, keeps the MinGW start-up and its `main`; any other module
gets R6's own process entry `orbit_start` and links without the start-up objects
(D-M1-104). On Linux every program keeps `main` and the glibc start-up, because
`orbit_start` calls msvcrt and the start-up check reads a PE header (D-M1-119).

`r6 build --object <module.ir> -o <module.o>` compiles a runtime module (`"runtimeModule": true`,
D-M1-105) to an object and stops before the link. A runtime module builds only that way.

The `.s` and `.o` temporaries go to `R6_TMPDIR`, else the system temporary
directory. They are named `r6-<pid>.s` and `r6-<pid>.o` and are deleted after a
successful link.

`r6 --version` prints `r6 <semver> ir-protocol <N>` and exits 0 (D-M1-100). The language
refuses an r6 whose protocol it does not know. N bumps when the command line, the
IR r6 reads, the width of a type or the runtime ABI changes; `docs/DECISIONS.md`
lists the cases.

A diagnostic is one line: `r6: <file>:<line>:<col>: error[<CODE>]: <msg>`. Exit
2 is CLI misuse. Exit 1 is every other failure.

## Gates

The commands below were verified on 2026-10-31 on both hosts, except the
compiler-IR census, last run on 2026-10-28. `--r6-command` must be the last
argument. In a POSIX shell the command is separate argv tokens, and only the
`{ir}` and `{exe}` placeholders are quoted.

```powershell
python tests/diff/run_diff.py `
  --compiler C:\Temp\orbit-fp-ee60c8af43.exe `
  --cc C:\w64devkit\bin\gcc.exe `
  --orbit-root C:\Temp\orbit-pin-01e31e8-git `
  --r6-command .r6-build\r6.exe build '{ir}' -o '{exe}'

python tests/reject/run_reject.py
python tests/reject/make_cases.py --check
```

```bash
python3 tests/diff/run_diff.py \
  --compiler /tmp/orbit-fp-linux --cc gcc \
  --orbit-root /tmp/orbit-pin-01e31e8-git \
  --r6-command ./.r6-build/r6 build '{ir}' -o '{exe}'

python3 tests/reject/run_reject.py
python3 tests/reject/make_cases.py --check
```

The verdict of the differential is identical exit code and identical stdout
bytes against the GCC build of the Orbit oracle. Contribution rules are in
`CONTRIBUTING.md`.

`scripts/ci.ps1` runs the build, `make_cases.py --check` and the loader suite
and stops at the first failure. `-Diff` and `-Corpus` add the differential and the
corpus ratchet, and take `-Compiler`, `-OrbitRoot` and `-Cc`.
`.github/workflows/ci.yml` runs it on every push and pull request; its `oracle`
job builds the fixed point and runs on manual dispatch.

Corpus and census:

```powershell
python tests/corpus/run_corpus.py `
  --compiler C:\Temp\orbit-fp-ee60c8af43.exe `
  --cc C:\w64devkit\bin\gcc.exe `
  --orbit-root C:\Temp\orbit-pin-01e31e8-git `
  --check `
  --r6-command .r6-build\r6.exe build '{ir}' -o '{exe}'

python tests/compiler-ir/run_census.py `
  --compiler C:\Temp\orbit-fp-ee60c8af43.exe `
  --tree C:\Temp\orbit-pin-01e31e8-git `
  --r6-command .r6-build\r6.exe build '{ir}' -o '{exe}'
```

```bash
python3 tests/corpus/run_corpus.py \
  --compiler /tmp/orbit-fp-linux --cc gcc \
  --orbit-root /tmp/orbit-pin-01e31e8-git --check \
  --r6-command ./.r6-build/r6 build '{ir}' -o '{exe}'
```

The compiler-IR census has no Linux measurement; its baseline is the Windows
one in `tests/compiler-ir/baseline.json`.

The self-host:

```powershell
.\scripts\selfhost.ps1 -OrbitRoot C:\Temp\orbit-pin-01e31e8-git `
  -Compiler C:\Temp\orbit-fp-ee60c8af43.exe -R6 $PWD\.r6-build\r6.exe
```

```bash
./scripts/selfhost.sh /tmp/orbit-pin-01e31e8-git /tmp/orbit-fp-linux ./.r6-build/r6
```

Compile time. `tests/perf/run_perf.py` sums the corpus builds and times R6 on
the self-host IR, records R6's own stages next to the wall time, and compares
with `tests/perf/baseline.json` for the host platform:

```powershell
python tests/perf/run_perf.py --r6 .r6-build\r6.exe `
  --orbit-root C:\Temp\orbit-pin-01e31e8-git `
  --compiler C:\Temp\orbit-fp-ee60c8af43.exe --check
```

```bash
python3 tests/perf/run_perf.py --r6 ./.r6-build/r6 \
  --orbit-root /tmp/orbit-pin-01e31e8-git \
  --compiler /tmp/orbit-fp-linux --check
```

`--write-baseline` re-takes the entry for the host platform. It is a
measurement, not a gate, and the recorded numbers are one machine's.

## The pin

R6 is pinned to one `orbit-lang` commit: `01e31e882610cea21fbfbf4fe02fee00051fab2e`,
on the public remote `https://github.com/joacokhzyx/orbit-lang`.
The pin is the Orbit commit and the inputs listed in `docs/PIN.md`: the IR
emitter, the fixed-point compiler, the runtime recipe, the runtime symbol set
and the corpus hash.

- Pinned bytes come from `git cat-file blob 01e31e8:<path>`, never from a
  working tree that may be ahead of the pin.
- The corpus and the census regenerate from a tree materialized at the pin.
  `--orbit-root` and `--tree` must point at that tree.
- `tests/corpus/manifest.json` carries the SHA-256 of every corpus document.
  `tests/corpus/baseline.json` and `tests/compiler-ir/baseline.json` are the
  ratchets: `r6BuildOk` and `match` may not go down.
- Moving the pin takes a dated entry in `docs/DECISIONS.md`, and moves
  `docs/PIN.md`, the corpus manifest and the baseline in one commit.

## Layout

| Path | Content |
|---|---|
| `src/loader/` | Strict JSON IRv1 parser and validator; admits opcodes |
| `src/codegen-x64/` | `target.h` (the host picks the target), `win64.c` (IR to private asm text), `peephole.c` (reload removal on the asm text), `regalloc.c` (registers for hot slots), `asm_win64.c` (encoder), `coff.c` (COFF writer), `elf64.c` (ELF64 writer) |
| `src/driver/r6.c` | CLI, temporaries in `R6_TMPDIR` or the system temp, direct `ld` link |
| `scripts/` | `build-r6.ps1` and `build-r6.sh` (driver), `build-runtime-linux.sh`, `selfhost.ps1` and `selfhost.sh`, `ci.ps1` |
| `tests/diff/` | Differential harness |
| `tests/reject/` | Loader contract suite: `manifest.json` is the truth, `cases/` is rendered from it |
| `tests/corpus/` | Language-emitted corpus manifest, ratchet and runner |
| `tests/compiler-ir/` | Census of the compiler's own IR |
| `tests/elf/` | Writer-only probe for the ELF64 object writer |
| `tests/perf/` | Compile-time measurement and the per-platform baseline |
| `docs/` | `PIN.md`, `CONTRACT.md`, `DECISIONS.md`, `M1_REPORT.md`, fixtures in `proposed-ir/m1/` |
| `scripts/` | `build-r6.ps1`, `migrate-m1-fixtures.py` |

Contract prose in `docs/` is Spanish. `AGENTS.md` holds the working rules.

## License

MIT. See `LICENSE`.
