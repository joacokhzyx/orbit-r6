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
  comentario `/* print */`, `c_backend.orb:2797-2802`; U-17).
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
  instrucción definidora, espejando `inferRegisterTypes` (`c_backend.orb:674`)
  restringido a los opcodes del subconjunto nativo, en un punto fijo de tres
  pasadas. Solo se sustituye el texto exacto `unknown`; un tipo declarado no se
  toca y un registro que ninguna regla explica sigue rechazado con el mismo
  `CGEN`.
- Por qué: el backend del lenguaje declara cada registro como palabra
  `uintptr_t` sin tipo (`c_backend.orb:3741`) y castea en el punto de uso; R6
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
- Referencia: `c_backend.orb:1176-1198` (`irVerifierOperandType` tipa el literal
  inline igual que un registro), `:3887` (`isStringOperand` acepta un
  `IRValue.String` inline), `:1323`/`:1356` (`verifyarithBinop`/`verifycmpBinop`)
  y `:804`/`:1051` (inferencia y emisión de `add`).
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

## Actualización de M1 - `load_var` de un parámetro (2026-10-17)

- Decisión: D-M1-22. `load_var` cuyo O1 nombra un **parámetro de la función**
  queda admitido: el destino se tipa `paramTypes[p]` y el valor es el propio
  parámetro, el mismo slot que lee su binding `arg`. El orden de resolución
  dentro de la función es parámetro, luego local de `decl_var`, luego global
  declarada, luego tag de unión. Nada más se relaja: un nombre que no es nada de
  eso sigue rechazado, el parámetro de **otra** función sigue rechazado (el
  parámetro es de ámbito de función, no un nombre de módulo) y un destino con
  tipo distinto de `paramTypes[p]` sigue rechazado.
- Referencia (blob pineado `compiler/emit/c_backend.orb 0c9dd786…`, re-anclado
  desde `2d64c8de…` por D-F0-13): la emisión
  de `load_var` (`:2600-2653`) cae a `func.params` y toma `paramTypes[p]` cuando
  el tipo propio de la variable es `unknown`/vacío (`:2625-2633`), el cast sale
  de ese tipo (`:2635-2651`) y un `Sym` se rinde como el nombre tal cual
  (`generateValue`); la inferencia (`:777-802`) consulta `func.params` **antes**
  que los locales; el binding `arg` (`:2862-2875`) emite `reg[dest] =
  params[index]`, el mismo valor. La spec (`docs/IR_SPEC.md`, blob `371f873e…`)
  §1.2 dice lo mismo (`load_var` = `D <- variable named by O1`; `arg` con `D` =
  índice de parámetro, `D <- parameter name`), aunque sus citas internas de
  línea apuntan a una revisión anterior del backend, no al blob pineado.
- Mecanismo: R6 ya derrama el parámetro `p` a su home `(registerCount + isMain +
  p + 1) * 8` en el preludio y ya lee ese slot para el binding `arg`, y su
  inferencia ya resolvía el nombre (`param_info` es ahora la única definición de
  esa búsqueda, extraída del pase de inferencia). El cambio es una rama de
  parámetro en la validación y otra en la emisión de `load_var`, que copia el
  home al destino con el par `movq`/`movsd` que ya usa `arg`. No cambia la
  aritmética del frame ni se mueve ningún offset.
- Evidencia del corpus (pin `0106d531`): decodificados los 23 primeros rechazos
  de `load_var` en su offset de diagnóstico, **23/23 son lecturas de parámetro**
  y nada más (tipos `int` 16, `string` 5, `float` 1, `bool` 1; 14 funciones
  distintas, con `intToString` 8, `getEnv` 2 y `addSeconds` 2). Los primeros
  rechazos con opcode `load_var` caen **23 -> 0**, `r6BuildOk` y `match` suben
  **21 -> 23** (`tests/suite/float_boundaries.orb` y `tests/suite/fn_recursive.orb`
  pasan a coincidencia byte a byte), **sin retrocesos**. Los otros 21 programas
  avanzan a su siguiente muro: las paredes son conjuntivas.
- Puertas: diferencial **32/32 -> 33/33** (fixture 33, stream propio del emisor
  del pin leyendo parámetros int/string/bool/float por nombre, exit 42 y stdout
  vacío); loader **142/142 -> 146/146** (116 rechazos + 30 controles);
  `make_cases.py --check` sin drift (138 archivos de caso + manifest);
  `migrate-m1-fixtures.py --check` 20/20; trinquete re-tomado (`r6BuildOk 23`,
  `match 23`). Controles negativos `x-load-var-foreign-param` y
  `x-load-var-param-mismatch`; control positivo `ok-load-var-param` (exit 42).
- Mensaje: el diagnóstico pasa a `load_var requires a supported local,
  parameter, declared global, or union tag constant`, para nombrar la fuente que
  ahora admite. El código (`CGEN`) y el contrato fail-closed no cambian; el
  mensaje histórico citado en las mediciones de D-M1-20/21 se deja como registro
  de aquellas corridas.

## Actualización de M1 - `add` de string: spread, concat y el arena fuera de `main` (2026-10-18)

- Decisión: D-M1-23. `add` con un `string` en **un solo** lado dispersa el otro
  operando (un escalar `int`/`float`/`bool`) con el conversor del runtime y
  concatena; y `add` de dos strings se baja en **cualquier** función, no solo en
  `main`. La propia medición de la ronda destapó y corrigió dos defectos
  preexistentes del lowering: D-M1-24 (los ids de etiqueta de las lecturas de
  lista perdían el ordinal de función porque `string_id` se asignaba a locales de
  32 bits, y dos funciones podían acuñar la misma etiqueta `.LCr6_f0_*`) y
  D-M1-25 (la longitud de `list_len` se guardaba a 32 bits en un slot `int` que
  sus consumidores leen a 64).
- Referencia (pin `17ed624`, leído con `git cat-file blob`): `c_backend.orb:2664-2670`
  (el brazo `IROpcode.add`: si cualquiera de los operandos es string, los dos pasan
  por `stringifyConcatOperand` y el resultado es `orbit_string_concat(arena, s1,
  s2)`), `:4347-4384` (`stringifyConcatOperand`), `:3887-3914` (`isStringOperand`,
  el mismo dato que el `type_at` de R6 ya tiene), `:193`/`:2227` (el prototipo y
  la definición de `orbit_main`, cuyo primer parámetro es `OrbitArena* arena`;
  en `ce87d508` eran `_init_arena` más el global `:387` y la asignación
  `:2242`, líneas que `17ed624` borró), y el
  runtime `selfhost.c:170` (`orbit_int_to_string_selfhost`, sin arena),
  `collections.c:716`/`:730`/`:815`/`:831` (int/float/bool -> string y
  `orbit_string_concat`), `orbit_api.h:388-391` (las declaraciones). D-M1-25
  descansa en que `OrbitList.len` es un `size_t` (`orbit_api.h:219`) y
  `orbit_list_len` devuelve `size_t` (`:519-521`).
- Mecanismo: dos slots de 8 bytes por debajo de los inmediatos de D-M1-21
  (`spread_scratch_offset`), elegidos para que **ningún offset existente se
  mueva**; el arena se lee del parámetro `_init_arena` en `main` y del global
  `r6_arena` fuera de él, y `module_needs_arena_global` pasa a activarse también
  por un `add` cuyo destino es `string`. Los dos defectos son de ancho: los
  parámetros de `emit_list_read` y sus dos llamadas pasan a `unsigned long long`
  (D-M1-24) y `R6_OP_LIST_LEN` escribe el slot con `movq` (D-M1-25).
- Evidencia del corpus (pin `0106d531`): el primer rechazo con opcode `add` cae
  **7 -> 0**, `r6BuildOk` y `match` suben **23 -> 27** (`interpolation`,
  `integer_literals` y `function_pointers` por D-M1-23; `for_index_labels` por
  D-M1-24/D-M1-25), **sin retrocesos**. Los cuatro programas liberados empujan el
  nivel de módulo 35 -> 37: las paredes siguen siendo conjuntivas.
- Puertas: diferencial **33/33 -> 35/35** (fixture 34, el stream del emisor
  dispersando int/float/bool en `main` y en un callee, exit 42 y stdout
  `[spread]` + CRLF; fixture 35, un bucle indexado sobre una lista en una función
  no-`main`, exit 42 y stdout vacío); loader **146/146 -> 153/153** (122 rechazos
  + 31 controles); `make_cases.py --check` sin drift (143 archivos de caso +
  manifest); `migrate-m1-fixtures.py --check` 20/20; trinquete re-tomado
  (`r6BuildOk 27`, `match 27`). Controles negativos `x-add-spread-dest`,
  `x-add-spread-pointer`, `x-add-spread-float-literal`, `x-add-string-dest`;
  control positivo `ok-add-string-spread` (exit 42).
- Observación: D-M1-24 y D-M1-25 no cambian ninguna regla de validación - eran
  código mal emitido en módulos que R6 ya aceptaba, y ninguno de los dos aparecía
  en el corpus porque ningún programa anterior leía una lista en una función
  distinta de `main`.
- Mensaje: `add` gana dos diagnósticos (`add spread requires a string destination
  and an int, float, or bool scalar operand` y `string add requires a string
  destination`); el `add` de int/float conserva el suyo. El código (`CGEN`) y el
  contrato fail-closed no cambian.

