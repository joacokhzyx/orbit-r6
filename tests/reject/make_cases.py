#!/usr/bin/env python3
"""Deterministic builder for the tests/reject loader suite (fase B).

Every rejection case is a one-field delta from a valid IRv1 document, so each
one fails on exactly the rule it cites. Byte-level cases (BOM, missing
trailing LF, invalid UTF-8, elision marker, exponent forms, duplicate keys) do
not survive an editor round-trip, so they are assembled here; the committed
files under cases/ and manifest.json are the artifacts the runner consumes.
`--check` re-renders everything in memory and compares it with the tree.

Run from anywhere: python tests/reject/make_cases.py [--check]
"""

import argparse
import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
HERE = Path(__file__).resolve().parent
CASES = HERE / "cases"

NONE = {"kind": "None"}


def Int(n):
    return {"kind": "Int", "value": n}


def Flt(v):
    return {"kind": "Float", "value": v}


def Reg(n):
    return {"kind": "Register", "value": n}


def Lab(n):
    return {"kind": "Label", "value": n}


def Sym(s):
    return {"kind": "Sym", "value": s}


def Types(n):
    return ["int"] * n


def I(op, dest=-1, o1=None, o2=None, o3=None, ek=0):
    return {"opcode": op, "dest": dest,
            "operand1": NONE if o1 is None else o1,
            "operand2": NONE if o2 is None else o2,
            "operand3": NONE if o3 is None else o3,
            "elemKind": ek}


def FN(name, instrs, types, rtype="int", extern=False, params=None, ptypes=None, route=None):
    r = route or {}
    return {"name": name, "params": params or [], "paramTypes": ptypes or [],
            "instructions": instrs,
            "registerCount": len(types), "registerTypes": list(types),
            "returnType": rtype, "isExtern": extern,
            "routeMethod": r.get("method", ""), "routePath": r.get("path", ""),
            "routeHasLimit": r.get("has_limit", False),
            "routeLimitRate": r.get("limit_rate", 0),
            "routeLimitWindowMs": r.get("limit_window_ms", 0),
            "routeLimitBurst": r.get("limit_burst", 0)}


def MAIN(instrs=None, types=None, route=None, **kw):
    if instrs is None:
        instrs = [I("load_const", 0, Int(0)), I("ret", -1, Reg(0))]
    return FN("main", instrs, ["int"] if types is None else types, route=route, **kw)


def MOD(fns):
    return {"functions": fns, "globals": [], "models": [], "types": [], "migrations": []}


def DOC(fns=None):
    return {"irVersion": "1", "module": MOD(fns if fns is not None else [MAIN()])}


def ZERO():
    return FN("zero", [I("ret")], [], rtype="void")


