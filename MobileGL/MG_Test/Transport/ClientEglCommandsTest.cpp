// MobileGL - MobileGL/MG_Test/Transport/ClientEglCommandsTest.cpp
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
    MobileGLSessionId g_makeSession = 0;
    uint64_t g_draw = 0;
    uint64_t g_read = 0;
    int32_t g_interval = 0;
    MobileGLDisplayId g_resizeDisplay = 0;
    uint64_t g_resizeSurface = 0;
    uint32_t g_resizeWidth = 0;
    uint32_t g_resizeHeight = 0;

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

    bool TestMakeEGLCurrent(MobileGLBackend* backend, MobileGLSessionId session,
                            MobileGLBackendHandle draw, MobileGLBackendHandle read) {
        (void)backend;
        g_makeSession = session;
        g_draw = draw;
        g_read = read;
        return true;
    }

    void TestSetSwapInterval(MobileGLBackend* backend, MobileGLSessionId session, int32_t interval) {
        (void)backend;
        (void)session;
        g_interval = interval;
    }

    bool TestResizeSurface(MobileGLBackend* backend, MobileGLDisplayId display,
                           MobileGLBackendHandle surface, uint32_t width, uint32_t height) {
        (void)backend;
        g_resizeDisplay = display;
        g_resizeSurface = surface;
        g_resizeWidth = width;
        g_resizeHeight = height;
        return true;
    }
} // namespace

namespace MobileGL::Transport {
    TEST(ClientEglCommandsTest, MakeCurrentSwapIntervalResize) {
        g_makeSession = 0;
        g_draw = 0;
        g_read = 0;
        g_interval = 0;
        g_resizeDisplay = 0;
        g_resizeSurface = 0;
        g_resizeWidth = 0;
        g_resizeHeight = 0;

        const char* endpoint = "/tmp/mobilegl_egl_commands_test.sock";
        unlink(endpoint);

        MobileGLBackend backend{};
        MobileGLBackendVTable vtable{};
        vtable.structSize = sizeof(MobileGLBackendVTable);
        vtable.apiVersion = (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR;
        vtable.OnSessionCreated = &OnSessionCreated;
        vtable.OnSessionDestroyed = &OnSessionDestroyed;
        vtable.OnDisplayCreated = &OnDisplayCreated;
        vtable.OnDisplayDestroyed = &OnDisplayDestroyed;
        vtable.MakeEGLCurrent = &TestMakeEGLCurrent;
        vtable.SetSwapInterval = &TestSetSwapInterval;
        vtable.ResizeSurface = &TestResizeSurface;

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
            for (int i = 0; i < 5; ++i) {
                if (!core.ServiceOnce()) return;
            }
            serverOk = true;
        });

        ASSERT_TRUE(Client::SubmitSessionControl(10, true, 1));
        EXPECT_TRUE(Client::WaitResponseForToken(1, 5000));
        ASSERT_TRUE(Client::SubmitDisplayControl(50, true, 2));
        EXPECT_TRUE(Client::WaitResponseForToken(2, 5000));

        ASSERT_TRUE(Client::SendEglMakeCurrent(10, 100, 200, 3));
        ASSERT_TRUE(Client::SendEglSetSwapInterval(10, -1, 4));
        ASSERT_TRUE(Client::SendEglResizeSurface(50, 70, 128, 64, 5));
        serverThread.join();

        EXPECT_TRUE(serverOk);
        EXPECT_EQ(g_makeSession, 10u);
        EXPECT_EQ(g_draw, 100u);
        EXPECT_EQ(g_read, 200u);
        EXPECT_EQ(g_interval, -1);
        EXPECT_EQ(g_resizeDisplay, 50u);
        EXPECT_EQ(g_resizeSurface, 70u);
        EXPECT_EQ(g_resizeWidth, 128u);
        EXPECT_EQ(g_resizeHeight, 64u);

        core.Shutdown();
        Client::Shutdown();

        DestroyLocalSocketShmTransport(accepted);
        DestroyLocalSocketShmTransport(client);
        DestroyLocalSocketShmTransport(server);
        unlink(endpoint);
    }
} // namespace MobileGL::Transport

// End of File
