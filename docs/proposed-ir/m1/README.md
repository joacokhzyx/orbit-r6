# Fixtures M1 manuales IRv1 (diferencial contra el pin)

Cada `.json` usa el formato IRv1 del lado lenguaje
(`orbit-lang/docs/IR_SPEC.md`, blob `371f873eb9713ae2849ca35a761ee7ec0d47260f`):
raíz `irVersion`+`module`, operandos `{"kind":K,"value":V}`, `dest` `-1` para
"sin destino" y la aridad de cada `call` en `operand2` (V-13). El draft local
`docs/IR_SPEC_DRAFT.md` está superseded (D-F0-06) y los archivos son LF por
`.gitattributes` (V-01). Cada `.orb` es el fuente de comportamiento equivalente
para el oracle gcc. Outputs/exit codes se comparan como exit status/stdout
bytes con `orbit build` del pin. `orbit_print` se invoca como símbolo runtime
declarado por ABI C en `runtime/orbit_api.h`.

| Caso | Familia | Exit esperado | Stdout esperado |
|---|---|---:|---|
| `01-int-call` | suma entera y llamada runtime | 42 | `M1-add\n` |
| `02-string` | concat de strings y llamada runtime | 0 | `IRv1\n` |
| `03-float-branch` | suma/igualdad binary64 y branch | 0 | vacío |
| `04-branch` | salto condicional | 7 | vacío |
| `05-call-3args` | parámetros y llamada de tres argumentos | 6 | vacío |
| `06-signed-arithmetic` | sub/mul/div/mod y valores negativos | 2147483669 | vacío |
| `07-signed-compare` | comparación signed y branch | 3 | vacío |
| `08-bool-logic` | and/or/not sobre booleanos | 9 | vacío |
| `09-float-arithmetic` | sub/mul/div binary64 con resultado exacto | 10 | vacío |
| `10-float-compare-nan` | seis comparaciones con NaN y signed zero | 0 | vacío |
| `11-neg` | negación entera y binary64 | 0 | vacío |
| `12-jump-copy` | salto incondicional y copia de registro | 3 | vacío |
| `13-while-counter` | loop con variable local, branch hacia atrás y acumulación | 10 | vacío |
| `14-global-constants` | lectura de `val`/`const` globales int/bool, literal negativo y branch | 33 | vacío |
| `15-string-equality` | igualdad por contenido y desigualdad de strings, incluida concatenación | 5 | vacío |
| `16-call-stack-args` | argumentos enteros 5.º/6.º por pila en llamada y callee | 11 | vacío |
| `17-call-stack-float` | argumento binary64 en posición 5 y entero en posición 6 | 11 | vacío |
| `18-int-list-literal-len` | create/push de lista int, referencia local y lectura de longitud | 3 | vacío |
| `19-int-list-get` | lectura indexada de lista int | 20 | vacío |
| `20-int-list-get-oob` | índice de lista fuera de rango vía función dinámica | 2 | vacío |
| `21-nop-blocks` | `nop`/`begin_block`/`end_block` sin efecto observable | 7 | vacío |
| `22-result-ops` | `result_ok`/`err`/`is_ok`/`unwrap`/`unwrap_err` con payload float, int y string, `catch` con binding | 46 | vacío |
| `23-list-set-pop` | `list_set`/`list_pop` y kinds string/int/float/bool fuera de `main` (arena global) | 135 | vacío |
| `24-model-fields` | constructor de modelo y `load_field`/`store_field` int/float/bool/string (refs `Vec.*`) | 42 | `seed\nmut\nf-ok\n` |
| `25-union-tag-data` | ctor de unión unit/single/multi (pack), `union_get_tag`/`union_get_data` y cuatro matches exhaustivos | 85 | `u-ok\n` |
| `26-fn-values` | valores de función sin captura, llamada indirecta, rebinding `var` y binding externo. **Retirado del diferencial en D-F0-13**: el pin `17ed624` rechaza un extern como valor, así que el `.orb` ya no construye con el oráculo; el IR sigue medido por `ok-fn-extern` (exit 98) | 42 | `fn-ok\n` |
| `27-call-string-target` | target de llamada `String` (el formulario del builder) con destino `void` y con destino `int` | 42 | `string-target\n` |
| `28-sym-literals` | literales bool/float como texto `Sym` en `load_const`, con el `decl_var` del builder | 42 | vacío |
| `29-decl-var-literals` | inicializadores literales inline en `decl_var` (int/string/bool) leídos por `load_var` | 42 | `ok\n` |
| `30-inferred-model-fields` | stream del emisor con los seis registros en `unknown`: ctor de modelo, local, `load_field` y `add` inferidos | 7 | vacío |
| `31-inferred-unary-neg` | stream del emisor con cuatro registros en `unknown`: cadena de `neg`/`add` inferida | 2 | vacío |
| `32-inline-literal-operands` | stream del emisor con literales inline en operandos de `mul`/`sub`/`div`/`mod`/`add`/`ne`/`ge`/`lt`/`eq`/`le`/`gt` y concatenación de strings | 42 | `lit-ok\n\n` |
| `33-parameter-by-name` | stream del emisor leyendo por nombre parámetros int/string/bool/float (`load_var` de un parámetro) | 42 | vacío |
| `34-string-spreading-add` | stream del emisor con `add` de string en un solo lado (spread int/float/bool) y concatenación de dos strings, en `main` y en un callee | 42 | `[spread]\n` |
| `35-list-len-bound` | bucle indexado sobre una lista en una función distinta de `main`: la longitud de 64 bits y la etiqueta por función | 42 | vacío |
| `36-print-builtin` | el builtin `print` con un argumento (string/int/float) y destino `void`: las tres reducciones medidas del lowering | 0 | `hello\n42\n1.5\n` |
| `37-arena-extern-call` | extern del runtime con el arena como primer parámetro C (`orbit_int_to_string`): el arena se antepone y los argumentos se desplazan | 0 | `42\n-7\n` |
| `38-void-ret-value` | `return f()` de un callee `void`: el `ret` lleva el registro destino de la llamada y no se lee (tail call encadenada y forma sentencia) | 42 | `T\nT\n` |
| `39-string-intrinsics` | `.len()`, `.at()` y `.slice()` sin declaración en el módulo: firmas de la cabecera del runtime y el arena de D-M1-27 en el slice | 0 | `5\n90\nbc\n` |
| `40-list-create-opaque` | lista vacía creada con `elemKind 0` (opaca): el primer push tipado adopta el kind y las dos lecturas llevan `elemKind 2` | 0 | (sin salida) |
| `41-bool-and-word-comparisons` | `bool` en `gt`/`ne`/`eq`, `int` contra `string` por la vía de palabras y `float` contra `float` en `ne` | 0 | (sin salida) |
| `42-list-parameter-spellings` | parámetro `list<int>`, parámetro `list<string>`, retorno `list<int>`, un resultado de `call` con tipo `list<int>` y un parámetro `list<list<int>>`: el deletreo nombra el contenido y el puntero es uno | 0 | (sin salida) |
| `43-global-constant-type` | tres constantes de módulo con `typeName` `unknown` y texto decimal (`32`, `3`, `-2`): el texto es el tipo, se imprimen, se comparan y se suman | 0 | `32\r\n3\r\n-2\r\n` |
| `44-enum-tag-constant` | una enumeración de tres variantes: constante `Color_TAG_*`, campo de modelo del tipo, parámetro, retorno y ocho comparaciones | 0 | `blue\r\nred\r\n2\r\nred\r\n` |
| `45-int-overflow` | un `int` de 32 bits: `bump(2147483647)` negativo, `sq(65536)` igual a 0, `over - 1` igual a 2147483647, `sum4(1, 2, 3, -4)` igual a 2 e `id(-1)` negativo, con el destino de cada `call` y literales de argumento inline | 0 | (sin salida) |
| `46-process-argc` | el punto de entrada guarda `argc`/`argv`: `orbit_os_argc` al menos 1, `orbit_os_argv(0)` no vacío y `orbit_os_argv(argc)` vacío | 42 | `1\r\n` |
| `47-opaque-list-read` | lectura opaca (`elemKind 0`) de un parámetro `list` tipada por su único uso: argumento `string` de `strLen` y argumento `int` de `twice`, con `-5` y `2147483647` como palabras | 42 | `5\r\n` |
| `48-model-list-field` | campos `list<int>` y `list<string>` de un modelo: constructor, `push` a través del campo, lectura por locales tipados y reemplazo de la lista | 42 | `bc\r\n` |
| `49-model-parameter` | un modelo como parámetro: lectura de campos, mutación visible en el llamador, quinto argumento por pila y retorno de un campo `string` | 42 | `origin\r\n` |
| `50-literal-args-r8-r9` | literales `int` en `%r8`/`%r9` con un puntero de string en `%rcx` o `%r8`: el codificador conserva REX.R/REX.B en `movslq` | 42 | `65\r\n` |
| `51-c-bool-return` | tres predicados del runtime que devuelven `bool` de C en `al`: `starts_with`, `ends_with` y `contains` con respuestas `false` y `true` | 42 | `1\r\n` |
| `52-result-return` | funciones que devuelven `Result` `int`/`float`/`string` por puntero oculto, `try` con valor anotado, dos `catch e` y un quinto argumento por pila | 42 | `odd\r\nhi r6\r\nsum off\r\n` |
| `53-model-return` | un modelo devuelto como puntero: fábrica, identidad que muta su argumento, llamadas anidadas y retorno tras cinco argumentos | 42 | `wide\r\nmade\r\n` |
| `54-result-local` | locales `result` declarados, reasignados con `var` y devueltos como celdas de 24 bytes | 42 | `zero\r\nsmall\r\nbig\r\nnegative\r\n` |
| `55-union-parameter` | una unión de cuatro variantes como parámetro: `match` liga payloads `int` y `string`, brazo comodín y variante unitaria | 42 | `shape orbit\r\n` |
| `56-extern-result-list` | externs del runtime que devuelven `result` (puntero oculto antes de la arena) y `list`: lectura fallida, bytes aleatorios y directorio vacío | 42 | `read failed\r\n0\r\n` |
| `57-bare-return` | funciones `int`, `float`, `string` y `bool` que caen al final: el `ret` sin operando devuelve `0`, `0.0` o `false` | 42 | `one\r\n` |
| `58-result-spelling` | la grafía `Result` en retorno, local y registros es el tipo `result` | 42 | `neg\r\n` |
| `59-opaque-compare` | lecturas opacas (`elemKind 0`) comparadas con literales y registros `int`, con `-1` y `2147483647` en la lista | 42 | `3\r\n` |
| `60-int-neg-wrap` | `neg` de un `int` en 32 bits: `-INT32_MIN` vuelve a sí mismo y el literal `-2147483648` se compara con el desborde de `add` | 42 | `-2147483648\r\n` |
| `61-union-pack-arm` | brazos de `match` con varios payloads: la palabra de datos es el puntero al paquete y cada `load_field <Union>_<Variant>._i` lee el tipo declarado (`int`, `bool`, `string`) | 42 | `pack\r\n` |
| `62-union-named-payload` | payloads que nombran un modelo o una unión: una palabra puntero en la palabra de datos y en el paquete | 42 | `named\r\n` |
| `63-union-return` | funciones que devuelven una unión: la celda vuelve en `rax`, se re-construye y se pasa | 42 | `shapes\r\n` |
| `64-model-word-fields` | campos de modelo que son un modelo o una unión: una palabra puntero que se construye, se lee y se reescribe | 42 | `ab\r\n` |
| `65-ref-lists` | listas de referencias (`elemKind 5`): modelos y uniones se agregan, leen, reemplazan y quitan como palabras puntero | 42 | `a\r\n` |
| `66-opaque-lists` | listas sin tipo (`elemKind 0`): se agregan y reemplazan `int`, `string` y listas como la palabra que copia `orbit_list_push`, y se leen según el destino | 42 | `second\r\n` |
| `67-null-words` | `NULL` es la palabra cero: termina una lista enlazada, se devuelve desde una función de modelo, y `eq`/`ne` entre modelos compara punteros | 42 | `null\r\n` |

