// MobileGL - MobileGL/MG_Impl/GLImpl/Sync/GL_Sync.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "GL_Sync.h"
#include <MG_Backend/BackendObjects.h>
#include <MG_State/GLState/Core.h>

namespace MobileGL::MG_Impl::GLImpl {
    namespace {
        using SyncObject = MobileGL::MG_State::GLState::SessionPrivateState::SyncObject;

        MobileGL::MG_State::GLState::SessionPrivateState& SessionSyncState() {
            return MG_State::pGLContext->GetSessionState();
        }

        SyncObject* FindSyncObject(GLsync sync) {
            return SessionSyncState().FindSync(sync);
        }
    } // namespace

    GLsync FenceSync(GLenum condition, GLbitfield flags) {
        // GL 4.6 core 4.1.2: GL_SYNC_GPU_COMMANDS_COMPLETE is the only condition and the only
        // legal flags value is zero; both violations return 0 rather than a handle. A caller that
        // then hands the 0 back to glDeleteSync hits the glDeleteSync(0) no-op below.
        if (condition != GL_SYNC_GPU_COMMANDS_COMPLETE) {
            MG_State::pGLContext->RecordError(
                ErrorCode::InvalidEnum,
                MakeUnique<GenericErrorInfo>("MG_Impl/GLImpl", __func__,
                                             "condition must be GL_SYNC_GPU_COMMANDS_COMPLETE."));
            return nullptr;
        }
        if (flags != 0) {
            MG_State::pGLContext->RecordError(
                ErrorCode::InvalidValue,
                MakeUnique<GenericErrorInfo>("MG_Impl/GLImpl", __func__, "flags must be zero."));
            return nullptr;
        }
        SyncObject syncObject;
        syncObject.Condition = condition;
        syncObject.Flags = flags;
        if (const auto backendFenceSync = MG_Backend::gBackendFunctionsTable.GL.FenceSync) {
            syncObject.BackendHandle = backendFenceSync();
        }
        auto& state = SessionSyncState();
        void* rawHandle = state.AllocateSyncHandle();
        const GLsync handle = reinterpret_cast<GLsync>(rawHandle);
        state.InsertSync(handle, Move(syncObject));
        return handle;
    }

    GLboolean IsSync(GLsync sync) {
        return FindSyncObject(sync) != nullptr ? GL_TRUE : GL_FALSE;
    }

