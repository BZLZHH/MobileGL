// MobileGL - MobileGL/MG_Test/Transport/ClientTextureSubImageTest.cpp
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
    uint64_t g_texture = 0;
    uint32_t g_level = 0;
    uint32_t g_format = 0;
    uint32_t g_type = 0;
    uint32_t g_width = 0;
    uint32_t g_height = 0;
    uint32_t g_depth = 0;
    uint64_t g_dataSize = 0;
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

    void TestTextureSubImage(MobileGLBackend* backend, MobileGLSessionId session,
                             MobileGLBackendHandle texture, const MobileGLTextureUpload* upload) {
        (void)backend;
        (void)session;
        g_texture = texture;
        if (upload == nullptr) {
            return;
        }
        g_level = upload->level;
        g_format = upload->format;
        g_type = upload->type;
        g_width = upload->width;
        g_height = upload->height;
        g_depth = upload->depth;
        g_dataSize = upload->dataSize;
        g_firstDataByte = upload->data == nullptr ? 0 : *static_cast<const uint8_t*>(upload->data);
    }
} // namespace

namespace MobileGL::Transport {
    TEST(ClientTextureSubImageTest, ShmPixelsReachBackend) {
        g_texture = 0;
        g_level = 0;
        g_format = 0;
        g_type = 0;
        g_width = 0;
        g_height = 0;
        g_depth = 0;
        g_dataSize = 0;
        g_firstDataByte = 0;

        const char* endpoint = "/tmp/mobilegl_texsubimg_test.sock";
        unlink(endpoint);

        MobileGLBackend backend{};
        MobileGLBackendVTable vtable{};
        vtable.structSize = sizeof(MobileGLBackendVTable);
        vtable.apiVersion = (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR;
        vtable.OnSessionCreated = &OnSessionCreated;
        vtable.OnSessionDestroyed = &OnSessionDestroyed;
        vtable.TextureSubImage = &TestTextureSubImage;

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
        data[0] = 0x55;
        data[1] = 0x66;

        Bool serverOk = false;
        std::thread serverThread([&] {
            for (int i = 0; i < 2; ++i) {
                if (!core.ServiceOnce()) return;
            }
            serverOk = true;
        });

        ASSERT_TRUE(Client::SubmitSessionControl(10, true, 1));
        EXPECT_TRUE(Client::WaitResponseForToken(1, 5000));
        ASSERT_TRUE(Client::SendTextureSubImage(10, 111, 3, 0x8058, 0x1401, 8, 6, 1, 2, &shm, 2));
        serverThread.join();

        EXPECT_TRUE(serverOk);
        EXPECT_EQ(g_texture, 111u);
        EXPECT_EQ(g_level, 3u);
        EXPECT_EQ(g_format, 0x8058u);
        EXPECT_EQ(g_type, 0x1401u);
        EXPECT_EQ(g_width, 8u);
        EXPECT_EQ(g_height, 6u);
        EXPECT_EQ(g_depth, 1u);
        EXPECT_EQ(g_dataSize, 2u);
        EXPECT_EQ(g_firstDataByte, 0x55u);

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
