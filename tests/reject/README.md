# Loader contract suite

The second gate (decision D-M1-06). Every case in `manifest.json` names the
decision it pins in its `rule` field; the decisions are in
[`../../docs/DECISIONS.md`](../../docs/DECISIONS.md). The suite is 454 cases:
311 rejections and 141 acceptance controls, measured on 2026-10-31 on Windows
x64 and Linux x64.

```powershell
python tests/reject/run_reject.py            # 311 rejections + 141 acceptance controls
python tests/reject/run_reject.py --case v13 # one case (stem substring)
python tests/reject/make_cases.py --check    # cases/ and manifest.json drift check
```

```bash
python3 tests/reject/run_reject.py
python3 tests/reject/run_reject.py --case v13
python3 tests/reject/make_cases.py --check
```

- `manifest.json` is the suite truth: file, expected exit, expected diagnostic
  code and spec rule. `make_cases.py` renders it and `cases/` deterministically.
  The byte-exact cases (BOM, final LF, invalid UTF-8, elision marker, `1e3`,
  `1e999`, duplicate key) do not survive an editor and are regenerated there,
  never edited by hand.
- `cases/` holds the local rejections and controls. The remaining acceptance
  controls are fixtures in `tests/diff/fixtures/`: `01-int-call`,
  `27-call-string-target`, `28-sym-literals`, `29-decl-var-literals`,
  `30-inferred-model-fields`, `31-inferred-unary-neg`,
  `32-inline-literal-operands` and `33-parameter-by-name`.
- `run_reject.py` runs the driver with the repository root as CWD, because the
  runtime archive path is resolved relative to the CWD. Every rejection must
  exit 1 with exactly one `r6: <file>:<line>:<col>: error[CODE]: <msg>` line
  carrying the manifest code. Acceptance controls build, and run with
  byte-compared stdout when the manifest declares `run_exit` or `run_stdout`.
- On Linux the runner defaults to `.r6-build/r6` and
  `.r6-build/runtime-linux-x64`, normalizes CRLF to LF when it compares run
  stdout, and compares exit codes modulo 256.

## Coverage by rule

The case names are stems in `cases/` unless a line says otherwise.

| Rule | Cases |
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
| V-13 (E-05) | `v13-arity-exceeds-pending`, `v13-arity-negative`, `v13-arity-not-int`, `x-fn-arity-pending`; control `ok-call-arity-suffix` |
| V-14 (E-08) | `v14-jump-undefined-label`, `v14-jump-target-not-label` |
| V-15 | `v15-model-typeparams` |
| V-16 (E-10) | `v16-route-path-without-method`, `v16-route-limits-without-method` |
| V-17 (E-11) | `v17-type-methods` |
| V-18 (E-09) | control `ok-extern-body` |
| V-19 (E-12) | `v19-db-get` |
| R6 subset | `x-unsupported-opcode`, `x-route-function` |
| R6 strictness | `x-duplicate-key`, `x-duplicate-label` |
| E-07 | control `ok-untargeted-label` |
| D-M1-10 (maps) | `x-map-create`, `x-map-set`, `x-map-get`, `x-map-delete`, `x-map-has`, `x-map-keys` |

The manifest is the complete list; this table only names the rule that each
family pins. A new admission adds a rejection and an acceptance control in the
same change.
