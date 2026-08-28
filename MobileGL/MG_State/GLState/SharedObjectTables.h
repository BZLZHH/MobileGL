// MobileGL - MobileGL/MG_State/GLState/SharedObjectTables.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#pragma once

#include <Includes.h>
#include "MG_State/GLState/BufferState/BufferState.h"

namespace MobileGL::MG_State::GLState {
    // The GL object tables that are shared between every session of a
    // SharedGroup (GL shareCtx semantics): buffers, textures, samplers, VAOs,
    // programs, framebuffers, renderbuffers, transform feedbacks. Session
    // private objects (default objects, queries, syncs) stay in GLContext.
    //
    // Phase 2 migration: the existing GLContext-owned *State instances move
    // into this container incrementally, starting with BufferState.
    class SharedObjectTables {
    public:
        BufferState& GetBufferState() { return m_bufferState; }
        const BufferState& GetBufferState() const { return m_bufferState; }

    private:
        BufferState m_bufferState;
    };
} // namespace MobileGL::MG_State::GLState

// End of File
