#!/usr/bin/env python3
"""
MobileGL - scripts/generate_protocol_fbs.py
Copyright (c) 2025-2026 MobileGL-Dev
Licensed under the GNU Lesser General Public License v3.0:
  https://www.gnu.org/licenses/gpl-3.0.txt
  https://www.gnu.org/licenses/lgpl-3.0.txt
SPDX-License-Identifier: LGPL-3.0-only

Generate FlatBuffers payload tables for the GL API from the existing
Definitions.cpp decls. The hand-maintained seed lives in
MobileGL/Protocol/protocol.fbs; this generator produces
MobileGL/Protocol/protocol_generated.fbs, which the Phase 4 codegen merges
into the canonical schema at the @INSERTION_POINT:PAYLOAD_CASES@ marker.

This is a deliberately small, deterministic generator: it handles the C type
shapes actually used by the frontend and leaves exotic signatures as TODO
comments rather than emitting invalid FlatBuffers.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
DEFINITIONS = REPO_ROOT / "MobileGL/MG_Impl/GLImpl/Exporting/Definitions.cpp"
OUTPUT = REPO_ROOT / "MobileGL/Protocol/protocol_generated.fbs"

# DECLARE_GL_FUNCTION_HEAD(type, name, args...)
HEADER_RE = re.compile(
    r"DECLARE_GL_FUNCTION_(?:STUB_)?HEAD\s*\(\s*([^,]+)\s*,\s*([A-Za-z0-9_]+)\s*,\s*(.*?)\)\s*"
    r"DECLARE_GL_FUNCTION_(?:STUB_)?END(?:_NO_RETURN)?\s*\(",
    re.DOTALL,
)

FBS_TYPE_BY_C = [
    (r"GLfloat\s*\*", "[float]"),
    (r"GLint\s*\*", "[int]"),
    (r"GLuint\s*\*", "[uint]"),
    (r"GLsizei\s*\*", "[int]"),
    (r"GLboolean\s*\*", "[bool]"),
    (r"GLchar\s*\*", "string"),
    (r"const\s+GLchar\s*\*", "ShmRegion"),
    (r"const\s+void\s*\*", "ShmRegion"),
    (r"void\s*\*", "ShmRegion"),
    (r"GLuint64(?:EXT)?", "ulong"),
    (r"GLint64(?:EXT)?", "long"),
    (r"GLsizeiptr(?:ARB)?", "ulong"),
    (r"GLintptr(?:ARB)?", "long"),
    (r"GLenum", "uint"),
    (r"GLuint", "uint"),
    (r"GLushort", "uint"),
    (r"GLubyte", "uint"),
    (r"GLint", "int"),
    (r"GLshort", "int"),
    (r"GLbyte", "int"),
    (r"GLsizei", "int"),
    (r"GLboolean", "bool"),
    (r"GLfloat", "float"),
    (r"GLclampf", "float"),
    (r"GLdouble", "double"),
    (r"GLclampd", "double"),
    (r"GLfixed", "int"),
    (r"GLhalf", "uint"),
    (r"GLsync", "ulong"),
    (r"GLbitfield", "uint"),
    (r"GLhandleARB", "ulong"),
]


def fbs_type(arg: str) -> str | None:
    arg = arg.strip()
    # Strip default initializers, e.g. `int x = 0`.
    arg = re.sub(r"=\s*[^,]+$", "", arg).strip()
    # A const GLchar* stream (shader source / names) always travels as a
    # ShmRegion payload; do this check before stripping the qualifier.
    if re.search(r"GLchar\s*\*\s*const\s*\*", arg) or re.search(r"const\s+GLchar\s*\*", arg):
        return "ShmRegion"
    # Strip storage qualifiers.
    arg = re.sub(r"\b(?:const|volatile)\s+", "", arg).strip()
    arg = arg.replace("GLvoid", "void")
    for pattern, fbs in FBS_TYPE_BY_C:
        if re.fullmatch(pattern + r"\s*", arg):
            return fbs
    return None


def parse_args(arglist: str) -> list[tuple[str, str]]:
    if not arglist.strip():
        return []
    args: list[tuple[str, str]] = []
    # Split on top-level commas (no nested parens in these signatures).
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
    # Remove parameter name: last token, ignoring pointer stars.
    trimmed = part.strip()
    name_match = re.search(r"([A-Za-z_][A-Za-z0-9_]*)\s*$", trimmed)
    if name_match is None:
        # No named parameter (occurs in C prototypes such as `(void)`); the
        # generator keeps the whole string as the type and synthesizes a name.
        return trimmed, "arg"
    name = name_match.group(1)
    # type text = everything before the name, dropping trailing pointer stars
    # attached to the name.
    type_text = trimmed[: name_match.start()].strip()
    type_text = re.sub(r"\s*\*\s*$", "*", type_text)
    return type_text, name


def main() -> int:
    if not DEFINITIONS.exists():
        print(f"missing {DEFINITIONS}", file=sys.stderr)
        return 1

    text = DEFINITIONS.read_text(encoding="utf-8")
    # Skip the macro definition blocks and comment lines; only the real
    # DECLARE_GL_FUNCTION_* invocations describe API signatures.
    text = "\n".join(
        line for line in text.splitlines()
        if not line.lstrip().startswith("#define") and not line.lstrip().startswith("//")
    )
    tables: list[str] = []
    union_entries: list[str] = []
    seen_names: set[str] = set()

    for match in HEADER_RE.finditer(text):
        c_type, name, arglist = match.group(1).strip(), match.group(2).strip(), match.group(3)
        table_name = "Gl" + name
        if table_name in seen_names:
            continue
        seen_names.add(table_name)
        fields = parse_args(arglist)
        fbs_fields: list[str] = []
        ok = True
        for c_arg_type, arg_name in fields:
            fbs = fbs_type(c_arg_type)
            if fbs is None:
                ok = False
                fbs_fields.append(f"    // TODO: unsupported {c_arg_type} {arg_name}")
            else:
                fbs_fields.append(f"    {arg_name}: {fbs};")
        table_name = "Gl" + name
        tables.append(f"table {table_name} {{\n" + "\n".join(fbs_fields) + "\n}")
        union_entries.append(f"    {table_name},")
        if not ok:
            tables.append(f"// TODO(GEN): incomplete signature for {name}")

    header = (
        "// MobileGL - MobileGL/Protocol/protocol_generated.fbs\n"
        "// Copyright (c) 2025-2026 MobileGL-Dev\n"
        "// Licensed under the GNU Lesser General Public License v3.0:\n"
        "// SPDX-License-Identifier: LGPL-3.0-only\n"
        "// End of Source File Header\n\n"
        "// GENERATED by scripts/generate_protocol_fbs.py\n"
        "// Merge into MobileGL/Protocol/protocol.fbs at the\n"
        "// @INSERTION_POINT:PAYLOAD_CASES@ marker.\n\n"
        "namespace MobileGL.Protocol;\n\n"
    )
    body = "\n".join(tables)
    union = "union GeneratedPayload {\n" + "\n".join(union_entries) + "\n}"
    OUTPUT.write_text(header + body + "\n\n" + union + "\n", encoding="utf-8")
    print(f"Wrote {len(tables)} tables to {OUTPUT.relative_to(REPO_ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
