// MobileGL - MobileGL/MG_Test/Transport/ClientGetStringTest.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include <Includes.h>
#include <gtest/gtest.h>
#include "MG_Client/Client.h"
#include "MG_FullServer/ServerCore.h"
#include "MG_Protocol/transport.h"
#include "MG_Transport/LocalSocketShmTransport.h"

#include <unistd.h>

struct MobileGLBackend {
    uint32_t Nonce = 0;
};

namespace {
    const char* kExtensions[] = {
        "GL_EXT_mobilegl_test",
        "GL_KHR_debug",
        nullptr,
    };

    MobileGLRendererInfo s_rendererInfo = {.structSize = sizeof(MobileGLRendererInfo),
                                           .name = "Espryt Renderer",
                                           .vendor = "Espryt Vendor",
                                           .version = "OpenGL ES 3.2",
                                           .shaderLanguageVersion = "OpenGL ES GLSL ES 3.20",
                                           .extensions = kExtensions,
                                           .extensionCount = 2,
                                           .reserved = 0};

    bool OnSessionCreated(MobileGLBackend* backend, MobileGLSessionId session, const MobileGLBackendInitInfo* info) {
        (void)backend;
        (void)session;
        (void)info;
        return true;
    }

    void OnSessionDestroyed(MobileGLBackend* backend, MobileGLSessionId session) {
        (void)backend;
        (void)session;
    }

    const MobileGLRendererInfo* GetRendererInfo(MobileGLBackend* backend, MobileGLSessionId session) {
        (void)backend;
        (void)session;
        return &s_rendererInfo;
    }
} // namespace

namespace MobileGL::Transport {
    TEST(ClientGetStringTest, StringQueriesRoundTrip) {
        const char* endpoint = "/tmp/mobilegl_getstring_test.sock";
        unlink(endpoint);

        MobileGLBackend backend{};
        MobileGLBackendVTable vtable{};
        vtable.structSize = sizeof(MobileGLBackendVTable);
        vtable.apiVersion = (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR;
        vtable.OnSessionCreated = &OnSessionCreated;
        vtable.OnSessionDestroyed = &OnSessionDestroyed;
        vtable.GetRendererInfo = &GetRendererInfo;

        MobileGLTransport* server = CreateLocalSocketShmServer(endpoint);
        ASSERT_NE(server, nullptr);
        MobileGLTransport* client = CreateLocalSocketShmTransport();
        ASSERT_NE(client, nullptr);
        const MobileGLTransportOps& ops = GetLocalSocketShmTransportOps();

        MobileGLTransportConfig config{};
        config.structSize = sizeof(MobileGLTransportConfig);
        config.kind = MobileGLTransportKindLocalSocketShm;
        config.endpoint = endpoint;
        config.maxShmArenaSize = 1024 * 1024;
        ASSERT_TRUE(ops.Start(client, &config));

        MobileGLTransport* accepted = AcceptLocalSocketShmConnection(server);
        ASSERT_NE(accepted, nullptr);

        MobileGL::FullServer::ServerCore core(&ops, accepted, &backend, &vtable);
        ASSERT_TRUE(core.Start());
        ASSERT_TRUE(Client::InitializeWithTransport(client, &ops));

        Bool serverOk = false;
        std::thread serverThread([&] {
            for (int i = 0; i < 5; ++i) {
                if (!core.ServiceOnce()) return;
            }
            serverOk = true;
        });

        ASSERT_TRUE(Client::SubmitSessionControl(10, true, 1));
        EXPECT_TRUE(Client::WaitResponseForToken(1, 5000));

        String value;
        ASSERT_TRUE(Client::SendGetString(10, static_cast<uint32_t>(GL_VENDOR), 2, &value));
        EXPECT_EQ(value, "Espryt Vendor");
        ASSERT_TRUE(Client::SendGetString(10, static_cast<uint32_t>(GL_RENDERER), 3, &value));
        EXPECT_EQ(value, "Espryt Renderer");
        ASSERT_TRUE(Client::SendGetString(10, static_cast<uint32_t>(GL_VERSION), 4, &value));
        EXPECT_EQ(value, "OpenGL ES 3.2");
        ASSERT_TRUE(Client::SendGetString(10, static_cast<uint32_t>(GL_SHADING_LANGUAGE_VERSION), 5, &value));
        EXPECT_EQ(value, "OpenGL ES GLSL ES 3.20");

        value.clear();
        ASSERT_TRUE(Client::SendGetStringi(10, static_cast<uint32_t>(GL_EXTENSIONS), 0, 6, &value));
        EXPECT_EQ(value, "GL_EXT_mobilegl_test");
        value.clear();
        ASSERT_TRUE(Client::SendGetStringi(10, static_cast<uint32_t>(GL_EXTENSIONS), 1, 7, &value));
        EXPECT_EQ(value, "GL_KHR_debug");
        value.clear();
        EXPECT_FALSE(Client::SendGetStringi(10, static_cast<uint32_t>(GL_EXTENSIONS), 2, 8, &value));

        serverThread.join();
        EXPECT_TRUE(serverOk);

        core.Shutdown();
        Client::Shutdown();

        DestroyLocalSocketShmTransport(accepted);
        DestroyLocalSocketShmTransport(client);
        DestroyLocalSocketShmTransport(server);
        unlink(endpoint);
    }
} // namespace MobileGL::Transport

// End of File
