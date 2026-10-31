# Contributing to R6

`.github/workflows/ci.yml` runs the build, the reject-case drift check and the
loader suite on every push and pull request. It does not run the differential or
the corpus ratchet by default. The gates below are the contract, and a
contribution runs them locally before it is sent. A contribution keeps both
platforms green: Windows x64 and Linux x64 (D-M1-119).

`AGENTS.md` holds the full working rules; this file does not replace it. The
pin, the oracle and the fixed point are defined in `README.md` (Status and The
pin). `.\scripts\ci.ps1` runs the driver build, the drift check and the loader
suite locally.

## What a contribution carries

A pull request that changes `src/` without a case and a decision is not
reviewable. Each one arrives with:

1. A fixture pair (`.json` plus `.orb`) in `docs/proposed-ir/m1/`, or a case in
   `tests/reject/manifest.json`. A new admission needs a positive and a negative
   control in the same change.
2. A dated decision with an ID in `docs/DECISIONS.md`. A pin move, a new
   admission and a change of a documented claim are each a decision.
3. An Evidence block in the commit body (see Commits).

Edit `tests/reject/manifest.json`, never `tests/reject/cases/`. Render with
`python tests/reject/make_cases.py`. Keep `.json` and `.orb` fixtures LF.

## Setup

- Build the driver with `.\scripts\build-r6.ps1` on Windows or
  `./scripts/build-r6.sh` on Linux. GCC builds the driver only and never an R6
  program.
- Rebuild the runtime archive from the pinned Orbit checkout as `docs/PIN.md`
  describes for Windows. On Linux, `./scripts/build-runtime-linux.sh <checkout>`
  writes `libruntime_net.a` (25 members) and `libruntime_net_db.a` (27 members)
  into `.r6-build/runtime-linux-x64`. Do not commit an archive or any binary.
- Materialize a checkout of `orbit-lang` at the pinned commit and build its
  fixed-point compiler. Do not use a working tree that is ahead of the pin.
- Never write to the Orbit checkout. Read pinned bytes with
  `git cat-file blob 01e31e8:<path>`.

## Gates

Run from the repository root. `--r6-command` must be the last argument. The paths
are those of one machine; substitute yours. The current counts are in the
README status table. The counts here were measured on 2026-10-31 on Windows x64
and Linux x64.

Differential, the primary gate, 100/100:

```powershell
python tests/diff/run_diff.py `
  --compiler C:\Temp\orbit-fp-ee60c8af43.exe `
  --cc C:\w64devkit\bin\gcc.exe `
  --orbit-root C:\Temp\orbit-pin-01e31e8-git `
  --r6-command .r6-build\r6.exe build '{ir}' -o '{exe}'
```

```bash
python3 tests/diff/run_diff.py \
  --compiler <fixed-point> \
  --cc gcc \
  --orbit-root <pinned checkout> \
  --r6-command ./.r6-build/r6 build '{ir}' -o '{exe}'
```

In a POSIX shell the r6 command is separate argv tokens, and only `{ir}` and
`{exe}` are quoted. `--case <stem>` runs one fixture. The verdict is identical
exit code and stdout bytes against the oracle. A build failure counts as a
failure.

Loader contract suite, the second gate, 452/452:

```powershell
python tests/reject/run_reject.py
python tests/reject/run_reject.py --case v13
python tests/reject/make_cases.py --check
```

```bash
python3 tests/reject/run_reject.py
python3 tests/reject/run_reject.py --case v13
python3 tests/reject/make_cases.py --check
```

On Linux the runner defaults to `.r6-build/r6` and
`.r6-build/runtime-linux-x64`. Every rejection exits 1 with exactly one `r6:
<file>:<line>:<col>: error[CODE]: <msg>` line carrying the manifest code.

Corpus, a measurement with a one-way ratchet, 143/143 with the ratchet holding:

```powershell
python tests/corpus/run_corpus.py `
  --compiler C:\Temp\orbit-fp-ee60c8af43.exe `
  --cc C:\w64devkit\bin\gcc.exe `
  --orbit-root C:\Temp\orbit-pin-01e31e8-git `
  --check `
  --r6-command .r6-build\r6.exe build '{ir}' -o '{exe}'
