# Alineación del loader con la spec IRv1 del lenguaje

Fecha: 2026-10-15. Decisión de adopción: [DECISIONS.md](DECISIONS.md) D-F0-06.

Fuentes comparadas:

- `orbit-lang` `docs/IR_SPEC.md`, blob `371f873eb9713ae2849ca35a761ee7ec0d47260f`
  en `main` `862f257` (el pin actual; D-F0-11 movió la cita desde `16f26a9d…`,
  que era el blob en `299eb09`, y ese cambio es solo menciones de ruta por la
  reorg de `compiler/`; D-F0-10 la había movido desde `110ce439…`, que era el
  blob en `0837185`, al reconciliar §5.5 con el árbol).
- Implementación actual: `src/loader/ir.c`. El draft local `IR_SPEC_DRAFT.md`
  se retiró; su último texto está en el commit `3172044`.

Regla adoptada, textual de la spec: *"Where this document and the draft
disagree, this document prevails; where the language publishes something else,
the language prevails."* Este documento enumera las diferencias que se
migraron; el resultado final está en §7.

## 1. Lo que ya coincide

- Claves de `module`: `functions`, `globals`, `models`, `types`, `migrations`.
- Rechazo de claves extra, claves duplicadas, tags desconocidos, UTF-8 inválido,
  LF final y BOM (la spec lo exige en V-01; R6 ya lo implementa).
- `registerCount` == longitud de `registerTypes`; `dest` y `Register(n)` se
  validan contra `registerCount`.
- `elemKind` 0..5 y distinto de cero sólo en opcodes de colección.
- `IRModelDecl.typeParams` vacío y `IRTypeDecl.methods` vacío: ambos se rechazan
  hoy con `E_UNSPECIFIED` (la spec lo exige en V-15/V-17).
- Exclusión de DB: los cuatro `db_*` se rechazan (D-F0-03); la spec lo confirma
  en §4 y V-19 con recuento propio (4 opcodes, 0 archivos removidos).
- Corpus: 67 ejecutables + 1 support en `tests/suite/`, 22 en `tests/std/`
  (89 ejecutables), idéntico a [PIN.md](PIN.md).
- `int` de 32 bits (V-06) y enteros fuera de rango rechazados.

## 2. Forma del documento: incompatible hoy

| | Draft de R6 (implementado) | Spec del lenguaje (§5.1-5.2) |
|---|---|---|
| Raíz | `{"format":"orbit-ir","version":1,"module":M}` | `{"irVersion":"1","module":M}` — `irVersion` es **string** |
| `dest` vacío | `null` | `-1` |
| Instrucción | `op`, `dest`, `operands`[3], `elem_kind` | `opcode`, `dest`, `operand1`, `operand2`, `operand3`, `elemKind` |
| Valor | objeto de una clave: `{"int":"40"}` | objeto `kind`+`value`: `{"kind":"Int","value":40}` |
| Vacío | `{"none":null}` | `{"kind":"None"}` (sin `value`) |
| Tags | `int`, `float64_bits`, `string`, `symbol`, `bool`, `reg`, `label`, `none` | `Int`, `Float`, `String`, `Sym`, `Bool`, `Register`, `Label`, `None` |
| Enteros | string decimal | número JSON (sin fracción/exponente, int32) |
| Floats | 16 hex minúsculas (`float64_bits`) | número JSON; **no finitos rechazados** (V-08) |
| Función | `param_types`, `register_count`, `register_types`, `return_type`, `extern`, `route` (objeto o `null`) | `paramTypes`, `registerCount`, `registerTypes`, `returnType`, `isExtern`, más 6 campos planos `routeMethod`, `routePath`, `routeHasLimit`, `routeLimitRate`, `routeLimitWindowMs`, `routeLimitBurst` |
| Modelo | `type_params`, `fields[].type_name` | `typeParams`, `fields[].typeName` |

Consecuencia inmediata: el parser de R6 **no puede leer** el formato de la spec
(rechaza números JSON con `.`/`e`, exige `format`, `dest` null, etc.) y el
formato de R6 no pasa V-02 (`format` es clave desconocida para la spec).

Pérdida de expresividad a aceptar: V-08 prohíbe NaN/infinito como literales. Los
20 fixtures actuales usan sólo floats finitos (`3ff8…`, `0000…`, `8000…`, etc.),
así que **no se pierde evidencia existente**; la comparación IEEE con NaN del
fixture 10 surge de operaciones, no de literales. Un `ir_float(NaN)` del builder
no sería serializable: queda como pregunta P2 al lado lenguaje.

