# Compile-time baseline

`run_perf.py` measures three things per build, on the machine that runs it:

- **corpus total**: every executable program of the pinned corpus built by R6,
  summed, best of `--reps`. It includes the `ld` invocation.
- **corpus in R6**: the sum of the driver's own stage times (load, codegen,
  peephole, regalloc, asm, object) over the same builds, read from
  `R6_TIMINGS=1`. The link is the one stage R6 does not run, and on a machine
  with a small page cache it is most of the wall time.
- **self-host R6 build**: R6 builds the IR of `compiler/host/main.orb`, best of
  `--reps`. `selfhostWallMs` records the whole `scripts/selfhost.ps1` or
  `scripts/selfhost.sh` run; the rest of that wall is the fixed point emitting
  IR and the compiler R6 built running, which R6 does not time.

```powershell
python tests/corpus/run_corpus.py --compiler C:\Temp\orbit-fp-ee60c8af43.exe `
  --cc C:\w64devkit\bin\gcc.exe --orbit-root C:\Temp\orbit-pin-01e31e8-git `
  --r6-command .r6-build\r6.exe build '{ir}' -o '{exe}'
.\scripts\selfhost.ps1 -OrbitRoot C:\Temp\orbit-pin-01e31e8-git `
  -Compiler C:\Temp\orbit-fp-ee60c8af43.exe -R6 $PWD\.r6-build\r6.exe
python tests/perf/run_perf.py --r6 .r6-build\r6.exe `
  --orbit-root C:\Temp\orbit-pin-01e31e8-git `
  --compiler C:\Temp\orbit-fp-ee60c8af43.exe --check
```

On Linux, `scripts/build-r6.sh`, `scripts/build-runtime-linux.sh` and
`scripts/selfhost.sh` replace the PowerShell halves; the arguments are the same
and `run_perf.py` picks the host script.

`baseline.json` holds one entry per platform, with the date and the number of
programs. The harness copies the IR and the runtime archives into the host
temporary directory first and builds with `R6_RUNTIME_DIR` pointing there: the
link reads the archives, and a shared folder would put the hypervisor's read
path inside every build. `--check` fails when a total grows past its baseline
by more than the tolerance and always accepts a smaller one. The stage totals
R6 owns ratchet at 3% and the self-host build at 5%; the walls ratchet at 15%,
because GNU `ld` is about half of a Linux corpus build (18.5 ms of 33.6 ms per
build) and it is sensitive to the page cache. The numbers are wall time on one
machine: a slower machine fails the check, and the fix is to re-take the
baseline there, deliberately, with `--write-baseline`.
