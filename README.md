# Orbit R6

R6 turns the IR that the Orbit compiler emits into a native executable. It
writes the machine code and the object file itself, and calls the GNU linker.
No C compiler and no assembler run while a program is built.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

![Orbit Banner](assets/banner.png)

## What it is

Orbit writes its intermediate representation as JSON IR (`orbit build
--emit-ir`). R6 reads that file and produces an executable for the host: a
Windows x64 PE on Windows, a Linux x64 ELF on Linux. It validates the module,
translates each instruction to x86-64, and writes a COFF or ELF64 object for
`ld`.

R6 also builds the Orbit compiler itself. The compiler it produces translates
its own source to C byte-identical to the C the GCC-built compiler emits, and
that C hashes to
`E794E2DBF070AD5EA04DC2617E888694D37069896AC3B67BE2A22A9D857E92B1`, the value
the pin publishes.

## Try it in five minutes

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

Both runs print `M1-add` and exit with status 42.

## What it guarantees

- The exit code and stdout bytes equal the pinned Orbit compiler's GCC build.
  The differential and the corpus check this.
- An opcode, type, version or platform outside the implemented set produces
  one diagnostic and a nonzero exit. No stubs, no invented semantics and no
  fallback.
- The program path runs no C compiler and no assembler.
- One binary builds one target, chosen when the driver is compiled. There is
  no `--target` flag and no cross build.

What it does not cover:

- IRv1 at the pin only. A different IR version is rejected.
- The measured subset in [`docs/CONTRACT.md`](docs/CONTRACT.md#measured-subset).
  Integer overflow outside the fixtures, NaN payloads beyond fixture 10, and
  other list kinds are not evidence.
- macOS, ARM64, threading and benchmarks. No measurement exists for them.
- The compiler-IR census in `tests/compiler-ir/baseline.json` builds 2 of 27
  modules. The other 25 declare no non-extern `main`.

## Status

Measured at the pin `01e31e8` on Windows x64 (2026-10-31) and Linux x64
(2026-11-01):

| Check | Windows x64 | Linux x64 | Kind |
|---|---|---|---|
| Differential (`tests/diff/run_diff.py`) | 100/100 | 100/100 | gate |
| Loader contract (`tests/reject/run_reject.py`) | 452/452 | 452/452 | gate |
| Corpus (`tests/corpus/run_corpus.py`) | 143 build / 143 match | 143 build / 143 match | measurement, one-way ratchet |
| Self-host (`scripts/selfhost.ps1`, `selfhost.sh`) | SELFHOST OK | SELFHOST OK | measurement |
| Compile time (`tests/perf/run_perf.py`) | `tests/perf/baseline.json` | `tests/perf/baseline.json` | measurement, one-way ratchet |

The differential builds 100 fixture pairs with R6 and with the pinned Orbit
compiler through GCC, then requires identical exit code and stdout bytes. The
loader suite runs 311 rejections and 141 acceptance controls, and
`tests/reject/manifest.json` holds that count. The corpus is the IR the pin
emits over its own test and standard-library programs: 143 executable
documents and one support module.

## Performance

The 143-program corpus takes 5,418.6 ms end to end on Windows x64, and R6
spends 447.0 ms of that. The self-host build takes 1,181.1 ms with the link;
R6 spends 1,086.3 ms of it. On Linux x64 the corpus total is 3,060.7 ms, R6
spends 331.6 ms of it, and the self-host build takes 943.6 ms, of which R6
spends 881.9 ms. The linker dominates the corpus time; the numbers and their
method are in `tests/perf/baseline.json`.

On Windows x64 the self-host compile pipeline fell from 2,187.2 ms to
1,147.733 ms between the start of the performance work and its end
(2026-10-31, measure.py, 7 reps, three runs per driver).

## Use

```text
r6 [build] <input.ir> -o <output> [--runtime-dir <dir>] [--timings]
r6 build --object <module.ir> -o <module.o>
r6 --version
```

`--runtime-dir` names the directory holding the runtime archives. The lookup
order is the flag, then `R6_RUNTIME_DIR`, then `.r6-build/runtime-win64` on
Windows or `.r6-build/runtime-linux-x64` on Linux, relative to the current
directory. A missing archive produces `error[LINK]` with the full path tried.
`--object` compiles a runtime module and stops before the link.

The `.o` temporary goes to `R6_TMPDIR`, else the system temporary directory,
and a successful link deletes it. The assembler text stays in memory.

A diagnostic is one line, `r6: <file>:<line>:<col>: error[<CODE>]: <msg>`.
Exit 2 is CLI misuse and exit 1 is every other failure. `--version` prints
`r6 0.1.0 ir-protocol 3`.

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
| `src/codegen-x64/` | `target.h`, `win64.c`, `text.{h,c}`, `peephole.c`, `regalloc.c`, `asm_win64.c`, `coff.c`, `elf64.c` |
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
