#!/usr/bin/env python3
"""
MobileGL - scripts/generate_client_trampoline.py
Copyright (c) 2025-2026 MobileGL-Dev
Licensed under the GNU Lesser General Public License v3.0:
  https://www.gnu.org/licenses/gpl-3.0.txt
  https://www.gnu.org/licenses/lgpl-3.0.txt
SPDX-License-Identifier: LGPL-3.0-only
End of Source File Header

Generate MobileGL/MG_Client/generated_wire_trampoline.h/.cpp: exported
MOBILEGL_GL_API gl* entry points that forward to the Wire::SendGl* wrappers.
Entry points with a scalar return read Response.ret_i64 through
Client::GetLastResponseRetI64(); shm-data entries (glBufferData and friends
whose payload has an explicit size field) allocate a transport shared-memory
region through Client::AllocateShm. Functions whose wire signature cannot be
expressed (out vectors, multi-level pointers, pointer returns other than
glGetString) are skipped by design: they stay out of the declared surface and
eglGetProcAddress returns nullptr for them.

The generated files are untracked (see .gitignore); they are produced by the
MG_TrampolineGen custom target at build time.
"""
from __future__ import annotations

import re
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
DEFINITIONS = REPO_ROOT / "MobileGL/MG_Impl/GLImpl/Exporting/Definitions.cpp"
WIRE_FULL = REPO_ROOT / "MobileGL/MG_Protocol/wire_full.fbs"
OPCODE_H = REPO_ROOT / "MobileGL/MG_Protocol/generated_opcodes.h"
DISPATCH_H = REPO_ROOT / "MobileGL/MG_Protocol/generated_dispatch.h"
OUT_H = REPO_ROOT / "MobileGL/MG_Client/generated_wire_trampoline.h"
OUT_CPP = REPO_ROOT / "MobileGL/MG_Client/generated_wire_trampoline.cpp"
OUT_COVERAGE = REPO_ROOT / "MobileGL/MG_Client/generated_trampoline_coverage.txt"

HEADER_RE = re.compile(
    r"DECLARE_GL_FUNCTION_(STUB_)?HEAD\s*\(\s*([^,]+)\s*,\s*([A-Za-z0-9_]+)\s*,\s*(.*?)\)\s*"
    r"DECLARE_GL_FUNCTION_(?:STUB_)?END(?:_NO_RETURN)?\s*\(",
    re.DOTALL,
)

TABLE_RE = re.compile(r"^\s*table\s+(\w+)\s*\{", re.MULTILINE)
FIELD_RE = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_]*):\s*([A-Za-z0-9_\[\]]+);", re.MULTILINE)

# Numeric FlatBuffer field names that can size a shm payload.
SIZE_FIELD_NAMES = ("size", "data_size", "dataSize", "length", "byte_count", "count")

