// MobileGL - MobileGL/FullServer/BackendPluginLoader.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "BackendPluginLoader.h"

#if defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__)
#include <dlfcn.h>
#endif

namespace MobileGL::FullServer {
    BackendPluginLoader::~BackendPluginLoader() {
        Unload();
    }

    Bool BackendPluginLoader::Load(const char* pluginPath) {
        if (m_library != nullptr) {
            return false;
        }

#ifdef _WIN32
        m_library = reinterpret_cast<void*>(LoadLibraryA(pluginPath));
        if (m_library == nullptr) {
            return false;
        }
        auto manifestFn = reinterpret_cast<const MobileGLBackendManifest* (*)(void)>(
            GetProcAddress(reinterpret_cast<HMODULE>(m_library), "mobilegl_backend_manifest"));
#else
        m_library = dlopen(pluginPath, RTLD_NOW | RTLD_LOCAL);
        if (m_library == nullptr) {
            return false;
        }
        auto manifestFn = reinterpret_cast<const MobileGLBackendManifest* (*)(void)>(
            dlsym(m_library, "mobilegl_backend_manifest"));
#endif

        if (manifestFn == nullptr) {
            Unload();
            return false;
        }

        m_manifest = manifestFn();
        if (m_manifest == nullptr || m_manifest->structSize < sizeof(MobileGLBackendManifest) ||
            m_manifest->abiMajor != MOBILEGL_BFA_ABI_MAJOR) {
            Unload();
            return false;
        }
        return true;
    }

    void BackendPluginLoader::Unload() {
        if (m_library != nullptr) {
#ifdef _WIN32
            FreeLibrary(reinterpret_cast<HMODULE>(m_library));
#else
            dlclose(m_library);
#endif
        }
        m_library = nullptr;
        m_manifest = nullptr;
    }

    Bool BackendPluginLoader::ValidateAbi() const {
        if (m_manifest == nullptr) {
            return false;
        }
        return m_manifest->abiMajor == MOBILEGL_BFA_ABI_MAJOR &&
               m_manifest->abiMinor >= MOBILEGL_BFA_ABI_MINOR &&
               m_manifest->GetAbiVersion != nullptr;
    }

    MobileGLBackend* BackendPluginLoader::Create(const MobileGLBackendHost& host,
                                                 const MobileGLUtilApi& utilApi,
                                                 const MobileGLBackendInitInfo& initInfo) const {
        if (!ValidateAbi() || m_manifest->Create == nullptr) {
            return nullptr;
        }
        return m_manifest->Create(&host, &utilApi, &initInfo);
    }
} // namespace MobileGL::FullServer

// End of File
