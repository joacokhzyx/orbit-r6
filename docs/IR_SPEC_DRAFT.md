# Propuesta de contrato IRv1 — pendiente de revisión

Estado: **borrador M0; no es un formato publicado por Orbit**. Se deriva del
commit del lenguaje `5c01cea68f125f06dd3ac82a61776827bdb0c6f3`. No empezar el
loader ni codegen hasta que el mantenedor del lenguaje revise y acepte este
contrato, o publique uno equivalente.

## Evidencia del inventario

`compiler/ir.orb:3-70` define los 60 miembros de `IROpcode`. Esto prueba el
inventario del enum, no que `IRBuilder` emita cada opcode. El builder construye
instrucciones mediante `ir_init_instr` (definido en `ir.orb:119-121`); usos
concretos representativos están en `builder.orb:1218-1231` (arg/call),
`2606-2681` (saltos/labels/copy), `2906-2910` y `3910-3975` (colecciones),
`3117-3192` (resultados) y `3285-3378` (uniones/campos). La representación
interna está en `ir.orb:72-105`. El dispatch C está en
`c_backend.orb:2422-3598`.

### Lista exacta (60)

`nop`, `load_const`, `load_var`, `store_var`, `decl_var`, `add`, `sub`, `mul`,
`div`, `mod`, `eq`, `ne`, `lt`, `le`, `gt`, `ge`, `and_op`, `or_op`, `not_op`,
`neg`, `begin_block`, `end_block`, `call`, `ret`, `jump`, `jump_if_false`,
`alloc`, `free`, `db_get`, `db_set`, `db_all`, `db_where`, `http_response`,
`label`, `copy`, `arg`, `load_field`, `store_field`, `switch_op`, `type_info`,
`list_create`, `list_push`, `list_get`, `list_set`, `list_len`, `list_pop`,
`map_create`, `map_set`, `map_get`, `map_has`, `map_delete`, `map_keys`,
`result_ok`, `result_err`, `result_unwrap`, `result_is_ok`, `result_unwrap_err`,
`union_create`, `union_get_tag`, `union_get_data`.

## Modelo de valores observado

`IRValue` es una unión etiquetada: `Int(int)`, `Float(float)`, `String(string)`,
`Sym(string)`, `Bool(bool)`, `Register(int)`, `Label(int)` y `None`.
`IRInstruction` contiene `opcode`, `dest` (−1 significa destino nulo), tres
operandos `IRValue` y `elemKind`. Los kinds de colección documentados son 0
(indeterminado), 1 string, 2 int, 3 float, 4 bool, 5 ref. Un `IRFunction`
contiene nombre, parámetros y tipos, instrucciones, cantidad/tipos de registro,
tipo de retorno, indicador extern y metadatos de ruta; `IRModule` agrega
funciones, globals, modelos, tipos y migraciones.

Esto describe el IR interno, **no** un formato textual estable. En este commit
no encuentro `--emit-ir`, spec IR versionada ni corpus IR serializado. No hay
versión publicada que R6 pueda declarar soportada.

## Propuesta exacta de serialización (requiere aceptación del lenguaje)

Propuesta, no formato publicado: un documento JSON UTF-8, sin BOM, un solo
objeto raíz y LF final. Gramática léxica JSON de RFC 8259; claves duplicadas,
campos faltantes, campos extra, tags desconocidos, UTF-8 inválido y trailing
bytes se rechazan. El orden de arrays es semánticamente significativo; orden de
claves de objetos no lo es. La salida del emisor debe ser determinista con
claves en el orden indicado abajo. `null` se usa solo donde se declara.

