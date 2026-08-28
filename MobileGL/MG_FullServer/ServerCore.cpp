// MobileGL - MobileGL/MG_FullServer/ServerCore.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "ServerCore.h"
#include "MG_Protocol/control.h"
#include "MG_Protocol/gen/wire_generated.h"
#include "MG_Protocol/generated_opcodes.h"

#include <flatbuffers/flatbuffers.h>

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
        if (in.count == 0 || in.flatBufferData == nullptr) {
            return false;
        }

        const auto* message = MobileGL::Protocol::Wire::GetMessage(in.flatBufferData);
        if (message == nullptr || message->command() == nullptr) {
            return false;
        }
        const auto* command = message->command();

        uint32_t dataByte = 0;
        const uint32_t shmCount = message->shm_count();
        if (shmCount > 0) {
            if (m_ops->ReceiveShmHandle == nullptr) {
                return false;
            }
            for (Uint32 i = 0; i < shmCount; ++i) {
                MobileGLShmHandle handle{};
                if (!m_ops->ReceiveShmHandle(m_transport, &handle)) {
                    return false;
                }
                if (command->data() != nullptr && handle.mappedAddress != nullptr) {
                    dataByte = *static_cast<const Uint8*>(handle.mappedAddress);
                }
                m_ops->ReleaseSharedMemory(m_transport, &handle);
            }
        }

        uint32_t status = 1;
        const uint32_t opcode = command->opcode();
        const auto sessionId = static_cast<MobileGLSessionId>(command->session_id());
        if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLControlOpcode::SessionCreate) &&
            m_vtable->OnSessionCreated != nullptr) {
            status = m_vtable->OnSessionCreated(m_backend, sessionId, nullptr) ? 0 : 1;
            if (status == 0) {
                m_liveSessions[sessionId] = true;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLControlOpcode::SessionDestroy) &&
                   m_vtable->OnSessionDestroyed != nullptr) {
            m_vtable->OnSessionDestroyed(m_backend, sessionId);
            m_liveSessions.erase(sessionId);
            status = 0;
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLControlOpcode::DisplayCreate) &&
                   m_vtable->OnDisplayCreated != nullptr) {
            const auto displayId = static_cast<MobileGLDisplayId>(command->session_id());
            status = m_vtable->OnDisplayCreated(m_backend, displayId, nullptr) ? 0 : 1;
            if (status == 0) {
                m_liveDisplays[displayId] = true;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLControlOpcode::DisplayDestroy) &&
                   m_vtable->OnDisplayDestroyed != nullptr) {
            const auto displayId = static_cast<MobileGLDisplayId>(command->session_id());
            m_vtable->OnDisplayDestroyed(m_backend, displayId);
            m_liveDisplays.erase(displayId);
            status = 0;
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLControlOpcode::SharedGroupCreate) &&
                   m_vtable->OnSharedGroupCreated != nullptr) {
            const auto groupId = static_cast<MobileGLSharedGroupId>(command->session_id());
            status = m_vtable->OnSharedGroupCreated(m_backend, groupId, nullptr) ? 0 : 1;
            if (status == 0) {
                m_liveSharedGroups[groupId] = true;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLControlOpcode::SharedGroupDestroy) &&
                   m_vtable->OnSharedGroupDestroyed != nullptr) {
            const auto groupId = static_cast<MobileGLSharedGroupId>(command->session_id());
            m_vtable->OnSharedGroupDestroyed(m_backend, groupId);
            m_liveSharedGroups.erase(groupId);
            status = 0;
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glClear) &&
                   m_vtable->Clear != nullptr) {
            const uint32_t mask = command->clear() == nullptr ? 0 : command->clear()->mask();
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                m_vtable->Clear(m_backend, sessionId, mask);
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glClearColor) &&
                   m_vtable->ClearColor != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* color = command->clear_color();
                m_vtable->ClearColor(m_backend, sessionId,
                                     color == nullptr ? 0 : color->red(),
                                     color == nullptr ? 0 : color->green(),
                                     color == nullptr ? 0 : color->blue(),
                                     color == nullptr ? 0 : color->alpha());
                status = 0;
            }
        }

        flatbuffers::FlatBufferBuilder responseBuilder;
        const auto response = MobileGL::Protocol::Wire::CreateResponse(responseBuilder, status,
                                                                       command->token(), dataByte);
        responseBuilder.Finish(response);

        MobileGLCommandBatch out{};
        out.structSize = sizeof(MobileGLCommandBatch);
        out.flatBufferData = responseBuilder.GetBufferPointer();
        out.flatBufferSize = static_cast<Uint32>(responseBuilder.GetSize());
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
