#!/usr/bin/env python3
"""
MobileGL - scripts/fix_cs_dir_paths.py
Copyright (c) 2025-2026 MobileGL-Dev
Licensed under the GNU Lesser General Public License v3.0:
  https://www.gnu.org/licenses/gpl-3.0.txt
  https://www.gnu.org/licenses/lgpl-3.0.txt
SPDX-License-Identifier: LGPL-3.0-only

Update path references after moving the C/S modules under the house `MG_*`
directory convention (MG_Protocol / MG_Client / MG_FullServer / MG_Transport /
MG_UtilRuntime).
"""
from __future__ import annotations

from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]

WALK_DIRS = [
    REPO_ROOT / "MobileGL",
    REPO_ROOT / "scripts",
    REPO_ROOT / "docs",
]

REPLACEMENTS = {
    "MobileGL/MG_Protocol": "MobileGL/MG_Protocol",
    "\"MG_Protocol/": "\"MG_Protocol/",
    "'MG_Protocol/": "'MG_Protocol/",
    "MobileGL/MG_Client": "MobileGL/MG_Client",
    "MobileGL/MG_FullServer": "MobileGL/MG_FullServer",
    "\"MG_FullServer/": "\"MG_FullServer/",
    "MobileGL/MG_Transport": "MobileGL/MG_Transport",
    "\"MG_Transport/": "\"MG_Transport/",
    "MobileGL/MG_UtilRuntime": "MobileGL/MG_UtilRuntime",
}


def main() -> int:
    changed = 0
    files = 0
    for root in WALK_DIRS:
        for path in root.rglob("*"):
            if not path.is_file():
                continue
            try:
                text = path.read_text(encoding="utf-8")
            except UnicodeDecodeError:
                continue
            original = text
            for old, new in REPLACEMENTS.items():
                text = text.replace(old, new)
            if text != original:
                path.write_text(text, encoding="utf-8")
                files += 1
                changed += sum(original.count(old) for old in REPLACEMENTS)
                print(f"updated {path.relative_to(REPO_ROOT)}")
    print(f"changed {files} files, {changed} replacements")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
