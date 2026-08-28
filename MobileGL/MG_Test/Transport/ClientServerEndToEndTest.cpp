// MobileGL - MobileGL/MG_Test/Transport/ClientServerEndToEndTest.cpp
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

// This test defines the opaque backend object locally; it is not linked with
// any BackendObject plugin .so, so completing the type here is safe.
struct MobileGLBackend {
    uint32_t Nonce = 0;
};

namespace {
    uint32_t g_clientServerClearCount = 0;

    void TestClear(MobileGLBackend* backend, MobileGLSessionId session, uint32_t mask) {
        (void)backend;
        (void)session;
        (void)mask;
        ++g_clientServerClearCount;
    }
} // namespace

namespace MobileGL::Transport {
    TEST(ClientServerEndToEndTest, ClientCommandRoundTrip) {
        g_clientServerClearCount = 0;

        MobileGLBackend backend{};
        MobileGLBackendVTable vtable{};
        vtable.structSize = sizeof(MobileGLBackendVTable);
        vtable.apiVersion = (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR;
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

        // Serve the command concurrently: Client::SendCommand waits for its
        // response, so the server has to process while the client is blocked.
        Bool serverOk = false;
        std::thread serverThread([&] {
            serverOk = core.ServiceOnce();
        });
        const Bool sent = Client::SendCommand(
            42, static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glClear));
        serverThread.join();

        EXPECT_TRUE(serverOk);
        EXPECT_TRUE(sent) << Client::GetLastError();

        core.Shutdown();
        Client::Shutdown();
        DestroyInProcessTransport(server);
        EXPECT_EQ(g_clientServerClearCount, 1u);
    }
} // namespace MobileGL::Transport

// End of File
