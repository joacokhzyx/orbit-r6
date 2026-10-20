# tests/reject — suite del contrato del loader (segundo gate)

Fecha: 2026-10-15. Decisión: `DECISIONS.md` D-M1-06. Contrato: formato IRv1 del
lenguaje (`orbit-lang/docs/IR_SPEC.md`, blob `371f873e…`), reglas V-01..V-19 y
casos límite E-01..E-12.

```powershell
python tests/reject/run_reject.py            # 116 rechazos + 30 controles
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
- `cases/` - 116 rechazos + 22 controles locales; los otros ocho controles
  son `docs/proposed-ir/m1/01-int-call.json`,
  `docs/proposed-ir/m1/27-call-string-target.json`,
  `docs/proposed-ir/m1/28-sym-literals.json`,
  `docs/proposed-ir/m1/29-decl-var-literals.json`,
  `docs/proposed-ir/m1/30-inferred-model-fields.json`,
  `docs/proposed-ir/m1/31-inferred-unary-neg.json`,
  `docs/proposed-ir/m1/32-inline-literal-operands.json` y
  `docs/proposed-ir/m1/33-parameter-by-name.json`.
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
| V-13 (E-05) | `v13-arity-exceeds-pending`, `v13-arity-negative`, `v13-arity-not-int`, `x-fn-arity-pending`; aceptación: `ok-call-arity-suffix` |
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
| D-M1-12 | `x-union-create-opcode`, `x-union-switch-op`, `x-union-type-info`, `x-union-payload-float`, `x-union-payload-arity`, `x-union-kind-enumeration`, `x-union-get-tag-dest-bool`, `x-union-get-tag-src-int`, `x-union-tag-unknown`, `x-union-pack-missing-field`, `x-union-pack-single`, `x-union-ctor-arity`, `x-union-duplicate-variant`, `x-union-variant-two-types`, `x-union-tag-collision`, `x-union-rich-lengths`; aceptación: `ok-union-ctor` |
| D-M1-13 | `x-alloc-opcode`, `x-free-opcode` |
| D-M1-14 | `x-fn-value-unknown`, `x-fn-value-main`, `x-fn-value-void-signature`, `x-fn-value-dest-type`, `x-fn-callee-not-fn`, `x-fn-arity-signature`, `x-fn-arg-type`, `x-fn-dest-mismatch`, `x-fn-store-symbol`, `x-fn-return-boundary`, `x-fn-type-spelling`, `x-fn-type-nonscalar`; aceptación: `ok-fn-value`, `ok-fn-extern` |
| D-M1-15 | `x-global-register` |
| D-M1-16 | `x-list-create-kind-opaque`, `x-list-get-kind-ref`, `x-print-two-args` |
| D-M1-18 | `x-call-string-target-arity`, `x-call-string-target-arg-type`, `x-call-string-target-dest`, `x-call-string-target-main`, `x-call-void-dest-type`; aceptación: `ok-call-void-dest` |
| D-M1-19 | `x-sym-literal-float-dest-type`, `x-sym-literal-bool-dest-type`, `x-sym-literal-not-finite`, `x-sym-integer-text-is-a-symbol`, `x-decl-var-literal-type`; aceptación: `ok-sym-bool-literal`, `ok-sym-float-literal`, `ok-decl-var-literal` (+ los fixtures 28/29) |
| D-M1-20 | `x-infer-arith-unknown-operand`, `x-infer-unresolved-call`, `x-infer-unknown-load-var`, `x-infer-result-unwrap`; aceptación: `ok-infer-eq-bool`, `ok-infer-load-var`, `ok-infer-neg-chain`, `ok-infer-call-return`, `ok-infer-arg-param`, `ok-infer-list-create`, `ok-infer-union-tag` (+ los fixtures 30/31) |
| D-M1-21 | `x-lit-float-operand`, `x-lit-and-op-operand`, `x-lit-type-mismatch`; aceptación: `ok-lit-operands` (+ el fixture 32) |
| D-M1-22 | `x-load-var-foreign-param`, `x-load-var-param-mismatch`; aceptación: `ok-load-var-param` (+ el fixture 33) |
| D-M1-23 | `x-add-spread-dest`, `x-add-spread-pointer`, `x-add-spread-float-literal`, `x-add-string-dest`; aceptación: `ok-add-string-spread` (+ el fixture 34) |
| D-M1-25 | aceptación: `35-list-len-bound` (el fixture del bucle con bound de `list_len` en una función no-`main`) |

E-03 (`list_get` con `elemKind 5` y tipo `unknown`), E-04 (`elemKind 0` en
`list_create`) y E-06 (`print` con 2 argumentos pendientes) quedan cerrados
como rechazos con evidencia en D-M1-16 (casos `x-list-get-kind-ref`,
`x-list-create-kind-opaque`, `x-print-two-args`): los kinds 0/5 no tienen tipo
de acceso dentro del modelo de registros (D-M1-09) y el print multicarácter no
tiene productor —la sema del pin lo rechaza con E1003— ni lowering con
semántica (solo el comentario `/* print */`; U-17).

Estado 2026-10-19: **171/171** (129 rechazos + 42 controles); gate diferencial
39/39 en el mismo árbol. D-M1-26/D-M1-27 quedan pineados por
`x-builtin-name-outside-print`, `x-print-symbol-target` y
`x-arena-extern-undeclared` (rechazos), más los controles `ok-print-*`,
`ok-arena-*`, `36-print-builtin` y `37-arena-extern-call`; D-M1-28 por
`x-ret-type-mismatch`, `ok-void-ret-value` y `38-void-ret-value`; D-M1-29 por
`x-intrinsic-symbol-target`, `x-intrinsic-arity`, `x-list-len-bool`,
`ok-list-len-string` y `39-string-intrinsics`. El borde de D-M1-21 (operando literal inline en las
operaciones) queda pineado por los rechazos `x-lit-float-operand`,
`x-lit-and-op-operand`, `x-lit-type-mismatch` y los controles
`ok-lit-operands` y `32-inline-literal-operands`. El de D-M1-22 (`load_var` de
un parámetro) queda pineado por `x-load-var-foreign-param` (un parámetro de
otra función no es un nombre del módulo), `x-load-var-param-mismatch` (el tipo
del destino debe ser `paramTypes[p]`) y los controles `ok-load-var-param` y
`33-parameter-by-name`; `x-infer-unknown-load-var` sigue cubriendo el nombre
sin declarar.

El borde de D-M1-23 (`add` con un `string` en un solo lado) queda pineado por
`x-add-spread-dest` (destino que no es `string`), `x-add-spread-pointer` (el lado
escalar es un puntero, sin conversor medido), `x-add-spread-float-literal` (el
literal `Float` de D-M1-21 no se dispersa) y `x-add-string-dest` (un `add` de
strings debe producir un `string`), con el control de aceptación
`ok-add-string-spread` y el fixture `34-string-spreading-add`.

D-M1-24 y D-M1-25 no añaden rechazos: son dos defectos preexistentes del
lowering que la ronda destapó, y no cambian ninguna regla de validación. Su pin
es el control de aceptación `35-list-len-bound` (bucle indexado sobre una lista en
una función distinta de `main`: la etiqueta de la lectura de lista lleva el
ordinal de función y la longitud de `list_len` ocupa el slot `int` completo). Por
sí solos no mueven el corpus (23 -> 23, medido); la ronda sube a 27/93 con
D-M1-23.
