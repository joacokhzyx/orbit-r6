# Language corpus measurement

The corpus is the IRv1 the pinned language emits over its own tests: 129
programs from `tests/suite` plus `imports.support.orb`, and 24 programs from
`tests/std`. That is 154 documents: 153 executable and one support module. The
language designates this corpus in `orbit-lang/docs/IR_SPEC.md` §5.5, and it is
pinned by hash here (D-M1-17). The corpus gate checks the pinned hashes and the build and match ratchets.

```powershell
python tests/corpus/run_corpus.py `
  --compiler $OrbitFp `
  --cc C:\w64devkit\bin\gcc.exe `
  --orbit-root C:\Temp\orbit-pin-2275910 `
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

- `manifest.json` is the pin: 154 entries with `path`, `kind`, `ir`, `bytes`
  and `sha256`, the single `filesSha256 3c0227b3…` over the corpus, and the
  emitter identity (version, language commit `2275910`, binary hash). The
  language's `scripts/measure.py emit-ir` produces it; regenerating and
  comparing is the contract, not copying files. The same pattern pins
  `runtime/orbit_api.h` and the runtime archive.
- `baseline.json` is the current measurement: `summary` (`members`, `programs`,
  `oracleOk`, `r6BuildOk`, `match`, `unknownRegisterTypes`), `byCode`,
  `byFailingOpcode` and `byOpcode`. Measured at this pin: 153 build and 153
  match over 153 programs. `--write-baseline` re-takes it after `--check`, so one run holds
  the old ratchet and records the new one. A pin move replaces the corpus
  whole, so its baseline is taken without `--check`: the ratchet refuses to
  compare a baseline taken against another corpus.
- `run_corpus.py` regenerates the corpus with the pinned compiler, verifies
  every hash against the pin, and per program builds and runs both sides in the
  same directory. It asks the pin which emitter it carries, because `e8e1e19`
  folded `scripts/emit_ir_corpus.py` into `scripts/measure.py emit-ir`. The
  oracle builds from the `.orb` and R6 from the `.ir`, with exit and stdout
  compared byte for byte.
  `--ir-dir` measures an already-emitted corpus; `--json report.json` keeps the
  per-case detail.

## What fails and what does not

Only two things fail with exit 1:

1. A corpus that no longer matches `manifest.json`. The pin is broken, and
   silence would be worse.
2. `--check` when `r6BuildOk` or `match` drops below `baseline.json`. The
   ratchet is one-way, the same contract as the language's `unknown`-type
   ratchet.

A failing corpus program, including an `oracle-fail`, is part of the
measurement. When the whole corpus reports `oracle-fail` with `r6BuildOk 0`,
the language side failed to build, not R6; the check then refuses to write a
baseline. `ORBIT_CCACHE=0` takes the checkout's C-cache wrapper out of one run.

The oracle's exit, stdout and stderr are cached in
`.r6-build/oracle-cache.json`, shared with `run_diff.py`. A corpus entry is
keyed by the manifest SHA-256, the fixed-point SHA-256 and the program path;
`--no-oracle-cache` rebuilds every oracle. `--axes` adds six axes per matching
program: correctness, section bytes, best-of-five wall milliseconds,
best-of-five R6 build milliseconds, peak working set and a CPU-time energy
proxy. `--check-axes` compares them with `axes-baseline.json`, and
`--write-axes-baseline` re-takes it.
