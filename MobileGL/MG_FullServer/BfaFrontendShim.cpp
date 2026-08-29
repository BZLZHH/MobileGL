// MobileGL - MobileGL/MG_FullServer/BfaFrontendShim.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "BfaFrontendShim.h"
#include "MG_Backend/BackendObjects.h"

namespace MobileGL::FullServer {
    namespace {
        constexpr uint32_t kFenceGpuCommandsComplete = 0x9119;

        MobileGLBackend* Backend() {
            return BfaFrontendShim::Get().m_state.Backend;
        }
        const MobileGLBackendVTable* VTable() {
            return BfaFrontendShim::Get().m_state.VTable;
        }
        MobileGLSessionId Session() {
            return BfaFrontendShim::Get().m_state.Session;
        }

        void ThunkClear(GLbitfield mask) {
            if (VTable() != nullptr && VTable()->Clear != nullptr) {
                VTable()->Clear(Backend(), Session(), mask);
            }
        }

        void ThunkDrawArrays(GLenum mode, GLint first, GLsizei count) {
            if (VTable() != nullptr && VTable()->DrawArrays != nullptr) {
                VTable()->DrawArrays(Backend(), Session(), mode, first, count);
            }
        }

        void ThunkDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices) {
            if (VTable() != nullptr && VTable()->DrawElements != nullptr) {
                VTable()->DrawElements(Backend(), Session(), mode, count, type, indices);
            }
        }

        void ThunkDrawArraysInstanced(GLenum mode, GLint first, GLsizei count,
                                      GLsizei instancecount) {
            if (VTable() != nullptr && VTable()->DrawArraysInstanced != nullptr) {
                VTable()->DrawArraysInstanced(Backend(), Session(), mode, first, count, instancecount);
            }
        }

        void ThunkDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type,
                                        const void* indices, GLsizei instancecount) {
            if (VTable() != nullptr && VTable()->DrawElementsInstanced != nullptr) {
                VTable()->DrawElementsInstanced(Backend(), Session(), mode, count, type, indices,
                                                instancecount);
            }
        }

        void ThunkDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count,
                                    GLenum type, const void* indices) {
            if (VTable() != nullptr && VTable()->DrawRangeElements != nullptr) {
                VTable()->DrawRangeElements(Backend(), Session(), mode, start, end, count, type,
                                            indices);
            }
        }

        void ThunkBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                                  GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                                  GLbitfield mask, GLenum filter) {
            if (VTable() != nullptr && VTable()->BlitFramebuffer != nullptr) {
                VTable()->BlitFramebuffer(Backend(), Session(), 0, 0, srcX0, srcY0, srcX1, srcY1,
                                          dstX0, dstY0, dstX1, dstY1, mask, filter);
            }
        }

        void ThunkGenerateMipmap(GLenum target) {
            if (VTable() != nullptr && VTable()->GenerateMipmap != nullptr) {
                VTable()->GenerateMipmap(Backend(), Session(), target);
            }
        }

        void ThunkDispatchCompute(GLuint x, GLuint y, GLuint z) {
            if (VTable() != nullptr && VTable()->DispatchCompute != nullptr) {
                VTable()->DispatchCompute(Backend(), Session(), x, y, z);
            }
        }

        void ThunkDispatchComputeIndirect(GLintptr indirect) {
            if (VTable() != nullptr && VTable()->DispatchComputeIndirect != nullptr) {
                VTable()->DispatchComputeIndirect(Backend(), Session(), indirect);
            }
        }

        void ThunkMemoryBarrier(GLbitfield barriers) {
            if (VTable() != nullptr && VTable()->MemoryBarrier != nullptr) {
                VTable()->MemoryBarrier(Backend(), Session(), barriers);
            }
        }

        void ThunkMemoryBarrierByRegion(GLbitfield barriers) {
            if (VTable() != nullptr && VTable()->MemoryBarrierByRegion != nullptr) {
                VTable()->MemoryBarrierByRegion(Backend(), Session(), barriers);
            }
        }

        MG_Backend::BackendSyncHandle ThunkFenceSync() {
            if (VTable() != nullptr && VTable()->FenceSync != nullptr) {
                return static_cast<MG_Backend::BackendSyncHandle>(
                    VTable()->FenceSync(Backend(), Session(), kFenceGpuCommandsComplete, 0));
            }
            return nullptr;
        }

        void ThunkDeleteSync(MG_Backend::BackendSyncHandle sync) {
            if (VTable() != nullptr && VTable()->DeleteSync != nullptr) {
                VTable()->DeleteSync(Backend(), Session(), sync);
            }
        }

        void ThunkWaitSync(MG_Backend::BackendSyncHandle sync, GLbitfield flags, GLuint64 timeout) {
            if (VTable() != nullptr && VTable()->WaitSync != nullptr) {
                VTable()->WaitSync(Backend(), Session(), sync, flags, timeout);
            }
        }

        void ThunkPresent() {
            if (VTable() != nullptr && VTable()->SwapBuffers != nullptr) {
                VTable()->SwapBuffers(Backend(), Session(), 0);
            }
        }
    } // namespace

    BfaFrontendShim& BfaFrontendShim::Get() {
        static BfaFrontendShim s_shim;
        return s_shim;
    }

    void BfaFrontendShim::Install(MobileGLBackend* backend, const MobileGLBackendVTable* vtable,
                                  MG_Backend::GlobalBackendFunctionsTable* table) {
        m_state.Backend = backend;
        m_state.VTable = vtable;
        m_state.Table = table != nullptr ? table : &MG_Backend::gBackendFunctionsTable;
        m_state.Session = 0;
        if (m_state.Table == nullptr) {
            return;
        }
        m_state.Table->GL.Clear = &ThunkClear;
        m_state.Table->GL.DrawArrays = &ThunkDrawArrays;
        m_state.Table->GL.DrawElements = &ThunkDrawElements;
        m_state.Table->GL.DrawArraysInstanced = &ThunkDrawArraysInstanced;
        m_state.Table->GL.DrawElementsInstanced = &ThunkDrawElementsInstanced;
        m_state.Table->GL.DrawRangeElements = &ThunkDrawRangeElements;
        m_state.Table->GL.BlitFramebuffer = &ThunkBlitFramebuffer;
        m_state.Table->GL.GenerateMipmap = &ThunkGenerateMipmap;
        m_state.Table->GL.DispatchCompute = &ThunkDispatchCompute;
        m_state.Table->GL.DispatchComputeIndirect = &ThunkDispatchComputeIndirect;
        m_state.Table->GL.MemoryBarrier = &ThunkMemoryBarrier;
        m_state.Table->GL.MemoryBarrierByRegion = &ThunkMemoryBarrierByRegion;
        m_state.Table->GL.FenceSync = &ThunkFenceSync;
        m_state.Table->GL.DeleteSync = &ThunkDeleteSync;
        m_state.Table->GL.WaitSync = &ThunkWaitSync;
        m_state.Table->Present = &ThunkPresent;
    }

    void BfaFrontendShim::SetCurrentSession(MobileGLSessionId session) {
        m_state.Session = session;
    }

    MobileGLSessionId BfaFrontendShim::GetCurrentSession() const {
        return m_state.Session;
    }

    void BfaFrontendShim::Clear() {
        m_state.Backend = nullptr;
        m_state.VTable = nullptr;
        m_state.Table = nullptr;
        m_state.Session = 0;
    }
} // namespace MobileGL::FullServer

// End of File
