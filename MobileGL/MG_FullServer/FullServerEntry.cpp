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
#include "MG_Protocol/control.h"
#include "MG_Protocol/gen/wire_generated.h"
#include "MG_Protocol/gen/wire_full_generated.h"
#include "MG_Protocol/generated_opcodes.h"
#include "MG_Protocol/generated_wire_dispatch.h"
#include "Init.h"
#include "MG_State/GLState/ContextRegistry.h"
#include "MG_Transport/InProcessTransport.h"
#include "MG_Transport/LocalSocketShmTransport.h"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <flatbuffers/flatbuffers.h>
#include <thread>

namespace MobileGL::FullServer {
    struct FullServerInstance {
        UtilRuntimeLoader runtime;
        BackendPluginLoader backend;
        MobileGLBackend* backendObject = nullptr;
        const MobileGLBackendVTable* vtable = nullptr;
        ServerCore* core = nullptr;

        // In-process hosting state (MOBILEGL_CS_MODE=inprocess): the host owns
        // the server thread and the server half of the transport pair; the
        // client half is handed to MobileGL_Client.
        std::thread serverThread;
        std::atomic<Bool> running{false};
        MobileGLTransport* inProcessClient = nullptr;
        MobileGLTransport* inProcessServer = nullptr;
        const MobileGLTransportOps* inProcessOps = nullptr;
    };

    MobileGLSessionId s_frontendSession = 0;
    Bool s_wireRetValid = false;
    int64_t s_wireRetI64 = 0;
    const Uint8* s_wireRetBytes = nullptr;
    Uint32 s_wireRetBytesSize = 0;
    Vector<Uint8> s_wireOutBytes;

