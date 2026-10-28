#!/usr/bin/env python3
"""Renders the two IR documents of the runtime-module end-to-end check (D-M1-106).

probe-rt.json is a runtime module with two exports, written against the byte
opcodes: rtmod_probe_upper(s: string) -> string and rtmod_probe_sum(s: string)
-> int. probe-main.json is an ordinary program that declares both as externs and
prints their results. `--check` compares the committed files with a fresh render.
"""

import argparse
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "reject"))

from make_cases import FN, I, Int, Lab, MAIN, Reg, RTDOC, DOC, Str, jd  # noqa: E402


def upper():
    # r0 s, r1 n, r2 out, r3 i, r4 loop test, r5 byte, r6 'a', r7 byte >= 'a',
    # r8 'z', r9 byte <= 'z', r10 32, r11 converted byte, r12 1, r13 i + 1
    types = ["string", "int", "string", "int", "bool", "int", "int", "bool",
             "int", "bool", "int", "int", "int", "int"]
    ins = [
        I("arg", 0, Int(0)),
        I("str_len", 1, Reg(0)),
        I("bytes_alloc", 2, Reg(1)),
        I("load_const", 3, Int(0)),
        I("load_const", 6, Int(97)),
        I("load_const", 8, Int(122)),
        I("load_const", 10, Int(32)),
        I("load_const", 12, Int(1)),
        I("label", -1, Lab(1)),
        I("lt", 4, Reg(3), Reg(1)),
        I("jump_if_false", -1, Reg(4), Lab(2)),
        I("byte_load", 5, Reg(0), Reg(3)),
        I("copy", 11, Reg(5)),
        I("ge", 7, Reg(5), Reg(6)),
        I("jump_if_false", -1, Reg(7), Lab(3)),
        I("le", 9, Reg(5), Reg(8)),
        I("jump_if_false", -1, Reg(9), Lab(3)),
        I("sub", 11, Reg(5), Reg(10)),
        I("label", -1, Lab(3)),
        I("byte_store", -1, Reg(2), Reg(3), Reg(11)),
        I("add", 13, Reg(3), Reg(12)),
        I("copy", 3, Reg(13)),
        I("jump", -1, Lab(1)),
        I("label", -1, Lab(2)),
        I("ret", -1, Reg(2)),
    ]
    return FN("rtmod_probe_upper", ins, types, rtype="string", params=["s"],
              ptypes=["string"], export=True)


def total():
    # r0 s, r1 n, r2 i, r3 sum, r4 1, r5 loop test, r6 byte, r7 sum + byte, r8 i + 1
    types = ["string", "int", "int", "int", "int", "bool", "int", "int", "int"]
    ins = [
        I("arg", 0, Int(0)),
        I("str_len", 1, Reg(0)),
        I("load_const", 2, Int(0)),
        I("load_const", 3, Int(0)),
        I("load_const", 4, Int(1)),
        I("label", -1, Lab(1)),
        I("lt", 5, Reg(2), Reg(1)),
        I("jump_if_false", -1, Reg(5), Lab(2)),
        I("byte_load", 6, Reg(0), Reg(2)),
        I("add", 7, Reg(3), Reg(6)),
        I("copy", 3, Reg(7)),
        I("add", 8, Reg(2), Reg(4)),
        I("copy", 2, Reg(8)),
        I("jump", -1, Lab(1)),
        I("label", -1, Lab(2)),
        I("ret", -1, Reg(3)),
    ]
    return FN("rtmod_probe_sum", ins, types, rtype="int", params=["s"],
              ptypes=["string"], export=True)


