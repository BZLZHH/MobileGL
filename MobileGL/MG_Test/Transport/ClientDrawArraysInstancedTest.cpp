// MobileGL - MobileGL/MG_Test/Transport/ClientDrawArraysInstancedTest.cpp
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
    uint32_t g_mode = 0;
    int32_t g_first = 0, g_count = 0;
    int32_t g_primcount = 0;

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

    void TestDrawArraysInstanced(MobileGLBackend* backend, MobileGLSessionId session,
                                 uint32_t mode, int32_t first, int32_t count,
                                 int32_t primcount) {
        (void)backend;
        (void)session;
        g_mode = mode;
        g_first = first;
        g_count = count;
        g_primcount = primcount;
    }
} // namespace

namespace MobileGL::Transport {
    TEST(ClientDrawArraysInstancedTest, TypedPayload) {
        g_mode = 0;
        g_first = 0;
        g_count = 0;
        g_primcount = 0;

        MobileGLBackend backend{};
        MobileGLBackendVTable vtable{};
        vtable.structSize = sizeof(MobileGLBackendVTable);
        vtable.apiVersion = (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR;
        vtable.OnSessionCreated = &OnSessionCreated;
        vtable.OnSessionDestroyed = &OnSessionDestroyed;
        vtable.DrawArraysInstanced = &TestDrawArraysInstanced;

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

        EXPECT_TRUE(Client::SubmitSessionControl(26, true, 1));
        EXPECT_TRUE(Client::WaitResponseForToken(1, 5000));
        EXPECT_TRUE(Client::SendDrawArraysInstanced(26, 4, 3, 6, 2, 2));

        serverThread.join();
        EXPECT_TRUE(serverOk);
        EXPECT_EQ(g_mode, 4u);
        EXPECT_EQ(g_first, 3);
        EXPECT_EQ(g_count, 6);
        EXPECT_EQ(g_primcount, 2);

        core.Shutdown();
        Client::Shutdown();
        DestroyInProcessTransport(server);
    }
} // namespace MobileGL::Transport

// End of File
