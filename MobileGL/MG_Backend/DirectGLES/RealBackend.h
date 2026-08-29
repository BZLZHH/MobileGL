// MobileGL - MobileGL/MG_Backend/DirectGLES/RealBackend.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#pragma once

#include <Includes.h>
#include "MG_Protocol/bfa.h"
#include "MG_Protocol/mgruntime_api.h"
#include "MG_Util/BackendLoaders/OpenGL/Loader.h"

// bfa.h declares MobileGLBackend as an opaque C type; this header is the one
// place the DirectGLES plugin completes it. All fields are plugin-private:
// FullServer only ever sees the opaque pointer.
struct MobileGLBackend {
    const MobileGLBackendVTable* VTable = nullptr;
    const MobileGLBackendHost* Host = nullptr;
    const MobileGLUtilApi* UtilApi = nullptr;

    // Dynamically loaded EGL/GLES libraries and their resolved entry points.
    void* EglLibrary = nullptr;
    void* GlesLibrary = nullptr;
    MobileGL::MG_External::EGLFunctionsTable Egl;
    MobileGL::MG_External::GLESFunctionsTable Gl;

    MobileGL::Bool Initialized = false;
    MobileGL::Bool DisplayInitialized = false;
    MobileGL::Bool CapabilitiesInitialized = false;

    // One display/config/context for every session created through the BFA.
    EGLDisplay Display = EGL_NO_DISPLAY;
    EGLConfig Config = nullptr;
    EGLSurface Surface = EGL_NO_SURFACE;
    EGLContext Context = EGL_NO_CONTEXT;
    MobileGLDisplayId CurrentDisplayId = 0;
    MobileGLSharedGroupId CurrentSharedGroupId = 0;
    MobileGLSessionId CurrentSessionId = 0;

    std::recursive_mutex Mutex;

    struct NativeSession {
        EGLSurface Surface = EGL_NO_SURFACE;
        EGLContext Context = EGL_NO_CONTEXT;
    };
    MobileGL::UnorderedMap<MobileGLSessionId, NativeSession> Sessions;

    // Handle -> native name maps. The BFA passes opaque handles; a real GL
    // driver addresses objects by its own name, so the plugin owns the mapping.
    MobileGL::UnorderedMap<MobileGLBackendHandle, GLuint> BufferNames;
    MobileGL::UnorderedMap<MobileGLBackendHandle, GLuint> TextureNames;
    MobileGL::UnorderedMap<MobileGLBackendHandle, GLsync> SyncNames;

    // Renderer / capability answers owned by the plugin.
    MobileGL::String RendererName;
    MobileGL::String VendorName;
    MobileGL::String VersionString;
    MobileGL::String ShaderLanguageString;
    MobileGL::Vector<MobileGL::String> ExtensionStrings;
    MobileGL::Vector<const char*> ExtensionPointers;
    MobileGLRendererInfo RendererInfo{};
    MobileGLDynamicParameters DynamicParameters{};
};

const MobileGLBackendVTable* RealBackendGetVTable();
MobileGLBackend* RealBackendCreate(const MobileGLBackendHost* host, const MobileGLUtilApi* utilApi,
                                   const MobileGLBackendInitInfo* initInfo);

// End of File
