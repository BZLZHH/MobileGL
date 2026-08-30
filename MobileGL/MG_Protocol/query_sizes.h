// MobileGL - MobileGL/MG_Protocol/query_sizes.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#pragma once

#include <cstdint>
#include <GL/gl.h>

namespace MobileGL::Protocol {
    // Shared pname -> element-count table for no-size out-parameter queries
    // (glGetIntegerv / glGetFloatv / glGetDoublev / glGetBooleanv /
    // glGetInteger64v). Both the server-side generated dispatch and the
    // client-side generated trampoline use this table, so the bytes the server
    // allocates and writes are exactly the bytes the client copies back: the
    // caller's buffer is never overrun and multi-value queries are never
    // truncated. Unknown pnames fall back to 1 element, the common case for
    // the GL_MAX_* / binding / state queries.
    //
    // WARNING: keep every count equal to what the GL spec returns for that
    // query; a count that is too small truncates, one that is too large can
    // overrun the caller's buffer.
    inline std::uint32_t MobileGLQueryCount(std::uint32_t pname) {
        switch (pname) {
            case GL_VIEWPORT: return 4;
            case GL_SCISSOR_BOX: return 4;
            case GL_COLOR_WRITEMASK: return 4;
            case GL_DEPTH_RANGE: return 2;
            case GL_BLEND_COLOR: return 4;
            case GL_ALIASED_LINE_WIDTH_RANGE: return 2;
            case GL_ALIASED_POINT_SIZE_RANGE: return 2;
            default: return 1;
        }
    }
} // namespace MobileGL::Protocol

// End of File
