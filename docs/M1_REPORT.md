# Informe M1 — Windows x86-64

Fecha: 2026-10-14. El diferencial de fixtures manuales está verde para dieciocho
casos en Windows x86-64 (los cinco iniciales fueron aceptados para IRv1). El diferencial compara exit y stdout crudos; stderr se conserva
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
- Driver construido sin optimizaciones, ahora con lectura de globals literales
  int/bool, igualdad string vía `strcmp`, parámetros Win64 en pila y listas int;
  SHA-256 `F699D2E2FCEFB5708C8CDCA85378792A589F6CDC9C54FA51CF9A94ADB6AB2854`.
- Fallo cerrado: IR version 2 devuelve exit 1 con `E_VERSION`; opcode `alloc`
  (fuera del subconjunto nativo actual) devuelve exit 1 con `E_UNSUPPORTED`.
- El harness y los ejecutables temporales de esta corrida residieron bajo
  `.r6-build/`, excluido de Git.
- El gate diferencial anterior quedó en **13/13 pass** después de
  verificar branches tomados/no tomados, back-edge de `while`, comparaciones
  IEEE con NaN y signed zero, y un local `int` cargado/actualizado por nombre
  (build y diferencial ejecutados; no es benchmark), incluso
  con `R6_CC` apuntando a una ruta inexistente.
  La reconstrucción anterior + gate de cinco casos tardó 19.9 s;
  no es benchmark. El driver R6 todavía se construye con GCC; la ruta de
  programa requiere GNU `ld`, MinGW CRT y el runtime oficial.
- Nuevo fixture `14-global-constants`: **1/1 pass** aislado, exit `33` y stdout
  vacío, incluyendo tres globals int y uno bool del patrón observado en
  `builder.orb:2333-2372`. Globals const no literales (IRValue.Register),
  globals float/string y demás tipos siguen sin soporte.
- Repetición completa del gate: **14/14 pass**, exit 0, con `R6_CC` apuntando
  a `C:\missing\gcc.exe`; gcc sí se usó para el oráculo. Ningún programa
  compilado por R6 invocó GCC. No es benchmark.
- Fixture `15-string-equality`: **1/1 pass**, exit `5`, stdout vacío. Verifica
  `eq` por contenido tras concatenar y `ne` contra una cadena distinta; ambos
  llaman a `strcmp` del CRT. Gate completo repetido: **15/15 pass**, exit 0,
  con `R6_CC` inválido. No es benchmark.
- Fixtures `16-call-stack-args` y `17-call-stack-float`: **2/2 pass**, ambos
  exit `11`, stdout vacío. Cubren posiciones 5 y 6 enteras y binary64 en
  posición 5; repetición completa del gate: **17/17 pass**, exit 0, `R6_CC`
  apuntando a ruta inexistente. No es benchmark.
- Fixture `18-int-list-literal-len`: **1/1 pass**, exit `3`, stdout vacío. Mide
  create/push oficiales, local de referencia list y accessor de longitud; gate
  completo repetido: **18/18 pass**, exit 0 con `R6_CC` inválido. No es benchmark.

Comando del gate:

```powershell
$env:R6_CC = 'C:\missing\gcc.exe'
& 'C:\Users\Usuario\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' `
  tests/diff/run_diff.py `
  --compiler C:\Temp\orbit-fp-26cfb87400.exe `
  --orbit-root C:\Users\Usuario\Downloads\orbit-lang `
  --cc C:\w64devkit\bin\gcc.exe `
  --r6-command .r6-build\r6.exe build '{ir}' -o '{exe}'