Orbit no incluye emisor IR en el pin. Los primeros cinco ejemplos manuales
fueron aceptados por el usuario para IRv1; los casos 06-43 amplían la
validación del backend nativo usando semántica observada en el pin. Estado
medido: los veintiséis primeros pares, ya en el formato de la spec, pasan
diferencial en Windows x86-64 desde el 2026-10-15 con exit/stdout idénticos a
la línea base previa a la migración, el par 27 desde D-M1-18 (2026-10-16), los
pares 28–29 desde D-M1-19 (2026-10-16), los pares 30–31 desde D-M1-20
(2026-10-16), el par 32 desde D-M1-21 (2026-10-17), el par 33 desde D-M1-22
(2026-10-17), el par 34 desde D-M1-23 (2026-10-18) y el par 35 desde
D-M1-24/D-M1-25 (2026-10-18), el par 36 desde D-M1-26 (2026-10-19), el par
37 desde D-M1-27 (2026-10-19), el par 38 desde D-M1-28 (2026-10-19), el par
39 desde D-M1-29 (2026-10-19), el par 40 desde D-M1-30 (2026-10-19) y el par
41 desde D-M1-31 (2026-10-20), el par
42 desde D-M1-32 (2026-10-20) y el par 43 desde D-M1-33 (2026-10-20) (ver
`../../DECISIONS.md`
D-F0-06/D-F0-07/D-M1-07/D-M1-08/D-M1-09/D-M1-11/D-M1-12/D-M1-14/D-M1-18/
D-M1-19/D-M1-20/D-M1-21/D-M1-22/D-M1-23/D-M1-24/D-M1-25/D-M1-26/
D-M1-27/D-M1-28/D-M1-29/D-M1-30/D-M1-31/D-M1-32/D-M1-33 y
`../../M1_REPORT.md`). El par 26 se retiró del diferencial en D-F0-13 (el pin
`17ed624` rechaza un extern como valor); el resto de los pares sigue medido.

