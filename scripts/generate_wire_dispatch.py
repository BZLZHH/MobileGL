#!/usr/bin/env python3
"""
MobileGL - scripts/generate_wire_dispatch.py
Copyright (c) 2025-2026 MobileGL-Dev
Licensed under the GNU Lesser General Public License v3.0:
  https://www.gnu.org/licenses/gpl-3.0.txt
  https://www.gnu.org/licenses/lgpl-3.0.txt
SPDX-License-Identifier: LGPL-3.0-only
End of Source File Header

Generate MobileGL/MG_Protocol/generated_wire_dispatch.h/.cpp from
Definitions.cpp + wire_full.fbs + generated_opcodes.h. The generated router
covers every GL/EGL opcode in kMobileGLDispatch (the full 1396-entry source
list): opcodes whose payload table decodes into callable C++ arguments are
dispatched to the exported gl*/egl* entry point; opcodes that the current
generator cannot yet express (exotic pointer shapes, callbacks) are registered
as unsupported so the dispatch table is complete and the classify/coverage
test can report the remaining gap precisely.
"""
from __future__ import annotations

import re
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
DEFINITIONS = REPO_ROOT / "MobileGL/MG_Impl/GLImpl/Exporting/Definitions.cpp"
WIRE_FULL = REPO_ROOT / "MobileGL/MG_Protocol/wire_full.fbs"
OPCODE_H = REPO_ROOT / "MobileGL/MG_Protocol/generated_opcodes.h"
DISPATCH_H = REPO_ROOT / "MobileGL/MG_Protocol/generated_dispatch.h"
OUT_H = REPO_ROOT / "MobileGL/MG_Protocol/generated_wire_dispatch.h"
OUT_CPP = REPO_ROOT / "MobileGL/MG_Protocol/generated_wire_dispatch.cpp"
OUT_REPORT = REPO_ROOT / "MobileGL/MG_Protocol/generated_wire_dispatch_report.txt"

# Same parsing as generate_protocol_fbs.py.
HEADER_RE = re.compile(
    r"DECLARE_GL_FUNCTION_(STUB_)?HEAD\s*\(\s*([^,]+)\s*,\s*([A-Za-z0-9_]+)\s*,\s*(.*?)\)\s*"
    r"DECLARE_GL_FUNCTION_(?:STUB_)?END(?:_NO_RETURN)?\s*\(",
    re.DOTALL,
)

TABLE_RE = re.compile(r"^\s*table\s+(\w+)\s*\{", re.MULTILINE)
FIELD_RE = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_]*):\s*([A-Za-z0-9_\[\]]+);", re.MULTILINE)

SUPPORTED_FBS_TYPES = {
    "int",
    "uint",
    "float",
    "double",
    "bool",
    "long",
    "ulong",
    "string",
    "[int]",
    "[uint]",
    "[float]",
    "[double]",
    "[bool]",
    "[long]",
    "[ulong]",
    "ShmRegion",
}


def collect_tables(text: str) -> dict[str, dict[str, str]]:
    """Return {table_name: {field_name: fbs_type}}."""
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
    type_text = trimmed[: name_match.start()].strip()
    return type_text, name


def arg_cast_for_call(c_type: str, arg_name: str) -> str:
    c_type = c_type.strip()
    c_type = re.sub(r"\b(?:const|volatile)\s+", "", c_type).strip()
    # Pointers that carry shm data or vectors are decoded directly; scalar
    # casts are only needed for the handful of GL types with an underlying
    # representation different from the FlatBuffer scalar.
    if "*" in c_type:
        return "nullptr"
    if re.search(r"\(\)\s*$", arg_name):
        return arg_name
    if c_type in ("GLenum", "GLbitfield", "GLuint", "GLushort", "GLubyte", "GLsync", "GLboolean"):
        return f"static_cast<{c_type}>({arg_name})"
    if c_type in ("GLint", "GLsizei", "GLshort", "GLbyte", "GLfixed"):
        return f"static_cast<{c_type}>({arg_name})"
    if c_type in ("GLfloat", "GLclampf", "GLhalf"):
        return f"static_cast<{c_type}>({arg_name})"
    if c_type in ("GLdouble", "GLclampd", "GLuint64", "GLint64"):
        return f"static_cast<{c_type}>({arg_name})"
    return arg_name


