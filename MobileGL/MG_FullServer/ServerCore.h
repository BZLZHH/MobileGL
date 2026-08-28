// MobileGL - MobileGL/MG_FullServer/ServerCore.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#pragma once

#include <Includes.h>
#include "MG_Protocol/bfa.h"
#include "MG_Protocol/transport.h"

namespace MobileGL::FullServer {
    // Minimal command router for the BigServer side. It owns one transport
    // endpoint plus the backend created from a plugin; ServiceOnce reads one
    // batch, dispatches by opcode into the BFA vtable and writes one response.
    // Phase 4 replaces the hand-rolled header with FlatBuffers decode; the
    // control flow (batch -> dispatch -> response) is the target shape.

    struct CommandHeader {
        uint32_t Opcode;
        uint32_t SessionId;
    };

    struct ResponseHeader {
        uint32_t Status; // 0 = OK
    };

    class ServerCore {
    public:
        ServerCore(const MobileGLTransportOps* ops, MobileGLTransport* transport,
                   MobileGLBackend* backend, const MobileGLBackendVTable* vtable);

        Bool Start();
        Bool ServiceOnce();
        void Shutdown();

    private:
        const MobileGLTransportOps* m_ops = nullptr;
        MobileGLTransport* m_transport = nullptr;
        MobileGLBackend* m_backend = nullptr;
        const MobileGLBackendVTable* m_vtable = nullptr;
        Bool m_running = false;
    };
} // namespace MobileGL::FullServer

// End of File
