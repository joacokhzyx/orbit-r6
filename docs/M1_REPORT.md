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

## Actualización de M1 — suite negativa y familias C (2026-10-15)

- Segunda puerta del contrato del loader (`tests/reject/`, decisión D-M1-06):
  **88/88** (82 rechazos + 6 controles de aceptación). `make_cases.py`
  renderiza `cases/` y `manifest.json` de forma determinista; cada rechazo
  exige exit 1 y una única línea `r6: <file>:<line>:<col>: error[CODE]` con el
  código esperado del manifiesto, y los controles de aceptación corren con
  exit/stdout comparados byte a byte. Cubre las reglas V-01..V-19/E-01..E-12
  aplicables y los bordes de cada familia (D-M1-10 a D-M1-13).
- Gate diferencial: **25/25 pass** (exit 0), con cinco fixtures nuevos respecto
  de la migración: `21-nop-blocks` (D-M1-07), `22-result-ops` (D-M1-08),
  `23-list-set-pop` (D-M1-09), `24-model-fields` (D-M1-11) y
  `25-union-tag-data` (D-M1-12); los 20 `.orb` previos no cambiaron.
- Familias cerradas con evidencia en lugar de implementadas: `map_*`
  (D-M1-10) y `alloc`/`free` (D-M1-13) quedan `E_UNSUPPORTED` con pins
  propios: sin productor en el builder (spec §1.1) o sin contrato estable
  (lowerings incompletos y gaps U-04/U-09).
- SHA-256 del driver probado:
  `DDBF33F0BBB144A1EF4AA09481639E81BA74E18E0136463DDCB31B8CADC1053D`.
- La fase D (emisor `--emit-ir` del lado lenguaje, corpus y re-pin) sigue
  fuera de lo medido aquí; las preguntas abiertas P1-P5 están registradas en
  `IR_SPEC_ALIGNMENT.md`. Esta evidencia es M1 manual (fixtures escritos a
  mano; el pin aún no emite IR) y no es un benchmark.

## Actualización de M1 — valores de función, llamada indirecta y globales (2026-10-15)

- Fixture `26-fn-values` (D-M1-14): `val`/`var f = inc` liga un nombre de
  función en posición de valor (`decl_var` + Symbol), `f(args)` baja como
  `arg` pendientes, `load_var` del callee y `call` con la aridad en
  `operand2`, chequeada contra el spelling `fn(p1;...;pn)->ret` del registro;
  el rebinding (`h = dec`) carga el símbolo a un registro y usa `store_var`.
  Seis subtests: un parámetro, dos parámetros, nullary, string, externo y
  rebinding. El oráculo del pin acepta el binding externo
  (`orbit_string_at("abc", 1) == 98`) y R6 lo replica: exit 42, stdout
  `fn-ok` (idénticos).
- La rama de globales con valor `ir_register` queda cerrada con evidencia
  (D-M1-15): el builder contiene la ruta `IRGlobal(ir_register)` para un
  `const` de archivo no literal (`builder.orb:2377`) pero la sema la hace
  inalcanzable (`E1007`, `sema.orb:1824-1831`); el loader mantiene el rechazo
  y el borde queda pineado (`x-global-register`).
- Gate diferencial: **26/26 pass** (exit 0); los 25 pares previos y sus `.orb`
  no cambiaron.
- Segunda puerta: **104/104** (96 rechazos + 8 controles de aceptación):
  trece pins `x-fn-*` (doce `CGEN` + uno V-13 `E_SCHEMA`), el pin
  `x-global-register` y los controles `ok-fn-value`/`ok-fn-extern`;
  `make_cases.py --check` sin drift.
- SHA-256 del driver probado:
  `8D080E25331F4A5BD7889E44F3DCB1CE28846A8D9D99683F65D804956EB92004`.

## Actualización de M1 — bordes de la spec E-03/E-04/E-06 (2026-10-16)

