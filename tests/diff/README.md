# Differential harness

`run_diff.py` builds each fixture's `.json` with R6 and its paired `.orb` with
the pinned Orbit compiler through GCC, runs both programs in the same
directory, and compares process exit status and stdout as raw bytes. Stderr is
shown when it helps diagnosis; the verdict is exit plus stdout identity. A build
failure is a failure.

The fixtures are the 100 pairs in [`fixtures/`](fixtures/), and
[`fixtures/README.md`](fixtures/README.md) lists what each one exercises. They
are hand-written and predate the emitter; the language's own output is the
corpus in `tests/corpus/`.

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

`--r6-command` must be last. It is an argv template, not a shell command: every
`{ir}` and `{exe}` token is replaced with an absolute path, and a different
driver command is allowed. `--case 01-int-call` runs one fixture. `--orbit-root`
is a checkout at the pin; the harness sets `ORBIT_BACKEND=c` for the oracle
because native is the default backend on Windows at this pin.

`--compiler` and `--cc` mirror the pinned suite runner. Omitting `--cc` lets the
Orbit compiler's default apply while the disposable environment suppresses its
nested compiler call, as the pinned suite runner does. `ORBIT_ROOT` sets the
pinned checkout when `--orbit-root` is omitted. Oracle results are cached in
`.r6-build/oracle-cache.json`; `--no-oracle-cache` rebuilds every one.
