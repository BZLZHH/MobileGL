// MobileGL - MobileGL/MG_Test/Transport/InProcessTransportTest.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include <Includes.h>
#include <gtest/gtest.h>
#include "MG_Protocol/transport.h"
#include "MG_Transport/InProcessTransport.h"
#include "MG_Transport/LocalSocketShmTransport.h"

#if defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__)
#include <unistd.h>
#endif

namespace MobileGL::Transport {
    TEST(InProcessTransportTest, ClientServerExchange) {
        MobileGLTransport* client = nullptr;
        MobileGLTransport* server = nullptr;
        CreateInProcessTransportPair(&client, &server);
        ASSERT_NE(client, nullptr);
        ASSERT_NE(server, nullptr);

        const MobileGLTransportOps& ops = GetInProcessTransportOps();

        MobileGLTransportConfig config{};
        config.structSize = sizeof(MobileGLTransportConfig);
        config.kind = MobileGLTransportKindInProcess;
        config.endpoint = "in-process";
        config.timeoutMs = 0;
        EXPECT_TRUE(ops.Start(client, &config));
        EXPECT_TRUE(ops.Start(server, &config));

        const char command[] = "hello";
        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = command;
        batch.flatBufferSize = static_cast<uint32_t>(sizeof(command) - 1);
        ASSERT_TRUE(ops.SubmitCommands(client, &batch));

        MobileGLResponseQueue received{};
        ASSERT_TRUE(ops.WaitResponses(server, &received, 0));
        ASSERT_EQ(received.count, 1u);
        ASSERT_EQ(received.flatBufferSize, 5u);
        EXPECT_EQ(memcmp(received.flatBufferData, command, 5), 0);

        const char response[] = "world";
        MobileGLCommandBatch responseBatch{};
        responseBatch.structSize = sizeof(MobileGLCommandBatch);
        responseBatch.flatBufferData = response;
        responseBatch.flatBufferSize = static_cast<uint32_t>(sizeof(response) - 1);
        ASSERT_TRUE(ops.SubmitCommands(server, &responseBatch));

        MobileGLResponseQueue clientReceived{};
        ASSERT_TRUE(ops.WaitResponses(client, &clientReceived, 0));
        ASSERT_EQ(clientReceived.count, 1u);
        ASSERT_EQ(clientReceived.flatBufferSize, 5u);
        EXPECT_EQ(memcmp(clientReceived.flatBufferData, response, 5), 0);

        DestroyInProcessTransport(client);
        DestroyInProcessTransport(server);
    }

    TEST(InProcessTransportTest, LocalSocketShmRoundTrip) {
        const char* endpoint = "/tmp/mobilegl_transport_test.sock";
        unlink(endpoint);

        MobileGLTransport* server = CreateLocalSocketShmServer(endpoint);
        ASSERT_NE(server, nullptr);

        Bool clientOk = false;
        Bool exchangeOk = false;
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
            clientOk = true;

            const char command[] = "hello";
            MobileGLCommandBatch batch{};
            batch.structSize = sizeof(MobileGLCommandBatch);
            batch.flatBufferData = command;
            batch.flatBufferSize = static_cast<uint32_t>(sizeof(command) - 1);
            if (!ops.SubmitCommands(client, &batch)) {
                DestroyLocalSocketShmTransport(client);
                return;
            }

            MobileGLResponseQueue response{};
            if (ops.WaitResponses(client, &response, 0) && response.count == 1 &&
                response.flatBufferSize == 5u && memcmp(response.flatBufferData, "world", 5) == 0) {
                exchangeOk = true;
            }
            DestroyLocalSocketShmTransport(client);
        });

        MobileGLTransport* accepted = AcceptLocalSocketShmConnection(server);
        ASSERT_NE(accepted, nullptr);
        const MobileGLTransportOps& serverOps = GetLocalSocketShmTransportOps();

        MobileGLResponseQueue command{};
        ASSERT_TRUE(serverOps.WaitResponses(accepted, &command, 0));
        ASSERT_EQ(command.count, 1u);
        ASSERT_EQ(command.flatBufferSize, 5u);
        EXPECT_EQ(memcmp(command.flatBufferData, "hello", 5), 0);

        const char response[] = "world";
        MobileGLCommandBatch responseBatch{};
        responseBatch.structSize = sizeof(MobileGLCommandBatch);
        responseBatch.flatBufferData = response;
        responseBatch.flatBufferSize = static_cast<uint32_t>(sizeof(response) - 1);
        ASSERT_TRUE(serverOps.SubmitCommands(accepted, &responseBatch));

        clientThread.join();
        EXPECT_TRUE(clientOk);
        EXPECT_TRUE(exchangeOk);

        DestroyLocalSocketShmTransport(accepted);
        DestroyLocalSocketShmTransport(server);
        unlink(endpoint);
    }

    TEST(InProcessTransportTest, LocalSocketShmAllocatesSharedMemory) {
        const char* endpoint = "/tmp/mobilegl_transport_shm_test.sock";
        unlink(endpoint);

        MobileGLTransport* server = CreateLocalSocketShmServer(endpoint);
        ASSERT_NE(server, nullptr);

        MobileGLTransport* client = CreateLocalSocketShmTransport();
        ASSERT_NE(client, nullptr);
        const MobileGLTransportOps& ops = GetLocalSocketShmTransportOps();

        MobileGLTransportConfig config{};
        config.structSize = sizeof(MobileGLTransportConfig);
        config.kind = MobileGLTransportKindLocalSocketShm;
        config.endpoint = endpoint;
        config.maxShmArenaSize = 1024 * 1024;
        config.timeoutMs = 0;
        ASSERT_TRUE(ops.Start(client, &config));

        MobileGLTransport* accepted = AcceptLocalSocketShmConnection(server);
        ASSERT_NE(accepted, nullptr);

        MobileGLShmHandle shm{};
        ASSERT_TRUE(ops.OpenSharedMemory(client, &shm));
        EXPECT_GT(shm.size, 0u);
        EXPECT_GT(shm.capacity, 0u);
        EXPECT_NE(shm.mappedAddress, nullptr);
        EXPECT_GT(shm.platformHandle, 0);

        auto* bytes = static_cast<Uint8*>(shm.mappedAddress);
        bytes[0] = 0xAB;
        EXPECT_EQ(bytes[0], 0xAB);

        ops.ReleaseSharedMemory(client, &shm);
        EXPECT_EQ(shm.mappedAddress, nullptr);

        DestroyLocalSocketShmTransport(accepted);
        DestroyLocalSocketShmTransport(client);
        DestroyLocalSocketShmTransport(server);
        unlink(endpoint);
    }
} // namespace MobileGL::Transport

// End of File
