#!/usr/bin/env python3
"""One-shot D-F0-06 converter: R6 draft serialization -> language-side IRv1.

Reads the hand-written fixtures in docs/proposed-ir/m1/*.json (mint format
`{"format":"orbit-ir","version":1,"module":...}`) and rewrites them into the
adopted `orbit-lang/docs/IR_SPEC.md` format (root `irVersion` + `module`,
`kind`/`value` operand objects, flat route fields, JSON numbers for ints and
floats, `call` operand2 carrying the arity recorded by the builder).

The conversion is mechanical: every float64_bits literal is decoded and
re-encoded as the shortest round-trip JSON number, and the written file is
re-read to prove the bits survive. No .orb file is touched.

`--check` verifies the working tree against a fresh conversion of HEAD.
"""

import argparse
import json
import math
import struct
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
FIXTURES = REPO / "docs" / "proposed-ir" / "m1"


def convert_value(v: dict) -> dict:
    keys = set(v.keys())
    if keys == {"none"}:
        if v["none"] is not None:
            raise ValueError("none payload must be null")
        return {"kind": "None"}
    if keys == {"float64_bits"}:
        text = v["float64_bits"]
        if len(text) != 16 or text != text.lower():
            raise ValueError(f"bad float64_bits {text!r}")
        f = struct.unpack(">d", bytes.fromhex(text))[0]
        if not math.isfinite(f):
            raise ValueError(f"non-finite float64_bits {text!r} (spec V-08)")
        out = {"kind": "Float", "value": f}
        if struct.pack(">d", f).hex() != text:
            raise ValueError(f"float round-trip changed bits for {text!r}")
        return out
    if keys == {"int"}:
        n = int(v["int"], 10)
        if not -(2**31) <= n <= 2**31 - 1:
            raise ValueError(f"int out of range: {v['int']!r}")
        return {"kind": "Int", "value": n}
    if keys == {"reg"}:
        return {"kind": "Register", "value": int(v["reg"])}
    if keys == {"label"}:
        return {"kind": "Label", "value": int(v["label"])}
    if keys == {"string"}:
        return {"kind": "String", "value": v["string"]}
    if keys == {"symbol"}:
        return {"kind": "Sym", "value": v["symbol"]}
    if keys == {"bool"}:
        return {"kind": "Bool", "value": bool(v["bool"])}
    raise ValueError(f"unknown draft value tag: {sorted(keys)}")


def convert_function(f: dict) -> dict:
    out = {
        "name": f["name"],
        "params": f["params"],
        "paramTypes": f["param_types"],
        "instructions": [],
        "registerCount": f["register_count"],
        "registerTypes": f["register_types"],
        "returnType": f["return_type"],
        "isExtern": f["extern"],
    }
    route = f["route"]
    if route is None:
        out.update(routeMethod="", routePath="", routeHasLimit=False,
                   routeLimitRate=0, routeLimitWindowMs=0, routeLimitBurst=0)
    else:
        out.update(routeMethod=route["method"], routePath=route["path"],
                   routeHasLimit=route["has_limit"], routeLimitRate=route["limit_rate"],
                   routeLimitWindowMs=route["limit_window_ms"],
                   routeLimitBurst=route["limit_burst"])
    pending = 0
    for ins in f["instructions"]:
        op = ins["op"]
        operands = [convert_value(v) for v in ins["operands"]]
        if len(operands) != 3:
            raise ValueError(f"{op}: expected 3 operands")
        # V-13/E-05: the draft fixtures always let a call consume every pending
        # arg, so the recorded arity is the pending count at the call.
        arity = pending if op == "call" else None
        if op == "call":
            operands[1] = {"kind": "Int", "value": arity}
        out["instructions"].append({
            "opcode": op,
            "dest": -1 if ins["dest"] is None else ins["dest"],
            "operand1": operands[0],
            "operand2": operands[1],
            "operand3": operands[2],
            "elemKind": ins["elem_kind"],
        })
        if op == "arg" and ins["dest"] is None:
            pending += 1
        elif op == "call":
            pending -= arity
    return out


def convert_document(doc: dict) -> dict:
    if doc.get("format") != "orbit-ir" or doc.get("version") != 1:
        raise ValueError("not a draft serialization")
    m = doc["module"]
    return {
        "irVersion": "1",
        "module": {
            "functions": [convert_function(f) for f in m["functions"]],
            "globals": [{"name": g["name"], "value": convert_value(g["value"]),
                         "typeName": g["type_name"]} for g in m["globals"]],
            "models": [{"name": d["name"],
                        "fields": [{"name": x["name"], "typeName": x["type_name"]}
                                   for x in d["fields"]],
                        "typeParams": d["type_params"]} for d in m["models"]],
            "types": [{"name": t["name"], "kind": t["kind"], "variants": t["variants"],
                       "richVariants": t["rich_variants"], "methods": t["methods"]}
                      for t in m["types"]],
            "migrations": m["migrations"],
        },
    }


def normalize(doc: dict) -> str:
    return json.dumps(doc, sort_keys=True, separators=(",", ":"))


def render(doc: dict, pretty: bool) -> str:
    if pretty:
        return json.dumps(doc, indent=2, ensure_ascii=True) + "\n"
    return json.dumps(doc, separators=(",", ":"), ensure_ascii=True) + "\n"


def head_text(path: Path) -> str:
    rel = path.relative_to(REPO).as_posix()
    return subprocess.run(["git", "-C", str(REPO), "show", f"HEAD:{rel}"],
                          check=True, capture_output=True, text=True).stdout


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--check", action="store_true",
                    help="verify the working tree equals a fresh conversion of HEAD")
    args = ap.parse_args()
    files = sorted(FIXTURES.glob("*.json"))
    if not files:
        print("no fixtures found", file=sys.stderr)
        return 1
    converted = []
    for path in files:
        working = path.read_text(encoding="utf-8")
        if args.check:
            new = convert_document(json.loads(head_text(path)))
            if normalize(json.loads(working)) != normalize(new):
                print(f"FAIL {path.name}: working tree differs from a fresh conversion of HEAD")
                return 1
            converted.append(path.name)
            continue
        pretty = "\n" in working.rstrip("\n")
        new = convert_document(json.loads(working))
        with open(path, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(render(new, pretty))
        # Prove the written text re-reads to the same values (float bits included).
        if normalize(json.loads(path.read_text(encoding="utf-8"))) != normalize(new):
            print(f"FAIL {path.name}: re-read mismatch after write", file=sys.stderr)
            return 1
        converted.append(path.name)
    verb = "checked" if args.check else "converted"
    print(f"{verb} {len(converted)} fixtures: {' '.join(n.split('-')[0] for n in converted)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
