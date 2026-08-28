// MobileGL - MobileGL/FullServer/UtilRuntimeLoader.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "UtilRuntimeLoader.h"

#if defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__)
#include <dlfcn.h>
#endif

namespace MobileGL::FullServer {
    UtilRuntimeLoader::~UtilRuntimeLoader() {
        Unload();
    }

    Bool UtilRuntimeLoader::Load(const char* libraryPath) {
        if (m_library != nullptr || libraryPath == nullptr) {
            return false;
        }

#ifdef _WIN32
        m_library = reinterpret_cast<void*>(LoadLibraryA(libraryPath));
        if (m_library == nullptr) {
            return false;
        }
        auto apiFn = reinterpret_cast<const MobileGLUtilApi* (*)(uint32_t, uint32_t)>(
            GetProcAddress(reinterpret_cast<HMODULE>(m_library), "mobilegl_util_api"));
#else
        m_library = dlopen(libraryPath, RTLD_NOW | RTLD_LOCAL);
        if (m_library == nullptr) {
            return false;
        }
        auto apiFn = reinterpret_cast<const MobileGLUtilApi* (*)(uint32_t, uint32_t)>(
            dlsym(m_library, "mobilegl_util_api"));
#endif

        if (apiFn == nullptr) {
            Unload();
            return false;
        }

        m_api = apiFn(sizeof(MobileGLUtilApi), MOBILEGL_RUNTIME_ABI_MAJOR);
        if (m_api == nullptr || m_api->abiMajor != MOBILEGL_RUNTIME_ABI_MAJOR ||
            m_api->structSize < sizeof(MobileGLUtilApi)) {
            Unload();
            return false;
        }
        return true;
    }

    void UtilRuntimeLoader::Unload() {
        if (m_library != nullptr) {
#ifdef _WIN32
            FreeLibrary(reinterpret_cast<HMODULE>(m_library));
#else
            dlclose(m_library);
#endif
        }
        m_library = nullptr;
        m_api = nullptr;
    }
} // namespace MobileGL::FullServer

// End of File
