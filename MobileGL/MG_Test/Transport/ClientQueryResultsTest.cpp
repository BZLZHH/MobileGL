// MobileGL - MobileGL/MG_Test/Transport/ClientQueryResultsTest.cpp
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
    uint64_t g_checkedQuery = 0;
    uint64_t g_resultQuery = 0;
    uint64_t g_resultWait = 0;
    uint64_t g_endedOcclusion = 0;
    uint64_t g_endedXfb = 0;
    uint32_t g_xfbGenerated = 0;

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

    bool TestIsQueryResultAvailable(MobileGLBackend* backend, MobileGLSessionId session,
                                    void* query) {
        (void)backend;
        (void)session;
        g_checkedQuery = reinterpret_cast<uint64_t>(query);
        return true;
    }

    bool TestGetQueryResult64(MobileGLBackend* backend, MobileGLSessionId session, void* query,
                              bool wait, uint64_t* outNanoseconds) {
        (void)backend;
        (void)session;
        g_resultQuery = reinterpret_cast<uint64_t>(query);
        g_resultWait = wait ? 1 : 0;
        *outNanoseconds = 0xABCDEF;
        return true;
    }

    void* TestBeginOcclusionQuery(MobileGLBackend* backend, MobileGLSessionId session) {
        (void)backend;
        (void)session;
        return reinterpret_cast<void*>(0x7778);
    }

    void TestEndOcclusionQuery(MobileGLBackend* backend, MobileGLSessionId session, void* query) {
        (void)backend;
        (void)session;
        g_endedOcclusion = reinterpret_cast<uint64_t>(query);
    }

    void* TestBeginXfbPrimitivesQuery(MobileGLBackend* backend, MobileGLSessionId session,
                                      bool generated) {
        (void)backend;
        (void)session;
        g_xfbGenerated = generated ? 1 : 0;
        return reinterpret_cast<void*>(0x7779);
    }

    void TestEndXfbPrimitivesQuery(MobileGLBackend* backend, MobileGLSessionId session,
                                   void* query) {
        (void)backend;
        (void)session;
        g_endedXfb = reinterpret_cast<uint64_t>(query);
    }
} // namespace

namespace MobileGL::Transport {
    TEST(ClientQueryResultsTest, ResultOcclusionXfbLifecycle) {
        g_checkedQuery = 0;
        g_resultQuery = 0;
        g_resultWait = 0;
        g_endedOcclusion = 0;
        g_endedXfb = 0;
        g_xfbGenerated = 0;

        MobileGLBackend backend{};
        MobileGLBackendVTable vtable{};
        vtable.structSize = sizeof(MobileGLBackendVTable);
        vtable.apiVersion = (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR;
        vtable.OnSessionCreated = &OnSessionCreated;
        vtable.OnSessionDestroyed = &OnSessionDestroyed;
        vtable.IsQueryResultAvailable = &TestIsQueryResultAvailable;
        vtable.GetQueryResult64 = &TestGetQueryResult64;
        vtable.BeginOcclusionQuery = &TestBeginOcclusionQuery;
        vtable.EndOcclusionQuery = &TestEndOcclusionQuery;
        vtable.BeginXfbPrimitivesQuery = &TestBeginXfbPrimitivesQuery;
        vtable.EndXfbPrimitivesQuery = &TestEndXfbPrimitivesQuery;

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
            for (int i = 0; i < 7; ++i) {
                if (!core.ServiceOnce()) return;
            }
            serverOk = true;
        });

        EXPECT_TRUE(Client::SubmitSessionControl(32, true, 1));
        EXPECT_TRUE(Client::WaitResponseForToken(1, 5000));

        Bool available = false;
        EXPECT_TRUE(Client::SendIsQueryResultAvailable(32, 0x100, 2, &available));
        EXPECT_TRUE(available);
        EXPECT_EQ(g_checkedQuery, 0x100u);

        Bool available64 = false;
        uint64_t ns = 0;
        EXPECT_TRUE(Client::SendGetQueryResult64(32, 0x200, 1, 3, &available64, &ns));
        EXPECT_TRUE(available64);
        EXPECT_EQ(g_resultQuery, 0x200u);
        EXPECT_EQ(g_resultWait, 1u);
        EXPECT_EQ(ns, 0xABCDEFu);

        uint64_t occlusion = 0;
        EXPECT_TRUE(Client::SendBeginOcclusionQuery(32, 4, &occlusion));
        EXPECT_EQ(occlusion, 0x7778u);
        EXPECT_TRUE(Client::SendEndOcclusionQuery(32, occlusion, 5));
        EXPECT_EQ(g_endedOcclusion, 0x7778u);

        uint64_t xfb = 0;
        EXPECT_TRUE(Client::SendBeginXfbPrimitivesQuery(32, 1, 6, &xfb));
        EXPECT_EQ(xfb, 0x7779u);
        EXPECT_EQ(g_xfbGenerated, 1u);
        EXPECT_TRUE(Client::SendEndXfbPrimitivesQuery(32, xfb, 7));
        EXPECT_EQ(g_endedXfb, 0x7779u);

        serverThread.join();
        EXPECT_TRUE(serverOk);

        core.Shutdown();
        Client::Shutdown();
        DestroyInProcessTransport(server);
    }
} // namespace MobileGL::Transport

// End of File