def cast_scalar(c_type: str, expr: str) -> str:
    """Wrap a FlatBuffer scalar in the exact GL C type the entry point wants."""
    c_type = c_type.strip()
    if c_type == "GLsync":
        return f"reinterpret_cast<GLsync>(static_cast<std::uintptr_t>({expr}))"
    if c_type in ("GLenum", "GLbitfield", "GLuint", "GLushort", "GLubyte", "GLboolean",
                  "GLint", "GLsizei", "GLshort", "GLbyte", "GLfixed", "GLfloat", "GLclampf",
                  "GLdouble", "GLclampd", "GLuint64", "GLint64", "GLhalf", "GLsizeiptr",
                  "GLintptr", "GLhandleARB"):
        return f"static_cast<{c_type}>({expr})"
    return expr


def cast_pointer(expr: str, c_type: str) -> str:
    """Convert a decoded pointer expression to the exact GL pointer type."""
    c_type = c_type.strip()
    const_pointer = "const" in c_type
    if "void" in c_type:
        if const_pointer:
            return expr
        return f"const_cast<void*>({expr})"
    # Remove const/volatile qualifiers to recover the pointee type.
    pointee = re.sub(r"\b(?:const|volatile)\s+", "", c_type).rstrip("*").strip()
    if const_pointer:
        return f"reinterpret_cast<{c_type}>({expr})"
    # First reinterpret to the const pointee pointer, then const_cast away.
    return f"const_cast<{c_type}>(reinterpret_cast<const {pointee}*>({expr}))"


def decode_expr(fbs_type: str, c_type: str, arg_name: str, table_var: str = "p") -> str:
    """Return the C++ expression that decodes one payload field for the call."""
    c_type = c_type.strip()
    const_pointer = "const" in c_type
    if fbs_type == "string":
        expr = f"({table_var}->{arg_name}() == nullptr ? nullptr : {table_var}->{arg_name}()->c_str())"
        if not const_pointer:
            return f"const_cast<char*>({expr})"
        return expr
    if fbs_type == "ShmRegion":
        expr = f"ResolveShm({table_var}->{arg_name}(), receivedShm, receivedShmCount)"
        return cast_pointer(expr, c_type)
    if fbs_type.startswith("["):
        expr = f"({table_var}->{arg_name}() == nullptr ? nullptr : {table_var}->{arg_name}()->data())"
        return cast_pointer(expr, c_type)
    return cast_scalar(c_type, f"{table_var}->{arg_name}()")