def jd(obj):
    return (json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n").encode("utf-8")


def base_instrs():
    return [I("load_const", 0, Int(0)), I("ret", -1, Reg(0))]


def build_reject():
    cases = []

    def add(stem, data, rule, code, note):
        cases.append({"stem": stem, "data": data, "rule": rule, "code": code, "note": note})

    # ---- V-01: JSON envelope -------------------------------------------------
    add("v01-no-trailing-lf", jd(DOC())[:-1], "V-01", "E_JSON",
        "document must end with LF")
    add("v01-bom", b"\xef\xbb\xbf" + jd(DOC()), "V-01", "E_JSON",
        "UTF-8 BOM is not allowed")
    elision = DOC()
    elision["module"]["migrations"] = ["…[42 chars]"]
    add("v01-elision-marker", jd(elision), "V-01", "E_JSON",
        "authoring-tool elision marker")
    bad_utf8 = DOC()
    bad_utf8["module"]["migrations"] = ["é"]
    add("v01-invalid-utf8", jd(bad_utf8).replace(b"\xc3\xa9", b"\xff"),
        "V-01", "E_UTF8", "string must be valid UTF-8")
    add("v01-malformed-json", b'{"irVersion":"1",}\n', "V-01", "E_JSON",
        "JSON syntax error")

    # ---- V-02: root object ---------------------------------------------------
    add("v02-missing-module", jd({"irVersion": "1"}), "V-02", "E_SCHEMA",
        "root has exactly irVersion + module")
    extra = DOC()
    extra["extra"] = 1
    add("v02-extra-root-key", jd(extra), "V-02", "E_SCHEMA", "unknown root key")
    num = DOC()
    num["irVersion"] = 1
    add("v02-irversion-number", jd(num), "V-02", "E_VERSION",
        "irVersion must be the string \"1\"")
    two = DOC()
    two["irVersion"] = "2"
    add("v02-irversion-2", jd(two), "V-02", "E_VERSION", "unsupported version")
    draft = {"format": "orbit-ir", "version": 1, "module": MOD([MAIN()])}
    add("v02-draft-format", jd(draft), "V-02", "E_FORMAT",
        "superseded R6 draft serialization")

    # ---- V-03: opcode names --------------------------------------------------
    add("v03-unknown-opcode",
        jd(DOC([MAIN([I("load_constant", 0, Int(0)), I("ret", -1, Reg(0))])])),
        "V-03", "E_OPCODE", "opcode must be one of the 60 names")
    add("v03-case-folded-opcode",
        jd(DOC([MAIN([I("LOAD_CONST", 0, Int(0)), I("ret", -1, Reg(0))])])),
        "V-03", "E_OPCODE", "no case folding or aliases")

    # ---- V-04: elemKind ------------------------------------------------------
    add("v04-elemkind-on-add",
        jd(DOC([MAIN([I("load_const", 0, Int(1)), I("load_const", 1, Int(2)),
                      I("add", 2, Reg(0), Reg(1), ek=1), I("ret", -1, Reg(2))], Types(3))])),
        "V-04", "E_SCHEMA", "elemKind nonzero outside collection opcodes")
    add("v04-elemkind-out-of-range",
        jd(DOC([MAIN([I("list_create", 0, ek=6), I("ret", -1, Reg(0))])])),
        "V-04", "E_RANGE", "elemKind is an integer in 0..5")

    # ---- V-05: operand values ------------------------------------------------
    def with_operand2(op):
        ins = base_instrs()
        ins[0]["operand2"] = op
        return jd(DOC([MAIN(ins)]))

    add("v05-missing-kind", with_operand2({"value": 0}), "V-05", "E_SCHEMA",
        "kind is required")
    add("v05-unknown-kind", with_operand2({"kind": "Floating", "value": 1}),
        "V-05", "E_SCHEMA", "kind must be one of the 8")
    add("v05-none-with-payload", with_operand2({"kind": "None", "value": 0}),
        "V-05", "E_SCHEMA", "None carries no value")
    add("v05-extra-key", with_operand2({"kind": "Int", "value": 0, "extra": 1}),
        "V-05", "E_SCHEMA", "value objects have kind/value only")
    add("v05-string-wrong-type", with_operand2({"kind": "String", "value": 5}),
        "V-05", "E_SCHEMA", "value must match the kind")
    add("v05-missing-value", with_operand2({"kind": "Int"}), "V-05", "E_SCHEMA",
        "value is required for non-None kinds")

    # ---- V-06: integer values ------------------------------------------------
    ins = base_instrs()
    ins[0]["operand1"] = {"kind": "Int", "value": 1.5}
    add("v06-int-fraction", jd(DOC([MAIN(ins)])), "V-06", "E_SCHEMA",
        "no fraction in integer values")
    ins = base_instrs()
    ins[0]["operand1"] = Int(424242)
    add("v06-int-exponent", jd(DOC([MAIN(ins)])).replace(b"424242", b"1e3"),
        "V-06", "E_SCHEMA", "no exponent in integer values")
    ins = base_instrs()
    ins[0]["operand1"] = Int(2147483648)
    add("v06-int-overflow", jd(DOC([MAIN(ins)])), "V-06", "E_RANGE",
        "signed 32-bit limit")

    # ---- V-07/V-09: destinations and register ids ----------------------------
    add("v07-dest-below-minus-one",
        jd(DOC([MAIN([I("load_const", -2, Int(0)), I("ret", -1, Reg(0))])])),
        "V-07", "E_RANGE", "dest is -1 or a non-negative integer")
    ins = base_instrs()
    ins[0]["operand2"] = Reg(-1)
    add("v07-register-negative", jd(DOC([MAIN(ins)])), "V-07/V-09 (E-02)", "E_RANGE",
        "no negative register ids")

    # ---- V-08: floats --------------------------------------------------------
    ins = base_instrs()
    ins[0]["operand1"] = Flt(7.5625)
    add("v08-float-overflow", jd(DOC([MAIN(ins)])).replace(b"7.5625", b"1e999"),
        "V-08", "E_RANGE", "floats must be finite")

    # ---- V-10: required destinations -----------------------------------------
    add("v10-dest-minus-one-load-const",
        jd(DOC([MAIN([I("load_const", -1, Int(0)), I("ret", -1, Reg(0))])])),
        "V-10 (E-01)", "E_SCHEMA", "value-producing opcodes need dest")
    m_ins = [I("load_const", 0, Int(1)), I("load_const", 1, Int(2)),
             I("arg", -1, Reg(0)), I("arg", -1, Reg(1)),
             I("call", -1, Sym("add2"), Int(2)), I("ret", -1, Int(0))]
    add("v10-call-nonvoid-no-dest",
        jd(DOC([MAIN(m_ins, Types(3)),
                FN("add2", [], [], rtype="int", extern=True,
                   params=["a", "b"], ptypes=["int", "int"])])),
        "V-10", "E_SCHEMA", "non-void call needs dest")

    # ---- V-11: registerCount and registerTypes -------------------------------
    add("v11-dest-out-of-range",
        jd(DOC([MAIN([I("load_const", 1, Int(0)), I("ret", -1, Reg(0))])])),
        "V-11", "E_RANGE", "dest < registerCount")
    add("v11-register-out-of-range",
        jd(DOC([MAIN([I("load_const", 0, Int(0)), I("ret", -1, Reg(5))])])),
        "V-11", "E_RANGE", "Register(n) < registerCount")
    fn = MAIN()
    fn["registerCount"] = 2
    add("v11-registertypes-length", jd(DOC([fn])), "V-11", "E_SCHEMA",
        "registerCount == len(registerTypes)")

    # ---- V-12: params and arg bindings ---------------------------------------
    fn = MAIN()
    fn["params"] = ["a"]
    add("v12-paramtypes-length", jd(DOC([fn])), "V-12", "E_SCHEMA",
        "params and paramTypes have equal length")
    add("v12-arg-index-out-of-range",
        jd(DOC([FN("main", [I("arg", 0, Int(7)), I("ret", -1, Int(0))], ["int"],
                   params=["a"], ptypes=["int"])])),
        "V-12", "E_RANGE", "arg-with-dest index < param count")

    # ---- V-13: call arity ----------------------------------------------------
    add("v13-arity-exceeds-pending",
        jd(DOC([MAIN([I("call", -1, Sym("zero"), Int(1)), I("ret", -1, Int(0))]), ZERO()])),
        "V-13", "E_SCHEMA", "arity <= pending args")
    add("v13-arity-negative",
        jd(DOC([MAIN([I("call", -1, Sym("zero"), Int(-1)), I("ret", -1, Int(0))]), ZERO()])),
        "V-13", "E_RANGE", "arity >= 0")
    add("v13-arity-not-int",
        jd(DOC([MAIN([I("call", -1, Sym("zero")), I("ret", -1, Int(0))]), ZERO()])),
        "V-13", "E_SCHEMA", "arity is an Int")

    # ---- V-14: labels --------------------------------------------------------
    add("v14-jump-undefined-label", jd(DOC([MAIN([I("jump", -1, Lab(3))])])),
        "V-14 (E-08)", "E_SCHEMA", "jump target must exist")
    add("v14-jump-target-not-label", jd(DOC([MAIN([I("jump", -1, Reg(0))])])),
        "V-14", "E_SCHEMA", "jump target must be a Label operand")

    # ---- V-15/V-16/V-17/V-19: declaration surface ----------------------------
    mod = MOD([MAIN()])
    mod["models"] = [{"name": "Box", "fields": [], "typeParams": ["T"]}]
    add("v15-model-typeparams", jd({"irVersion": "1", "module": mod}),
        "V-15", "E_UNSPECIFIED", "typeParams must be []")
    add("v16-route-path-without-method", jd(DOC([MAIN(route={"path": "/x"})])),
        "V-16 (E-10)", "E_SCHEMA", "plain vs route decided by routeMethod")
    add("v16-route-limits-without-method", jd(DOC([MAIN(route={"has_limit": True})])),
        "V-16", "E_SCHEMA", "plain functions have zero limits")
    mod = MOD([MAIN()])
    mod["types"] = [{"name": "Color", "kind": "enum", "variants": [],
                     "richVariants": [], "methods": ["m"]}]
    add("v17-type-methods", jd({"irVersion": "1", "module": mod}),
        "V-17 (E-11)", "E_UNSPECIFIED", "methods must be []")
    add("v19-db-get", jd(DOC([MAIN([I("db_get", 0), I("ret", -1, Reg(0))])])),
        "V-19 (E-12)", "E_UNSUPPORTED", "db_* opcodes excluded")

    # ---- R6 fail-closed extensions beyond the numbered rules -----------------
    add("x-unsupported-opcode", jd(DOC([MAIN([I("alloc", 0), I("ret", -1, Reg(0))])])),
        "R6 subset", "E_UNSUPPORTED", "opcode outside the native subset")
    add("x-route-function", jd(DOC([MAIN(route={"method": "GET", "path": "/"})])),
        "R6 subset", "E_UNSUPPORTED", "Kynx routes outside the native subset")
    add("x-duplicate-key",
        jd(DOC()).replace(b'{"irVersion":"1",', b'{"irVersion":"1","irVersion":"1",', 1),
        "R6 strictness", "E_DUPLICATE", "duplicate object keys are rejected")
    add("x-duplicate-label",
        jd(DOC([MAIN([I("label", -1, Lab(1)), I("label", -1, Lab(1)), I("jump", -1, Lab(1))])])),
        "R6 strictness", "E_DUPLICATE", "one label number per function")

    # ---- D-M1-10: the map family stays closed (docs/DECISIONS.md) ------------
    add("x-map-create",
        jd(DOC([MAIN([I("map_create", 0, Int(8)), I("ret", -1, Int(0))], ["map"])])),
        "D-M1-10", "E_UNSUPPORTED",
        "map_create: no producer; 1-arg lowering vs arena+value_size")
    add("x-map-set",
        jd(DOC([MAIN([I("map_set", 3, Reg(0), Reg(1), Reg(2)), I("ret", -1, Int(0))],
                     ["map", "string", "string", "result"])])),
        "D-M1-10", "E_UNSUPPORTED",
        "map_set: no producer; a live map value needs U-04 (arena prepend)")
    add("x-map-get",
        jd(DOC([MAIN([I("map_get", 2, Reg(0), Reg(1)), I("ret", -1, Int(0))],
                     ["map", "string", "result"])])),
        "D-M1-10", "E_UNSUPPORTED",
        "map_get: no producer; a live map value needs U-04 (arena prepend)")
    add("x-map-has",
        jd(DOC([MAIN([I("map_has", 2, Reg(0), Reg(1)), I("ret", -1, Int(0))],
                     ["map", "string", "bool"])])),
        "D-M1-10", "E_UNSUPPORTED",
        "map_has: needs a map; creation depends on U-04 (arena prepend)")
    add("x-map-delete",
        jd(DOC([MAIN([I("map_delete", 3, Reg(0), Reg(1)), I("ret", -1, Int(0))],
                     ["map", "string", "void"])])),
        "D-M1-10", "E_UNSUPPORTED",
        "map_delete: producer dest register is never declared")
    add("x-map-keys",
        jd(DOC([MAIN([I("map_keys", 1, Reg(0)), I("ret", -1, Int(0))],
                     ["map", "list"])])),
        "D-M1-10", "E_UNSUPPORTED",
        "map_keys: 1-arg lowering vs (map, arena)")

    return cases


def build_accept_files():
    files = []
    helper = FN("helper", [I("call", -1, Sym("nosuchfn"), Int(0)), I("ret")], [],
                rtype="void", extern=True)
    files.append(("ok-extern-body",
                  jd(DOC([MAIN([I("load_const", 0, Int(7)), I("ret", -1, Reg(0))]), helper]))))
    files.append(("ok-untargeted-label",
                  jd(DOC([MAIN([I("load_const", 0, Int(5)), I("label", -1, Lab(1)),
                                I("ret", -1, Reg(0))])]))))
    add2 = FN("add2",
              [I("arg", 0, Int(0)), I("arg", 1, Int(1)),
               I("add", 2, Reg(0), Reg(1)), I("ret", -1, Reg(2))],
              Types(3), params=["a", "b"], ptypes=["int", "int"])
    main = MAIN([I("load_const", 0, Int(4)), I("load_const", 1, Int(5)),
                 I("arg", -1, Reg(0)), I("arg", -1, Reg(1)),
                 I("call", -1, Sym("zero"), Int(0)),
                 I("call", 2, Sym("add2"), Int(2)),
                 I("ret", -1, Reg(2))], Types(3))
    files.append(("ok-call-arity-suffix", jd(DOC([main, ZERO(), add2]))))
    return files


def build_manifest(reject, accept_files):
    del accept_files  # local accept files are emitted from build_accept_files()
    return {
        "reject": [{"file": f"cases/{c['stem']}.json", "exit": 1, "code": c["code"],
                    "rule": c["rule"], "note": c["note"]} for c in reject],
        "accept": [
            {"file": "../../docs/proposed-ir/m1/01-int-call.json", "run_exit": 42,
             "run_stdout": "M1-add\r\n\r\n", "rule": "corpus",
             "note": "valid m1 fixture compiles, runs and prints"},
            {"file": "cases/ok-extern-body.json", "run_exit": 7, "rule": "V-18 (E-09)",
             "note": "extern body is parsed and ignored, never emitted"},
            {"file": "cases/ok-untargeted-label.json", "run_exit": 5, "rule": "E-07",
             "note": "label never targeted by a jump is accepted"},
            {"file": "cases/ok-call-arity-suffix.json", "run_exit": 9, "rule": "V-13 (E-05)",
             "note": "call consumes exactly operand2 pending args; zero consumes none"},
        ],
    }


def render_all():
    reject = build_reject()
    accept_files = build_accept_files()
    manifest = build_manifest(reject, accept_files)
    expected = {}
    for case in reject:
        expected[CASES / (case["stem"] + ".json")] = case["data"]
    for stem, data in accept_files:
        expected[CASES / (stem + ".json")] = data
    expected[HERE / "manifest.json"] = (
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
    return expected


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--check", action="store_true",
                    help="compare the tree against a fresh render")
    args = ap.parse_args()
    expected = render_all()
    if args.check:
        bad = []
        for path, data in expected.items():
            if not path.exists() or path.read_bytes() != data:
                bad.append(path.relative_to(REPO).as_posix())
        for path in sorted(CASES.glob("*.json")):
            if path not in expected:
                bad.append(path.relative_to(REPO).as_posix() + " (unexpected)")
        if bad:
            print("FAIL drift: " + ", ".join(bad))
            return 1
        print(f"reject suite render matches the tree "
              f"({len(expected) - 1} case files + manifest)")
        return 0
    CASES.mkdir(parents=True, exist_ok=True)
    for path, data in expected.items():
        path.write_bytes(data)
    print(f"wrote {len(expected) - 1} case files + manifest.json")
    return 0


if __name__ == "__main__":
    sys.exit(main())