## 3. Reglas de validación: dónde se aplican hoy

| Regla | Estado en R6 | Acción |
|---|---|---|
| V-01 LF/BOM/elisión | ok en loader | Falta sólo el marcador de elisión `…[N chars]` si algún día se escribe a mano |
| V-02 raíz exacta | ok (con otro nombre de clave) | Cambia al formato de la spec |
| V-03 opcode exacto | ok (`E_OPCODE`) | Sin cambio |
| V-04 `elemKind` | ok, pero R6 sólo lo admite en `list_*`; la spec dice "colecciones" (incluye `map_*`) | Alinear cuando exista soporte de map |
| V-05 operandos `kind`+`value` | parser distinto | Cambia con el formato |
| V-06/V-07 rangos | ok | Sin cambio |
| V-08 floats finitos | **no**: R6 acepta cualquier patrón de 16 hex, incluido NaN | Rechazo nuevo en loader |
| V-09 registros negativos | ok (`E_RANGE`) | Sin cambio |
| V-10 `dest` requerido en opcodes con valor | **no**: R6 lo exige en codegen (`CGEN`) | Mover/duplicar en loader con diagnóstico por opcode |
| V-11 longitudes y cotas | ok | Sin cambio |
| V-12 `params`/`paramTypes` | ok | Renombrar claves |
| V-13 aridad O2 de `call` | **no**: R6 ignora O2 y consume *todos* los `arg` pendientes; sus fixtures usan `operand2: none` | Cambio real de semántica: leer O2, consumir exactamente esa cantidad, rechazar aridad > pendientes (E-05) |
| V-14 salto a label inexistente | **no**: hoy falla en codegen (`jump target label is undefined`) | Mover/duplicar en loader |
| V-15 `typeParams` vacío | ok | Sin cambio |
| V-16 coherencia de `route` | **no**: `parse_route` acepta `route` no nulo con `method` vacío y marca `has_route=true` | Rechazo nuevo: `method==""` ⇔ `path==""` y límites en cero/false (E-10) |
| V-17 `methods` vacío | ok | Sin cambio |
| V-18 extern resuelve fuera | parcial | La spec acepta un extern **con cuerpo y lo ignora** (E-09); R6 lo rechaza en codegen ("extern function %s must not contain instructions"). Alinear |
| V-19 DB excluida | ok (`E_UNSUPPORTED`) | Mantener |

## 4. Lo que el corpus va a exigir y no está en el plan de R6

Cuando exista el emisor, el diferencial de los 89 programas expondrá
comportamiento del backend C documentado por la spec que R6 hoy no modela:

- §3.1: alias de callee (`slice`, `at`, `find`/`indexOf` → `orbit_string_*`),
  mínimos de aridad por callee, forma especial de `print` (1 argumento
  string/float/otro; !=1 argumento no imprime nada), sufijo `_` para keywords C,
  y `arena` prependido en ciertas llamadas (U-04, lista sin auditar).
- §3.2: llamadas indirectas con casts por tipo de parámetro (R6 sólo admite
  llamada directa; seguirá rechazando lo demás de forma explícita).
- §2.3: `registerTypes` es *advisory*; `inferRegisterTypes` la reescribe en el
  backend C. R6 hoy usa `register_types` como contrato estricto de tipos
  ("value type %s does not match register type %s"). Si el emisor serializa el
  IR antes de esa pasada, habrá tipos `"unknown"` legítimos que R6 hoy
  rechazaría: preguntas P1/P4.
- §2.4: además de `val NAME = <int|bool>`, el builder produce globals con valor
  `ir_register` (`builder.orb:2377`); R6 sólo soporta literales int/bool
  (D-M1-01). Models, enums, unions y fields del corpus quedan fuera del
  subconjunto actual.

Estado de esta lista tras las fases C–E (2026-10-16):

- **Cerrado por decisión datada**: §2.3 `unknown` (D-M1-20: inferencia espejo de
  `inferRegisterTypes`, tres pasadas, fail-closed cuando ninguna regla decide);
  `call` con target `String` (D-M1-18: resuelve por orden de declaración
  función → modelo → variante); literal `Sym` float/bool y inicializadores
  literales inline de `decl_var` (D-M1-19); globals con valor `ir_register`
  (D-M1-15: ruta inalcanzable del builder, rechazo pineado); modelos y fields
  (D-M1-11) y uniones (D-M1-12); llamada indirecta con valores de función
  (D-M1-14); aridad O2 de `call` (A4/E-05) y `extern` con cuerpo (E-09).
