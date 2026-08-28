// MobileGL - MobileGL/MG_Backend/DirectVulkan/PluginManifest.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include <Includes.h>
#include "Protocol/bfa.h"

// Phase 5 wires the real DirectVulkan backend behind the BFA vtable. Until then
// the manifest exists so FullServer can perform ABI negotiation and return a
// structured "not available" error instead of a crash.
namespace {
    uint32_t GetAbiVersion() {
        return (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR;
    }

    // Skeleton vtable: structSize/apiVersion only, all entry points null until
    // Phase 5 fills them with the DirectVulkan adapter.
    const MobileGLBackendVTable s_backendVTable = {
        sizeof(MobileGLBackendVTable),
        (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR
    };

    const MobileGLBackendVTable* GetBackendVTable() {
        return &s_backendVTable;
    }

    MobileGLBackend* CreateBackend(const MobileGLBackendHost* host,
                                   const MobileGLUtilApi* utilApi,
                                   const MobileGLBackendInitInfo* initInfo) {
        (void)host;
        (void)utilApi;
        (void)initInfo;
        // TODO: Phase 5 instantiates the DirectVulkan backend adapter.
        return nullptr;
    }

    const MobileGLBackendManifest s_manifest = {
        sizeof(MobileGLBackendManifest),
        MOBILEGL_BFA_ABI_MAJOR,
        MOBILEGL_BFA_ABI_MINOR,
        MobileGLBackendTypeDirectVulkan,
        "DirectVulkan BackendObject",
        "0.1.0",
        &GetAbiVersion,
        &GetBackendVTable,
        &CreateBackend
    };
} // namespace

extern "C" const MobileGLBackendManifest* mobilegl_backend_manifest(void) {
    return &s_manifest;
}

// End of File
