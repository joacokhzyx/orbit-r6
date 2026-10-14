# Fixtures de propuesta IRv1 para revisión (no aprobados ni ejecutables)

Cada `.json` usa la gramática candidata de `docs/IR_SPEC_DRAFT.md`; cada
`.orb` es el fuente que debería producir comportamiento equivalente en el
oracle gcc. Outputs/exit codes son expectativas de revisión, no resultados
medidos. `orbit_print` se invoca como símbolo runtime declarado por la ABI;
debe confirmarse que Orbit permite esta llamada externa y su firma.

| Caso | Familia | Exit esperado | Stdout esperado |
|---|---|---:|---|
| `01-int-call` | suma entera y llamada runtime | 42 | `M1-add\n` |
| `02-string` | concat de strings y llamada runtime | 0 | `IRv1\n` |
| `03-float-branch` | suma/igualdad binary64 y branch | 0 | vacío |
| `04-branch` | salto condicional | 7 | vacío |
| `05-call-3args` | parámetros y llamada de tres argumentos | 6 | vacío |

Para convertirlos en M1 fixtures habrá que obtener IR canónico del emisor
aprobado, cotejarlo con estos ejemplos, y medir los cinco diferenciales. No se
deben aceptar los JSON propuestos como evidencia de implementación.