## Actualización de M1 - `print` y el arena de los extern del runtime (2026-10-19)

- Decisiones: D-M1-26 (`print`) y D-M1-27 (extern del runtime con el arena como
  primer parámetro C). D-M1-27 la destapó la medición de la propia ronda: tras
  D-M1-26 tres programas compilaron por primera vez y fallaron en ejecución.
- D-M1-26: `print` es el target de llamada más frecuente del corpus (134 sitios
  en 32/93 programas). El lowering del pin lo intercepta **por nombre** antes de
  resolver ninguna declaración (`c_backend.orb:2977-3005`), lo tipa `void`
  (`:3917`) y el macro `runtime/orbit_api.h:293` demuestra que no existe símbolo
  `print` (era el único `LINK` del corpus). R6 replica: un argumento imprime una
  línea según su clase (`int`/`bool` -> `%lld`, `string` -> `%s`, `float` ->
  `orbit_float_to_string(arena, v)` y `%s`), cualquier otra aridad es el no-op que
  consume **todos** los pendientes (`:2999-3004`), y el destino se acepta pero
  **nunca se escribe** (el builder lo tipa `int` desde un `extern fn print(...)`
  en tres programas, 8 sitios, y ningún programa del corpus lo lee después).
- D-M1-27: el backend C antepone `arena` a los nombres de `functionNeedsArena`
  (`c_backend.orb:4416-4437`; desde `17ed624` también a toda función no externa
  del módulo, `:4447-4458`; el prepend en `:3097-3104`) porque el runtime
  declara `orbit_string orbit_int_to_string(OrbitArena*, orbit_int)`
  (`runtime/orbit_api.h:388-389`, definiciones en `runtime/collections.c:716`/
  `:730`). La declaración IR no repite el arena. R6 emite el arena en RCX y
  desplaza cada argumento una posición, que es posicional también para XMM
  (`orbit_float_to_string` deja el valor en XMM1).
- Mecanismo: `extern_needs_arena` copia literal la lista del pin; la inyección
  exige nombre **y** `isExtern` declarado; `max_outgoing_args` suma un slot al
  outgoing (el arity del IR sigue siendo lo que consume la pila de pendientes) y
  `module_needs_arena_global` se activa también por una llamada de la familia
  para que `r6_arena` exista fuera de `main`.
- Evidencia del corpus (pin `filesSha256 0106d531`, 93+1): `r6BuildOk` y `match`
  suben **27 -> 35**, `byVerdict` queda `match 35`, `r6-build-fail 58`,
  `support 1` - **ningún** `r6-differs` - y por código `CGEN 64 -> 57`, `LINK
  1 -> 0`, `E_OPCODE 1 -> 1`. Los ocho programas liberados son `test_assert`,
  `test_convert`, `test_hash`, `test_log`, `test_time`, `float_literals`,
  `if_expression_return` y `scope_shadow_clobbers_outer`; los tres primeros solo
  pasaron a *match* con D-M1-27. El primer rechazo de nivel de módulo cae
  **37 -> 28** y `list_create` sube **4 -> 5** (un programa llega ahora a esa
  pared), sin retrocesos.
- Puertas: diferencial **35/35 -> 37/37** (fixture 36, `print` string/int/float
  en `main`, exit 0 y `hello`/`42`/`1.5` con CRLF; fixture 37,
  `orbit_int_to_string(42)` y `(-7)`, exit 0 y `42`/`-7`); loader **153/153 ->
  163/163** (125 rechazos + 38 controles); `make_cases.py --check` sin drift
  (151 archivos de caso + manifest); `migrate-m1-fixtures.py --check` 20/20;
  trinquete re-tomado en `r6BuildOk 35`, `match 35`.
- Controles: `x-builtin-name-outside-print` (`println` sigue siendo target
  desconocido), `x-print-symbol-target` (un `Sym("print")` no es el builtin),
  `x-arena-extern-undeclared` (el arena solo se inyecta a un extern declarado);
  positivos `ok-print-noop` (aridades 0 y 2, exit 7 sin salida),
  `ok-print-inline-literals`, `ok-print-nonmain-float` (el arena fuera de
  `main`), `ok-print-shadowed-extern` (`extern fn print` con destino `int` que
  nunca se escribe) y `ok-arena-extern-float` (el desplazamiento posicional de
  XMM1).
- Observación: D-M1-27 no es un cambio de validación sino de ABI emitida: los
  tres programas ya se aceptaban y se estaban **mal compilando** (crash o
  `assert: zero -- got "", want "0"`). La medición los convirtió en evidencia.
  Un `print` con destino no-void ya no se rechaza porque el lowering del pin
  tampoco lo mira; lo que acota la admisión es el nombre, la forma `String` del
  target y las clases medidas del argumento.
- Mensaje: aparece `print argument has no measured printable type` (defensa en
  profundidad: el `arg` ya restringe los escalares a int/float/string/bool); el
  código sigue siendo `CGEN` y el contrato fail-closed no cambia.

## Actualización de M1 - `ret` de una función `void` (2026-10-19)

- Decisión: D-M1-28. `return f()` sobre un callee `void` es la forma natural de
  una tail call; el builder guarda en `operand1` el registro destino de la
  llamada, y R6 lo rechazaba con `void ret must not carry a value`.
- Referencia (pin `17ed624`, leído con `git cat-file blob`):
  `c_backend.orb:3435-3445` (la forma registro emite `return;` y no lee el
  registro; el comentario del pin explica que la llamada ya se emitió como
  sentencia y que leerlo produce C que no compila) y `:3466-3471` (la forma no
  registro comparte la rama). `main` se decide antes del tipo declarado
  (`:3426`, `:3457`), pero R6 ya exige `main` sin parámetros y con retorno
  `int` (`win64.c:779-780`), así que esa rama no es alcanzable.
- Admisión: en una función `void`, `ret` acepta cualquier `operand1` y lo
  ignora; el emisor no carga nada y emite `leave; ret`. En una función no
  `void` no cambia nada: el valor debe coincidir con el tipo declarado, el
  registro debe existir y un `result` sigue rechazado.
- Evidencia del corpus (pin `filesSha256 0106d531`): `void_return_void_call`
  pasa a *match*; `r6BuildOk`/`match` suben **35 -> 36**, el primer rechazo
  `ret` cae **2 -> 1** (queda el retorno de `result`, por diseño), sin
  retrocesos.
- Puertas: diferencial **37/37 -> 38/38** (fixture 38, tail call encadenada y
  forma sentencia, exit 42 y `T`/`T`); loader **163/163 -> 166/166** (126
  rechazos + 40 controles); `make_cases.py --check` sin drift (153 archivos de
  caso + manifest); `migrate-m1-fixtures.py --check` 20/20 (los pares nuevos se
  omiten por ser posteriores a la base); trinquete re-tomado en `r6BuildOk 36`,
  `match 36`.
- Controles: `x-ret-type-mismatch` (una función `int` que devuelve un string
  sigue rechazada) y `ok-void-ret-value` (el `ret` lleva el destino `int` de la
  llamada `void` a `print`, que no se lee).
- Mensaje: desaparece `void ret must not carry a value`; el resto de los
  diagnósticos de `ret` no cambia y el código sigue siendo `CGEN`.

## Actualización de M1 - los helpers de string sin declaración (2026-10-19)

- Decisión: D-M1-29. Dos primeros rechazos de la misma familia: el builder
  emite `call` con destino String a `orbit_string_len` / `orbit_string_at` /
  `orbit_string_slice` **sin declaración** en el módulo (el backend C ve los
  prototipos por las cabeceras del runtime), y `.len()` sobre un string llega
  al **opcode** `list_len` cuando el builder no conocía el tipo del receptor.
- Referencia (pin `17ed624`, leído con `git cat-file blob`):
  `runtime/orbit_api.h:385` (`orbit_string_at`), `:386`
  (`orbit_string_slice`, que está en `functionNeedsArena`,
  `c_backend.orb:4416-4437`, por lo que D-M1-27 le antepone el arena) y
  `:411-413` (`orbit_string_len`, `ORBIT_INLINE`, `s ? strlen(s) : 0`: no hay
  símbolo en `libruntime_net.a`, `nm` lista `at`/`slice` como `T` y no lista
  `len`). El opcode `list_len` consulta `isStringOperand` y emite
  `orbit_string_len` (`c_backend.orb:3615-3621`).
- Admisión: una tabla de tres firmas consultada sólo cuando el módulo no
  declara el nombre y sólo para el destino String que emite el builder; el
  arena de `slice` sale de la tabla de D-M1-27; `len` se baja inline (misma
  secuencia `strlen` con guarda de nulo) tanto en la forma `call` como en el
  opcode `list_len` con receptor string, porque no existe símbolo al que
  llamar.
- Fail closed: un nombre String fuera de la tabla sigue siendo `call target
  ... is not a declared function`; la forma Symbol nunca resuelve por la tabla;
  se exige la aridad de la cabecera; `list_len` con un receptor que no es lista
  ni string sigue rechazado.
