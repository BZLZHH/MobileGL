// MobileGL - MobileGL/MG_Test/Transport/ClientIndirectDrawsTest.cpp
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
    uint32_t g_mode = 0;
    uint32_t g_type = 0;
    uint8_t g_firstIndirectByte = 0;

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

    void TestDrawArraysIndirect(MobileGLBackend* backend, MobileGLSessionId session, uint32_t mode,
                                const void* indirect) {
        (void)backend;
        (void)session;
        g_mode = mode;
        g_firstIndirectByte = indirect == nullptr ? 0 : *static_cast<const uint8_t*>(indirect);
    }

    void TestDrawElementsIndirect(MobileGLBackend* backend, MobileGLSessionId session, uint32_t mode, uint32_t type,
                                  const void* indirect) {
        (void)backend;
        (void)session;
        g_mode = mode;
        g_type = type;
        g_firstIndirectByte = indirect == nullptr ? 0 : *static_cast<const uint8_t*>(indirect);
    }
} // namespace

namespace MobileGL::Transport {
    TEST(ClientIndirectDrawsTest, DrawArraysIndirectShmReachesBackend) {
        g_mode = 0;
        g_firstIndirectByte = 0;

        const char* endpoint = "/tmp/mobilegl_dai_test.sock";
        unlink(endpoint);

        MobileGLBackend backend{};
        MobileGLBackendVTable vtable{};
        vtable.structSize = sizeof(MobileGLBackendVTable);
        vtable.apiVersion = (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR;
        vtable.OnSessionCreated = &OnSessionCreated;
        vtable.OnSessionDestroyed = &OnSessionDestroyed;
        vtable.DrawArraysIndirect = &TestDrawArraysIndirect;

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
        data[0] = 0x11;
        data[1] = 0x22;
        data[2] = 0x33;

        Bool serverOk = false;
        std::thread serverThread([&] {
            for (int i = 0; i < 2; ++i) {
                if (!core.ServiceOnce()) return;
            }
            serverOk = true;
        });

        ASSERT_TRUE(Client::SubmitSessionControl(10, true, 1));
        EXPECT_TRUE(Client::WaitResponseForToken(1, 5000));
        ASSERT_TRUE(Client::SendDrawArraysIndirect(10, 4, 1, &shm, 2));
        serverThread.join();

        EXPECT_TRUE(serverOk);
        EXPECT_EQ(g_mode, 4u);
        EXPECT_EQ(g_firstIndirectByte, 0x22u);

        ops.ReleaseSharedMemory(client, &shm);
        core.Shutdown();
        Client::Shutdown();

        DestroyLocalSocketShmTransport(accepted);
        DestroyLocalSocketShmTransport(client);
        DestroyLocalSocketShmTransport(server);
        unlink(endpoint);
    }

    TEST(ClientIndirectDrawsTest, DrawElementsIndirectShmReachesBackend) {
        g_mode = 0;
        g_type = 0;
        g_firstIndirectByte = 0;

        const char* endpoint = "/tmp/mobilegl_dei_test.sock";
        unlink(endpoint);

        MobileGLBackend backend{};
        MobileGLBackendVTable vtable{};
        vtable.structSize = sizeof(MobileGLBackendVTable);
        vtable.apiVersion = (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR;
        vtable.OnSessionCreated = &OnSessionCreated;
        vtable.OnSessionDestroyed = &OnSessionDestroyed;
        vtable.DrawElementsIndirect = &TestDrawElementsIndirect;

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
        data[2] = 0x66;

        Bool serverOk = false;
        std::thread serverThread([&] {
            for (int i = 0; i < 2; ++i) {
                if (!core.ServiceOnce()) return;
            }
            serverOk = true;
        });

        ASSERT_TRUE(Client::SubmitSessionControl(10, true, 1));
        EXPECT_TRUE(Client::WaitResponseForToken(1, 5000));
        ASSERT_TRUE(Client::SendDrawElementsIndirect(10, 4, 0x1405, 2, &shm, 2));
        serverThread.join();

        EXPECT_TRUE(serverOk);
        EXPECT_EQ(g_mode, 4u);
        EXPECT_EQ(g_type, 0x1405u);
        EXPECT_EQ(g_firstIndirectByte, 0x66u);

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
