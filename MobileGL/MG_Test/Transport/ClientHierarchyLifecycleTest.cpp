// MobileGL - MobileGL/MG_Test/Transport/ClientHierarchyLifecycleTest.cpp
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
    uint32_t g_displayCreated = 0;
    uint32_t g_displayDestroyed = 0;
    uint32_t g_groupCreated = 0;
    uint32_t g_groupDestroyed = 0;

    bool OnDisplayCreated(MobileGLBackend* backend, MobileGLDisplayId display,
                          const void* nativeDisplay) {
        (void)backend;
        (void)nativeDisplay;
        if (display == 7) ++g_displayCreated;
        return true;
    }

    void OnDisplayDestroyed(MobileGLBackend* backend, MobileGLDisplayId display) {
        (void)backend;
        if (display == 7) ++g_displayDestroyed;
    }

    bool OnGroupCreated(MobileGLBackend* backend, MobileGLSharedGroupId group,
                        const void* shareInfo) {
        (void)backend;
        (void)shareInfo;
        if (group == 8) ++g_groupCreated;
        return true;
    }

    void OnGroupDestroyed(MobileGLBackend* backend, MobileGLSharedGroupId group) {
        (void)backend;
        if (group == 8) ++g_groupDestroyed;
    }
} // namespace

namespace MobileGL::Transport {
    TEST(ClientHierarchyLifecycleTest, DisplayAndSharedGroupLifecycle) {
        g_displayCreated = 0;
        g_displayDestroyed = 0;
        g_groupCreated = 0;
        g_groupDestroyed = 0;

        MobileGLBackend backend{};
        MobileGLBackendVTable vtable{};
        vtable.structSize = sizeof(MobileGLBackendVTable);
        vtable.apiVersion = (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR;
        vtable.OnDisplayCreated = &OnDisplayCreated;
        vtable.OnDisplayDestroyed = &OnDisplayDestroyed;
        vtable.OnSharedGroupCreated = &OnGroupCreated;
        vtable.OnSharedGroupDestroyed = &OnGroupDestroyed;

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
            for (int i = 0; i < 4; ++i) {
                if (!core.ServiceOnce()) return;
            }
            serverOk = true;
        });

        EXPECT_TRUE(Client::SubmitDisplayControl(7, true, 1));
        EXPECT_TRUE(Client::WaitResponseForToken(1, 5000));
        EXPECT_TRUE(Client::SubmitDisplayControl(7, false, 2));
        EXPECT_TRUE(Client::WaitResponseForToken(2, 5000));
        EXPECT_TRUE(Client::SubmitSharedGroupControl(8, true, 3));
        EXPECT_TRUE(Client::WaitResponseForToken(3, 5000));
        EXPECT_TRUE(Client::SubmitSharedGroupControl(8, false, 4));
        EXPECT_TRUE(Client::WaitResponseForToken(4, 5000));

        serverThread.join();
        EXPECT_TRUE(serverOk);
        EXPECT_EQ(g_displayCreated, 1u);
        EXPECT_EQ(g_displayDestroyed, 1u);
        EXPECT_EQ(g_groupCreated, 1u);
        EXPECT_EQ(g_groupDestroyed, 1u);

        core.Shutdown();
        Client::Shutdown();
        DestroyInProcessTransport(server);
    }
} // namespace MobileGL::Transport

// End of File
