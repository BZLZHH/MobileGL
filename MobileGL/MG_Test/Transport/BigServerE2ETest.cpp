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
#include "MG_Protocol/gen/wire_generated.h"
#include "MG_Protocol/generated_opcodes.h"
#include "MG_Protocol/transport.h"
#include "MG_Transport/InProcessTransport.h"
#include "MG_Transport/LocalSocketShmTransport.h"

#include <flatbuffers/flatbuffers.h>

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

    // Builds one FlatBuffer Message with a GlClear command. The buffer is kept
    // in a thread-local builder so the returned pointer stays valid until the
    // next rebuild on the same thread.
    thread_local flatbuffers::FlatBufferBuilder s_batchBuilder;

    MobileGLCommandBatch MakeClearBatch(uint64_t sessionId) {
        s_batchBuilder.Clear();
        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        const auto clear = MobileGL::Protocol::Wire::CreateGlClear(s_batchBuilder, 0);
        const auto command =
            MobileGL::Protocol::Wire::CreateCommand(s_batchBuilder,
                                                    static_cast<uint32_t>(
                                                        MobileGL::Protocol::MobileGLOpcode::glClear),
                                                    sessionId, clear);
        const auto message = MobileGL::Protocol::Wire::CreateMessage(s_batchBuilder, command);
        s_batchBuilder.Finish(message);
        batch.flatBufferData = s_batchBuilder.GetBufferPointer();
        batch.flatBufferSize = static_cast<uint32_t>(s_batchBuilder.GetSize());
        return batch;
    }

    void DestroyBatch(MobileGLCommandBatch* batch) {
        batch->flatBufferData = nullptr;
        batch->flatBufferSize = 0;
    }

    uint32_t ParseResponseStatus(MobileGLResponseQueue* response) {
        const auto* parsed =
            flatbuffers::GetRoot<MobileGL::Protocol::Wire::Response>(response->flatBufferData);
        return parsed == nullptr ? 0xFFFFFFFFu : parsed->status();
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

        MobileGLCommandBatch batch = MakeClearBatch(42);
        ASSERT_TRUE(ops.SubmitCommands(client, &batch));
        DestroyBatch(&batch);

        ASSERT_TRUE(core.ServiceOnce());

        MobileGLResponseQueue response{};
        ASSERT_TRUE(ops.WaitResponses(client, &response, 0));
        EXPECT_EQ(ParseResponseStatus(&response), 0u);
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

            MobileGLCommandBatch batch = MakeClearBatch(7);
            if (!ops.SubmitCommands(client, &batch)) {
                DestroyBatch(&batch);
                DestroyLocalSocketShmTransport(client);
                return;
            }
            DestroyBatch(&batch);

            MobileGLResponseQueue response{};
            if (ops.WaitResponses(client, &response, 0) && ParseResponseStatus(&response) == 0) {
                clientOk = true;
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
