#!/usr/bin/env python3
"""
MobileGL - scripts/merge_protocol_fbs.py
Copyright (c) 2025-2026 MobileGL-Dev
Licensed under the GNU Lesser General Public License v3.0:
  https://www.gnu.org/licenses/gpl-3.0.txt
  https://www.gnu.org/licenses/lgpl-3.0.txt
SPDX-License-Identifier: LGPL-3.0-only

Merge the full generated payload tables (protocol_generated.fbs) into the
seed protocol.fbs at @INSERTION_POINT:PAYLOAD_CASES@, producing a flatc-valid
protocol_full.fbs with no duplicate table definitions.
"""
from __future__ import annotations

import re
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
SEED = REPO_ROOT / "MobileGL/MG_Protocol/protocol.fbs"
GENERATED = REPO_ROOT / "MobileGL/MG_Protocol/protocol_generated.fbs"
OUTPUT = REPO_ROOT / "MobileGL/MG_Protocol/protocol_full.fbs"
MARKER = "@INSERTION_POINT:PAYLOAD_CASES@"

TABLE_RE = re.compile(r"^\s*table\s+(\w+)\s*\{", re.MULTILINE)
UNION_RE = re.compile(r"^\s*union\s+(\w+)\s*\{", re.MULTILINE)
DEF_BRACE_RE = re.compile(r"^\s*(?:table|union)\s+(\w+)\s*\{", re.MULTILINE)
BRACE_RE = re.compile(r"[{}]")


def remove_top_level_union(text: str, name: str) -> str:
    """Remove one top-level `union name { ... }` block from schema text."""
    pattern = re.compile(rf"^\s*union\s+{name}\s*\{{", re.MULTILINE)
    match = pattern.search(text)
    if match is None:
        return text
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
    return text[:match.start()] + text[cursor + 1:]


def collect_tables(text: str) -> tuple[dict[str, str], list[int]]:
    """Return {name: block_source} and byte offsets of each table block start."""
    tables: dict[str, str] = {}
    positions: list[int] = []
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
        end = cursor + 1
        block = text[start:end]
        tables[name] = block
        positions.append(start)
        pos = end
    return tables, positions


def main() -> int:
    seed = SEED.read_text(encoding="utf-8")
    generated = GENERATED.read_text(encoding="utf-8")

    if MARKER not in seed:
        print(f"marker {MARKER} not found in {SEED}", file=__import__("sys").stderr)
        return 1

    generated_tables, _ = collect_tables(generated)
    seed_tables, seed_positions = collect_tables(seed)
    marker_index = seed.find(MARKER)

    # Remove seed tables that are redefined by the generated full payload set.
    pieces: list[str] = []
    last = 0
    for start in sorted(seed_positions):
        if start >= marker_index:
            break
        name = re.match(r"^\s*table\s+(\w+)\s*\{", seed[start:]).group(1)
        if name in generated_tables:
            end = seed.find("}", start) + 1
            if start > last:
                pieces.append(seed[last:start])
            last = end
    before_marker = "".join(pieces) + seed[last:marker_index]

    # Generated file's own header comment + namespace are harmless; keep them.
    merged = before_marker + generated + seed[marker_index + len(MARKER):]
    # The generated union has >255 members, which flatc rejects; dispatch uses
    # the opcode table instead, so the union is dropped.
    merged = remove_top_level_union(merged, "GeneratedPayload")

    OUTPUT.write_text(merged, encoding="utf-8")
    print(f"Wrote {OUTPUT.relative_to(REPO_ROOT)}")
    print(f"generated tables={len(generated_tables)} seed tables removed={len(seed_tables)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
