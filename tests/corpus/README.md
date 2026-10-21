# tests/corpus - diferencial sobre el corpus IR del lenguaje (medición, no puerta)

Fecha: 2026-10-16; re-pin 2026-10-21 (D-F0-13). Decisión:
`docs/DECISIONS.md` D-M1-17. Corpus: los 97+1 documentos IRv1 que el lenguaje
emite sobre `tests/suite` (74 programas + `imports.support.orb`) y `tests/std`
(23 programas) - el corpus designado por `orbit-lang/docs/IR_SPEC.md` §5.5 y
reconciliado con el árbol en D-F0-10; regenerado sin cambio en D-F0-11 y
movido en D-F0-13 - pineados por hash en `manifest.json`.

```powershell
python tests/corpus/run_corpus.py `
  --compiler C:\Temp\orbit-fp-08a471904d.exe `
  --cc C:\w64devkit\bin\gcc.exe `
  --orbit-root C:\Temp\orbit-pin-17ed624-git `
  --check `
  --r6-command .r6-build\r6.exe build '{ir}' -o '{exe}'
```

- `manifest.json` - el pin: 98 entradas (`path`, `kind`, `ir`, `bytes`,
  `sha256`), `filesSha256` (un solo hash del corpus) y la identidad del emisor
  (versión, commit del lenguaje, sha256 del binario). Lo produce
  `scripts/emit_ir_corpus.py` en el árbol del lenguaje (`main`, pin actual
  `17ed624`): regenerar y comparar es el contrato, no copiar ficheros. Mismo
  patrón con el que se pinean `runtime/orbit_api.h` y el archivo del runtime.
- `baseline.json` - la medición vigente: `summary` (members, programs,
  oracleOk, r6BuildOk, match, unknownRegisterTypes), `byCode`,
  `byFailingOpcode` y `byOpcode`. Se re-toma con `--write-baseline` (que corre
  después de `--check`, para que una misma corrida sostenga el trinquete viejo
  y registre el nuevo).
- `run_corpus.py` - regenera el corpus con el compilador pineado, verifica
  cada hash contra el pin y, por programa, compila y ejecuta los dos lados:
  oráculo desde el `.orb`, R6 desde el `.ir`, en el mismo directorio, con exit
  y stdout comparados byte a byte (`tests/diff/run_diff.py`). `--ir-dir` mide
  un corpus ya emitido; `--json` guarda el detalle por caso.

## Qué falla y qué no

La medición **no** puede ponerse roja porque el subconjunto nativo sea más
chico que el corpus: ese tamaño es el número que se mide. Fallan (exit 1) solo
dos cosas:

1. un corpus que ya no coincide con `manifest.json` (el pin está roto, y el
   silencio sería peor), y
2. `--check` cuando `r6BuildOk` o `match` bajan respecto de `baseline.json`
   (trinquete de una sola dirección, mismo contrato que el ratchet de tipos
   `unknown` del lenguaje).

## Re-pin 2026-10-16 (D-F0-10, emisor `299eb09`, corpus `0106d531`)

| Métrica | Valor |
|---|---|
| miembros / programas | 94 / 93 (+1 módulo de soporte) |
| oráculo compila y corre | 93/93 |
| R6 compila | **7/93** |
| R6 coincide (exit + stdout) | **7/93** |
| documentos con `unknown` en `registerTypes` | **87/94** |

Al mergear el emisor en el `main` del usuario (`299eb09`, sin push) el corpus
designado creció de 89+1 a 93+1 (cuatro programas de suite nuevos, §5.5
reconciliado con el árbol) y el conteo subió de 6/89 a **7/93**; el trinquete de
`baseline.json` queda en `r6BuildOk 7`, `match 7`. Primer rechazo por opcode:

| Primer rechazo | Casos |
|---|---|
| (nivel de módulo: `registerTypes`, globales, modelos, uniones) | 29 |
| `load_var` | 23 |
| `add` | 10 |
| `ne` | 7 |
| `decl_var` | 6 |
| `lt`, `ret`, `gt` | 2 cada uno |
| `mul`, `sub`, `div`, `list_create`, `call` | 1 cada uno |

## Re-pin 2026-10-17 (D-F0-11, emisor `862f257`, corpus sin cambio)

El lenguaje avanzó cuatro commits más (`e75f2c7` H-ARENA, `8b9107a` retirada del
Arena Oracle, `0a145cc` fix del cargador, `862f257` reorg de `compiler/`) y R6
re-pinó **una vez**, al final del tramo.

| Métrica | `299eb09` (D-F0-10) | `862f257` (D-F0-11) |
|---|---|---|
| miembros / programas | 94 / 93 | 94 / 93 |
| bytes | 5419454 | 5419454 |
| `filesSha256` | `0106d531…` | **`0106d531…`** (idéntico) |
| R6 compila / coincide | 7/93 | **7/93** |
| documentos con `unknown` en `registerTypes` | 87/94 | 87/94 |

El corpus **no se movió**: medido por R6 con punto fijo propio en `299eb09`,
`8b9107a` y `862f257` (los tres dan `0106d531…`). En `manifest.json` sólo cambia
el bloque `emitter` (ruta, sha256 del binario, `languageCommit`); `baseline.json`
no cambia ninguna métrica. La identidad reproducible del emisor es el commit y el
canónico `9906A6F7…`: el sha256 del binario depende del directorio de build
(embebe su ruta absoluta; ver `docs/PIN.md`).

## Re-pin 2026-10-21 (D-F0-13, emisor `17ed624`, corpus `e8586b9a`)

Setenta commits después de `ce87d508` (D-F0-12). El corpus creció de 93+1 a
97+1 documentos (suite 71 -> 74, std 22 -> 23) y el canónico pasó de
`9906A6F7…` a `6C26A19E…`.

| Métrica | `ce87d508` (D-F0-12) | `17ed624` (D-F0-13) |
|---|---|---|
| miembros / programas | 94 / 93 | 98 / 97 |
| bytes | 5419454 | 5932267 |
| `filesSha256` | `0106d531…` | **`e8586b9a…`** |
| oráculo compila y corre | 93/93 | 97/97 |
| R6 compila | 46/93 | **47/97** |
| R6 coincide (exit + stdout) | 45/93 | **46/97** |
| documentos con `unknown` en `registerTypes` | 87/94 | 91/98 |

El mismo `filesSha256` salió de dos árboles materializados de forma
independiente (una extracción `git archive` y un `git clone`, ambos en el
commit): los bytes son función del commit, no del árbol. El trinquete queda en
`r6BuildOk 47`, `match 46`.

El par 26 se retiró del diferencial: el pin rechaza usar un extern como valor
(`Cannot use extern function 'orbit_string_at' as a value`), así que el `.orb`
ya no construye con el oráculo. La pareja pasa a `.skip` y el IR sigue medido
por `ok-fn-extern` (D-M1-14, exit 98).

| Métrica | Valor |
|---|---|
| miembros / programas | 90 / 89 (+1 módulo de soporte) |
| oráculo compila y corre | 89/89 |
| R6 compila | **6/89** |
| R6 coincide (exit + stdout) | **6/89** |
| documentos con `unknown` en `registerTypes` | **84/90** |

El pin se tomó con **2/89** (D-M1-17), D-M1-18 lo llevó a **3/89**
(`tests/suite/imports.orb`, exit 37, opcodes `call` + `ret`, el primer programa
no degenerado que R6 completa, junto a los degenerados `bom_utf8` exit 0 y
`nesting_legal` exit 9) y D-M1-20 lo lleva a **6/89**:
`tests/suite/model_fields.orb` (exit 7), `tests/suite/model_two_fields.orb` y
`tests/suite/unary_neg.orb` (exit 2) pasan a coincidencia byte a byte al
recuperar los tipos `unknown` de su stream. D-M1-19 mantuvo el conteo (las
puertas de nivel de módulo y `unknown` dominaban) pero corrigió el lowering de
los literales: `load_const` dejó de ser el primer rechazo de cualquier
programa. 32 de los 41 opcodes del corpus no aparecen todavía en un programa
que R6 haya corrido completo.

Primer rechazo por opcode (dónde se detiene cada programa), tras D-M1-20:

| Primer rechazo | Casos |
|---|---|
| (nivel de módulo: `registerTypes`, globales, modelos, uniones) | 26 |
| `load_var` | 23 |
| `add` | 10 |
| `ne` | 7 |
| `decl_var` | 6 |
| `lt` | 2 |
| `ret` | 2 |
| `gt` | 2 |
| `mul`, `sub`, `div`, `list_create`, `call` | 1 cada uno |

Mensajes dominantes (`error[CGEN]` en los 83): `load_var requires a supported
local, declared global, or union tag constant` **23** (el grueso, tags de
enum sin tipo de acceso), la familia de **operando literal dentro de una
operación binaria** (`ne`/`add`/`gt`/`lt`/`mul`/`sub`/`div` `... operand N must
be a valid register`) **22** más `add supports matching int, float, or string
registers only` 2, `decl_var requires a declared local and a matching scalar,
list, void*, model, or function value register` 6, `register type unknown is
unsupported in M1` **4** (los no inferibles por D-M1-20), `parameter type
list<string> is unsupported in M1` 3 y una cola de 1-2 casos (globales
`CHAR_SPACE`/`WALK_MAX_DEPTH`, `type kind enumeration`, `extern return type`,
campos de modelo con `list<...>`, `register type Shape`/`object`/`string_raw`,
payloads de union, `returning result values`, `void ret must not carry a
value`). Ya no queda ningún `load_const` entre los primeros rechazos (cerró con
D-M1-19) ni ningún `call target ...` (cerró con D-M1-18). El detalle completo
está en el informe de la corrida (`--json`) y en `docs/M1_REPORT.md`.

## Las paredes, en orden de tamaño

El corpus, medido sobre los 90 documentos, usa los kinds `Int`, `String`,
`Sym`, `Bool`, `Register`, `Label` y `None`: **`Float` no aparece nunca** (0
ocurrencias). El emisor serializa el modelo del builder (D-F0-08 P1), y eso
deja huecos que R6 no cubre hoy, de mayor a menor alcance:

1. `unknown` en `registerTypes`: **cerrado por D-M1-20**. Los **6735 registros
   `unknown` en 84/90 documentos** (las entradas se serializan antes de
   `inferRegisterTypes`, P1) se recuperan por una inferencia espejo del backend
   del lenguaje: **5941 (88.2%)**, y **40 de esos 84 documentos quedan sin
   `unknown`**. Los 794 restantes son lo que el backend nativo no representa
   (tags de **enum** -el grueso, `load_var` 492-, `result_unwrap` 71,
   `union_get_data` 20, `map`, targets de `call` no declarados, aritmética con
   operandos sin tipo) y mantienen el rechazo fail-closed. Solo **4
   documentos** se detienen hoy en `unknown` (`test_regex`, `float_literals`,
   `if_expression_return`, `list_index_out_of_range`), ninguno inferible con
   esas reglas.
2. `call` con O1 `String`: **cerrado por D-M1-18** para los 2227 nombres que
   resuelven a una función declarada (de 2917 llamadas `String` en 74/90
   documentos; no hay ni un `Sym` como target y las otras 6 son indirectas por
   `Register`). Quedan **409 llamadas a nombres que el documento no declara**
   —el frente de builtins/alias de la spec §3.1 pasos 4-6: `orbit_string_at`
   130, `print` 121, `orbit_string_len` 78, `orbit_string_slice` 38,
   `orbit_object_*` 24, `orbit_db_*` 4, `orbit_string_find` 3, `system_*` 8—
   que sigue fail-closed.
3. literales del builder: **cerrado por D-M1-19**. Los **412 `load_const` con
   `Sym` en 36/90 documentos** (`true` 206, `false` 112, floats 93, un valor
   de función) se clasifican por gramática de texto (bool, float finito con
   fracción o exponente, o valor de función); el mismo frente descubrió y
   cerró un **miscompile silencioso** de 299 inicializadores literales inline
   de `decl_var` en 46 documentos. Queda abierto, y reportado sin arreglar, el
   **operando literal dentro de una operación binaria** (`ne`/`eq`/`add`/… con
   inmediato): 2622 sitios en 76/90 documentos, incluida la concatenación de
   strings por `add` (376 sitios, frontera U-17) - necesita decisión propia.
   Tras D-M1-20 es el **frente abierto dominante**: 22 primeros rechazos por
   `... must be a valid register` más 2 por `add supports matching int, float,
   or string registers only`.

El hueco 1 cerró con D-M1-20 como decisión de contrato datada (inferencia
espejo del backend del lenguaje, no relajación del gate: un registro que sigue
`unknown` mantiene el rechazo, con el mismo mensaje y código). Los huecos 2 y 3
cerraron con D-M1-18 y D-M1-19. El residuo medido del hueco 3 -el operando
literal dentro de una operación binaria, 2622 sitios en 76/90- es hoy el frente
abierto dominante y sigue necesitando una decisión datada propia.

## Ronda 2026-10-17 (D-M1-21, operandos literales inline, corpus sin cambio)

El residuo del hueco 3 cerró con D-M1-21: los once opcodes
`add`/`sub`/`mul`/`div`/`mod`/`eq`/`ne`/`lt`/`le`/`gt`/`ge` admiten un literal
inline en operand1 y/o operand2 (`Int`/`String`/`Bool`; 2661 sitios en 79/94
documentos, ningún `Float`), materializado en un slot de inmediatos del frame.

| Métrica | D-F0-11 (7/93) | D-M1-21 |
|---|---|---|
| miembros / programas | 94 / 93 | 94 / 93 |
| `filesSha256` | `0106d531…` | **`0106d531…`** (idéntico) |
| oráculo compila y corre | 93/93 | 93/93 |
| R6 compila | 7/93 | **21/93** |
| R6 coincide (exit + stdout) | 7/93 | **21/93** |
| documentos con `unknown` en `registerTypes` | 87/94 | 87/94 |
| opcodes que nunca aparecen en un programa que coincide | 32/41 | **16/41** |

Los 14 programas nuevos (`arith`, `bool_and_or`, `compound_assignment`,
`div_mod_signed`, `division`, `for_range`, `if_branch`, `model_mutation`,
`numeric_literals`, `peephole_int_bool_only`, `strings_eq`, `ternary`,
`while_break`, `while_counter`) pasan de rechazo a coincidencia byte a byte con
el oráculo, sin ningún retroceso. Primer rechazo por opcode tras D-M1-21:

| Primer rechazo | Casos |
|---|---:|
| (nivel de módulo: `registerTypes`, globales, modelos, uniones) | 29 |
| `load_var` | 23 |
| `decl_var` | 6 |
| `add` | 4 |
| `ret`, `list_create`, `list_len` | 2 cada uno |
| `arg`, `gt`, `ne`, `call` | 1 cada uno |

El frente del literal deja de ser un muro: los 6 primeros rechazos que aún caen
en una operación no son la forma del literal, sino (a) cuatro por residuo de
D-M1-20 (un registro operando o destino todavía `unknown`), (b) el `add` de
`String` + `Int` de `peephole_folds`, que es el *string spreading* del lenguaje
-una regla de conversión que R6 no tiene y que se reporta como pregunta-, y (c)
el `ne` sobre bool de `list_element_type`, que el backend acepta y la regla de
R6 para `ne` no. Los dos últimos son frentes nuevos, no admisiones.

## Ronda 2026-10-17 (D-M1-22, `load_var` de un parámetro, corpus sin cambio)

El muro de `load_var` era una sola regla: los 23 primeros rechazos de esa
familia, decodificados en su offset de diagnóstico, son **23/23 lecturas de
parámetro por nombre** (tipos `int` 16, `string` 5, `float` 1, `bool` 1; 14
funciones). D-M1-22 los admite y la familia cae **23 -> 0**.

| Métrica | D-M1-21 | D-M1-22 |
|---|---|---|
| miembros / programas | 94 / 93 | 94 / 93 |
| `filesSha256` | `0106d531…` | **`0106d531…`** (idéntico) |
| oráculo compila y corre | 93/93 | 93/93 |
| R6 compila | 21/93 | **23/93** |
| R6 coincide (exit + stdout) | 21/93 | **23/93** |
| documentos con `unknown` en `registerTypes` | 87/94 | 87/94 |
| opcodes que nunca aparecen en un programa que coincide | 16/41 | **15/41** (`le`) |

Los dos programas nuevos son `float_boundaries` (parámetro float) y
`fn_recursive` (parámetro int), byte a byte con el oráculo y sin retrocesos. Los
otros 21 documentos de la familia avanzan a su siguiente muro: las paredes son
conjuntivas, así que cerrar una familia no cierra por sí solo un programa.
Primer rechazo por opcode tras D-M1-22:

| Primer rechazo | D-M1-21 | D-M1-22 |
|---|---:|---:|
| (nivel de módulo: `registerTypes`, globales, modelos, uniones) | 29 | **35** |
| `call` | 1 | **11** |
| `add` | 4 | **7** |
| `decl_var` | 6 | 6 |
| `list_create` | 2 | **4** |
| `ret`, `list_len` | 2 cada uno | 2 cada uno |
| `arg`, `gt`, `ne` | 1 cada uno | 1 cada uno |
| `load_var` | 23 | **0** |

Lo que crece es el avance de los 21 programas liberados: el siguiente muro
dominante es de nuevo el nivel de módulo (`register type unknown` 9, tipo de
registro `object` 3, `parameter type list<string>` 3) y las llamadas a nombres
que el documento no declara (`orbit_string_len` 5, `orbit_string_slice`,
`orbit_db_query_all`, `orbit_object_*`: el frente de builtins de la spec §3.1
pasos 4-6, que sigue fail-closed a la espera de la tabla del lenguaje). El
`unknown` estructural no se movió: sigue en 87/94 documentos.

## Ronda 2026-10-18 (D-M1-23, `add` de string: spread y concat, y el arena fuera de `main`)

El frente de *string spreading* que D-M1-21 dejó abierto cerró con D-M1-23: `add`
con un `string` en un solo lado dispersa el escalar por los conversores del
runtime (`orbit_int_to_string_selfhost` para `int`, `orbit_float_to_string` para
`float`, `orbit_bool_to_string` para `bool`) y concatena con
`orbit_string_concat`; y `add` de dos strings se baja en **cualquier** función
(el arena se lee del parámetro `_init_arena` en `main` y del global `r6_arena`
fuera de él). La medición de la ronda destapó además dos defectos preexistentes
del lowering, corregidos en la misma ronda (D-M1-24: el id de etiqueta de las
lecturas de lista perdía el ordinal de función; D-M1-25: la longitud de
`list_len` se escribía a 32 bits en un slot `int` que sus consumidores leen a 64).

| Métrica | D-M1-22 (23/93) | D-M1-23/24/25 |
|---|---|---|
| miembros / programas | 94 / 93 | 94 / 93 |
| `filesSha256` | `0106d531.` | **`0106d531.`** (idéntico) |
| oráculo compila y corre | 93/93 | 93/93 |
| R6 compila | 23/93 | **27/93** |
| R6 coincide (exit + stdout) | 23/93 | **27/93** |
| documentos con `unknown` en `registerTypes` | 87/94 | 87/94 |
| opcodes que nunca aparecen en un programa que coincide | 15/41 | **11/41** |

Los cuatro programas nuevos (`interpolation`, `integer_literals` y
`function_pointers` por D-M1-23; `for_index_labels` por D-M1-24/D-M1-25) pasan de
rechazo a coincidencia byte a byte con el oráculo, sin retrocesos. Primer rechazo
por opcode tras la ronda:

| Primer rechazo | D-M1-22 | D-M1-23/24/25 |
|---|---:|---:|
| (nivel de módulo: `registerTypes`, globales, modelos, uniones) | 35 | **37** |
| `call` | 11 | 11 |
| `decl_var` | 6 | 6 |
| `list_create` | 4 | 4 |
| `ret`, `list_len` | 2 cada uno | 2 cada uno |
| `arg`, `gt`, `ne` | 1 cada uno | 1 cada uno |
| `eq` | 0 | **1** (expuesto al liberarse su programa) |
| `add` | 7 | **0** |

El `add` deja de ser un muro (7 -> 0) y el frente dominante vuelve a ser el nivel
de módulo, que absorbe los cuatro programas liberados. Los dos defectos de
lowering quedan pineados por el fixture `35-list-len-bound` (bucle indexado sobre
una lista en una función distinta de `main`), y el `add` de string por el fixture
`34-string-spreading-add` y los controles `x-add-spread-*`/`x-add-string-dest` de
`tests/reject/`.

## Ronda 2026-10-19 (D-M1-26..D-M1-29: `print`, arena en externos, `ret` void y los helpers de string)

Cuatro decisiones en dos tandas, cada una medida antes de la siguiente. D-M1-26
admitió el builtin `print` (interceptado por nombre, destino nunca almacenado,
una línea por argumento según su clase) y D-M1-27 el arena como primer parámetro
C de los externos de `functionNeedsArena`, que es lo que los tres programas
recién liberados necesitaban para no diferir; juntas llevan el corpus **27 -> 35**.
D-M1-28 dejó que una función `void` ignore el valor que lleva su `ret` (el pin
emite `return;` sin leerlo), y `void_return_void_call` pasó a coincidir: **35 ->
36**. D-M1-29 resolvió `orbit_string_len`/`orbit_string_at`/`orbit_string_slice`
(llamadas sin declaración, firmas de la cabecera del runtime, con el arena de
D-M1-27 en `slice`) y la rama string del opcode `list_len`: **36 -> 37 -> 39**,
con `if_expression_else_if`, `strings_concat` y `strings_escape`.

| Métrica | D-M1-25 (27/93) | D-M1-26/27 | D-M1-28 | D-M1-29 (39/93) |
|---|---|---|---|---|
| miembros / programas | 94 / 93 | 94 / 93 | 94 / 93 | 94 / 93 |
| `filesSha256` | `0106d531.` | `0106d531.` (idéntico) | `0106d531.` | **`0106d531.`** (idéntico) |
| oráculo compila y corre | 93/93 | 93/93 | 93/93 | 93/93 |
| R6 compila | 27/93 | **35/93** | **36/93** | **39/93** |
| R6 coincide (exit + stdout) | 27/93 | **35/93** | **36/93** | **39/93** |
| documentos con `unknown` en `registerTypes` | 87/94 | 87/94 | 87/94 | 87/94 |
| opcodes que nunca aparecen en un programa que coincide | 11/41 | 11/41 | 11/41 | **10/41** |

Ninguna ronda perdió un programa: cada una se midió con el binario congelado y
con `--check`, y el trinquete se re-tomó al final (35, 36 y 39). Primer rechazo
por opcode tras D-M1-29:

| Primer rechazo | D-M1-23/24/25 | D-M1-26..29 |
|---|---:|---:|
| (nivel de módulo: `registerTypes`, globales, modelos, uniones) | 37 | **34** |
| `call` | 11 | **3** (`orbit_db_query_all` 2, `system_uptime` 1) |
| `decl_var` | 6 | 6 |
| `list_create` | 4 | **5** (un programa liberado avanzó hasta aquí) |
| `ret` | 2 | 2 (los dos son retornos de `result`, por diseño) |
| `list_len` | 2 | **0** |
| `arg`, `gt`, `ne`, `eq` | 1 cada uno | 1 cada uno |
| `add` | 0 | 0 |

El frente dominante sigue siendo el nivel de módulo, ahora con `register type
unknown` (6), los parámetros `list<int>`/`list<string>` (6), el tipo de registro
`object` (3), las enumeraciones (2) y los globales `CHAR_SPACE`/`WALK_MAX_DEPTH`
(4). El `unknown` estructural no se movió (87/94 documentos): las rondas
liberaron puertas anteriores, no esa.

## Ronda 2026-10-19 (D-M1-30: la lista vacía se crea opaca)

`val xs = []` emite `list_create` con `elemKind 0`, no con el kind del primer
push. R6 lo rechazaba como "un tipo sin accesor". El pin lo acepta: pasa el kind
tal cual a `orbit_list_create_typed` (`c_backend.orb:3580-3585`),
`orbit_list_create` es él mismo `ORBIT_ELEM_OPAQUE` (`collections.c:170`), el
primer push tipado adopta el kind (`:102-116`) y una lectura acepta una lista
opaca (`:118-128`). D-M1-30 admite el kind 0 **en el create**; el acceso con
kind 0 y destino `unknown` sigue fuera.

| Métrica | D-M1-29 (39/93) | D-M1-30 (41/93) |
|---|---|---|
| miembros / programas | 94 / 93 | 94 / 93 |
| `filesSha256` | `0106d531.` | **`0106d531.`** (idéntico) |
| oráculo compila y corre | 93/93 | 93/93 |
| R6 compila | 39/93 | **41/93** |
| R6 coincide (exit + stdout) | 39/93 | **41/93** |
| documentos con `unknown` en `registerTypes` | 87/94 | 87/94 |

Los dos que pasan son `list_index_out_of_range` y `test_process_exit`. Los otros
tres que creaban con kind 0 (`list_push`, `list_element_type_annotation`,
`test_process`) avanzan al muro siguiente: `register type unknown` en dos casos
y un `list_push` con `elemKind 0` en el tercero, que es un push de una lista
dentro de otra lista. Ninguno retrocede.

| Primer rechazo | D-M1-29 | D-M1-30 |
|---|---:|---:|
| (nivel de módulo: `registerTypes`, globales, modelos, uniones) | 34 | **36** |
| `decl_var` | 6 | 6 |
| `call` | 3 | 3 |
| `ret` | 2 | 2 |
| `list_create` | 5 | **0** |
| `list_push` | 0 | **1** |
| `arg`, `gt`, `ne`, `eq` | 1 cada uno | 1 cada uno |

El mensaje `register type unknown is unsupported in M1` pasa de 6 a 8 porque los
dos programas liberados llegan a él. El trinquete se re-tomó en 41.

## Ronda 2026-10-20 (D-M1-31: las comparaciones siguen los brazos del pin)

`eq`, `ne` y las relaciones exigen operandos del mismo tipo en R6. El pin no:
`eq`/`ne` comparan dos strings con `strcmp`, promueven a double si un lado es
float y comparan las palabras en el resto (`c_backend.orb:2745-2772`); las
relaciones hacen lo mismo sin brazo de string (`:2781-2812`). Un bool es una
palabra, y `n == "5"` compara el 5 con la dirección del literal, que es falso.
D-M1-31 admite bool en las seis operaciones y el int contra string en `eq`/`ne`,
y deja fuera el float junto a un int y el string en una relación.

| Métrica | D-M1-30 (41/93) | D-M1-31 (43/93) |
|---|---|---|
| miembros / programas | 94 / 93 | 94 / 93 |
| `filesSha256` | `0106d531.` | **`0106d531.`** (idéntico) |
| oráculo compila y corre | 93/93 | 93/93 |
| R6 compila | 41/93 | **43/93** |
| R6 coincide (exit + stdout) | 41/93 | **43/93** |
| documentos con `unknown` en `registerTypes` | 87/94 | 87/94 |
| código `CGEN` | 51 | **49** |

`if_expression_value` (`c > 4`, bool contra int) y `peephole_folds`
(`n == "5"`, int contra string) pasan a coincidir. `list_element_type` avanza
del `ne` bool/bool a `register type unknown`, así que no se pierde: se mueve al
siguiente muro. Ningún programa retrocede.

| Primer rechazo | D-M1-30 | D-M1-31 |
|---|---:|---:|
| (nivel de módulo: `registerTypes`, globales, modelos, uniones) | 36 | **37** |
| `decl_var` | 6 | 6 |
| `call` | 3 | 3 |
| `ret` | 2 | 2 |
| `list_push` | 1 | 1 |
| `arg` | 1 | 1 |
| `gt`, `ne`, `eq` | 1 cada uno | **0** |

El mensaje `register type unknown is unsupported in M1` pasa de 8 a 9 porque el
programa que avanzó llega a él. El trinquete se re-tomó en 43.

## Ronda 2026-10-20 (D-M1-32: un deletreo de lista es el puntero de lista)

El muro era el tipo del parámetro. R6 exigía escalares, y el pin no: su mapa de
tipos responde `OrbitList*` para `list`, `List` y cualquier `list<...>` a
cualquier profundidad, porque los argumentos dicen qué hay DENTRO de la lista y
no qué es el puntero (`c_backend.orb:476-497`). El tipo del elemento viaja en
el `elemKind` de cada operación (`:604-609`), no en la firma. D-M1-32 reescribe
esos deletreos a `list` en las tres posiciones ABI antes de cualquier
comprobación; un `map<...>` conserva su deletreo y sigue rechazado.

| Métrica | D-M1-31 (43/93) | D-M1-32 (44/93) |
|---|---|---|
| miembros / programas | 94 / 93 | 94 / 93 |
| `filesSha256` | `0106d531.` | **`0106d531.`** (idéntico) |
| oráculo compila y corre | 93/93 | 93/93 |
| R6 compila | 43/93 | **45/93** |
| R6 coincide (exit + stdout) | 43/93 | **44/93** |
| R6 difiere (compila y no coincide) | 0 | **1** |
| documentos con `unknown` en `registerTypes` | 87/94 | 87/94 |
| código `CGEN` | 49 | **47** |

`list_param_element_types` pasa a coincidir. `test_time_ns` compila y ahora
**difiere** en su propio assert 16: `2147483647 + 1` envuelve a int32 en el pin
y sigue en 64 bits en R6, así que `sinceNsBetween(0, pastCap)` responde
2147483648 en vez de -1. Es el primer `r6-differs` del corpus y nombra la pared
del ancho de int. `generic_functions` deja la pared del `arg` y para en
`register type unknown`; `std_lists_generic` y `test_collections` pasan la
puerta de parámetros y paran en `register type unknown`;
`test_collections_boxed` para en `list_push` kind 0. `call_arg_types` y
`parameter_type_fallback` paran ahora en sus parámetros de modelo (`Box`,
`Point`). Ningún programa retrocede.

| Primer rechazo | D-M1-31 | D-M1-32 |
|---|---:|---:|
| (nivel de módulo: `registerTypes`, globales, modelos, uniones) | 37 | **35** |
| `decl_var` | 6 | 6 |
| `call` | 3 | 3 |
| `ret` | 2 | 2 |
| `list_push` | 1 | **2** |
| `arg` | 1 | **0** |

El mensaje `register type unknown is unsupported in M1` pasa de 9 a 12 porque
tres de los programas liberados llegan a él, y desaparecen los tres
`parameter type list... is unsupported in M1`. El trinquete se re-tomó en 45/44.
El puesto del registro `List` no tiene testigo en el corpus (`type_spellings`
para antes, en un `decl_var` de la línea 1, columna 381, en las dos
mediciones): su única evidencia es `ok-list-capital-spelling`.

## Ronda 2026-10-20 (D-M1-33: una constante de módulo es un #define)

El muro era el tipo del global. Los cuatro programas paraban en `global ...
is outside M1 literal int/bool support` porque R6 exigía un tipo declarado, y el
pin no declara ninguno: escribe `#define NOMBRE <texto>` en el preámbulo
(`c_backend.orb:396-399`, `:1633`) y `cDefineLiteral` (`:1647-1651`) devuelve el
payload `String` tal cual. Los 21 globales del corpus llegan con
`typeName: "unknown"` y el texto decimal en `value`; el texto es el tipo.
D-M1-33 lo admite como `int` cuando el texto es un entero decimal dentro de
int32, y mantiene el rechazo para un tipo declarado no escalar y para un texto
que no es un entero.

