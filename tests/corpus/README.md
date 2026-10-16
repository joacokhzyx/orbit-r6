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
| R6 compila | **2/89** |
| R6 coincide (exit + stdout) | **2/89** |
| documentos con `unknown` en `registerTypes` | **84/90** |

Los dos únicos programas que R6 compila y reproduce son degenerados: su único
registro no existe y su única instrucción es `ret` con un literal `Int`
(`bom_utf8` exit 0, `nesting_legal` exit 9). Ningún otro opcode del corpus
(40 de 41) aparece en un programa que R6 haya corrido completo.

Primer rechazo por opcode (dónde se detiene cada programa):

| Primer rechazo | Casos |
|---|---|
| (nivel de módulo: `registerTypes`, globales, modelos, uniones) | 63 |
| `load_var` | 11 |
| `call` | 6 |
| `decl_var` | 6 |
| `load_const` | 1 |

Mensajes dominantes (`error[CGEN]` en los 87): `register type unknown is
unsupported in M1` 45, `load_var requires a supported local, declared global,
or union tag constant` 11, `decl_var requires a declared local and a matching
scalar, list, void*, model, or function value register` 6, `parameter type
list<string> is unsupported in M1` 3, y una cola de 1-2 casos (globales
`CHAR_SPACE`/`WALK_MAX_DEPTH`, `type kind enumeration`, `extern return type`,
`call target system_pid`/`system_uptime`/`print`/`imported_value`, `function
value 1.5`, campos de modelo con `list<...>`, `register type Shape`/`object`/
`string_raw`, payloads de union). El detalle completo está en el informe de la
corrida (`--json`) y en `docs/M1_REPORT.md`.

## Las tres paredes, en orden de tamaño

El corpus, medido sobre los 90 documentos, usa los kinds `Int`, `String`,
`Sym`, `Bool`, `Register`, `Label` y `None`: **`Float` no aparece nunca** (0
ocurrencias). El emisor serializa el modelo del builder (D-F0-08 P1), y eso
deja tres huecos que R6 no cubre hoy, de mayor a menor alcance:

1. `unknown` en `registerTypes`: **84/90 documentos**. Las entradas se
   serializan antes de `inferRegisterTypes` (P1), así que todo programa con
   registros arrastra tipos `unknown` legítimos; R6 los rechaza con contrato
   estricto (D-M1-09). Es el primer rechazo en 45 programas.
2. `call` con O1 `String`: **2917 llamadas en 74/90 documentos** (el resto, 6,
   son indirectas por `Register`; no hay ni una `Sym`). La spec §3.1 paso 2
   dice "`call` reads O1: `String`/`Sym` → named call"; R6 solo resuelve `Sym`
   como función y trata `String` como construcción de variante (D-M1-12), de
   modo que rechaza llamadas a funciones declaradas en el mismo documento
   (`print`, `system_pid`, `system_uptime`, `imported_value`).
3. literales de float/bool como `Sym`: **412 `load_const` con `Sym` en 36/90
   documentos** (`0.0`, `1.5`, `true`, `false`). La spec §2.1 ya registra que
   `Float` "no dedicated constructor observed" y que `Sym` se emite crudo en
   C (por eso `1.5` y `true` llegan como texto), pero R6 lee todo `Sym` de
   `load_const` como valor de función (D-M1-14).

Las tres son decisiones de contrato (alcance del subconjunto nativo, no
semántica inventada) y quedan registradas en D-M1-17 como abiertas: se
resuelven con una entrada datada nueva, no relajando el gate sobre la marcha.