- Evidencia del corpus (pin `filesSha256 0106d531`), medida en dos pasos porque
  la segunda puerta sólo apareció al abrir la primera: los intrínsecos solos
  llevan el corpus **36 -> 37** (`if_expression_else_if` pasa a *match*, las
  nueve primeras puertas desaparecen, sin retrocesos); la rama string de
  `list_len` suma `strings_concat` y `strings_escape` y cierra en **39/93
  match, 39/93 build**. El primer rechazo `call` cae de 11 a 3 y el de
  `list_len` desaparece. Trinquete re-tomado en 39.
- Puertas: diferencial **38/38 -> 39/39** (fixture 39: `.len()`, `.at()` y
  `.slice()` a través de parámetros, exit 0 y `5`/`90`/`bc`); loader
  **166/166 -> 171/171** (129 rechazos + 42 controles); `make_cases.py
  --check` sin drift (157 archivos de caso + manifest);
  `migrate-m1-fixtures.py --check` 20/20.
- Controles: `ok-list-len-string` (la forma opcode sobre un registro string,
  exit 3), `x-intrinsic-symbol-target` (el destino Symbol sigue sin declarar),
  `x-intrinsic-arity` (aridad 2 contra una firma de 1) y `x-list-len-bool` (un
  receptor bool sigue rechazado).
- Mensaje: desaparecen `call target orbit_string_len/at/slice is not a declared
  function...` y `list_len requires a list register...`; el nuevo texto es
  `list_len requires a list or string register and an int destination` y el
  código sigue siendo `CGEN`.

## Actualización de M1 - la lista vacía se crea opaca (2026-10-19)

- Decisión: D-M1-30. Primer rechazo: `list_create requires an 8-byte slot size,
  a kind in 1..4, a list destination, and an int capacity` en cinco programas
  (`list_push`, `list_index_out_of_range`, `list_element_type_annotation`,
  `test_process`, `test_process_exit`). Los cinco crean con `elemKind 0` y
  destino `list`: es el literal de lista vacía (`val xs = []`), cuyo tipo de
  elemento lo decide el primer push, no el literal.
- Referencia (pin `17ed624`, leído con `git cat-file blob`): el brazo
  `list_create` del backend C pasa `instr.elemKind` tal cual a
  `orbit_list_create_typed(arena, sizeof(void*), capacity, kind)`
  (`c_backend.orb:3586-3591`); `orbit_list_create` es
  `orbit_list_create_typed(..., ORBIT_ELEM_OPAQUE)` (`collections.c:170`), el
  primer push/set tipado adopta el kind (`collections.c:102-116`) y una lectura
  acepta una lista opaca porque el tipo estático del llamante es la única
  cuenta de su contenido (`collections.c:118-128`).
- Admisión: `list_create` acepta `elemKind 0` además de 1..4; la llamada ya
  reenvía el kind, así que no cambia el lowering. El destino sigue teniendo que
  ser lista, el slot de 8 bytes y la capacidad un registro o literal `int`.
- Fail closed: el kind 5 (ref) sigue rechazado en el `create` (no hay decodificador
  nativo de slot) y el **acceso** opaco sigue fuera: `list_get`/`list_set`/
  `list_push` exigen kind 1..4, así que el `list_get` con `elemKind 0` y destino
  `unknown` (el caso E-03 es el kind 5; éste es el kind 0, pineado como
  `x-list-get-kind-opaque`) sigue siendo un rechazo `CGEN`, igual que la
  puerta `register type unknown`; `x-list-set-kind-opaque` y
  `x-list-push-kind-opaque` cierran las otras dos puertas de escritura.
- Sustituye D-M1-16/E-04 **sólo para el create**: la tabla del borrador de la
  spec leía "accesor sin tipo + destino void*" como una sola forma, y el corpus
  muestra que el builder separa las dos mitades. `x-list-create-kind-opaque`
  pasa a ser el control de aceptación `ok-list-create-opaque` y el rechazo se
  re-pinea en el kind 5 como `x-list-create-kind-ref`; E-03
  (`x-list-get-kind-ref`) y E-06 (`x-print-two-args`) no cambian.
- Controles: fixture `40-list-create-opaque` (la forma `filled` de
  `tests/suite/list_push.orb`: create 0, push 41/7 con kind 2, dos lecturas
  tipadas; exit 0), `ok-list-create-opaque` (el mismo ida y vuelta a mano, exit
  41), `x-list-create-kind-ref` (kind 5 sigue `CGEN`) y
  `x-list-get-kind-opaque`/`x-list-set-kind-opaque`/`x-list-push-kind-opaque`
  para las tres puertas de acceso.
- Evidencia del corpus (pin `filesSha256 0106d531`): **39 -> 41/93** match y
  build (`list_index_out_of_range` y `test_process_exit` pasan a coincidir; los
  otros tres avanzan sin retrocesos: dos llegan a `register type unknown` y uno
  a un `list_push` con `elemKind 0`). El primer rechazo `list_create` cae de 5 a
  0 y el de `call` queda en 3. Trinquete re-tomado en 41.
- Puertas: diferencial **39/39 -> 40/40** (fixture 40, exit 0); loader
  **171/171 -> 176/176** (132 rechazos + 44 controles); `make_cases.py --check`
  sin drift (161 archivos de caso + manifest); `migrate-m1-fixtures.py --check`
  20/20.
- Mensaje: desaparece `list_create requires ... a kind in 1..4 ...`; el nuevo
  texto es `list_create requires an 8-byte slot size, a kind in 0..4, a list
  destination, and an int capacity` y el código sigue siendo `CGEN`.

## Actualización de M1 - las comparaciones siguen los brazos del pin (2026-10-20)

- Decisión: D-M1-31. Primer rechazo: `ne requires matching int/float/string
  operands; relational ops require int/float and bool destination` en tres
  programas: `if_expression_value` en `c > 4` (bool contra int),
  `list_element_type` en `flags.get(0) != true` (bool contra bool) y
  `peephole_folds` en `n == "5"` (int contra string).
- Referencia (pin `17ed624`, leído con `git cat-file blob`): `eq` y `ne`
  (`c_backend.orb:2747-2760` y `:2761-2774`) comparan dos strings con `strcmp`,
  promueven a `orbit_float` cuando un lado es float y comparan `uintptr_t` en
  el resto; un bool cae en ese resto. `lt`/`le`/`gt`/`ge`
  (`c_backend.orb:2783-2814`) no tienen brazo de string: double si hay un
  float e `intptr_t` si no, con la razón del signo en `:2775-2782`.
- Admisión: `eq`, `ne` y las cuatro relaciones aceptan operandos `bool`. Un
  string sólo es operando de `strcmp` contra otro string; en `eq`/`ne` un
  string contra un int es la vía de palabras. El destino sigue siendo `bool`.
- Lowering: un slot `bool` guarda el valor en el byte bajo. `load_const` y
  los resultados de comparación lo escriben con 32 bits (`movl`), así que la
  mitad alta puede quedar sucia; una lectura de lista bool escribe 64. La comparación
  normaliza el lado bool con `movzbl`, que es el valor de la C del pin
  `(uintptr_t)(bool)`, y deja el lado int en el destino del `cmp` para que
  `setl`/`setg` conserven la dirección.
- Fail closed: un float junto a un int sigue rechazado
  (`x-compare-float-int`: el brazo del pin escribe `(orbit_float)(v)` y R6 no
  tiene conversión int-a-double); un string en una relación sigue rechazado
  (`x-relational-string`: el pin no tiene ese brazo); un bool contra un string
  queda fuera porque ningún programa del corpus los compara.
- Controles: fixture `41-bool-and-word-comparisons` (`gt` bool/int, `ne`/`eq`
  bool/bool, `eq`/`ne` int/string, `ne` string/string y `ne` float/float; exit
  0), `ok-compare-bool-word` (`true > 0`, exit 41), `ok-compare-string-word`
  (`5 == "5"` es falso, exit 41) y `ok-compare-int-bool` (`1 > false`,
  exit 41).
- Evidencia del corpus (pin `filesSha256 0106d531`): **41 -> 43/93** match y
  build. `if_expression_value` y `peephole_folds` pasan a coincidir;
  `list_element_type` avanza del `ne` a `register type unknown` sin
  retrocesos. Los primeros rechazos `gt`, `ne` y `eq` caen a 0 y
  `(module-level)` sube de 36 a 37. Trinquete re-tomado en 43.
- Puertas: diferencial **40/40 -> 41/41** (fixture 41, exit 0); loader
  **176/176 -> 182/182** (134 rechazos + 48 controles); `make_cases.py --check`
  sin drift (165 archivos de caso + manifest); `migrate-m1-fixtures.py --check`
  20/20.
- Mensaje: desaparece `ne requires matching int/float/string operands...`; los
  nuevos textos son `eq and ne require int, float, bool, or string operands and
  a bool destination; a string is compared with strcmp only against another
  string` y `relational ops require int, float, or bool operands and a bool
  destination; ne also takes strings`, ambos con código `CGEN`.

## Actualización de M1 - un deletreo de lista es el puntero de lista (2026-10-20)

- Decisión: D-M1-32. Primer rechazo: `parameter type list<int> is unsupported
  in M1` en `list_param_element_types`, `parameter_type_fallback` y
  `test_time_ns`; `parameter type list<string> is unsupported in M1` en
  `std_lists_generic`, `test_collections` y `test_collections_boxed`; y
  `parameter type list is unsupported in M1` en `call_arg_types`.
  `generic_functions` paraba en la pared vecina, `arg value type is unsupported
  in M1`, una lista pasada como argumento de llamada.