HANDLE_RET_TYPES = {
    "GLsync",
    "GLhandleARB",
    "GLVULKANPROCNV",
    "GLVULKANPROCNVNV",
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
    type_text = trimmed[: name_match.start()].strip()
    type_text = re.sub(r"\s*\*\s*$", "*", type_text)
    return type_text, name


def find_size_field(fields: dict[str, str]) -> str | None:
    for candidate in SIZE_FIELD_NAMES:
        if candidate in fields and fields[candidate] in (
            "int", "uint", "long", "ulong", "float", "double",
        ):
            return candidate
    return None


def is_pointer_like(ret_type: str) -> bool:
    # Function-pointer typedefs (GLVULKANPROCNV etc.) contain parentheses.
    return "*" in ret_type or "(" in ret_type or ret_type in HANDLE_RET_TYPES


def uniform_elems_per(api: str) -> int | None:
    """Return elements per `count` for glUniform{1,2,3,4}{i,ui,f}v,
    glUniformMatrix{2,3,4}fv and glUniformMatrix{3x2,4x3,...}fv."""
    if not api.startswith("glUniform"):
        return None
    body = api[len("glUniform"):]
    if body.startswith("Matrix"):
        rest = body[len("Matrix"):]
        rect = re.match(r"(\d+)x(\d+)", rest)
        if rect is not None:
            return int(rect.group(1)) * int(rect.group(2))
        digits = re.match(r"(\d+)", rest)
        if digits is None:
            return None
        n = int(digits.group(1))
        return n * n
    digits = re.match(r"(\d+)", body)
    if digits is None:
        return None
    return int(digits.group(1))


def main() -> int:
    if not all(p.exists() for p in (DEFINITIONS, WIRE_FULL, OPCODE_H, DISPATCH_H)):
        print("missing input", file=__import__("sys").stderr)
        return 1

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

    declarations: list[str] = []
    bodies: list[str] = []
    generated: list[str] = []
    skipped: list[tuple[str, str]] = []

    def skip(api: str, reason: str) -> None:
        skipped.append((api, reason))

    for opcode, api, _table in dispatch_entries:
        if not api.startswith("gl"):
            continue
        name = api[2:]
        entry = api_args.get(name)
        if entry is None:
            skip(api, "no signature in Definitions.cpp")
            continue
        ret_type, args, _stub = entry
        gen_table = "Gen" + name
        fields = wire_tables.get(gen_table)
        if fields is None:
            skip(api, "no payload table")
            continue

        # Wire argument plan. Skip entries that cannot be expressed today.
        shm_arg: tuple[str, str, str] | None = None  # (c_type, arg_name, size_field)
        arg_plan: list[tuple[str, str]] = []  # (c_type, arg_name) kept in wire order
        out_vectors: list[tuple[str, str, str]] = []  # (c_type, arg_name, fbs_type)
        in_vectors: list[tuple[str, str, str, int]] = []  # (c_type, arg_name, fbs_type, elems)
        ok = True
        skip_reason = ""
        out_count_field = None
        for c_type, arg_name in args:
            if arg_name in ("n", "count", "num") and fields.get(arg_name) in (
                "int", "uint", "long", "ulong",
            ):
                out_count_field = arg_name
                break
        # Getter family (no count in payload): capacity comes from the shared
        # MobileGLQueryCount(pname) table on both halves, so the trampoline and
        # the server agree on the exact number of elements.
        pname_arg = None
        for c_type, arg_name in args:
            if arg_name == "pname" and fields.get(arg_name) in (
                "int", "uint", "long", "ulong",
            ):
                pname_arg = arg_name
                break
        # Input-vector count argument (glUniform* / glDelete*).
        count_arg = None
        for c_type, arg_name in args:
            if arg_name in ("count", "num", "n") and fields.get(arg_name) in (
                "int", "uint", "long", "ulong",
            ):
                count_arg = arg_name
                break
        for c_type, arg_name in args:
            if c_type.count("*") > 1:
                ok = False
                skip_reason = "multi-level pointer"
                break
            arg_type = fields.get(arg_name)
            if arg_type is None:
                ok = False
                skip_reason = "field missing"
                break
            if arg_type.startswith("["):
                if "const" not in c_type and (out_count_field is not None or pname_arg is not None):
                    # Out-vector: values come back through Response.ret_bytes.
                    out_vectors.append((c_type, arg_name, arg_type))
                    continue
                if "const" in c_type and count_arg is not None:
                    # Input vectors (glUniform* with element multiplicity,
                    # glDelete* with elems=1): materialize client-side.
                    if api.startswith("glUniform"):
                        elems = uniform_elems_per(api)
                        if elems is None:
                            ok = False
                            skip_reason = "uniform vector without element count"
                            break
                    else:
                        elems = 1
                    in_vectors.append((c_type, arg_name, arg_type, elems))
                    continue
                ok = False
                skip_reason = "input vector or out-vector without count/pname"
                break
            if arg_type == "ShmRegion":
                size_field = find_size_field(fields)
                if size_field is None:
                    ok = False
                    skip_reason = "shm payload without size field"
                    break
                if shm_arg is not None:
                    ok = False
                    skip_reason = "multiple shm payloads"
                    break
                shm_arg = (c_type, arg_name, size_field)
                continue
            if arg_type not in ("int", "uint", "float", "double", "bool", "long", "ulong", "string"):
                ok = False
                skip_reason = "unsupported payload type " + arg_type
                break
            arg_plan.append((c_type, arg_name))
        if not ok:
            skip(api, skip_reason)
            continue

        # glGetString/glGetStringi have pointer returns but a dedicated string channel.
        # Buffer-mapping entries return a server-side address; in the
        # in-process transport that address lives in the same process, so the
        # client can use it directly (cross-process connect mode would need a
        # shm-mapping channel later).
        MAP_RETURN_FNS = {
            "MapBuffer",
            "MapBufferARB",
            "MapBufferRange",
            "MapBufferRangeARB",
            "MapBufferRangeEXT",
            "MapBufferRangeNV",
            "MapNamedBuffer",
            "MapNamedBufferARB",
            "MapNamedBufferRange",
            "MapNamedBufferRangeARB",
            "MapNamedBufferRangeEXT",
        }
        if is_pointer_like(ret_type) and name not in ("GetString", "GetStringi") and \
                name not in MAP_RETURN_FNS:
            skip(api, "pointer return not supported")
            continue

        # Signature declaration (exact GL types).
        decl_args = ", ".join(f"{c_type} {arg_name}" for c_type, arg_name in args)
        declarations.append(f"MOBILEGL_GL_API {ret_type} gl{name}({decl_args});")

        # Body.
        line: list[str] = []
        line.append(f"MOBILEGL_GL_API {ret_type} gl{name}({decl_args}) {{")
        line.append("    const Uint64 _session = MobileGL::Client::GetCurrentSessionId();")
        if ret_type == "void":
            line.append("    if (_session == 0) {")
            line.append("        return;")
            line.append("    }")
        else:
            line.append("    if (_session == 0) {")
            line.append("        return {};")
            line.append("    }")
        line.append("    const Uint64 _token = MobileGL::ClientTrampoline::Gen::NextToken();")

        # Allocate shm when the entry takes a data payload.
        pre: list[str] = []
        post: list[str] = []
        call_extra: list[str] = []
        if shm_arg is not None:
            c_type, arg_name, size_field = shm_arg
            var_name = "_shm_" + arg_name
            pre.append(f"    MobileGLShmHandle {var_name}{{}};")
            pre.append(f"    Bool {var_name}Ok = false;")
            pre.append(
                f"    if ({arg_name} != nullptr && ((Uint64)({size_field}) > 0) &&"
                f" MobileGL::Client::AllocateShm((Uint64)({size_field}), &{var_name})) {{"
            )
            pre.append(f"        {var_name}Ok = true;")
            pre.append(
                f"        memcpy({var_name}.mappedAddress, (const void*)({arg_name}),"
                f" (SizeT)({size_field}));"
            )
            pre.append("    }")
            call_extra.append(f"{var_name}Ok ? &{var_name} : nullptr")
            post.append(f"    if ({var_name}Ok) {{ MobileGL::Client::ReleaseShm(&{var_name}); }}")
        else:
            call_extra.append("nullptr")

        # Out-vector entries: copy Response.ret_bytes into the caller buffer.
        # Capacity is the payload count field when present, otherwise the
        # shared pname table (glGetIntegerv family).
        if out_count_field is not None:
            cap_expr = f"(Uint32)({out_count_field})"
        elif pname_arg is not None:
            cap_expr = (
                f"(Uint32)MobileGL::Protocol::MobileGLQueryCount((Uint32)({pname_arg}))"
            )
        else:
            cap_expr = "1u"
        out_vec_bytes = {"[uint]": 4, "[int]": 4, "[float]": 4, "[double]": 8,
                         "[long]": 8, "[ulong]": 8, "[bool]": 1}
        for out_c_type, out_arg_name, out_fbs in out_vectors:
            elem_bytes = out_vec_bytes.get(out_fbs, 4)
            post.append(f"    if ({out_arg_name} != nullptr && {cap_expr} > 0) {{")
            post.append("        const Vector<Uint8>& _outBytes = MobileGL::Client::GetLastResponseBytes();")
            post.append("        if (!_outBytes.empty()) {")
            post.append(f"            const SizeT _outWant = (SizeT)({cap_expr}) * {elem_bytes}u;")
            post.append("            const SizeT _outCopy = _outBytes.size() < _outWant ? _outBytes.size() : _outWant;")
            post.append(f"            memcpy((void*)({out_arg_name}), _outBytes.data(), _outCopy);")
            post.append("        }")
            post.append("    }")

        # Input vectors (glUniform*): materialize the caller pointer into the
        # vector the WireFull wrapper expects.
        in_vec_bytes = {"[uint]": 4, "[int]": 4, "[float]": 4, "[double]": 8,
                        "[long]": 8, "[ulong]": 8, "[bool]": 1}
        in_vec_cpp = {"[uint]": "uint32_t", "[int]": "int32_t", "[float]": "float",
                      "[double]": "double", "[long]": "int64_t", "[ulong]": "uint64_t",
                      "[bool]": "uint8_t"}
        for in_c_type, in_arg_name, in_fbs, elems in in_vectors:
            var = "_in_" + in_arg_name
            cpp_elem = in_vec_cpp.get(in_fbs, "uint8_t")
            elem_b = in_vec_bytes.get(in_fbs, 1)
            pre.append(f"    std::vector<{cpp_elem}> {var}(static_cast<size_t>({count_arg}) * {elems}u, 0);")
            pre.append(f"    if ({in_arg_name} != nullptr) {{")
            pre.append(f"        memcpy({var}.data(), (const void*)({in_arg_name}), {var}.size() * {elem_b}u);")
            pre.append("    }")

        # Ordered wire arguments: scalars/strings in signature order, input
        # vectors materialized, shm/out handled by call_extra/capacity.
        ordered_args: list[str] = []
        for c_type, arg_name in args:
            if (c_type, arg_name) in arg_plan:
                # Handles (GLsync et al.) travel as ulong tokens on the wire.
                if fields.get(arg_name) == "ulong" and ("*" in c_type or c_type == "GLsync"):
                    ordered_args.append(f"reinterpret_cast<Uint64>({arg_name})")
                else:
                    ordered_args.append(arg_name)
            elif any(in_arg_name == arg_name for _, in_arg_name, _, _ in in_vectors):
                ordered_args.append("_in_" + arg_name)
            # ShmRegion and out-vectors are handled by call_extra/capacity.
        call_args = [f"_session"] + ordered_args + ["_token"] + call_extra
        if out_vectors:
            call_args.append(cap_expr)
        post_text = ("\n" + "\n".join(post)) if post else ""
        if name == "GetString":
            body = (
                "    static thread_local String _stringCache;\n"
                "    String _out;\n"
                "    if (!MobileGL::Client::SendGetString(_session, (Uint32)(name), _token, &_out)) {\n"
                "        return nullptr;\n"
                "    }\n"
                "    _stringCache = MobileGL::Move(_out);\n"
                "    // DIAGNOSTIC PROBE: prove the C/S path is live by tagging the\n"
                "    // renderer string; remove once integration is confirmed.\n"
                "    if ((Uint32)(name) == 0x1F01 /* GL_RENDERER */ && !_stringCache.empty()) {\n"
                "        _stringCache += \" [MobileGL-CS]\";\n"
                "    }\n"
                "    return reinterpret_cast<const GLubyte*>(_stringCache.c_str());\n"
            )
        elif name == "GetStringi":
            body = (
                "    static thread_local String _stringCache;\n"
                "    String _out;\n"
                "    if (!MobileGL::Client::SendGetStringi(_session, (Uint32)(name), (Uint32)(index), _token, &_out)) {\n"
                "        return nullptr;\n"
                "    }\n"
                "    _stringCache = MobileGL::Move(_out);\n"
                "    return reinterpret_cast<const GLubyte*>(_stringCache.c_str());\n"
            )
        else:
            call = ("    const Bool _submitOk = MobileGL::Client::Wire::" + "SendGl" + name +
                    "(" + ", ".join(call_args) + ");")
            if ret_type == "void":
                body = call + post_text + "\n    return;\n"
            else:
                # Scalar/pointer/out-vector returns are only valid after the
                # server processed the command: wait for our token before
                # reading Response.ret_i64 / ret_bytes. (Void calls stay
                # fire-and-forget.)
                body = (
                    call +
                    "    if (_submitOk) { MobileGL::Client::WaitResponseForToken(_token, 5000); }\n" +
                    post_text
                )
                if is_pointer_like(ret_type):
                    body += (
                        f"    return reinterpret_cast<{ret_type}>("
                        f"static_cast<std::uintptr_t>(MobileGL::Client::GetLastResponseRetI64()));\n"
                    )
                else:
                    body += (
                        f"    return static_cast<{ret_type}>("
                        f"MobileGL::Client::GetLastResponseRetI64());\n"
                    )

        line.extend(pre)
        line.append(body)
        line.append("}")
        bodies.append("\n".join(line))
        generated.append(api)

    header = (
        "// MobileGL - MobileGL/MG_Client/generated_wire_trampoline.h\n"
        "// Copyright (c) 2025-2026 MobileGL-Dev\n"
        "// Licensed under the GNU Lesser General Public License v3.0:\n"
        "// SPDX-License-Identifier: LGPL-3.0-only\n"
        "// End of Source File Header\n\n"
        "#pragma once\n\n"
        "#include <Includes.h>\n\n"
        "// GENERATED by scripts/generate_client_trampoline.py\n\n"
        + "\n".join(declarations) +
        "\n\nnamespace MobileGL::ClientTrampoline::Gen {\n"
        "    // Resolves a GL function name against the exported trampoline table\n"
        "    // (direct &glXxx references - no dlsym, mirroring monolith\n"
        "    // MobileGL::MG_Impl::GetProcAddress semantics).\n"
        "    void* LookupGeneratedProc(const char* name);\n"
        "}\n\n// End of File\n"
    )
    OUT_H.write_text(header, encoding="utf-8")

    proc_entries = "\n".join(
        f'            {{"{api}", reinterpret_cast<void*>(&{api})}},'
        for api in generated
    )
    proc_lookup = (
        "namespace MobileGL::ClientTrampoline::Gen {\n"
        "    void* LookupGeneratedProc(const char* name) {\n"
        "        struct Entry {\n"
        "            const char* name;\n"
        "            void* address;\n"
        "        };\n"
        "        static const Entry kTable[] = {\n"
        + proc_entries +
        "\n        };\n"
        "        if (name != nullptr) {\n"
        "            for (const Entry& entry : kTable) {\n"
        "                if (std::strcmp(name, entry.name) == 0) {\n"
        "                    return entry.address;\n"
        "                }\n"
        "            }\n"
        "        }\n"
        "        return nullptr;\n"
        "    }\n"
        "} // namespace MobileGL::ClientTrampoline::Gen\n\n"
    )

    cpp = (
        "// MobileGL - MobileGL/MG_Client/generated_wire_trampoline.cpp\n"
        "// Copyright (c) 2025-2026 MobileGL-Dev\n"
        "// Licensed under the GNU Lesser General Public License v3.0:\n"
        "// SPDX-License-Identifier: LGPL-3.0-only\n"
        "// End of Source File Header\n\n"
        "#include <Includes.h>\n"
        "#include <cstring>\n"
        "#include <vector>\n"
        "#include \"generated_wire_trampoline.h\"\n"
        "#include \"MG_Client/Client.h\"\n"
        "#include \"MG_Protocol/query_sizes.h\"\n"
        "#include \"generated_wire_client.h\"\n\n"
        "// GENERATED by scripts/generate_client_trampoline.py\n\n"
        "// The exported entry points are at global scope; MobileGL type aliases\n"
        "// (Uint64, Vector, String, ...) live in MobileGL::, so import them.\n"
        "using namespace MobileGL;\n\n"
        "namespace MobileGL::ClientTrampoline::Gen {\n"
        "    namespace {\n"
        "        Uint64 NextToken() {\n"
        "            static thread_local Uint64 token = 1;\n"
        "            return token++;\n"
        "        }\n"
        "    }\n"
        "} // namespace MobileGL::ClientTrampoline::Gen\n\n"
        + proc_lookup
        + "\n\n".join(bodies) +
        "\n\n// End of File\n"
    )
    OUT_CPP.write_text(cpp, encoding="utf-8")

    coverage_lines = [
        "# MobileGL client trampoline coverage (generated)",
        f"# supported {len(generated)}",
        *generated,
        "",
        f"# skipped {len(skipped)}",
        *[f"{api}\t{reason}" for api, reason in skipped],
        "",
    ]
    OUT_COVERAGE.write_text("\n".join(coverage_lines), encoding="utf-8")

    print(f"Wrote {OUT_H.name}: {len(generated)} generated trampoline entry points")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
