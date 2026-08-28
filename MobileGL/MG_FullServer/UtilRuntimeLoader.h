// MobileGL - MobileGL/MG_FullServer/UtilRuntimeLoader.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#pragma once

#include <Includes.h>
#include "MG_Protocol/mgruntime_api.h"

namespace MobileGL::FullServer {
    // Loads MobileGL_UtilRuntime.so, negotiates the ABI and exposes the root
    // MobileGLUtilApi for injection into BackendObject plugins.
    class UtilRuntimeLoader {
    public:
        UtilRuntimeLoader() = default;
        ~UtilRuntimeLoader();

        UtilRuntimeLoader(const UtilRuntimeLoader&) = delete;
        UtilRuntimeLoader& operator=(const UtilRuntimeLoader&) = delete;

        Bool Load(const char* libraryPath);
        void Unload();

        const MobileGLUtilApi* GetApi() const { return m_api; }
        Bool IsLoaded() const { return m_api != nullptr; }

    private:
        void* m_library = nullptr;
        const MobileGLUtilApi* m_api = nullptr;
    };
} // namespace MobileGL::FullServer

// End of File