    GLenum ClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout) {
        // GL 4.6 core 4.1.1: GL_SYNC_FLUSH_COMMANDS_BIT is the only bit this call accepts, and
        // any other bit is INVALID_VALUE. Silently ignoring the stray bits used to make a caller
        // that passed, say, GL_SYNC_GPU_COMMANDS_COMPLETE by mistake think it had asked for a
        // flush it never got.
        if ((flags & ~static_cast<GLbitfield>(GL_SYNC_FLUSH_COMMANDS_BIT)) != 0) {
            MG_State::pGLContext->RecordError(
                ErrorCode::InvalidValue,
                MakeUnique<GenericErrorInfo>("MG_Impl/GLImpl", __func__,
                                             "flags must be zero or GL_SYNC_FLUSH_COMMANDS_BIT."));
            return GL_WAIT_FAILED;
        }
        const auto* syncObject = FindSyncObject(sync);
        if (!syncObject) {
            // The spec pairs the GL_WAIT_FAILED return with a recorded INVALID_VALUE; returning
            // the enum alone left glGetError() clean and the failure indistinguishable from a
            // genuine wait failure on a live sync.
            MG_State::pGLContext->RecordError(
                ErrorCode::InvalidValue,
                MakeUnique<GenericErrorInfo>("MG_Impl/GLImpl", __func__, "sync is not the name of a sync object."));
            return GL_WAIT_FAILED;
        }
        const auto backendClientWaitSync = MG_Backend::gBackendFunctionsTable.GL.ClientWaitSync;
        if (!backendClientWaitSync || !syncObject->BackendHandle) {
            return GL_ALREADY_SIGNALED; // legacy always-signaled fallback
        }
        return backendClientWaitSync(syncObject->BackendHandle, flags, timeout);
    }

    void WaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout) {
        // GL 4.6 core 4.1.2: the server-side wait takes no flags and no finite timeout - both
        // arguments exist only to be forward-compatible, and anything else is INVALID_VALUE.
        // Neither backend ever honored a nonzero timeout (DirectGLES hard-codes
        // 0/GL_TIMEOUT_IGNORED, DirectVulkan's queue ordering makes the wait implicit), so
        // rejecting the call loses no wait that used to happen.
        if (flags != 0 || timeout != GL_TIMEOUT_IGNORED) {
            MG_State::pGLContext->RecordError(
                ErrorCode::InvalidValue,
                MakeUnique<GenericErrorInfo>("MG_Impl/GLImpl", __func__,
                                             "flags must be zero and timeout must be GL_TIMEOUT_IGNORED."));
            return;
        }
        const auto* syncObject = FindSyncObject(sync);
        if (!syncObject) {
            MG_State::pGLContext->RecordError(
                ErrorCode::InvalidValue,
                MakeUnique<GenericErrorInfo>("MG_Impl/GLImpl", __func__, "sync is not the name of a sync object."));
            return;
        }
        const auto backendWaitSync = MG_Backend::gBackendFunctionsTable.GL.WaitSync;
        if (backendWaitSync && syncObject->BackendHandle) {
            backendWaitSync(syncObject->BackendHandle, flags, timeout);
        }
    }

    void DeleteSync(GLsync sync) {
        if (sync == nullptr) {
            return; // glDeleteSync(0) is silently ignored
        }
        auto& state = SessionSyncState();
        auto* syncObject = state.FindSync(sync);
        if (syncObject == nullptr) {
            return;
        }
        const auto backendDeleteSync = MG_Backend::gBackendFunctionsTable.GL.DeleteSync;
        if (backendDeleteSync && syncObject->BackendHandle) {
            backendDeleteSync(syncObject->BackendHandle);
        }
        state.EraseSync(sync);
    }

    void GetSynciv(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei* length, GLint* values) {
        // GL 4.6 core 4.1: a negative bufSize is INVALID_VALUE, an unnamed sync is INVALID_VALUE
        // and an unrecognised pname is INVALID_ENUM. All three used to leave glGetError() clean
        // and write a plausible-looking zero, which is the one failure mode a caller cannot tell
        // apart from a real answer - GL_SYNC_STATUS legitimately answers GL_UNSIGNALED (0x9118),
        // but a mistyped pname answered a bare 0 that no query ever returns.
        if (bufSize < 0) {
            MG_State::pGLContext->RecordError(
                ErrorCode::InvalidValue,
                MakeUnique<GenericErrorInfo>("MG_Impl/GLImpl", __func__, "bufSize must not be negative."));
            return;
        }
        const auto* syncObject = FindSyncObject(sync);
        if (!syncObject) {
            MG_State::pGLContext->RecordError(
                ErrorCode::InvalidValue,
                MakeUnique<GenericErrorInfo>("MG_Impl/GLImpl", __func__, "sync is not the name of a sync object."));
            if (length) {
                *length = 0;
            }
            return;
        }

        GLint value = 0;
        switch (pname) {
        case GL_OBJECT_TYPE:
            value = GL_SYNC_FENCE;
            break;
        case GL_SYNC_STATUS: {
            const auto backendGetSyncStatus = MG_Backend::gBackendFunctionsTable.GL.GetSyncStatus;
            const Bool signaled = !backendGetSyncStatus || !syncObject->BackendHandle ||
                                  backendGetSyncStatus(syncObject->BackendHandle);
            value = signaled ? GL_SIGNALED : GL_UNSIGNALED;
            break;
        }
        case GL_SYNC_CONDITION:
            value = static_cast<GLint>(syncObject->Condition);
            break;
        case GL_SYNC_FLAGS:
            value = static_cast<GLint>(syncObject->Flags);
            break;
        default:
            MG_State::pGLContext->RecordError(
                ErrorCode::InvalidEnum,
                MakeUnique<GenericErrorInfo>("MG_Impl/GLImpl", __func__,
                                             "pname must be GL_OBJECT_TYPE, GL_SYNC_STATUS, GL_SYNC_CONDITION or "
                                             "GL_SYNC_FLAGS."));
            if (length) {
                *length = 0;
            }
            return;
        }

        if (length) {
            *length = bufSize > 0 && values ? 1 : 0;
        }
        if (bufSize > 0 && values) {
            values[0] = value;
        }
    }

    void DestroyAllSyncObjects() {
        // Sweep the current session's session-private registry. Entries the app
        // already deleted were erased by DeleteSync, so nothing here double-frees.
        // Both backends' DeleteSync only free the heap wrapper once their GL
        // context/renderer is gone (generation/current-thread guards), so this is
        // safe after the backend has released its EGL resources.
        auto& state = SessionSyncState();
        const auto handles = state.GetSyncHandles();
        if (handles.empty()) {
            return;
        }
        const auto backendDeleteSync = MG_Backend::gBackendFunctionsTable.GL.DeleteSync;
        for (void* handle : handles) {
            auto* syncObject = state.FindSync(handle);
            if (syncObject != nullptr) {
                if (backendDeleteSync && syncObject->BackendHandle) {
                    backendDeleteSync(syncObject->BackendHandle);
                }
                state.EraseSync(handle);
            }
        }
        MGLOG_D("DestroyAllSyncObjects: reclaimed %zu sync object(s) the app left undeleted",
                handles.size());
    }
} // namespace MobileGL::MG_Impl::GLImpl
