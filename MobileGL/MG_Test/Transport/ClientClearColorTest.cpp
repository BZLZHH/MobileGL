// MobileGL - MobileGL/MG_Test/Transport/ClientClearColorTest.cpp
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
#include "MG_Transport/InProcessTransport.h"

struct MobileGLBackend {
    uint32_t Nonce = 0;
};

namespace {
    float g_red = 0, g_green = 0, g_blue = 0, g_alpha = 0;

    bool OnSessionCreated(MobileGLBackend* backend, MobileGLSessionId session,
                          const MobileGLBackendInitInfo* info) {
        (void)backend;
        (void)session;
        (void)info;
        return true;
    }

    void OnSessionDestroyed(MobileGLBackend* backend, MobileGLSessionId session) {
        (void)backend;
        (void)session;
    }

    void TestClearColor(MobileGLBackend* backend, MobileGLSessionId session,
                        float red, float green, float blue, float alpha) {
        (void)backend;
        (void)session;
        g_red = red;
        g_green = green;
        g_blue = blue;
        g_alpha = alpha;
    }
} // namespace

namespace MobileGL::Transport {
    TEST(ClientClearColorTest, TypedClearColorPayload) {
        g_red = g_green = g_blue = g_alpha = 0;

        MobileGLBackend backend{};
        MobileGLBackendVTable vtable{};
        vtable.structSize = sizeof(MobileGLBackendVTable);
        vtable.apiVersion = (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR;
        vtable.OnSessionCreated = &OnSessionCreated;
        vtable.OnSessionDestroyed = &OnSessionDestroyed;
        vtable.ClearColor = &TestClearColor;

        MobileGLTransport* client = nullptr;
        MobileGLTransport* server = nullptr;
        CreateInProcessTransportPair(&client, &server);
        ASSERT_NE(client, nullptr);
        ASSERT_NE(server, nullptr);

        const MobileGLTransportOps& ops = GetInProcessTransportOps();
        MobileGLTransportConfig config{};
        config.structSize = sizeof(MobileGLTransportConfig);
        config.kind = MobileGLTransportKindInProcess;
        ASSERT_TRUE(ops.Start(client, &config));
        ASSERT_TRUE(ops.Start(server, &config));

        MobileGL::FullServer::ServerCore core(&ops, server, &backend, &vtable);
        ASSERT_TRUE(core.Start());
        ASSERT_TRUE(Client::InitializeWithTransport(client, &ops));

        Bool serverOk = false;
        std::thread serverThread([&] {
            for (int i = 0; i < 2; ++i) {
                if (!core.ServiceOnce()) return;
            }
            serverOk = true;
        });

        EXPECT_TRUE(Client::SubmitSessionControl(5, true, 1));
        EXPECT_TRUE(Client::WaitResponseForToken(1, 5000));
        EXPECT_TRUE(Client::SendClearColor(5, 0.25f, 0.5f, 0.75f, 1.0f, 2));

        serverThread.join();
        EXPECT_TRUE(serverOk);
        EXPECT_FLOAT_EQ(g_red, 0.25f);
        EXPECT_FLOAT_EQ(g_green, 0.5f);
        EXPECT_FLOAT_EQ(g_blue, 0.75f);
        EXPECT_FLOAT_EQ(g_alpha, 1.0f);

        core.Shutdown();
        Client::Shutdown();
        DestroyInProcessTransport(server);
    }
} // namespace MobileGL::Transport

// End of File
