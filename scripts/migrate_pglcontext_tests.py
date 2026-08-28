#!/usr/bin/env python3
"""
MobileGL - scripts/migrate_pglcontext_tests.py
Copyright (c) 2025-2026 MobileGL-Dev
Licensed under the GNU Lesser General Public License v3.0:
  https://www.gnu.org/licenses/gpl-3.0.txt
  https://www.gnu.org/licenses/lgpl-3.0.txt
SPDX-License-Identifier: LGPL-3.0-only

Migrate MG_Test sources from `UniquePtr<GLContext>& pGLContext` to the
non-owning `GLContext* pGLContext` + Set/Take/ResetLegacyCurrentContext helpers.
Only rewrites the assignment/ownership sites; `pGLContext->` call sites stay.
"""
from __future__ import annotations

from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
TEST_DIR = REPO_ROOT / "MobileGL/MG_Test"

REPLACEMENTS = [
    (
        "MobileGL::MG_State::pGLContext = MobileGL::MakeUnique<MobileGL::MG_State::GLState::GLContext>();",
        "MobileGL::MG_State::SetLegacyCurrentContext(MobileGL::MakeUnique<MobileGL::MG_State::GLState::GLContext>());",
    ),
    (
        "MG_State::pGLContext = MakeUnique<MG_State::GLState::GLContext>();",
        "MG_State::SetLegacyCurrentContext(MakeUnique<MG_State::GLState::GLContext>());",
    ),
    (
        "MobileGL::MG_State::pGLContext = MobileGL::Move(previousContext);",
        "MobileGL::MG_State::SetLegacyCurrentContext(MobileGL::Move(previousContext));",
    ),
    (
        "MG_State::pGLContext = Move(previousContext);",
        "MG_State::SetLegacyCurrentContext(Move(previousContext));",
    ),
    (
        "MG_State::pGLContext.reset();",
        "MG_State::ResetLegacyCurrentContext();",
    ),
    (
        "auto previousContext = Move(MG_State::pGLContext);",
        "auto previousContext = MG_State::TakeLegacyCurrentContext();",
    ),
    (
        "previousContext(MobileGL::Move(MobileGL::MG_State::pGLContext))",
        "previousContext(MobileGL::MG_State::TakeLegacyCurrentContext())",
    ),
]


def main() -> int:
    changed = 0
    for path in sorted(TEST_DIR.rglob("*.cpp")):
        text = path.read_text(encoding="utf-8")
        original = text
        for old, new in REPLACEMENTS:
            text = text.replace(old, new)
        if text != original:
            path.write_text(text, encoding="utf-8")
            changed += 1
            print(f"updated {path.relative_to(REPO_ROOT)}")
    print(f"changed {changed} files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
