// MobileGL - MobileGL/MG_Test/Transport/ClientSessionLifecycleTest.cpp
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
    uint32_t g_createdSessions = 0;
    uint32_t g_destroyedSessions = 0;
    const MobileGLSessionId kTrackedSession = 123;

    bool OnCreated(MobileGLBackend* backend, MobileGLSessionId session,
                   const MobileGLBackendInitInfo* info) {
        (void)backend;
        (void)info;
        if (session == kTrackedSession) ++g_createdSessions;
        return true;
    }

    void OnDestroyed(MobileGLBackend* backend, MobileGLSessionId session) {
        (void)backend;
        if (session == kTrackedSession) ++g_destroyedSessions;
    }
} // namespace

namespace MobileGL::Transport {
    TEST(ClientSessionLifecycleTest, CreateAndDestroySession) {
        g_createdSessions = 0;
        g_destroyedSessions = 0;

        MobileGLBackend backend{};
        MobileGLBackendVTable vtable{};
        vtable.structSize = sizeof(MobileGLBackendVTable);
        vtable.apiVersion = (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR;
        vtable.OnSessionCreated = &OnCreated;
        vtable.OnSessionDestroyed = &OnDestroyed;

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

        EXPECT_TRUE(Client::SubmitSessionControl(kTrackedSession, true, 1));
        EXPECT_TRUE(Client::WaitResponseForToken(1, 5000));
        EXPECT_TRUE(Client::SubmitSessionControl(kTrackedSession, false, 2));
        EXPECT_TRUE(Client::WaitResponseForToken(2, 5000));
        // glClear on a destroyed session must be rejected.
        EXPECT_FALSE(Client::SendCommand(
            static_cast<Uint32>(kTrackedSession),
            static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glClear), 3));

        serverThread.join();
        EXPECT_TRUE(serverOk);
        EXPECT_EQ(g_createdSessions, 1u);
        EXPECT_EQ(g_destroyedSessions, 1u);

        core.Shutdown();
        Client::Shutdown();
        DestroyInProcessTransport(server);
    }
} // namespace MobileGL::Transport

// End of File
