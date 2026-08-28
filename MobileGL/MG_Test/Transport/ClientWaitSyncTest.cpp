// MobileGL - MobileGL/MG_Test/Transport/ClientWaitSyncTest.cpp
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
    void* g_waitedSync = nullptr;
    uint32_t g_flags = 0;
    uint64_t g_timeout = 0;

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

    void* TestFenceSync(MobileGLBackend* backend, MobileGLSessionId session,
                        uint32_t condition, uint32_t flags) {
        (void)backend;
        (void)session;
        (void)condition;
        (void)flags;
        return reinterpret_cast<void*>(0x1234);
    }

    void TestWaitSync(MobileGLBackend* backend, MobileGLSessionId session, void* sync,
                      uint32_t flags, uint64_t timeout) {
        (void)backend;
        (void)session;
        g_waitedSync = sync;
        g_flags = flags;
        g_timeout = timeout;
    }
} // namespace

namespace MobileGL::Transport {
    TEST(ClientWaitSyncTest, FenceThenWait) {
        g_waitedSync = nullptr;
        g_flags = 0;
        g_timeout = 0;

        MobileGLBackend backend{};
        MobileGLBackendVTable vtable{};
        vtable.structSize = sizeof(MobileGLBackendVTable);
        vtable.apiVersion = (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR;
        vtable.OnSessionCreated = &OnSessionCreated;
        vtable.OnSessionDestroyed = &OnSessionDestroyed;
        vtable.FenceSync = &TestFenceSync;
        vtable.WaitSync = &TestWaitSync;

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
            for (int i = 0; i < 3; ++i) {
                if (!core.ServiceOnce()) return;
            }
            serverOk = true;
        });

        EXPECT_TRUE(Client::SubmitSessionControl(25, true, 1));
        EXPECT_TRUE(Client::WaitResponseForToken(1, 5000));
        uint64_t sync = 0;
        EXPECT_TRUE(Client::SendFenceSync(25, 0x11, 0x22, 2, &sync));
        EXPECT_TRUE(Client::SendWaitSync(25, sync, 0x33, 0x12345678, 3));

        serverThread.join();
        EXPECT_TRUE(serverOk);
        EXPECT_EQ(reinterpret_cast<uint64_t>(g_waitedSync), 0x1234u);
        EXPECT_EQ(g_flags, 0x33u);
        EXPECT_EQ(g_timeout, 0x12345678u);

        core.Shutdown();
        Client::Shutdown();
        DestroyInProcessTransport(server);
    }
} // namespace MobileGL::Transport

// End of File
