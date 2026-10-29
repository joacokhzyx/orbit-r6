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

from make_cases import FN, Flt, I, Int, Lab, MAIN, Reg, RTDOC, DOC, Str, jd  # noqa: E402


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


def slice_():
    # r0 arena, r1 s, r2 start, r3 len, r4 s + start, r5 out
    types = ["RtArena", "string", "int", "int", "string", "string"]
    ins = [
        I("arg", 0, Int(0)), I("arg", 1, Int(1)), I("arg", 2, Int(2)), I("arg", 3, Int(3)),
        I("str_offset", 4, Reg(1), Reg(2)),
        I("bytes_alloc", 5, Reg(3), Reg(0)),
        I("bytes_copy", -1, Reg(5), Reg(4), Reg(3)),
        I("ret", -1, Reg(5)),
    ]
    return FN("rtmod_probe_slice", ins, types, rtype="string",
              params=["a", "s", "start", "len"],
              ptypes=["RtArena", "string", "int", "int"], export=True)


def guard():
    # A negative count copies nothing: the buffer keeps "x".
    # r0 s, r1 buffer, r2 0, r3 'x', r4 1, r5 NUL, r6 -5
    types = ["string", "string", "int", "int", "int", "int", "int"]
    ins = [
        I("arg", 0, Int(0)),
        I("load_const", 4, Int(1)), I("load_const", 2, Int(0)),
        I("load_const", 3, Int(120)), I("load_const", 5, Int(0)),
        I("load_const", 6, Int(-5)),
        I("bytes_alloc", 1, Reg(4)),
        I("byte_store", -1, Reg(1), Reg(2), Reg(3)),
        I("byte_store", -1, Reg(1), Reg(4), Reg(5)),
        I("bytes_copy", -1, Reg(1), Reg(0), Reg(6)),
        I("ret", -1, Reg(1)),
    ]
    return FN("rtmod_probe_guard", ins, types, rtype="string", params=["s"],
              ptypes=["string"], export=True)


def cell():
    # D-M1-117. A 40-byte cell from rt_alloc: x at +0 (word), -3 at +8 (32 bits),
    # the cell's own address at +24 (pointer). The reads go through the stored
    # pointer. A negative x is an error with code 9; mode 1 returns the pointer.
    # r0 arena, r1 x, r2 mode, r3 40, r4 buffer, r5 cell, r6 0, r7 8, r8 24, r9 -3,
    # r10 stored pointer, r11 x read back, r12 -3 read back, r13 sum, r14 0, r15 x < 0,
    # r16 code, r17 message, r18 mode == 1, r19 1, r20 error, r21 ok pointer, r22 ok int
    types = ["RtArena", "int", "int", "int", "string", "RtPtr", "int", "int", "int", "int",
             "RtPtr", "int", "int", "int", "int", "bool", "int", "string", "bool", "int",
             "result", "result", "result"]
    ins = [
        I("arg", 0, Int(0)), I("arg", 1, Int(1)), I("arg", 2, Int(2)),
        I("load_const", 3, Int(40)),
        I("bytes_alloc", 4, Reg(3), Reg(0)),
        I("reinterpret", 5, Reg(4)),
        I("load_const", 6, Int(0)), I("load_const", 7, Int(8)),
        I("load_const", 8, Int(24)), I("load_const", 9, Int(-3)),
        I("mem_store", -1, Reg(5), Reg(6), Reg(1), ek=8),
        I("mem_store", -1, Reg(5), Reg(7), Reg(9), ek=4),
        I("mem_store", -1, Reg(5), Reg(8), Reg(5), ek=8),
        I("mem_load", 10, Reg(5), Reg(8), ek=8),
        I("mem_load", 11, Reg(10), Reg(6), ek=8),
        I("mem_load", 12, Reg(5), Reg(7), ek=4),
        I("add", 13, Reg(11), Reg(12)),
        I("load_const", 14, Int(0)),
        I("lt", 15, Reg(1), Reg(14)),
        I("jump_if_false", -1, Reg(15), Lab(1)),
        I("load_const", 16, Int(9)), I("load_const", 17, Str("negative")),
        I("result_err", 20, Reg(16), Reg(17)),
        I("ret", -1, Reg(20)),
        I("label", -1, Lab(1)),
        I("load_const", 19, Int(1)),
        I("eq", 18, Reg(2), Reg(19)),
        I("jump_if_false", -1, Reg(18), Lab(2)),
        I("result_ok", 21, Reg(10)),
        I("ret", -1, Reg(21)),
        I("label", -1, Lab(2)),
        I("result_ok", 22, Reg(13)),
        I("ret", -1, Reg(22)),
    ]
    return FN("rtmod_probe_cell", ins, types, rtype="result", params=["a", "x", "mode"],
              ptypes=["RtArena", "int", "int"], export=True)


def float_cell():
    # D-M1-117. A float goes through memory as its bit pattern and comes back as a
    # float in an ok result.
    # r0 arena, r1 y, r2 8, r3 buffer, r4 cell, r5 0, r6 bits, r7 bits read back,
    # r8 float read back, r9 result
    types = ["RtArena", "float", "int", "string", "RtPtr", "int", "int", "int", "float",
             "result"]
    ins = [
        I("arg", 0, Int(0)), I("arg", 1, Int(1)),
        I("load_const", 2, Int(8)),
        I("bytes_alloc", 3, Reg(2), Reg(0)),
        I("reinterpret", 4, Reg(3)),
        I("load_const", 5, Int(0)),
        I("float_bits", 6, Reg(1)),
        I("mem_store", -1, Reg(4), Reg(5), Reg(6), ek=8),
        I("mem_load", 7, Reg(4), Reg(5), ek=8),
        I("bits_float", 8, Reg(7)),
        I("result_ok", 9, Reg(8)),
        I("ret", -1, Reg(9)),
    ]
    return FN("rtmod_probe_float", ins, types, rtype="result", params=["a", "y"],
              ptypes=["RtArena", "float"], export=True)


