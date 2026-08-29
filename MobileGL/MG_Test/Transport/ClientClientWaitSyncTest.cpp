// MobileGL - MobileGL/MG_Test/Transport/ClientClientWaitSyncTest.cpp
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
    uint64_t g_waitedSync = 0;
    uint32_t g_flags = 0;
    uint64_t g_timeout = 0;
    uint64_t g_endedQuery = 0;
    uint64_t g_deletedQuery = 0;

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

    uint32_t TestClientWaitSync(MobileGLBackend* backend, MobileGLSessionId session,
                                void* sync, uint32_t flags, uint64_t timeout) {
        (void)backend;
        (void)session;
        g_waitedSync = reinterpret_cast<uint64_t>(sync);
        g_flags = flags;
        g_timeout = timeout;
        return 0x911B;
    }

    void* TestBeginTimeElapsedQuery(MobileGLBackend* backend, MobileGLSessionId session) {
        (void)backend;
        (void)session;
        return reinterpret_cast<void*>(0x7777);
    }

    void TestEndTimeElapsedQuery(MobileGLBackend* backend, MobileGLSessionId session,
                                 void* query) {
        (void)backend;
        (void)session;
        g_endedQuery = reinterpret_cast<uint64_t>(query);
    }

    void TestDeleteBackendQuery(MobileGLBackend* backend, MobileGLSessionId session,
                                void* query) {
        (void)backend;
        (void)session;
        g_deletedQuery = reinterpret_cast<uint64_t>(query);
    }
} // namespace

namespace MobileGL::Transport {
    TEST(ClientClientWaitSyncTest, ClientWaitAndQueryLifecycle) {
        g_waitedSync = 0;
        g_flags = 0;
        g_timeout = 0;
        g_endedQuery = 0;
        g_deletedQuery = 0;

        MobileGLBackend backend{};
        MobileGLBackendVTable vtable{};
        vtable.structSize = sizeof(MobileGLBackendVTable);
        vtable.apiVersion = (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR;
        vtable.OnSessionCreated = &OnSessionCreated;
        vtable.OnSessionDestroyed = &OnSessionDestroyed;
        vtable.ClientWaitSync = &TestClientWaitSync;
        vtable.BeginTimeElapsedQuery = &TestBeginTimeElapsedQuery;
        vtable.EndTimeElapsedQuery = &TestEndTimeElapsedQuery;
        vtable.DeleteBackendQuery = &TestDeleteBackendQuery;

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
            for (int i = 0; i < 5; ++i) {
                if (!core.ServiceOnce()) return;
            }
            serverOk = true;
        });

        EXPECT_TRUE(Client::SubmitSessionControl(31, true, 1));
        EXPECT_TRUE(Client::WaitResponseForToken(1, 5000));

        uint32_t result = 0;
        EXPECT_TRUE(Client::SendClientWaitSync(31, 0x1234, 0x33, 0x55, 2, &result));
        EXPECT_EQ(result, 0x911Bu);
        EXPECT_EQ(g_waitedSync, 0x1234u);
        EXPECT_EQ(g_flags, 0x33u);
        EXPECT_EQ(g_timeout, 0x55u);

        uint64_t query = 0;
        EXPECT_TRUE(Client::SendBeginTimeElapsedQuery(31, 3, &query));
        EXPECT_EQ(query, 0x7777u);
        EXPECT_TRUE(Client::SendEndTimeElapsedQuery(31, query, 4));
        EXPECT_EQ(g_endedQuery, 0x7777u);
        EXPECT_TRUE(Client::SendDeleteBackendQuery(31, query, 5));
        EXPECT_EQ(g_deletedQuery, 0x7777u);

        serverThread.join();
        EXPECT_TRUE(serverOk);

        core.Shutdown();
        Client::Shutdown();
        DestroyInProcessTransport(server);
    }
} // namespace MobileGL::Transport

// End of File
