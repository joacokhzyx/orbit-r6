# tests/corpus - diferencial sobre el corpus IR del lenguaje (medición, no puerta)

Fecha: 2026-10-16. Decisión: `docs/DECISIONS.md` D-M1-17. Corpus: los 93+1
documentos IRv1 que el lenguaje emite sobre `tests/suite` (71 programas +
`imports.support.orb`) y `tests/std` (22 programas) - el corpus designado por
`orbit-lang/docs/IR_SPEC.md` §5.5 y reconciliado con el árbol en D-F0-10 (sin
cambio en D-F0-11) - pineados por hash en `manifest.json`.

```powershell
python tests/corpus/run_corpus.py `
  --compiler C:\Temp\orbit-fp-26cfb87400.exe `
  --cc C:\w64devkit\bin\gcc.exe `
  --orbit-root C:\Users\Usuario\Downloads\orbit-lang `
  --check `
  --r6-command .r6-build\r6.exe build '{ir}' -o '{exe}'
```

- `manifest.json` - el pin: 94 entradas (`path`, `kind`, `ir`, `bytes`,
  `sha256`), `filesSha256` (un solo hash del corpus) y la identidad del emisor
  (versión, commit del lenguaje, sha256 del binario). Lo produce
  `scripts/emit_ir_corpus.py` en el árbol del lenguaje (`main`, pin actual
  `862f257`): regenerar y comparar es el contrato, no copiar ficheros. Mismo
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
