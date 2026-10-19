# AGENTS.md — R6

R6 is a native x86-64 code generator for the Orbit language IR, pinned to
`orbit-lang` `main` commit `ce87d508e8bdb7a1a0059726d3f45ecc73f8f5e3` (D-F0-12,
a runtime + harness tramo that leaves `compiler/` untouched, so the oracle,
the canonical and the emitted corpus are byte-identical to D-F0-11
`862f257`; before that: the H-ARENA landing, the Arena Oracle removal, the
loader `normalizePath` fix and the `compiler/` stage reorg). This is a
separate repo from the language: `C:\Users\Usuario\Downloads\orbit-lang` is
read-only to R6, and *its commit* is the pin — note the working tree there can
be **dirty** (a concurrent session's harness work, class (ii) `scripts/**`);
the pin rests on the commit, so read pinned bytes with `git show
ce87d508:<path>`, never from the working tree. Do not write there.

## Commands

Driver build (GCC is used only here, never for R6 programs):

```powershell
.\scripts\build-r6.ps1          # -> .r6-build\r6.exe
```

Uses `$env:R6_CC`, else `$env:CC`, else `gcc` on PATH; flags `-std=c11 -Wall
-Wextra -Werror`. This is the only build step in the repo.

Primary gate: differential — verified 35/35 on 2026-10-18 with:

```powershell
python tests/diff/run_diff.py `
  --compiler C:\Temp\orbit-fp-8b0bb38a82.exe `
  --cc C:\w64devkit\bin\gcc.exe `
  --orbit-root C:\Temp\orbit-pin-ce87d508 `
  --r6-command .r6-build\r6.exe build '{ir}' -o '{exe}'
```

- `--r6-command` must be the last argument (argparse `REMAINDER`); keep the
  `'{ir}'`/`'{exe}'` quoting in PowerShell.
- `--case 19-int-list-get` runs one fixture (repeatable). Default = every
  `.json` with a matching `.orb` in `docs/proposed-ir/m1/`.
- `--orbit-root` is a checkout **at the pinned commit**: the sibling `orbit-lang`
  tree can be ahead of the pin and keeps advancing (on 2026-10-18 it was
  `adb8b96` when the round was measured and `82cde1a5` hours later; the `C:\Temp`
  binary named `orbit-fp-26cfb87400.exe` - `sha256(checkout path)` - was rebuilt
  in place to that newer revision, so it fails the whole corpus). The pinned
  fixed point is `C:\Temp\orbit-fp-8b0bb38a82.exe` (sha256 in `docs/PIN.md`);
  `make fp` in a pinned checkout rebuilds it. Read pinned bytes with
  `git cat-file blob ce87d508:<path>`, never from the working tree.
- Verdict = identical process exit + stdout bytes vs the gcc/Orbit oracle; a
  build failure counts as a failure. `R6_CC` only affects the driver build —
  the R6 program path must never invoke GCC.
- `python tests/diff/run_diff.py --help` — remaining flags mirror the pinned
  `scripts/test_suite.py`.

Second gate: loader contract suite (`tests/reject/`, decision D-M1-06;
`map_*` pins are D-M1-10, model pins are D-M1-11, union pins are D-M1-12,
alloc/free pins are D-M1-13, function-value pins are D-M1-14, the
register-global pin is D-M1-15, edge-case pins are D-M1-16, call-target pins
are D-M1-18, literal-form pins are D-M1-19, unknown-register-type pins are
D-M1-20, inline-literal-operand pins are D-M1-21, parameter-by-name pins are
D-M1-22, the corpus round-4 string-`add` pins are D-M1-23 and the
list-length-slot fixture is D-M1-25) — verified 153/153 on
2026-10-18:

```powershell
python tests/reject/run_reject.py            # 122 rejections + 31 acceptance controls
python tests/reject/run_reject.py --case v13 # one case (stem substring)
python tests/reject/make_cases.py --check    # cases/ + manifest.json drift check
```

- `tests/reject/manifest.json` is the suite truth (expected exit, expected
  diagnostic code, spec rule); `make_cases.py` renders it together with
  `cases/` deterministically — do not hand-edit the byte-exact cases.
- Every rejection must exit 1 with exactly one
  `r6: <file>:<line>:<col>: error[CODE]: <msg>` line carrying the manifest
  code; acceptance controls build, and run with byte-compared stdout when the
  manifest declares `run_exit`/`run_stdout`.
- The runner executes the driver with the repo root as CWD (the runtime
  archive path is resolved relative to the CWD).

Corpus differential: the language-emitted corpus (`tests/corpus/`, decision
D-M1-17) — a **measurement, not a gate**. Measured **27/93** on 2026-10-18 at the
D-M1-23/24/25 round (2/89 at the D-M1-17 pin; D-M1-18 lifted it to 3/89; D-M1-19
kept 3/89 while removing `load_const` as a first refusal anywhere; D-M1-20 lifted
it to 6/89 by recovering, by inference, the register types the builder serialized
as `unknown`; the merge + re-pin lifted it to 7/93 on the grown corpus;
**unchanged at the D-F0-11 re-pin** (`862f257`, same `filesSha256 0106d531…`);
D-M1-21 lifted it to 21/93 by admitting the inline literal operand — the first
wall of 24 documents; D-M1-22 to 23/93 by admitting `load_var` of a parameter, the
whole `load_var` wall of 23 programs; and D-M1-23, with the two pre-existing
lowering defects D-M1-24/D-M1-25 (found by this round’s own measurement), to
**27/93** by admitting the string `add` — spread on one side, concat on both, in
every function — all with zero regressions):

```powershell
python tests/corpus/run_corpus.py `
  --compiler C:\Temp\orbit-fp-8b0bb38a82.exe `
  --cc C:\w64devkit\bin\gcc.exe `
  --orbit-root C:\Temp\orbit-pin-ce87d508 `
  --check `
  --r6-command .r6-build\r6.exe build '{ir}' -o '{exe}'
```

- Regenerates the 93+1 corpus with the pinned compiler (`--orbit-root` is the
  tree carrying the language's `scripts/emit_ir_corpus.py`; `--ir-dir` measures
  an already-emitted one), verifies every `sha256` against
  `tests/corpus/manifest.json`, then builds both sides per program and requires
  identical exit + stdout bytes.
- Only two things go red: corpus drift from the pin, and `--check` when
  `r6BuildOk`/`match` drop below `tests/corpus/baseline.json` (one-way
  ratchet). Failing corpus programs are the measurement, never a failure.
- `--json report.json` keeps the per-case detail; `--write-baseline` re-takes
  the baseline after the check passes.
- Trust the ratchet when the oracle looks broken: a whole-corpus
  `oracle-fail` (with `r6BuildOk 0`) means the *language* side failed to
  build, not R6 — the check then refuses to write a baseline. That happened
  on 2026-10-17 while the checkout's C-cache wrapper was mid-edit (a quoted
  multi-token `ORBIT_CC` the pinned fixed point cannot spawn:
  `ERROR_INVALID_NAME`); `ORBIT_CCACHE=0`, the harness's own escape hatch,
  takes the wrapper out of one run and is what the D-M1-21 round used.

Probe binaries (historical COFF/assembler probes, not part of the gate):

```powershell
gcc -std=c11 -Wall -Wextra -Werror tests/coff/probe.c src/codegen-x64/coff.c -o .r6-build\coff-probe.exe
# asm_probe.c is the same but adds src/codegen-x64/asm_win64.c; argv: <in.s> <out.o>
```

## Running the driver

- `r6 [build] <input.ir> -o <output.exe>`. Run it from the repo root: temp
  `.r6-build\r6-<pid>.s/.o` files and the runtime archive path are resolved
  relative to the CWD (and the temps are left behind, not cleaned up).
- Needs `.r6-build\runtime-win64\libruntime_net.a`, which is **gitignored and
  not in the repo**. A fresh clone must rebuild it from the pinned checkout
  with its `scripts/mklib_runtime.py --out ...` (expected hash/symbols in
  `docs/PIN.md`), otherwise linking fails with `error[LINK]`.
- MinGW CRT/lib dirs come from `R6_MINGW_ROOT` (default `C:/w64devkit`) and
  `R6_MINGW_GCC_VERSION` (default `16.2.0`) — packaging paths only; just
  `ld.exe` is executed.
- Diagnostics are one line: `r6: <file>:<line>:<col>: error[<CODE>]: <msg>`;
  exit 2 = CLI misuse, 1 = everything else. There is no CI in this repo.

## Hard rules (docs/DECISIONS.md, docs/CONTRACT.md)

- Fail closed: any opcode, version, type or platform not explicitly supported
  gets a diagnostic and nonzero exit. No stubs, invented semantics or fallback.
- Programs are never lowered through C and never assembled by GAS/GCC:
  `win64.c` emits a private GAS-shaped text, `asm_win64.c` encodes it to bytes,
  `coff.c` writes AMD64 COFF; GNU `ld` is only the linker.
- Never write to the Orbit checkout; moving the pin requires a dated entry in
  `docs/DECISIONS.md`.
- Do not generalize semantics that pass in fixtures beyond what
  `docs/M1_REPORT.md` states is measured (overflow, NaN payloads, other list
  kinds, Linux/macOS are currently out of evidence).

## Layout

- `src/loader/ir.{h,c}` — strict JSON IRv1 parser/validator, owns `R6Module`;
  also gates which opcodes are admitted (`E_UNSUPPORTED` outside the range).
- `src/codegen-x64/win64.{c,h}` — IR → private asm text; `asm_win64.{c,h}` —
  encodes only the selector's instruction subset; `coff.{c,h}` — AMD64 COFF
  writer (sections, symbols, REL32/ADDR32NB relocations, unwind data).
- `src/driver/r6.c` — CLI, temporaries under `.r6-build`, direct `ld` link.
- `tests/diff/run_diff.py` — differential harness; `tests/coff/*.c` — probes
  that produced `docs/COFF_OBJECTS.md`.
- `tests/reject/` — loader contract suite: `run_reject.py` (second gate),
  `make_cases.py` (deterministic case builder), `cases/` + `manifest.json`.
- `scripts/` — `build-r6.ps1` (driver build), `migrate-m1-fixtures.py`
  (one-shot D-F0-06 fixture converter; `--check` re-verifies against the
  pre-migration revision, `06d7d2c^`, skipping fixtures newer than the base).
- `docs/` — `PIN.md` (pinned inputs + hashes), `CONTRACT.md` (module
  interfaces), `IR_SPEC_DRAFT.md` (superseded draft grammar, kept for
  reference), `IR_SPEC_ALIGNMENT.md` (adopted spec vs implementation, D-F0-06),
  `DECISIONS.md` (dated rulings), `M1_PLAN.md`/`M1_REPORT.md` (plan/evidence).
  Contract prose is Spanish; match the file you edit.

## Adding IR support

1. Fixtures are hand-written `.json` + `.orb` pairs in `docs/proposed-ir/m1/`;
   they predate the emitter and stay manual. The pinned language does emit its
   own corpus with `orbit build --emit-ir` (`tests/corpus/`, D-F0-10).
2. **Format (migrated 2026-10-15, commit `06d7d2c`)**: the loader implements
   the language-side `orbit-lang/docs/IR_SPEC.md` format (blob `371f873e…`):
   root `{"irVersion":"1","module":M}`, instructions
   `opcode`/`dest`/`operand1..3`/`elemKind`, values `{"kind":K,"value":V}`
   (`{"kind":"None"}` bare), `dest` `-1` for none, flat `route*` fields, ints
   and floats as JSON numbers (floats must be finite, V-08). The draft
   serialization (`{"format":"orbit-ir","version":1,...}`) is rejected with
   `E_FORMAT`; there is no dual acceptance (D-F0-06/D-F0-07 in
   `DECISIONS.md`, extraction in `IR_SPEC_ALIGNMENT.md`). Keep `.json`/`.orb`
   LF (pinned by `.gitattributes`); fixtures convert mechanically with
   `python scripts/migrate-m1-fixtures.py --check` (re-verifies against the
   pre-migration revision, `06d7d2c^`).
3. Support for something new = loader's allowed-opcode range + codegen +
   fixture pair + the table in `docs/proposed-ir/m1/README.md`, then record the
   decision and differential count in `DECISIONS.md` / `M1_REPORT.md`.
4. Commit style follows history: `feat: lower <thing> (N/N diff)`, `fix:`,
   `test:`, `docs:`.