- Los tres casos límite que las notas de fase B dejaron abiertos quedan
  cerrados como rechazos con evidencia (D-M1-16): `list_get` con `elemKind 5`
  y tipo `unknown` (E-03), `elemKind 0` en `list_create` (E-04) y `print` con
  2 argumentos pendientes (E-06). La tabla de bordes de la spec pide "accept"
  para los tres; R6 mantiene su subconjunto fail-closed sobre los límites ya
  decididos (D-M1-09 para kinds 0/5) con evidencia adicional: los kinds 0/5 no
  tienen tipo de acceso dentro del modelo de registros (`unknown`/`void*`) y
  el print multicarácter no tiene productor — la sema del pin lo rechaza con
  E1003 (`sema.orb:3577-3584`) — ni lowering con semántica (solo el
  comentario `/* print */`, `c_backend.orb:2795-2800`; U-17).
- Pins nuevos en `tests/reject/`: `x-list-get-kind-ref`,
  `x-list-create-kind-opaque`, `x-print-two-args` (los tres `CGEN`).
- Segunda puerta: **107/107** (99 rechazos + 8 controles); gate diferencial
  **26/26** en el mismo árbol.

## Actualización de M1 - corpus IR emitido por el lenguaje (2026-10-16)

- Fase D: el lenguaje ya emite su corpus (rama `r6-emit-ir` en el clon
  aparte, desde `5c01cea`; el checkout del usuario queda intacto). Son 89+1
  documentos IRv1 -67 programas de `tests/suite` + `imports.support.orb`, y 22
  de `tests/std`- de 5334302 bytes, pineados por
  `filesSha256 10f6e7d97977aaeec3018b9f8694e78dc7fee3ae614fa16cf4db27cc10b7f89b`
  en `tests/corpus/manifest.json`; el emisor es el canónico
  `1e6a378a6a67…` (`805cf61894ca…`).
- **Re-pin (2026-10-16, D-F0-10)**: la rama se mergeó en el `main` del usuario
  (`299eb09`, sin push). El corpus designado se reconcilió con el árbol a
  **93+1** (71+1 de `tests/suite` + 22 de `tests/std`, `IR_SPEC.md` §5.5) porque
  `main` había añadido cuatro programas de suite posteriores a la spec; el pin
  pasa a `filesSha256 0106d531…` con emisor `299eb09` (`36c42e20…`) y se midió
  en **7/93** (trinquete `r6BuildOk 7`, `match 7`). La corrida en
  `1e6a378`/`10f6e7d9` (6/89) queda arriba como la medición de ese pin.
- **Re-pin (2026-10-17, D-F0-11)**: `main` avanzó cuatro commits (`e75f2c7`
  H-ARENA, `8b9107a` retirada del Arena Oracle, `0a145cc` fix del cargador,
  `862f257` reorg de `compiler/`) y el pin pasa a `862f257`. El corpus **no se
  movió** (`filesSha256 0106d531…`, 93+1, 5419454 bytes; medido por R6 con punto
  fijo propio en `299eb09`, `8b9107a` y `862f257`) y la medición sigue en
  **7/93**. Canónico `9906A6F7…`; `runtime/orbit_api.h` `bf098c54…`;
  `runtime/runtime.h` `ffbd6a94…` (fila nueva); receta `86858c14…` sin cambio.
  Las puertas siguen verdes (31/31, 137/137, ratchet 7→7, sin drift de casos).
  Se documenta además que **ningún sha256 de artefacto local** (binario emisor,
  archivo de runtime) es un ancla estable: ambos embeben la ruta absoluta del
  directorio de build (D-F0-11).
- El diferencial del corpus (D-M1-17) es una **medición, no una puerta**. La
  corrida completa del 2026-10-16 (`--check --write-baseline`) sale con exit 0
  e imprime `ratchet holds: r6BuildOk 2 -> 2, match 2 -> 2`:

| Métrica | Valor |
|---|---|
| miembros / programas | 90 / 89 (+1 módulo de soporte) |
| oráculo compila y corre | 89/89 |
| R6 compila | 2/89 |
| R6 coincide (exit + stdout) | 2/89 |
| documentos con `unknown` en `registerTypes` | 84/90 |

- Los dos programas que coinciden son degenerados -un solo `ret` con literal
  `Int` y ningún registro-: `bom_utf8` (exit 0) y `nesting_legal` (exit 9). 40
  de los 41 opcodes presentes en el corpus no aparecen en ningún programa que
  R6 haya corrido completo.
- Rechazos: 87, todos `error[CGEN]` y ninguno `E_*` (el loader acepta el
  corpus entero). Primer rechazo por opcode (por posición del diagnóstico, o
  nivel de módulo cuando no la tiene): módulo 63, `load_var` 11, `call` 6,
  `decl_var` 6, `load_const` 1.

