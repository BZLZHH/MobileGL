// MobileGL - MobileGL/MG_Protocol/control.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#pragma once

#include <cstdint>

namespace MobileGL::Protocol {
    // Control opcodes live far above the API opcode space (kMobileGLOpcodeCount
    // is ~1400); they never collide with a GL/EGL command.
    enum class MobileGLControlOpcode : uint32_t {
        SessionCreate = 1'000'000,
        SessionDestroy = 1'000'001,
        DisplayCreate = 1'000'002,
        DisplayDestroy = 1'000'003,
        SharedGroupCreate = 1'000'004,
        SharedGroupDestroy = 1'000'005,
        QueryResultAvailable = 1'000'020,
        QueryResult64 = 1'000'021,
        BeginOcclusionQuery = 1'000'022,
        EndOcclusionQuery = 1'000'023,
        BeginXfbPrimitivesQuery = 1'000'024,
        EndXfbPrimitivesQuery = 1'000'025,
        // Wake-up sentinel for hosted server loops: the host owning the loop
        // decides when to exit; this opcode only unblocks WaitResponses so the
        // loop can observe its stop flag.
        ServerShutdown = 1'000'050,
    };
} // namespace MobileGL::Protocol

// End of File