    void OnSessionChanged(MobileGLSessionId sessionId, void* user) {
        (void)user;
        BfaFrontendShim::Get().SetCurrentSession(sessionId);
        const auto threadId =
            static_cast<Uint64>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
        if (sessionId == 0) {
            if (s_frontendSession != 0) {
                MobileGL::MG_State::GLState::GLContextRegistry::DestroySession(s_frontendSession);
                s_frontendSession = 0;
            }
            MobileGL::MG_State::RestoreLegacyCurrentContext();
            return;
        }
        const auto group =
            MobileGL::MG_State::GLState::GLContextRegistry::GetOrCreateSharedGroup(0, 0);
        MobileGL::MG_State::GLState::GLContextRegistry::CreateSession(0, group, sessionId);
        MobileGL::MG_State::GLState::GLContextRegistry::SetCurrent(threadId, sessionId);
        auto* context = MobileGL::MG_State::GLState::GLContextRegistry::GetCurrentGLContext(threadId);
        if (context != nullptr) {
            MobileGL::MG_State::pGLContext = context;
        }
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

    uint32_t WireDispatchWithSession(uint32_t opcode, uint32_t sessionId,
                                     const void* payloadBytes, uint64_t payloadSize,
                                     const void* const* receivedShm, uint32_t receivedShmCount,
                                     uint32_t outCapacity) {
        const auto threadId =
            static_cast<Uint64>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
        MobileGL::MG_State::GLState::GLContextRegistry::SetCurrent(threadId, sessionId);
        auto* context = MobileGL::MG_State::GLState::GLContextRegistry::GetCurrentGLContext(threadId);
        if (context != nullptr) {
            MobileGL::MG_State::pGLContext = context;
        }
        BfaFrontendShim::Get().SetCurrentSession(sessionId);

        // glShaderSource needs a real pointer array, which the generic payload
        // cannot express: the client sends NUL-joined sources in shm, we
        // rebuild the array here and call the frontend entry directly.
        if (static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glShaderSource) == opcode &&
            payloadBytes != nullptr && payloadSize >= 4 && receivedShmCount > 0 &&
            receivedShm != nullptr && receivedShm[0] != nullptr && outCapacity == 0) {
            const auto* p = ::flatbuffers::GetRoot<
                MobileGL::Protocol::WireFull::GenShaderSource>(payloadBytes);
            if (p != nullptr && p->count() > 0 && p->count() < 1024) {
                const char* base = static_cast<const char*>(receivedShm[0]);
                Vector<const GLchar*> sources;
                sources.reserve(static_cast<SizeT>(p->count()));
                const char* cursor = base;
                for (int i = 0; i < p->count(); ++i) {
                    sources.push_back(static_cast<const GLchar*>(cursor));
                    cursor += std::strlen(cursor) + 1;
                }
                ::glShaderSource(p->shader(), p->count(), sources.data(), nullptr);
                return 0u;
            }
        }

        // glGetActiveAttrib / glGetActiveUniform: pack [length,size,type,name]
        // into Response.ret_bytes through the WireRetProvider path.
        if ((static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glGetActiveAttrib) == opcode ||
             static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glGetActiveUniform) == opcode) &&
            payloadBytes != nullptr && payloadSize >= 4) {
            const auto* p =
                ::flatbuffers::GetRoot<MobileGL::Protocol::Wire::ActiveVariable>(payloadBytes);
            if (p != nullptr) {
                GLsizei length = 0;
                GLint size = 0;
                GLenum type = 0;
                GLchar name[1024] = {};
                const GLsizei buf = p->buf_size() > 1023 ? 1023
                                                         : static_cast<GLsizei>(p->buf_size());
                const GLuint program = p->program();
                const GLuint index = p->index();
                if (static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glGetActiveAttrib) == opcode) {
                    ::glGetActiveAttrib(program, index, buf, &length, &size, &type, name);
                } else {
                    ::glGetActiveUniform(program, index, buf, &length, &size, &type, name);
                }
                s_wireOutBytes.clear();
                const auto pushU32 = [](Vector<Uint8>& out, uint32_t value) {
                    const Uint8 bytes[4] = {static_cast<Uint8>(value),
                                            static_cast<Uint8>(value >> 8),
                                            static_cast<Uint8>(value >> 16),
                                            static_cast<Uint8>(value >> 24)};
                    out.insert(out.end(), bytes, bytes + 4);
                };
                pushU32(s_wireOutBytes, static_cast<uint32_t>(length));
                pushU32(s_wireOutBytes, static_cast<uint32_t>(size));
                pushU32(s_wireOutBytes, static_cast<uint32_t>(type));
                const SizeT nameLen = (length > 0 && static_cast<Uint32>(length) < static_cast<Uint32>(buf))
                    ? static_cast<SizeT>(length)
                    : 0;
                s_wireOutBytes.insert(s_wireOutBytes.end(), name, name + nameLen);
                s_wireOutBytes.push_back(0);
                s_wireRetValid = true;
                s_wireRetI64 = 0;
                s_wireRetBytes = s_wireOutBytes.data();
                s_wireRetBytesSize = static_cast<Uint32>(s_wireOutBytes.size());
                return 0u;
            }
        }

        const uint32_t result = MobileGL::Protocol::Wire::WireDispatchCall(
            opcode, sessionId, payloadBytes, payloadSize, receivedShm, receivedShmCount,
            outCapacity);
        s_wireRetValid = MobileGL::Protocol::Wire::WireDispatchReturnValid();
        s_wireRetI64 = MobileGL::Protocol::Wire::WireDispatchReturnI64();
        s_wireRetBytes = MobileGL::Protocol::Wire::WireDispatchBytes();
        s_wireRetBytesSize = MobileGL::Protocol::Wire::WireDispatchBytesSize();
        return result;
    }

    // Forward the scalar/out-vector return captured by WireDispatchWithSession
    // to the ServerCore response path without linking ServerCore to the
    // generated dispatch implementation.
    void WireRetProvider(Bool* outValid, int64_t* outRetI64,
                         const Uint8** outBytes, Uint32* outBytesSize) {
        if (outValid != nullptr) {
            *outValid = s_wireRetValid;
        }
        if (outRetI64 != nullptr) {
            *outRetI64 = s_wireRetI64;
        }
        if (outBytes != nullptr) {
            *outBytes = s_wireRetBytes;
        }
        if (outBytesSize != nullptr) {
            *outBytesSize = s_wireRetBytesSize;
        }
    }
} // namespace MobileGL::FullServer

