// MobileGL - MobileGL/MG_FullServer/FullServerEntry.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "FullServerEntry.h"
#include "BackendHost.h"
#include "BackendPluginLoader.h"
#include "ServerCore.h"
#include "UtilRuntimeLoader.h"

namespace MobileGL::FullServer {
    struct FullServerInstance {
        UtilRuntimeLoader runtime;
        BackendPluginLoader backend;
        MobileGLBackend* backendObject = nullptr;
        const MobileGLBackendVTable* vtable = nullptr;
        ServerCore* core = nullptr;
    };
} // namespace MobileGL::FullServer

extern "C" MobileGLFullServerHandle mobilegl_fullserver_create(const char* utilRuntimePath,
                                                               const char* backendPath) {
    if (utilRuntimePath == nullptr || backendPath == nullptr) {
        return nullptr;
    }

    auto* instance = new MobileGL::FullServer::FullServerInstance();
    if (!instance->runtime.Load(utilRuntimePath)) {
        delete instance;
        return nullptr;
    }
    if (!instance->backend.Load(backendPath)) {
        delete instance;
        return nullptr;
    }

    MobileGLBackendInitInfo initInfo{};
    initInfo.structSize = sizeof(MobileGLBackendInitInfo);
    initInfo.apiVersion = (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR;
    initInfo.backendType = static_cast<MobileGLBackendType>(instance->backend.GetManifest()->backendType);
    initInfo.pluginPath = backendPath;

    instance->backendObject = instance->backend.Create(
        MobileGL::FullServer::GetBackendHost(), *instance->runtime.GetApi(), initInfo);
    if (instance->backendObject == nullptr) {
        delete instance;
        return nullptr;
    }
    instance->vtable = instance->backend.GetManifest()->GetBackendVTable();
    if (instance->vtable == nullptr) {
        delete instance;
        return nullptr;
    }
    return instance;
}

extern "C" int mobilegl_fullserver_start(MobileGLFullServerHandle handle) {
    auto* instance = static_cast<MobileGL::FullServer::FullServerInstance*>(handle);
    if (instance == nullptr || instance->vtable == nullptr || instance->backendObject == nullptr) {
        return -1;
    }
    if (instance->vtable->Initialize != nullptr &&
        !instance->vtable->Initialize(instance->backendObject, nullptr)) {
        return -1;
    }
    return 0;
}

extern "C" int mobilegl_fullserver_attach_transport(MobileGLFullServerHandle handle,
                                                    const MobileGLTransportOps* ops,
                                                    MobileGLTransport* transport) {
    auto* instance = static_cast<MobileGL::FullServer::FullServerInstance*>(handle);
    if (instance == nullptr || ops == nullptr || transport == nullptr) {
        return -1;
    }
    if (instance->core != nullptr) {
        return -1;
    }
    instance->core = new MobileGL::FullServer::ServerCore(ops, transport, instance->backendObject,
                                                          instance->vtable);
    return instance->core->Start() ? 0 : -1;
}

extern "C" int mobilegl_fullserver_service_once(MobileGLFullServerHandle handle) {
    auto* instance = static_cast<MobileGL::FullServer::FullServerInstance*>(handle);
    if (instance == nullptr || instance->core == nullptr) {
        return -1;
    }
    return instance->core->ServiceOnce() ? 0 : -1;
}

extern "C" void mobilegl_fullserver_destroy(MobileGLFullServerHandle handle) {
    auto* instance = static_cast<MobileGL::FullServer::FullServerInstance*>(handle);
    if (instance == nullptr) {
        return;
    }
    if (instance->core != nullptr) {
        instance->core->Shutdown();
        delete instance->core;
    }
    if (instance->vtable != nullptr && instance->vtable->Shutdown != nullptr &&
        instance->backendObject != nullptr) {
        instance->vtable->Shutdown(instance->backendObject);
    }
    delete instance;
}

// End of File
