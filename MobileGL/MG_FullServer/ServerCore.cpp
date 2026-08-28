// MobileGL - MobileGL/MG_FullServer/ServerCore.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "ServerCore.h"

namespace MobileGL::FullServer {
    ServerCore::ServerCore(const MobileGLTransportOps* ops, MobileGLTransport* transport,
                           MobileGLBackend* backend, const MobileGLBackendVTable* vtable)
        : m_ops(ops),
          m_transport(transport),
          m_backend(backend),
          m_vtable(vtable) {
    }

    Bool ServerCore::Start() {
        if (m_ops == nullptr || m_transport == nullptr || m_backend == nullptr || m_vtable == nullptr) {
            return false;
        }
        if (m_vtable->Initialize != nullptr && !m_vtable->Initialize(m_backend, nullptr)) {
            return false;
        }
        m_running = true;
        return true;
    }

    Bool ServerCore::ServiceOnce() {
        if (!m_running) {
            return false;
        }

        MobileGLResponseQueue in{};
        if (!m_ops->WaitResponses(m_transport, &in, 0)) {
            return false;
        }
        if (in.count == 0 || in.flatBufferData == nullptr || in.flatBufferSize < sizeof(CommandHeader)) {
            return false;
        }

        CommandHeader header{};
        memcpy(&header, in.flatBufferData, sizeof(CommandHeader));

        uint32_t status = 1;
        // Opcode 1 = GlClear (minimal command set for the E2E smoke test).
        if (header.Opcode == 1 && m_vtable->Clear != nullptr) {
            m_vtable->Clear(m_backend, header.SessionId, 0);
            status = 0;
        }

        ResponseHeader response{status};
        MobileGLCommandBatch out{};
        out.structSize = sizeof(MobileGLCommandBatch);
        out.flatBufferData = &response;
        out.flatBufferSize = static_cast<uint32_t>(sizeof(response));
        return m_ops->SubmitCommands(m_transport, &out);
    }

    void ServerCore::Shutdown() {
        if (m_vtable != nullptr && m_vtable->Shutdown != nullptr && m_backend != nullptr) {
            m_vtable->Shutdown(m_backend);
        }
        m_running = false;
    }
} // namespace MobileGL::FullServer

// End of File
