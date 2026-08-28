// MobileGL - MobileGL/FullServer/BackendHost.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "BackendHost.h"
#include "MG_State/GLState/ContextRegistry.h"

namespace MobileGL::FullServer {
    namespace {
        Bool Host_GetSessionDrawState(MobileGLSessionId session, MobileGLSessionDrawState* out) {
            if (out == nullptr) return false;
            // TODO(Phase 3): snapshot from GLContextSession::GetContext().
            (void)session;
            *out = MobileGLSessionDrawState{};
            out->structSize = sizeof(MobileGLSessionDrawState);
            return false;
        }

        Bool Host_GetRenderState(MobileGLSessionId session, Uint64 desiredVersion, MobileGLRenderState* out) {
            if (out == nullptr) return false;
            (void)session;
            (void)desiredVersion;
            *out = MobileGLRenderState{};
            out->structSize = sizeof(MobileGLRenderState);
            return false;
        }

        Bool Host_GetPixelStore(MobileGLSessionId session, int isUnpack, MobileGLPixelStore* out) {
            if (out == nullptr) return false;
            (void)session;
            (void)isUnpack;
            *out = MobileGLPixelStore{};
            out->structSize = sizeof(MobileGLPixelStore);
            return false;
        }

        Bool Host_GetBufferInfo(MobileGLBackendHandle h, MobileGLBufferInfo* out) {
            if (out == nullptr) return false;
            (void)h;
            *out = MobileGLBufferInfo{};
            out->structSize = sizeof(MobileGLBufferInfo);
            return false;
        }

        Bool Host_GetTextureInfo(MobileGLBackendHandle h, MobileGLTextureInfo* out) {
            if (out == nullptr) return false;
            (void)h;
            *out = MobileGLTextureInfo{};
            out->structSize = sizeof(MobileGLTextureInfo);
            return false;
        }

        Bool Host_GetTextureMipInfo(MobileGLBackendHandle h, uint32_t level, MobileGLTextureMipInfo* out) {
            if (out == nullptr) return false;
            (void)h;
            (void)level;
            *out = MobileGLTextureMipInfo{};
            out->structSize = sizeof(MobileGLTextureMipInfo);
            return false;
        }

        Bool Host_GetFramebufferInfo(MobileGLBackendHandle h, MobileGLFramebufferInfo* out) {
            if (out == nullptr) return false;
            (void)h;
            *out = MobileGLFramebufferInfo{};
            out->structSize = sizeof(MobileGLFramebufferInfo);
            return false;
        }

        Bool Host_GetVertexArrayInfo(MobileGLBackendHandle h, MobileGLVertexArrayInfo* out) {
            if (out == nullptr) return false;
            (void)h;
            *out = MobileGLVertexArrayInfo{};
            out->structSize = sizeof(MobileGLVertexArrayInfo);
            return false;
        }

        Bool Host_GetSamplerInfo(MobileGLBackendHandle h, MobileGLSamplerInfo* out) {
            if (out == nullptr) return false;
            (void)h;
            *out = MobileGLSamplerInfo{};
            out->structSize = sizeof(MobileGLSamplerInfo);
            return false;
        }

        Bool Host_GetProgramInfo(MobileGLBackendHandle h, MobileGLProgramInfo* out) {
            if (out == nullptr) return false;
            (void)h;
            *out = MobileGLProgramInfo{};
            out->structSize = sizeof(MobileGLProgramInfo);
            return false;
        }

        Bool Host_GetProgramModules(MobileGLBackendHandle h, MobileGLProgramStageModule* out, uint32_t* inOutCount) {
            if (out == nullptr || inOutCount == nullptr) return false;
            (void)h;
            *inOutCount = 0;
            return false;
        }

        Bool Host_GetRenderbufferInfo(MobileGLBackendHandle h, MobileGLRenderbufferInfo* out) {
            if (out == nullptr) return false;
            (void)h;
            *out = MobileGLRenderbufferInfo{};
            out->structSize = sizeof(MobileGLRenderbufferInfo);
            return false;
        }

        Bool Host_GetBufferShadow(MobileGLSessionId session, MobileGLBackendHandle h,
                                  const void** data, Uint64* size) {
            (void)session;
            (void)h;
            if (data != nullptr) *data = nullptr;
            if (size != nullptr) *size = 0;
            return false;
        }

        Bool Host_ReadBufferRange(MobileGLSessionId session, MobileGLBackendHandle h,
                                  Uint64 offset, Uint64 size, void* dst) {
            (void)session;
            (void)h;
            (void)offset;
            (void)size;
            (void)dst;
            return false;
        }

        Bool Host_GetTextureMipData(MobileGLBackendHandle h, uint32_t level, uint32_t layer,
                                    const void** data, Uint64* size) {
            (void)h;
            (void)level;
            (void)layer;
            if (data != nullptr) *data = nullptr;
            if (size != nullptr) *size = 0;
            return false;
        }

        void Host_RecordError(MobileGLSessionId session, uint32_t glErrorCode, const char* message) {
            (void)session;
            (void)glErrorCode;
            (void)message;
            // TODO(Phase 3): route into the owning session's ErrorState.
        }

        void Host_Log(int level, const char* message) {
            (void)level;
            (void)message;
        }

        void Host_CapabilityChanged(MobileGLBackend* backend) {
            (void)backend;
        }

        const MobileGLBackendHost s_backendHost = {
            sizeof(MobileGLBackendHost),
            (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR,
            &Host_GetSessionDrawState,
            &Host_GetRenderState,
            &Host_GetPixelStore,
            &Host_GetBufferInfo,
            &Host_GetTextureInfo,
            &Host_GetTextureMipInfo,
            &Host_GetFramebufferInfo,
            &Host_GetVertexArrayInfo,
            &Host_GetSamplerInfo,
            &Host_GetProgramInfo,
            &Host_GetProgramModules,
            &Host_GetRenderbufferInfo,
            &Host_GetBufferShadow,
            &Host_ReadBufferRange,
            &Host_GetTextureMipData,
            &Host_RecordError,
            &Host_Log,
            &Host_CapabilityChanged
        };
    } // namespace

    const MobileGLBackendHost& GetBackendHost() {
        return s_backendHost;
    }
} // namespace MobileGL::FullServer

// End of File
