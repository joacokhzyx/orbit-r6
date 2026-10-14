# Fixtures M1 manuales IRv1 (diferencial contra el pin)

Cada `.json` usa la gramática aceptada de `docs/IR_SPEC_DRAFT.md`; cada `.orb`
es el fuente de comportamiento equivalente para el oracle gcc. Outputs/exit
codes se comparan como exit status/stdout bytes con `orbit build` del pin.
`orbit_print` se invoca como símbolo runtime declarado por ABI C en
`runtime/orbit_api.h`.

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

Orbit no incluye emisor IR en el pin. Los primeros cinco ejemplos manuales
fueron aceptados por el usuario para IRv1; los casos 06–10 amplían la
validación del backend nativo usando semántica observada en el pin. Estado
medido al 2026-10-14: diez de diez pares pasan diferencial en Windows x86-64.
