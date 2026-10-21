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


def Str(s):
    return {"kind": "String", "value": s}


def Bool(b):
    return {"kind": "Bool", "value": b}


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


def G(name, type_name, value):
    return {"name": name, "typeName": type_name, "value": value}


def DOC(fns=None, globals_=None):
    doc = {"irVersion": "1", "module": MOD(fns if fns is not None else [MAIN()])}
    if globals_ is not None:
        doc["module"]["globals"] = globals_
    return doc


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

    # ---- D-M1-11: model constructors + field access stay inside the ----------
    # ---- measured int/bool/float/string field subset (docs/DECISIONS.md) -----
    box = {"name": "Box", "fields": [{"name": "n", "typeName": "int"}],
           "typeParams": []}
    pair = [{"name": "A", "fields": [{"name": "f", "typeName": "int"}],
             "typeParams": []},
            {"name": "B", "fields": [{"name": "g", "typeName": "int"}],
             "typeParams": []}]

    mod = MOD([MAIN([I("load_const", 0, Int(1)),
                     I("load_field", 1, Reg(0), Sym("Box.n")),
                     I("ret", -1, Int(0))], ["int", "int"])])
    mod["models"] = [box]
    add("x-load-field-nonmodel-base", jd({"irVersion": "1", "module": mod}),
        "D-M1-11", "CGEN", "load_field receiver must be a model register")

    mod = MOD([MAIN([I("arg", -1, Int(1)), I("call", 0, Str("Box"), Int(1)),
                     I("load_field", 1, Reg(0), Sym("Box.missing")),
                     I("ret", -1, Int(0))], ["Box", "int"])])
    mod["models"] = [box]
    add("x-field-missing-field", jd({"irVersion": "1", "module": mod}),
        "D-M1-11", "CGEN", "field reference must exist in the owner model")

    mod = MOD([MAIN([I("arg", -1, Int(1)), I("call", 0, Str("A"), Int(1)),
                     I("load_field", 1, Reg(0), Sym("B.g")),
                     I("ret", -1, Int(0))], ["A", "int"])])
    mod["models"] = list(pair)
    add("x-field-owner-mismatch", jd({"irVersion": "1", "module": mod}),
        "D-M1-11", "CGEN", "field reference owner must match the receiver type")

    mod = MOD([MAIN([I("arg", -1, Int(1)), I("call", 0, Str("Box"), Int(1)),
                     I("load_field", 1, Reg(0), Sym("Box.n")),
                     I("ret", -1, Int(0))], ["Box", "string"])])
    mod["models"] = [box]
    add("x-load-field-dest-mismatch", jd({"irVersion": "1", "module": mod}),
        "D-M1-11", "CGEN", "load_field destination type must equal the field type")

    mod = MOD([MAIN([I("arg", -1, Int(1)), I("call", 0, Str("Box"), Int(1)),
                     I("store_field", 1, Reg(0), Sym("Box.n"), Int(5)),
                     I("ret", -1, Int(0))], ["Box", "int"])])
    mod["models"] = [box]
    add("x-store-field-with-dest", jd({"irVersion": "1", "module": mod}),
        "D-M1-11", "CGEN", "store_field does not write a destination")

    mod = MOD([MAIN([I("arg", -1, Int(1)), I("arg", -1, Int(2)),
                     I("call", 0, Str("Box"), Int(2)),
                     I("ret", -1, Int(0))], ["Box"])])
    mod["models"] = [box]
    add("x-ctor-arity-mismatch", jd({"irVersion": "1", "module": mod}),
        "D-M1-11", "CGEN", "constructor arity equals the model field count")

    mod = MOD([MAIN()])
    mod["models"] = [{"name": "Box",
                      "fields": [{"name": "p", "typeName": "list"}],
                      "typeParams": []}]
    add("x-model-field-unsupported-type", jd({"irVersion": "1", "module": mod}),
        "D-M1-11", "CGEN", "only int/bool/float/string fields are lowered")

    mod = MOD([MAIN()])
    mod["models"] = [box, dict(box)]
    add("x-duplicate-model-name", jd({"irVersion": "1", "module": mod}),
        "D-M1-11", "E_DUPLICATE", "duplicate model name")

    mod = MOD([MAIN(), ZERO()])
    mod["models"] = [{"name": "zero", "fields": [], "typeParams": []}]
    add("x-model-function-collision", jd({"irVersion": "1", "module": mod}),
        "D-M1-11", "E_DUPLICATE", "model name collides with a function name")

    mod = MOD([MAIN([I("load_field", 0, Reg(0), Int(0)),
                     I("ret", -1, Reg(0))], ["int"])])
    mod["models"] = [box]
    add("x-field-ref-kind", jd({"irVersion": "1", "module": mod}),
        "D-M1-11", "E_SCHEMA", "field reference is a Symbol or String")

    mod = MOD([MAIN([I("call", 0, Str("Nope"), Int(0)),
                     I("ret", -1, Reg(0))], ["int"])])
    add("x-ctor-unknown-model", jd({"irVersion": "1", "module": mod}),
        "D-M1-11", "CGEN", "constructor target must be a declared model")

    # ---- D-M1-12: union tag dispatch + payload binding stay inside the ------
    # ---- measured ctor/tag/data subset (docs/DECISIONS.md) -------------------
    def utype(name, variants, rich):
        return {"name": name, "kind": "union_type", "variants": list(variants),
                "richVariants": list(rich), "methods": []}

    add("x-union-create-opcode",
        jd(DOC([MAIN([I("union_create", 0, Int(0)), I("ret", -1, Reg(0))], ["int"])])),
        "D-M1-12", "E_UNSUPPORTED",
        "union_create: no producer; construction stays on the call lowering")
    add("x-union-switch-op",
        jd(DOC([MAIN([I("switch_op", -1, Reg(0)), I("ret", -1, Reg(0))], ["int"])])),
        "D-M1-12", "E_UNSUPPORTED",
        "switch_op: no producer; the lowering is an empty switch skeleton")
    add("x-union-type-info",
        jd(DOC([MAIN([I("type_info", 0, Reg(0)), I("ret", -1, Reg(0))], ["int"])])),
        "D-M1-12", "E_UNSUPPORTED",
        "type_info: no producer; the lowering is a comment only (U-12)")

    mod = MOD([MAIN()])
    mod["types"] = [utype("Shape", ["Dot"], ["float"])]
    add("x-union-payload-float", jd({"irVersion": "1", "module": mod}),
        "D-M1-12", "CGEN",
        "single float payloads truncate in the pin and stay fail-closed")

    mod = MOD([MAIN()])
    mod["types"] = [utype("Shape", ["Many"], ["int\tint\tint"])]
    add("x-union-payload-arity", jd({"irVersion": "1", "module": mod}),
        "D-M1-12", "CGEN", "payload arity beyond the measured pack subset")

    mod = MOD([MAIN()])
    mod["types"] = [{"name": "Color", "kind": "enumeration",
                     "variants": ["Red", "Green"], "richVariants": ["", ""],
                     "methods": []}]
    add("x-union-kind-enumeration", jd({"irVersion": "1", "module": mod}),
        "D-M1-12", "CGEN", "only kind union_type is lowered")

    mod = MOD([MAIN([I("arg", -1, Int(1)),
                     I("call", 0, Str("Dot"), Int(1)),
                     I("decl_var", -1, Str("d"), Reg(0)),
                     I("load_var", 1, Str("d")),
                     I("union_get_tag", 2, Reg(1)),
                     I("ret", -1, Reg(2))], ["void*", "void*", "bool"])])
    mod["types"] = [utype("Shape", ["Dot"], ["int"])]
    add("x-union-get-tag-dest-bool", jd({"irVersion": "1", "module": mod}),
        "D-M1-12", "CGEN", "union_get_tag destination must be int")

    add("x-union-get-tag-src-int",
        jd(DOC([MAIN([I("load_const", 0, Int(3)),
                      I("union_get_tag", 1, Reg(0)),
                      I("ret", -1, Reg(1))], ["int", "int"])])),
        "D-M1-12", "CGEN",
        "union_get_tag source must be a void* cell pointer")

    mod = MOD([MAIN([I("load_var", 0, Sym("Shape_TAG_Nope")),
                     I("ret", -1, Reg(0))], ["int"])])
    mod["types"] = [utype("Shape", ["Dot"], ["int"])]
    add("x-union-tag-unknown", jd({"irVersion": "1", "module": mod}),
        "D-M1-12", "CGEN", "tag constant must name a declared variant")

    mod = MOD([MAIN([I("arg", -1, Int(1)), I("arg", -1, Str("s")),
                     I("call", 0, Str("Pair"), Int(2)),
                     I("union_get_data", 1, Reg(0)),
                     I("load_field", 2, Reg(1), Sym("Shape_Pair._9")),
                     I("ret", -1, Reg(2))], ["void*", "void*", "int"])])
    mod["types"] = [utype("Shape", ["Pair"], ["int\tstring"])]
    add("x-union-pack-missing-field", jd({"irVersion": "1", "module": mod}),
        "D-M1-12", "CGEN", "pack field index must exist in the variant")

    mod = MOD([MAIN([I("arg", -1, Int(1)),
                     I("call", 0, Str("Dot"), Int(1)),
                     I("union_get_data", 1, Reg(0)),
                     I("load_field", 2, Reg(1), Sym("Shape_Dot._0")),
                     I("ret", -1, Reg(2))], ["void*", "void*", "int"])])
    mod["types"] = [utype("Shape", ["Dot"], ["int"])]
    add("x-union-pack-single", jd({"irVersion": "1", "module": mod}),
        "D-M1-12", "CGEN",
        "single payloads are the data word, not a pack struct")

    mod = MOD([MAIN([I("arg", -1, Int(1)),
                     I("call", 0, Str("Pair"), Int(1)),
                     I("ret", -1, Reg(0))], ["void*"])])
    mod["types"] = [utype("Shape", ["Pair"], ["int\tstring"])]
    add("x-union-ctor-arity", jd({"irVersion": "1", "module": mod}),
        "D-M1-12", "CGEN",
        "constructor arity must equal the variant payload count")

    mod = MOD([MAIN()])
    mod["types"] = [utype("Shape", ["Dup", "Dup"], ["int", "int"])]
    add("x-union-duplicate-variant", jd({"irVersion": "1", "module": mod}),
        "D-M1-12", "E_DUPLICATE", "duplicate variant name in one type")

    mod = MOD([MAIN()])
    mod["types"] = [utype("A", ["Same"], [""]), utype("B", ["Same"], [""])]
    add("x-union-variant-two-types", jd({"irVersion": "1", "module": mod}),
        "D-M1-12", "E_DUPLICATE", "variant name declared by more than one type")

    mod = MOD([MAIN(), FN("Shape_TAG_Dot", [], [], rtype="void", extern=True)])
    mod["types"] = [utype("Shape", ["Dot"], ["int"])]
    add("x-union-tag-collision", jd({"irVersion": "1", "module": mod}),
        "D-M1-12", "E_DUPLICATE", "tag constant collides with a function name")

    mod = MOD([MAIN()])
    mod["types"] = [utype("Shape", ["A", "B"], ["int"])]
    add("x-union-rich-lengths", jd({"irVersion": "1", "module": mod}),
        "D-M1-12", "E_SCHEMA",
        "variants and richVariants must have equal lengths")

    # ---- D-M1-13: alloc/free stay outside the native subset ------------------
    add("x-alloc-opcode",
        jd(DOC([MAIN([I("alloc", 0, Int(8)), I("ret", -1, Reg(0))], ["int"])])),
        "D-M1-13", "E_UNSUPPORTED",
        "alloc: no producer; the raw pointer result has no consumer in the subset")
    add("x-free-opcode",
        jd(DOC([MAIN([I("free", -1), I("ret", -1, Int(0))])])),
        "D-M1-13", "E_UNSUPPORTED",
        "free: no producer; the lowering is a comment only, not semantics (U-09)")

    # ---- D-M1-14: function values and indirect calls -------------------------
    def inc_fn():
        return FN("inc", [I("arg", 0, Int(0)), I("load_const", 1, Int(1)),
                          I("add", 2, Reg(0), Reg(1)), I("ret", -1, Reg(2))],
                  ["int", "int", "int"], params=["x"], ptypes=["int"])

    add("x-fn-value-unknown",
        jd(DOC([MAIN([I("decl_var", -1, Sym("f"), Sym("nosuch"))] + base_instrs())])),
        "D-M1-14", "CGEN", "symbol initializer must name a module function")
    add("x-fn-value-main",
        jd(DOC([MAIN([I("decl_var", -1, Sym("f"), Sym("main"))] + base_instrs())])),
        "D-M1-14", "CGEN", "main is an entry point, never a function value (n77)")
    add("x-fn-value-void-signature",
        jd(DOC([MAIN([I("decl_var", -1, Sym("g"), Sym("sink"))] + base_instrs()),
                FN("sink", [], [], rtype="void", extern=True)])),
        "D-M1-14", "CGEN", "only the scalar signature spellings are admitted")
    add("x-fn-value-dest-type",
        jd(DOC([inc_fn(), MAIN([I("load_const", 0, Sym("inc")),
                                I("ret", -1, Reg(0))])])),
        "D-M1-14", "CGEN", "the spelling must equal the destination register type")
    add("x-fn-callee-not-fn",
        jd(DOC([MAIN([I("load_const", 0, Int(5)),
                      I("call", 1, Reg(0), Int(0)),
                      I("ret", -1, Reg(1))], Types(3))])),
        "D-M1-14", "CGEN", "an indirect callee register must carry a fn(...) spelling")
    add("x-fn-arity-pending",
        jd(DOC([inc_fn(), MAIN([I("decl_var", -1, Sym("f"), Sym("inc")),
                                I("call", 1, Reg(0), Int(2)),
                                I("load_var", 0, Sym("f")),
                                I("load_const", 2, Int(0)),
                                I("ret", -1, Reg(2))],
                               ["fn(int)->int", "int", "int"])])),
        "V-13", "E_SCHEMA", "call consumes at most the pending argument count")
    add("x-fn-arity-signature",
        jd(DOC([inc_fn(), MAIN([I("decl_var", -1, Sym("f"), Sym("inc")),
                                I("arg", -1, Int(2)), I("arg", -1, Int(3)),
                                I("load_var", 0, Sym("f")),
                                I("call", 1, Reg(0), Int(2)),
                                I("load_const", 2, Int(0)),
                                I("ret", -1, Reg(2))],
                               ["fn(int)->int", "int", "int"])])),
        "D-M1-14", "CGEN", "arity must equal the spelling's parameter count")
    add("x-fn-arg-type",
        jd(DOC([inc_fn(), MAIN([I("decl_var", -1, Sym("f"), Sym("inc")),
                                I("arg", -1, Flt(1.5)),
                                I("load_var", 0, Sym("f")),
                                I("call", 1, Reg(0), Int(1)),
                                I("load_const", 2, Int(0)),
                                I("ret", -1, Reg(2))],
                               ["fn(int)->int", "int", "int"])])),
        "D-M1-14", "CGEN", "each pending argument must match its parameter token")
    add("x-fn-dest-mismatch",
        jd(DOC([inc_fn(), MAIN([I("decl_var", -1, Sym("f"), Sym("inc")),
                                I("arg", -1, Int(41)),
                                I("load_var", 0, Sym("f")),
                                I("call", 1, Reg(0), Int(1)),
                                I("load_const", 2, Int(0)),
                                I("ret", -1, Reg(2))],
                               ["fn(int)->int", "bool", "int"])])),
        "D-M1-14", "CGEN", "the destination must match the spelling's return token")
    add("x-fn-store-symbol",
        jd(DOC([MAIN([I("store_var", -1, Sym("h"), Sym("inc"))] + base_instrs())])),
        "D-M1-14", "CGEN",
        "only decl_var takes a Symbol initializer; a rebinding loads a register first")
    add("x-fn-return-boundary",
        jd(DOC([MAIN(), FN("badret", [I("ret")], [], rtype="fn(int)->int")])),
        "D-M1-14", "CGEN", "functions never return function values in M1")
    add("x-fn-type-spelling",
        jd(DOC([MAIN([I("load_const", 0, Int(0)), I("ret", -1, Reg(0))],
                     ["fn(int->int"])])),
        "D-M1-14", "CGEN", "register fn(...) types must be well-formed spellings")
    add("x-fn-type-nonscalar",
        jd(DOC([MAIN([I("load_const", 0, Int(0)), I("ret", -1, Reg(0))],
                     ["fn(list)->int"])])),
        "D-M1-14", "CGEN", "only scalar parameter and return tokens are admitted")

    # ---- D-M1-18: the builder's direct-call form (O1 String = named call) ----
    # The pinned builder writes the callee name as a String target (spec 3.1
    # step 2) and always allocates a `void`-typed destination for a void callee
    # (step 7 says the destination is optional, not forbidden). Both facts come
    # from the language-emitted corpus; the rejections below keep every other
    # shape fail-closed.
    def one_arg_fn():
        return FN("one", [I("arg", 0, Int(0)), I("ret", -1, Reg(0))],
                  ["int"], params=["a"], ptypes=["int"])

    add("x-call-string-target-arity",
        jd(DOC([one_arg_fn(), MAIN([I("arg", -1, Int(1)), I("arg", -1, Int(2)),
                                    I("call", 0, Str("one"), Int(2)),
                                    I("ret", -1, Reg(0))], ["int"])])),
        "D-M1-18", "CGEN",
        "a String target naming a function is a named call: arity must match")
    add("x-call-string-target-arg-type",
        jd(DOC([one_arg_fn(), MAIN([I("arg", -1, Str("x")),
                                    I("call", 0, Str("one"), Int(1)),
                                    I("ret", -1, Reg(0))], ["int"])])),
        "D-M1-18", "CGEN",
        "each pending argument must match the declared parameter type")
    add("x-call-string-target-dest",
        jd(DOC([one_arg_fn(), MAIN([I("arg", -1, Int(1)),
                                    I("call", 1, Str("one"), Int(1)),
                                    I("load_const", 0, Int(0)),
                                    I("ret", -1, Reg(0))], ["int", "bool"])])),
        "D-M1-18", "CGEN",
        "the destination must match the return type of the String-named callee")
    add("x-call-string-target-main",
        jd(DOC([MAIN([I("call", 0, Str("main"), Int(0)),
                      I("ret", -1, Reg(0))], ["int"])])),
        "D-M1-18", "CGEN",
        "main is an entry point, never a String call target either")
    add("x-call-void-dest-type",
        jd(DOC([ZERO(), MAIN([I("call", 0, Str("zero"), Int(0)),
                              I("ret", -1, Int(0))], ["int"])])),
        "D-M1-18", "CGEN",
        "a void call may only carry a void-typed destination, never a value")

    # ---- D-M1-19: the literal forms the builder writes -----------------------
    # Spec 2.1 has no dedicated constructor for bool and float literals: the
    # pinned builder renders them as raw `Sym` text ("true", "2.5e3"), and it
    # inlines a literal initializer in decl_var (buildVarDecl). The text
    # grammar decides (a fraction or an exponent is required), the literal
    # still has to match the destination type, and every other shape keeps
    # failing closed.
    add("x-sym-literal-float-dest-type",
        jd(DOC([MAIN([I("load_const", 0, Sym("1.5")),
                      I("ret", -1, Int(0))], ["int"])])),
        "D-M1-19", "CGEN",
        "a float literal in Sym text must still match the register type")
    add("x-sym-literal-bool-dest-type",
        jd(DOC([MAIN([I("load_const", 0, Sym("true")),
                      I("ret", -1, Int(0))], ["float"])])),
        "D-M1-19", "CGEN",
        "a bool literal in Sym text must still match the register type")
    add("x-sym-literal-not-finite",
        jd(DOC([MAIN([I("load_const", 0, Sym("1e999")),
                      I("ret", -1, Int(0))], ["float"])])),
        "D-M1-19", "CGEN",
        "numeric Sym text whose value is not finite is rejected (spec V-08)")
    add("x-sym-integer-text-is-a-symbol",
        jd(DOC([MAIN([I("load_const", 0, Sym("7")),
                      I("ret", -1, Int(0))], ["int"])])),
        "D-M1-19", "CGEN",
        "a bare integer is not a float literal: it stays a function value")
    add("x-decl-var-literal-type",
        jd(DOC([MAIN([I("decl_var", -1, Sym("a"), Int(1)),
                      I("decl_var", -1, Sym("a"), Str("x")),
                      I("ret", -1, Int(0))])])),
        "D-M1-19", "CGEN",
        "a local keeps the type of its first decl_var initializer")

    # ---- D-M1-20: the slots the builder serialized before it could type them -
    # Spec 2.3 makes registerTypes advisory: the pinned builder writes
    # `unknown` for a slot it has not typed yet (it serializes before
    # inferRegisterTypes), and the language backend recovers the slot from
    # the defining instruction. R6 mirrors those rules for the opcodes in the
    # native subset and keeps failing closed wherever a rule cannot decide.
    add("x-infer-arith-unknown-operand",
        jd(DOC([MAIN([I("load_const", 0, Int(1)),
                      I("add", 2, Reg(1), Reg(0)),
                      I("ret", -1, Int(0))],
                     ["int", "unknown", "unknown"])])),
        "D-M1-20", "CGEN",
        "an arithmetic slot with an untyped operand is never guessed")
    add("x-infer-unresolved-call",
        jd(DOC([MAIN([I("call", 0, Sym("nosuchfn"), Int(0)),
                      I("ret", -1, Int(0))], ["unknown"])])),
        "D-M1-20", "CGEN",
        "a call whose target is not declared in the module stays unknown")
    add("x-infer-unknown-load-var",
        jd(DOC([MAIN([I("load_var", 0, Sym("nosuchlocal")),
                      I("ret", -1, Int(0))], ["unknown"])])),
        "D-M1-20", "CGEN",
        "a load_var of an undeclared name stays unknown")
    add("x-infer-result-unwrap",
        jd(DOC([MAIN([I("result_unwrap", 0, Reg(1)),
                      I("ret", -1, Int(0))], ["unknown", "result"])])),
        "D-M1-20", "CGEN",
        "result_unwrap has no rule in the language either: the slot stays unknown")

    # ---- D-M1-15: globals hold literals, never register words ----------------
    mod = MOD([MAIN()])
    mod["globals"] = [{"name": "G", "value": Reg(0), "typeName": "int"}]
    add("x-global-register", jd({"irVersion": "1", "module": mod}),
        "D-M1-15", "CGEN",
        "a global with an ir_register value has no producer; literals only")

    # ---- D-M1-16: spec edge cases E-03/E-04/E-06 stay fail-closed ------------
    # The spec edge table says "accept" for all three; R6 keeps its native
    # subset and pins the rejections (see DECISIONS.md D-M1-16 and D-M1-09).
    # D-M1-30 supersedes D-M1-16/E-04 for the create: elemKind 0 is the
    # runtime's own OPAQUE state (collections.c:170), the first typed write
    # adopts the kind (:102-116) and a read accepts an opaque list (:118-128).
    # What stays out is kind 5 (ref), which has no native slot decoder, and the
    # opaque accessor: kind 0 is admitted at the create only.
    add("x-list-create-kind-ref",
        jd(DOC([MAIN([I("list_create", 0, Int(8), Int(1), ek=5),
                      I("ret", -1, Int(0))], ["list"])])),
        "D-M1-30", "CGEN",
        "elemKind 5 (ref) still has no native slot decoder for a created list")
    add("x-list-get-kind-opaque",
        jd(DOC([MAIN([I("list_create", 0, Int(8), Int(1)),
                      I("load_const", 1, Int(0)),
                      I("list_get", 2, Reg(0), Reg(1)),
                      I("ret", -1, Int(0))],
                     ["list", "int", "int"])])),
        "D-M1-30", "CGEN",
        "elemKind 0 is admitted at list_create only; a read has no slot decoder")
    add("x-list-set-kind-opaque",
        jd(DOC([MAIN([I("list_create", 0, Int(8), Int(1)),
                      I("load_const", 1, Int(0)),
                      I("load_const", 2, Int(7)),
                      I("list_set", -1, Reg(0), Reg(1), Reg(2)),
                      I("ret", -1, Int(0))],
                     ["list", "int", "int"])])),
        "D-M1-30", "CGEN",
        "elemKind 0 is admitted at list_create only; a write has no slot decoder")
    add("x-list-push-kind-opaque",
        jd(DOC([MAIN([I("list_create", 0, Int(8), Int(1)),
                      I("load_const", 1, Int(7)),
                      I("list_push", 2, Reg(0), Reg(1)),
                      I("ret", -1, Int(0))],
                     ["list", "int", "void"])])),
        "D-M1-30", "CGEN",
        "elemKind 0 is admitted at list_create only; a push has no slot decoder")
    # D-M1-31 pins the comparison arms (c_backend.orb:2745-2810): a string
    # against a number is the pin's word fallthrough, a bool is a number, but
    # a float still needs both sides typed float and a string has no
    # relational arm. These two cases are the boundaries, not the rule.
    add("x-compare-float-int",
        jd(DOC([MAIN([I("load_const", 0, Flt(1.5)),
                      I("load_const", 1, Int(2)),
                      I("eq", 2, Reg(0), Reg(1)),
                      I("ret", -1, Int(0))],
                     ["float", "int", "bool"])])),
        "D-M1-31", "CGEN",
        "a float compared with an int stays out: the emitter has no conversion")
    add("x-relational-string",
        jd(DOC([MAIN([I("load_const", 0, Str("a")),
                      I("load_const", 1, Str("b")),
                      I("lt", 2, Reg(0), Reg(1)),
                      I("ret", -1, Int(0))],
                     ["string", "string", "bool"])])),
        "D-M1-31", "CGEN",
        "the relational ops have no string arm; only eq/ne compare words")
    add("x-list-get-kind-ref",
        jd(DOC([MAIN([I("list_create", 0, Int(8), Int(1), ek=1),
                      I("load_const", 1, Int(0)),
                      I("list_get", 2, Reg(0), Reg(1), ek=5),
                      I("ret", -1, Int(0))],
                     ["list", "int", "unknown"])])),
        "D-M1-16", "CGEN",
        "E-03: elemKind 5 keeps ref types unknown/void*, outside the subset")
    add("x-print-two-args",
        jd(DOC([MAIN([I("arg", -1, Str("a")), I("arg", -1, Str("b")),
                      I("call", -1, Sym("orbit_print"), Int(2)),
                      I("load_const", 0, Int(0)),
                      I("ret", -1, Reg(0))]),
                FN("orbit_print", [], [], rtype="void", extern=True,
                   params=["s"], ptypes=["string"])])),
        "D-M1-16", "CGEN",
        "E-06: multi-argument print has no producer (sema E1003) and no lowering")

    # ---- D-M1-21: the inline literal operand form of the operations ---------
    # The pinned builder writes a literal straight into the operand of
    # add/sub/mul/div/mod/eq/ne/lt/le/gt/ge instead of loading it into a
    # register (2661 operands across 79/94 documents, kinds Int/String/Bool).
    # R6 materializes each admitted one into a reserved scratch slot. Nothing
    # outside that measured set is admitted, and admitting the form never
    # admits a mismatched type.
    add("x-lit-float-operand",
        jd(DOC([MAIN([I("load_const", 0, Int(1)),
                      I("add", 1, Reg(0), Flt(1.5)),
                      I("ret", -1, Reg(1))], ["int", "int"])])),
        "D-M1-21", "CGEN",
        "a float literal operand has no producer in the pinned builder")
    add("x-lit-and-op-operand",
        jd(DOC([MAIN([I("load_const", 0, Sym("true")),
                      I("and_op", 1, Reg(0), Bool(False)),
                      I("ret", -1, Reg(1))], ["bool", "bool"])])),
        "D-M1-21", "CGEN",
        "an inline literal outside the measured operation set still fails closed")
    add("x-lit-type-mismatch",
        jd(DOC([MAIN([I("load_const", 0, Int(1)),
                      I("add", 1, Reg(0), Str("x")),
                      I("ret", -1, Reg(1))], ["int", "int"])])),
        "D-M1-21", "CGEN",
        "admitting the operand form does not admit a mismatched literal type")

    # ---- D-M1-22: a parameter read by name is function-scoped ---------------
    # The pinned builder binds every parameter with `arg` and reads it in the
    # body with `load_var <name>`: the backend types that read from paramTypes
    # (c_backend.orb:775-800) and its `arg` emits the parameter itself
    # (:2860-2873). Admitting the read makes no name global, guesses no type,
    # and rescues no undeclared name.
    add("x-load-var-foreign-param",
        jd(DOC([FN("other", [I("arg", 0, Int(0)), I("ret", -1, Reg(0))],
                   ["int"], params=["x"], ptypes=["int"]),
                FN("takes", [I("arg", 0, Int(0)),
                             I("load_var", 1, Sym("x")),
                             I("ret", -1, Reg(1))],
                   ["int", "int"], params=["y"], ptypes=["int"]),
                MAIN([I("load_const", 0, Int(7)), I("ret", -1, Reg(0))],
                     ["int"])])),
        "D-M1-22", "CGEN",
        "a parameter name is scoped to its own function; another's is not guessed")
    add("x-load-var-param-mismatch",
        jd(DOC([FN("takes", [I("arg", 0, Int(0)),
                             I("load_var", 1, Sym("y")),
                             I("ret", -1, Reg(1))],
                   ["int", "float"], params=["y"], ptypes=["int"]),
                MAIN([I("load_const", 0, Int(7)), I("ret", -1, Reg(0))],
                     ["int"])])),
        "D-M1-22", "CGEN",
        "loading a parameter into a differently typed register is refused")

    # ---- D-M1-32: a list spelling is the list pointer ----------------------
    # Every list spelling maps to one C type (c_backend.orb:469-495), so a
    # parameter may say `list<int>`. The rule is about the base: `map<...>` is a
    # different base and a different C type, and R6 has none.
    add("x-param-map-spelling",
        jd(DOC([FN("count", [I("arg", 0, Int(0)),
                             I("load_const", 1, Int(0)),
                             I("ret", -1, Reg(1))],
                   ["list", "int"], params=["m"], ptypes=["map<string, int>"]),
                MAIN([I("load_const", 0, Int(0)), I("ret", -1, Reg(0))],
                     ["int"])])),
        "D-M1-32", "CGEN",
        "only a list spelling is the list type; a map parameter stays unsupported")
    # An extern prototype is a C declaration: R6 types it from the scalar
    # subset and does not lower a list there (no corpus extern takes one).
    add("x-extern-list-param",
        jd(DOC([FN("peek", [I("ret")], [], rtype="void", extern=True,
                   params=["xs"], ptypes=["list<int>"]),
                MAIN([I("load_const", 0, Int(0)), I("ret", -1, Reg(0))],
                     ["int"])])),
        "D-M1-32", "CGEN",
        "an extern parameter stays scalar; a list is not lowered there")

    # ---- D-M1-33: a top-level constant is a #define ------------------------
    # c_backend.orb:396-399 writes `#define NAME <text>` for every global and
    # :1647-1651 renders the value's String payload verbatim, so the text is the
    # type. The builder writes "unknown" for a type it cannot tell; a decimal
    # integer text is an int. Any other text stays outside the subset.
    add("x-global-unknown-text",
        jd(DOC([MAIN()], globals_=[G("TEXT", "unknown", Str("hide"))])),
        "D-M1-33", "CGEN",
        "an unknown-typed global is an int only when its text is a decimal integer")
    add("x-global-declared-string",
        jd(DOC([MAIN()], globals_=[G("GREETING", "string", Str("hi"))])),
        "D-M1-33", "CGEN",
        "a declared non-scalar global type is still refused, whatever the text")
    add("x-global-int-overflow",
        jd(DOC([MAIN()], globals_=[G("BIG", "unknown", Str("2147483648"))])),
        "D-M1-33", "CGEN",
        "a decimal text outside int32 is refused, never clamped")
    add("x-global-int-underflow",
        jd(DOC([MAIN()], globals_=[G("SMALL", "unknown", Str("-2147483649"))])),
        "D-M1-33", "CGEN",
        "a negative decimal text outside int32 is refused on the same parse")

    # ---- D-M1-23: a string `add` concatenates and spreads -------------------
    # The pinned backend turns `add` into orbit_string_concat whenever either
    # operand is a string operand (c_backend.orb:2662-2668) and stringifies each
    # side on its own (:4341-4378, :3881-3908). The spread side must therefore
    # be a scalar a converter covers, the result is always a string, and the
    # Float literal of D-M1-21 is still not something the builder emits into an
    # operand. Each delta below breaks exactly one of those.
    add("x-add-spread-dest",
        jd(DOC([MAIN([I("load_const", 0, Str("n=")),
                      I("load_const", 1, Int(7)),
                      I("add", 2, Reg(0), Reg(1)),
                      I("load_const", 3, Int(0)),
                      I("ret", -1, Reg(3))],
                     ["string", "int", "int", "int"])])),
        "D-M1-23", "CGEN",
        "spreading a scalar into a string add requires a string destination")
    add("x-add-spread-pointer",
        jd(DOC([MAIN([I("load_const", 0, Str("v=")),
                      I("add", 1, Reg(0), Reg(2)),
                      I("ret", -1, Int(0))],
                     ["string", "string", "void*"])])),
        "D-M1-23", "CGEN",
        "a pointer operand has no measured converter and is never spread: the destination is a string, so the scalar guard is the only clause that can refuse")
    add("x-add-spread-float-literal",
        jd(DOC([MAIN([I("load_const", 0, Str("f=")),
                      I("add", 1, Reg(0), Flt(1.5)),
                      I("ret", -1, Int(0))], ["string", "unknown"])])),
        "D-M1-23", "CGEN",
        "a float literal spread operand stays outside the measured builder output")
    add("x-add-string-dest",
        jd(DOC([MAIN([I("load_const", 0, Str("a")),
                      I("load_const", 1, Str("b")),
                      I("add", 2, Reg(0), Reg(1)),
                      I("load_const", 3, Int(0)),
                      I("ret", -1, Reg(3))],
                     ["string", "string", "int", "int"])])),
        "D-M1-23", "CGEN",
        "a string add must produce a string")

    # ---- D-M1-26: the builtin `print` ---------------------------------------
    # feBuildTimeCallType types `print` void (c_backend.orb:3911) and its
    # lowering (c_backend.orb:2971-2999) intercepts the NAME before any
    # declaration, then dispatches on the one argument it consumes. The
    # destination is never stored, so it is admitted as any valid register.
    # The arity rules are the loader's (V-13/E-05) and already have their own
    # cases; what these two pin is that the admission is exactly one name in
    # exactly the String form the builder emits.
    add("x-builtin-name-outside-print",
        jd(DOC([MAIN([I("arg", -1, Int(1)),
                      I("call", -1, Str("println"), Int(1)),
                      I("load_const", 0, Int(0)),
                      I("ret", -1, Reg(0))], ["int"])])),
        "D-M1-26", "CGEN",
        "admitting `print` by name admits no other undeclared builtin")
    add("x-print-symbol-target",
        jd(DOC([MAIN([I("arg", -1, Int(1)),
                      I("call", -1, Sym("print"), Int(1)),
                      I("load_const", 0, Int(0)),
                      I("ret", -1, Reg(0))], ["int"])])),
        "D-M1-26", "CGEN",
        "only the String call target the builder emits is the builtin")

    # ---- D-M1-27: an enumerated runtime extern takes the arena first --------
    # The IR declaration repeats only the program-visible parameters; the
    # arena is injected for a name that is both declared and extern, never for
    # a spelling alone. An undeclared name is still an unknown target.
    add("x-arena-extern-undeclared",
        jd(DOC([MAIN([I("arg", -1, Int(1)),
                      I("call", 0, Str("orbit_int_to_string"), Int(1)),
                      I("arg", -1, Reg(0)),
                      I("call", -1, Str("print"), Int(1)),
                      I("load_const", 1, Int(0)),
                      I("ret", -1, Reg(1))], ["string", "int"])])),
        "D-M1-27", "CGEN",
        "the arena is injected only into a declared extern, not into a bare name")

    # ---- D-M1-28: a void function returns without reading its operand -------
    # Both pinned ret arms emit a bare `return;` once the return type is void
    # (c_backend.orb:3429-3439, :3460-3465), so a carried value is admitted and
    # ignored. The non-void direction is unchanged: the value must still match
    # the declared return type, and its register must still exist.
    add("x-ret-type-mismatch",
        jd(DOC([MAIN(),
                FN("f", [I("load_const", 0, Str("s")),
                         I("ret", -1, Reg(0))],
                   ["string", "int"], rtype="int")])),
        "D-M1-28", "CGEN",
        "a non-void ret still requires a value of the declared return type")

    # ---- D-M1-29: the undeclared runtime string intrinsics ------------------
    # len/at/slice are calls the builder emits as String targets without a
    # declaration; the pinned C backend resolves them through the runtime
    # headers (orbit_api.h:385-386, :411-413). The admission is that table in
    # that form, with the header's arity: a Symbol target stays a declaration
    # lookup and a wrong arity is still refused.
    add("x-intrinsic-symbol-target",
        jd(DOC([MAIN([I("load_const", 0, Str("abcdef")),
                      I("arg", -1, Reg(0)),
                      I("call", 1, Sym("orbit_string_len"), Int(1)),
                      I("load_const", 2, Int(0)),
                      I("ret", -1, Reg(2))],
                     ["string", "int", "int"])])),
        "D-M1-29", "CGEN",
        "the intrinsic table is the builder's String form, not a Symbol alias")
    add("x-intrinsic-arity",
        jd(DOC([MAIN([I("load_const", 0, Str("abc")),
                      I("load_const", 1, Int(1)),
                      I("arg", -1, Reg(0)),
                      I("arg", -1, Reg(1)),
                      I("call", 2, Str("orbit_string_len"), Int(2)),
                      I("load_const", 3, Int(0)),
                      I("ret", -1, Reg(3))],
                     ["string", "int", "int", "int"])])),
        "D-M1-29", "CGEN",
        "an intrinsic call is checked against the runtime header's arity")
    add("x-list-len-bool",
        jd(DOC([MAIN([I("load_const", 0, Bool(True)),
                      I("list_len", 1, Reg(0)),
                      I("load_const", 2, Int(0)),
                      I("ret", -1, Reg(2))],
                     ["bool", "int", "int"])])),
        "D-M1-29", "CGEN",
        "list_len still refuses a receiver that is neither a list nor a string")

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
    files.append(("ok-call-void-dest",
                  jd(DOC([ZERO(), MAIN([I("call", 1, Str("zero"), Int(0)),
                                        I("load_const", 0, Int(6)),
                                        I("ret", -1, Reg(0))], ["int", "void"])]))))
    mod = MOD([MAIN([I("arg", -1, Int(11)),
                     I("call", 0, Str("Box"), Int(1)),
                     I("decl_var", -1, Str("b"), Reg(0)),
                     I("load_var", 1, Str("b")),
                     I("load_field", 2, Reg(1), Sym("Box.n")),
                     I("ret", -1, Reg(2))], ["Box", "Box", "int"])])
    mod["models"] = [{"name": "Box", "fields": [{"name": "n", "typeName": "int"}],
                      "typeParams": []}]
    files.append(("ok-model-ctor", jd({"irVersion": "1", "module": mod})))
    mod = MOD([MAIN([I("arg", -1, Int(11)),
                     I("call", 0, Str("Dot"), Int(1)),
                     I("decl_var", -1, Str("d"), Reg(0)),
                     I("load_var", 1, Str("d")),
                     I("union_get_tag", 2, Reg(1)),
                     I("union_get_data", 3, Reg(1)),
                     I("ret", -1, Reg(3))], ["void*", "void*", "int", "int"])])
    mod["types"] = [{"name": "Shape", "kind": "union_type", "variants": ["Dot"],
                     "richVariants": ["int"], "methods": []}]
    files.append(("ok-union-ctor", jd({"irVersion": "1", "module": mod})))
    inc = FN("inc", [I("arg", 0, Int(0)), I("load_const", 1, Int(1)),
                     I("add", 2, Reg(0), Reg(1)), I("ret", -1, Reg(2))],
             ["int", "int", "int"], params=["x"], ptypes=["int"])
    main = MAIN([I("decl_var", -1, Sym("f"), Sym("inc")),
                 I("arg", -1, Int(41)),
                 I("load_var", 0, Sym("f")),
                 I("call", 1, Reg(0), Int(1)),
                 I("ret", -1, Reg(1))],
                ["fn(int)->int", "int"])
    files.append(("ok-fn-value", jd(DOC([inc, main]))))
    ext = FN("orbit_string_at", [], [], rtype="int", extern=True,
             params=["s", "i"], ptypes=["string", "int"])
    main = MAIN([I("decl_var", -1, Sym("at"), Sym("orbit_string_at")),
                 I("arg", -1, Str("abc")),
                 I("arg", -1, Int(1)),
                 I("load_var", 0, Sym("at")),
                 I("call", 1, Reg(0), Int(2)),
                 I("ret", -1, Reg(1))],
                ["fn(string;int)->int", "int"])
    files.append(("ok-fn-extern", jd(DOC([ext, main]))))
    files.append(("ok-sym-bool-literal",
                  jd(DOC([MAIN([I("load_const", 0, Sym("true")),
                                I("jump_if_false", -1, Reg(0), Lab(1)),
                                I("load_const", 1, Int(5)),
                                I("ret", -1, Reg(1)),
                                I("label", -1, Lab(1)),
                                I("load_const", 1, Int(9)),
                                I("ret", -1, Reg(1))], ["bool", "int"])]))))
    files.append(("ok-sym-float-literal",
                  jd(DOC([MAIN([I("load_const", 0, Sym("0.5")),
                                I("load_const", 1, Sym("5e-1")),
                                I("eq", 2, Reg(0), Reg(1)),
                                I("jump_if_false", -1, Reg(2), Lab(1)),
                                I("load_const", 3, Int(6)),
                                I("ret", -1, Reg(3)),
                                I("label", -1, Lab(1)),
                                I("load_const", 3, Int(1)),
                                I("ret", -1, Reg(3))],
                               ["float", "float", "bool", "int"])]))))
    files.append(("ok-decl-var-literal",
                  jd(DOC([MAIN([I("decl_var", -1, Sym("i"), Int(3)),
                                I("decl_var", -1, Sym("s"), Str("z")),
                                I("decl_var", -1, Sym("b"), Bool(True)),
                                I("load_var", 0, Sym("i")),
                                I("load_var", 1, Sym("s")),
                                I("load_var", 2, Sym("b")),
                                I("ret", -1, Reg(0))],
                               ["int", "string", "bool"])]))))
    files.append(("ok-infer-eq-bool",
                  jd(DOC([MAIN([I("load_const", 0, Int(7)),
                                I("load_const", 1, Int(7)),
                                I("eq", 2, Reg(0), Reg(1)),
                                I("jump_if_false", -1, Reg(2), Lab(1)),
                                I("load_const", 3, Int(5)),
                                I("ret", -1, Reg(3)),
                                I("label", -1, Lab(1)),
                                I("load_const", 3, Int(9)),
                                I("ret", -1, Reg(3))],
                               ["int", "int", "unknown", "int"])]))))
    files.append(("ok-infer-load-var",
                  jd(DOC([MAIN([I("decl_var", -1, Sym("a"), Int(3)),
                                I("load_var", 0, Sym("a")),
                                I("ret", -1, Reg(0))], ["unknown"])]))))
    files.append(("ok-infer-neg-chain",
                  jd(DOC([MAIN([I("load_const", 0, Int(5)),
                                I("neg", 1, Reg(0)),
                                I("neg", 2, Reg(1)),
                                I("ret", -1, Reg(2))],
                               ["int", "unknown", "unknown"])]))))
    seven = FN("seven", [I("load_const", 0, Int(7)), I("ret", -1, Reg(0))], ["int"])
    files.append(("ok-infer-call-return",
                  jd(DOC([seven, MAIN([I("call", 0, Str("seven"), Int(0)),
                                       I("ret", -1, Reg(0))], ["unknown"])]))))
    files.append(("ok-infer-arg-param",
                  jd(DOC([FN("echo", [I("arg", 0, Int(0)), I("ret", -1, Reg(0))],
                             ["unknown"], params=["x"], ptypes=["int"]),
                          MAIN([I("arg", -1, Int(33)),
                                I("call", 0, Sym("echo"), Int(1)),
                                I("ret", -1, Reg(0))], ["int"])]))))
    files.append(("ok-infer-list-create",
                  jd(DOC([MAIN([I("list_create", 0, Int(8), Int(1), ek=1),
                                I("ret", -1, Int(8))], ["unknown"])]))))
    mod = MOD([MAIN([I("load_var", 0, Sym("Shape_TAG_Dot")),
                     I("ret", -1, Reg(0))], ["unknown"])])
    mod["types"] = [{"name": "Shape", "kind": "union_type", "variants": ["Dot"],
                     "richVariants": ["int"], "methods": []}]
    files.append(("ok-infer-union-tag", jd({"irVersion": "1", "module": mod})))
    # D-M1-21: a frame that reserves the result scratch of the list calls and
    # the immediate slots of the literal operands at the same time. The list
    # round-trip is fixture 18's, so the length is the one the oracle verifies;
    # the add puts a literal into a frame whose offsets neighbour the scratch.
    files.append(("ok-lit-operands",
                  jd(DOC([MAIN([I("list_create", 0, Int(8), Int(3), ek=2),
                                I("load_const", 1, Int(10)),
                                I("list_push", 2, Reg(0), Reg(1), ek=2),
                                I("load_const", 3, Int(20)),
                                I("list_push", 4, Reg(0), Reg(3), ek=2),
                                I("load_const", 5, Int(30)),
                                I("list_push", 6, Reg(0), Reg(5), ek=2),
                                I("decl_var", -1, Sym("xs"), Reg(0)),
                                I("load_var", 7, Sym("xs")),
                                I("list_len", 8, Reg(7)),
                                I("add", 9, Reg(8), Int(2)),
                                I("ret", -1, Reg(9))],
                               ["list", "int", "void", "int", "void", "int",
                                "void", "list", "int", "int"])]))))
    # D-M1-22: reading a parameter by name is reading its `arg` binding, so a
    # body that never touches the bound register still sees the argument.
    sum2 = FN("sum2",
              [I("arg", 0, Int(0)), I("arg", 1, Int(1)),
               I("load_var", 2, Sym("x")), I("load_var", 3, Sym("y")),
               I("add", 4, Reg(2), Reg(3)), I("ret", -1, Reg(4))],
              Types(5), params=["x", "y"], ptypes=["int", "int"])
    files.append(("ok-load-var-param",
                  jd(DOC([sum2, MAIN([I("arg", -1, Int(20)), I("arg", -1, Int(22)),
                                      I("call", 0, Sym("sum2"), Int(2)),
                                      I("ret", -1, Reg(0))], ["int"])]))))
    # D-M1-23: a string `add` concatenates, spreading a scalar side through the
    # runtime converters; outside main the arena is r6_arena. Both a non-main
    # int spread (rcx carries the value) and a non-main bool spread (rcx is the
    # arena, rdx the value) must build and run.
    ints = FN("ints",
              [I("arg", 0, Int(0)), I("load_var", 1, Sym("n")),
               I("add", 2, Str("n="), Reg(1)), I("ret", -1, Reg(2))],
              ["int", "int", "unknown"], rtype="string",
              params=["n"], ptypes=["int"])
    bools = FN("bools",
               [I("arg", 0, Int(0)), I("load_var", 1, Sym("b")),
                I("add", 2, Str("b="), Reg(1)), I("ret", -1, Reg(2))],
               ["bool", "bool", "unknown"], rtype="string",
               params=["b"], ptypes=["bool"])
    files.append(("ok-add-string-spread",
                  jd(DOC([ints, bools,
                          MAIN([I("arg", -1, Int(40)),
                                I("call", 0, Sym("ints"), Int(1)),
                                I("eq", 1, Reg(0), Str("n=40")),
                                I("jump_if_false", -1, Reg(1), Lab(1)),
                                I("arg", -1, Bool(True)),
                                I("call", 2, Sym("bools"), Int(1)),
                                I("eq", 3, Reg(2), Str("b=true")),
                                I("jump_if_false", -1, Reg(3), Lab(1)),
                                I("load_const", 4, Int(42)),
                                I("ret", -1, Reg(4)),
                                I("label", -1, Lab(1)),
                                I("load_const", 4, Int(0)),
                                I("ret", -1, Reg(4))],
                               ["string", "bool", "string", "bool", "int"])]))))
    # D-M1-26: the three measured reductions of the builtin `print`, the
    # pinned no-op for any other arity, and the interception by name that
    # shadows a declared extern of the same name (the corpus form: dest typed
    # from the extern's int return and never stored).
    files.append(("ok-print-noop",
                  jd(DOC([MAIN([I("call", -1, Str("print"), Int(0)),
                                I("arg", -1, Int(1)),
                                I("arg", -1, Int(2)),
                                I("call", -1, Str("print"), Int(2)),
                                I("load_const", 0, Int(7)),
                                I("ret", -1, Reg(0))], ["int"])]))))
    files.append(("ok-print-inline-literals",
                  jd(DOC([MAIN([I("arg", -1, Str("hi")),
                                I("call", -1, Str("print"), Int(1)),
                                I("arg", -1, Int(3)),
                                I("call", -1, Str("print"), Int(1)),
                                I("load_const", 0, Int(0)),
                                I("ret", -1, Reg(0))], ["int"])]))))
    show = FN("show", [I("arg", 0, Int(0)),
                       I("load_var", 1, Sym("x")),
                       I("arg", -1, Reg(1)),
                       I("call", -1, Str("print"), Int(1)),
                       I("ret")],
              ["float", "float"], rtype="void", params=["x"], ptypes=["float"])
    files.append(("ok-print-nonmain-float",
                  jd(DOC([show,
                          MAIN([I("load_const", 0, Flt(2.5)),
                                I("arg", -1, Reg(0)),
                                I("call", -1, Str("show"), Int(1)),
                                I("load_const", 1, Int(0)),
                                I("ret", -1, Reg(1))], ["float", "int"])]))))
    declare = FN("print", [], ["int"], rtype="int", extern=True,
                 params=["s"], ptypes=["string"])
    files.append(("ok-print-shadowed-extern",
                  jd(DOC([declare,
                          MAIN([I("arg", -1, Str("x")),
                                I("call", 0, Str("print"), Int(1)),
                                I("load_const", 1, Int(0)),
                                I("ret", -1, Reg(1))], ["int", "int"])]))))
    # D-M1-27: the arena is prepended to a declared extern in the enumerated
    # family. The float member proves the argument shift is positional for both
    # register files. A local function carrying an arena-family name cannot be
    # a control: the runtime already defines that symbol, so the link fails
    # before any injection could be observed.
    fl = FN("orbit_float_to_string", [], ["string"], rtype="string", extern=True,
            params=["v"], ptypes=["float"])
    files.append(("ok-arena-extern-float",
                  jd(DOC([fl,
                          MAIN([I("load_const", 0, Flt(2.5)),
                                I("arg", -1, Reg(0)),
                                I("call", 1, Str("orbit_float_to_string"), Int(1)),
                                I("arg", -1, Reg(1)),
                                I("call", -1, Str("print"), Int(1)),
                                I("load_const", 2, Int(0)),
                                I("ret", -1, Reg(2))], ["float", "string", "int"])]))))
    # D-M1-28: a void function returns without reading the value its ret
    # carries; both pinned arms emit a bare `return;` (c_backend.orb:3429-3439,
    # :3460-3465). The register here is an int, which would be a mismatch if it
    # were read - the corpus shape is the destination of the void call.
    tailfn = FN("tail",
                [I("arg", -1, Str("plain")),
                 I("call", 0, Str("print"), Int(1)),
                 I("ret", -1, Reg(0))],
                ["int"], rtype="void")
    files.append(("ok-void-ret-value",
                  jd(DOC([tailfn,
                          MAIN([I("call", -1, Str("tail"), Int(0)),
                                I("load_const", 0, Int(0)),
                                I("ret", -1, Reg(0))], ["int"])]))))
    # D-M1-29: `.len()` on a string is the same list_len opcode when the
    # builder did not know the receiver's type; the pinned arm lowers it to
    # orbit_string_len (c_backend.orb:3609-3615).
    files.append(("ok-list-len-string",
                  jd(DOC([MAIN([I("load_const", 0, Str("abc")),
                                I("list_len", 1, Reg(0)),
                                I("ret", -1, Reg(1))],
                               ["string", "int"])]))))
    # D-M1-30: the builder emits list_create with elemKind 0 when the first
    # push decides the element type; the runtime starts OPAQUE and adopts it
    # (collections.c:170, :102-116).
    files.append(("ok-list-create-opaque",
                  jd(DOC([MAIN([I("list_create", 0, Int(8), Int(0)),
                                I("load_const", 1, Int(41)),
                                I("list_push", 2, Reg(0), Reg(1), ek=2),
                                I("load_const", 3, Int(0)),
                                I("list_get", 4, Reg(0), Reg(3), ek=2),
                                I("ret", -1, Reg(4))],
                               ["list", "int", "void", "int", "int"])]))))
    # D-M1-31: a bool is a number to the pinned comparison arms, and a string
    # against a number is the word fallthrough. Both results are used, so the
    # control asserts the value and not only that the pair builds.
    files.append(("ok-compare-bool-word",
                  jd(DOC([MAIN([I("load_const", 0, Sym("true")),
                                I("load_const", 1, Int(0)),
                                I("gt", 2, Reg(0), Reg(1)),
                                I("jump_if_false", -1, Reg(2), Lab(1)),
                                I("load_const", 3, Int(41)),
                                I("ret", -1, Reg(3)),
                                I("label", -1, Lab(1)),
                                I("load_const", 3, Int(0)),
                                I("ret", -1, Reg(3))],
                               ["bool", "int", "bool", "int"])]))))
    files.append(("ok-compare-string-word",
                  jd(DOC([MAIN([I("load_const", 0, Int(5)),
                                I("load_const", 1, Str("5")),
                                I("eq", 2, Reg(0), Reg(1)),
                                I("jump_if_false", -1, Reg(2), Lab(1)),
                                I("load_const", 3, Int(0)),
                                I("ret", -1, Reg(3)),
                                I("label", -1, Lab(1)),
                                I("load_const", 3, Int(41)),
                                I("ret", -1, Reg(3))],
                               ["int", "string", "bool", "int"])]))))
    # D-M1-31: operand order matters for the relational lowering. With the int
    # first, the int has to stay in the compare destination so `setg` reads
    # int - bool; an inverted compare answers 0 here, not 41.
    files.append(("ok-compare-int-bool",
                  jd(DOC([MAIN([I("load_const", 0, Int(1)),
                                I("load_const", 1, Sym("false")),
                                I("gt", 2, Reg(0), Reg(1)),
                                I("jump_if_false", -1, Reg(2), Lab(1)),
                                I("load_const", 3, Int(41)),
                                I("ret", -1, Reg(3)),
                                I("label", -1, Lab(1)),
                                I("load_const", 3, Int(0)),
                                I("ret", -1, Reg(3))],
                               ["int", "bool", "bool", "int"])]))))
    # D-M1-32: a list spelling is one pointer. The parameter binds, reads by
    # name, is passed, and its element comes back: 41 travels into `first` and
    # out through the return value.
    files.append(("ok-param-list-spelling",
                  jd(DOC([FN("first", [I("arg", 0, Int(0)),
                                       I("load_var", 1, Sym("xs")),
                                       I("load_const", 2, Int(0)),
                                       I("list_get", 3, Reg(1), Reg(2), ek=2),
                                       I("ret", -1, Reg(3))],
                                  ["list", "list", "int", "int"], params=["xs"],
                                  ptypes=["list<int>"]),
                           MAIN([I("list_create", 0, Int(8), Int(2)),
                                 I("load_const", 1, Int(41)),
                                 I("list_push", 2, Reg(0), Reg(1), ek=2),
                                 I("arg", -1, Reg(0)),
                                 I("call", 3, Sym("first"), Int(1)),
                                 I("ret", -1, Reg(3))],
                                ["list", "int", "void", "int"])]))))
    # D-M1-32: the C type map answers the list pointer for `List` as well
    # (c_backend.orb:475), and the builder serializes that spelling into
    # registerTypes when the source names the type that way.
    files.append(("ok-list-capital-spelling",
                  jd(DOC([MAIN([I("list_create", 0, Int(8), Int(2)),
                                I("load_const", 1, Int(41)),
                                I("list_push", 2, Reg(0), Reg(1), ek=2),
                                I("list_len", 3, Reg(0)),
                                I("load_const", 4, Int(40)),
                                I("add", 5, Reg(3), Reg(4)),
                                I("ret", -1, Reg(5))],
                               ["List", "int", "void", "int", "int", "int"])]))))
    # D-M1-33: the text is the type. Two unknown-typed constants add to 41.
    files.append(("ok-global-unknown-type",
                  jd(DOC([MAIN([I("load_var", 0, Sym("LIMIT")),
                                I("load_var", 1, Sym("NEG")),
                                I("add", 2, Reg(0), Reg(1)),
                                I("ret", -1, Reg(2))],
                               ["int", "int", "int"])],
                          globals_=[G("LIMIT", "unknown", Str("43")),
                                    G("NEG", "unknown", Str("-2"))]))))
    # D-M1-33 boundary: the parse admits the whole int32 range and stops there.
    # `2147483647` and `-2147483648` are the last texts inside it;
    # `x-global-int-overflow` and `x-global-int-underflow` are the first outside.
    files.append(("ok-global-int32-max",
                  jd(DOC([MAIN([I("load_var", 0, Sym("MAXI")),
                                I("ret", -1, Reg(0))], ["int"])],
                          globals_=[G("MAXI", "unknown", Str("2147483647"))]))))
    files.append(("ok-global-int32-min",
                  jd(DOC([MAIN([I("load_var", 0, Sym("MINI")),
                                I("ret", -1, Reg(0))], ["int"])],
                          globals_=[G("MINI", "unknown", Str("-2147483648"))]))))
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
            {"file": "../../docs/proposed-ir/m1/27-call-string-target.json",
             "run_exit": 42, "run_stdout": "string-target\r\n\r\n", "rule": "D-M1-18",
             "note": "String call targets and a void-typed destination compile and run"},
            {"file": "cases/ok-extern-body.json", "run_exit": 7, "rule": "V-18 (E-09)",
             "note": "extern body is parsed and ignored, never emitted"},
            {"file": "cases/ok-untargeted-label.json", "run_exit": 5, "rule": "E-07",
             "note": "label never targeted by a jump is accepted"},
            {"file": "cases/ok-call-arity-suffix.json", "run_exit": 9, "rule": "V-13 (E-05)",
             "note": "call consumes exactly operand2 pending args; zero consumes none"},
            {"file": "cases/ok-call-void-dest.json", "run_exit": 6, "rule": "D-M1-18",
             "note": "a void call may carry a void-typed destination (builder form)"},
            {"file": "cases/ok-model-ctor.json", "run_exit": 11, "rule": "D-M1-11",
             "note": "model constructor + field load round-trip inside main"},
            {"file": "cases/ok-union-ctor.json", "run_exit": 11, "rule": "D-M1-12",
             "note": "union constructor + tag + data round-trip inside main"},
            {"file": "cases/ok-fn-value.json", "run_exit": 42, "rule": "D-M1-14",
             "note": "function value binding + indirect call round-trip (f(41) == 42)"},
            {"file": "cases/ok-fn-extern.json", "run_exit": 98, "rule": "D-M1-14",
             "note": "extern function value binding (orbit_string_at(\"abc\", 1) == 98)"},
            {"file": "../../docs/proposed-ir/m1/28-sym-literals.json",
             "run_exit": 42, "run_stdout": "", "rule": "D-M1-19",
             "note": "emitter Sym bool/float literals compile and run"},
            {"file": "../../docs/proposed-ir/m1/29-decl-var-literals.json",
             "run_exit": 42, "run_stdout": "ok\r\n", "rule": "D-M1-19",
             "note": "emitter inline literal decl_var initializers compile and run"},
            {"file": "cases/ok-sym-bool-literal.json", "run_exit": 5, "rule": "D-M1-19",
             "note": "Sym \"true\" lowers as the bool literal and drives the branch"},
            {"file": "cases/ok-sym-float-literal.json", "run_exit": 6, "rule": "D-M1-19",
             "note": "Sym \"0.5\" and \"5e-1\" lower as equal float literals"},
            {"file": "cases/ok-decl-var-literal.json", "run_exit": 3, "rule": "D-M1-19",
             "note": "inline Int/String/Bool decl_var initializers fill the local slot"},
            {"file": "cases/ok-infer-eq-bool.json", "run_exit": 5, "rule": "D-M1-20",
             "note": "an unknown comparison destination is recovered as bool"},
            {"file": "cases/ok-infer-load-var.json", "run_exit": 3, "rule": "D-M1-20",
             "note": "an unknown load_var destination takes the local's declared type"},
            {"file": "cases/ok-infer-neg-chain.json", "run_exit": 5, "rule": "D-M1-20",
             "note": "an unknown neg destination takes its operand's type, twice"},
            {"file": "cases/ok-infer-call-return.json", "run_exit": 7, "rule": "D-M1-20",
             "note": "an unknown call destination takes the callee's return type"},
            {"file": "cases/ok-infer-arg-param.json", "run_exit": 33, "rule": "D-M1-20",
             "note": "an unknown arg binding takes the parameter's declared type"},
            {"file": "cases/ok-infer-list-create.json", "run_exit": 8, "rule": "D-M1-20",
             "note": "an unknown list_create destination is recovered as list"},
            {"file": "cases/ok-infer-union-tag.json", "run_exit": 0, "rule": "D-M1-20",
             "note": "an unknown <union>_TAG_<variant> load is recovered as int"},
            {"file": "../../docs/proposed-ir/m1/30-inferred-model-fields.json",
             "run_exit": 7, "run_stdout": "", "rule": "D-M1-20",
             "note": "the emitter stream with six unknown slots converges and runs"},
            {"file": "../../docs/proposed-ir/m1/31-inferred-unary-neg.json",
             "run_exit": 2, "run_stdout": "", "rule": "D-M1-20",
             "note": "the emitter stream for neg/add is recovered and runs"},
            {"file": "../../docs/proposed-ir/m1/32-inline-literal-operands.json",
             "run_exit": 42, "run_stdout": "lit-ok\r\n\r\n", "rule": "D-M1-21",
             "note": "the emitter stream with inline literal operands matches the oracle"},
            {"file": "cases/ok-lit-operands.json", "run_exit": 5, "rule": "D-M1-21",
             "note": "literal immediate slots and the list result scratch coexist"},
            {"file": "cases/ok-load-var-param.json", "run_exit": 42, "rule": "D-M1-22",
             "note": "a parameter read by name is the value of its arg binding"},
            {"file": "../../docs/proposed-ir/m1/33-parameter-by-name.json",
             "run_exit": 42, "run_stdout": "", "rule": "D-M1-22",
             "note": "the emitter stream reading int/string/bool/float parameters by name matches the oracle"},
            {"file": "cases/ok-add-string-spread.json", "run_exit": 42, "rule": "D-M1-23",
             "note": "a non-main int spread and a non-main bool spread concat and run"},
            {"file": "../../docs/proposed-ir/m1/34-string-spreading-add.json",
             "run_exit": 42, "run_stdout": "[spread]\r\n", "rule": "D-M1-23",
             "note": "the emitter stream spreading int/float/bool in main and in a callee matches the oracle"},
            {"file": "../../docs/proposed-ir/m1/35-list-len-bound.json",
             "run_exit": 42, "run_stdout": "", "rule": "D-M1-25",
             "note": "a list-length-bounded indexed loop in a non-main function matches the oracle"},
            {"file": "../../docs/proposed-ir/m1/36-print-builtin.json",
             "run_exit": 0, "run_stdout": "hello\r\n42\r\n1.5\r\n", "rule": "D-M1-26",
             "note": "the builtin print lowers string, int and float in main"},
            {"file": "cases/ok-print-noop.json", "run_exit": 7, "run_stdout": "",
             "rule": "D-M1-26",
             "note": "an arity other than one is the pinned no-op and prints nothing"},
            {"file": "cases/ok-print-inline-literals.json", "run_exit": 0,
             "run_stdout": "hi\r\n3\r\n", "rule": "D-M1-26",
             "note": "inline String and Int arguments print through %s and %lld"},
            {"file": "cases/ok-print-nonmain-float.json", "run_exit": 0,
             "run_stdout": "2.5\r\n", "rule": "D-M1-26",
             "note": "a float printed outside main reaches the arena and the runtime formatter"},
            {"file": "cases/ok-print-shadowed-extern.json", "run_exit": 0,
             "run_stdout": "x\r\n", "rule": "D-M1-26",
             "note": "a declared extern named print is the builtin: the int dest is never stored"},
            {"file": "../../docs/proposed-ir/m1/37-arena-extern-call.json",
             "run_exit": 0, "run_stdout": "42\r\n-7\r\n", "rule": "D-M1-27",
             "note": "an arena-taking extern is called with the arena prepended and matches the oracle"},
            {"file": "cases/ok-arena-extern-float.json", "run_exit": 0,
             "run_stdout": "2.5\r\n", "rule": "D-M1-27",
             "note": "the arena shift is positional for the xmm file too (orbit_float_to_string)"},
            {"file": "../../docs/proposed-ir/m1/38-void-ret-value.json",
             "run_exit": 42, "run_stdout": "T\r\nT\r\n", "rule": "D-M1-28",
             "note": "a void function returns without reading the value its ret carries"},
            {"file": "cases/ok-void-ret-value.json", "run_exit": 0,
             "run_stdout": "plain\r\n", "rule": "D-M1-28",
             "note": "both ret forms with a carried value match the oracle"},
            {"file": "../../docs/proposed-ir/m1/39-string-intrinsics.json",
             "run_exit": 0, "run_stdout": "5\r\n90\r\nbc\r\n", "rule": "D-M1-29",
             "note": "len/at/slice resolve through the runtime header signatures"},
            {"file": "cases/ok-list-len-string.json", "run_exit": 3,
             "run_stdout": "", "rule": "D-M1-29",
             "note": "list_len on a string receiver is orbit_string_len"},
            {"file": "cases/ok-list-create-opaque.json", "run_exit": 41,
             "run_stdout": "", "rule": "D-M1-30",
             "note": "an opaque list adopts the kind of its first typed push"},
            {"file": "../../docs/proposed-ir/m1/40-list-create-opaque.json",
             "run_exit": 0, "run_stdout": "", "rule": "D-M1-30",
             "note": "empty literal created opaque, first push adopts"},
            {"file": "cases/ok-compare-bool-word.json", "run_exit": 41,
             "run_stdout": "", "rule": "D-M1-31",
             "note": "a bool is a number to the comparison arms"},
            {"file": "cases/ok-compare-string-word.json", "run_exit": 41,
             "run_stdout": "", "rule": "D-M1-31",
             "note": "a string against a number compares as words and is false"},
            {"file": "cases/ok-compare-int-bool.json", "run_exit": 41,
             "run_stdout": "", "rule": "D-M1-31",
             "note": "int first: the int stays in the compare destination"},
            {"file": "../../docs/proposed-ir/m1/41-bool-and-word-comparisons.json",
             "run_exit": 0, "run_stdout": "", "rule": "D-M1-31",
             "note": "bool relops, bool eq, and the int/string word compare"},
            {"file": "cases/ok-param-list-spelling.json", "run_exit": 41,
             "run_stdout": "", "rule": "D-M1-32",
             "note": "a list<int> parameter binds, reads, passes, and returns its element"},
            {"file": "cases/ok-list-capital-spelling.json", "run_exit": 41,
             "run_stdout": "", "rule": "D-M1-32",
             "note": "`List` is the list pointer in a register too"},
            {"file": "../../docs/proposed-ir/m1/42-list-parameter-spellings.json",
             "run_exit": 0, "run_stdout": "", "rule": "D-M1-32",
             "note": "list<int>/list<string> parameters and a list<int> return match the oracle"},
            {"file": "cases/ok-global-unknown-type.json", "run_exit": 41,
             "run_stdout": "", "rule": "D-M1-33",
             "note": "an unknown-typed decimal global is an int constant"},
            {"file": "cases/ok-global-int32-max.json", "run_exit": 2147483647,
             "run_stdout": "", "rule": "D-M1-33",
             "note": "the largest int32 text builds and is the whole exit code"},
            {"file": "cases/ok-global-int32-min.json", "run_exit": 2147483648,
             "run_stdout": "", "rule": "D-M1-33",
             "note": "the smallest int32 text builds; the exit code is its 32-bit pattern"},
            {"file": "../../docs/proposed-ir/m1/43-global-constant-type.json",
             "run_exit": 0, "run_stdout": "32\r\n3\r\n-2\r\n", "rule": "D-M1-33",
             "note": "three corpus-shaped globals print and compare as ints"},
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
