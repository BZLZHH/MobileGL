#!/usr/bin/env python3
"""
MobileGL - scripts/generate_wire_client.py
Copyright (c) 2025-2026 MobileGL-Dev
Licensed under the GNU Lesser General Public License v3.0:
  https://www.gnu.org/licenses/gpl-3.0.txt
  https://www.gnu.org/licenses/lgpl-3.0.txt
SPDX-License-Identifier: LGPL-3.0-only
End of Source File Header

Generate MobileGL/MG_Client/generated_wire_client.h: typed Send* wrappers
over Client::SendWirePayload for every GL API whose Gen* payload table can be
built from scalar/string/vector/ShmRegion arguments. APIs that the server
dispatch generator registers as unsupported are skipped here too, so the
client trampoline set is exactly the supported subset and the coverage test
can report the same gap.
"""
from __future__ import annotations

import re
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
DEFINITIONS = REPO_ROOT / "MobileGL/MG_Impl/GLImpl/Exporting/Definitions.cpp"
EGL_DEFINITIONS = REPO_ROOT / "MobileGL/MG_Impl/EGLImpl/Exporting/Definitions.cpp"
WIRE_FULL = REPO_ROOT / "MobileGL/MG_Protocol/wire_full.fbs"
OPCODE_H = REPO_ROOT / "MobileGL/MG_Protocol/generated_opcodes.h"
DISPATCH_H = REPO_ROOT / "MobileGL/MG_Protocol/generated_dispatch.h"
OUT_H = REPO_ROOT / "MobileGL/MG_Client/generated_wire_client.h"

HEADER_RE = re.compile(
    r"DECLARE_GL_FUNCTION_(STUB_)?HEAD\s*\(\s*([^,]+)\s*,\s*([A-Za-z0-9_]+)\s*,\s*(.*?)\)\s*"
    r"DECLARE_GL_FUNCTION_(?:STUB_)?END(?:_NO_RETURN)?\s*\(",
    re.DOTALL,
)

# EGL functions are plain `MOBILEGL_EGL_API <ret> egl*<Name>(args) {` bodies.
EGL_FUNC_RE = re.compile(
    r"MOBILEGL_EGL_API\s+(.+?)\s+(egl[A-Za-z0-9_]+)\s*\(([^)]*)\)\s*\{",
    re.DOTALL,
)

TABLE_RE = re.compile(r"^\s*table\s+(\w+)\s*\{", re.MULTILINE)
FIELD_RE = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_]*):\s*([A-Za-z0-9_\[\]]+);", re.MULTILINE)

SCALAR_CPP = {
    "int": "int32_t",
    "uint": "uint32_t",
    "float": "float",
    "double": "double",
    "bool": "bool",
    "long": "int64_t",
    "ulong": "uint64_t",
}
VECTOR_CPP = {
    "[int]": "int32_t",
    "[uint]": "uint32_t",
    "[float]": "float",
    "[double]": "double",
    "[bool]": "bool",
    "[long]": "int64_t",
    "[ulong]": "uint64_t",
}


def collect_tables(text: str) -> dict[str, dict[str, str]]:
    tables: dict[str, dict[str, str]] = {}
    pos = 0
    while True:
        match = TABLE_RE.search(text, pos)
        if match is None:
            break
        name = match.group(1)
        start = match.start()
        brace_index = match.end() - 1
        depth = 0
        cursor = brace_index
        while cursor < len(text):
            ch = text[cursor]
            if ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
                if depth == 0:
                    break
            cursor += 1
        block = text[start : cursor + 1]
        fields: dict[str, str] = {}
        for field_match in FIELD_RE.finditer(block):
            fields[field_match.group(1)] = field_match.group(2)
        tables[name] = fields
        pos = cursor + 1
    return tables


def parse_args(arglist: str) -> list[tuple[str, str]]:
    if not arglist.strip():
        return []
    args: list[tuple[str, str]] = []
    depth = 0
    start = 0
    for i, ch in enumerate(arglist):
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        elif ch == "," and depth == 0:
            part = arglist[start:i].strip()
            if part:
                args.append(parse_one_arg(part))
            start = i + 1
    tail = arglist[start:].strip()
    if tail:
        args.append(parse_one_arg(tail))
    return args


