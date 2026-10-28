# AGENTS.md — R6

R6 is a native x86-64 code generator for the Orbit language IR, pinned to
`orbit-lang` `main` commit `e34833af5a8fae76bd12c1fdb1ac65be6b002659` (D-F0-18,
26 commits past D-F0-17 `0cfce74`: the emitted IR is complete and typed, with six
bitwise opcodes, an extern entry for every runtime symbol and `void*` for an
untyped register; the corpus is 135+1 documents).
The commit is on the public remote `https://github.com/joacokhzyx/orbit-lang`.
This is a separate repo from the language:
`C:\Users\Usuario\Downloads\orbit-lang` is read-only to R6, and *its commit* is
the pin — note the working tree there can be **ahead** of the pin and
**dirty**; the pin rests on the commit, so read pinned bytes with
`git cat-file blob e34833a:<path>`, never from the working tree. Do not write
there.

The `orbit-lang` writing and commit rules apply here, adapted to the pin and
the corpus; they are in Writing, Commits, The read-only reviewer and When to
stop below.

## Commands

Driver build (GCC is used only here, never for R6 programs):

```powershell
.\scripts\build-r6.ps1          # -> .r6-build\r6.exe
```

Uses `$env:R6_CC`, else `$env:CC`, else `gcc` on PATH; flags `-std=c11 -Wall
-Wextra -Werror`. This is the only build step in the repo.

Primary gate: differential — verified 92/92 on 2026-10-27 at the D-F0-18 pin with:

```powershell
python tests/diff/run_diff.py `
  --compiler C:\Temp\orbit-fp-c9c04bc393.exe `
  --cc C:\w64devkit\bin\gcc.exe `
  --orbit-root C:\Temp\orbit-pin-e34833a-git `
  --r6-command .r6-build\r6.exe build '{ir}' -o '{exe}'
```

- `--r6-command` must be the last argument (argparse `REMAINDER`); keep the
  `'{ir}'`/`'{exe}'` quoting in PowerShell.
- `--case 19-int-list-get` runs one fixture (repeatable). Default = every
  `.json` with a matching `.orb` in `docs/proposed-ir/m1/`.
- `--orbit-root` is a checkout **at the pinned commit**: the sibling `orbit-lang`
  tree can be ahead of the pin and keeps advancing, and a `C:\Temp` fixed point
  named `orbit-fp-<sha256(checkout path)>.exe` is rebuilt in place when that
  path moves, so it then fails the whole corpus. The pinned fixed point is
  `C:\Temp\orbit-fp-c9c04bc393.exe` (sha256 in `docs/PIN.md`), built from
  `C:\Temp\orbit-pin-e34833a-git` with `python scripts/dev.py fp`, which
  rebuilds it in a pinned checkout. Read
  pinned bytes with `git cat-file blob e34833a:<path>`, never from the working
  tree.
- Verdict = identical process exit + stdout bytes vs the gcc/Orbit oracle; a
  build failure counts as a failure. `R6_CC` only affects the driver build —
  the R6 program path must never invoke GCC.
- `python tests/diff/run_diff.py --help` — remaining flags mirror the pinned
  `scripts/test_suite.py`.

Second gate: loader contract suite (`tests/reject/`, decision D-M1-06). Each
case in `tests/reject/manifest.json` names the decision it pins in its `rule`
field, and `docs/DECISIONS.md` holds the decisions. Verified 303/303 on
2026-10-27:

```powershell
python tests/reject/run_reject.py            # 202 rejections + 101 acceptance controls
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
D-M1-17) — a **measurement, not a gate**. Measured **135 build / 135 match over 135
programs** on 2026-10-27 at the D-F0-18 pin: 136 documents, 6 850 719 bytes,
`filesSha256 1f2c44a4…`. The rounds that took the count from 2/89 at the
D-M1-17 pin to there are in `docs/M1_REPORT.md`:

```powershell
python tests/corpus/run_corpus.py `
  --compiler C:\Temp\orbit-fp-c9c04bc393.exe `
  --cc C:\w64devkit\bin\gcc.exe `
  --orbit-root C:\Temp\orbit-pin-e34833a-git `
  --check `
  --r6-command .r6-build\r6.exe build '{ir}' -o '{exe}'
```

