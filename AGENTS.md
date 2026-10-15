# AGENTS.md — R6

R6 is a native x86-64 code generator for the Orbit language IR, pinned to
`orbit-lang` commit `5c01cea68f125f06dd3ac82a61776827bdb0c6f3`. This is a
separate repo from the language: `C:\Users\Usuario\Downloads\orbit-lang` is
read-only to R6 and is currently a live dev tree (HEAD ahead of the pin, dirty
files present) — never treat that working tree as the pin.

## Commands

Driver build (GCC is used only here, never for R6 programs):

```powershell
.\scripts\build-r6.ps1          # -> .r6-build\r6.exe
```

Uses `$env:R6_CC`, else `$env:CC`, else `gcc` on PATH; flags `-std=c11 -Wall
-Wextra -Werror`. This is the only build step in the repo.

Differential gate — the only test suite; verified 20/20 on 2026-10-15 with:

```powershell
python tests/diff/run_diff.py `
  --compiler C:\Temp\orbit-fp-26cfb87400.exe `
  --cc C:\w64devkit\bin\gcc.exe `
  --r6-command .r6-build\r6.exe build '{ir}' -o '{exe}'
```

- `--r6-command` must be the last argument (argparse `REMAINDER`); keep the
  `'{ir}'`/`'{exe}'` quoting in PowerShell.
- `--case 19-int-list-get` runs one fixture (repeatable). Default = every
  `.json` with a matching `.orb` in `docs/proposed-ir/m1/`.
- `--orbit-root` defaults to the sibling `orbit-lang` checkout; `--compiler` is
  a fixed-point build of the pinned commit (M1_REPORT.md records the SHA it was
  verified against; `make fp` in the checkout rebuilds it).
- Verdict = identical process exit + stdout bytes vs the gcc/Orbit oracle; a
  build failure counts as a failure. `R6_CC` only affects the driver build —
  the R6 program path must never invoke GCC.
- `python tests/diff/run_diff.py --help` — remaining flags mirror the pinned
  `scripts/test_suite.py`.

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
- `scripts/` — `build-r6.ps1` (driver build), `migrate-m1-fixtures.py`
  (one-shot D-F0-06 fixture converter; `--check` compares against HEAD).
- `docs/` — `PIN.md` (pinned inputs + hashes), `CONTRACT.md` (module
  interfaces), `IR_SPEC_DRAFT.md` (IRv1 grammar + per-opcode gaps),
  `DECISIONS.md` (dated rulings), `M1_PLAN.md`/`M1_REPORT.md` (plan/evidence).
  Contract prose is Spanish; match the file you edit.

## Adding IR support

1. Fixtures are hand-written `.json` + `.orb` pairs in `docs/proposed-ir/m1/`;
   Orbit has no `--emit-ir` even at its HEAD, so nothing generates them yet.
2. **Format (migrated 2026-10-15, commit `06d7d2c`)**: the loader implements
   the language-side `orbit-lang/docs/IR_SPEC.md` format (blob `110ce439…`):
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