La familia `map_*` queda cerrada con evidencia y sin fixture propio (D-M1-10):
`map_create`/`map_set`/`map_get` no tienen productor en el builder (spec
§1.1); los lowerings de `map_create` y `map_keys` llaman al runtime con
aridad incorrecta contra `runtime/orbit_api.h`; el `map_delete` producido
escribe un registro destino que nunca se declara (tipo `void`); y todo valor
mapa vivo depende de la lista de `arena` prepend, que es el gap U-04 y el
loader debe rechazar (spec §6). Los seis opcodes quedan pineados como
rechazos `E_UNSUPPORTED` en `tests/reject/` (casos `x-map-*`, regla D-M1-10).

La familia de modelos queda admitida solo dentro del subconjunto medido
(D-M1-11): constructor de modelo como el que emite el builder (`call` con
target `String`, aridad = nº de campos, valores en orden de declaración) y
`load_field`/`store_field` sobre campos `int`/`bool`/`float`/`string` de un
registro receptor cuyo tipo declara el modelo, con la referencia resuelta en
ese modelo (bare) o prefijada `Owner.campo` exigiendo coincidencia. Queda
fuera y falla cerrado (CGEN, o `E_DUPLICATE`/`E_SCHEMA` en el loader): valores
modelo en fronteras de función (retorno, parámetros, `arg`), acceso a campos
sobre receptores no-modelo (rutas string/JSON/object incluidas), campos de
tipo colección, aridad o target de constructor incorrectos, nombres de modelo
inválidos o duplicados —también la colisión con una función— y
`types`/`migrations`. El borde está pineado en `tests/reject/` (casos `x-*`
regla D-M1-11 y el control `ok-model-ctor`).