def lower_core():
    # r0 arena, r1 s, r2 n, r3 out, r4 i, r5 loop test, r6 byte, r7 'A', r8 byte >= 'A',
    # r9 'Z', r10 byte <= 'Z', r11 32, r12 converted byte, r13 1, r14 i + 1
    types = ["RtArena", "string", "int", "string", "int", "bool", "int", "int", "bool",
             "int", "bool", "int", "int", "int", "int"]
    ins = [
        I("arg", 0, Int(0)),
        I("arg", 1, Int(1)),
        I("str_len", 2, Reg(1)),
        I("bytes_alloc", 3, Reg(2), Reg(0)),
        I("load_const", 4, Int(0)),
        I("load_const", 7, Int(65)),
        I("load_const", 9, Int(90)),
        I("load_const", 11, Int(32)),
        I("load_const", 13, Int(1)),
        I("label", -1, Lab(1)),
        I("lt", 5, Reg(4), Reg(2)),
        I("jump_if_false", -1, Reg(5), Lab(2)),
        I("byte_load", 6, Reg(1), Reg(4)),
        I("copy", 12, Reg(6)),
        I("ge", 8, Reg(6), Reg(7)),
        I("jump_if_false", -1, Reg(8), Lab(3)),
        I("le", 10, Reg(6), Reg(9)),
        I("jump_if_false", -1, Reg(10), Lab(3)),
        I("add", 12, Reg(6), Reg(11)),
        I("label", -1, Lab(3)),
        I("byte_store", -1, Reg(3), Reg(4), Reg(12)),
        I("add", 14, Reg(4), Reg(13)),
        I("copy", 4, Reg(14)),
        I("jump", -1, Lab(1)),
        I("label", -1, Lab(2)),
        I("ret", -1, Reg(3)),
    ]
    return FN("rtmod_lower_core", ins, types, rtype="string", params=["a", "s"],
              ptypes=["RtArena", "string"])


def lower():
    # Forwards the caller's arena to a file-local helper.
    types = ["RtArena", "string", "string"]
    ins = [
        I("arg", 0, Int(0)),
        I("arg", 1, Int(1)),
        I("arg", -1, Reg(0)),
        I("arg", -1, Reg(1)),
        I("call", 2, Str("rtmod_lower_core"), Int(2)),
        I("ret", -1, Reg(2)),
    ]
    return FN("rtmod_probe_lower", ins, types, rtype="string", params=["a", "s"],
              ptypes=["RtArena", "string"], export=True)


def program():
    ext_upper = FN("rtmod_probe_upper", [], [], rtype="string", extern=True,
                   params=["s"], ptypes=["string"])
    ext_sum = FN("rtmod_probe_sum", [], [], rtype="int", extern=True,
                 params=["s"], ptypes=["string"])
    ext_lower = FN("rtmod_probe_lower", [], [], rtype="string", extern=True,
                   params=["s"], ptypes=["string"])
    main = MAIN([
        I("arg", -1, Str("hello, Orbit 42!")),
        I("call", 0, Str("rtmod_probe_upper"), Int(1)),
        I("arg", -1, Reg(0)),
        I("call", 1, Str("print"), Int(1)),
        I("arg", -1, Str("")),
        I("call", 2, Str("rtmod_probe_upper"), Int(1)),
        I("arg", -1, Reg(2)),
        I("call", 3, Str("print"), Int(1)),
        I("arg", -1, Str("hé")),
        I("call", 4, Str("rtmod_probe_sum"), Int(1)),
        I("arg", -1, Reg(4)),
        I("call", 5, Str("print"), Int(1)),
        I("arg", -1, Str("Hello, ORBIT 42!")),
        I("call", 6, Str("rtmod_probe_lower"), Int(1)),
        I("arg", -1, Reg(6)),
        I("call", 7, Str("print"), Int(1)),
        I("ret", -1, Int(0)),
    ], ["string", "void", "string", "void", "int", "void", "string", "void"])
    return DOC([ext_upper, ext_sum, ext_lower, main])


def render():
    return {HERE / "probe-rt.json": jd(RTDOC([upper(), total(), lower(), lower_core()])),
            HERE / "probe-main.json": jd(program())}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    files = render()
    if args.check:
        bad = [p.name for p, data in files.items()
               if not p.exists() or p.read_bytes() != data]
        if bad:
            print("FAIL drift: " + ", ".join(bad))
            return 1
        print("runtime-module probes match the tree")
        return 0
    for path, data in files.items():
        path.write_bytes(data)
    print("wrote " + ", ".join(p.name for p in files))
    return 0


if __name__ == "__main__":
    sys.exit(main())
