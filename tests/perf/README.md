# Compile-time baseline

`run_perf.py` measures two R6 numbers, on the machine that runs it:

- **corpus total**: every executable program of the pinned corpus built by R6,
  summed, best of `--reps`.
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
programs. `--check` fails when a total grows past its baseline by more than the
tolerance and always accepts a smaller one. The numbers are wall time on one
machine: a slower machine fails the check, and the fix is to re-take the
baseline there, deliberately, with `--write-baseline`.