```

```bash
python3 tests/corpus/run_corpus.py \
  --compiler <fixed-point> \
  --cc gcc \
  --orbit-root <pinned checkout> \
  --check \
  --r6-command ./.r6-build/r6 build '{ir}' -o '{exe}'
```

Only corpus drift from the pin and a drop of `r6BuildOk` or `match` below
`tests/corpus/baseline.json` fail the run. Do not run `--write-baseline` on a
change you are reviewing.

Self-host check, both hosts print `SELFHOST OK` with `PUBLISHED_C E794E2DB…`:

```powershell
.\scripts\selfhost.ps1 -OrbitRoot C:\Temp\orbit-pin-01e31e8-git `
  -Compiler C:\Temp\orbit-fp-ee60c8af43.exe -R6 $PWD\.r6-build\r6.exe
```

```bash
./scripts/selfhost.sh <pinned checkout> <fixed-point> ./.r6-build/r6
```

Compile time is a measurement, not a gate:

```bash
python tests/perf/run_perf.py --r6 <driver> --orbit-root <pinned checkout> \
  --compiler <fixed-point> --check
```

`python3` takes the place of `python` on Linux. `tests/perf/baseline.json` holds
one entry per platform, and `--write-baseline` re-takes only the host's entry.

The compiler-IR census (`tests/compiler-ir/run_census.py`, 2 build over 27
modules) is also a measurement. Its ratchet forbids `r6BuildOk` going down.

## Rules for the code

- C11, four spaces, no tabs, braces on the same line. The driver builds with
  `-std=c11 -O2 -Wall -Wextra -Werror`; a warning is a failure.
- Fail closed: an unsupported opcode, version, type or platform is a diagnostic
  and a nonzero exit. No stubs, invented semantics or fallback.
- Do not generalize beyond what `docs/M1_REPORT.md` states as measured. That is
  a new decision with its own evidence.
- Programs are never lowered through C and never assembled by GAS or GCC. The
  object writer is AMD64 COFF on Windows and ELF64 ET_REL on Linux. GNU `ld`
  only links, on both hosts.
- Contract prose in `docs/` is Spanish. Match the file you edit.

## Commits

One decision, one commit. A code change and a documentation change are two
commits, unless the document is the reason the code changed.

- Subject: `area: what changed`, imperative, under 60 characters, no period.
  Prefixes: `feat`, `fix`, `test`, `docs`.
- Body: why. The diff says what. One claim per sentence, and every number is one
  the reader can reproduce.
- End with an Evidence block, one line per command and what it printed. A
  lowering change carries `run_diff.py` N/N, `run_reject.py` N/N, the corpus
  count and the ratchet delta.
- A commit does not say "this commit" or "as requested", does not list files and
  does not carry a round label.
- Do not commit a state that has not passed the gates. A pushed commit is a
  record: fix forward, do not rewrite history.
- A pin move is one commit: `docs/PIN.md`, `docs/DECISIONS.md`, the regenerated
  corpus manifest and the re-taken baseline together.

## Review

A reviewer edits nothing. It runs `tests/diff/run_diff.py` and
`tests/reject/run_reject.py` with the pinned inputs, and compares every count in
the commit body with what the gates print. A different count is the finding.

1. Does the change do what the fixture claims? One positive control proves only
   that the program builds.
2. Is it silent on the control? A change that admits one more program must not
   admit the one next to it.
3. Does it survive both gates and the corpus on both hosts without a regression?
   The ratchet only goes up.

A NO to any question means the author redoes the work. A defect in another file
is written up as separate work. A reviewer that cannot run the gates says so and
does not sign.

## Writing

Documentation is impersonal and states what was measured. Sentences carry one
idea and every claim carries a command, a number or a line. `AGENTS.md` lists
the banned words.

## License

By contributing you agree that your work is released under the MIT license in
`LICENSE`.