- Referencia (pin `17ed624`, leído con `git cat-file blob`):
  `c_backend.orb:478` responde `OrbitList*` para `list`, `List` y `OrbitList*`.
  El comentario de `:479-482` lo explica: el sufijo dice qué hay DENTRO de la
  lista, una propiedad de los valores y no del puntero, así que el tipo C no
  cambia. Las líneas `:483-499` aplican la regla a cualquier deletreo
  parametrizado a cualquier profundidad y deciden por la base: `list`/`List`
  dan `OrbitList*`, `map`/`Map` dan `OrbitMap*`. Las lecturas de `:606-611`
  eligen la función de runtime por `elemKind`, así que el tipo del elemento
  viaja con la operación y no con el puntero.
- Admisión: un tipo de parámetro, un tipo de retorno y un tipo de registro
  pueden deletrear una lista como `list`, `List`, `list<...>` o `List<...>` a
  cualquier profundidad. `r6_codegen_win64` los reescribe a `list` en esas tres
  posiciones ABI antes de que las lea cualquier comprobación, y libera el
  string que posee el módulo. Nada más se reescribe: modelo, unión, result,
  tipo de función y `map` conservan su deletreo.
- Lowering: un parámetro lista llega como la palabra puntero en su registro GP
  y el prólogo lo mueve como cualquier palabra no float. `list_get`,
  `list_set`, `list_push` y `list_len` siguen despachando por el `elemKind` de
  la instrucción.
- Fail closed: un parámetro `map<string, int>` sigue rechazado
  (`x-param-map-spelling`: misma sintaxis parametrizada con otra base, y el pin
  responde `OrbitMap*`, un tipo que R6 no baja); un parámetro de extern sigue
  siendo escalar (`x-extern-list-param`: R6 baja los extern sólo desde el
  subconjunto escalar y ningún extern del corpus toma una lista); un deletreo de
  valor de función `fn(list)->int` sigue rechazado (`x-fn-type-nonscalar`,
  D-M1-14) porque el canonicalizador reescribe el deletreo entero y nunca un
  token dentro de `fn(...)`.
- Controles: fixture `42-list-parameter-spellings` (parámetro `list<int>`,
  parámetro `list<string>`, retorno `list<int>`, un resultado de `call` con
  tipo `list<int>` y un parámetro `list<list<int>>` que mide su longitud con
  `list_len`; exit 0), `ok-param-list-spelling` (41 entra por el
  parámetro `list<int>` y sale como elemento; exit 41) y
  `ok-list-capital-spelling` (un registro `List` creado, con push, y medido;
  exit 41).
- Evidencia del corpus (pin `filesSha256 0106d531`): **43 -> 44/93** match y
  **43 -> 45/93** build. `list_param_element_types` pasa a coincidir.
  `test_time_ns` compila y ahora difiere en su propio assert 16:
  `2147483647 + 1` envuelve a int32 en el pin y sigue en 64 bits en R6, así que
  `sinceNsBetween(0, pastCap)` responde 2147483648 en vez de -1. Es el primer
  `r6-differs` del corpus y es la pared del ancho de int, no esta admisión.
  `generic_functions` deja la pared del `arg` y para en `register type
  unknown`; `std_lists_generic` y `test_collections` pasan la puerta de
  parámetros y paran en `register type unknown`; `test_collections_boxed` para
  en `list_push` kind 0; `call_arg_types` y `parameter_type_fallback` paran en
  sus parámetros de modelo (`Box`, `Point`). Los
  primeros rechazos `(module-level)` bajan de 37 a 35 y `list_push` sube de 1 a
  2. Trinquete re-tomado en 45/44.
- Puertas: diferencial **41/41 -> 42/42** (fixture 42, exit 0); loader
  **182/182 -> 187/187** (136 rechazos + 51 controles); `make_cases.py --check`
  sin drift (170 archivos de caso + manifest); `migrate-m1-fixtures.py --check`
  20/20.
- Mensaje: desaparece `parameter type list... is unsupported in M1`; quedan
  `parameter type Box is unsupported in M1` (`call_arg_types`) y
  `parameter type Point is unsupported in M1` (`parameter_type_fallback`), los
  parámetros de modelo, todavía con código `CGEN`.

## Actualización de M1 - una constante de módulo es un #define (2026-10-20)

- Decisión: D-M1-33. Primer rechazo: `global CHAR_SPACE is outside M1 literal
  int/bool support` en `global_const` y `ast_function_body_is_a_block`;
  `global WALK_MAX_DEPTH is outside M1 literal int/bool support` en `test_fs` y
  `test_imports`.
- Referencia (pin `17ed624`, leído con `git cat-file blob`):
  `c_backend.orb:398-401` y `:1635` escriben `#define <nombre> <texto>` en el
  preámbulo por cada global; `:1649-1653` (`cDefineLiteral`) devuelve el payload
  `String` tal cual, y `:1641-1648` dice que el builder guarda el literal en un
  `IRValue.String` para que un registro no acabe en un `#define`; `:2406-2415`
  confirma que un nombre de nivel superior ES ese define. Los 21 globales del
  corpus llegan con `typeName: "unknown"` y el texto decimal en `value`.
- Admisión: un global con `typeName` `unknown` o vacío y valor `String` de un
  `-` opcional y dígitos decimales, dentro de int32, es una constante `int`. La
  clave ausente no llega aquí: el loader la rechaza con `E_SCHEMA`. Un tipo
  declarado `int`/`bool` conserva su regla. `load_var` ya emitía el valor como
  inmediato: la misma sustitución que hace el `#define`.
- Fail closed: un tipo declarado no escalar sigue rechazado sea cual sea el
  texto (`x-global-declared-string`); un global `unknown` con texto no decimal
  sigue rechazado (`x-global-unknown-text`); un texto fuera de int32 se rechaza
  en el mismo parseo (`x-global-int-overflow`, `x-global-int-underflow`). `long`
  mide 32 bits en este destino, así que `strtol` satura en `LONG_MAX`/`LONG_MIN`
  sin salir de int32: el parseo es `strtoll` con `ERANGE`, y los bordes
  `2147483647` y `-2147483648` siguen construyendo y coincidiendo con el
  oráculo (`ok-global-int32-max`, exit 2147483647; `ok-global-int32-min`, exit
  0x80000000). `0x20` y `+5` son literales int de C que el pin aceptaría y R6
  rechaza: huecos declarados, no divergencias.
- Controles: fixture `43-global-constant-type` (`val SPACE = 32`,
  `const LIMIT = 3`, `val NEG = -2`; los imprime, los compara y los suma; salida
  `32\r\n3\r\n-2\r\n`, exit 0) y `ok-global-unknown-type` (43 + -2 = 41 por
  exit). Los rechazos son `x-global-unknown-text`, `x-global-declared-string`,
  `x-global-int-overflow` y `x-global-int-underflow`.
- Evidencia del corpus (pin `filesSha256 0106d531`): **44 -> 45/93** match y
  **45 -> 46/93** build. `global_const` pasa a coincidir.
  `ast_function_body_is_a_block` avanza a `union ASTNode variant Program
  payloads are outside the M1 native subset`, `test_fs` a `extern return type is
  unsupported in M1` y `test_imports` a `register type unknown is unsupported in
  M1`. Los primeros rechazos `(module-level)` bajan de 35 a 34; `register type
  unknown` sube de 12 a 13, `extern return type` de 2 a 3 y desaparecen los
  cuatro mensajes `global ... is outside M1 literal int/bool support`.
- Puertas: diferencial **42/42 -> 43/43** (fixture 43, exit 0); loader
  **187/187 -> 193/193** (140 rechazos + 53 controles); `make_cases.py --check`
  sin drift (175 archivos de caso + manifest); `migrate-m1-fixtures.py --check`
  20/20. Trinquete re-tomado en 46/45.
- Mensaje: desaparece `global ... is outside M1 literal int/bool support`; sigue
  `register type unknown is unsupported in M1` (13 programas, 11 de ellos en
  `list_get` con `elemKind 0`).

## Actualización de M1 - una enumeración es un `int` (2026-10-22)

D-M1-35. El backend del pin emite `typedef enum { Name_TAG_Variant = <índice> }`
para un tipo `enumeration` (`c_backend.orb:1692-1706`), no emite aviso cuando
la comparación es enum contra `int` o enum contra enum (`:1362-1363`), y deja
que el compilador de C dé 4 bytes con alineación 4 a un campo de ese tipo,
igual que a un `int`. La ronda admite el tipo con el lowering de un entero.

- Forma en el pin: `{"name","kind":"enumeration","variants":[...],
  "richVariants":["",...],"methods":[]}`, una variante rica vacía por nombre.
  El builder escribe la constante de una variante como `load_var` de
  `Name_TAG_Variant`; el registro llega con el nombre del enum en el front del
  propio compilador (`front/token.ir`) y con `unknown` en el fixture, donde
  `infer_dest_type` le da `int`.
- Cambio: `canonicalize_enum_types` reescribe todo uso del nombre a `int`
  antes de que lo lea cualquier comprobación: `paramTypes`, `returnType`,
  `registerTypes`, campos de modelo y `typeName` de globales. Es el mismo paso
  que `canonicalize_list_types` hace con los deletreos de lista (D-M1-32). La
  tabla de tipos conserva el nombre, que es la clave de `Name_TAG_Variant`;
  `tag_constant`, antes `union_tag_constant`, acepta `union_type` y
  `enumeration`.
