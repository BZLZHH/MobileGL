// MobileGL - MobileGL/MG_FullServer/BfaFrontendShim.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#pragma once
#include <Includes.h>
#include "MG_Protocol/bfa.h"
#include "MG_State/GLState/FramebufferState/FramebufferObject.h"

namespace MobileGL::MG_Backend {
    struct GlobalBackendFunctionsTable;
} // namespace MobileGL::MG_Backend

namespace MobileGL::FullServer {
    // Bridges the legacy frontend backend-function table (gBackendFunctionsTable.GL)
    // to the loaded BFA backend vtable. The FullServer decides which session is
    // current (from ServerCore session create / EGL MakeCurrent), so the legacy
    // frontend calls transparently run on the BFA backend for that session.
    class BfaFrontendShim {
    public:
        struct State;

        static BfaFrontendShim& Get();

        // Populates `table` (or the process-global gBackendFunctionsTable when
        // table==nullptr) with thunks that forward to `vtable` on `backend`.
        void Install(MobileGLBackend* backend, const MobileGLBackendVTable* vtable,
                     MG_Backend::GlobalBackendFunctionsTable* table = nullptr);

        void SetCurrentSession(MobileGLSessionId session);
        MobileGLSessionId GetCurrentSession() const;

        void Clear();

        struct State {
            MobileGLBackend* Backend = nullptr;
            const MobileGLBackendVTable* VTable = nullptr;
            MG_Backend::GlobalBackendFunctionsTable* Table = nullptr;
            MobileGLSessionId Session = 0;
            // Optional fallback: derives the current BFA session from the
            // frontend's current GL context (used when ServerCore has not
            // reported a session yet). Set by FullServerEntry.
            MobileGLSessionId (*SessionProvider)() = nullptr;
            // Maps a frontend GL name to a BFA object handle (e.g. via
            // GLContext::GetObjectHandle). null = pass the name through.
            MobileGLBackendHandle (*HandleProvider)(uint32_t objectKind, uint64_t glName) = nullptr;
            // Extracts the GL name from a frontend framebuffer object; supplied
            // by FullServerEntry so the shim itself has no core dependency.
            Uint (*FramebufferNameProvider)(
                const SharedPtr<MG_State::GLState::FramebufferObject>& framebuffer) = nullptr;
        };

        State m_state{};

    private:
        BfaFrontendShim() = default;
    };
} // namespace MobileGL::FullServer

// End of File
