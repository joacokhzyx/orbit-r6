# Pinned language inputs

| Input | Value | Evidence / status |
|---|---|---|
| Orbit repository | `C:\Users\Usuario\Downloads\orbit-lang` (read-only to R6) | Local checkout inspected; do not write there. |
| Orbit commit | `299eb094d51098945a6618a60ba2efc02dfe59d9` (`main`, the local merge of the emitter branch) | `git rev-parse HEAD` on 2026-10-16, after the local merge of `r6-emit-ir` (no push). Second parent is the emitter tip `1e6a378a6a67a6bfdd36df127584f49c54405538`. Moved from `5c01cea` by D-F0-10. |
| Orbit IR version | IRv1, published by `main` at `299eb09` | `orbit build --emit-ir` is part of the pinned commit (merged additively; D-F0-08, D-F0-10). |
| IR emitter (in-repo) | `C:\Users\Usuario\Downloads\orbit-lang`, branch `main`, commit `299eb094d51098945a6618a60ba2efc02dfe59d9` | The emitter commits `ed17dac` (new `compiler/ir_emit.orb` + `--emit-ir` in `orbit build`), `a610905` (`scripts/emit_ir_corpus.py`) and `1e6a378` (self-test + `scripts/alive_check.py` row) are the second parent of the merge, so they are ancestors of `main`: the emitter pin resolves from the user's own repository, not only a clone. The separate clone `orbit-lang-emit-ir` is no longer required. **Local only: not pushed to any remote** (D-F0-10). |
| IR emitter binary (oracle) | `C:\Temp\orbit-fp-26cfb87400.exe` | SHA-256 `36c42e20215a72de727b2f948d146f046626c39a1195095163a7ff821f78b103`; 2540544 bytes; `orbit 0.1.0-rc.2`; fixed-point build of `299eb09`. The name is `orbit-fp-` + the first 10 hex of `sha256(checkout path)`, so a rebuild in place overwrites it. Oracle of the differential and of the corpus. |
| Emitted IR corpus (R6 pin) | `tests/corpus/manifest.json` with `baseline.json` and `run_corpus.py` | 93 executable + 1 support = 94 documents, 5419454 bytes, `filesSha256 0106d531cc10f2edb02097db2babdef4291f845a03be85dcfee47c3d91b5bfb9`, per-file SHA-256 in the manifest, `languageCommit 299eb094d51098945a6618a60ba2efc02dfe59d9`; regenerate with `run_corpus.py --orbit-root C:\Users\Usuario\Downloads\orbit-lang`. Measured, not committed as `.ir` (D-M1-17); re-measured at the re-pin as **7/93** (baseline ratchet `r6BuildOk 7`, `match 7`). |
| Self-host canonical (in-repo) | `compiler/selfhost/stage3.exe.c` at `299eb09`; `PUBLISHED_C` in `scripts/verify_seed.py` | `15341D24EEBD4F01B37FED7C58639DAF0736AC2BE8E073A519432DDB5B01A059`; `verify_seed` 4/4; `dev.py all` 21/21. Re-promoted from the merged sources during the merge (D-F0-10). |
| Runtime ABI header | `runtime/orbit_api.h` at `299eb09` | Committed blob `27fc52b86ab14b8e868178915c81d89ca56f186b`, equal to working-tree bytes; `orbit_print` is `void orbit_print(const char* str)` at line 611. The move from `6a29a5a0` is additive: one added declaration, `bool orbit_os_isatty_selfhost(orbit_int fd)` (line 657); no existing signature changes. |
| Runtime build recipe | `scripts/mklib_runtime.py` at `299eb09` | Committed blob `86858c142759ba539e10e6880467f9372e7b617c`. Tracked by the pin; it builds unity runtime source with `ORBIT_WITH_NET`, `ORBIT_LIB_NO_ROUTER`, `ORBIT_PERF_IMPL`; `--out` supports an R6-local artifact path. The move adds an optional `defines=` parameter (default unchanged; used by the shard bootstrap). |
| Runtime library, Windows x64 | Rebuilt from the official pinned script/source to `.r6-build/runtime-win64/libruntime_net.a` (local ignored build output; not committed) | SHA-256 `BFEE53955BCF5755126998037F228D95FB2B7E81C24EA7A51060A07E8E0B2C52`; one archive member `orbit_lib.o`; `objdump` reports PE x86-64; `nm` confirms `orbit_print`, `orbit_alloc`, `orbit_result_ok`, `orbit_string_concat`; `orbit_handle_request` absent as required. Build used GCC 16.2.0 `x86_64-w64-mingw32`. Rebuilt at the re-pin from `299eb09` (D-F0-10). This is source/pin correspondence for this Windows build, not byte reproduction of the untracked local archive. |
| Existing local runtime archive | `.orbit/lib/libruntime_net.a` in the Orbit checkout; untracked | SHA-256 `6D2FF3CA957AB18529B285BA6296DF8E092867A7143FBE27F5DACA565314ADAC`; differs from the official-script output above; excluded from R6. |
| Runtime library, Linux x64 | Not yet built/verified | No Linux distribution is installed in WSL and no native Linux x64 compiler is available. Must build with the pinned script/source on a Linux x64 toolchain before enabling that platform. |
| Available GCC | GCC 16.2.0, target `x86_64-w64-mingw32` | Local Windows x64 MinGW toolchain; use `-lws2_32` at final Windows link as the pinned header documents. |
| Candidate M2 source corpus | 93 executable cases: 71 suite programs + 22 std programs; suite also has 1 `.support.orb` helper excluded by `scripts/test_suite.py:59` | Pinned checkout inventory. No direct `db(...)`, migration, `orbit_db_*` or auth calls found in these source files; thus 0 currently identified DB cases in this specific source inventory. This is not a measured IR/differential result. |

Changing the Orbit pin requires a dated decision in `DECISIONS.md`, with new
header/library hashes and corpus inventory. M2 may not silently include tests
from another commit. Build artifact hashes depend on compiler/archive toolchain;
source provenance is established by the pinned recipe, not archive byte equality.

The emitter is now an **in-repo input** (D-F0-10): the language commit pin is
`299eb09` (`main`), the merge of the emitter branch, so the emitter commit
resolves from the user's own checkout and the separate clone is no longer part
of the pin. Nothing was pushed: provenance is local, which does not change what
R6 verifies. The earlier local-only pin (D-F0-09: `5c01cea` + the clone) is
superseded.