Mensajes completos de la corrida:

| Casos | Mensaje |
|---|---|
| 45 | register type unknown is unsupported in M1 |
| 11 | load_var requires a supported local, declared global, or union tag constant |
| 6 | decl_var requires a declared local and a matching scalar, list, void*, model, or function value register |
| 3 | parameter type list<string> is unsupported in M1 |
| 2 | global CHAR_SPACE is outside M1 literal int/bool support |
| 2 | global WALK_MAX_DEPTH is outside M1 literal int/bool support |
| 2 | M1 codegen does not support type kind enumeration |
| 2 | extern return type is unsupported in M1 |
| 2 | call target system_pid is not a declared function, model, or union variant |
| 2 | call target system_uptime is not a declared function, model, or union variant |
| 1 | call target imported_value is not a declared function, model, or union variant |
| 1 | call target print is not a declared function, model, or union variant |
| 1 | function value 1.5 is not declared in the module |
| 1 | model Bag_int field items has unsupported type list<int> |
| 1 | model Nested_float field grid has unsupported type list<list<float>> |
| 1 | model Post field tags has unsupported type list<string> |
| 1 | register type Shape is unsupported in M1 |
| 1 | register type object is unsupported in M1 |
| 1 | register type string_raw is unsupported in M1 |
| 1 | union Wrapped variant At payloads are outside the M1 native subset |

- Tres huecos, en orden de tamaño, registrados como preguntas abiertas en
  D-M1-17 (cita de la spec incluida):
  1. `unknown` en `registerTypes` (84/90): consecuencia de serializar antes de
     `inferRegisterTypes` (D-F0-08 P1); R6 lo rechaza por contrato (D-M1-09) y
     es el primer rechazo en 45 programas.
  2. `call` con O1 `String` (2917 llamadas en 74/90; las otras 6 son indirectas
     por `Register`, ninguna por `Sym`): la spec §3.1 paso 2 dice
     "`call` reads O1: `String`/`Sym` → named call"; R6 solo resuelve `Sym` y
     manda `String` al camino de variante (D-M1-12), así que rechaza llamadas a
     funciones declaradas en el mismo documento (`print`, `system_pid`,
     `system_uptime`, `imported_value`).
  3. Float/bool como `Sym` (412 `load_const` en 36/90): la spec §2.1 registra
     `Float` como "no dedicated constructor observed" y `Sym` crudo en C, pero
     R6 lee todo `Sym` de `load_const` como valor de función (D-M1-14). El
     corpus **no usa nunca `Float`** (0 ocurrencias).
- Gate diferencial **26/26** y suite del loader **107/107** sin cambios en el
  mismo árbol; `tests/corpus/README.md` documenta la medición, el pin y el
  trinquete.

## Actualización de M1 - target de `call` String y destino `void` (2026-10-16)

- Decisión: D-M1-18. El target de `call` se resuelve como dice la spec §3.1
  paso 2: `String`/`Sym` es llamada nombrada, en el orden función declarada →
  constructor de modelo (D-M1-11) → variante de unión (D-M1-12). Es el
  formulario del builder: el corpus emitido tiene 2917 `call` con O1 `String`
  en 74/90 documentos y **ninguna** con `Sym` (las otras 6 son indirectas por
  `Register`). De esos 2917 nombres, 2227 resuelven a una función declarada,
  177 a un modelo, 104 a una variante y 409 no están declarados en su
  documento (builtins/alias de la spec §3.1 pasos 4-6: `orbit_string_at` 130,
  `print` 121, `orbit_string_len` 78, `orbit_string_slice` 38,
  `orbit_object_*` 24, `orbit_db_*` 4, `orbit_string_find` 3, `system_*` 8),
  que siguen fail-closed. Sin solapes: ningún nombre cae en dos de los tres
  espacios de nombres.
- Misma entrada: un callee void puede llevar destino declarado `void` (paso 7:
  el destino no es necesario, no está prohibido). El builder siempre lo asigna
  y lo tipa `void` (205 llamadas void en 14 documentos, ninguna sin destino);
  un destino no `void` en una llamada void sigue siendo `CGEN`, y el emisor no
  escribe el retorno en un slot `void`.