def main() -> int:
    if not all(p.exists() for p in (DEFINITIONS, WIRE_FULL, OPCODE_H, DISPATCH_H)):
        print("missing input", file=__import__("sys").stderr)
        return 1

    # opcode by GL name.
    opcode_by_name: dict[str, int] = {}
    for line in OPCODE_H.read_text(encoding="utf-8").splitlines():
        match = re.match(r"\s*(\w+)\s*=\s*(\d+),", line)
        if match:
            opcode_by_name[match.group(1)] = int(match.group(2))

    # dispatch metadata: (opcode, api, table).
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

    # API -> (return type, argument list, is_stub) from Definitions.cpp.
    api_args: dict[str, tuple[str, list[tuple[str, str]], bool]] = {}
    for match in HEADER_RE.finditer(definitions_text):
        stub = match.group(1) is not None
        c_type, name, arglist = match.group(2).strip(), match.group(3).strip(), match.group(4)
        api_args[name] = (c_type, parse_args(arglist), stub)
        opcode_by_name.setdefault("gl" + name, 0)

    entries: dict[int, tuple[str, bool, str]] = {}
    unsupported_reason: dict[int, str] = {}
    for opcode, api, table in dispatch_entries:
        if opcode in entries:
            continue
        gen_table = "Gen" + api[2:] if api.startswith("gl") else table
        fields = wire_tables.get(gen_table)
        if fields is None:
            entries[opcode] = (api, False, "")
            unsupported_reason[opcode] = "no payload table"
            continue
        if api.startswith("gl"):
            _, args, is_stub = api_args.get(api[2:], ("void", [], True))
        else:
            args, is_stub = [], False
        # Stubs are still dispatched: they compile through the exported
        # gl*/egl* entry point and keep their own WARN log.
        # Build decode expressions; any unsupported fbs type makes the entry
        # registered as unsupported (still dispatched through the router to a
        # clean status=1 response, never silently mis-decoded).
        call_args: list[str] = []
        ok = True
        seen_fields: set[str] = set()
        for c_type, arg_name in args:
            # Multi-level pointers (char** shader sources, const void* const*)
            # cannot be decoded into a single call expression yet; they are
            # registered as unsupported rather than mis-decoded.
            if c_type.count("*") > 1:
                ok = False
                break
            fbs_type = fields.get(arg_name)
            if fbs_type is None:
                ok = False
                break
            seen_fields.add(arg_name)
            if fbs_type not in SUPPORTED_FBS_TYPES:
                ok = False
                break
            call_args.append(decode_expr(fbs_type, c_type, arg_name))
        if not ok:
            entries[opcode] = (api, False, gen_table)
            unsupported_reason[opcode] = "unsupported payload signature"
            continue
        entries[opcode] = (api, True, gen_table)

    supported = [op for op, (_, ok, _) in entries.items() if ok]
    total = len(entries)
    table_size = max(entries) + 1 if entries else 0

    header = (
        "// MobileGL - MobileGL/MG_Protocol/generated_wire_dispatch.h\n"
        "// Copyright (c) 2025-2026 MobileGL-Dev\n"
        "// Licensed under the GNU Lesser General Public License v3.0:\n"
        "// SPDX-License-Identifier: LGPL-3.0-only\n"
        "// End of Source File Header\n\n"
        "#pragma once\n\n"
        "#include <cstdint>\n"
        "#include \"generated_dispatch.h\"\n\n"
        "// GENERATED by scripts/generate_wire_dispatch.py\n\n"
        "namespace MobileGL::Protocol::Wire {\n\n"
        f"inline constexpr uint32_t kWireDispatchTotal = {total}u;\n"
        f"inline constexpr uint32_t kWireDispatchSupported = {len(supported)}u;\n"
        "inline constexpr uint32_t kWireDispatchUnsupported = "
        f"{total - len(supported)}u;\n\n"
        "inline bool WireDispatchIsSupported(uint32_t opcode) {\n"
        "    switch (opcode) {\n"
        + "\n".join(f"        case {op}: return true;" for op in supported) +
        "\n        default: return false;\n"
        "    }\n"
        "}\n\n"
        "inline const char* WireDispatchApi(uint32_t opcode) {\n"
        "    for (uint32_t i = 0; i < kMobileGLDispatchCount; ++i) {\n"
        "        if (kMobileGLDispatch[i].opcode == opcode) return kMobileGLDispatch[i].api;\n"
        "    }\n"
        "    return \"?\";\n"
        "}\n\n"
        "inline const char* WireDispatchPayloadTable(uint32_t opcode) {\n"
        "    for (uint32_t i = 0; i < kMobileGLDispatchCount; ++i) {\n"
        "        if (kMobileGLDispatch[i].opcode == opcode) return kMobileGLDispatch[i].payloadTable;\n"
        "    }\n"
        "    return \"\";\n"
        "}\n\n"
        "// Executes one generated wire command. payloadBytes must point at the\n"
        "// root table of the opcode's Gen* payload; returns 0 when dispatched,\n"
        "// 1 when the opcode has no supported generated handler.\n"
        "uint32_t WireDispatchCall(uint32_t opcode,\n"
        "                          uint32_t sessionId,\n"
        "                          const void* payloadBytes,\n"
        "                          uint64_t payloadSize,\n"
        "                          const void* const* receivedShm,\n"
        "                          uint32_t receivedShmCount);\n\n"
        "} // namespace MobileGL::Protocol::Wire\n\n"
        "// End of File\n"
    )
    OUT_H.write_text(header, encoding="utf-8")

    lines: list[str] = []
    gl_declarations: list[str] = []
    for name, (ret_type, args, _stub) in sorted(api_args.items()):
        arg_str = ", ".join(f"{c_type} {arg_name}" for c_type, arg_name in args)
        gl_declarations.append(f"    {ret_type} gl{name}({arg_str});")
    lines.append(
        "// MobileGL - MobileGL/MG_Protocol/generated_wire_dispatch.cpp\n"
        "// Copyright (c) 2025-2026 MobileGL-Dev\n"
        "// Licensed under the GNU Lesser General Public License v3.0:\n"
        "// SPDX-License-Identifier: LGPL-3.0-only\n"
        "// End of Source File Header\n\n"
        "#include \"generated_wire_dispatch.h\"\n"
        "#include \"generated_dispatch.h\"\n"
        "#include \"gen/wire_full_generated.h\"\n"
        "#include \"MG_Impl/GLImpl/Buffer/GL_Buffer.h\"\n"
        "#include \"MG_Impl/GLImpl/Getter/GL_Getter.h\"\n"
        "#include \"MG_Impl/GLImpl/Sampler/GL_Sampler.h\"\n"
        "#include \"MG_Impl/GLImpl/Sync/GL_Sync.h\"\n"
        "#include \"MG_Impl/GLImpl/Query/GL_Query.h\"\n"
        "#include \"MG_Impl/GLImpl/Texture/GL_Texture.h\"\n"
        "#include \"MG_Impl/GLImpl/Drawing/GL_Drawing.h\"\n"
        "#include \"MG_Impl/GLImpl/Program/GL_Program.h\"\n"
        "#include \"MG_Impl/GLImpl/Program/GL_ProgramPipeline.h\"\n"
        "#include \"MG_Impl/GLImpl/RenderState/GL_RenderState.h\"\n"
        "#include \"MG_Impl/GLImpl/Framebuffer/GL_Framebuffer.h\"\n"
        "#include \"MG_Impl/GLImpl/VertexArray/GL_VertexArray.h\"\n"
        "#include \"MG_Impl/GLImpl/Debug/GL_Debug.h\"\n\n"
        "extern \"C\" {\n"
        + "\n".join(gl_declarations) +
        "\n}\n\n"
        "namespace {\n"
        "    const void* ResolveShm(const MobileGL::Protocol::WireFull::ShmRegion* region,\n"
        "                           const void* const* receivedShm, uint32_t receivedShmCount) {\n"
        "        if (region == nullptr || receivedShm == nullptr || receivedShmCount == 0) {\n"
        "            return nullptr;\n"
        "        }\n"
        "        const auto* base = static_cast<const uint8_t*>(receivedShm[0]);\n"
        "        return base + region->offset();\n"
        "    }\n"
        "} // namespace\n\n"
        "namespace MobileGL::Protocol::Wire {\n\n"
        "    uint32_t WireDispatchCall(uint32_t opcode, uint32_t sessionId,\n"
        "                               const void* payloadBytes, uint64_t payloadSize,\n"
        "                               const void* const* receivedShm, uint32_t receivedShmCount) {\n"
        "        (void)sessionId;\n"
        "        if (payloadBytes == nullptr || payloadSize < 4) {\n"
        "            return 1u;\n"
        "        }\n"
        "        switch (opcode) {\n"
    )

    # Sort supported first then unsupported, still keyed by opcode order.
    for opcode, (api, ok, table) in sorted(entries.items()):
        if not ok:
            continue
        gen_table = "Gen" + api[2:] if api.startswith("gl") else ""
        _, args, _ = api_args.get(api[2:], ("void", [], True)) if api.startswith("gl") else ("", [], False)
        # Recompute decode expressions from the table.
        fields = wire_tables.get(gen_table, {})
        call_args: list[str] = []
        ok_local = True
        for c_type, arg_name in args:
            fbs_type = fields.get(arg_name)
            if fbs_type is None or fbs_type not in SUPPORTED_FBS_TYPES:
                ok_local = False
                break
            call_args.append(decode_expr(fbs_type, c_type, arg_name))
        if not ok_local:
            continue
        lines.append(
            f"        case {opcode}: {{\n"
            f"            const auto* p = ::flatbuffers::GetRoot<MobileGL::Protocol::WireFull::{gen_table}>(payloadBytes);\n"
            f"            if (p == nullptr) {{ return 1u; }}\n"
            f"            ::gl{api[2:]}({', '.join(call_args)});\n"
            f"            return 0u;\n"
            f"        }}\n"
        )

    # Unsupported opcodes are still registered: their case returns 1 cleanly.
    for opcode, (api, ok, table) in sorted(entries.items()):
        if ok:
            continue
        lines.append(
            f"        case {opcode}:\n"
            f"            return 1u;\n"
        )
    lines.append(
        "        default:\n"
        "            return 1u;\n"
        "        }\n"
        "    }\n"
        "} // namespace MobileGL::Protocol::Wire\n\n"
        "// End of File\n"
    )
    OUT_CPP.write_text("\n".join(lines), encoding="utf-8")

    report_lines = ["# MobileGL wire dispatch unsupported report (generated)"] + [
        f"{op}\t{api}\t{reason}" for op, (api, ok, _) in sorted(entries.items())
        if not ok for reason in [unsupported_reason.get(op, "")]
    ]
    OUT_REPORT.write_text("\n".join(report_lines) + "\n", encoding="utf-8")

    print(f"Wrote {OUT_H.name}: {total} opcodes, {len(supported)} supported")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
