// MobileGL - MobileGL/MG_Test/Transport/ClientTokenSessionTest.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include <Includes.h>
#include <gtest/gtest.h>
#include <thread>
#include "MG_Client/Client.h"
#include "MG_FullServer/ServerCore.h"
#include "MG_Protocol/generated_opcodes.h"
#include "MG_Protocol/transport.h"
#include "MG_Transport/InProcessTransport.h"

// This test defines the opaque backend object locally; it is not linked with
// any BackendObject plugin .so, so completing the type here is safe.
struct MobileGLBackend {
    uint32_t Nonce = 0;
};

namespace {
    void TestClear(MobileGLBackend* backend, MobileGLSessionId session, uint32_t mask) {
        (void)backend;
        (void)session;
        (void)mask;
    }

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

    MobileGLBackendVTable MakeVTable() {
        MobileGLBackendVTable vtable{};
        vtable.structSize = sizeof(MobileGLBackendVTable);
        vtable.apiVersion = (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR;
        vtable.OnSessionCreated = &OnSessionCreated;
        vtable.OnSessionDestroyed = &OnSessionDestroyed;
        vtable.Clear = &TestClear;
        return vtable;
    }
} // namespace

namespace MobileGL::Transport {
    TEST(ClientTokenSessionTest, InvalidateSessionDiscardsLateResponses) {
        constexpr uint64_t kSessionId = 77;
        constexpr uint64_t kClearToken = 2001;
        constexpr uint64_t kDestroyToken = 2002;

        MobileGLBackend backend{};
        const MobileGLBackendVTable vtable = MakeVTable();

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

        EXPECT_TRUE(Client::SubmitSessionControl(kSessionId, true, 1));
        EXPECT_TRUE(Client::WaitResponseForToken(1, 5000));

        // Submit a clear command without waiting; then invalidate the session
        // before the response is consumed. The wait must fail immediately and
        // the server's late response must be discarded by a later wait.
        EXPECT_TRUE(Client::SubmitDataCommand(
            static_cast<uint32_t>(kSessionId),
            static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glClear),
            kClearToken, 0, 0, nullptr));
        Client::InvalidateSession(kSessionId);
        EXPECT_EQ(Client::GetPendingTokenCount(), 0u);
        EXPECT_FALSE(Client::WaitResponseForToken(kClearToken, 100));

        // Destroy the session; this wait reads the stale clear response first
        // and discards it, then consumes the destroy response.
        EXPECT_TRUE(Client::SubmitSessionControl(kSessionId, false, kDestroyToken));
        EXPECT_TRUE(Client::WaitResponseForToken(kDestroyToken, 5000));

        serverThread.join();
        EXPECT_TRUE(serverOk);
        EXPECT_EQ(Client::GetPendingTokenCount(), 0u);

        core.Shutdown();
        Client::Shutdown();
        DestroyInProcessTransport(server);
        DestroyInProcessTransport(client);
    }

    TEST(ClientTokenSessionTest, TimeoutInvalidatesPendingToken) {
        constexpr uint64_t kSessionId = 88;

        MobileGLBackend backend{};
        const MobileGLBackendVTable vtable = MakeVTable();

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

        // No server thread: WaitResponseForToken must time out and drop the
        // pending token instead of blocking forever or leaving a zombie entry.
        EXPECT_TRUE(Client::SubmitDataCommand(
            static_cast<uint32_t>(kSessionId),
            static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glClear),
            10, 0, 0, nullptr));
        EXPECT_FALSE(Client::WaitResponseForToken(10, 50));
        EXPECT_EQ(Client::GetPendingTokenCount(), 0u);

        core.Shutdown();
        Client::Shutdown();
        DestroyInProcessTransport(server);
        DestroyInProcessTransport(client);
    }
} // namespace MobileGL::Transport

// End of File
