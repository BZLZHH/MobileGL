// MobileGL - MobileGL/MG_Backend/DirectGLES/PluginManifest.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include <Includes.h>
#include "MG_Protocol/bfa.h"
#include "RealBackend.h"

// Plugin entry point. The DirectGLES real backend lives in RealBackend.cpp;
// this TU only owns the manifest exchange with FullServer.

namespace {
    uint32_t GetAbiVersion() {
        return (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR;
    }

    const MobileGLBackendManifest s_manifest = {sizeof(MobileGLBackendManifest),
                                                MOBILEGL_BFA_ABI_MAJOR,
                                                MOBILEGL_BFA_ABI_MINOR,
                                                MobileGLBackendTypeDirectGLES,
                                                "DirectGLES BackendObject",
                                                "0.2.0",
                                                &GetAbiVersion,
                                                &RealBackendGetVTable,
                                                &RealBackendCreate};
} // namespace

extern "C" const MobileGLBackendManifest* mobilegl_backend_manifest(void) {
    return &s_manifest;
}

// End of File
