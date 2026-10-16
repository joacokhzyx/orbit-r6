# Fixtures M1 manuales IRv1 (diferencial contra el pin)

Cada `.json` usa el formato IRv1 del lado lenguaje
(`orbit-lang/docs/IR_SPEC.md`, blob `110ce439be8969d0e97986ad5eaa0cf0db92b1af`):
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

Orbit no incluye emisor IR en el pin. Los primeros cinco ejemplos manuales
fueron aceptados por el usuario para IRv1; los casos 06–25 amplían la
validación del backend nativo usando semántica observada en el pin. Estado
medido al 2026-10-15: los veinticinco pares, ya en el formato de la spec,
pasan diferencial en Windows x86-64 con exit/stdout idénticos a la línea base
previa a la migración (ver `../../DECISIONS.md`
D-F0-06/D-F0-07/D-M1-07/D-M1-08/D-M1-09/D-M1-11/D-M1-12 y `../../M1_REPORT.md`).

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