| Métrica | D-M1-32 (44/93) | D-M1-33 (45/93) |
|---|---|---|
| miembros / programas | 94 / 93 | 94 / 93 |
| `filesSha256` | `0106d531.` | **`0106d531.`** (idéntico) |
| oráculo compila y corre | 93/93 | 93/93 |
| R6 compila | 45/93 | **46/93** |
| R6 coincide (exit + stdout) | 44/93 | **45/93** |
| R6 difiere (compila y no coincide) | 1 | 1 |
| documentos con `unknown` en `registerTypes` | 87/94 | 87/94 |
| código `CGEN` | 47 | **46** |

`global_const` pasa a coincidir. `ast_function_body_is_a_block` avanza a
`union ASTNode variant Program payloads are outside the M1 native subset`,
`test_fs` a `extern return type is unsupported in M1` y `test_imports` a
`register type unknown is unsupported in M1`. Ningún programa retrocede.

| Primer rechazo | D-M1-32 | D-M1-33 |
|---|---:|---:|
| (nivel de módulo: `registerTypes`, globales, modelos, uniones) | 35 | **34** |
| `decl_var` | 6 | 6 |
| `call` | 3 | 3 |
| `ret` | 2 | 2 |
| `list_push` | 2 | 2 |

El mensaje `register type unknown is unsupported in M1` sube de 12 a 13
(`test_imports` llega a él), `extern return type is unsupported in M1` de 2 a 3
(`test_fs`) y desaparecen los cuatro `global ... is outside M1 literal int/bool
support`. El trinquete se re-tomó en 46/45.
