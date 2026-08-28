// MobileGL - MobileGL/FullServer/BackendPluginLoader.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#pragma once

#include <Includes.h>
#include "Protocol/bfa.h"

namespace MobileGL::FullServer {
    // One loaded BackendObject plugin. FullServer dlopens the module, reads
    // mobilegl_backend_manifest(), checks the ABI, and creates the backend.
    class BackendPluginLoader {
    public:
        BackendPluginLoader() = default;
        ~BackendPluginLoader();

        BackendPluginLoader(const BackendPluginLoader&) = delete;
        BackendPluginLoader& operator=(const BackendPluginLoader&) = delete;

        Bool Load(const char* pluginPath);
        void Unload();

        Bool IsLoaded() const { return m_library != nullptr; }
        const MobileGLBackendManifest* GetManifest() const { return m_manifest; }
        Bool ValidateAbi() const;

        MobileGLBackend* Create(const MobileGLBackendHost& host,
                                const MobileGLUtilApi& utilApi,
                                const MobileGLBackendInitInfo& initInfo) const;

    private:
        void* m_library = nullptr;
        const MobileGLBackendManifest* m_manifest = nullptr;
    };
} // namespace MobileGL::FullServer

// End of File
