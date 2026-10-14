# Win64 program toolchain

The R6 program-build path lowers IR to the current internal assembler text,
parses only that emitted subset in `src/codegen-x64/asm_win64.c`, writes AMD64
COFF with `coff.c`, and invokes GNU `ld` directly. It does not invoke GCC or an
external assembler when compiling an Orbit IR program. The emitted text is a
temporary internal representation, not a supported user-facing assembler
language. Direct instruction emission from IR can replace it later without
changing the COFF writer.

R6 uses the official runtime archive built from the pinned Orbit checkout at
`.r6-build/runtime-win64/libruntime_net.a`, plus the matching MinGW-w64 CRT and
system import libraries. `R6_MINGW_ROOT` selects the MinGW installation
(default `C:/w64devkit`); `R6_MINGW_GCC_VERSION` selects its CRT library
directory (default `16.2.0`). The latter names a packaging directory only; no
GCC executable is used by the R6 program-build path. The runtime archive and
header are consumed as supplied/built from the language pin, not reimplemented.

Linker order follows the equivalent GCC driver expansion observed with
`gcc -###` for target `x86_64-w64-mingw32`: CRT startup objects, generated
object, pinned runtime archive, WinSock and MinGW/MSVCRT import/static
libraries, then CRT end object. The internal assembler/object writer and this
direct-link path were exercised by the five M1 differential fixtures on
2026-10-14 (5/5). `R6_CC` may be unset or invalid during the program build; it is
not read by `r6 build`. `scripts/build-r6.ps1` still uses GCC to build the R6
driver itself, and the differential oracle still uses GCC by design.

Alternativa descartada: conservar GCC o GAS en la ruta de cada programa; R6
codifica el subconjunto interno a bytes y COFF, y conserva `ld` para enlazar el
runtime/CRT oficial.