extern "C" MobileGLFullServerHandle mobilegl_fullserver_create(const char* utilRuntimePath,
                                                               const char* backendPath) {
    if (utilRuntimePath == nullptr || backendPath == nullptr) {
        return nullptr;
    }

    // The frontend probes (GL_Program, GL_Getter, EGLImpl, ...) read
    // MG_Backend::pActiveBackendObject for renderer/capability info; that
    // object only exists after the monolith-style MobileGL::Initialize().
    // Backend selection in the C/S build is owned by MOBILEGL_CS_BACKEND;
    // MOBILEGL_BACKEND_TYPE is a monolith-era variable that launchers may
    // inject with an arbitrary/empty value (ConfigLoader would resolve it to
    // Unknown), so pin the monitor env var to the C/S value first.
    {
        const char* csBackend = std::getenv("MOBILEGL_CS_BACKEND");
        const char* backendName =
            (csBackend != nullptr && csBackend[0] != '\0') ? csBackend : "DirectGLES";
#if defined(_WIN32)
        _putenv_s("MOBILEGL_BACKEND_TYPE", backendName);
#else
        setenv("MOBILEGL_BACKEND_TYPE", backendName, 1);
#endif
    }
    MobileGL::Initialize();

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
    instance->core->SetWireDispatch(&MobileGL::FullServer::WireDispatchWithSession);
    instance->core->SetWireRet(&MobileGL::FullServer::WireRetProvider);
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
    instance->core->SetWireDispatch(&MobileGL::FullServer::WireDispatchWithSession);
    instance->core->SetWireRet(&MobileGL::FullServer::WireRetProvider);
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

namespace MobileGL::FullServer {
    // Unblocks a blocking WaitResponses inside the hosted server loop by
    // submitting the ServerShutdown sentinel on the client half. The server
    // thread observes `running == false` right after ServiceOnce returns.
    void WakeUpInProcess(FullServerInstance* instance) {
        if (instance == nullptr || instance->inProcessClient == nullptr ||
            instance->inProcessOps == nullptr) {
            return;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto command = MobileGL::Protocol::Wire::CreateCommand(
            builder,
            static_cast<uint32_t>(MobileGL::Protocol::MobileGLControlOpcode::ServerShutdown),
            0, 0);
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<uint32_t>(builder.GetSize());
        instance->inProcessOps->SubmitCommands(instance->inProcessClient, &batch);
    }
} // namespace MobileGL::FullServer

extern "C" MobileGLFullServerHandle mobilegl_fullserver_create_inprocess(
    const char* utilRuntimePath,
    const char* backendPath,
    MobileGLTransport** clientOut,
    const MobileGLTransportOps** clientOpsOut) {
    if (utilRuntimePath == nullptr || backendPath == nullptr || clientOut == nullptr ||
        clientOpsOut == nullptr) {
        return nullptr;
    }

    auto* instance = static_cast<MobileGL::FullServer::FullServerInstance*>(
        mobilegl_fullserver_create(utilRuntimePath, backendPath));
    if (instance == nullptr) {
        return nullptr;
    }

    MobileGLTransport* client = nullptr;
    MobileGLTransport* server = nullptr;
    MobileGL::Transport::CreateInProcessTransportPair(&client, &server);
    const MobileGLTransportOps& ops = MobileGL::Transport::GetInProcessTransportOps();

    MobileGLTransportConfig config{};
    config.structSize = sizeof(MobileGLTransportConfig);
    config.kind = MobileGLTransportKindInProcess;
    config.maxBatchCommands = 256;
    config.maxShmArenaSize = 64u * 1024u * 1024u;
    config.timeoutMs = 0;
    if (!ops.Start(client, &config) || !ops.Start(server, &config)) {
        MobileGL::Transport::DestroyInProcessTransport(client);
        MobileGL::Transport::DestroyInProcessTransport(server);
        mobilegl_fullserver_destroy(instance);
        return nullptr;
    }

    instance->core = new MobileGL::FullServer::ServerCore(
        &ops, server, instance->backendObject, instance->vtable);
    instance->core->SetSessionListener(&MobileGL::FullServer::OnSessionChanged, nullptr);
    instance->core->SetWireDispatch(&MobileGL::FullServer::WireDispatchWithSession);
    instance->core->SetWireRet(&MobileGL::FullServer::WireRetProvider);
    if (!instance->core->Start()) {
        delete instance->core;
        instance->core = nullptr;
        MobileGL::Transport::DestroyInProcessTransport(client);
        MobileGL::Transport::DestroyInProcessTransport(server);
        mobilegl_fullserver_destroy(instance);
        return nullptr;
    }

    instance->inProcessClient = client;
    instance->inProcessServer = server;
    instance->inProcessOps = &ops;
    instance->running = true;
    instance->serverThread = std::thread([instance]() {
        while (instance->running.load()) {
            if (!instance->core->ServiceOnce()) {
                break;
            }
        }
    });

    *clientOut = client;
    *clientOpsOut = &ops;
    return instance;
}

extern "C" void mobilegl_fullserver_destroy(MobileGLFullServerHandle handle) {
    auto* instance = static_cast<MobileGL::FullServer::FullServerInstance*>(handle);
    if (instance == nullptr) {
        return;
    }
    // Stop the hosted server loop: set the flag first, then submit the
    // ServerShutdown sentinel so a blocked WaitResponses unblocks and the
    // thread can observe running == false and exit.
    if (instance->running.load()) {
        instance->running = false;
        MobileGL::FullServer::WakeUpInProcess(instance);
    }
    if (instance->serverThread.joinable()) {
        instance->serverThread.join();
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
    // The server half of the in-process pair is owned here; the client half is
    // owned by MobileGL_Client and freed by Client::Shutdown.
    if (instance->inProcessServer != nullptr) {
        MobileGL::Transport::DestroyInProcessTransport(instance->inProcessServer);
        instance->inProcessServer = nullptr;
    }
    instance->inProcessClient = nullptr;
    instance->inProcessOps = nullptr;
    delete instance;
}

// End of File