Raíz exacta: `{"format":"orbit-ir","version":1,"module":M}`. `M` contiene
exactamente arrays `functions`, `globals`, `models`, `types`, `migrations`.
Función: `name`, `params` (array de strings), `param_types` (mismo largo),
`instructions`, `register_count`, `register_types` (mismo largo), `return_type`,
`extern` (boolean), `route` (objeto con `method`, `path`, `has_limit`,
`limit_rate`, `limit_window_ms`, `limit_burst`, o null). Global: `name`, `value`,
`type_name`. Modelo: `name`, `fields` (array de `{name,type_name}`) y
`type_params` (array de strings). Tipo: `name`, `kind`, `variants` (strings),
`rich_variants` (strings, forma interna actual), `methods` (strings).
Migraciones son strings. Para este pin, `models[].type_params` y
`types[].methods` deben estar vacíos; el builder no agrega métodos IR ni
serializa las plantillas genéricas ahí. `route` es null cuando `routeMethod` es
vacío. Cada instrucción contiene
exactamente `op` (uno de los 60 nombres), `dest` (entero o null), `operands`
(array de exactamente 3 values) y `elem_kind` (0..5). Tipo de valor: objeto de
una sola clave; tags `int`, `float64_bits`, `string`, `symbol`, `bool`, `reg`,
`label`, o `none`. Cargas: `int` como decimal firmado en string; `float64_bits`
como 16 dígitos hex minúsculos de IEEE-754 binary64; strings/symbols como
strings JSON; bool como JSON boolean; reg/label como entero JSON no negativo;
`none: null`. El emisor conserva el mismo orden de todos los arrays del IR.

Restricciones: `register_count` no es negativo y `register_types` tiene esa
longitud; cada `dest` y tag `reg` cae en `[0, register_count)`; `elem_kind` solo
es distinto de cero en opcodes de colección; operands no usados son `none`;
cada función conserva el orden textual de sus instrucciones. Claves/funciones,
globales y labels duplicados, y operandos estructuralmente inválidos se
rechazan. Si el lenguaje rechaza esta serialización o propone otra, prevalece
su decisión; R6 no debe compensarla.

Casos borde propuestos: signed zero se preserva en los bits; NaN/infinito se
representan en bits sin normalizar payload; strings escapan comillas, barra,
controles y se codifican UTF-8; enteros fuera de rango del tipo Orbit se
rechazan semánticamente después del parseo. La aceptación debe confirmar esos
puntos, sobre todo si Orbit permite strings no UTF-8 o si `int` no comparte el
mismo dominio que `orbit_int` en `runtime/orbit_api.h`.

**Bloquea revisión del lenguaje:** gramática concreta y escaping; tratamiento de
NaN/infinito y signed zero; overflow y ancho de `int`; reglas de división y
módulo; semántica de registros no inicializados; operandos/destinos requeridos
por opcode; semántica exacta de `switch_op`, `type_info`, `alloc`/`free`, DB y
HTTP; estabilidad de nombres/tipos y declaraciones; targets/ABI; compatibilidad
de versiones. Todo lo no especificado permanece **UNSPECIFIED** en R6 y debe
fallar con diagnóstico identificando opcode/campo, no aproximarse.

## Semántica observable por opcode (solo el lowering observado)

La siguiente tabla describe lo que el C backend consume/emite en
`c_backend.orb:2422-3598`; **no** basta para definir casos no ejercitados por ese
lowering. Lo no fijado queda UNSPECIFIED en R6. `O1..O3` son los operandos
etiquetados de `IRInstruction`; `D` es dest.

