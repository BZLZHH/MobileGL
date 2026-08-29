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
                                                                       command->token(), dataByte,
                                                                       syncHandle, responseString,
                                                                       responseShmCount);
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

    void ServerCore::Shutdown() {
        if (m_vtable != nullptr && m_vtable->Shutdown != nullptr && m_backend != nullptr) {
            m_vtable->Shutdown(m_backend);
        }
        m_running = false;
    }
} // namespace MobileGL::FullServer

// End of File