- **Sigue abierto, reportado y sin autorización**: la tabla de **alias de
  callee** de §3.1 (`slice`/`at`/`find` → `orbit_string_*`) y las formas
  especiales de `print`. El diferencial mide **409 `call` con target `String`
  sin declaración en el módulo, en 37 documentos y 22 nombres** (dominados por
  `orbit_string_at` 130, `print` 121, `orbit_string_len` 78,
  `orbit_string_slice` 38), todos fail-closed. También siguen abiertas U-04
  (`arena` prependido), U-05 (orden de argumentos indirectos) y U-17 (formato
  de `print`), y los **enum** (constantes de tag sin tipo de acceso fuera de
  `union`).

No confundir con TIR: `orbit frontend` emite `orbit-tir 1`
(`compiler/frontend/tir.orb`), un formato de texto con bloques y value ids para
el pipeline interno. No es IRv1 ni un candidato: convertir TIR→IRv1 invertiría
el lowering y el diferencial mediría una reconstrucción, no el IR del builder.

## 5. Plan de migración (orden propuesto)

1. **Loader**: adoptar el formato de la spec (`irVersion`/`module`/`operand1..3`
   con `kind`+`value`/`dest` -1/floats numéricos/`route` plano) conservando los
   códigos `E_*` y el diagnóstico de una línea.
2. **Validaciones**: agregar V-08, V-10, V-13, V-14, V-16 y E-09 en el loader,
   con mensaje que nombre opcode/campo; el codegen conserva sus propios chequeos.
3. **Fixtures**: regenerar los 20 `.json` (mecánico) — renombrar claves, `dest`
   -1, valores `kind`/`value`, aridad O2 en los 6 con `call`
   (`01`, `02`, `05`, `16`, `17`, `20`) y floats numéricos en los 5 con
   `float64_bits` (`03`, `09`, `10`, `11`, `17`). Los `.orb` no cambian.
   Criterio de aceptación: `run_diff.py` sigue en **20/20** con los mismos exit
   y stdout.
4. **Codegen**: consumir la aridad O2 de `call` (E-05), aceptar extern con
   cuerpo ignorándolo (E-09) y validar coherencia de `route` (V-16).
5. **Recién entonces**: rama en `orbit-lang` desde `5c01cea` con el emisor
   `--emit-ir` (§5), corpus de 89+1 programas y manifiesto de hashes; nuevo pin
   con hashes en [PIN.md](PIN.md) y evidencia diferencial ampliada.

## 6. Respuestas del lado lenguaje (2026-10-16)

Las cinco preguntas que bloqueaban el emisor quedaron respondidas el
2026-10-16 (registradas también en DECISIONS.md, D-F0-08):

- **P1 — serialización de `registerTypes`**: el emisor serializa **antes** de
  `inferRegisterTypes` (modelo del builder; §5.1 define la forma del texto a
  partir del modelo en memoria). Consecuencia aceptada: el corpus llevará
  tipos `unknown` legítimos; R6 los rechaza hoy (contrato estricto) y quedarán
  en la lista de faltantes del diferencial del corpus, con decisión datada
  después.
- **P2 — floats no finitos**: ante un `ir_float` NaN/infinito el emisor
  **falla con diagnóstico** (V-08, fail-closed; sin encoding por bits mientras
  ningún programa lo necesite).
- **P3 — claves desconocidas**: el rechazo aplica a **todo el documento**
  (raíz y niveles internos); R6 ya rechaza en todos los niveles.
- **P4 — §3.1/§3.2**: **normativos en lo auditado** (alias, mínimos de
  aridad, formas de `print`, retornos); U-04 (arena), U-05 (orden de argumentos
  indirectos) y U-17 (formato de `print`) siguen explícitamente abiertas.
- **P5 — corpus serializado**: lo emite el lenguaje; R6 lo **regenera y pinea
  por hash + manifiesto** (mismo patrón que `runtime/orbit_api.h` y el archive
  del runtime).
