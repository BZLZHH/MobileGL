// MobileGL - MobileGL/MG_Test/Transport/ClientBufferSubDataShmTest.cpp
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
    uint64_t g_handle = 0, g_offset = 0, g_size = 0;
    uint8_t g_firstDataByte = 0;

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

    void TestBufferSubData(MobileGLBackend* backend, MobileGLSessionId session,
                           MobileGLBackendHandle buffer, uint64_t offset, uint64_t size,
                           const void* data) {
        (void)backend;
        (void)session;
        g_handle = buffer;
        g_offset = offset;
        g_size = size;
        g_firstDataByte = data == nullptr ? 0 : *static_cast<const uint8_t*>(data);
    }
} // namespace

namespace MobileGL::Transport {
    TEST(ClientBufferSubDataShmTest, ShmDataReachesBackend) {
        g_handle = 0;
        g_offset = 0;
        g_size = 0;
        g_firstDataByte = 0;

        const char* endpoint = "/tmp/mobilegl_bufsub_test.sock";
        unlink(endpoint);

        MobileGLBackend backend{};
        MobileGLBackendVTable vtable{};
        vtable.structSize = sizeof(MobileGLBackendVTable);
        vtable.apiVersion = (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR;
        vtable.OnSessionCreated = &OnSessionCreated;
        vtable.OnSessionDestroyed = &OnSessionDestroyed;
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

        MobileGLShmHandle shm{};
        ASSERT_TRUE(ops.OpenSharedMemory(client, &shm));
        auto* data = static_cast<uint8_t*>(shm.mappedAddress);
        data[0] = 0x44;
        data[1] = 0x55;

        Bool serverOk = false;
        std::thread serverThread([&] {
            for (int i = 0; i < 2; ++i) {
                if (!core.ServiceOnce()) return;
            }
            serverOk = true;
        });

        ASSERT_TRUE(Client::SubmitSessionControl(10, true, 1));
        EXPECT_TRUE(Client::WaitResponseForToken(1, 5000));
        ASSERT_TRUE(Client::SendBufferSubData(10, 123, 64, 2, &shm, 2));
        serverThread.join();

        EXPECT_TRUE(serverOk);
        EXPECT_EQ(g_handle, 123u);
        EXPECT_EQ(g_offset, 64u);
        EXPECT_EQ(g_size, 2u);
        EXPECT_EQ(g_firstDataByte, 0x44u);

        ops.ReleaseSharedMemory(client, &shm);
        core.Shutdown();
        Client::Shutdown();

        DestroyLocalSocketShmTransport(accepted);
        DestroyLocalSocketShmTransport(client);
        DestroyLocalSocketShmTransport(server);
        unlink(endpoint);
    }
} // namespace MobileGL::Transport

// End of File
