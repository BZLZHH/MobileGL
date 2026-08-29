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
#include "BfaFrontendShim.h"
#include "ServerCore.h"
#include "UtilRuntimeLoader.h"
#include "MG_State/GLState/ContextRegistry.h"
#include "MG_Transport/LocalSocketShmTransport.h"

namespace MobileGL::FullServer {
    struct FullServerInstance {
        UtilRuntimeLoader runtime;
        BackendPluginLoader backend;
        MobileGLBackend* backendObject = nullptr;
        const MobileGLBackendVTable* vtable = nullptr;
        ServerCore* core = nullptr;
    };

    MobileGLSessionId s_frontendSession = 0;

    void OnSessionChanged(MobileGLSessionId sessionId, void* user) {
        (void)user;
        BfaFrontendShim::Get().SetCurrentSession(sessionId);
        if (sessionId == 0) {
            if (s_frontendSession != 0) {
                MobileGL::MG_State::GLState::GLContextRegistry::DestroySession(s_frontendSession);
                s_frontendSession = 0;
            }
            return;
        }
        const auto group =
            MobileGL::MG_State::GLState::GLContextRegistry::GetOrCreateSharedGroup(0, 0);
        MobileGL::MG_State::GLState::GLContextRegistry::CreateSession(0, group, sessionId);
        MobileGL::MG_State::GLState::GLContextRegistry::SetCurrent(
            static_cast<Uint64>(std::hash<std::thread::id>{}(std::this_thread::get_id())),
            sessionId);
        s_frontendSession = sessionId;
    }

    MobileGLSessionId CurrentFrontendSession() {
        const auto threadId =
            static_cast<Uint64>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
        auto* context = MobileGL::MG_State::GLState::GLContextRegistry::GetCurrentGLContext(threadId);
        return context == nullptr ? 0 : context->GetSessionId();
    }

    MobileGLBackendHandle CurrentFrontendHandle(uint32_t objectKind, uint64_t glName) {
        const auto threadId =
            static_cast<Uint64>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
        auto* context = MobileGL::MG_State::GLState::GLContextRegistry::GetCurrentGLContext(threadId);
        return context == nullptr ? glName : context->GetObjectHandle(objectKind, glName);
    }
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
    MobileGL::FullServer::BfaFrontendShim::Get().Install(instance->backendObject, instance->vtable,
                                                         nullptr);
    MobileGL::FullServer::BfaFrontendShim::Get().m_state.SessionProvider =
        &MobileGL::FullServer::CurrentFrontendSession;
    MobileGL::FullServer::BfaFrontendShim::Get().m_state.HandleProvider =
        &MobileGL::FullServer::CurrentFrontendHandle;
    MobileGL::FullServer::BfaFrontendShim::Get().m_state.FramebufferNameProvider =
        [](const MobileGL::SharedPtr<MobileGL::MG_State::GLState::FramebufferObject>& framebuffer)
        -> MobileGL::Uint {
            return framebuffer != nullptr ? framebuffer->GetExternalIndex() : 0;
        };
    MobileGL::FullServer::BfaFrontendShim::Get().m_state.TextureNameProvider =
        [](const MobileGL::SharedPtr<MobileGL::MG_State::GLState::ITextureObject>& texture)
        -> MobileGL::Uint {
            return texture != nullptr ? texture->GetExternalIndex() : 0;
        };
    MobileGL::FullServer::BfaFrontendShim::Get().m_state.RenderbufferNameProvider =
        [](const MobileGL::SharedPtr<MobileGL::MG_State::GLState::RenderbufferObject>& renderbuffer)
        -> MobileGL::Uint {
            return renderbuffer != nullptr ? renderbuffer->GetExternalIndex() : 0;
        };
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
    instance->core->SetSessionListener(&MobileGL::FullServer::OnSessionChanged, nullptr);
    return instance->core->Start() ? 0 : -1;
}

extern "C" int mobilegl_fullserver_service_once(MobileGLFullServerHandle handle) {
    auto* instance = static_cast<MobileGL::FullServer::FullServerInstance*>(handle);
    if (instance == nullptr || instance->core == nullptr) {
        return -1;
    }
    return instance->core->ServiceOnce() ? 0 : -1;
}

extern "C" int mobilegl_fullserver_run_socket(MobileGLFullServerHandle handle,
                                              const char* endpoint,
                                              uint32_t maxCommands) {
    auto* instance = static_cast<MobileGL::FullServer::FullServerInstance*>(handle);
    if (instance == nullptr || endpoint == nullptr || maxCommands == 0) {
        return -1;
    }

    MobileGLTransport* server = MobileGL::Transport::CreateLocalSocketShmServer(endpoint);
    if (server == nullptr) {
        return -1;
    }
    MobileGLTransport* accepted = MobileGL::Transport::AcceptLocalSocketShmConnection(server);
    if (accepted == nullptr) {
        MobileGL::Transport::DestroyLocalSocketShmTransport(server);
        return -1;
    }

    const MobileGLTransportOps& ops = MobileGL::Transport::GetLocalSocketShmTransportOps();
    instance->core = new MobileGL::FullServer::ServerCore(&ops, accepted, instance->backendObject,
                                                          instance->vtable);
    instance->core->SetSessionListener(&MobileGL::FullServer::OnSessionChanged, nullptr);
    if (!instance->core->Start()) {
        delete instance->core;
        instance->core = nullptr;
        MobileGL::Transport::DestroyLocalSocketShmTransport(accepted);
        MobileGL::Transport::DestroyLocalSocketShmTransport(server);
        return -1;
    }

    int result = 0;
    for (uint32_t i = 0; i < maxCommands; ++i) {
        if (!instance->core->ServiceOnce()) {
            result = -1;
            break;
        }
    }

    instance->core->Shutdown();
    delete instance->core;
    instance->core = nullptr;
    MobileGL::Transport::DestroyLocalSocketShmTransport(accepted);
    MobileGL::Transport::DestroyLocalSocketShmTransport(server);
    return result;
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
    MobileGL::FullServer::BfaFrontendShim::Get().Clear();
    delete instance;
}

// End of File