- Fail-closed: el gate de kind nombra `enumeration` y `union_type` y rechaza el
  resto (`x-union-kind-sum`); una variante de enumeración con payload se
  rechaza (`x-enum-payload`) porque el backend de C la descartaría; una
  constante que no nombra una variante declarada sigue siendo un símbolo
  desconocido (`x-enum-undeclared-tag`). El control de D-M1-12
  `x-union-kind-enumeration` pasa a control de aceptación `ok-enum-kind-unit`.
- Controles: fixture `44-enum-tag-constant` (constante, campo de modelo,
  parámetro, retorno y ocho comparaciones: cuatro `eq` en los dos callees y
  cuatro `ne` en `main`; salida `blue\r\nred\r\n2\r\nred\r\n`, exit 0). Los dos
  rechazos y la aceptación están en `tests/reject/`.
- Puertas: diferencial **42/42 -> 43/43**; loader **195/195 -> 199/199** (142
  rechazos + 57 controles; `make_cases.py --check` 180 archivos sin drift);
  corpus **47 build / 46 match sobre 97 programas**, trinquete sin cambio y su
  baseline intacto.
- Censo: **0 -> 0 build** y el grupo `enumeration` de 8 desaparece. `union
  ASTNode` 9 -> 14, `union IRValue` 4 -> 4, `extern return type` 2 -> 3,
  `module must define one non-extern main` 2 -> 2, `register type unknown`
  0 -> 2 (`front/lexer`, `front/token`), `model field list` 1 -> 1. Trinquete
  re-tomado.
- Mensaje: el IR del compilador ya no se detiene en `M1 codegen does not
  support type kind enumeration`. El siguiente borde es la unión `ASTNode`
  (14 módulos) y, en los dos módulos del front, un slot de registro que
  ninguna instrucción escribe ni lee.

## Re-pin D-F0-13: `17ed624`, corpus 97+1 y canónico `6C26A19E`

Fecha: 2026-10-21. Setenta commits después de `ce87d508` (D-F0-12). El pin se
movió porque el usuario mergeó en `main` la recuperación de OOM por petición
del runtime y el tramo de `compiler/` que regenera el canónico.

| Métrica | `ce87d508` (D-F0-12) | `17ed624` (D-F0-13) |
|---|---|---|
| miembros / programas | 94 / 93 | 98 / 97 |
| bytes | 5419454 | 5932267 |
| `filesSha256` | `0106d531…` | **`e8586b9a…`** |
| oráculo compila y corre | 93/93 | 97/97 |
| R6 compila | 46/93 | **47/97** |
| R6 coincide (exit + stdout) | 45/93 | **46/97** |
| documentos con `unknown` en `registerTypes` | 87/94 | 91/98 |

El canónico pasó de `9906A6F7…` a `6C26A19E…` y el archivo del runtime de 324
a 337 símbolos definidos. El mismo `filesSha256` salió de una extracción `git
archive` y de un `git clone` en el commit, así que los bytes son función del
commit.

El diferencial perdió un caso: el pin rechaza usar un extern como valor
(`Cannot use extern function 'orbit_string_at' as a value`), así que
`26-fn-values.orb` ya no construye con el oráculo. La pareja se renombró a
`.skip` y su IR sigue medido por `ok-fn-extern` (D-M1-14, exit 98). Las puertas
quedan en **diferencial 42/42**, **loader 193/193** y **corpus 47 build / 46
match**, con el trinquete re-tomado en 47/46.

Queda deuda declarada: las citas de línea de D-M1-19 a D-M1-33 apuntan a
`c_backend.orb` en el blob `2d64c8de`, que ya no es el del pin. El re-anclaje
contra `0c9dd786` es trabajo de documentación aparte: 130 citas, 65
desplazadas, 5 reescritas y 1 desaparecida.

## Censo del IR del compilador (2026-10-22)

Segundo objetivo medido (D-M1-34): los 26 módulos de `compiler/**/*.orb` del pin
`33abf7a` (D-F0-14), emitidos con el punto fijo del pin y medidos con
`tests/compiler-ir/run_census.py`.

    python tests/compiler-ir/run_census.py `
      --compiler <punto fijo del pin> --tree <checkout en 33abf7a> `
      --write-baseline --r6-command .r6-build\r6.exe build '{ir}' -o '{exe}'

| Primer rechazo | Módulos | Cuáles |
|---|---|---|
| `union ASTNode variant Program payloads are outside the M1 native subset` | 14 | `emit/c_backend`, `front/ast`, `front/parser`, `front/resolver`, `front/sema`, `host/cluster`, `host/main`, `host/pipeline`, `host/start`, `ir/builder`, `tir/entry`, `tir/lower`, `tools/doctor`, `tools/fmt` |
| `union IRValue variant Int payloads are outside the M1 native subset` | 4 | `emit/ir_emit`, `ir/cteval`, `ir/ir`, `ir/optimizer` |
| `extern return type is unsupported in M1` | 3 | `host/extern`, `selfhost/lexer_trace`, `tools/palette` |
| `module must define one non-extern main` | 2 | `emit/route_runtime`, `front/literal` |
| `register type unknown is unsupported in M1` | 2 | `front/lexer`, `front/token` |
| `model TirFunction field parameters has unsupported type list` | 1 | `tir/format` |
| **construye** | **0** | |

Los 26 documentos suman 97 699 992 B (`irFilesSha256 34e710ca…`) en `33abf7a`;
97 638 934 B y `858dad12…` en `17ed624`, medidos antes de que el pin trajera el
arreglo del emisor. El mayor es `host/main.orb`: 16 198 821 B en `33abf7a`
(16 187 421 en `17ed624`), 1 012 funciones, 16 globales, 95 modelos y 4
tipos, una enumeración `TokType` de 90 variantes y tres uniones (`ASTNode` 45,
`IROpcode` 66, `IRValue` 8).

Los documentos son los del pin, emitidos desde un checkout limpio en el commit.
En `17ed624` el arreglo vivía solo en el binario emisor, porque el árbol no
podía llevarlo: `emit/ir_emit.orb` es uno de los 26 módulos y las amalgamas lo
incluyen, así que un árbol parcheado emitía su propio documento (97 692 498 B
en vez de 97 638 934). Desde D-F0-14 el pin ya trae el arreglo y el baseline se
re-tomó sobre sus propios bytes.

Las dos últimas filas no son paredes del subconjunto: `emit/route_runtime.orb` y
`front/literal.orb` son módulos de biblioteca sin `main`, y construirían dentro
de una amalgama.

El orden no lo da el tamaño del grupo. El punto de entrada del compilador,
`host/main.orb`, caía en la enumeración, así que esa pared se encontraba
primero. D-M1-35 admite `enumeration` y la predicción se cumple: el grupo de 8
desaparece y `host/main.orb` pasa a la unión de su AST, que ahora agrupa 14
módulos. Dos de los ocho (`front/lexer.orb`, `front/token.orb`) paran antes, en
`register type unknown is unsupported in M1`: el IR del pin deja un slot de
registro que ninguna instrucción escribe ni lee, y el gate todavía le exige
tipo. Ese es el siguiente borde medido, no una divergencia: los dos documentos
compilan con el oráculo.

El censo fue posible porque el emisor dejó de ser cuadrático (commit `33abf7a`
de `orbit-lang`): `host/main.orb` emite en 2,62 s con 1 000 MB de pico, y sin el
arreglo no termina por encima de 4,5 GB. Los bytes no cambian: el corpus sigue
en `filesSha256 e8586b9a…`, documento por documento.

El trinquete vive en `tests/compiler-ir/baseline.json`: `r6BuildOk 0` con
`irFilesSha256 34e710ca…`. Un commit del lenguaje que toque `compiler/` cambia
los documentos y el hash, así que un movimiento de pin re-toma este baseline en
el mismo acto.

## Actualización de M1 - un `int` es de 32 bits (2026-10-22)

D-M1-36. El pin declara `typedef int orbit_int` (`runtime/orbit_api.h`), así
que la aritmética de un `int` es la de un `int` de C: 32 bits, y el resultado
que no cabe envuelve. R6 la emitía a 64 bits (`addq`, `subq`, `imulq`), y el
slot conservaba el valor largo. El programa `test_time_ns` lo midió al
construirse por primera vez (D-M1-32): su aserción 16 esperaba -1 de
`sinceNsBetween(0, pastCap)` y recibía 2147483648.

El arreglo toca tres puntos. `add`, `sub` y `mul` son `addl`/`subl`/`imull`
seguidos de `movslq %eax, %rax`. Un `call` con destino `int` extiende el signo
de `%eax` antes de guardar. Un literal `Int` inline de argumento se carga con
`movl` y `movslq`. `div`, `mod`, `list_get` y `load_field` ya usaban la forma de
32 bits, y `list_len` conserva el almacén de 64 bits porque `OrbitList.len` es
un `size_t` (D-M1-25). La negación sigue en `negq`: solo difiere en
`-INT32_MIN`, que es indefinido en C, y el oráculo dobla esa comparación (la
sonda `.orb` sale 11), así que ningún fixture puede pinearla.

