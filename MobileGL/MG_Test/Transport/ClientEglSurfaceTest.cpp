// MobileGL - MobileGL/MG_Test/Transport/ClientEglSurfaceTest.cpp
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
    MobileGLDisplayId g_display = 0;
    MobileGLBackendHandle g_surface = 0;
    int32_t g_width = 0;
    int32_t g_height = 0;
    bool g_destroyed = false;

    bool OnDisplayCreated(MobileGLBackend* backend, MobileGLDisplayId display, const void* nativeDisplay) {
        (void)backend;
        (void)display;
        (void)nativeDisplay;
        return true;
    }

    void OnDisplayDestroyed(MobileGLBackend* backend, MobileGLDisplayId display) {
        (void)backend;
        (void)display;
    }

    bool TestCreatePbufferSurface(MobileGLBackend* backend, MobileGLDisplayId display,
                                  MobileGLBackendHandle surface, int32_t width, int32_t height) {
        (void)backend;
        g_display = display;
        g_surface = surface;
        g_width = width;
        g_height = height;
        return true;
    }

    void TestReleaseEGLSurface(MobileGLBackend* backend, MobileGLDisplayId display,
                               MobileGLBackendHandle surface) {
        (void)backend;
        (void)display;
        (void)surface;
        g_destroyed = true;
    }
} // namespace

namespace MobileGL::Transport {
    TEST(ClientEglSurfaceTest, PbufferSurfaceLifecycle) {
        g_display = 0;
        g_surface = 0;
        g_width = 0;
        g_height = 0;
        g_destroyed = false;

        const char* endpoint = "/tmp/mobilegl_egl_surface_test.sock";
        unlink(endpoint);

        MobileGLBackend backend{};
        MobileGLBackendVTable vtable{};
        vtable.structSize = sizeof(MobileGLBackendVTable);
        vtable.apiVersion = (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR;
        vtable.OnDisplayCreated = &OnDisplayCreated;
        vtable.OnDisplayDestroyed = &OnDisplayDestroyed;
        vtable.CreatePbufferSurface = &TestCreatePbufferSurface;
        vtable.ReleaseEGLSurface = &TestReleaseEGLSurface;

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

        ASSERT_TRUE(Client::SubmitDisplayControl(50, true, 1));
        EXPECT_TRUE(Client::WaitResponseForToken(1, 5000));
        ASSERT_TRUE(Client::SendEglCreatePbufferSurface(50, 70, 64, 48, 2));
        ASSERT_TRUE(Client::SendEglDestroySurface(50, 70, 3));
        serverThread.join();

        EXPECT_TRUE(serverOk);
        EXPECT_EQ(g_display, 50u);
        EXPECT_EQ(g_surface, 70u);
        EXPECT_EQ(g_width, 64);
        EXPECT_EQ(g_height, 48);
        EXPECT_TRUE(g_destroyed);

        core.Shutdown();
        Client::Shutdown();

        DestroyLocalSocketShmTransport(accepted);
        DestroyLocalSocketShmTransport(client);
        DestroyLocalSocketShmTransport(server);
        unlink(endpoint);
    }
} // namespace MobileGL::Transport

// End of File