def bits():
    types = ["float", "int"]
    ins = [I("arg", 0, Int(0)), I("float_bits", 1, Reg(0)), I("ret", -1, Reg(1))]
    return FN("rtmod_probe_bits", ins, types, rtype="int", params=["y"],
              ptypes=["float"], export=True)


def program():
    ext_upper = FN("rtmod_probe_upper", [], [], rtype="string", extern=True,
                   params=["s"], ptypes=["string"])
    ext_sum = FN("rtmod_probe_sum", [], [], rtype="int", extern=True,
                 params=["s"], ptypes=["string"])
    ext_lower = FN("rtmod_probe_lower", [], [], rtype="string", extern=True,
                   params=["s"], ptypes=["string"])
    ext_copy = FN("rtmod_probe_copy_in", [], [], rtype="string", extern=True,
                  params=["s"], ptypes=["string"])
    ext_slice = FN("rtmod_probe_slice", [], [], rtype="string", extern=True,
                   params=["s", "start", "len"], ptypes=["string", "int", "int"])
    ext_guard = FN("rtmod_probe_guard", [], [], rtype="string", extern=True,
                   params=["s"], ptypes=["string"])
    ext_cell = FN("rtmod_probe_cell", [], [], rtype="result", extern=True,
                  params=["x", "mode"], ptypes=["int", "int"])
    ext_float = FN("rtmod_probe_float", [], [], rtype="result", extern=True,
                   params=["y"], ptypes=["float"])
    ext_bits = FN("rtmod_probe_bits", [], [], rtype="int", extern=True,
                  params=["y"], ptypes=["float"])
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
        I("arg", -1, Str("copy me")),
        I("call", 8, Str("rtmod_probe_copy_in"), Int(1)),
        I("arg", -1, Reg(8)),
        I("call", 9, Str("print"), Int(1)),
        I("load_const", 10, Int(7)), I("load_const", 11, Int(5)),
        I("arg", -1, Str("hello, Orbit 42")), I("arg", -1, Reg(10)), I("arg", -1, Reg(11)),
        I("call", 12, Str("rtmod_probe_slice"), Int(3)),
        I("arg", -1, Reg(12)), I("call", 13, Str("print"), Int(1)),
        I("arg", -1, Str("abc")),
        I("call", 14, Str("rtmod_probe_guard"), Int(1)),
        I("arg", -1, Reg(14)), I("call", 15, Str("print"), Int(1)),
        # D-M1-117
        I("load_const", 16, Int(5)), I("load_const", 17, Int(0)),
        I("arg", -1, Reg(16)), I("arg", -1, Reg(17)),
        I("call", 18, Str("rtmod_probe_cell"), Int(2)),
        I("result_unwrap", 19, Reg(18)),
        I("arg", -1, Reg(19)), I("call", 20, Str("print"), Int(1)),
        I("load_const", 21, Int(-1)),
        I("arg", -1, Reg(21)), I("arg", -1, Reg(17)),
        I("call", 22, Str("rtmod_probe_cell"), Int(2)),
        I("result_unwrap_err", 23, Reg(22)),
        I("arg", -1, Reg(23)), I("call", 24, Str("print"), Int(1)),
        I("load_const", 25, Int(1)),
        I("arg", -1, Reg(16)), I("arg", -1, Reg(25)),
        I("call", 26, Str("rtmod_probe_cell"), Int(2)),
        I("result_is_ok", 27, Reg(26)),
        I("arg", -1, Reg(27)), I("call", 28, Str("print"), Int(1)),
        I("load_const", 29, Flt(-2.5)),
        I("arg", -1, Reg(29)), I("call", 30, Str("rtmod_probe_float"), Int(1)),
        I("result_unwrap", 31, Reg(30)),
        I("arg", -1, Reg(31)), I("call", 32, Str("print"), Int(1)),
        I("load_const", 33, Flt(1.5)),
        I("arg", -1, Reg(33)), I("call", 34, Str("rtmod_probe_bits"), Int(1)),
        I("arg", -1, Reg(34)), I("call", 35, Str("print"), Int(1)),
        I("load_const", 36, Flt(-0.0)),
        I("arg", -1, Reg(36)), I("call", 37, Str("rtmod_probe_bits"), Int(1)),
        I("arg", -1, Reg(37)), I("call", 38, Str("print"), Int(1)),
        I("ret", -1, Int(0)),
    ], ["string", "void", "string", "void", "int", "void", "string", "void", "string", "void",
        "int", "int", "string", "void", "string", "void",
        "int", "int", "result", "int", "void", "int", "result", "string", "void", "int",
        "result", "bool", "void", "float", "result", "float", "void", "float", "int", "void",
        "float", "int", "void"])
    return DOC([ext_upper, ext_sum, ext_lower, ext_copy, ext_slice, ext_guard, ext_cell,
                ext_float, ext_bits, main])


def render():
    return {HERE / "probe-rt.json": jd(RTDOC([upper(), total(), lower(), lower_core(), slice_(), guard(),
                                            cell(), float_cell(), bits()])),
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