El par `45-int-overflow` mide las cuatro puertas: `bump(2147483647)` debe ser
negativo, `sq(65536)` debe ser 0, `over - 1` debe ser 2147483647 e `id(-1)`
debe ser negativo. Además pasa cuatro literales en una llamada
(`sum4(1, 2, 3, -4)` debe ser 2), lo que mide también el cuarto registro de
argumento, `%r9d`. Los dos lados salen 42. La primera sonda no puede usar el
valor de retorno de `main`: el exit del proceso conserva solo los 32 bits bajos,
así que la envoltura se observa con una comparación.

Medido: diferencial **43/43 -> 44/44**; loader **199/199** sin cambio; corpus
**47 build / 46 match -> 47 build / 47 match sobre 97 programas**, con un solo
veredicto movido (`test_time_ns`: `r6-differs` -> `match`) y los otros 97
documentos iguales; censo **0 -> 0 build** con 26 módulos, 97 699 992 bytes,
`irFilesSha256 34e710ca…` y los mismos seis primeros rechazos. El trinquete del
corpus se re-toma en `match 47`; el del censo no se toca.


## Actualización de M1 - el punto de entrada guarda argc y argv (2026-10-26)

D-M1-37. El `main` de C que escribe el pin empieza con `_orbit_argv = argv;` y
`_orbit_argc = argc;` (`c_backend.orb:302-306`), y `orbit_os_argc` y
`orbit_os_argv` leen esos dos globales (`runtime/os.c:371-378`). El wrapper de
entrada de R6 no los escribía, así que todo programa veía `argc` 0. Lo midió
`tests/std/test_process.orb`: construido con R6, salía 4 en
`argCount() < 1`, y el oráculo sale 0.

El wrapper guarda ahora `%ecx` y `%rdx` en los dos globales antes de su primera
llamada. El par `46-process-argc` lo mide: el oráculo y R6 imprimen `1\r\n` y
salen 42, y el driver de `01037c1` sale 10 con el mismo IR.

Medido: diferencial **44/44 -> 45/45**; loader **199/199 -> 200/200** (142
rechazos + 58 controles); corpus **47 build / 47 match sobre 97 programas**, sin
veredictos movidos, porque ningún programa del corpus que construye lee `argc`.
El trinquete no se re-toma.

## Actualización de M1 - la lectura opaca de una lista (2026-10-26)

D-M1-38. Un `list_get` con `elemKind 0` lee de una lista que el builder no pudo
tipar, como el parámetro `parts: list` del `join` de la std. El pin lo baja a
`orbit_list_read_word`, que devuelve la palabra del slot sin chequear el kind, y
C la convierte donde se usa. De las 18 lecturas opacas de los 12 programas que
paraban en `register type unknown`, 13 pasan como argumento a un parámetro
`string` de una función declarada.

R6 tipa la lectura con el tipo de ese parámetro cuando es `string` o `int`, el
registro tiene un solo escritor y un solo lector, y la `call` directa consume el
`arg` sin un `label` o salto en medio. Cualquier otro uso deja el registro
`unknown`, con el mismo diagnóstico. El par `47-opaque-list-read` mide los dos
tipos: el oráculo y R6 imprimen `5\r\n` y salen 42.

Medido: diferencial **45/45 -> 46/46**; loader **200/200 -> 206/206** (146
rechazos + 60 controles); corpus **47 build / 47 match -> 49 build / 49 match
sobre 97 programas** (`test_regex` y `test_process` coinciden byte a byte).
Cinco programas de la std pasan de `register type unknown` a
`returning result values is not supported in M1`, que es ahora el primer
rechazo de 7 programas. El trinquete se re-toma en `r6BuildOk 49`, `match 49`.

## Actualización de M1 - un campo `list` es un puntero (2026-10-26)

D-M1-39. El pin declara todo campo de lista de un modelo como `OrbitList*`
(`c_backend.orb:478`): 8 bytes, alineación 8. R6 reescribe los tipos de campo a
`list` como ya hacía con parámetros, retornos y registros (D-M1-32), y
`load_field`, `store_field` y el constructor copian el puntero. El par
`48-model-list-field` lo mide: el oráculo y R6 imprimen `bc\r\n` y salen 42.

El fixture lee los elementos a través de locales tipados. Leídos directo del
campo, el pin emite `elemKind 0` y su oráculo imprime `140697716711947` en vez
de `bc`: es un defecto del builder del lenguaje, no de R6.

Medido: diferencial **46/46 -> 47/47**; loader **206/206 -> 209/209** (148
rechazos + 61 controles); corpus **49 build / 49 match** sin cambio, porque los
tres programas con campos `list` paran ahora en un parámetro de tipo modelo;
censo **0/26**, con `tir/format` en `returning model or void* values`.

## Actualización de M1 - un parámetro de tipo modelo (2026-10-26)

D-M1-40. El pin pasa un modelo como `Name*` (`c_backend.orb:524-526`). R6 ya
movía todo parámetro y argumento no-float con `movq`, así que la decisión solo
admite el tipo en dos chequeos: el parámetro de una función declarada y el
valor de un `arg`. El chequeo de la llamada sigue exigiendo el mismo modelo.

El par `49-model-parameter` pasa el modelo en registro y en la pila (quinto
argumento), y lo muta en el callee: el oráculo y R6 imprimen `origin\r\n` y
salen 42.

Medido: diferencial **47/47 -> 48/48**; loader **209/209 -> 212/212** (150
rechazos + 62 controles); corpus **49/49 -> 52 build / 52 match sobre 97
programas**, con tres coincidencias nuevas y ningún `r6-differs`; censo
**0/26** sin cambio. El trinquete se re-toma en 52/52.

## Actualización de M1 - `movslq` con REX.R y REX.B (2026-10-26)

D-M1-41. El codificador emitía `movslq` registro a registro con el prefijo
fijo `0x48`, así que `movslq %r9d, %r9` se codificaba como
`movslq %ecx, %rcx`. Un literal `int` como tercer o cuarto argumento truncaba
entonces el puntero del primer argumento a 32 bits. Bajo gdb la pila queda por
debajo de `0x80000000` y el defecto no se veía. El prefijo lleva ahora REX.R y
REX.B, y nueve formas comparadas con GNU `as` dan los mismos bytes.

El par `50-literal-args-r8-r9` lo mide: el oráculo y R6 imprimen `65\r\n` y
salen 42; el driver anterior revienta con `0xC0000005`. Diferencial **49/49**;
loader **213/213** (150 rechazos + 63 controles); corpus **52/52** sin cambio;
censo **0/26**.

## Actualización de M1 - un `bool` de C se lee de `al` (2026-10-26)

D-M1-42. El runtime devuelve `bool` de C (`orbit_api.h:118`, `:405`), que la
ABI de Win64 deja en `al` con el resto de `rax` indefinido. R6 guardaba `rax`
entero en el slot, y un `false` con bits altos se leía como `true`.
`test_string` lo midió al construirse: salía 5 donde el oráculo sale 0. El
destino `bool` de una llamada directa pasa ahora por `movzbl %al, %eax`.

El par `51-c-bool-return` lo mide: el oráculo y R6 imprimen `1\r\n` y salen
42; el driver anterior sale 10. Diferencial **50/50**; loader **214/214** (150
rechazos + 64 controles); corpus **52/52** sin cambio; censo **0/26**.

## Actualización de M1 - un `Result` cruza una llamada (2026-10-26)

D-M1-43. El pin devuelve `OrbitResult`, un struct de 24 bytes, por valor. En
Win64 eso es un puntero oculto en `rcx` y los argumentos se corren una
posición. El callee guarda ese puntero en el slot que `main` usa para su arena,
y su `ret` copia la celda de 24 bytes. Cada `try` emite además un
`result_unwrap` con destino `unknown`: sin lector se copia como palabra
`void*`, y si inicializa un local toma el tipo que el builder da a los
`load_var` de ese local.

El par `52-result-return` pasa valores `int`, `float` y `string`, dos mensajes
de error y un quinto argumento por pila detrás del puntero oculto: el oráculo y
R6 imprimen `odd\r\nhi r6\r\nsum off\r\n` y salen 42.

Medido: diferencial **51/51**; loader **217/217** (151 rechazos + 66
controles); corpus **52/52 -> 56 build / 56 match sobre 97 programas**
(`float_results`, `test_codec`, `test_crypto`, `test_string`); censo **0/26**.
El trinquete se re-toma en 56/56.

## Actualización de M1 - un modelo devuelto (2026-10-26)

D-M1-44. El pin devuelve un modelo como `Name*`. R6 ya movía el slot a `rax` en
el `ret` y guardaba `rax` en el destino de la llamada, así que solo cambia el
chequeo de la firma; una unión (`void*`) sigue rechazada. El par
`53-model-return` lo mide: el oráculo y R6 imprimen `wide\r\nmade\r\n` y salen
42.

Medido: diferencial **52/52**; loader **219/219** (152 rechazos + 67
controles); corpus **56/56 -> 60 build / 60 match sobre 97 programas**; censo
**0/26**, con `tir/format` en `list_push`. Los dos trinquetes se re-toman.

## Actualización de M1 - un local `Result` (2026-10-26)

