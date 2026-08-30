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
        using SessionListener = void (*)(MobileGLSessionId sessionId, void* user);
        // Optional generated wire dispatch hook (set by FullServerEntry to
        // WireDispatchCall). It decodes Command.payload_bytes and executes the
        // full source-list GL/EGL API surface; tests may leave it null.
        using WireDispatchFn = uint32_t (*)(uint32_t opcode,
                                            uint32_t sessionId,
                                            const void* payloadBytes,
                                            uint64_t payloadSize,
                                            const void* const* receivedShm,
                                            uint32_t receivedShmCount,
                                            uint32_t outCapacity);
        // Optional scalar/out-vector return provider, forwarded from the
        // generated wire dispatch by the FullServer host. Kept as a callback
        // so ServerCore tests can compile without linking
        // generated_wire_dispatch.cpp.
        using WireRetFn = void (*)(Bool* outValid, int64_t* outRetI64,
                                   const Uint8** outBytes, Uint32* outBytesSize);

        ServerCore(const MobileGLTransportOps* ops, MobileGLTransport* transport,
                   MobileGLBackend* backend, const MobileGLBackendVTable* vtable);

        Bool Start();
        Bool ServiceOnce();
        void Shutdown();

        // Optional hook fired with the created session id (or 0 on destroy) so
        // the FullServer can keep the frontend shim's current session in sync.
        void SetSessionListener(SessionListener listener, void* user);
        void SetWireDispatch(WireDispatchFn fn);
        void SetWireRet(WireRetFn fn);

    private:
        void NotifySessionChanged(MobileGLSessionId sessionId);

        const MobileGLTransportOps* m_ops = nullptr;
        MobileGLTransport* m_transport = nullptr;
        MobileGLBackend* m_backend = nullptr;
        const MobileGLBackendVTable* m_vtable = nullptr;
        Bool m_running = false;
        SessionListener m_sessionListener = nullptr;
        void* m_sessionUser = nullptr;
        WireDispatchFn m_wireDispatch = nullptr;
        WireRetFn m_wireRet = nullptr;
        // Sessions created through SessionCreate control commands; commands for
        // unknown/destroyed sessions are rejected until re-created.
        UnorderedMap<MobileGLSessionId, Bool> m_liveSessions;
        UnorderedMap<MobileGLDisplayId, Bool> m_liveDisplays;
        UnorderedMap<MobileGLSharedGroupId, Bool> m_liveSharedGroups;
    };
} // namespace MobileGL::FullServer

// End of File