La familia de uniones queda admitida solo dentro del subconjunto medido
(D-M1-12): construcción de variante como `call` con target `String` (aridad =
nº de payloads, destino `void*`), donde unit y single-payload usan el data
word y multi-payload (≤ 2 partes `int`/`bool`/`string`) pasa por el struct
pack `<Union>_<Variante>`; lectura de tag por ordinal (`union_get_tag`) y de
payload (`union_get_data` + `load_field` con refs `<Union>_<Variante>._i`); y
constantes `<Union>_TAG_<Variante>` cargadas como símbolo. Queda fuera y falla
cerrado (CGEN, o `E_DUPLICATE`/`E_SCHEMA` en el loader): `union_create`,
`switch_op` y `type_info` (`E_UNSUPPORTED`, sin productor en el builder),
payloads float o de tipo nombrado, aridad de payload > 2, kinds distintos de
`union_type`, tags no declarados, destinos o fuentes fuera del contrato de
tipos, refs de pack inexistentes o sobre variantes sin pack, aridad de
constructor, valores `void*` en fronteras de función, y colisiones o
longitudes de `variants`/`richVariants` inconsistentes. El borde está pineado
en `tests/reject/` (casos `x-union-*` regla D-M1-12 y el control
`ok-union-ctor`).

Desde D-M1-35 una `enumeration` también se admite. Sus variantes no llevan
payload y su valor es el índice de declaración, así que el tipo se reescribe a
`int` en todo uso y `Name_TAG_Variant` carga ese índice. El borde queda
pineado por `x-enum-payload` (una variante con payload), `x-enum-undeclared-tag`
(una constante que no nombra una variante), `x-union-kind-sum` (el gate de
kind), el control `ok-enum-kind-unit` y el par 44.

