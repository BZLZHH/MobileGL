// MobileGL - MobileGL/MG_Test/Transport/BigServerE2ETest.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include <Includes.h>
#include <gtest/gtest.h>
#include "MG_FullServer/ServerCore.h"
#include "MG_Protocol/generated_opcodes.h"
#include "MG_Protocol/transport.h"
#include "MG_Transport/InProcessTransport.h"
#include "MG_Transport/LocalSocketShmTransport.h"

#if defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__)
#include <unistd.h>
#endif

// This test defines the opaque backend object locally; it is not linked with
// any BackendObject plugin .so, so completing the type here is safe.
struct MobileGLBackend {
    uint32_t Nonce = 0;
};

namespace {
    uint32_t g_clearCount = 0;
    uint32_t g_socketClearCount = 0;

    void TestClear(MobileGLBackend* backend, MobileGLSessionId session, uint32_t mask) {
        (void)backend;
        (void)session;
        (void)mask;
        ++g_clearCount;
    }

    void TestClearSocket(MobileGLBackend* backend, MobileGLSessionId session, uint32_t mask) {
        (void)backend;
        (void)session;
        (void)mask;
        ++g_socketClearCount;
    }
} // namespace

namespace MobileGL::Transport {
    TEST(BigServerE2ETest, ClientCommandReachesBackendVTable) {
        g_clearCount = 0;

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
        EXPECT_TRUE(ops.Start(client, &config));
        EXPECT_TRUE(ops.Start(server, &config));

        MobileGL::FullServer::ServerCore core(&ops, server, &backend, &vtable);
        ASSERT_TRUE(core.Start());

        MobileGL::FullServer::CommandHeader command{};
        command.Opcode = static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glClear);
        command.SessionId = 42;
        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = &command;
        batch.flatBufferSize = static_cast<uint32_t>(sizeof(command));
        ASSERT_TRUE(ops.SubmitCommands(client, &batch));

        ASSERT_TRUE(core.ServiceOnce());

        MobileGLResponseQueue response{};
        ASSERT_TRUE(ops.WaitResponses(client, &response, 0));
        ASSERT_GE(response.flatBufferSize, sizeof(MobileGL::FullServer::ResponseHeader));

        MobileGL::FullServer::ResponseHeader header{};
        memcpy(&header, response.flatBufferData, sizeof(header));
        EXPECT_EQ(header.Status, 0u);
        EXPECT_EQ(g_clearCount, 1u);

        core.Shutdown();
        DestroyInProcessTransport(client);
        DestroyInProcessTransport(server);
    }

    TEST(BigServerE2ETest, LocalSocketShmServerCoreE2E) {
        g_socketClearCount = 0;

        const char* endpoint = "/tmp/mobilegl_bigserver_e2e.sock";
        unlink(endpoint);

        MobileGLBackend backend{};
        MobileGLBackendVTable vtable{};
        vtable.structSize = sizeof(MobileGLBackendVTable);
        vtable.apiVersion = (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR;
        vtable.Clear = &TestClearSocket;

        MobileGLTransport* server = CreateLocalSocketShmServer(endpoint);
        ASSERT_NE(server, nullptr);

        Bool clientOk = false;
        std::thread clientThread([&] {
            MobileGLTransport* client = CreateLocalSocketShmTransport();
            ASSERT_NE(client, nullptr);
            const MobileGLTransportOps& ops = GetLocalSocketShmTransportOps();

            MobileGLTransportConfig config{};
            config.structSize = sizeof(MobileGLTransportConfig);
            config.kind = MobileGLTransportKindLocalSocketShm;
            config.endpoint = endpoint;
            config.timeoutMs = 0;
            if (!ops.Start(client, &config)) {
                DestroyLocalSocketShmTransport(client);
                return;
            }

            MobileGL::FullServer::CommandHeader command{};
            command.Opcode = static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glClear);
            command.SessionId = 7;
            MobileGLCommandBatch batch{};
            batch.structSize = sizeof(MobileGLCommandBatch);
            batch.flatBufferData = &command;
            batch.flatBufferSize = static_cast<uint32_t>(sizeof(command));
            if (!ops.SubmitCommands(client, &batch)) {
                DestroyLocalSocketShmTransport(client);
                return;
            }

            MobileGLResponseQueue response{};
            if (ops.WaitResponses(client, &response, 0) && response.flatBufferSize >=
                                                              sizeof(MobileGL::FullServer::ResponseHeader)) {
                MobileGL::FullServer::ResponseHeader header{};
                memcpy(&header, response.flatBufferData, sizeof(header));
                if (header.Status == 0) {
                    clientOk = true;
                }
            }
            DestroyLocalSocketShmTransport(client);
        });

        MobileGLTransport* accepted = AcceptLocalSocketShmConnection(server);
        ASSERT_NE(accepted, nullptr);

        MobileGL::FullServer::ServerCore core(&GetLocalSocketShmTransportOps(), accepted,
                                              &backend, &vtable);
        ASSERT_TRUE(core.Start());
        ASSERT_TRUE(core.ServiceOnce());
        core.Shutdown();

        clientThread.join();
        EXPECT_TRUE(clientOk);
        EXPECT_EQ(g_socketClearCount, 1u);

        DestroyLocalSocketShmTransport(accepted);
        DestroyLocalSocketShmTransport(server);
        unlink(endpoint);
    }
} // namespace MobileGL::Transport

// End of File
