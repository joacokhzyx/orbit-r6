# M1 differential harness

`run_diff.py` builds each accepted hand-written IR fixture with R6 and its
paired `.orb` source with the pinned Orbit compiler, then compares process
exit status and stdout as raw bytes. This follows the build and disposable
compiler environment of `scripts/test_suite.py` in the pinned Orbit checkout.
Build diagnostics are reported as failures. Stderr is shown when useful for
diagnosis, but the conformance condition for these programs is exit plus
stdout byte identity.

The default fixture set is all matching `.json` and `.orb` pairs in
`docs/proposed-ir/m1/`. No pass is reported until both binaries build and run.

Example, once an R6 build driver is available:

```powershell
python tests/diff/run_diff.py `
  --compiler C:\path\to\orbit.exe `
  --orbit-root C:\Users\Usuario\Downloads\orbit-lang `
  --cc gcc `
  --r6-command C:\path\to\r6.exe build {ir} -o {exe}
```

The R6 command is an argv template, not a shell command. Every `{ir}` and
`{exe}` token is substituted with an absolute path. A custom driver may use a
different command shape, for example `--r6-command python path\driver.py
--input {ir} --output {exe}`. Use `--case 01-int-call` to run one fixture.

`--compiler` and `--cc` mirror the pinned suite runner options. Omitting
`--cc` lets the Orbit compiler's normal default apply while the disposable
environment suppresses its nested compiler call, as the pinned suite runner
does. `ORBIT_ROOT` can set the pinned checkout when `--orbit-root` is omitted.