- Regenerates the 135+1 corpus with the pinned compiler (`--orbit-root` is the
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
- The oracle's exit, stdout and stderr are cached in
  `.r6-build\oracle-cache.json`, shared with `run_diff.py`. The corpus keys
  an entry by the manifest sha256, the fixed point sha256 and the program
  path; the differential keys it by the `.orb`+`.json` pair and the fixed
  point. `--no-oracle-cache` rebuilds every oracle. On 2026-10-28 the corpus
  took 120 s without the cache and 37 s with it; run_diff.py took 66 s and
  18 s.
- `--axes` adds six axes per matching program: correctness, section bytes,
  best-of-5 wall ms, best-of-5 R6 build ms, peak working set, and a CPU-time
  energy proxy. `--check-axes` compares them with
  `tests/corpus/axes-baseline.json`; `--write-axes-baseline` re-takes it.

Self-host check: R6 builds the pinned compiler, and that compiler's C must
hash to `PUBLISHED_C` in the pin's `scripts/verify_seed.py` (20 s):

```powershell
.\scripts\selfhost.ps1 -OrbitRoot C:\Temp\orbit-pin-e34833a-git `
  -Compiler C:\Temp\orbit-fp-c9c04bc393.exe -R6 $PWD\.r6-build\r6.exe
```

`scripts\ci.ps1 -Selfhost` runs it as a CI stage. `scripts\bisect.ps1` is the
`git bisect run` verdict: driver build (125 when it does not compile),
`run_reject.py`, `run_diff.py` and the self-host (1 at the first failure).
Copy it with `selfhost.ps1` outside the tree first; older commits lack both.

Compiler-IR census (D-M1-34) — a measurement, not a gate. It emits every
`compiler/**/*.orb` at the pin and records R6's first refusal on each; the
ratchet in `tests/compiler-ir/baseline.json` only forbids `r6BuildOk` going
down:

```powershell
python tests/compiler-ir/run_census.py `
  --compiler <fixed point of the pin, docs/PIN.md> `
  --tree <checkout at that commit> `
  --r6-command .r6-build\r6.exe build '{ir}' -o '{exe}'
```

Probe binaries (historical COFF/assembler probes, not part of the gate):

```powershell
gcc -std=c11 -Wall -Wextra -Werror tests/coff/probe.c src/codegen-x64/coff.c -o .r6-build\coff-probe.exe
# asm_probe.c is the same but adds src/codegen-x64/asm_win64.c; argv: <in.s> <out.o>
```

## Running the driver

- `r6 [build] <input.ir> -o <output.exe>`. Run it from the repo root: temp
  `.r6-build\r6-<pid>.s/.o` files and the runtime archive path are resolved
  relative to the CWD. A successful build deletes the temps; a failed one
  keeps them.
- `--timings` or `R6_TIMINGS=1` prints one stderr line per stage: load,
  codegen, peephole, regalloc, asm, coff, ld.
- Needs `.r6-build\runtime-win64\libruntime_net.a`, which is **gitignored and
  not in the repo**. A fresh clone must rebuild it from the pinned checkout
  with its `scripts/mklib_runtime.py --out ...` (expected hash/symbols in
  `docs/PIN.md`), otherwise linking fails with `error[LINK]`.
- A module with an `orbit_db_*` extern links `libruntime_net_db.a` and
  `sqlite3.lib` from the same directory, built by `python
  scripts/build-runtime-db.py --orbit-root <pin checkout>` (D-M1-87); the
  executable finds `sqlite3.dll` on `PATH` (`runtime/vendor/win-x64`).
- MinGW CRT/lib dirs come from `R6_MINGW_ROOT` (default `C:/w64devkit`) and
  `R6_MINGW_GCC_VERSION` (default `16.2.0`) — packaging paths only; just
  `ld.exe` is executed.
- Diagnostics are one line: `r6: <file>:<line>:<col>: error[<CODE>]: <msg>`;
  exit 2 = CLI misuse, 1 = everything else. CI is `.github/workflows/ci.yml`,
  which runs `scripts/ci.ps1`.

## Hard rules (docs/DECISIONS.md, docs/CONTRACT.md)

- Fail closed: any opcode, version, type or platform not explicitly supported
  gets a diagnostic and nonzero exit. No stubs, invented semantics or fallback.
- Programs are never lowered through C and never assembled by GAS/GCC:
  `win64.c` emits a private GAS-shaped text, `peephole.c` drops reloads of a
  slot just stored, `regalloc.c` moves the hottest slots into callee-saved
  registers, `asm_win64.c` encodes it to bytes, `coff.c` writes AMD64 COFF; GNU
  `ld` is only the linker.
- Never write to the Orbit checkout; moving the pin requires a dated entry in
  `docs/DECISIONS.md`.
- Do not generalize semantics that pass in fixtures beyond what
  `docs/M1_REPORT.md` states is measured (overflow, NaN payloads, other list
  kinds, Linux/macOS are currently out of evidence).

## Layout

- `src/loader/ir.{h,c}` — strict JSON IRv1 parser/validator, owns `R6Module`;
  also gates which opcodes are admitted (`E_UNSUPPORTED` outside the range).
- `src/codegen-x64/win64.{c,h}` — IR → private asm text; `peephole.{c,h}` —
  rewrites that text in place (D-M1-67); `regalloc.{c,h}` — assigns registers
  to slots on that text (D-M1-68); `asm_win64.{c,h}` — encodes only the
  selector's instruction subset; `coff.{c,h}` — AMD64 COFF writer (sections,
  symbols, REL32/ADDR32NB relocations, unwind data).
