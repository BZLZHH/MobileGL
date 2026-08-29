// MobileGL - MobileGL/MG_Test/Transport/ClientBufferRespecifyTest.cpp
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
    uint64_t g_handle = 0;
    uint64_t g_size = 0;
    uint32_t g_usage = 0;
    uint8_t g_firstDataByte = 0;

    bool OnSessionCreated(MobileGLBackend* backend, MobileGLSessionId session, const MobileGLBackendInitInfo* info) {
        (void)backend;
        (void)session;
        (void)info;
        return true;
    }

    void OnSessionDestroyed(MobileGLBackend* backend, MobileGLSessionId session) {
        (void)backend;
        (void)session;
    }

    void TestBufferRespecify(MobileGLBackend* backend, MobileGLSessionId session, MobileGLBackendHandle buffer,
                             uint64_t size, uint32_t usage, const MobileGLBufferOps* ops) {
        (void)backend;
        (void)session;
        g_handle = buffer;
        g_size = size;
        g_usage = usage;
        g_firstDataByte =
            ops == nullptr || ops->initialData == nullptr ? 0 : *static_cast<const uint8_t*>(ops->initialData);
    }
} // namespace

namespace MobileGL::Transport {
    TEST(ClientBufferRespecifyTest, ShmInitialDataReachesBackend) {
        g_handle = 0;
        g_size = 0;
        g_usage = 0;
        g_firstDataByte = 0;

        const char* endpoint = "/tmp/mobilegl_bufrespec_test.sock";
        unlink(endpoint);

        MobileGLBackend backend{};
        MobileGLBackendVTable vtable{};
        vtable.structSize = sizeof(MobileGLBackendVTable);
        vtable.apiVersion = (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR;
        vtable.OnSessionCreated = &OnSessionCreated;
        vtable.OnSessionDestroyed = &OnSessionDestroyed;
        vtable.BufferRespecify = &TestBufferRespecify;

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
        data[0] = 0xAB;
        data[1] = 0xCD;

        Bool serverOk = false;
        std::thread serverThread([&] {
            for (int i = 0; i < 2; ++i) {
                if (!core.ServiceOnce()) return;
            }
            serverOk = true;
        });

        ASSERT_TRUE(Client::SubmitSessionControl(10, true, 1));
        EXPECT_TRUE(Client::WaitResponseForToken(1, 5000));
        ASSERT_TRUE(Client::SendBufferRespecify(10, 456, 4, 0x88E4, &shm, 2));
        serverThread.join();

        EXPECT_TRUE(serverOk);
        EXPECT_EQ(g_handle, 456u);
        EXPECT_EQ(g_size, 4u);
        EXPECT_EQ(g_usage, 0x88E4u);
        EXPECT_EQ(g_firstDataByte, 0xABu);

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
