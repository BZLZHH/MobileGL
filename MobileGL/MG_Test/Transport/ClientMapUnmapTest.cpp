// MobileGL - MobileGL/MG_Test/Transport/ClientMapUnmapTest.cpp
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
#include "MG_Transport/LocalSocketShmTransport.h"

#include <unistd.h>

struct MobileGLBackend {
    uint32_t Nonce = 0;
};

namespace {
    uint64_t g_subHandle = 0;
    uint64_t g_subOffset = 0;
    uint64_t g_subSize = 0;
    uint8_t g_subData[4] = {};

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

    bool TestBufferReadback(MobileGLBackend* backend, MobileGLSessionId session,
                            MobileGLBackendHandle buffer, uint64_t offset, uint64_t size,
                            void* dst) {
        (void)backend;
        (void)session;
        (void)buffer;
        (void)offset;
        if (dst == nullptr || size < 4) {
            return false;
        }
        auto* bytes = static_cast<uint8_t*>(dst);
        bytes[0] = 0x10;
        bytes[1] = 0x20;
        bytes[2] = 0x30;
        bytes[3] = 0x40;
        return true;
    }

    void TestBufferSubData(MobileGLBackend* backend, MobileGLSessionId session,
                           MobileGLBackendHandle buffer, uint64_t offset, uint64_t size,
                           const void* data) {
        (void)backend;
        (void)session;
        g_subHandle = buffer;
        g_subOffset = offset;
        g_subSize = size;
        if (data != nullptr && size >= 4) {
            const auto* bytes = static_cast<const uint8_t*>(data);
            for (uint32_t i = 0; i < 4; ++i) {
                g_subData[i] = bytes[i];
            }
        }
    }
} // namespace

namespace MobileGL::Transport {
    TEST(ClientMapUnmapTest, MapThenUnmapFlushesWrites) {
        g_subHandle = 0;
        g_subOffset = 0;
        g_subSize = 0;
        for (uint8_t& byte : g_subData) {
            byte = 0;
        }

        const char* endpoint = "/tmp/mobilegl_map_unmap_test.sock";
        unlink(endpoint);

        MobileGLBackend backend{};
        MobileGLBackendVTable vtable{};
        vtable.structSize = sizeof(MobileGLBackendVTable);
        vtable.apiVersion = (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR;
        vtable.OnSessionCreated = &OnSessionCreated;
        vtable.OnSessionDestroyed = &OnSessionDestroyed;
        vtable.BufferReadbackFromGpu = &TestBufferReadback;
        vtable.BufferSubData = &TestBufferSubData;

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
        ASSERT_TRUE(ops.Start(client, &config));

        MobileGLTransport* accepted = AcceptLocalSocketShmConnection(server);
        ASSERT_NE(accepted, nullptr);

        MobileGL::FullServer::ServerCore core(&ops, accepted, &backend, &vtable);
        ASSERT_TRUE(core.Start());
        ASSERT_TRUE(Client::InitializeWithTransport(client, &ops));

        Bool serverOk = false;
        std::thread serverThread([&] {
            for (int i = 0; i < 3; ++i) {
                if (!core.ServiceOnce()) return;
            }
            serverOk = true;
        });

        ASSERT_TRUE(Client::SubmitSessionControl(10, true, 1));
        EXPECT_TRUE(Client::WaitResponseForToken(1, 5000));

        void* mapped = nullptr;
        ASSERT_TRUE(Client::MapBufferRange(10, 77, 0, 4, 0x1, 2, &mapped));
        ASSERT_NE(mapped, nullptr);
        const auto* mappedBytes = static_cast<const uint8_t*>(mapped);
        EXPECT_EQ(mappedBytes[0], 0x10u);
        EXPECT_EQ(mappedBytes[1], 0x20u);
        EXPECT_EQ(mappedBytes[2], 0x30u);
        EXPECT_EQ(mappedBytes[3], 0x40u);

        static_cast<uint8_t*>(mapped)[2] = 0x99;
        ASSERT_TRUE(Client::UnmapBuffer(10, 3));
        serverThread.join();

        EXPECT_TRUE(serverOk);
        EXPECT_EQ(g_subHandle, 77u);
        EXPECT_EQ(g_subOffset, 0u);
        EXPECT_EQ(g_subSize, 4u);
        EXPECT_EQ(g_subData[0], 0x10u);
        EXPECT_EQ(g_subData[1], 0x20u);
        EXPECT_EQ(g_subData[2], 0x99u);
        EXPECT_EQ(g_subData[3], 0x40u);

        core.Shutdown();
        Client::Shutdown();

        DestroyLocalSocketShmTransport(accepted);
        DestroyLocalSocketShmTransport(client);
        DestroyLocalSocketShmTransport(server);
        unlink(endpoint);
    }
} // namespace MobileGL::Transport

// End of File