- Artefactos: fixture `27-call-string-target` (exit 42, stdout
  `string-target`), cinco pines de rechazo (`x-call-string-target-arity`,
  `x-call-string-target-arg-type`, `x-call-string-target-dest`,
  `x-call-string-target-main` y `x-call-void-dest-type`, todos `CGEN`) y los
  controles `ok-call-void-dest` y el propio fixture 27.
- Gates: diferencial **27/27**; suite del loader **114/114** (104 rechazos + 10
  controles); `make_cases.py --check` sin drift.
- Medición del corpus (no puerta): **3/89** (antes 2/89);
  `tests/suite/imports.orb` (exit 37, opcodes `call` + `ret`) pasa a
  coincidencia y es el primer programa no degenerado que R6 completa. `call`
  deja de ser el primer rechazo de cualquier programa: el reparto queda en
  módulo 65, `load_var` 13, `decl_var` 6, `load_const` 1 y `ret` 1, con
  `unknown` en `registerTypes` todavía en 84/90 (47 primeros rechazos).
  Trinquete re-tomado tras `--check`: `r6BuildOk 3`, `match 3`.

## Actualización de M1 - literales del builder: `Sym` y `decl_var` inline (2026-10-16)

- Decisión: D-M1-19. `load_const` clasifica un `Sym` por su **gramática de
  texto**: `true`/`false` es el literal bool, un texto numérico con fracción o
  exponente es el literal float (finito, spec V-08) y cualquier otro texto
  sigue siendo un valor de función (D-M1-14). Un entero desnudo (`"7"`) no es
  un float literal. El censo del corpus pineado da **412 `load_const` con
  `Sym` en 36/90 documentos**: `true` 206, `false` 112, 28 spellings de float
  93 (`0.0`, `1.5`, `2.5e3`, `1.5E-3`, `100000000000000000000.0`) y **un solo
  valor de función** (`whisper`); gramática y tipo destino coinciden en los
  412. `Sym` no aparece en ningún otro slot de valor del corpus.
- Segundo hueco, hallado al medir la misma familia: el emisor de
  `decl_var`/`store_var` suponía que el inicializador era siempre un registro y
  copiaba un slot de registro, pero el builder **inlinea literales**
  (`val n = 7`, `val s = "x"`, `val b = true`: **299 sitios en 46
  documentos**, Int 212 / String 53 / Bool 34) sin que ninguno de los tres
  documentos que hoy coinciden lo ejerza, así que la medición no podía verlo:
  era una compilación incorrecta silenciosa (el local recibía palabras de un
  registro ajeno). El emit materializa ahora la constante en el slot del local
  para `Int`/`Bool`/`Float64`/`String`, conserva la copia para `Register` y el
  `leaq` para un `Symbol` de función; el loader ya tipaba los inicializadores
  literales, así que no se relajó ninguna puerta. Fixture 28 fue el testigo:
  antes del arreglo el diferencial leía `gcc exit=42, r6 exit=7`.
- Reportado y **no** arreglado (fuera de esta decisión; una parte es una
  pregunta de semántica): un operando literal dentro de una operación binaria
  (`eq` 884, `add` 737, `ne` 474, `sub` 123, `ge` 84, `lt` 83, `gt` 71,
  `le` 66, `mul` 39, `div` 35, `mod` 26 = 2622 sitios en 76/90 documentos)
  sigue rechazado con `operand 2 must be a valid register`; `add` con un
  operando `String` (376 sitios) es concatenación de strings, frontera U-17.
- Artefactos: fixtures `28-sym-literals` (exit 42, stdout vacío) y
  `29-decl-var-literals` (exit 42, stdout `ok`), ambos registrados como casos
  de aceptación; cinco pines de rechazo (`x-sym-literal-float-dest-type`,
  `x-sym-literal-bool-dest-type`, `x-sym-literal-not-finite`,
  `x-sym-integer-text-is-a-symbol`, `x-decl-var-literal-type`, todos `CGEN`) y
  tres controles nuevos (`ok-sym-bool-literal` exit 5,
  `ok-sym-float-literal` exit 6, `ok-decl-var-literal` exit 3).
- Gates: diferencial **29/29**; suite del loader **124/124** (109 rechazos + 15
  controles); `make_cases.py --check` sin drift.
