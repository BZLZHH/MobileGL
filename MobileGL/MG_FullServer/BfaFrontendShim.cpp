// MobileGL - MobileGL/MG_FullServer/BfaFrontendShim.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "BfaFrontendShim.h"
#include "MG_Backend/BackendObjects.h"
#include "MG_State/GLState/FramebufferState/FramebufferObject.h"

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
            auto& state = BfaFrontendShim::Get().m_state;
            if (state.Session != 0) {
                return state.Session;
            }
            return state.SessionProvider != nullptr ? state.SessionProvider() : 0;
        }

        MobileGLBackendHandle Handle(uint32_t objectKind, uint64_t glName) {
            auto& state = BfaFrontendShim::Get().m_state;
            return state.HandleProvider != nullptr ? state.HandleProvider(objectKind, glName)
                                                   : glName;
        }

        Uint FramebufferName(const SharedPtr<MG_State::GLState::FramebufferObject>& framebuffer) {
            auto& state = BfaFrontendShim::Get().m_state;
            return state.FramebufferNameProvider != nullptr ? state.FramebufferNameProvider(framebuffer)
                                                            : 0;
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

        void ThunkDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type,
                                         const void* indices, GLint basevertex) {
            if (VTable() != nullptr && VTable()->DrawElementsBaseVertex != nullptr) {
                VTable()->DrawElementsBaseVertex(Backend(), Session(), mode, count, type, indices,
                                                 basevertex);
            }
        }

        void ThunkDrawArraysIndirect(GLenum mode, const void* indirect) {
            if (VTable() != nullptr && VTable()->DrawArraysIndirect != nullptr) {
                VTable()->DrawArraysIndirect(Backend(), Session(), mode, indirect);
            }
        }

        void ThunkDrawElementsIndirect(GLenum mode, GLenum type, const void* indirect) {
            if (VTable() != nullptr && VTable()->DrawElementsIndirect != nullptr) {
                VTable()->DrawElementsIndirect(Backend(), Session(), mode, type, indirect);
            }
        }

        void ThunkMultiDrawArrays(GLenum mode, const GLint* first, const GLsizei* count,
                                  GLsizei drawcount) {
            if (VTable() != nullptr && VTable()->MultiDrawArrays != nullptr) {
                VTable()->MultiDrawArrays(Backend(), Session(), mode, first, count, drawcount);
            }
        }

        void ThunkMultiDrawElements(GLenum mode, const GLsizei* count, GLenum type,
                                    const GLvoid* const* indices, GLsizei drawcount) {
            if (VTable() != nullptr && VTable()->MultiDrawElements != nullptr) {
                VTable()->MultiDrawElements(Backend(), Session(), mode, count, type, indices,
                                            drawcount);
            }
        }

        void ThunkMultiDrawArraysIndirect(GLenum mode, const void* indirect, GLsizei drawcount,
                                          GLsizei stride) {
            if (VTable() != nullptr && VTable()->MultiDrawArraysIndirect != nullptr) {
                VTable()->MultiDrawArraysIndirect(Backend(), Session(), mode, indirect, drawcount,
                                                  stride);
            }
        }

        void ThunkMultiDrawElementsIndirect(GLenum mode, GLenum type, const void* indirect,
                                            GLsizei drawcount, GLsizei stride) {
            if (VTable() != nullptr && VTable()->MultiDrawElementsIndirect != nullptr) {
                VTable()->MultiDrawElementsIndirect(Backend(), Session(), mode, type, indirect,
                                                    drawcount, stride);
            }
        }

        void ThunkClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat* value) {
            if (VTable() != nullptr && VTable()->ClearBufferfv != nullptr) {
                VTable()->ClearBufferfv(Backend(), Session(), buffer, drawbuffer, value);
            }
        }

        void ThunkClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint* value) {
            if (VTable() != nullptr && VTable()->ClearBufferiv != nullptr) {
                VTable()->ClearBufferiv(Backend(), Session(), buffer, drawbuffer, value);
            }
        }

        void ThunkClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint* value) {
            if (VTable() != nullptr && VTable()->ClearBufferuiv != nullptr) {
                VTable()->ClearBufferuiv(Backend(), Session(), buffer, drawbuffer, value);
            }
        }

        void ThunkClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil) {
            if (VTable() != nullptr && VTable()->ClearBufferfi != nullptr) {
                VTable()->ClearBufferfi(Backend(), Session(), buffer, drawbuffer, depth, stencil);
            }
        }

        void ThunkCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x,
                                 GLint y, GLsizei width, GLsizei height, GLint border) {
            if (VTable() != nullptr && VTable()->CopyTexImage2D != nullptr) {
                VTable()->CopyTexImage2D(Backend(), Session(), target, level, internalformat, x, y,
                                         width, height, border);
            }
        }

        void ThunkCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                    GLint x, GLint y, GLsizei width, GLsizei height) {
            if (VTable() != nullptr && VTable()->CopyTexSubImage2D != nullptr) {
                VTable()->CopyTexSubImage2D(Backend(), Session(), target, level, xoffset, yoffset,
                                            x, y, width, height);
            }
        }

        void ThunkBindImageTexture(GLuint unit, GLuint texture, GLint level, GLboolean layered,
                                   GLint layer, GLenum access, GLenum format) {
            if (VTable() != nullptr && VTable()->BindImageTexture != nullptr) {
                VTable()->BindImageTexture(Backend(), Session(), unit, texture, level, layered,
                                           layer, access, format);
            }
        }

        void ThunkShaderStorageBlockBinding(GLuint program, const GLchar* storageBlockName,
                                            GLuint storageBlockBinding) {
            if (VTable() != nullptr && VTable()->ShaderStorageBlockBinding != nullptr) {
                VTable()->ShaderStorageBlockBinding(Backend(), Session(), program, storageBlockName,
                                                    storageBlockBinding);
            }
        }

        GLenum ThunkClientWaitSync(MG_Backend::BackendSyncHandle sync, GLbitfield flags,
                                   GLuint64 timeout) {
            if (VTable() != nullptr && VTable()->ClientWaitSync != nullptr) {
                return static_cast<GLenum>(
                    VTable()->ClientWaitSync(Backend(), Session(), sync, flags, timeout));
            }
            return 0;
        }

        Bool ThunkGetSyncStatus(MG_Backend::BackendSyncHandle sync) {
            if (VTable() != nullptr && VTable()->GetSyncStatus != nullptr) {
                return VTable()->GetSyncStatus(Backend(), Session(), sync) ? 1 : 0;
            }
            return 1;
        }

        MG_Backend::BackendQueryHandle ThunkBeginTimeElapsedQuery() {
            if (VTable() != nullptr && VTable()->BeginTimeElapsedQuery != nullptr) {
                return static_cast<MG_Backend::BackendQueryHandle>(
                    VTable()->BeginTimeElapsedQuery(Backend(), Session()));
            }
            return nullptr;
        }

        void ThunkEndTimeElapsedQuery(MG_Backend::BackendQueryHandle query) {
            if (VTable() != nullptr && VTable()->EndTimeElapsedQuery != nullptr) {
                VTable()->EndTimeElapsedQuery(Backend(), Session(), query);
            }
        }

        MG_Backend::BackendQueryHandle ThunkQueryCounterTimestamp() {
            if (VTable() != nullptr && VTable()->QueryCounterTimestamp != nullptr) {
                return static_cast<MG_Backend::BackendQueryHandle>(
                    VTable()->QueryCounterTimestamp(Backend(), Session()));
            }
            return nullptr;
        }

        Bool ThunkIsQueryResultAvailable(MG_Backend::BackendQueryHandle query) {
            if (VTable() != nullptr && VTable()->IsQueryResultAvailable != nullptr) {
                return VTable()->IsQueryResultAvailable(Backend(), Session(), query) ? 1 : 0;
            }
            return 1;
        }

        Bool ThunkGetQueryResult64(MG_Backend::BackendQueryHandle query, Bool wait,
                                   Uint64* outNanoseconds) {
            if (VTable() != nullptr && VTable()->GetQueryResult64 != nullptr) {
                return VTable()->GetQueryResult64(Backend(), Session(), query, wait,
                                                  outNanoseconds)
                           ? 1
                           : 0;
            }
            return 0;
        }

        void ThunkDeleteBackendQuery(MG_Backend::BackendQueryHandle query) {
            if (VTable() != nullptr && VTable()->DeleteBackendQuery != nullptr) {
                VTable()->DeleteBackendQuery(Backend(), Session(), query);
            }
        }

        MG_Backend::BackendQueryHandle ThunkBeginOcclusionQuery() {
            if (VTable() != nullptr && VTable()->BeginOcclusionQuery != nullptr) {
                return static_cast<MG_Backend::BackendQueryHandle>(
                    VTable()->BeginOcclusionQuery(Backend(), Session()));
            }
            return nullptr;
        }

        void ThunkEndOcclusionQuery(MG_Backend::BackendQueryHandle query) {
            if (VTable() != nullptr && VTable()->EndOcclusionQuery != nullptr) {
                VTable()->EndOcclusionQuery(Backend(), Session(), query);
            }
        }

        MG_Backend::BackendQueryHandle ThunkBeginXfbPrimitivesQuery(Bool generated) {
            if (VTable() != nullptr && VTable()->BeginXfbPrimitivesQuery != nullptr) {
                return static_cast<MG_Backend::BackendQueryHandle>(
                    VTable()->BeginXfbPrimitivesQuery(Backend(), Session(), generated != 0));
            }
            return nullptr;
        }

        void ThunkEndXfbPrimitivesQuery(MG_Backend::BackendQueryHandle query) {
            if (VTable() != nullptr && VTable()->EndXfbPrimitivesQuery != nullptr) {
                VTable()->EndXfbPrimitivesQuery(Backend(), Session(), query);
            }
        }

        void ThunkPatchParameteri(GLenum pname, GLint value) {
            if (VTable() != nullptr && VTable()->PatchParameteri != nullptr) {
                VTable()->PatchParameteri(Backend(), Session(), pname, value);
            }
        }

        void ThunkBeginTransformFeedback(GLenum primitiveMode) {
            if (VTable() != nullptr && VTable()->BeginTransformFeedback != nullptr) {
                VTable()->BeginTransformFeedback(Backend(), Session(), primitiveMode);
            }
        }

        void ThunkEndTransformFeedback() {
            if (VTable() != nullptr && VTable()->EndTransformFeedback != nullptr) {
                VTable()->EndTransformFeedback(Backend(), Session());
            }
        }

        void ThunkPauseTransformFeedback() {
            if (VTable() != nullptr && VTable()->PauseTransformFeedback != nullptr) {
                VTable()->PauseTransformFeedback(Backend(), Session());
            }
        }

        void ThunkResumeTransformFeedback() {
            if (VTable() != nullptr && VTable()->ResumeTransformFeedback != nullptr) {
                VTable()->ResumeTransformFeedback(Backend(), Session());
            }
        }

        void ThunkBindTransformFeedback(GLuint name) {
            if (VTable() != nullptr && VTable()->BindTransformFeedback != nullptr) {
                VTable()->BindTransformFeedback(Backend(), Session(), name);
            }
        }

        void ThunkDeleteTransformFeedback(GLuint name) {
            if (VTable() != nullptr && VTable()->DeleteTransformFeedback != nullptr) {
                VTable()->DeleteTransformFeedback(Backend(), Session(), name);
            }
        }

        Int64 ThunkGetGpuTimestampNs() {
            if (VTable() != nullptr && VTable()->GetGpuTimestampNs != nullptr) {
                return VTable()->GetGpuTimestampNs(Backend(), Session());
            }
            return 0;
        }

        void ThunkSetSwapInterval(Int interval) {
            if (VTable() != nullptr && VTable()->SetSwapInterval != nullptr) {
                VTable()->SetSwapInterval(Backend(), Session(), interval);
            }
        }

        void ThunkClearNamedFramebufferfv(
            const SharedPtr<MG_State::GLState::FramebufferObject>& framebuffer,
            GLenum buffer, GLint drawbuffer, const GLfloat* value) {
            if (VTable() != nullptr && VTable()->ClearNamedFramebufferfv != nullptr && framebuffer != nullptr) {
                VTable()->ClearNamedFramebufferfv(
                    Backend(), Session(),
                    Handle(MobileGLObjectKindFramebuffer, FramebufferName(framebuffer)),
                    buffer, drawbuffer, value);
            }
        }

        void ThunkClearNamedFramebufferfi(
            const SharedPtr<MG_State::GLState::FramebufferObject>& framebuffer,
            GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil) {
            if (VTable() != nullptr && VTable()->ClearNamedFramebufferfi != nullptr && framebuffer != nullptr) {
                VTable()->ClearNamedFramebufferfi(
                    Backend(), Session(),
                    Handle(MobileGLObjectKindFramebuffer, FramebufferName(framebuffer)),
                    buffer, drawbuffer, depth, stencil);
            }
        }

        void ThunkClearNamedFramebufferiv(
            const SharedPtr<MG_State::GLState::FramebufferObject>& framebuffer,
            GLenum buffer, GLint drawbuffer, const GLint* value) {
            if (VTable() != nullptr && VTable()->ClearNamedFramebufferiv != nullptr && framebuffer != nullptr) {
                VTable()->ClearNamedFramebufferiv(
                    Backend(), Session(),
                    Handle(MobileGLObjectKindFramebuffer, FramebufferName(framebuffer)),
                    buffer, drawbuffer, value);
            }
        }

        void ThunkClearNamedFramebufferuiv(
            const SharedPtr<MG_State::GLState::FramebufferObject>& framebuffer,
            GLenum buffer, GLint drawbuffer, const GLuint* value) {
            if (VTable() != nullptr && VTable()->ClearNamedFramebufferuiv != nullptr && framebuffer != nullptr) {
                VTable()->ClearNamedFramebufferuiv(
                    Backend(), Session(),
                    Handle(MobileGLObjectKindFramebuffer, FramebufferName(framebuffer)),
                    buffer, drawbuffer, value);
            }
        }

        void ThunkBlitNamedFramebuffer(
            const SharedPtr<MG_State::GLState::FramebufferObject>& readFramebuffer,
            const SharedPtr<MG_State::GLState::FramebufferObject>& drawFramebuffer,
            GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
            GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
            GLbitfield mask, GLenum filter) {
            if (VTable() != nullptr && VTable()->BlitFramebuffer != nullptr) {
                VTable()->BlitFramebuffer(
                    Backend(), Session(),
                    readFramebuffer != nullptr
                        ? Handle(MobileGLObjectKindFramebuffer, FramebufferName(readFramebuffer))
                        : 0,
                    drawFramebuffer != nullptr
                        ? Handle(MobileGLObjectKindFramebuffer, FramebufferName(drawFramebuffer))
                        : 0,
                    srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
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
        m_state.Table->GL.DrawElementsBaseVertex = &ThunkDrawElementsBaseVertex;
        m_state.Table->GL.DrawArraysIndirect = &ThunkDrawArraysIndirect;
        m_state.Table->GL.DrawElementsIndirect = &ThunkDrawElementsIndirect;
        m_state.Table->GL.MultiDrawArrays = &ThunkMultiDrawArrays;
        m_state.Table->GL.MultiDrawElements = &ThunkMultiDrawElements;
        m_state.Table->GL.MultiDrawArraysIndirect = &ThunkMultiDrawArraysIndirect;
        m_state.Table->GL.MultiDrawElementsIndirect = &ThunkMultiDrawElementsIndirect;
        m_state.Table->GL.DrawArraysInstanced = &ThunkDrawArraysInstanced;
        m_state.Table->GL.DrawElementsInstanced = &ThunkDrawElementsInstanced;
        m_state.Table->GL.DrawRangeElements = &ThunkDrawRangeElements;
        m_state.Table->GL.ClearBufferfv = &ThunkClearBufferfv;
        m_state.Table->GL.ClearBufferiv = &ThunkClearBufferiv;
        m_state.Table->GL.ClearBufferuiv = &ThunkClearBufferuiv;
        m_state.Table->GL.ClearBufferfi = &ThunkClearBufferfi;
        m_state.Table->GL.ClearNamedFramebufferfv = &ThunkClearNamedFramebufferfv;
        m_state.Table->GL.ClearNamedFramebufferiv = &ThunkClearNamedFramebufferiv;
        m_state.Table->GL.ClearNamedFramebufferuiv = &ThunkClearNamedFramebufferuiv;
        m_state.Table->GL.ClearNamedFramebufferfi = &ThunkClearNamedFramebufferfi;
        m_state.Table->GL.BlitNamedFramebuffer = &ThunkBlitNamedFramebuffer;
        m_state.Table->GL.BlitFramebuffer = &ThunkBlitFramebuffer;
        m_state.Table->GL.CopyTexImage2D = &ThunkCopyTexImage2D;
        m_state.Table->GL.CopyTexSubImage2D = &ThunkCopyTexSubImage2D;
        m_state.Table->GL.GenerateMipmap = &ThunkGenerateMipmap;
        m_state.Table->GL.DispatchCompute = &ThunkDispatchCompute;
        m_state.Table->GL.DispatchComputeIndirect = &ThunkDispatchComputeIndirect;
        m_state.Table->GL.MemoryBarrier = &ThunkMemoryBarrier;
        m_state.Table->GL.MemoryBarrierByRegion = &ThunkMemoryBarrierByRegion;
        m_state.Table->GL.BindImageTexture = &ThunkBindImageTexture;
        m_state.Table->GL.PatchParameteri = &ThunkPatchParameteri;
        m_state.Table->GL.BeginTransformFeedback = &ThunkBeginTransformFeedback;
        m_state.Table->GL.EndTransformFeedback = &ThunkEndTransformFeedback;
        m_state.Table->GL.PauseTransformFeedback = &ThunkPauseTransformFeedback;
        m_state.Table->GL.ResumeTransformFeedback = &ThunkResumeTransformFeedback;
        m_state.Table->GL.BindTransformFeedback = &ThunkBindTransformFeedback;
        m_state.Table->GL.DeleteTransformFeedback = &ThunkDeleteTransformFeedback;
        m_state.Table->GL.ShaderStorageBlockBinding = &ThunkShaderStorageBlockBinding;
        m_state.Table->GL.FenceSync = &ThunkFenceSync;
        m_state.Table->GL.ClientWaitSync = &ThunkClientWaitSync;
        m_state.Table->GL.DeleteSync = &ThunkDeleteSync;
        m_state.Table->GL.WaitSync = &ThunkWaitSync;
        m_state.Table->GL.GetSyncStatus = &ThunkGetSyncStatus;
        m_state.Table->GL.BeginTimeElapsedQuery = &ThunkBeginTimeElapsedQuery;
        m_state.Table->GL.EndTimeElapsedQuery = &ThunkEndTimeElapsedQuery;
        m_state.Table->GL.QueryCounterTimestamp = &ThunkQueryCounterTimestamp;
        m_state.Table->GL.IsQueryResultAvailable = &ThunkIsQueryResultAvailable;
        m_state.Table->GL.GetQueryResult64 = &ThunkGetQueryResult64;
        m_state.Table->GL.DeleteBackendQuery = &ThunkDeleteBackendQuery;
        m_state.Table->GL.BeginOcclusionQuery = &ThunkBeginOcclusionQuery;
        m_state.Table->GL.EndOcclusionQuery = &ThunkEndOcclusionQuery;
        m_state.Table->GL.BeginXfbPrimitivesQuery = &ThunkBeginXfbPrimitivesQuery;
        m_state.Table->GL.EndXfbPrimitivesQuery = &ThunkEndXfbPrimitivesQuery;
        m_state.Table->GL.GetGpuTimestampNs = &ThunkGetGpuTimestampNs;
        m_state.Table->Present = &ThunkPresent;
        m_state.Table->SetSwapInterval = &ThunkSetSwapInterval;
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