La familia `alloc`/`free` queda cerrada con evidencia y sin fixture propio
(D-M1-13): ninguno tiene productor en el builder (spec §1.1), la spec deja
`free` como `UNSPECIFIED` (U-09: el no-op observado no es semántica, el
lowering es solo el comentario `/* free - arena managed */`) y el resultado de
`alloc` (`D ← orbit_arena_alloc(arena, O1)`; `sizeof(void*)` sin operando) es
un puntero crudo sin consumidor en el subconjunto admitido. Ambos quedan
pineados como rechazos `E_UNSUPPORTED` en `tests/reject/` (casos
`x-alloc-opcode` y `x-free-opcode`, regla D-M1-13).

La familia de valores de función queda admitida solo dentro del subconjunto
medido (D-M1-14): un nombre de función en posición de valor se serializa como
símbolo (`buildCallArgValue`, `builder.orb:1235`), `val`/`var f = inc` lo liga
con `decl_var` (+Symbol) y la llamada indirecta consume `arg` pendientes con
la aridad en `operand2` (`buildFnVarCall`, `builder.orb:3646`), chequeada
contra el spelling `fn(p1;...;pn)->ret` del registro callee
(`fnTypeSpellingFor`, `builder.orb:2202`). Solo se admiten spellings escalares
bien formadas (`int`/`bool`/`float`/`string`, cero params `fn()->ret`), el
binding de cualquier función del módulo —usuario o extern— salvo `main`
(`sema.orb:3365`), y el rebinding vía registro. Queda fuera y falla cerrado
(CGEN): símbolos no declarados, `main`, spellings void/no escalares, tipos de
registro `fn(...)` malformados, `store_var` con Symbol, retorno de fn,
llamada por registro no-`fn`, aridad distinta del spelling y desajustes de
tipo de argumento o destino; consumir más `arg` que los pendientes es V-13
(`E_SCHEMA`). El caso externo está medido (`orbit_string_at` como valor,
subtest `extern_value`; `ok-fn-extern`). La concatenación de strings en
callees queda bajo la frontera D-M1-09. El borde está pineado en
`tests/reject/` (casos `x-fn-*` regla D-M1-14 y controles
`ok-fn-value`/`ok-fn-extern`).

Los globales con valor no literal quedan cerrados con evidencia (D-M1-15): el
builder contiene la ruta que acuñaría `IRGlobal(name, ir_register(...))` para
un `const` de nivel superior no literal (`builder.orb:2377`), pero la sema
rechaza antes toda declaración de archivo no literal (`E1007`,
`sema.orb:1824-1831`; `isTopLevelConstantExpr`, `sema.orb:1840`), así que
ningún programa del corpus puede producirla; el rechazo del loader
(`global ... is outside M1 literal int/bool support`) queda pineado como
`x-global-register`.

Los bordes E-03 (`list_get` con `elemKind 5` y tipo `unknown`), E-04
(`elemKind 0` en `list_create`) y E-06 (`print` con 2 argumentos pendientes)
quedan cerrados como rechazos con evidencia en D-M1-16 (casos
`x-list-get-kind-ref`, `x-list-create-kind-opaque` y `x-print-two-args`): sin
accesor untyped/ref dentro del modelo de registros y sin productor del print
multicarácter (la sema del pin lo rechaza con E1003; el lowering es solo el
comentario `/* print */`, U-17).

