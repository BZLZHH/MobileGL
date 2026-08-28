// MobileGL - MobileGL/MG_Test/Transport/ClientBlitFramebufferTest.cpp
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
    int32_t g_sx0 = 0, g_sy0 = 0, g_sx1 = 0, g_sy1 = 0;
    int32_t g_dx0 = 0, g_dy0 = 0, g_dx1 = 0, g_dy1 = 0;
    uint32_t g_mask = 0, g_filter = 0;

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

    void TestBlit(MobileGLBackend* backend, MobileGLSessionId session,
                  MobileGLBackendHandle readFramebuffer, MobileGLBackendHandle drawFramebuffer,
                  int32_t sx0, int32_t sy0, int32_t sx1, int32_t sy1,
                  int32_t dx0, int32_t dy0, int32_t dx1, int32_t dy1,
                  uint32_t mask, uint32_t filter) {
        (void)backend;
        (void)session;
        (void)readFramebuffer;
        (void)drawFramebuffer;
        g_sx0 = sx0; g_sy0 = sy0; g_sx1 = sx1; g_sy1 = sy1;
        g_dx0 = dx0; g_dy0 = dy0; g_dx1 = dx1; g_dy1 = dy1;
        g_mask = mask;
        g_filter = filter;
    }
} // namespace

namespace MobileGL::Transport {
    TEST(ClientBlitFramebufferTest, TenScalarPayload) {
        g_sx0 = g_sy0 = g_sx1 = g_sy1 = 0;
        g_dx0 = g_dy0 = g_dx1 = g_dy1 = 0;
        g_mask = g_filter = 0;

        MobileGLBackend backend{};
        MobileGLBackendVTable vtable{};
        vtable.structSize = sizeof(MobileGLBackendVTable);
        vtable.apiVersion = (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR;
        vtable.OnSessionCreated = &OnSessionCreated;
        vtable.OnSessionDestroyed = &OnSessionDestroyed;
        vtable.BlitFramebuffer = &TestBlit;

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

        EXPECT_TRUE(Client::SubmitSessionControl(19, true, 1));
        EXPECT_TRUE(Client::WaitResponseForToken(1, 5000));
        EXPECT_TRUE(Client::SendBlitFramebuffer(19, 100, 200, 1, 2, 3, 4, 5, 6, 7, 8, 0x11, 0x22, 2));

        serverThread.join();
        EXPECT_TRUE(serverOk);
        EXPECT_EQ(g_sx0, 1); EXPECT_EQ(g_sy0, 2); EXPECT_EQ(g_sx1, 3); EXPECT_EQ(g_sy1, 4);
        EXPECT_EQ(g_dx0, 5); EXPECT_EQ(g_dy0, 6); EXPECT_EQ(g_dx1, 7); EXPECT_EQ(g_dy1, 8);
        EXPECT_EQ(g_mask, 0x11u);
        EXPECT_EQ(g_filter, 0x22u);

        core.Shutdown();
        Client::Shutdown();
        DestroyInProcessTransport(server);
    }
} // namespace MobileGL::Transport

// End of File