D-M1-45. El pin declara un local `result` como `OrbitResult` por valor
(`c_backend.orb:4195`). R6 le da una celda propia de 24 bytes junto a las
celdas de los registros, y `decl_var`, `store_var` y `load_var` copian la celda
entera. El par `54-result-local` declara, reasigna y devuelve locales `result`:
el oráculo y R6 imprimen `zero\r\nsmall\r\nbig\r\nnegative\r\n` y salen 42.

Medido: diferencial **53/53**; loader **221/221** (153 rechazos + 68
controles); corpus **60/60 -> 63 build / 63 match sobre 97 programas**; censo
**0/26**. El trinquete se re-toma en 63/63.

## Actualización de M1 - un registro que nadie toca (2026-10-26)

D-M1-46. El builder reserva registros que ninguna instrucción escribe ni lee
(un brazo de `match` reserva uno por patrón), y los serializa `unknown`. El pin
los declara como `uintptr_t r_N;` sin uso, así que su tipo no es observable. R6
les da el tipo palabra (`void*`); un slot que se lee sin escritura sigue
rechazado.

Medido: diferencial **53/53** sin cambio; loader **223/223** (154 rechazos + 69
controles); corpus **63/63** sin cambio, porque los cuatro programas con slots
muertos paran antes en el tipo de unión; censo **0/26**, con `front/lexer` y
`front/token` en `ret value does not match function return type`.

## Actualización de M1 - una unión como valor (2026-10-26)

D-M1-47. El pin baja una unión a `Name*`, la celda que D-M1-12 ya trata como
palabra `void*`. R6 reescribe el nombre de la unión a `void*` en parámetros,
retornos y registros, admite esa palabra como parámetro y argumento, y tipa el
`union_get_data` de un brazo de `match` por el local que liga, como hace
D-M1-43 con `result_unwrap`. El par `55-union-parameter` lo mide: el oráculo y
R6 imprimen `shape orbit\r\n` y salen 42.

Medido: diferencial **54/54**; loader **226/226** (156 rechazos + 70
controles); corpus **63/63 -> 64 build / 64 match sobre 97 programas**
(`match`); censo **0/26**. El trinquete se re-toma en 64/64.

## Actualización de M1 - un extern del runtime devuelve `result` o `list` (2026-10-26)

D-M1-48. Los cuatro externs `result` del corpus reciben la arena primero
(`orbit_api.h:589`, `:599`, `:619`, `:722`). En Win64 el puntero de la celda va
en `rcx`, la arena pasa a `rdx` y los argumentos empiezan en la posición 2. Un
extern `list` devuelve una palabra en `rax`. El par `56-extern-result-list` lo
mide: el oráculo y R6 imprimen `read failed\r\n0\r\n` y salen 42.

Medido: diferencial **55/55**; loader **229/229** (158 rechazos + 71
controles); corpus **64/64 -> 65 build / 65 match sobre 97 programas**
(`test_http_client`); censo **0/26**, sin el grupo `extern return type`. Los dos
trinquetes se re-toman.

## Actualización de M1 - un `ret` sin valor (2026-10-26)

D-M1-49. El pin baja un `ret` sin operando en una función con valor a
`return 0;`, `0.0` o `NULL` según el tipo. R6 devuelve `0` en `eax` o `0.0` en
`xmm0`; una función `result` sigue rechazada. El par `57-bare-return` lo mide:
el oráculo y R6 imprimen `one\r\n` y salen 42.

Medido: diferencial **56/56**; loader **231/231** (159 rechazos + 72
controles); corpus **65/65** sin cambio; censo **0/26**, sin el grupo
`ret value does not match`.

## Actualización de M1 - la grafía `Result` (2026-10-26)

D-M1-50. El pin trata `Result` y `OrbitResult` como `result`
(`c_backend.orb:502`), y R6 los reescribe igual que las grafías de lista. El
par `58-result-spelling` lo mide: el oráculo y R6 imprimen `neg\r\n` y salen
42. Diferencial **57/57**; loader **233/233** (160 rechazos + 73 controles);
corpus **65/65** sin cambio; censo **0/26**.

## Actualización de M1 - una lectura opaca comparada con un `int` (2026-10-26)

D-M1-51. El pin compara `eq`/`ne` como palabras `uintptr_t` y el resto como
`intptr_t`; un elemento `int` está extendido en signo en su palabra, así que
leerlo como `int` compara igual. Una comparación con un string compara
punteros en el pin y sigue rechazada. El par `59-opaque-compare` lo mide: el
oráculo y R6 imprimen `3\r\n` y salen 42.

Medido: diferencial **58/58**; loader **234/234** (160 rechazos + 74
controles); corpus **65/65 -> 67 build / 67 match sobre 97 programas**
(`list_push`, `type_spellings`); censo **0/26**. El trinquete se re-toma en
67/67.

## Actualización de M1 - un `int` se niega en 32 bits (2026-10-26)

D-M1-52. El builder baja `-2147483648` como `load_const -2147483648` más
`neg`. R6 negaba con `negq` y obtenía `+2147483648`; el oráculo da
`-2147483648`. Lo midió `int_wrap_32` en el corpus de `d58d647`. Corrige la
excepción que D-M1-36 había dejado para `neg`: ahora es `negl` más `movslq`. El
par `60-int-neg-wrap` lo mide: el oráculo y R6 imprimen `-2147483648\r\n` y
salen 42; el driver anterior sale 10. Diferencial **59/59**; loader **235/235**
(160 rechazos + 75 controles); corpus **67/67**; censo **0/26**.

## Re-pin D-F0-15: `a5cdf77`, el primer pin público (2026-10-26)

El pin pasa de `33abf7a` a `a5cdf77`, 43 commits ya publicados en
`github.com/joacokhzyx/orbit-lang`. Trae 34 programas de suite, ocho
correcciones del compilador, el bootstrap a `-O1` y los tipos de registro que el
front end ya conoce: el corpus baja de 8045 destinos `unknown` a 425. El corpus
se regenera a 133 documentos (`filesSha256 91341401…`) y R6 construye y coincide
en **91 de 132 programas**, ninguno difiere (eran 67 de 97). Diferencial
**59/59**, loader **235/235**, censo **0 de 27 módulos**. Los dos trinquetes se
re-toman.

## Actualización de M1 - un brazo con varios payloads lee su paquete (2026-10-27)

D-M1-53. Desde D-F0-15 un brazo de `match` con varios payloads lee la palabra
de datos solo con `load_field <Union>_<Variant>._i`, después del test de tag, y
el builder deja esos registros en `unknown`. R6 tipa la palabra como el puntero
al paquete cuando todos sus lectores son campos del paquete, y cada parte con
su tipo declarado (`int`, `bool`, `string`). El par `61-union-pack-arm` lo mide:
oráculo y R6 imprimen `pack\r\n` y salen 42. Diferencial **60/60**; loader
**238/238** (162 rechazos + 76 controles); corpus **92 build / 92 match sobre
132 programas** (`union_multi_payload_unit_variant`); censo **0/27**. El
trinquete se re-toma en 92/92.

## Re-pin a `ca3eb91` (2026-10-27)

D-F0-16. El pin pasa de `a5cdf77` a `ca3eb91`: el builder tipa los `try`, los
payloads simples de unión y las lecturas de listas de listas o de modelos (341
registros `unknown` en el corpus, antes 425). El serializador y el runtime no
cambian. Diferencial **60/60**; loader **238/238**; corpus **95 build / 95
match sobre 132 programas** (`result_chain`, `result_try`,
`union_match_exhaustive` entran sin cambios en R6); censo **0/27**.

## Actualización de M1 - un payload de modelo o de unión es una palabra (2026-10-27)

D-M1-54. Un payload que nombra un modelo o una unión declarados es un puntero
de 8 bytes, solo en la palabra de datos o dentro del paquete. El par
`62-union-named-payload` lo mide: oráculo y R6 imprimen `named\r\n` y salen 42.
Diferencial **61/61**; loader **241/241** (164 rechazos + 77 controles); corpus
**97 build / 97 match sobre 132 programas** (`union_model_payload`,
`union_multi_payload`); censo **0/27**, y los 14 módulos de `ASTNode` pasan al
primer rechazo de un campo de modelo de tipo unión. El trinquete se re-toma en
97/97.

## Actualización de M1 - una unión se devuelve como su celda (2026-10-27)

D-M1-55. Una función puede devolver una unión: la celda de D-M1-12 vuelve en
`rax` como cualquier puntero. D-M1-44 lo dejaba rechazado sin una medición que
lo justificara. El par `63-union-return` lo mide: oráculo y R6 imprimen
`shapes\r\n` y salen 42. Diferencial **62/62**; loader **242/242** (164
rechazos + 78 controles); corpus **100 build / 100 match sobre 132 programas**
(`mini_parser`, `union_bare_variant_pattern`, `union_payload_return`); censo
**0/27**. El trinquete se re-toma en 100/100.

## Actualización de M1 - un campo de modelo o de unión es una palabra (2026-10-27)

D-M1-56. Un campo de modelo que nombra un modelo o una unión es un puntero de 8
bytes. El par `64-model-word-fields` lo mide: oráculo y R6 imprimen `ab\r\n` y
salen 42. Diferencial **63/63**; loader **245/245** (166 rechazos + 79
controles); corpus **101 build / 101 match sobre 132 programas**
(`model_nested`); censo **0/27**: los módulos del compilador pasan a los
registros `unknown` (10) y a `list_push` (9). El trinquete se re-toma en
101/101.

