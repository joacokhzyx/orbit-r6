# Informe M1 — Windows x86-64

Fecha: 2026-10-14. M1 está verde para los cinco fixtures manuales aceptados en
Windows x86-64. El diferencial compara exit y stdout crudos; stderr se conserva
para diagnósticos. Los `.orb` se compilaron con el fixed-point Orbit del pin y
GCC como oráculo; R6 parseó IRv1 JSON, bajó a su representación assembler
interna, codificó bytes/COFF sin GCC ni GAS y enlazó con GNU `ld` y el runtime
oficial reconstruido del commit pineado. El enlazador/CRT MinGW sigue siendo
dependencia de plataforma.

## Evidencia

- Differential original de cinco casos: **5/5 pass**, tiempo total del harness **16.8 s** (compilación de
  oráculos y R6 incluida; no es benchmark). El caso `01-int-call` coincide en
  exit `42` y bytes `M1-add\r\n\r\n`; `02-string` en exit `0` y bytes
  `IRv1\r\n\r\n`; `03-float-branch` exit `0`, stdout vacío; `04-branch` exit
  `7`, stdout vacío; `05-call-3args` exit `6`, stdout vacío.
- Target/toolchain: GCC 16.2.0 `x86_64-w64-mingw32`.
- Orbit pin: `5c01cea68f125f06dd3ac82a61776827bdb0c6f3`; fixed-point compiler
  freshness confirmado por `scripts/dev.py fp --no-build --cc
  C:\w64devkit\bin\gcc.exe`; compiler SHA-256
  `40BCBCB473D5A6647DC6EF197AB9563AC78F1D17E0A535A7CA1A8E4D597DD1A2`.
- Runtime archive construido desde receta y fuentes del pin; SHA-256
  `3FC8FBE3E8D0733E7FA8161FD330011DF71C9DDB5E4B3A5D207AADC1D4F5AF8B`.
- Driver construido sin optimizaciones después de añadir aritmética/comparación
  signed; SHA-256 `52BD0FF350A7827848CA2DBC6BBF9FE1300FB786F9830C10E7EF5FA824A27DFC`.
- Fallo cerrado: IR version 2 devuelve exit 1 con `E_VERSION`; opcode `alloc`
  (fuera del subconjunto nativo actual) devuelve exit 1 con `E_UNSUPPORTED`.
- El harness y los ejecutables temporales de esta corrida residieron bajo
  `.r6-build/`, excluido de Git.
- Tras integrar el codificador interno y el escritor COFF, la cobertura de
  enteros y comparaciones creció con fixtures 06 y 07. El gate completo quedó
  en **7/7 pass** (19.8 s, solo diferencial; build ejecutado antes), incluso
  con `R6_CC` apuntando a una ruta inexistente.
  La reconstrucción anterior + gate de cinco casos tardó 19.9 s;
  no es benchmark. El driver R6 todavía se construye con GCC; la ruta de
  programa requiere GNU `ld`, MinGW CRT y el runtime oficial.

Comando del gate:

```powershell
$env:R6_CC = 'C:\w64devkit\bin\gcc.exe'
& 'C:\Users\Usuario\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' `
  tests/diff/run_diff.py `
  --compiler C:\Temp\orbit-fp-26cfb87400.exe `
  --orbit-root C:\Users\Usuario\Downloads\orbit-lang `
  --cc C:\w64devkit\bin\gcc.exe `
  --r6-command .r6-build\r6.exe build '{ir}' -o '{exe}'
```

## Alcance y límites

- Implementado en R6: JSON IRv1 estricto; Win64 x64; `load_const`, `add`
  (int/float/string), `sub/mul/div/mod` (int), `eq` (numérico/bool),
  `ne/lt/le/gt/ge` (int), `jump_if_false`, `label`, `arg`, `call` y `ret` para
  combinaciones probadas. Stack slots; sin optimizaciones.
- No se midió la suite completa de Orbit. No hay `--emit-ir`, spec upstream ni
  corpus emitido por Orbit en el pin. Los siete JSON son fixtures manuales
  comparados contra sus fuentes `.orb`; los dos añadidos derivan cobertura
  básica de aritmética y comparación signed.
- Casos de overflow/extremos fuera de los fixtures siguen sin cobertura. La
  evidencia semántica de int se limita al `c_backend.orb` fijado y estos siete
  pares; no extrapolar a todos los bordes de IRv1.
- Linux runtime/toolchain no verificados en este host. DB queda fuera hasta M3
  salvo variante oficial compatible. ARM64 macOS comienza en M3. Multihilo y
  benchmarks no se evaluaron.

## Reproducción

```powershell
$env:R6_CC = 'C:\w64devkit\bin\gcc.exe'
.\scripts\build-r6.ps1
```

Alternativa descartada: generar C o llamar a un compilador/assembler externo
por programa. La máquina/COFF se codifica internamente; la representación
textual GAS intermedia es privada al backend y el linker MinGW permanece.
