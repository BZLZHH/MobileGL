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
} // namespace MobileGL::Transport

// End of File
