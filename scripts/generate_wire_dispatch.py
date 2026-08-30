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
EGL_DEFINITIONS = REPO_ROOT / "MobileGL/MG_Impl/EGLImpl/Exporting/Definitions.cpp"
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

# EGL functions are plain `MOBILEGL_EGL_API <ret> egl*<Name>(args) {` bodies.
EGL_FUNC_RE = re.compile(
    r"MOBILEGL_EGL_API\s+(.+?)\s+(egl[A-Za-z0-9_]+)\s*\(([^)]*)\)\s*\{",
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

# EGL object/handle parameters travel as ulong tokens and must be re-interpreted
# as the opaque EGL pointer types the entry point expects.
EGL_HANDLE_TYPES = {
    "EGLDisplay",
    "EGLContext",
    "EGLSurface",
    "EGLConfig",
    "EGLImage",
    "EGLSync",
    "EGLClientBuffer",
    "EGLNativePixmapType",
    "EGLNativeDisplayType",
    "NativeWindowType",
    "NativeDisplayType",
}

# Return types that are pointer-like (stored as uintptr, never as int).
RETURN_HANDLE_TYPES = EGL_HANDLE_TYPES | {
    "GLsync",
    "GLhandleARB",
    "GLVULKANPROCNV",
    "GLVULKANPROCNVNV",
    "__eglMustCastToProperFunctionPointerType",
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


def format_declaration_args(args: list[tuple[str, str]]) -> str:
    """Render C prototype arguments, special-casing the bare `(void)` form."""
    parts: list[str] = []
    for c_type, arg_name in args:
        if not c_type and arg_name == "void":
            parts.append("void")
        else:
            parts.append(f"{c_type} {arg_name}".strip())
    return ", ".join(parts)


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
    if c_type in EGL_HANDLE_TYPES:
        return f"reinterpret_cast<{c_type}>(static_cast<std::uintptr_t>({expr}))"
    if c_type == "void*":
        return f"reinterpret_cast<void*>(static_cast<std::uintptr_t>({expr}))"
    if c_type in ("GLenum", "GLbitfield", "GLuint", "GLushort", "GLubyte", "GLboolean",
                  "GLint", "GLsizei", "GLshort", "GLbyte", "GLfixed", "GLfloat", "GLclampf",
                  "GLdouble", "GLclampd", "GLuint64", "GLint64", "GLhalf", "GLsizeiptr",
                  "GLintptr", "GLhandleARB",
                  "EGLBoolean", "EGLenum", "EGLint", "EGLint64", "EGLTime",
                  "EGLuint64KHR", "EGLAttrib"):
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

    # EGL API signatures from EGLImpl/Exporting/Definitions.cpp.
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
            opcode_by_name.setdefault(name, 0)

    entries: dict[int, tuple[str, bool, str]] = {}
    unsupported_reason: dict[int, str] = {}
    for opcode, api, table in dispatch_entries:
        if opcode in entries:
            continue
        if api.startswith("gl"):
            gen_table = "Gen" + api[2:]
        elif api.startswith("egl"):
            gen_table = "GenEgl" + api[3:]
        else:
            gen_table = table
        fields = wire_tables.get(gen_table)
        if fields is None:
            entries[opcode] = (api, False, "")
            unsupported_reason[opcode] = "no payload table"
            continue
        if api.startswith("gl"):
            _, args, is_stub = api_args.get(api[2:], ("void", [], True))
        elif api.startswith("egl"):
            _, args, is_stub = egl_api_args.get(api[3:], ("void", [], True))
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
            # Out-strings (GLchar* infoLog etc.) are WRITE targets on the
            # server side; the wire encodes them as `string` (read-only), so
            # decoding one and const_cast-ing it into the call would make the
            # frontend write into FlatBuffer memory. Refuse the entry instead.
            if fbs_type == "string" and "const" not in c_type:
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
        "                          uint32_t receivedShmCount,\n"
        "                          uint32_t outCapacity);\n\n"
        "// Scalar return capture: WireDispatchCall stores the return value of\n"
        "// a non-void API so the ServerCore can echo it in Response.ret_i64.\n"
        "void WireDispatchResetReturn();\n"
        "bool WireDispatchReturnValid();\n"
        "int64_t WireDispatchReturnI64();\n"
        "void WireDispatchStoreI64(int64_t value);\n"
        "void WireDispatchStoreU64(uint64_t value);\n"
        "// Out-vector capture (glGen* families): the dispatch allocates the\n"
        "// target buffer, writes server-side results, then stores the bytes\n"
        "// for Response.ret_bytes.\n"
        "void WireDispatchStoreBytes(const uint8_t* data, uint32_t size);\n"
        "const uint8_t* WireDispatchBytes();\n"
        "uint32_t WireDispatchBytesSize();\n\n"
        "} // namespace MobileGL::Protocol::Wire\n\n"
        "// End of File\n"
    )
    OUT_H.write_text(header, encoding="utf-8")

    lines: list[str] = []
    extern_declarations: list[str] = []
    for name, (ret_type, args, _stub) in sorted(api_args.items()):
        extern_declarations.append(f"    {ret_type} gl{name}({format_declaration_args(args)});")
    for name, (ret_type, args, _stub) in sorted(egl_api_args.items()):
        extern_declarations.append(f"    {ret_type} egl{name}({format_declaration_args(args)});")
    lines.append(
        "// MobileGL - MobileGL/MG_Protocol/generated_wire_dispatch.cpp\n"
        "// Copyright (c) 2025-2026 MobileGL-Dev\n"
        "// Licensed under the GNU Lesser General Public License v3.0:\n"
        "// SPDX-License-Identifier: LGPL-3.0-only\n"
        "// End of Source File Header\n\n"
        "#include \"generated_wire_dispatch.h\"\n"
        "#include \"generated_dispatch.h\"\n"
        "#include \"gen/wire_full_generated.h\"\n"
        "#include <EGL/egl.h>\n"
        "#include <vector>\n"
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
        + "\n".join(extern_declarations) +
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
        "    namespace { thread_local bool s_returnValid = false; thread_local int64_t s_returnI64 = 0; thread_local std::vector<uint8_t> s_returnBytes; }\n"
        "    void WireDispatchResetReturn() { s_returnValid = false; s_returnI64 = 0; s_returnBytes.clear(); }\n"
        "    bool WireDispatchReturnValid() { return s_returnValid; }\n"
        "    int64_t WireDispatchReturnI64() { return s_returnI64; }\n"
        "    void WireDispatchStoreI64(int64_t value) { s_returnI64 = value; s_returnValid = true; }\n"
        "    void WireDispatchStoreU64(uint64_t value) { s_returnI64 = static_cast<int64_t>(value); s_returnValid = true; }\n"
        "    void WireDispatchStoreBytes(const uint8_t* data, uint32_t size) {\n"
        "        if (data != nullptr && size > 0) { s_returnBytes.assign(data, data + size); }\n"
        "    }\n"
        "    const uint8_t* WireDispatchBytes() {\n"
        "        return s_returnBytes.empty() ? nullptr : s_returnBytes.data();\n"
        "    }\n"
        "    uint32_t WireDispatchBytesSize() {\n"
        "        return static_cast<uint32_t>(s_returnBytes.size());\n"
        "    }\n\n"
        "    uint32_t WireDispatchCall(uint32_t opcode, uint32_t sessionId,\n"
        "                               const void* payloadBytes, uint64_t payloadSize,\n"
        "                               const void* const* receivedShm, uint32_t receivedShmCount,\n"
        "                               uint32_t outCapacity) {\n"
        "        (void)sessionId;\n"
        "        WireDispatchResetReturn();\n"
        "        if (payloadBytes == nullptr || payloadSize < 4) {\n"
        "            return 1u;\n"
        "        }\n"
        "        switch (opcode) {\n"
    )

    # Sort supported first then unsupported, still keyed by opcode order.
    for opcode, (api, ok, table) in sorted(entries.items()):
        if not ok:
            continue
        if api.startswith("gl"):
            gen_table = "Gen" + api[2:]
            _, args, _ = api_args.get(api[2:], ("void", [], True))
        elif api.startswith("egl"):
            gen_table = "GenEgl" + api[3:]
            _, args, _ = egl_api_args.get(api[3:], ("void", [], True))
        else:
            gen_table = ""
            args = []
        # Recompute decode expressions from the table. Non-const vector
        # arguments are out-vectors (glGen* families): the dispatch allocates
        # the target buffer server-side and returns it via ret_bytes.
        fields = wire_tables.get(gen_table, {})
        count_field = None
        for c_type, arg_name in args:
            if arg_name in ("n", "count", "num") and fields.get(arg_name) in (
                "int", "uint", "long", "ulong",
            ):
                count_field = arg_name
                break
        elem_bytes = {"[uint]": 4, "[int]": 4, "[float]": 4, "[double]": 8,
                      "[long]": 8, "[ulong]": 8, "[bool]": 1}
        elem_cpp = {"[uint]": "uint32_t", "[int]": "int32_t", "[float]": "float",
                    "[double]": "double", "[long]": "int64_t", "[ulong]": "uint64_t",
                    "[bool]": "uint8_t"}
        call_args: list[str] = []
        pre_lines: list[str] = []
        post_lines: list[str] = []
        out_vectors: list[tuple[str, int]] = []
        ok_local = True
        for c_type, arg_name in args:
            fbs_type = fields.get(arg_name)
            if fbs_type is None or fbs_type not in SUPPORTED_FBS_TYPES:
                ok_local = False
                break
            if fbs_type.startswith("[") and "const" not in c_type:
                if fbs_type not in elem_bytes:
                    ok_local = False
                    break
                var = "_out_" + arg_name
                if count_field is not None:
                    size_expr = (
                        f"(outCapacity != 0 && outCapacity < static_cast<uint32_t>(p->{count_field}())"
                        f" ? outCapacity : static_cast<uint32_t>(p->{count_field}()))"
                    )
                else:
                    size_expr = "(outCapacity != 0 ? outCapacity : 1u)"
                pre_lines.append(
                    f"            std::vector<{elem_cpp[fbs_type]}>{var}("
                    f"static_cast<size_t>({size_expr}));\n"
                )
                call_args.append(f"reinterpret_cast<{c_type}>({var}.data())")
                out_vectors.append((var, elem_bytes[fbs_type]))
                continue
            call_args.append(decode_expr(fbs_type, c_type, arg_name))
        if not ok_local:
            continue
        for var, bytes_per_elem in out_vectors:
            post_lines.append(
                f"            WireDispatchStoreBytes("
                f"reinterpret_cast<const uint8_t*>({var}.data()),\n"
                f"                                  static_cast<uint32_t>({var}.size() * {bytes_per_elem}));\n"
            )
        if api.startswith("egl"):
            call_prefix = f"::egl{api[3:]}"
            ret_type = egl_api_args.get(api[3:], ("void", [], True))[0]
        else:
            call_prefix = f"::gl{api[2:]}"
            ret_type = api_args.get(api[2:], ("void", [], True))[0]

        ret_type = (ret_type or "void").strip()
        args_text = ", ".join(call_args)
        if ret_type == "void":
            call_line = f"            {call_prefix}({args_text});\n"
        elif "*" in ret_type or ret_type in RETURN_HANDLE_TYPES:
            call_line = (
                f"            {{ const auto _ret = {call_prefix}({args_text});\n"
                + "              WireDispatchStoreU64(reinterpret_cast<std::uintptr_t>(_ret)); }\n"
            )
        else:
            call_line = (
                f"            {{ const auto _ret = {call_prefix}({args_text});\n"
                + "              WireDispatchStoreI64(static_cast<std::int64_t>(_ret)); }\n"
            )
        lines.append(
            f"        case {opcode}: {{\n"
            f"            const auto* p = ::flatbuffers::GetRoot<MobileGL::Protocol::WireFull::{gen_table}>(payloadBytes);\n"
            f"            if (p == nullptr) {{ return 1u; }}\n"
            + "".join(pre_lines) +
            call_line +
            "".join(post_lines) +
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
