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
        uint64_t syncHandle = 0;
        uint64_t queryNs = 0;
        String responseStringValue;
        Vector<MobileGLShmHandle> receivedShm;
        Vector<MobileGLShmHandle> responseShm;
        Uint32 responseShmCount = 0;
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
                receivedShm.push_back(handle);
            }
        }

        uint32_t status = 1;
        const uint32_t opcode = command->opcode();
        const auto sessionId = static_cast<MobileGLSessionId>(command->session_id());

        // Full source-list path: when the client carries a generic payload_bytes
        // blob (WireFull Gen* table) the generated dispatch hook executes the
        // complete GL/EGL frontend API surface. Typed legacy commands below
        // remain the v1 fast path and are still served first when no hook is
        // installed (e.g. standalone unit tests).
        Bool wireDispatched = false;
        if (command->payload_bytes() != nullptr && m_wireDispatch != nullptr) {
            Vector<const void*> shmPointers;
            shmPointers.reserve(receivedShm.size());
            for (const auto& shm : receivedShm) {
                shmPointers.push_back(shm.mappedAddress);
            }
            status = m_wireDispatch(opcode, static_cast<uint32_t>(sessionId),
                                    command->payload_bytes()->data(),
                                    command->payload_bytes()->size(),
                                    shmPointers.empty() ? nullptr : shmPointers.data(),
                                    static_cast<uint32_t>(shmPointers.size()));
            wireDispatched = true;
        }

        if (!wireDispatched) {
        if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLControlOpcode::SessionCreate) &&
            m_vtable->OnSessionCreated != nullptr) {
            status = m_vtable->OnSessionCreated(m_backend, sessionId, nullptr) ? 0 : 1;
            if (status == 0) {
                m_liveSessions[sessionId] = true;
                NotifySessionChanged(sessionId);
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLControlOpcode::SessionDestroy) &&
                   m_vtable->OnSessionDestroyed != nullptr) {
            m_vtable->OnSessionDestroyed(m_backend, sessionId);
            m_liveSessions.erase(sessionId);
            NotifySessionChanged(0);
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
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glDrawArrays) &&
                   m_vtable->DrawArrays != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* draw = command->draw_arrays();
                m_vtable->DrawArrays(m_backend, sessionId,
                                     draw == nullptr ? 0 : draw->mode(),
                                     draw == nullptr ? 0 : draw->first(),
                                     draw == nullptr ? 0 : draw->count());
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glDrawElements) &&
                   m_vtable->DrawElements != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* draw = command->draw_elements();
                const void* indices = nullptr;
                if (!receivedShm.empty() && receivedShm[0].mappedAddress != nullptr) {
                    const auto* base = static_cast<const Uint8*>(receivedShm[0].mappedAddress);
                    indices = base + (draw == nullptr ? 0 : draw->indices_offset());
                }
                m_vtable->DrawElements(m_backend, sessionId,
                                       draw == nullptr ? 0 : draw->mode(),
                                       draw == nullptr ? 0 : draw->count(),
                                       draw == nullptr ? 0 : draw->type(),
                                       indices);
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glBufferSubData) &&
                   m_vtable->BufferSubData != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* bs = command->buffer_sub_data();
                const void* data = receivedShm.empty() || receivedShm[0].mappedAddress == nullptr
                                       ? nullptr
                                       : receivedShm[0].mappedAddress;
                m_vtable->BufferSubData(m_backend, sessionId,
                                        bs == nullptr ? 0 : bs->buffer_handle(),
                                        bs == nullptr ? 0 : bs->offset(),
                                        bs == nullptr ? 0 : bs->size(),
                                        data);
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glMemoryBarrier) &&
                   m_vtable->MemoryBarrier != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* mb = command->memory_barrier();
                m_vtable->MemoryBarrier(m_backend, sessionId, mb == nullptr ? 0 : mb->barriers());
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glMemoryBarrierByRegion) &&
                   m_vtable->MemoryBarrierByRegion != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* mb = command->memory_barrier_by_region();
                m_vtable->MemoryBarrierByRegion(m_backend, sessionId,
                                                mb == nullptr ? 0 : mb->barriers());
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glPatchParameteri) &&
                   m_vtable->PatchParameteri != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* pp = command->patch_parameteri();
                m_vtable->PatchParameteri(m_backend, sessionId,
                                          pp == nullptr ? 0 : pp->pname(),
                                          pp == nullptr ? 0 : pp->value());
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glGenerateMipmap) &&
                   m_vtable->GenerateMipmap != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* gm = command->generate_mipmap();
                m_vtable->GenerateMipmap(m_backend, sessionId, gm == nullptr ? 0 : gm->target());
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glDispatchCompute) &&
                   m_vtable->DispatchCompute != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* dc = command->dispatch_compute();
                m_vtable->DispatchCompute(m_backend, sessionId,
                                          dc == nullptr ? 0 : dc->num_groups_x(),
                                          dc == nullptr ? 0 : dc->num_groups_y(),
                                          dc == nullptr ? 0 : dc->num_groups_z());
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glBeginTransformFeedback) &&
                   m_vtable->BeginTransformFeedback != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* btf = command->begin_transform_feedback();
                m_vtable->BeginTransformFeedback(m_backend, sessionId,
                                                 btf == nullptr ? 0 : btf->primitive_mode());
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glEndTransformFeedback) &&
                   m_vtable->EndTransformFeedback != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                m_vtable->EndTransformFeedback(m_backend, sessionId);
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glPauseTransformFeedback) &&
                   m_vtable->PauseTransformFeedback != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                m_vtable->PauseTransformFeedback(m_backend, sessionId);
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glResumeTransformFeedback) &&
                   m_vtable->ResumeTransformFeedback != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                m_vtable->ResumeTransformFeedback(m_backend, sessionId);
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glBindTransformFeedback) &&
                   m_vtable->BindTransformFeedback != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* btf = command->bind_transform_feedback();
                m_vtable->BindTransformFeedback(m_backend, sessionId,
                                                btf == nullptr ? 0 : btf->name());
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glBlitFramebuffer) &&
                   m_vtable->BlitFramebuffer != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* blit = command->blit_framebuffer();
                m_vtable->BlitFramebuffer(m_backend, sessionId,
                                          blit == nullptr ? 0 : blit->read_framebuffer(),
                                          blit == nullptr ? 0 : blit->draw_framebuffer(),
                                          blit == nullptr ? 0 : blit->src_x0(),
                                          blit == nullptr ? 0 : blit->src_y0(),
                                          blit == nullptr ? 0 : blit->src_x1(),
                                          blit == nullptr ? 0 : blit->src_y1(),
                                          blit == nullptr ? 0 : blit->dst_x0(),
                                          blit == nullptr ? 0 : blit->dst_y0(),
                                          blit == nullptr ? 0 : blit->dst_x1(),
                                          blit == nullptr ? 0 : blit->dst_y1(),
                                          blit == nullptr ? 0 : blit->mask(),
                                          blit == nullptr ? 0 : blit->filter());
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::eglSwapBuffers) &&
                   m_vtable->SwapBuffers != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* swp = command->swap_buffers();
                status = m_vtable->SwapBuffers(m_backend, sessionId,
                                               swp == nullptr ? 0 : swp->draw()) ? 0 : 1;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glDispatchComputeIndirect) &&
                   m_vtable->DispatchComputeIndirect != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* dci = command->dispatch_compute_indirect();
                m_vtable->DispatchComputeIndirect(m_backend, sessionId,
                                                  dci == nullptr ? 0 : dci->indirect_offset());
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glFenceSync) &&
                   m_vtable->FenceSync != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* fs = command->fence_sync();
                void* sync = m_vtable->FenceSync(m_backend, sessionId,
                                                 fs == nullptr ? 0 : fs->condition(),
                                                 fs == nullptr ? 0 : fs->flags());
                syncHandle = reinterpret_cast<uint64_t>(sync);
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glDeleteSync) &&
                   m_vtable->DeleteSync != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* ds = command->delete_sync();
                m_vtable->DeleteSync(m_backend, sessionId,
                                     reinterpret_cast<void*>(ds == nullptr ? 0 : ds->sync()));
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glWaitSync) &&
                   m_vtable->WaitSync != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* ws = command->wait_sync();
                m_vtable->WaitSync(m_backend, sessionId,
                                   reinterpret_cast<void*>(ws == nullptr ? 0 : ws->sync()),
                                   ws == nullptr ? 0 : ws->flags(),
                                   ws == nullptr ? 0 : ws->timeout());
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glClientWaitSync) &&
                   m_vtable->ClientWaitSync != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* cws = command->client_wait_sync();
                syncHandle = m_vtable->ClientWaitSync(
                    m_backend, sessionId,
                    reinterpret_cast<void*>(cws == nullptr ? 0 : cws->sync()),
                    cws == nullptr ? 0 : cws->flags(),
                    cws == nullptr ? 0 : cws->timeout());
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glBeginQueryIndexed) &&
                   m_vtable->BeginTimeElapsedQuery != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                syncHandle = reinterpret_cast<uint64_t>(
                    m_vtable->BeginTimeElapsedQuery(m_backend, sessionId));
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glEndQueryIndexed) &&
                   m_vtable->EndTimeElapsedQuery != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* eq = command->end_time_elapsed_query();
                m_vtable->EndTimeElapsedQuery(m_backend, sessionId,
                                              reinterpret_cast<void*>(eq == nullptr ? 0 : eq->query()));
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glDeleteQueries) &&
                   m_vtable->DeleteBackendQuery != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* dq = command->delete_backend_query();
                m_vtable->DeleteBackendQuery(m_backend, sessionId,
                                             reinterpret_cast<void*>(dq == nullptr ? 0 : dq->query()));
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLControlOpcode::QueryResultAvailable) &&
                   m_vtable->IsQueryResultAvailable != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* q = command->is_query_result_available();
                dataByte = m_vtable->IsQueryResultAvailable(
                               m_backend, sessionId,
                               reinterpret_cast<void*>(q == nullptr ? 0 : q->query()))
                               ? 1
                               : 0;
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLControlOpcode::QueryResult64) &&
                   m_vtable->GetQueryResult64 != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* q = command->get_query_result64();
                uint64_t ns = 0;
                dataByte = m_vtable->GetQueryResult64(
                               m_backend, sessionId,
                               reinterpret_cast<void*>(q == nullptr ? 0 : q->query()),
                               q == nullptr ? 0 : q->wait(), &ns)
                               ? 1
                               : 0;
                queryNs = ns;
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLControlOpcode::BeginOcclusionQuery) &&
                   m_vtable->BeginOcclusionQuery != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                syncHandle = reinterpret_cast<uint64_t>(
                    m_vtable->BeginOcclusionQuery(m_backend, sessionId));
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLControlOpcode::EndOcclusionQuery) &&
                   m_vtable->EndOcclusionQuery != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* q = command->end_occlusion_query();
                m_vtable->EndOcclusionQuery(
                    m_backend, sessionId,
                    reinterpret_cast<void*>(q == nullptr ? 0 : q->query()));
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLControlOpcode::BeginXfbPrimitivesQuery) &&
                   m_vtable->BeginXfbPrimitivesQuery != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* q = command->begin_xfb_primitives_query();
                syncHandle = reinterpret_cast<uint64_t>(
                    m_vtable->BeginXfbPrimitivesQuery(m_backend, sessionId,
                                                      q != nullptr && q->generated() != 0));
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLControlOpcode::EndXfbPrimitivesQuery) &&
                   m_vtable->EndXfbPrimitivesQuery != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* q = command->end_xfb_primitives_query();
                m_vtable->EndXfbPrimitivesQuery(
                    m_backend, sessionId,
                    reinterpret_cast<void*>(q == nullptr ? 0 : q->query()));
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glDrawArraysInstanced) &&
                   m_vtable->DrawArraysInstanced != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* dai = command->draw_arrays_instanced();
                m_vtable->DrawArraysInstanced(m_backend, sessionId,
                                              dai == nullptr ? 0 : dai->mode(),
                                              dai == nullptr ? 0 : dai->first(),
                                              dai == nullptr ? 0 : dai->count(),
                                              dai == nullptr ? 0 : dai->primcount());
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glDrawElementsInstanced) &&
                   m_vtable->DrawElementsInstanced != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* dei = command->draw_elements_instanced();
                const void* indices = nullptr;
                if (!receivedShm.empty() && receivedShm[0].mappedAddress != nullptr) {
                    const auto* base = static_cast<const Uint8*>(receivedShm[0].mappedAddress);
                    indices = base + (dei == nullptr ? 0 : dei->indices_offset());
                }
                m_vtable->DrawElementsInstanced(m_backend, sessionId,
                                                dei == nullptr ? 0 : dei->mode(),
                                                dei == nullptr ? 0 : dei->count(),
                                                dei == nullptr ? 0 : dei->type(),
                                                indices,
                                                dei == nullptr ? 0 : dei->instance_count());
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glDrawRangeElements) &&
                   m_vtable->DrawRangeElements != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* dre = command->draw_range_elements();
                const void* indices = nullptr;
                if (!receivedShm.empty() && receivedShm[0].mappedAddress != nullptr) {
                    const auto* base = static_cast<const Uint8*>(receivedShm[0].mappedAddress);
                    indices = base + (dre == nullptr ? 0 : dre->indices_offset());
                }
                m_vtable->DrawRangeElements(m_backend, sessionId,
                                            dre == nullptr ? 0 : dre->mode(),
                                            dre == nullptr ? 0 : dre->start(),
                                            dre == nullptr ? 0 : dre->end(),
                                            dre == nullptr ? 0 : dre->count(),
                                            dre == nullptr ? 0 : dre->type(),
                                            indices);
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glBufferData) &&
                   m_vtable->BufferRespecify != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* br = command->buffer_respecify();
                const void* data = receivedShm.empty() || receivedShm[0].mappedAddress == nullptr
                                       ? nullptr
                                       : receivedShm[0].mappedAddress;
                MobileGLBufferOps ops{data, br == nullptr ? 0 : br->size()};
                m_vtable->BufferRespecify(m_backend, sessionId,
                                          br == nullptr ? 0 : br->buffer_handle(),
                                          br == nullptr ? 0 : br->size(),
                                          br == nullptr ? 0 : br->usage(),
                                          data == nullptr ? nullptr : &ops);
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glDrawArraysIndirect) &&
                   m_vtable->DrawArraysIndirect != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* dai = command->draw_arrays_indirect();
                const void* indirect = nullptr;
                if (!receivedShm.empty() && receivedShm[0].mappedAddress != nullptr) {
                    const auto* base = static_cast<const Uint8*>(receivedShm[0].mappedAddress);
                    indirect = base + (dai == nullptr ? 0 : dai->shm_offset());
                }
                m_vtable->DrawArraysIndirect(m_backend, sessionId,
                                             dai == nullptr ? 0 : dai->mode(), indirect);
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glDrawElementsIndirect) &&
                   m_vtable->DrawElementsIndirect != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* dei = command->draw_elements_indirect();
                const void* indirect = nullptr;
                if (!receivedShm.empty() && receivedShm[0].mappedAddress != nullptr) {
                    const auto* base = static_cast<const Uint8*>(receivedShm[0].mappedAddress);
                    indirect = base + (dei == nullptr ? 0 : dei->shm_offset());
                }
                m_vtable->DrawElementsIndirect(m_backend, sessionId,
                                               dei == nullptr ? 0 : dei->mode(),
                                               dei == nullptr ? 0 : dei->type(),
                                               indirect);
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glGetString) &&
                   m_vtable->GetRendererInfo != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* gs = command->get_string();
                const auto* info = m_vtable->GetRendererInfo(m_backend, sessionId);
                const char* value = nullptr;
                if (info != nullptr && gs != nullptr) {
                    switch (gs->pname()) {
                    case GL_VENDOR:
                        value = info->vendor;
                        break;
                    case GL_RENDERER:
                        value = info->name;
                        break;
                    case GL_VERSION:
                        value = info->version;
                        break;
                    case GL_SHADING_LANGUAGE_VERSION:
                        value = info->shaderLanguageVersion;
                        break;
                    default:
                        break;
                    }
                }
                if (value == nullptr) {
                    status = 1;
                } else {
                    responseStringValue = value;
                    status = 0;
                }
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glTexImage2D) &&
                   m_vtable->TextureRespecify != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* tr = command->texture_respecify();
                const void* data = receivedShm.empty() || receivedShm[0].mappedAddress == nullptr
                                       ? nullptr
                                       : receivedShm[0].mappedAddress;
                MobileGLTextureUpload upload{};
                upload.level = tr == nullptr ? 0 : tr->level();
                upload.layer = 0;
                upload.format = tr == nullptr ? 0 : tr->format();
                upload.type = tr == nullptr ? 0 : tr->type();
                upload.width = tr == nullptr ? 0 : tr->width();
                upload.height = tr == nullptr ? 0 : tr->height();
                upload.depth = tr == nullptr ? 1 : tr->depth();
                upload.data = data;
                upload.dataSize = tr == nullptr ? 0 : tr->data_size();
                m_vtable->TextureRespecify(m_backend, sessionId,
                                           tr == nullptr ? 0 : tr->texture(), &upload, 1);
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glTexSubImage2D) &&
                   m_vtable->TextureSubImage != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* ts = command->texture_sub_image();
                const void* data = receivedShm.empty() || receivedShm[0].mappedAddress == nullptr
                                       ? nullptr
                                       : receivedShm[0].mappedAddress;
                MobileGLTextureUpload upload{};
                upload.level = ts == nullptr ? 0 : ts->level();
                upload.layer = 0;
                upload.format = ts == nullptr ? 0 : ts->format();
                upload.type = ts == nullptr ? 0 : ts->type();
                upload.width = ts == nullptr ? 0 : ts->width();
                upload.height = ts == nullptr ? 0 : ts->height();
                upload.depth = ts == nullptr ? 1 : ts->depth();
                upload.data = data;
                upload.dataSize = ts == nullptr ? 0 : ts->data_size();
                m_vtable->TextureSubImage(m_backend, sessionId,
                                          ts == nullptr ? 0 : ts->texture(), &upload);
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glReadPixels) &&
                   m_vtable->ReadPixels != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else if (m_ops->OpenSharedMemory == nullptr ||
                       !m_ops->OpenSharedMemory(m_transport, &responseShm.emplace_back())) {
                responseShm.clear();
                status = 1;
            } else {
                const auto* rp = command->read_pixels();
                m_vtable->ReadPixels(m_backend, sessionId,
                                     rp == nullptr ? 0 : rp->x(),
                                     rp == nullptr ? 0 : rp->y(),
                                     rp == nullptr ? 0 : rp->width(),
                                     rp == nullptr ? 0 : rp->height(),
                                     rp == nullptr ? 0 : rp->format(),
                                     rp == nullptr ? 0 : rp->type(),
                                     responseShm.back().mappedAddress);
                responseShmCount = 1;
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glGetBufferSubData) &&
                   m_vtable->BufferReadbackFromGpu != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else if (m_ops->OpenSharedMemory == nullptr ||
                       !m_ops->OpenSharedMemory(m_transport, &responseShm.emplace_back())) {
                responseShm.clear();
                status = 1;
            } else {
                const auto* rb = command->buffer_readback_from_gpu();
                const Bool readbackOk = m_vtable->BufferReadbackFromGpu(
                    m_backend, sessionId,
                    rb == nullptr ? 0 : rb->buffer_handle(),
                    rb == nullptr ? 0 : rb->offset(),
                    rb == nullptr ? 0 : rb->size(),
                    responseShm.back().mappedAddress);
                if (readbackOk) {
                    responseShmCount = 1;
                    status = 0;
                } else {
                    responseShm.clear();
                    status = 1;
                }
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glMapBufferRange) &&
                   m_vtable->BufferReadbackFromGpu != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else if (m_ops->OpenSharedMemory == nullptr ||
                       !m_ops->OpenSharedMemory(m_transport, &responseShm.emplace_back())) {
                responseShm.clear();
                status = 1;
            } else {
                const auto* mr = command->map_buffer_range();
                const Bool readbackOk = m_vtable->BufferReadbackFromGpu(
                    m_backend, sessionId,
                    mr == nullptr ? 0 : mr->buffer_handle(),
                    mr == nullptr ? 0 : mr->offset(),
                    mr == nullptr ? 0 : mr->size(),
                    responseShm.back().mappedAddress);
                if (readbackOk) {
                    responseShmCount = 1;
                    status = 0;
                } else {
                    responseShm.clear();
                    status = 1;
                }
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glUnmapBuffer) &&
                   m_vtable->BufferSubData != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* um = command->unmap_buffer();
                const void* data = receivedShm.empty() || receivedShm[0].mappedAddress == nullptr
                                       ? nullptr
                                       : receivedShm[0].mappedAddress;
                m_vtable->BufferSubData(m_backend, sessionId,
                                        um == nullptr ? 0 : um->buffer_handle(),
                                        um == nullptr ? 0 : um->offset(),
                                        um == nullptr ? 0 : um->size(),
                                        data);
                status = 0;
            }
        } else if (opcode ==
                   static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::eglCreatePbufferSurface) &&
                   m_vtable->CreatePbufferSurface != nullptr) {
            const auto* ps = command->egl_create_pbuffer_surface();
            const auto displayId = static_cast<MobileGLDisplayId>(ps == nullptr ? 0 : ps->display());
            if (m_liveDisplays.find(displayId) == m_liveDisplays.end()) {
                status = 1;
            } else {
                status = m_vtable->CreatePbufferSurface(m_backend, displayId,
                                                        ps == nullptr ? 0 : ps->surface(),
                                                        ps == nullptr ? 0 : ps->width(),
                                                        ps == nullptr ? 0 : ps->height())
                            ? 0
                            : 1;
            }
        } else if (opcode ==
                   static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::eglDestroySurface) &&
                   m_vtable->ReleaseEGLSurface != nullptr) {
            const auto* ds = command->egl_destroy_surface();
            const auto displayId = static_cast<MobileGLDisplayId>(ds == nullptr ? 0 : ds->display());
            if (m_liveDisplays.find(displayId) == m_liveDisplays.end()) {
                status = 1;
            } else {
                m_vtable->ReleaseEGLSurface(m_backend, displayId,
                                            ds == nullptr ? 0 : ds->surface());
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::eglMakeCurrent) &&
                   m_vtable->MakeEGLCurrent != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* mc = command->egl_make_current();
                status = m_vtable->MakeEGLCurrent(m_backend, sessionId,
                                                  mc == nullptr ? 0 : mc->draw(),
                                                  mc == nullptr ? 0 : mc->read())
                            ? 0
                            : 1;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::eglSwapInterval) &&
                   m_vtable->SetSwapInterval != nullptr) {
            if (m_liveSessions.find(sessionId) == m_liveSessions.end()) {
                status = 1;
            } else {
                const auto* si = command->egl_swap_interval();
                m_vtable->SetSwapInterval(m_backend, sessionId, si == nullptr ? 0 : si->interval());
                status = 0;
            }
        } else if (opcode == static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::eglSurfaceAttrib) &&
                   m_vtable->ResizeSurface != nullptr) {
            const auto* rs = command->egl_resize_surface();
            const auto displayId = static_cast<MobileGLDisplayId>(rs == nullptr ? 0 : rs->display());
            if (m_liveDisplays.find(displayId) == m_liveDisplays.end()) {
                status = 1;
            } else {
                status = m_vtable->ResizeSurface(m_backend, displayId,
                                                 rs == nullptr ? 0 : rs->surface(),
                                                 rs == nullptr ? 0 : rs->width(),
                                                 rs == nullptr ? 0 : rs->height())
                            ? 0
                            : 1;
            }
        }
        }

        for (auto& handle : receivedShm) {
            m_ops->ReleaseSharedMemory(m_transport, &handle);
        }

        flatbuffers::FlatBufferBuilder responseBuilder;
        flatbuffers::Offset<flatbuffers::String> responseString = 0;
        if (!responseStringValue.empty()) {
            responseString = responseBuilder.CreateString(responseStringValue);
        }
        const auto response = MobileGL::Protocol::Wire::CreateResponse(responseBuilder, status,
                                                                       command->token(), sessionId,
                                                                       dataByte, syncHandle,
                                                                       responseString,
                                                                       responseShmCount, queryNs);
        responseBuilder.Finish(response);

        MobileGLCommandBatch out{};
        out.structSize = sizeof(MobileGLCommandBatch);
        out.flatBufferData = responseBuilder.GetBufferPointer();
        out.flatBufferSize = static_cast<Uint32>(responseBuilder.GetSize());
        out.shmHandleCount = responseShmCount;
        out.shmHandles = responseShmCount > 0 ? responseShm.data() : nullptr;
        const Bool submitted = m_ops->SubmitCommands(m_transport, &out);
        for (auto& handle : responseShm) {
            m_ops->ReleaseSharedMemory(m_transport, &handle);
        }
        return submitted;
    }

    void ServerCore::SetSessionListener(SessionListener listener, void* user) {
        m_sessionListener = listener;
        m_sessionUser = user;
    }

    void ServerCore::SetWireDispatch(WireDispatchFn fn) {
        m_wireDispatch = fn;
    }

    void ServerCore::NotifySessionChanged(MobileGLSessionId sessionId) {
        if (m_sessionListener != nullptr) {
            m_sessionListener(sessionId, m_sessionUser);
        }
    }

    void ServerCore::Shutdown() {
        if (m_vtable != nullptr && m_vtable->Shutdown != nullptr && m_backend != nullptr) {
            m_vtable->Shutdown(m_backend);
        }
        m_running = false;
    }
} // namespace MobileGL::FullServer

// End of File
