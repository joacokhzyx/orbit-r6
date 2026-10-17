# Pinned language inputs

| Input | Value | Evidence / status |
|---|---|---|
| Orbit repository | `C:\Users\Usuario\Downloads\orbit-lang` (read-only to R6) | Local checkout inspected; do not write there. |
| Orbit commit | `5c01cea68f125f06dd3ac82a61776827bdb0c6f3` | `git rev-parse HEAD` on 2026-10-14. This is the fixed point for F0 proposal only; do not move it by following a branch. |
| Orbit IR version | IRv1, published by the additive emitter branch below; none at `5c01cea` | The pinned commit still has no `--emit-ir`; the branch adds it additively (D-F0-08, D-F0-09). |
| IR emitter branch (local) | `C:\Users\Usuario\Downloads\orbit-lang-emit-ir`, branch `r6-emit-ir`, commit `1e6a378a6a67a6bfdd36df127584f49c54405538` | Full clone (not shallow), tree clean. Three additive commits on the pin: `ed17dac` (new `compiler/ir_emit.orb` + `--emit-ir` in `orbit build`), `a610905` (`scripts/emit_ir_corpus.py`), `1e6a378` (self-test + `scripts/alive_check.py` row). **Local only: not merged into `main`, not pushed to any remote** (user decision 2026-10-16). |
| IR emitter binary (oracle) | `C:\Temp\orbit-fp-67e4ecfa75.exe` | SHA-256 `805cf61894ca83aec0afa1fe5a80a652a0cdc3e3e88a2e6be6a6abdf6e2e07dc`; 2764800 bytes; `orbit 0.1.0-rc.2`; fixed-point build of `1e6a378`. Oracle of the corpus differential. |
| Emitted IR corpus (R6 pin) | `tests/corpus/manifest.json` with `baseline.json` and `run_corpus.py` | 89 executable + 1 support = 90 documents, 5334302 bytes, `filesSha256 10f6e7d97977aaeec3018b9f8694e78dc7fee3ae614fa16cf4db27cc10b7f89b`, per-file SHA-256 in the manifest, `languageCommit 1e6a378a6a67`; regenerate with `run_corpus.py --orbit-root <clone>`. Measured, not committed as `.ir` (D-M1-17). |
| Self-host canonical (branch) | `compiler/selfhost/stage3.exe.c` at `1e6a378`; `PUBLISHED_C` in `scripts/verify_seed.py` | `32FE98D7CBA95C6B2334E7B6A0FD962D963AE428EF0F81D8ACAEAC05B9FCBACB`; `verify_seed` 4/4; `dev.py all` 19/19. Promoted alongside the emitter commit. |
| Runtime ABI header | `runtime/orbit_api.h` at the same commit | Committed blob `6a29a5a01ca15aff03fc4c8b42a2026ec9abef66`, equal to working-tree bytes; `orbit_print` is `void orbit_print(const char* str)` at line 611. |
| Runtime build recipe | `scripts/mklib_runtime.py` at the same commit | Tracked by the pin; it builds unity runtime source with `ORBIT_WITH_NET`, `ORBIT_LIB_NO_ROUTER`, `ORBIT_PERF_IMPL`; `--out` supports an R6-local artifact path. |
| Runtime library, Windows x64 | Rebuilt from the official pinned script/source to `.r6-build/runtime-win64/libruntime_net.a` (local ignored build output; not committed) | SHA-256 `3FC8FBE3E8D0733E7FA8161FD330011DF71C9DDB5E4B3A5D207AADC1D4F5AF8B`; one archive member `orbit_lib.o`; `objdump` reports PE x86-64; `nm` confirms `orbit_print`, `orbit_alloc`, `orbit_result_ok`, `orbit_string_concat`; `orbit_handle_request` absent as required. Build used GCC 16.2.0 `x86_64-w64-mingw32`. This is source/pin correspondence for this Windows build, not byte reproduction of the untracked local archive. |
| Existing local runtime archive | `.orbit/lib/libruntime_net.a` in the Orbit checkout; untracked | SHA-256 `6D2FF3CA957AB18529B285BA6296DF8E092867A7143FBE27F5DACA565314ADAC`; differs from the official-script output above; excluded from R6. |
| Runtime library, Linux x64 | Not yet built/verified | No Linux distribution is installed in WSL and no native Linux x64 compiler is available. Must build with the pinned script/source on a Linux x64 toolchain before enabling that platform. |
| Available GCC | GCC 16.2.0, target `x86_64-w64-mingw32` | Local Windows x64 MinGW toolchain; use `-lws2_32` at final Windows link as the pinned header documents. |
| Candidate M2 source corpus | 89 executable cases: 67 suite programs + 22 std programs; suite also has 1 `.support.orb` helper excluded by `scripts/test_suite.py:59` | Pinned checkout inventory. No direct `db(...)`, migration, `orbit_db_*` or auth calls found in these source files; thus 0 currently identified DB cases in this specific source inventory. This is not a measured IR/differential result. |

Changing the Orbit pin requires a dated decision in `DECISIONS.md`, with new
header/library hashes and corpus inventory. M2 may not silently include tests
from another commit. Build artifact hashes depend on compiler/archive toolchain;
source provenance is established by the pinned recipe, not archive byte equality.

The emitter branch is an **added local input** (D-F0-09): the language commit
pin stays at `5c01cea`, and the emitter commit exists only in the clone above,
so the emitter pin stops resolving if that clone is deleted. Publishing it (a
push or a bundle) would only add provenance; it does not change what R6
verifies.
