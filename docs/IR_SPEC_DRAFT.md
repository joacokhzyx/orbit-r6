# Propuesta de contrato IRv1 — pendiente de revisión

Estado: **borrador M0; no es un formato publicado por Orbit**. Se deriva del
commit del lenguaje `5c01cea68f125f06dd3ac82a61776827bdb0c6f3`. No empezar el
loader ni codegen hasta que el mantenedor del lenguaje revise y acepte este
contrato, o publique uno equivalente.

## Evidencia del inventario

`compiler/ir.orb:3-70` define los 59 miembros de `IROpcode`. `compiler/builder.orb`
construye instrucciones usando `ir_init_instr` y rellena sus operandos; la
representación de almacenamiento está definida en `compiler/ir.orb:72-105`.
`compiler/c_backend.orb:2661-2698` muestra la convención de `arg`/`call`, y
`compiler/c_backend.orb:3274-3592` contiene el lowering de memoria, colecciones,
resultados, uniones, DB y HTTP.

### Lista exacta (59)

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

## Propuesta de serialización para revisión

Propuesta, no decisión unilateral: salida UTF-8 con cabecera obligatoria
`orbit-ir 1`, seguida de una representación textual canónica del módulo. El
archivo debe ser autocontenido, preservar orden de funciones/instrucciones y
tipos, e incluir cada operando etiquetado, `elemKind`, firma, globals,
declaraciones de modelo/tipo y versión. Literales string deben tener un escape
sin pérdida; floats deben tener representación round-trip; enteros y etiquetas
deben ser decimales con signo según los tipos fuente.

**Bloquea revisión del lenguaje:** gramática concreta y escaping; tratamiento de
NaN/infinito y signed zero; overflow y ancho de `int`; reglas de división y
módulo; semántica de registros no inicializados; operandos/destinos requeridos
por opcode; semántica exacta de `switch_op`, `type_info`, `alloc`/`free`, DB y
HTTP; estabilidad de nombres/tipos y declaraciones; targets/ABI; compatibilidad
de versiones. Todo lo no especificado permanece **UNSPECIFIED** en R6 y debe
fallar con diagnóstico identificando opcode/campo, no aproximarse.

## Familias y dependencias runtime observadas

| Familia/opcodes | Dependencia/observación del backend actual |
|---|---|
| Constantes, variables, aritmética, booleanos, comparación, bloques, saltos, llamadas/retorno | Registros, ABI de llamada y comportamiento del proceso; `print`/builtins requieren símbolos runtime. Sin operación runtime para el cálculo escalar. |
| `alloc`, `free`, campos/modelos | `orbit_alloc` y layouts/type declarations compartidos con el runtime. |
| `list_*` | `OrbitList`, `orbit_list_create_typed`, `orbit_list_push_*`, `orbit_list_set_*`, accesores `orbit_list_read_*`, `orbit_list_len` y semántica `OrbitResult`; APIs declaradas en `runtime/orbit_api.h`. |
| `map_*` | `OrbitMap`, `orbit_map_create/set/delete/keys`, accessors `orbit_map_get/has` y `OrbitResult`. |
| `result_*`, `union_*`, `type_info`, `switch_op` | Layouts/tag/payload y helpers `OrbitResult` / tipos; el contrato de disposición y errores aún requiere validación contra `orbit_api.h` y C backend por opcode. |
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

Solicito revisión al mantenedor de Orbit de la propuesta de serialización,
semántica y disponibilidad del ABI en ambos targets. Hasta esa revisión no hay
formato IRv1 consumible y el M1 no puede comenzar. El soporte declarado de R6
deberá ser `none` hasta la aceptación; después deberá ser un rango explícito y
rechazar toda versión fuera de él.
