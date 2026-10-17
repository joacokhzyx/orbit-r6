# tests/corpus - diferencial sobre el corpus IR del lenguaje (medición, no puerta)

Fecha: 2026-10-16. Decisión: `docs/DECISIONS.md` D-M1-17. Corpus: los 89+1
documentos IRv1 que el lenguaje emite sobre `tests/suite` (67 programas +
`imports.support.orb`) y `tests/std` (22 programas) - el corpus designado por
`orbit-lang/docs/IR_SPEC.md` §5.5 - pineados por hash en `manifest.json`.

```powershell
python tests/corpus/run_corpus.py `
  --compiler C:\Temp\orbit-fp-67e4ecfa75.exe `
  --cc C:\w64devkit\bin\gcc.exe `
  --orbit-root C:\Users\Usuario\Downloads\orbit-lang-emit-ir `
  --check `
  --r6-command .r6-build\r6.exe build '{ir}' -o '{exe}'
```

- `manifest.json` - el pin: 90 entradas (`path`, `kind`, `ir`, `bytes`,
  `sha256`), `filesSha256` (un solo hash del corpus) y la identidad del emisor
  (versión, commit del lenguaje, sha256 del binario). Lo produce
  `scripts/emit_ir_corpus.py` en el árbol del lenguaje (rama `r6-emit-ir`):
  regenerar y comparar es el contrato, no copiar ficheros. Mismo patrón con el
  que se pinean `runtime/orbit_api.h` y el archivo del runtime.
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

## Medición 2026-10-16 (emisor `1e6a378`, corpus `10f6e7d9`)
| Métrica | Valor |
|---|---|
| miembros / programas | 90 / 89 (+1 módulo de soporte) |
| oráculo compila y corre | 89/89 |
| R6 compila | **3/89** |
| R6 coincide (exit + stdout) | **3/89** |
| documentos con `unknown` en `registerTypes` | **84/90** |

El pin se tomó con **2/89** (D-M1-17) y D-M1-18 lo lleva a **3/89**:
`tests/suite/imports.orb` (exit 37, opcodes `call` + `ret`) pasa de rechazo a
coincidencia completa, y es el primer programa no degenerado que R6 completa.
Los otros dos siguen siendo degenerados: `bom_utf8` (exit 0) y `nesting_legal`
(exit 9) tienen un único registro inexistente y una única instrucción `ret` con
literal `Int`. 39 de los 41 opcodes del corpus no aparecen todavía en un
programa que R6 haya corrido completo. D-M1-19 mantiene el conteo (las puertas
de nivel de módulo y `unknown` dominan) pero corrige el lowering de los
literales: `load_const` deja de ser el primer rechazo de cualquier programa.

Primer rechazo por opcode (dónde se detiene cada programa), tras D-M1-19:

| Primer rechazo | Casos |
|---|---|
| (nivel de módulo: `registerTypes`, globales, modelos, uniones) | 65 |
| `load_var` | 13 |
| `decl_var` | 6 |
| `ret` | 2 |

Mensajes dominantes (`error[CGEN]` en los 86): `register type unknown is
unsupported in M1` 47, `load_var requires a supported local, declared global,
or union tag constant` 13, `decl_var requires a declared local and a matching
scalar, list, void*, model, or function value register` 6, `parameter type
list<string> is unsupported in M1` 3, y una cola de 1-2 casos (globales
`CHAR_SPACE`/`WALK_MAX_DEPTH`, `type kind enumeration`, `extern return type`,
campos de modelo con `list<...>`, `register type
Shape`/`object`/`string_raw`, payloads de union, `returning result values`,
`void ret must not carry a value`). Ya no queda ningún `call target ...` entre
los primeros rechazos (cerró con D-M1-18) ni ningún `function value 1.5` (lo
cerró D-M1-19). El detalle completo está en el informe de la corrida (`--json`)
y en `docs/M1_REPORT.md`.

## Las paredes, en orden de tamaño

El corpus, medido sobre los 90 documentos, usa los kinds `Int`, `String`,
`Sym`, `Bool`, `Register`, `Label` y `None`: **`Float` no aparece nunca** (0
ocurrencias). El emisor serializa el modelo del builder (D-F0-08 P1), y eso
deja huecos que R6 no cubre hoy, de mayor a menor alcance:

1. `unknown` en `registerTypes`: **84/90 documentos**. Las entradas se
   serializan antes de `inferRegisterTypes` (P1), así que todo programa con
   registros arrastra tipos `unknown` legítimos; R6 los rechaza con contrato
   estricto (D-M1-09). Es el primer rechazo en 47 programas.
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
   strings por `add` (376 sitios, frontera U-17) — necesita decisión propia.

El hueco 1 sigue siendo una decisión de contrato (alcance del subconjunto
nativo, no semántica inventada) y sigue registrado como abierto en D-M1-17: se
resuelve con una entrada datada nueva, no relajando el gate sobre la marcha.
Los huecos 2 y 3 cerraron con D-M1-18 y D-M1-19.
