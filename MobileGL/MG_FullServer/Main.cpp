// MobileGL - MobileGL/MG_FullServer/Main.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include <Includes.h>
#include "BackendHost.h"
#include "BackendPluginLoader.h"
#include "UtilRuntimeLoader.h"

int main(int argc, char** argv) {
    if (argc < 3) {
        return 1;
    }

    MobileGL::FullServer::UtilRuntimeLoader runtimeLoader;
    if (!runtimeLoader.Load(argv[1])) {
        return 1;
    }

    MobileGL::FullServer::BackendPluginLoader backendLoader;
    if (!backendLoader.Load(argv[2])) {
        return 1;
    }

    MobileGLBackendInitInfo initInfo{};
    initInfo.structSize = sizeof(MobileGLBackendInitInfo);
    initInfo.apiVersion = (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR;
    initInfo.backendType = static_cast<MobileGLBackendType>(backendLoader.GetManifest()->backendType);
    initInfo.pluginPath = argv[2];

    MobileGLBackend* backend = backendLoader.Create(
        MobileGL::FullServer::GetBackendHost(), *runtimeLoader.GetApi(), initInfo);
    if (backend == nullptr) {
        return 1;
    }

    const MobileGLBackendVTable* vtable = backendLoader.GetManifest()->GetBackendVTable();
    if (vtable == nullptr) {
        return 1;
    }
    if (vtable->Initialize != nullptr && !vtable->Initialize(backend, &initInfo)) {
        return 1;
    }

    // TODO(Phase 4/5): start the transport server and route sessions into this
    // backend; v1 supports headless/pbuffer on the same device.
    if (vtable->Shutdown != nullptr) {
        vtable->Shutdown(backend);
    }
    return 0;
}

// End of File