- **Autorización (2026-10-16)**: rama `r6-emit-ir` desde `5c01cea` en un clon
  aparte del lado lenguaje (checkout del usuario intacto); sin re-pin hasta
  que rama, corpus y manifiesto existan con sus gates en verde.

## 7. Estado (2026-10-15): migración completada en R6

Commit `06d7d2c` (`feat: adopt language-side IRv1 format (20/20 diff)`); los
pasos 1–4 de §5 quedaron así:

- **Formato**: `src/loader/ir.c` acepta exactamente
  `{"irVersion":"1","module":M}`, instrucciones
  `opcode`/`dest`/`operand1..3`/`elemKind`, valores `kind`+`value`
  (`{"kind":"None"}` sin payload), `dest` `-1` para "sin destino", `route*`
  plano y floats como número JSON. El documento draft se rechaza con
  `E_FORMAT` y pista de migración (sin doble aceptación, D-F0-06).
- **Validaciones nuevas en loader**: V-06/V-07 (enteros JSON; fracción o
  exponente en contexto entero -> `E_SCHEMA`), V-08 (no finito -> `E_RANGE`),
  V-10 (dest requerido por opcode y por `call` no-void), V-13 (aridad O2:
  entero >= 0 y <= pendientes), V-14 (jump a label existente), V-16 (route
  coherente), V-18 (cuerpo de extern: se parsea y se ignora). V-11/V-12 y el
  marcador de elisión `…[N chars]` (V-01) también quedaron en el loader.
- **Fixtures**: los 20 `.json` se regeneraron con
  `scripts/migrate-m1-fixtures.py` (conversión mecánica con relectura de bits
  de floats); los 6 con `call` llevan ahora su aridad en `operand2`. Ningún
  `.orb` cambió. `.gitattributes` fija LF para `*.json`/`*.orb`.
- **Codegen**: `call` consume exactamente los últimos O2 argumentos pendientes
  (E-05) y mantiene la exigencia de aridad == parámetros declarados; un extern
  con cuerpo se ignora (E-09). Las rutas Kynx se rechazan con `E_UNSUPPORTED`
  (fuera del subconjunto nativo; antes el rechazo era accidental en el
  codegen).

Evidencia: gate **20/20** con exit/stdout idénticos a la línea base
(`.r6-build/baseline-20.txt` vs `.r6-build/migrated-20.txt`), el ejemplo mínimo
de §5.2 de la spec compila y corre (exit 0), y el draft de `01-int-call`
termina con `error[E_FORMAT]`.

Estado posterior (2026-10-16): la suite negativa de §3 quedó commiteada en
`tests/reject/` con caso y código esperado por regla (**137/137**: 113 rechazos
+ 24 controles, decisiones D-M1-06..D-M1-20) y el gate diferencial creció a
**31/31** con los fixtures `21`–`31`; las respuestas P1–P5 llegaron el
2026-10-16 (§6, D-F0-08) y la fase D tiene el emisor `--emit-ir` aditivo, el
corpus pineado por `filesSha256` y su diferencial de medición (D-M1-17 en
**2/89**, D-M1-20 en **6/89**). **El re-pin de la fase D quedó hecho**
(D-F0-10, 2026-10-16): la rama `r6-emit-ir` se mergeó en el `main` del usuario
(`299eb09`, sin push), §5.5 se reconcilió con el árbol a **93+1** (71+1 suite +
22 std) y el primer pin del corpus es `filesSha256 0106d531…`
(`languageCommit 299eb09`), medido en **7/93**; `PIN.md` y esta cita pasan al
blob `16f26a9d…`.

Estado posterior del pin (2026-10-17, **D-F0-11**): el lenguaje siguió moviéndose
dentro del mismo tramo (`e75f2c7` H-ARENA, `8b9107a` retirada del Arena Oracle,
`0a145cc` fix del cargador, `862f257` reorg de `compiler/`) y R6 re-pinó a `main`
`862f257`. Lo relevante para esta alineación: **el corpus no cambió**
(`filesSha256 0106d531…`, 93+1, 5419454 B — medido por R6 en `299eb09`,
`8b9107a` y `862f257`), `--emit-ir` y `scripts/emit_ir_corpus.py` no cambiaron de
ruta ni de comportamiento, y la cita de la spec pasa al blob `371f873e…` (solo
menciones de ruta: 16 líneas). La medición sigue en **7/93**. El canónico pasa a
`9906A6F7…` y `runtime/orbit_api.h` a `bf098c54…`.