La resolución del target de `call` sigue la spec §3.1 paso 2 dentro del
subconjunto medido (D-M1-18): `operand1` `String`/`Sym` es una llamada
nombrada, resuelta en el orden función declarada → constructor de modelo
(D-M1-11) → variante de unión (D-M1-12), que es el formulario que emite el
builder (`String` para toda llamada directa; el corpus emitido no contiene un
solo `Sym` como target). Un `Sym` solo puede ser llamada a función. Además, un
callee void puede llevar destino declarado `void` (paso 7: el destino no es
necesario, no está prohibido; el builder siempre lo asigna y lo tipa `void`).
Queda fuera y falla cerrado (CGEN): un `String` que no nombra función, modelo
ni variante, aridad distinta de la declaración, tipo de argumento o destino
incompatible, destino no `void` en llamada void, y `main` como target. Los
nombres runtime/builtin sin declaración (`orbit_string_*`, `orbit_object_*`,
`orbit_db_*`, `print`, `system_*`) son el frente de builtins/alias (§3.1 pasos
4-6) y siguen rechazados. El borde está pineado en `tests/reject/` (casos
`x-call-*` regla D-M1-18 y el control `ok-call-void-dest`).

Los literales del builder quedan admitidos en sus dos formas medidas
(D-M1-19): un `Sym` en `load_const` es el literal bool (`true`/`false`) o
float (texto numérico con fracción o exponente, finito) según su gramática de
texto, y solo si no es ninguna de las dos sigue siendo un valor de función
(D-M1-14); y `decl_var`/`store_var` aceptan un inicializador literal inline
(`val n = 7`, `val s = "x"`, `val b = true`: 299 sitios en 46 documentos), que
antes se bajaba como una copia de registro equivocada. Queda fuera y falla
cerrado (CGEN): un literal cuyo tipo no coincide con el registro destino, un
texto numérico no finito (V-08), un entero desnudo como `Sym` (no es un float
literal) y un local declarado dos veces con tipos distintos. El censo de
ambas formas está en D-M1-19; los pares 28–29 y los controles
`ok-sym-bool-literal`/`ok-sym-float-literal`/`ok-decl-var-literal` pinean el
borde.

El **operando literal dentro de una operación** queda admitido en su forma
medida (D-M1-21): los once opcodes `add`/`sub`/`mul`/`div`/`mod`/`eq`/`ne`/`lt`/
`le`/`gt`/`ge` aceptan un literal inline (`Int`/`String`/`Bool`; 2661 sitios en
79/94 documentos, **ningún `Float`**) y R6 lo materializa en un slot de
inmediatos reservado en el frame, con la regla de tipos por opcode intacta. Un
`Float` inline, un literal fuera de esos once y un tipo que no encaja siguen
`CGEN`. El par 32 (stream propio del emisor, incluida la concatenación de
strings) y el control `ok-lit-operands` pinean el borde; quedan reportados como
frentes propios el *string spreading* de `add` (`String` + `Int`, sin regla de
conversión en R6) y `ne` sobre bool.

Los tipos de registro que el builder serializa como `unknown` se recuperan
dentro del subconjunto nativo (D-M1-20): antes de validar el módulo, una pasada
de tres puntos fijos espeja las reglas de `inferRegisterTypes`
(`c_backend.orb:562`) para los opcodes admitidos -`load_const`/`load_var`/
aritmética/`neg`/comparaciones y lógicos/`copy`/`arg`/`call`/`load_field`/
`list_create`/`list_len`/`list_get`/`map_keys`/`union_get_tag`- y **solo**
sustituye el texto `unknown`, nunca un tipo declarado. Dos estrechamientos
deliberados: la aritmética no adivina (infiere `float` si algún operando lo es,
`int` si ambos lo son, y deja `unknown` si no coinciden) y lo que el backend
nativo no representa deja el registro `unknown` (tags de unión sin declarar,
`result_unwrap`, `union_get_data`, `map`, targets de `call` no declarados).
D-M1-35 saca de esa lista la constante `Name_TAG_Variant` de una enumeración:
se infiere `int`, igual que la de una unión.
Cierra el hueco 1 de D-M1-17 y está medido en detalle en D-M1-20 (6735
registros en 84/90 documentos; 5941 recuperados, 40 documentos sin `unknown`).
Los pares 30–31 son el stream del emisor **verbatim, con sus `unknown`**, y el
borde queda pineado en `tests/reject/` (casos `x-infer-*` regla D-M1-20 y
controles `ok-infer-*`).