| Opcode | Efecto/campos que observa C backend | Hueco/nota para IRv1 |
|---|---|---|
| `nop` | Sin efecto. | Ninguno observado. |
| `load_const` | `D ← O1`. | Codificación/semántica de cada literal depende de `generateValue`. |
| `load_var` | `D ← O1` (nombre). | Resolución/nombre exacto a fijar. |
| `store_var` | O1 nombre, O2 valor. | Tipos/casts siguen a backend y firmas. |
| `decl_var` | O1 nombre, O2 valor. | Diferencia observable con store requiere definirla. |
| `add` | D, O1, O2; numérico o string concat. | Overflows/tipos en spec. |
| `sub` | D, O1, O2. | Overflow UNSPECIFIED. |
| `mul` | D, O1, O2. | Overflow UNSPECIFIED. |
| `div` | D, O1, O2; C divide. | Div-zero, min/-1, rounding UNSPECIFIED. |
| `mod` | D, O1, O2; C remainder. | Div-zero y signo por fijar. |
| `eq` | D = igualdad O1/O2. | String/float/int según type; NaN por fijar. |
| `ne` | D = desigualdad O1/O2. | Idem `eq`. |
| `lt` | D = menor que O1/O2. | Tipos y NaN por fijar. |
| `le` | D = menor/igual O1/O2. | Tipos y NaN por fijar. |
| `gt` | D = mayor que O1/O2. | Tipos y NaN por fijar. |
| `ge` | D = mayor/igual O1/O2. | Tipos y NaN por fijar. |
| `and_op` | D = O1 && O2. | Cortocircuito/evaluación de operands por fijar. |
| `or_op` | D = O1 \|\| O2. | Cortocircuito/evaluación por fijar. |
| `not_op` | D = !O1. | Conversión a bool por fijar. |
| `neg` | D = -O1. | Overflow integer por fijar. |
| `begin_block` | Abre `{`. | Efecto de scope/vida útil por fijar. |
| `end_block` | Cierra `}`. | Efecto de scope/vida útil por fijar. |
| `call` | D opcional, O1 callee; argumentos acumulados por `arg`; C consulta O2 en algunos paths. | Secuencia, aridad, indirect calls y orden de evaluación requieren contrato. |
| `ret` | Retorna O1 si presente. | `None`, void y retorno por fijar. |
| `jump` | Salto a O1 label. | Tipo/alcance de label por fijar. |
| `jump_if_false` | O1 condición, O2 label destino. | Conversión bool y control por fijar. |
| `alloc` | D opcional, O1 tamaño; usa arena. | Fallos, alineación y tipo de arena por fijar. |
| `free` | Emite comentario; no libera en el backend C observado. | **UNSPECIFIED**; no imitar no-op como semántica. |
| `db_get` | D opcional, O1/O2 argumentos. | No disponible en lib net; excluir hasta M3 o variante oficial. |
| `db_set` | D opcional, O1/O2/O3 argumentos. | Idem. |
| `db_all` | D opcional, O1 argumento. | Idem. |
| `db_where` | D opcional, O1/O2 argumentos. | Idem. |
| `http_response` | O1/O2/O3 pasan al constructor de respuesta. | Tipos/layout y ciclo request por fijar. |
| `label` | Define O1 label. | Duplicados/alcanza por fijar. |
| `copy` | D ← O1. | Copia shallow/deep por tipo por fijar. |
| `arg` | Sin D: agrega O1 a args pendientes; con D: binding param por índice O1. | Estado entre calls/reset y semántica exacta por fijar. |
| `load_field` | D, O1 base, O2 nombre. | Layout/resolución depende tipo/modelo. |
| `store_field` | O1 base, O2 nombre, O3 valor. | Layout/resolución y efecto por fijar. |
| `switch_op` | Usa O1 y abre `switch(O1)`. | Casos/cierre no aparecen en lowering; **UNSPECIFIED**. |
| `type_info` | Comentario/no-op, no consume operandos. | **UNSPECIFIED**; no dar resultado inventado. |
| `list_create` | D, O2 count, `elemKind`; recibe `OrbitResult`. | O1 y capacidad/kind 0 por fijar. |
| `list_push` | O1 lista, O2 valor, `elemKind`. | Propagación de errores por fijar. |
| `list_get` | D, O1 lista, O2 índice, `elemKind`. | Out-of-range y error/value encoding por fijar. |
| `list_set` | O1 lista, O2 índice, O3 valor, `elemKind`. | D/error y mutabilidad por fijar. |
| `list_len` | D, O1; usado también para string. | Tratamiento null/overflow por fijar. |
| `list_pop` | D, O1 lista, `elemKind`. | Vacío y error por fijar. |
| `map_create` | D, O1 (backend lo pasa como argumento de create). | Significado/capacidad y tipos por fijar. |
| `map_set` | D, O1 mapa, O2 key, O3 value. | D/error semantics por fijar. |
| `map_get` | D, O1 mapa, O2 key. | Clave faltante y tipo de valor por fijar. |
| `map_has` | D, O1 mapa, O2 key. | Clave/tipos por fijar. |
| `map_delete` | D, O1 mapa, O2 key. | Resultado/falta de clave por fijar. |
| `map_keys` | D, O1 mapa. | Orden de keys por fijar. |
| `result_ok` | D, O1 payload. | Ownership/layout/type por fijar. |
| `result_err` | D, O1 error; O2 puede aportar mensaje. | Variantes, código/mensaje por fijar. |
| `result_unwrap` | D extrae O1. | Error case exacto por fijar. |
| `result_is_ok` | D = flag de O1. | Layout/flags por fijar. |
| `result_unwrap_err` | D extrae error de O1. | Error type/layout por fijar. |
| `union_create` | D; O1 tag, O2 data; emite layout local tag+puntero. | ABI/layout actual no está en helper contractual: **UNSPECIFIED** hasta armonizar. |
| `union_get_tag` | D, O1 unión. | Layout/tag mapping por fijar. |
| `union_get_data` | D, O1 unión. | Payload/layout por fijar. |

