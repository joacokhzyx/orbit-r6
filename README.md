# Orbit R6

R6 is a native x86-64 code generator for the Orbit language IR. It reads the
JSON IR that `orbit build --emit-ir` writes and produces an executable for the
host: a Windows x64 PE on Windows and a Linux x64 ELF on Linux. GCC never
compiles an R6 program.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

![Orbit Banner](assets/banner.png)

## Status

Measured on 2026-10-31 at the pin `01e31e8`, on Windows x64 and Linux x64:

| Check | Windows x64 | Linux x64 | Kind |
|---|---|---|---|
| Differential (`tests/diff/run_diff.py`) | 100/100 | 100/100 | gate |
| Loader contract (`tests/reject/run_reject.py`) | 452/452 | 452/452 | gate |
| Corpus (`tests/corpus/run_corpus.py`) | 143 build / 143 match | 143 build / 143 match | measurement, one-way ratchet |
| Self-host (`scripts/selfhost.ps1`, `selfhost.sh`) | SELFHOST OK | SELFHOST OK | measurement |
| Compile time (`tests/perf/run_perf.py`) | `tests/perf/baseline.json` | `tests/perf/baseline.json` | measurement, one-way ratchet |

The differential builds 100 fixture pairs with R6 and with the pinned Orbit
compiler through GCC, then requires identical exit code and stdout bytes. The
loader suite runs 311 rejections and 141 acceptance controls; `tests/reject/manifest.json`
is that count. The corpus is the IR the pin emits over its own test and
standard-library programs. The compiler-IR census (`tests/compiler-ir/run_census.py`)
was last measured on 2026-10-28: 2 modules build out of 27. The pin and the
hash of every input are in [`docs/PIN.md`](docs/PIN.md).

R6 builds the Orbit compiler itself. The compiler it produces translates its
own source to C byte-identical to the GCC-built compiler, and the C hashes to
the pin's `PUBLISHED_C`, `E794E2DBF070AD5EA04DC2617E888694D37069896AC3B67BE2A22A9D857E92B1`
(D-M1-63). The current driver also measures 18.5% less wall time over the
corpus and 34.7% less on the self-host R6 build than the pre-optimization
driver on Windows x64; on Linux x64 the figures are 19.1% and 42.0% (D-M1-122).

## Quick start

You need Windows x64 or Linux x64, Python 3, and GCC for the driver build only.
The runtime archive is not in the repository; it is rebuilt from the pinned
Orbit checkout. Replace the absolute paths with your own.

Windows:

```powershell
git clone https://github.com/joacokhzyx/orbit-r6
cd orbit-r6
.\scripts\build-r6.ps1

git clone https://github.com/joacokhzyx/orbit-lang C:\Temp\orbit-pin
git -C C:\Temp\orbit-pin checkout 01e31e8
python C:\Temp\orbit-pin\scripts\mklib_runtime.py `
  --cc C:\w64devkit\bin\gcc.exe `
  --out "$PWD\.r6-build\runtime-win64\libruntime_net.a"

.\.r6-build\r6.exe build tests\diff\fixtures\01-int-call.json -o hello.exe
.\hello.exe; $LASTEXITCODE
```

Linux:

```bash
git clone https://github.com/joacokhzyx/orbit-r6
cd orbit-r6
./scripts/build-r6.sh

git clone https://github.com/joacokhzyx/orbit-lang /tmp/orbit-pin
git -C /tmp/orbit-pin checkout 01e31e8
./scripts/build-runtime-linux.sh /tmp/orbit-pin

./.r6-build/r6 build tests/diff/fixtures/01-int-call.json -o hello
./hello; echo $?
```

Both runs print `M1-add` and exit with status 42. That program is the first
fixture of the differential; the other 99 are in `tests/diff/fixtures/`.
Building from source and running the gates is in [`CONTRIBUTING.md`](CONTRIBUTING.md).

## What it guarantees, and what it does not

R6 guarantees four things for the programs it accepts:

- The exit code and stdout bytes equal the pinned Orbit compiler's GCC build.
  The differential and the corpus enforce this.
- An opcode, type, version or platform outside the implemented set gets one
  diagnostic and a nonzero exit. No stubs, no invented semantics, no fallback.