- `src/driver/r6.c` — CLI, temporaries under `.r6-build`, direct `ld` link.
- `tests/diff/run_diff.py` — differential harness; `tests/coff/*.c` — probes
  that produced `docs/COFF_OBJECTS.md`.
- `tests/reject/` — loader contract suite: `run_reject.py` (second gate),
  `make_cases.py` (deterministic case builder), `cases/` + `manifest.json`.
- `tests/corpus/` — the language-emitted corpus (`run_corpus.py`, the second
  measurement and its ratchet; `manifest.json` is the pin).
- `tests/compiler-ir/` — census of the compiler's own IR (D-M1-34):
  `run_census.py` emits every `compiler/**/*.orb` at the pin and records R6's
  first refusal on each; `baseline.json` is the one-way ratchet over
  `r6BuildOk`.
- `scripts/` — `build-r6.ps1` (driver build), `ci.ps1` (the CI steps, runnable
  locally), `migrate-m1-fixtures.py`
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

## Writing

Write for the person who takes the pin tomorrow. They have the repository and
the gates; they do not have this session.

- Documentation is impersonal. The page says what was measured, not who measured
  it. A commit body may say "I".
- One idea per sentence. Average under 22 words, none over 40. A semicolon
  usually means two sentences.
- Every claim carries a command, a number or a line. "Faster", "cleaner" and
  "more robust" carry nothing.
- Name the thing: "the 39/93 corpus", not "the coverage"; "the first refusal at
  `list_create`", not "the gap".
- A table is for more than two columns. Two facts are a sentence.
- No hedges. "Should", "likely" and "probably" become a measurement or a
  question.
- Do not explain the sentence you are about to write, or the one you just wrote.
- Banned in new text: shape, surface, lane, load-bearing, battery, premise,
  clause, witness, residual, honest, and their inflections. They are metaphors a
  reader cannot look up.
- Defined once, used freely: **pin** (the Orbit commit and the inputs listed in
  PIN.md), **corpus** (the executable documents in `tests/corpus/`), **ratchet**
  (the one-way baseline in `tests/corpus/baseline.json`), **decision** (a dated
  entry in DECISIONS.md with an ID).
- Contract prose is Spanish; match the file you edit. This file is English.

## Commits

One decision, one commit. A commit is how a change is found again, reviewed and
undone.

- Commit when the change is small, verified and complete. A correction that
  changes behaviour or a document's claim gets its own commit, even if it is one
  line.
- Do not wait for the rest of the work. A verified fix left in the working tree
  is a fix nobody can find in the history.
- Do not mix. A code change and a documentation change are two commits, unless
  the document is the reason the code changed. A decision that needs both
  DECISIONS.md and code is one commit: the decision is the reason.
- Do not commit a state you have not gated. The body names the gate and what it
  printed.
- A pin move is one commit: PIN.md, DECISIONS.md, the regenerated corpus
  manifest and the re-taken baseline together. They describe one tree, and a
  commit that splits them describes a tree that never existed.
- A pushed commit is a record. Fix forward; do not rewrite history.

Message:

- Subject: `area: what changed`, imperative, under 60 characters, no period. Use
  the prefixes the history uses: `feat`, `fix`, `test`, `docs`.
- The body says why. The diff says what.
- One claim per sentence. Every number is one the reader can reproduce.
- End with an Evidence block: one line per command and what it printed. A
  lowering change carries `run_diff.py` N/N, `run_reject.py` N/N, the corpus
  count and the ratchet delta.
