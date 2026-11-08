<p align="center">
  <img src="assets/banner.png" alt="Orbit R6" width="720">
</p>

# Orbit R6

**The native backend for the Orbit language.**

R6 reads Orbit's intermediate representation and writes an executable for your machine: a Linux ELF or a Windows PE. It selects instructions, allocates registers and encodes the machine code itself. No C compiler ever sees your program.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Targets](https://img.shields.io/badge/targets-Linux%20x64%20%7C%20Windows%20x64-informational)](docs/CONTRACT.md)
[![Part of Orbit](https://img.shields.io/badge/part%20of-orbit--lang-blueviolet)](https://github.com/joacokhzyx/orbit-lang)

## Why R6 exists

Today `orbit build` finishes through C: the Orbit compiler emits C, and GCC or clang produces the binary. That keeps the compiler small and portable. It also ties every build to a C toolchain, and for the compiler itself the C compiler is the largest cost of a build ([measurements](https://github.com/joacokhzyx/orbit-lang/blob/develop/docs/PERF.md)).

R6 is the way out. It owns the whole path from Orbit's IR to a running binary, so Orbit can build fast, build anywhere a linker exists, and answer for every byte it produces.

## How it works

```text
 your program.orb
       |
       |  orbit build --emit-ir
       v
   IR (JSON)  ──►  loader  ──►  code generator  ──►  object file  ──►  GNU ld  ──►  executable
                   checks       selects, allocates,    COFF or ELF
                   every        optimizes, encodes
                   field
```

- **Loader** (`src/loader`) parses the IR and validates it strictly. Anything outside the supported set is rejected with one clear diagnostic.
- **Code generator** (`src/codegen-x64`) lowers IR to a private assembly text, improves it (`peephole.c`, `regalloc.c`), and encodes it (`asm_win64.c`).
- **Object writers** (`coff.c`, `elf64.c`) write the object file. GNU `ld` links it. That is the only external tool on the program path.
- **Driver** (`src/driver`) is the `r6` command-line tool.

## Run it

You need Windows x64 or Linux x64, Python 3, and GCC. GCC is used only to build the R6 driver and the Orbit runtime library, never to build an R6 program.

**Linux**

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

**Windows (PowerShell)**

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

Both print `M1-add` and exit with status 42. That is the first of 100 test programs in `tests/diff/fixtures/`. The runtime library is not committed to this repository: it is rebuilt from the pinned Orbit checkout, which is why the steps clone `orbit-lang` first.

## What has been checked

Measured on 2026-10-31 against the pinned Orbit commit `01e31e8`, on Windows x64 and Linux x64.

| Check | What it proves | Windows x64 | Linux x64 |
|---|---|---|---|
| Differential | 100 programs built with R6 and with the pinned compiler through GCC give the same exit code and the same stdout bytes | 100/100 | 100/100 |
| Loader contract | 311 malformed inputs are rejected and 141 valid ones are accepted | 452/452 | 452/452 |
| Corpus | 143 programs that the compiler emits from its own tests and standard library | 143 build, 143 match | 143 build, 143 match |
| Self-host | R6 builds the Orbit compiler, and that compiler reproduces its own C byte for byte | SELFHOST OK | SELFHOST OK |

The self-host result is the one to look at first. The compiler that R6 produces translates its own source to C that hashes to the pin's published value:

```text
E794E2DBF070AD5EA04DC2617E888694D37069896AC3B67BE2A22A9D857E92B1
```

Against the previous driver, wall time over the corpus dropped 18.5% on Windows and 19.1% on Linux. The self-host R6 build dropped 34.7% and 42.0%. The corpus and compile-time baselines are one-way ratchets: a change may improve them and may never make them worse.

## What R6 guarantees

For every program it accepts:

1. **Same behavior.** Exit code and stdout bytes equal those of the pinned Orbit compiler built through GCC. The differential and the corpus enforce this.
2. **No silent fallback.** An opcode, type, IR version or platform outside the supported set gets one diagnostic and a nonzero exit. R6 never stubs a feature or invents semantics.
3. **No C compiler on the program path.** `win64.c` emits assembly text, `peephole.c` and `regalloc.c` rewrite it, `asm_win64.c` encodes it, `coff.c` or `elf64.c` writes the object, and GNU `ld` links it.
4. **One target per binary.** The target is fixed when the driver is compiled (`src/codegen-x64/target.h`). There is no `--target` flag and no cross build.

## What R6 does not do yet

- **Services.** A program that declares a `route` is refused with a compile error. A service build still needs `orbit build --backend=c`.
- **Databases.** The `db_*` opcodes are excluded.
- **Other IR versions.** R6 reads IRv1 at the pin and rejects anything else.
- **Everything outside the measured subset.** Integer overflow beyond the fixtures, NaN payloads beyond fixture 10 and list kinds other than `int` have no evidence behind them. [`docs/CONTRACT.md`](docs/CONTRACT.md#measured-subset) lists the subset exactly.
- **Other platforms.** macOS, ARM64 and threading have no measurement. Benchmarks have not been run.
- **The compiler's own modules, one by one.** The census in `tests/compiler-ir/run_census.py` last measured 2 of 27 modules building on their own (2026-10-28).

A refusal is always a diagnostic. R6 does not produce a binary it cannot stand behind.

## Where R6 is going

R6 exists so that Orbit can leave C behind. The work ahead, in the order it unlocks things:

1. **Routes.** Compile services natively, so the programs Orbit was designed for need no C compiler at all.
2. **Database access.** Admit the `db_*` opcodes once the runtime exposes them to R6.
3. **ARM64 and macOS.** Take Orbit beyond x86-64 Windows and Linux.
4. **Cross compilation.** A `--target` flag, replacing the one-target-per-binary rule.
5. **Published benchmarks.** Compile time and run time against other toolchains, recorded with the method in [`docs/PIN.md`](docs/PIN.md) and the machine they ran on.

Each step arrives the way every R6 change does: one decision, its test programs, its evidence.

## Command line

```text
r6 [build] <input.ir> -o <output> [--runtime-dir <dir>] [--timings]
r6 build --object <module.ir> -o <module.o>
r6 --version
```

`--runtime-dir` names the folder that holds the runtime archives. R6 looks in the flag, then `R6_RUNTIME_DIR`, then `.r6-build/runtime-win64` on Windows or `.r6-build/runtime-linux-x64` on Linux. A missing archive is `error[LINK]` with the full path R6 tried.

`--object` compiles a runtime module and stops before the link. `--version` prints `r6 <semver> ir-protocol <N>`.

A diagnostic is one line: `r6: <file>:<line>:<col>: error[<CODE>]: <message>`. Exit code 2 means the command was misused. Exit code 1 covers every other failure.

## Documentation

| If you want to know | Read |
|---|---|
| Where everything is | [`docs/README.md`](docs/README.md) |
| How to build, run the checks and contribute | [`CONTRIBUTING.md`](CONTRIBUTING.md) |
| Exactly what R6 accepts | [`docs/CONTRACT.md`](docs/CONTRACT.md) |
| Which Orbit commit R6 is tested against | [`docs/PIN.md`](docs/PIN.md) |
| Why each design choice was made | [`docs/DECISIONS.md`](docs/DECISIONS.md) |
| How branches and releases work | [`docs/BRANCHES.md`](docs/BRANCHES.md) |

## Repository layout

| Path | Content |
|---|---|
| `src/loader/` | Strict IRv1 parser and validator |
| `src/codegen-x64/` | Code generator, register allocator, peephole pass, encoder, COFF and ELF writers |
| `src/driver/` | The `r6` command and the link step |
| `tests/diff/` | The 100 test programs and the differential harness |
| `tests/reject/` | The loader contract suite |
| `tests/corpus/` | Programs emitted by the Orbit compiler, with manifest and baseline |
| `tests/perf/` | Compile-time measurement and baselines |
| `scripts/` | Driver build, runtime build, self-host and CI |
| `docs/` | Pin, contract, decisions and index |

## Contributing

A contribution arrives with its test program or rejection case, a dated decision and its evidence. Start with [`CONTRIBUTING.md`](CONTRIBUTING.md). Run the checks on both platforms before you send a change.

## License

MIT. See [`LICENSE`](LICENSE).