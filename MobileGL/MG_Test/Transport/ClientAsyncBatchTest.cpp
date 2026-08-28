// MobileGL - MobileGL/MG_Test/Transport/ClientAsyncBatchTest.cpp
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
#include "MG_Protocol/generated_opcodes.h"
#include "MG_Protocol/transport.h"
#include "MG_Transport/InProcessTransport.h"

struct MobileGLBackend {
    uint32_t Nonce = 0;
};

namespace {
    uint32_t g_asyncClearCount = 0;

    void TestClear(MobileGLBackend* backend, MobileGLSessionId session, uint32_t mask) {
        (void)backend;
        (void)session;
        (void)mask;
        ++g_asyncClearCount;
    }

    bool OnSessionCreated(MobileGLBackend* backend, MobileGLSessionId session,
                          const MobileGLBackendInitInfo* info) {
        (void)backend;
        (void)session;
        (void)info;
        return true;
    }
} // namespace

namespace MobileGL::Transport {
    TEST(ClientAsyncBatchTest, SubmitAllThenAwaitTokens) {
        constexpr Uint32 kCommandCount = 8;
        g_asyncClearCount = 0;

        MobileGLBackend backend{};
        MobileGLBackendVTable vtable{};
        vtable.structSize = sizeof(MobileGLBackendVTable);
        vtable.apiVersion = (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR;
        vtable.OnSessionCreated = &OnSessionCreated;
        vtable.Clear = &TestClear;

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
            for (Uint32 i = 0; i < kCommandCount + 1; ++i) {
                if (!core.ServiceOnce()) {
                    return;
                }
            }
            serverOk = true;
        });

        EXPECT_TRUE(Client::SubmitSessionControl(1, true, 99));
        EXPECT_TRUE(Client::WaitResponseForToken(99, 5000));
        for (Uint32 i = 0; i < kCommandCount; ++i) {
            ASSERT_TRUE(Client::SubmitCommand(
                1, static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glClear), i));
        }
        for (Uint32 i = 0; i < kCommandCount; ++i) {
            EXPECT_TRUE(Client::WaitResponseForToken(i, 5000)) << "token " << i;
        }

        serverThread.join();
        EXPECT_TRUE(serverOk);
        EXPECT_EQ(g_asyncClearCount, kCommandCount);

        core.Shutdown();
        Client::Shutdown();
        DestroyInProcessTransport(server);
    }
} // namespace MobileGL::Transport

// End of File