- Medición del corpus (no puerta): **3/89** (las puertas module-level y
  `unknown` dominan). Cambio observable: `load_const` deja de ser primer
  rechazo de cualquier programa (el documento que paraba ahí avanza ahora a
  `ret`), con reparto módulo 65, `load_var` 13, `decl_var` 6 y `ret` 2.
  Trinquete re-tomado tras `--check --write-baseline`: `r6BuildOk 3`,
  `match 3`.
- Linaje de fixtures: los streams de instrucciones de 28 y 29 son la salida
  propia del emisor del pin para el `.orb` pareado; solo `registerTypes` se
  completa a los tipos concretos, porque el emisor serializa antes de
  `inferRegisterTypes` y deja `unknown` (hueco 1 de D-M1-17, D-M1-20).

## Actualización de M1 - tipos `unknown`: inferencia espejo del backend (2026-10-16)

- Decisión: D-M1-20. Antes de validar el módulo, R6 recorre cada función y
  recupera el tipo de un registro declarado `unknown` a partir de su
  instrucción definidora, espejando `inferRegisterTypes` (`c_backend.orb:562`)
  restringido a los opcodes del subconjunto nativo, en un punto fijo de tres
  pasadas. Solo se sustituye el texto exacto `unknown`; un tipo declarado no se
  toca y un registro que ninguna regla explica sigue rechazado con el mismo
  `CGEN`.
- Por qué: el backend del lenguaje declara cada registro como palabra
  `uintptr_t` sin tipo (`c_backend.orb:3735`) y castea en el punto de uso; R6
  emite x64 tipado y debe elegir `addl`/`addsd`, así que necesita el tipo. Los
  dos estrechamientos frente al original -la aritmética con operandos que no
  coinciden, y todo lo que el original resuelve por sus pasadas de "spreading"
  o el backend nativo no representa (tags de **enum**, `result_unwrap`,
  `union_get_data`, targets de `call` no declarados, `map`)- dejan el registro
  `unknown` en lugar de adivinar.
- Evidencia de la corrida (corpus `10f6e7d9`, `1e6a378`): de **6735 registros
  `unknown` en 84/90 documentos** se recuperan **5941 (88.2%)** y **40 de esos
  84 documentos quedan sin ningún `unknown`**; sobreviven 794, encabezados por
  `load_var` 492 (tags de enum en su mayoría), `call` 131, `result_unwrap` 71,
  `list_get` 32, `load_field` 21 y `union_get_data` 20.
- Artefactos: fixtures `30-inferred-model-fields` (exit 7) y
  `31-inferred-unary-neg` (exit 2), que son el stream textual del emisor **con
  sus `unknown` incluidos** (lo contrario de 28/29); cuatro rechazos nuevos
  (`x-infer-arith-unknown-operand`, `x-infer-unresolved-call`,
  `x-infer-unknown-load-var`, `x-infer-result-unwrap`, todos `CGEN`) y siete
  controles (`ok-infer-eq-bool` 5, `ok-infer-load-var` 3, `ok-infer-neg-chain`
  5, `ok-infer-call-return` 7, `ok-infer-arg-param` 33,
  `ok-infer-list-create` 8, `ok-infer-union-tag` 0).
- Gates: diferencial **31/31**; suite del loader **137/137** (113 rechazos + 24
  controles); `make_cases.py --check` sin drift; `migrate-m1-fixtures.py
  --check` 20/20.
- Medición del corpus (no puerta): `r6BuildOk` **6/89** (antes 3/89) y `match`
  **6/89**; `tests/suite/model_fields.orb` (exit 7),
  `tests/suite/model_two_fields.orb` y `tests/suite/unary_neg.orb` (exit 2)
  pasan a coincidencia byte a byte con el oráculo. De los 4 documentos que aún
  se detienen en `unknown` (`test_regex`, `float_literals`,
  `if_expression_return`, `list_index_out_of_range`) ninguno es inferible con
  estas reglas. El frente dominante pasa a ser el operando literal dentro de
  una operación binaria ya reportado por D-M1-19. Trinquete re-tomado tras
  `--check --write-baseline`: `r6BuildOk 6`, `match 6`.

## Actualización de M1 - operandos literales inline en las operaciones (2026-10-17)

