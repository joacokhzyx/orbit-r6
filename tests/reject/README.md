# tests/reject — suite del contrato del loader (segundo gate)

Fecha: 2026-10-15. Decisión: `DECISIONS.md` D-M1-06. Contrato: formato IRv1 del
lenguaje (`orbit-lang/docs/IR_SPEC.md`, blob `110ce439…`), reglas V-01..V-19 y
casos límite E-01..E-12.

```powershell
python tests/reject/run_reject.py            # 64 rechazos + 5 controles
python tests/reject/run_reject.py --case v13 # un caso (substring del stem)
python tests/reject/make_cases.py --check    # drift de cases/ + manifest.json
```

- `make_cases.py` — renderiza `cases/` y `manifest.json` de forma
  determinista; cada rechazo es un delta de un campo sobre un documento
  válido. Los casos byte-exactos (BOM, LF final, UTF-8 inválido, marcador de
  elisión, `1e3`, `1e999`, clave duplicada) no sobreviven a un editor: se
  regeneran aquí, no a mano.
- `manifest.json` — verdad de la suite: archivo, exit esperado, código de
  diagnóstico esperado y regla de la spec. Generado; no editar a mano.
- `cases/` — 64 rechazos + 4 controles locales; el quinto control es
  `docs/proposed-ir/m1/01-int-call.json`.
- `run_reject.py` — ejecuta el driver con la raíz del repo como CWD (el
  runtime se resuelve relativo al CWD) y exige: exit 1, exactamente una línea
  `r6: <file>:<line>:<col>: error[CODE]: <msg>` y el `CODE` del manifiesto.
  Los controles positivos compilan y, cuando el manifiesto declara `run_exit`
  (y opcionalmente `run_stdout`), corren con stdout comparado byte a byte.

## Cobertura por regla

| Regla | Casos |
|---|---|
| V-01 | `v01-no-trailing-lf`, `v01-bom`, `v01-elision-marker`, `v01-invalid-utf8`, `v01-malformed-json` |
| V-02 | `v02-missing-module`, `v02-extra-root-key`, `v02-irversion-number`, `v02-irversion-2`, `v02-draft-format` |
| V-03 | `v03-unknown-opcode`, `v03-case-folded-opcode` |
| V-04 | `v04-elemkind-on-add`, `v04-elemkind-out-of-range` |
| V-05 | `v05-missing-kind`, `v05-unknown-kind`, `v05-none-with-payload`, `v05-extra-key`, `v05-string-wrong-type`, `v05-missing-value` |
| V-06 | `v06-int-fraction`, `v06-int-exponent`, `v06-int-overflow` |
| V-07/V-09 (E-02) | `v07-dest-below-minus-one`, `v07-register-negative` |
| V-08 | `v08-float-overflow` |
| V-10 (E-01) | `v10-dest-minus-one-load-const`, `v10-call-nonvoid-no-dest` |
| V-11 | `v11-dest-out-of-range`, `v11-register-out-of-range`, `v11-registertypes-length` |
| V-12 | `v12-paramtypes-length`, `v12-arg-index-out-of-range` |
| V-13 (E-05) | `v13-arity-exceeds-pending`, `v13-arity-negative`, `v13-arity-not-int`; aceptación: `ok-call-arity-suffix` |
| V-14 (E-08) | `v14-jump-undefined-label`, `v14-jump-target-not-label` |
| V-15 | `v15-model-typeparams` |
| V-16 (E-10) | `v16-route-path-without-method`, `v16-route-limits-without-method` |
| V-17 (E-11) | `v17-type-methods` |
| V-18 (E-09) | aceptación: `ok-extern-body` |
| V-19 (E-12) | `v19-db-get` |
| Subconjunto R6 | `x-unsupported-opcode`, `x-route-function` |
| Estrictez R6 | `x-duplicate-key`, `x-duplicate-label` |
| E-07 | aceptación: `ok-untargeted-label` |
| D-M1-10 | `x-map-create`, `x-map-set`, `x-map-get`, `x-map-delete`, `x-map-has`, `x-map-keys` |
| D-M1-11 | `x-load-field-nonmodel-base`, `x-field-missing-field`, `x-field-owner-mismatch`, `x-load-field-dest-mismatch`, `x-store-field-with-dest`, `x-ctor-arity-mismatch`, `x-model-field-unsupported-type`, `x-field-ref-kind`, `x-ctor-unknown-model`, `x-duplicate-model-name`, `x-model-function-collision`; aceptación: `ok-model-ctor` |

E-03 (`list_get` con `elemKind 5` y tipo `unknown`), E-04 (`elemKind 0` en
`list_create`) y E-06 (`print` con 2 argumentos pendientes) quedan para la fase
C: R6 los rechaza en codegen por su subconjunto (`registerTypes` estricta,
alias de runtime fuera del subconjunto) y exigen decisión D-M1-xx con
evidencia, no un caso de aceptación.

Estado 2026-10-15: **69/69** (64 rechazos + 5 controles); gate diferencial
24/24 en el mismo árbol.