- The program path runs no C compiler and no assembler. `src/codegen-x64/win64.c`
  emits a private assembly text, `peephole.c` and `regalloc.c` rewrite it,
  `asm_win64.c` encodes it, `coff.c` or `elf64.c` writes the object, and GNU
  `ld` links it.
- One binary builds one target, chosen when the driver is compiled
  (`src/codegen-x64/target.h` reads `_WIN32`). There is no `--target` flag and
  no cross build (D-M1-119).

What it does not cover:

- IRv1 at the pin only. A different IR version is rejected.
- The measured subset in [`docs/CONTRACT.md`](docs/CONTRACT.md#measured-subset).
  Integer overflow outside the fixtures, NaN payloads beyond fixture 10, and
  other list kinds are not evidence.
- macOS, ARM64, threading and benchmarks. No measurement exists for them.

## Use

```text
r6 [build] <input.ir> -o <output> [--runtime-dir <dir>] [--timings]
r6 build --object <module.ir> -o <module.o>
r6 --version
```

`--runtime-dir` names the directory holding the runtime archives. Lookup order
is the flag, then `R6_RUNTIME_DIR`, then `.r6-build/runtime-win64` on Windows
or `.r6-build/runtime-linux-x64` on Linux, relative to the current directory
(D-M1-98). A missing archive is `error[LINK]` with the full path tried.

`--object` compiles a runtime module (`"runtimeModule": true`, D-M1-105) and
stops before the link. A runtime module builds only that way.

On Windows a module with no argv, network, database or migrations gets R6's
own entry `orbit_start` and links without the MinGW start-up objects; any other
module keeps `main` (D-M1-104). On Linux every program keeps `main` and the
glibc start-up (D-M1-119). The `.o` temporary goes to `R6_TMPDIR`, else the
system temporary directory, and a successful link deletes it. The assembler
text stays in memory (D-M1-134).

A diagnostic is one line, `r6: <file>:<line>:<col>: error[<CODE>]: <msg>`.
Exit 2 is CLI misuse and exit 1 is every other failure. `--version` prints
`r6 <semver> ir-protocol <N>`; the protocol number changes with the command
line, the IR R6 reads, a type width or the runtime ABI (D-M1-100, D-M1-107).

The interfaces between loader, codegen and driver are in
[`docs/CONTRACT.md`](docs/CONTRACT.md).

## Documentation

| Question | Document |
|---|---|
| Where is everything? | [`docs/README.md`](docs/README.md) |
| How do I build, gate and contribute? | [`CONTRIBUTING.md`](CONTRIBUTING.md) |
| What exactly does R6 accept? | [`docs/CONTRACT.md`](docs/CONTRACT.md) |
| Which Orbit commit, and which hashes? | [`docs/PIN.md`](docs/PIN.md) |
| Why is it built this way? | [`docs/DECISIONS.md`](docs/DECISIONS.md) |
| How do agents work in this tree? | [`AGENTS.md`](AGENTS.md) |

## Repository layout

| Path | Content |
|---|---|
| `src/loader/` | Strict JSON IRv1 parser and validator; admits the opcode set |
| `src/codegen-x64/` | `target.h`, `win64.c`, `peephole.c`, `regalloc.c`, `asm_win64.c`, `coff.c`, `elf64.c` |
| `src/driver/` | CLI, temporaries, direct `ld` link |
| `tests/diff/` | Differential harness and the 100 fixture pairs in `fixtures/` |
| `tests/reject/` | Loader contract suite; `manifest.json` is the truth |
| `tests/corpus/` | Language-emitted corpus, manifest and ratchet |
| `tests/compiler-ir/` | Census of the compiler's own IR |
| `tests/perf/` | Compile-time measurement and per-platform baseline |
| `tests/elf/`, `tests/coff/` | Writer probes for the ELF64 and COFF object writers |
| `scripts/` | Driver build, runtime build, self-host, CI |
| `docs/` | Pin, contract, decisions, index |

## Contributing

A contribution arrives with its fixture or reject case, its dated decision and
its evidence. Start with [`CONTRIBUTING.md`](CONTRIBUTING.md); the working rules
for agents are in [`AGENTS.md`](AGENTS.md). Run the gates before sending a
change. Both platforms stay green.

## License

MIT. See [`LICENSE`](LICENSE).