## Actualización de M1 - una lista de referencias guarda punteros (2026-10-27)

D-M1-57. Una lista con `elemKind 5` guarda palabras puntero: listas, modelos y
celdas de unión. Se lee con el chequeo de tipo contra 5 y se escribe con
`orbit_list_push_ref` y `orbit_list_set_ref`. El par `65-ref-lists` lo mide:
oráculo y R6 imprimen `a\r\n` y salen 42. Diferencial **64/64**; loader
**247/247** (167 rechazos + 80 controles); corpus **103 build / 103 match sobre
132 programas** (`list_element_type`, `test_collections_boxed`); censo
**0/27**. El trinquete se re-toma en 103/103.

## Actualización de M1 - una lista sin tipo mueve palabras (2026-10-27)

D-M1-58. Una lista con `elemKind 0` admite agregar, reemplazar y leer palabras
`int`, `string`, lista, modelo o `void*`, como hace el pin con
`orbit_list_push(list, &_tmp)` y `orbit_list_read_word`. `bool` y `float`
quedan afuera. El par `66-opaque-lists` lo mide: oráculo y R6 imprimen
`second\r\n` y salen 42. Diferencial **65/65**; loader **248/248** (167
rechazos + 81 controles); corpus **104 build / 104 match sobre 132 programas**
(`generic_nested_argument`); censo **0/27**. El trinquete se re-toma en
104/104.

## Actualización de M1 - NULL es la palabra cero (2026-10-27)

D-M1-59. El literal `NULL` carga la palabra cero en un registro `void*`; `eq`
y `ne` entre dos punteros (lista, modelo o celda) comparan las palabras; una
función de modelo puede devolver `NULL`. El par `67-null-words` lo mide:
oráculo y R6 imprimen `null\r\n` y salen 42. Diferencial **66/66**; loader
**252/252** (170 rechazos + 82 controles); corpus **104/104** sin cambios;
censo **0/27**.

## Actualización de M1 - indexOf y find (2026-10-27)

D-M1-60. `indexOf` y `find` sobre strings, con o sin el prefijo
`orbit_string_`, llaman a `orbit_string_indexOf` del runtime. El par
`68-string-index` lo mide: oráculo y R6 imprimen `found\r\n` y salen 42.
Diferencial **67/67**; loader **255/255** (172 rechazos + 83 controles); corpus
**105 build / 105 match sobre 132 programas** (`user_fn_named_find`); censo
**0/27**. El trinquete se re-toma en 105/105.

## Actualización de M1 - un main void sale con 0 (2026-10-27)

D-M1-61. `main` puede no devolver valor; el programa sale con 0, como el
`int orbit_main` del pin. El par `69-void-main` lo mide: oráculo y R6 imprimen
`void main\r\n` y salen 0. Diferencial **68/68**; loader **258/258** (174
rechazos + 84 controles); corpus **105/105**. Censo **1/27**: el primer módulo
del compilador construido por R6 es `compiler/selfhost/lexer_trace.orb`, el
lexer de Orbit con un driver de traza. Corrido desde el árbol del pin, imprime
los mismos 400 tokens de `compiler/host/main.orb` que el binario del oráculo,
byte por byte; el ejecutable de R6 pesa 310769 bytes y el del oráculo 442368.

## Actualización de M1 - s.length sobre un string (2026-10-27)

D-M1-62. `s.length` y `s.len` sobre un string llegan como `load_field`; R6 los
baja a `strlen`, 0 para un puntero nulo, como el `orbit_string_len` del pin. El
par `70-string-length-field` lo mide: oráculo y R6 imprimen `length\r\n` y
salen 42. Diferencial **69/69**; loader **261/261** (176 rechazos + 85
controles); corpus **105/105**; censo **1/27**.

## Actualización de M1 - el ensamblador crece; Orbit se compila con R6 (2026-10-27)

D-M1-63. Las tablas del ensamblador (etiquetas, procedimientos, relocaciones,
globales) crecen en el heap y las etiquetas se buscan por hash; los bytes del
objeto no cambian. Los chequeos quedan igual: loader **261/261**, diferencial
**69/69**, corpus **105/105**, censo **1/27**.

Medido sobre orbit-lang `0cfce74`, todavía fuera del pin: R6 construye el
compilador completo (`compiler/host/main.orb`, 1038 funciones) en 42 s, un
ejecutable de 3357756 bytes. Ese compilador traduce `compiler/host/main.orb` a
un C idéntico byte por byte al del oráculo (6212022 bytes, el `PUBLISHED_C` del
árbol), en 9345 ms contra 5423 ms. Su IR del compilador es idéntico al IR del
que salió, y R6 lo vuelve a construir en un ejecutable idéntico: el punto fijo
nativo cierra sin pasar por C.

## Re-pin a `0cfce74`: el censo construye el compilador (2026-10-27)

D-F0-17. Con el pin en `0cfce74` el censo construye las dos entradas del
compilador: `compiler/host/main.orb` (el compilador completo) y
`compiler/selfhost/lexer_trace.orb`. Los otros 25 módulos son bibliotecas sin
`main`, y ninguna de sus funciones es rechazada. Diferencial **69/69**; loader
**261/261**; corpus **109 build / 109 match sobre 135 programas**, ninguno
difiere; censo **2/27**.

## Actualización de M1 - R6 construye el compilador en 5 s (2026-10-27)

D-M1-64. `r6 build` sobre `compiler/host/main.orb` (17,6 MB de IR, 1038
funciones) baja de 41 s a 4,7 s, medido en la misma máquina cargada y con el
driver sin optimizar; en una máquina libre baja de 20,6 s a 2,4 s. Cuatro
causas, todas cuadráticas o de asignación: el escritor COFF avanzaba el cursor
de relocaciones por el desplazamiento del archivo en vez de por la cuenta (el
objeto pesaba 211 MB y se escribía cero a cero; ahora pesa 3,4 MB), el
ensamblador buscaba cada símbolo externo contra todos los símbolos, el
generador comparaba cada `decl_var` con todos los anteriores, y el cargador
hacía un `malloc` por nodo JSON. Los bytes del ejecutable no cambian: el
compilador de 3357756 bytes (`2b8cb011…`) y los 205 ejecutables de los
fixtures y del corpus tienen el mismo SHA-256 antes y después. Diferencial
**69/69**; loader **261/261**; corpus **109 build / 109 match sobre 135**,
la regla de no retroceso se cumple; censo **2/27**.

## Actualización de M1 - solo se emiten las funciones que main alcanza (2026-10-27)

D-M1-65. R6 emite solo las funciones que `main` alcanza por una llamada, por un
valor de función (un operando `Sym` que nombra una función) o por una cadena de
ambas. Las demás se validan igual y no se emiten. El par `71-dead-functions`
lo mide: oráculo y R6 imprimen `live\r\n` y salen 42, y `nm` del ejecutable de
R6 no lista las tres funciones inalcanzables. El control
`ok-dead-undefined-call` llama a un extern sin definición desde dos funciones
inalcanzables: antes terminaba en `error[LINK]`, ahora construye y sale 7. El
compilador completo baja de 3357756 a 3343316 bytes; sobre 70 fixtures y 135
programas del corpus, 21 de 179 ejecutables cambian y suman 142892 bytes menos.
Diferencial **70/70**; loader **264/264**; corpus **109 build / 109 match
sobre 135**; censo **2/27**. El compilador así construido se compila a sí
mismo a un C idéntico al del oráculo y el punto fijo nativo sigue cerrando.

## Actualización de M1 - un salto sobre una comparación lee los flags (2026-10-27)

D-M1-66. Un `jump_if_false` que sigue a `eq`, `ne`, `lt`, `le`, `gt` o `ge` y
prueba su resultado salta con la condición inversa (`jne`, `je`, `jge`, `jg`,
`jle`, `jl`) en vez de recargar el slot y compararlo con cero; la comparación
sigue guardando su resultado. Float (paridad) y `eq`/`ne` de strings
(`strcmp`) conservan la carga. El par `72-compare-branch` lo mide: oráculo y R6
imprimen `599\r\n` y salen 42. En el asm del compilador los pares
`cmpl $0` + `je` bajan de 7826 a 4543 y las instrucciones de 297829 a 294546;
el tiempo de auto-compilación no se mueve fuera del ruido (mediana 5,02 s
contra 5,11 s). Diferencial **71/71**; loader **265/265**; corpus **109 build
/ 109 match sobre 135**; censo **2/27**.

## Actualización de M1 - una carga tras el store de su slot lee el registro (2026-10-27)

D-M1-67. Entre el generador y el ensamblador, `peephole.c` reescribe dos líneas
contiguas `movq %R, M` y `movq M, %S`: quita la segunda si `S` es `R` y la
cambia por `movq %R, %S` si no. El store se queda, así que toda lectura
posterior del slot es la misma. Solo reescribe registros de 64 bits y solo
líneas contiguas. El compilador completo pasa de 294546 a 285322
instrucciones y de 3323348 a 3234260 bytes; su auto-compilación baja de 5,02 s
a 4,39 s (mediana de nueve corridas intercaladas), contra 2,31 s del oráculo.
El C que produce sigue siendo idéntico byte por byte al del oráculo.
Diferencial **71/71**; loader **265/265**; corpus **109 build / 109 match
sobre 135**; censo **2/27**.