- Never: "this commit", "as requested", a list of the files, a summary of the
  diff, or a round/phase label (`p7`, `round 7`). `D-M1-xx` and fixture names are
  stable identifiers: keep them.

## C style

- Four spaces, no tabs. Braces on the same line for functions and control flow,
  as the tree writes them.
- The driver builds with `-std=c11 -Wall -Wextra -Werror`; a warning there is a
  failure.
- Comments explain why the code is like this, not what it says. No new comment
  unless it says something the code cannot.
- Fail closed: an unsupported opcode, version, type or platform is a diagnostic
  and a nonzero exit. No stubs, no invented semantics, no fallback.
- Do not generalize semantics beyond what M1_REPORT.md states is measured.
- Preserve existing conventions: naming, allocation ownership, diagnostic codes,
  the one-line `r6: <file>:<line>:<col>: error[CODE]: <msg>` format.

## Working with other agents

- One writer of the pin at a time. PIN.md, DECISIONS.md,
  `tests/corpus/manifest.json` and `tests/corpus/baseline.json` move together.
  Before moving the pin, check who moved it last:

  ```powershell
  git log -1 --format='%h %ad %s' --date=short -- docs/PIN.md
  ```

  If that is within the hour, ask.
- Parallel zones: `src/loader`, `src/codegen-x64` and `src/driver` are separate
  files, but the codegen pipeline takes one writer and `win64.c` is one file.
  `tests/reject/manifest.json` is the suite truth and `make_cases.py` renders
  `cases/` from it; edit the manifest, never the cases, and one writer at a
  time.
- The Orbit checkout is read-only. Pinned bytes come from
  `git cat-file blob <pin>:<path>`, never from the working tree, which advances
  past the pin.
- Do not compare two corpus measurements taken against different `--orbit-root`
  trees. Materialize the pin and use it.

## The read-only reviewer

A reviewer edits nothing. It edits its answer, and the answer obliges the author
to redo the work.

- It reads the source diff, the fixture or corpus case the change adds, the
  decision in DECISIONS.md, and the commit body with its Evidence block. If a
  count in the body differs from what the gate prints, that is the finding.
- It runs `tests/diff/run_diff.py` and `tests/reject/run_reject.py` with the
  pinned inputs. It does not move the pin and does not `--write-baseline`: a
  reviewer that re-takes the baseline has made the change and is no longer a
  second pair of eyes.
- Three questions:
  1. Does the change do what the fixture claims? A new admission needs a
     positive and a negative control in the same commit; one positive proves
     only that it can build.
  2. Is it silent on the control? A change that admits one more program must not
     admit the one next to it.
  3. Does it survive both gates and the corpus without a regression? The ratchet
     only goes up.
- A NO to any of the three means redo. If it finds a defect in another file, it
  writes it up as separate work; it does not fix it. If it cannot run the gates,
  it says so and does not sign.

## When to stop and hand back

Stop and ask rather than push on, when:

- **A first refusal disappears without a change you made.** The corpus moved
  under you, usually because the checkout advanced. Materialize the pin and
  re-run before recording the count.
- **A gate fails only on a case you did not touch.** PIN.md and the corpus
  manifest are the authority; the advancing checkout is not.
- **DECISIONS.md or PIN.md says something your change contradicts.** They are
  the contract. Update them in the same commit, or ask first.
- **A change needs a second first refusal to pass.** One decision is one commit.
  When opening the second, stop.
- **You are about to generalize beyond what M1_REPORT.md states is measured.**
  That is a new decision with its own evidence, not a line in this one.

## Open source

- License: MIT, the same as `orbit-lang` (`LICENSE`).
- A public R6 cannot pin a commit no remote has. The pin `e34833a` is on
  `https://github.com/joacokhzyx/orbit-lang` (PIN.md), so the corpus is
  reproducible from public bytes. A pin move pushes the language commit first.
- The runtime archive is not committed and is not byte-reproducible (PIN.md). A
  fresh clone rebuilds it with the pinned recipe and checks the 337-symbol set.
  Do not add the archive or any other binary to the repository.
- CI (`.github/workflows/ci.yml`) builds the driver, the runtime archive and
  the loader suite on every push and pull request through `scripts/ci.ps1`.
  The differential and the corpus need the Orbit oracle and run only on
  manual dispatch; run them locally before a lowering change.
- A contribution arrives with its fixture, its decision and its evidence. A pull
  request that changes `src/` without a case and a decision is not reviewable.
- Do not commit secrets or build artifacts. The absolute paths in PIN.md are
  observations of one machine, not requirements.
