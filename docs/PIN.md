# Pinned language inputs

| Input | Value | Evidence / status |
|---|---|---|
| Orbit repository | `C:\Users\Usuario\Downloads\orbit-lang` (read-only to R6) | Local checkout inspected; do not write there. |
| Orbit commit | `5c01cea68f125f06dd3ac82a61776827bdb0c6f3` | `git rev-parse HEAD` on 2026-10-14. This is the fixed point for F0 proposal only; do not move it by following a branch. |
| Orbit IR version | none published | No `--emit-ir`, versioned spec or serialized IR corpus found at the pinned commit. |
| Runtime ABI header | `runtime/orbit_api.h` at the same commit | SHA-256 of current working-tree bytes: `1AF91EB9925C884F1D6AAC7322FE2BF4442E7D3D5A778B2F1FEC20AF226F0531`; verify committed blob before using. |
| Runtime library | `.orbit/lib/libruntime_net.a` present locally, 144214 bytes | SHA-256 `6D2FF3CA957AB18529B285BA6296DF8E092867A7143FBE27F5DACA565314ADAC`; `objdump -f` reports `pe-x86-64` (`i386:x86-64`); `nm` shows `orbit_print`, `orbit_alloc`, `orbit_result_ok` and HTTP exports. `git ls-files` does not list this artifact, so its presence/hash/symbols do **not** prove it is published or reproducibly built from the pin. Do not link until provenance and target are established. No Linux variant was found in `.orbit/lib`. |
| Available GCC | `gcc 16.2.0`, target `x86_64-w64-mingw32` | Local Windows x64 MinGW toolchain observed. No Linux x64 native toolchain has been verified in this environment. |
| Candidate M2 source corpus | 90 files: 68 `tests/suite/*.orb` + 22 `tests/std/*.orb` | Pinned checkout inventory. No direct `db(...)`, migration, `orbit_db_*` or auth calls found in these files; thus 0 currently identified DB cases in this specific source inventory. This is not a measured IR/differential result. |

Changing the Orbit pin requires a dated decision in `DECISIONS.md`, with new
header/library hashes and corpus inventory. M2 may not silently include tests
from another commit.