- Decisión: D-M1-21. Los once opcodes `add`/`sub`/`mul`/`div`/`mod`/`eq`/`ne`/
  `lt`/`le`/`gt`/`ge` admiten un literal inline en operand1 y/o operand2, con
  los kinds medidos en el corpus (`Int`, `String`, `Bool`; **nunca `Float`**).
  La regla de tipos por opcode no cambia: el literal se tipa con el mismo
  `value_type` que un registro, así que un tipo que no encaja sigue siendo
  `CGEN`. Un `Float` inline, un literal fuera de esos once opcodes y un registro
  sin tipo siguen rechazados.
- Mecanismo: dos slots de 8 bytes bajo el área de resultados
  (`literal_scratch_offset`, con `result_area_bytes()` extraído de la aritmética
  del frame para que **ningún offset existente se mueva**); el preludio de la
  instrucción escribe el inmediato con la misma materialización que
  `load_const`/`decl_var` y `operand_slot()` devuelve el slot del registro o el
  del literal, de modo que la operación sigue siendo una lectura de slot.
- Referencia: `c_backend.orb:1174-1196` (`irVerifierOperandType` tipa el literal
  inline igual que un registro), `:3881` (`isStringOperand` acepta un
  `IRValue.String` inline), `:1321`/`:1354` (`verifyarithBinop`/`verifycmpBinop`)
  y `:802`/`:1049` (inferencia y emisión de `add`).
- Evidencia del corpus (pin `0106d531`): los primeros rechazos cuyo opcode es uno
  de los once caen **24 -> 6**, y esos 6 no son el literal: cuatro son residuo de
  D-M1-20 (un registro operando o destino que sigue `unknown`), uno es el
  `add` de `String` + `Int` de `peephole_folds` (el *string spreading* del
  lenguaje, no implementado en R6) y uno es el `ne` sobre bool de
  `list_element_type`. `r6BuildOk` **7 -> 21** y `match` **7 -> 21**
  (`arith`, `bool_and_or`, `compound_assignment`, `div_mod_signed`, `division`,
  `for_range`, `if_branch`, `model_mutation`, `numeric_literals`,
  `peephole_int_bool_only`, `strings_eq`, `ternary`, `while_break`,
  `while_counter`), **sin que ningún documento retroceda**.
- Frentes nuevos medidos (preguntas, no admisiones): el *string spreading* de
  `add` (qué texto produce el oráculo para el lado no-string) y `ne` sobre bool
  (el backend lo acepta; la regla de R6 para `ne` solo admite int/float/string).
- Puertas: diferencial **31/31 -> 32/32** (fixture 32, stream propio del emisor
  del pin con sus `unknown` incluidos, exit 42 y stdout `lit-ok\r\n\r\n`); loader
  **137/137 -> 142/142** (116 rechazos + 26 controles); `make_cases.py --check`
  sin drift (135 archivos de caso + manifest); `migrate-m1-fixtures.py --check`
  20/20; trinquete re-tomado (`r6BuildOk 21`, `match 21`). Controles negativos
  `x-lit-float-operand`, `x-lit-and-op-operand`, `x-lit-type-mismatch`; control
  positivo `ok-lit-operands` (exit 5), que reserva el scratch de resultados de
  lista y los slots de inmediatos en el mismo frame.
- Observación de entorno (afecta a la reproducibilidad de la medición): mientras
  se tomaba esta medición, el working tree del lenguaje **avanzó** de `862f257`
  (pin D-F0-11) a `ce87d508` con trabajo concurrente sin commitear
  (`M scripts/_common.py`, `M scripts/alive_check.py`, `?? scripts/cc_cached.py`).
  Durante esa ventana la envoltura de caché de C del harness pasó a `ORBIT_CC`
  multi-token entrecomillado, que el fp del pin no puede lanzar
  (`ERROR_INVALID_NAME`), y varias corridas del corpus dieron `oracle-fail 93`
  con `r6BuildOk 0`: el trinquete lo detectó y **se negó a escribir baseline**.
  La medición final se tomó con `ORBIT_CCACHE=0` (escotilla documentada del
  harness). El oráculo es el mismo binario de D-F0-11 (sha256 `10a16956.`) y el
  corpus emitido es **idéntico** (`filesSha256 0106d531.`), de modo que la
  medición corresponde al contenido del pin D-F0-11.
