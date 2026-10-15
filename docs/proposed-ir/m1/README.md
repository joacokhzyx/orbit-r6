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

Orbit no incluye emisor IR en el pin. Los primeros cinco ejemplos manuales
fueron aceptados por el usuario para IRv1; los casos 06–22 amplían la
validación del backend nativo usando semántica observada en el pin. Estado
medido al 2026-10-15: los veintidós pares, ya en el formato de la spec, pasan
diferencial en Windows x86-64 con exit/stdout idénticos a la línea base previa
a la migración (ver `../../DECISIONS.md` D-F0-06/D-F0-07/D-M1-07/D-M1-08 y
`../../M1_REPORT.md`).
