# Fixtures M1 manuales IRv1 (aceptados; diferencial pendiente)

Cada `.json` usa la gramática aceptada de `docs/IR_SPEC_DRAFT.md`; cada `.orb`
es el fuente de comportamiento equivalente para el oracle gcc. Outputs/exit
codes son expectativas aún no medidas. `orbit_print` se invoca como símbolo
runtime declarado por ABI C en `runtime/orbit_api.h`.

| Caso | Familia | Exit esperado | Stdout esperado |
|---|---|---:|---|
| `01-int-call` | suma entera y llamada runtime | 42 | `M1-add\n` |
| `02-string` | concat de strings y llamada runtime | 0 | `IRv1\n` |
| `03-float-branch` | suma/igualdad binary64 y branch | 0 | vacío |
| `04-branch` | salto condicional | 7 | vacío |
| `05-call-3args` | parámetros y llamada de tres argumentos | 6 | vacío |

Orbit no incluye emisor IR en el pin. Estos cinco ejemplos manuales fueron
aceptados por el usuario para IRv1; cada uno debe superar differential contra
su fuente `.orb` y el binario gcc de referencia para contar como evidencia.