El backend C cae a un comentario `Unknown opcode` (`c_backend.orb:3596-3598`),
lo que no cumple la regla de R6. R6 debe diagnosticar y fallar ante todo opcode
fuera de su rango/implementación. El auditor confirma además que el builder no
tiene referencias directas observadas a `nop`, `begin_block`, `end_block`,
`alloc`, `free`, `db_*`, `http_response`, `switch_op`, `type_info`, `map_*` y
`union_create`; sus productores y alcance deben identificarse en la revisión,
no inferirse del enum.

## Familias y dependencias runtime observadas

| Familia/opcodes | Dependencia/observación del backend actual |
|---|---|
| Constantes, variables, aritmética, booleanos, comparación, bloques, saltos, llamadas/retorno | Registros, ABI de llamada y comportamiento del proceso; `print`/builtins requieren símbolos runtime. Sin operación runtime para el cálculo escalar. |
| `alloc`, `free`, campos/modelos | `alloc` usa arena; campos/modelos generan accesos que dependen del tipo/layout. `free` no libera en el C lowering observado. |
| `list_*` | `OrbitList`, create usa `OrbitResult`; push/get/set/pop usan accessors runtime tipados y reglas de error. `orbit_api.h` declara helpers y accesores. |
| `map_*` | `OrbitMap`, `orbit_map_create/set/delete/keys`, accessors `orbit_map_get/has`; no asumir que todas las operaciones devuelven `OrbitResult`. |
| `result_*`, `union_*`, `type_info`, `switch_op` | `OrbitResult` para result ops; `union_create` emite layout local `{int tag; void* data;}`; type_info/switch incompletos en el lowering observado. |
| `db_*` | El backend C genera llamadas DB, pero `libruntime_net.a` es variante **sin DB**: conflicto contractual que requiere artefacto de runtime DB publicado/pineado antes de soportar estos opcodes. No shim ni omisión silenciosa. |
| `http_response` | `OrbitRequest`/`OrbitResponse` y helpers HTTP del runtime. |

ABI observado: `orbit_api.h` declara aliases (`orbit_int` = `int`, `orbit_float`
= `double`, `orbit_string` = `const char*`), arena, resultados, colecciones y
funciones HTTP. El propio header documenta enlace separado con
`.orbit/lib/libruntime_net.a`; no se ha verificado aquí que ese archivo sea un
artefacto reproducible para ambos targets x64. No copiar tipos/layouts ni crear
shims locales: cualquier incompatibilidad se reporta como deriva.

## Targets propuestos y decisiones pendientes

| Target | Estado propuesto |
|---|---|
| Windows x86-64 | M1/M2 inicial, sujeto a disponibilidad de runtime y toolchain pineados. |
| Linux x86-64 | M1/M2 inicial, sujeto a disponibilidad de runtime y toolchain pineados. |
| macOS ARM64 | Fuera de alcance hasta M3; no codegen ni promesa de soporte antes. |

Decisiones: emitir objeto nativo desde IR con ABI del sistema (alternativa
descartada: generar C, prohibido); usar runtime oficial pineado (alternativa
descartada: reimplementar/shim, prohibido); rechazar opcode/plataforma/versión
desconocidos (alternativa descartada: stub o fallback silencioso).

## Aprobación requerida

Solicito revisión humana de la propuesta exacta de serialización, semántica y
disponibilidad del ABI en ambos targets. Hasta esa aceptación no hay IRv1
consumible y M1 no puede comenzar. El soporte declarado de R6 deberá ser `none`
hasta entonces; después será un rango explícito y rechazará versiones fuera de
él. Ninguna parte de este borrador convierte por sí misma una observación del C
backend en semántica normativa del lenguaje.