def parse_one_arg(part: str) -> tuple[str, str]:
    trimmed = part.strip()
    name_match = re.search(r"([A-Za-z_][A-Za-z0-9_]*)\s*$", trimmed)
    if name_match is None:
        return trimmed, "arg"
    name = name_match.group(1)
    return trimmed[: name_match.start()].strip(), name


def build_param_and_expr(fbs_type: str, arg_name: str, index: int) -> tuple[str, str, str, bool]:
    """Return (param_decl, create_expr, extra_suffix, ok)."""
    if fbs_type in SCALAR_CPP:
        cpp = SCALAR_CPP[fbs_type]
        return f"{cpp} {arg_name}", arg_name, "", True
    if fbs_type == "string":
        return f"const char* {arg_name}", f"builder.CreateString({arg_name})", "", True
    if fbs_type in VECTOR_CPP:
        cpp = VECTOR_CPP[fbs_type]
        return f"const std::vector<{cpp}>& {arg_name}", f"builder.CreateVector({arg_name})", "", True
    if fbs_type == "ShmRegion":
        return "MobileGLShmHandle* shm", \
               "MobileGL::Protocol::WireFull::CreateShmRegion(builder, 0, shm == nullptr ? 0 : shm->size)", \
               "", True
    return "", "", "", False