```

## Alcance y límites

- Implementado en R6: JSON IRv1 estricto; Win64 x64; `load_const`, `add`
  (int/float/string), `sub/mul/div` (int/float), `mod` (int), `eq`
  (numérico/bool/string), `ne` (int/float/string), `lt/le/gt/ge` (int/float),
  `and_op/or_op/not_op` (bool), `neg` (int/float),
  `copy`, `jump`, `jump_if_false`, `label`, `load_var/decl_var/store_var`
  (locales escalares/list y lectura de globals literales int/bool),
  `list_create/list_push/list_len` (solo list<int> en main), `arg`, `call` y
  `ret` para combinaciones probadas; llamadas directas permiten hasta 256
  argumentos escalares y usan slots Win64 para posiciones 5+. Stack
  slots; sin
  optimizaciones.
- No se midió la suite completa de Orbit. No hay `--emit-ir`, spec upstream ni
  corpus emitido por Orbit en el pin. Los dieciocho JSON son fixtures manuales
  comparados contra sus fuentes `.orb`; los trece añadidos extienden cobertura
  de aritmética entera/float, comparación signed/float, lógica, negación,
  control de flujo/copia/loops, variables locales, globals int/bool,
  comparación string por contenido, llamadas con argumentos de pila y lista int.
- Casos de overflow/extremos fuera de los fixtures siguen sin cobertura. El
  fixture 10 confirma comparación IEEE unordered para NaN y equivalencia de
  `+0.0`/`-0.0`; fixture 11 cubre negación finita, no todos los payloads NaN.
  La evidencia semántica se limita al `c_backend.orb` fijado y estos dieciocho
  pares; los fixtures 14–18 añaden globals int/bool literales, comparaciones
  string, llamadas con argumentos en pila y lista int. No extrapolar a todos
  los bordes de IRv1.
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

## Actualización de M1 — list_get (2026-10-14)

- `19-int-list-get` y `20-int-list-get-oob`: **2/2 pass** aislado.
- Gate completo manual: **20/20 pass**, exit 0; el caso válido devuelve 20 y el error de índice devuelve 2. Comparación exacta de exit y stdout bytes con el compiler pineado y GCC `C:\w64devkit\bin\gcc.exe`.
- SHA-256 del driver probado: `E839C49E29223D6AF5C940537847F3FBFE0334D37C723298BCB9DDD8AFBA20D7`.
- `list_get` usa el layout documentado de `OrbitList` para cargar el slot y llama a los errores oficiales para índice/kind. El alcance permanece `list<int>` en `main`; otros kinds, set/pop y helpers fuera de main siguen rechazados.
- Esta evidencia es M1 manual, no la suite/std completa; no constituye benchmark.

## Actualización de M1 — formato IRv1 del lenguaje (2026-10-15)

- R6 migró su serialización al formato publicado por el lado lenguaje
  (`orbit-lang/docs/IR_SPEC.md`, blob
  `110ce439be8969d0e97986ad5eaa0cf0db92b1af`); decisión D-F0-06, resultado
  D-F0-07, análisis completo en `IR_SPEC_ALIGNMENT.md`. El pin no se movió: la
  spec está en el HEAD del checkout, no en `5c01cea`.
- Gate completo tras la migración: **20/20 pass** (exit 0) con exit/stdout
  byte a byte idénticos a la línea base previa por fixture; los 20 `.json` se
  regeneraron mecánicamente (`scripts/migrate-m1-fixtures.py`, con relectura de
  bits de floats) y ningún `.orb` cambió. No es benchmark.
- Loader: acepta `irVersion`/`module`, operandos `kind`/`value`, `dest` -1,
  `route*` plano y floats numéricos; rechaza el formato draft con `E_FORMAT`.
  Validaciones nuevas: V-08 (no finitos), V-10 (dest requerido), V-13 (aridad
  de `call`), V-14 (jump a label existente), V-16 (route coherente), V-18
  (cuerpo de extern ignorado) y marcador de elisión de V-01.
- Codegen: `call` consume exactamente `operand2` argumentos pendientes (E-05);
  un extern con cuerpo ya no se rechaza (E-09); las rutas Kynx se rechazan con
  `E_UNSUPPORTED` (fuera del subconjunto nativo).
- Verificaciones directas: el ejemplo mínimo de §5.2 de la spec compila con R6
  y corre (exit 0); el documento draft de `01-int-call` termina con
  `error[E_FORMAT]`. `.gitattributes` fija LF para `*.json`/`*.orb` (V-01)
  independientemente de `core.autocrlf`.
- SHA-256 del driver probado:
  `CAD71E9FD9786867ECD91F69EED76E54858372833095F20B82BF9541D53AA932`.
  Nota de reproducibilidad: el fixed-point `40BCBCB4…` registrado arriba no
  aparece en este host; el binario reconstruido (`C:\Temp\orbit-fp-26cfb87400.exe`,
  SHA-256 `6D60938B5BAF49755F93FA40A9FBACB91592F36418BCEA57D9FD064E1D5DA758`)
  pasa 20/20 y es el usado en esta corrida.
- La suite negativa (una prueba por regla V/E con código esperado) y su runner
  `tests/reject/` llegan en la fase B; esta nota sólo cubre la migración de
  formato.
