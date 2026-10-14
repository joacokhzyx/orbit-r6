# Win64 program toolchain

The R6 program-build path currently emits GNU assembler text, invokes GNU `as`
directly, then invokes GNU `ld` directly. It does not invoke GCC or another
compiler driver when translating/linking an Orbit IR program. This is an
intermediate dependency reduction: the assembler is still external, so this is
not yet direct machine-code/object emission and does not satisfy the final
native-backend goal.

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
libraries, then CRT end object. This direct-link path was exercised by the
five M1 differential fixtures on 2026-10-14.

Alternativa descartada: conservar GCC como assembler/linker-driver de cada
programa; se eligió `as` y `ld` directos como paso de transición, hasta que R6
genere bytes y COFF sin herramientas de ensamblado externas.