def main() -> int:
    if not all(p.exists() for p in (DEFINITIONS, WIRE_FULL, OPCODE_H, DISPATCH_H)):
        print("missing input", file=__import__("sys").stderr)
        return 1

    opcode_by_name: dict[str, int] = {}
    for line in OPCODE_H.read_text(encoding="utf-8").splitlines():
        match = re.match(r"\s*(\w+)\s*=\s*(\d+),", line)
        if match:
            opcode_by_name[match.group(1)] = int(match.group(2))

    dispatch_entries: list[tuple[int, str, str]] = []
    for line in DISPATCH_H.read_text(encoding="utf-8").splitlines():
        match = re.match(r"\s*\{(\d+)u,\s*\"(\w+)\",\s*\"(\w*)\"\},\s*$", line)
        if match:
            dispatch_entries.append((int(match.group(1)), match.group(2), match.group(3)))

    wire_tables = collect_tables(WIRE_FULL.read_text(encoding="utf-8"))

    definitions_text = DEFINITIONS.read_text(encoding="utf-8")
    definitions_text = "\n".join(
        line for line in definitions_text.splitlines()
        if not line.lstrip().startswith("#define") and not line.lstrip().startswith("//")
    )
    api_args: dict[str, tuple[str, list[tuple[str, str]], bool]] = {}
    for match in HEADER_RE.finditer(definitions_text):
        stub = match.group(1) is not None
        c_type, name, arglist = match.group(2).strip(), match.group(3).strip(), match.group(4)
        api_args[name] = (c_type, parse_args(arglist), stub)

    egl_api_args: dict[str, tuple[str, list[tuple[str, str]], bool]] = {}
    if EGL_DEFINITIONS.exists():
        egl_text = EGL_DEFINITIONS.read_text(encoding="utf-8")
        egl_text = "\n".join(
            line for line in egl_text.splitlines()
            if not line.lstrip().startswith("//")
        )
        for match in EGL_FUNC_RE.finditer(egl_text):
            ret_type, name, arglist = match.group(1).strip(), match.group(2).strip(), match.group(3)
            egl_api_args[name[3:]] = (ret_type, parse_args(arglist), False)

    functions: list[str] = []
    wrapped: set[int] = set()
    for opcode, api, _table in dispatch_entries:
        if not api.startswith(("gl", "egl")) or opcode in wrapped:
            continue
        if api.startswith("egl"):
            name = api[3:]
            entry = egl_api_args.get(name)
            fn_name = "SendEgl" + name
        else:
            name = api[2:]
            entry = api_args.get(name)
            fn_name = "SendGl" + name
        if entry is None:
            continue
        _, args, _stub = entry
        if api.startswith("egl"):
            gen_table = "GenEgl" + name
        else:
            gen_table = "Gen" + name
        fields = wire_tables.get(gen_table)
        if fields is None:
            continue
        # Reuse the same supportedness decision as the server dispatch: a
        # payload that cannot be decoded server-side is not wrapped client-side.
        params: list[str] = []
        create_exprs: list[str] = []
        shm_count = 0
        has_out_vector = False
        ok = True
        for c_type, arg_name in args:
            if c_type.count("*") > 1:
                ok = False
                break
            fbs_type = fields.get(arg_name)
            if fbs_type is None:
                ok = False
                break
            if fbs_type == "ShmRegion":
                shm_count += 1
                if shm_count > 1:
                    ok = False
                    break
                create_exprs.append(
                    "MobileGL::Protocol::WireFull::CreateShmRegion("
                    "builder, 0, shm == nullptr ? 0 : shm->size)"
                )
                continue
            if fbs_type.startswith("[") and "const" not in c_type:
                # Out-vector: values are not sent; the results come back in
                # Response.ret_bytes and the trampoline copies them out.
                # Keep Create* argument positions aligned with the table's
                # field order: the out field stays null (0 offset).
                has_out_vector = True
                create_exprs.append("0")
                continue
            param_decl, expr, _, param_ok = build_param_and_expr(fbs_type, arg_name, len(params))
            if not param_ok:
                ok = False
                break
            params.append(param_decl)
            create_exprs.append(expr)
        if not ok:
            continue
        params.append("Uint64 token")
        if shm_count > 0:
            params.append("MobileGLShmHandle* shm")
        else:
            params.append("MobileGLShmHandle* shm = nullptr")
        if has_out_vector:
            params.append("Uint32 outCapacity = 0")
        opcode_expr = f"static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::{api})"
        extra_call = ", outCapacity" if has_out_vector else ""
        body = (
            f"    inline Bool {fn_name}(Uint64 sessionId, {', '.join(params)}) {{\n"
            f"        flatbuffers::FlatBufferBuilder builder;\n"
            f"        auto payload = MobileGL::Protocol::WireFull::Create{gen_table}(builder"
            + (", " + ", ".join(create_exprs) if create_exprs else "") +
            ");\n"
            f"        builder.Finish(payload);\n"
            f"        return MobileGL::Client::SendWirePayload(sessionId, {opcode_expr}, token,\n"
            f"                                 builder.GetBufferPointer(),\n"
            f"                                 static_cast<Uint32>(builder.GetSize()), shm{extra_call});\n"
            f"    }}\n"
        )
        functions.append(body)
        wrapped.add(opcode)

    header = (
        "// MobileGL - MobileGL/MG_Client/generated_wire_client.h\n"
        "// Copyright (c) 2025-2026 MobileGL-Dev\n"
        "// Licensed under the GNU Lesser General Public License v3.0:\n"
        "// SPDX-License-Identifier: LGPL-3.0-only\n"
        "// End of Source File Header\n\n"
        "#pragma once\n\n"
        "#include <vector>\n"
        "#include \"MG_Client/Client.h\"\n"
        "#include \"MG_Protocol/generated_opcodes.h\"\n"
        "#include \"MG_Protocol/gen/wire_full_generated.h\"\n\n"
        "// GENERATED by scripts/generate_wire_client.py\n\n"
        "namespace MobileGL::Client::Wire {\n\n"
        f"inline constexpr uint32_t kWireClientWrappedCount = {len(functions)}u;\n\n"
        + "\n".join(functions) +
        "\n} // namespace MobileGL::Client::Wire\n\n"
        "// End of File\n"
    )
    OUT_H.write_text(header, encoding="utf-8")
    print(f"Wrote {OUT_H.name}: {len(functions)} Send* wrappers")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
