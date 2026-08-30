// MobileGL - MobileGL/MG_Test/Transport/ClientWirePayloadTest.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include <Includes.h>
#include <gtest/gtest.h>
#include <thread>
#include "MG_Client/Client.h"
#include "MG_Client/generated_wire_client.h"
#include "MG_FullServer/ServerCore.h"
#include "MG_Protocol/generated_opcodes.h"
#include "MG_Protocol/gen/wire_full_generated.h"
#include "MG_Transport/InProcessTransport.h"

// This test defines the opaque backend object locally; it is not linked with
// any BackendObject plugin .so, so completing the type here is safe.
struct MobileGLBackend {
    uint32_t Nonce = 0;
};

namespace {
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

    // A minimal generated-dispatch stand-in: decodes the WireFull GenClear
    // root and verifies the mask arrived intact.
    uint32_t FakeWireDispatch(uint32_t opcode, uint32_t sessionId,
                              const void* payloadBytes, uint64_t payloadSize,
                              const void* const* receivedShm, uint32_t receivedShmCount,
                              uint32_t outCapacity) {
        (void)sessionId;
        (void)receivedShm;
        (void)receivedShmCount;
        (void)outCapacity;
        if (opcode != static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glClear) ||
            payloadBytes == nullptr || payloadSize < 4) {
            return 1u;
        }
        const auto* payload =
            ::flatbuffers::GetRoot<MobileGL::Protocol::WireFull::GenClear>(payloadBytes);
        return payload != nullptr && payload->mask() == 0xAB'CD'EF'01u ? 0u : 1u;
    }
} // namespace

namespace MobileGL::Transport {
    TEST(ClientWirePayloadTest, GenericPayloadBytesReachWireDispatch) {
        constexpr uint64_t kSessionId = 111;
        constexpr uint64_t kToken = 5001;

        MobileGLBackend backend{};
        MobileGLBackendVTable vtable{};
        vtable.structSize = sizeof(MobileGLBackendVTable);
        vtable.apiVersion = (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR;
        vtable.OnSessionCreated = &OnSessionCreated;
        vtable.OnSessionDestroyed = &OnSessionDestroyed;

        MobileGLTransport* client = nullptr;
        MobileGLTransport* server = nullptr;
        CreateInProcessTransportPair(&client, &server);
        ASSERT_NE(client, nullptr);
        ASSERT_NE(server, nullptr);

        const MobileGLTransportOps& ops = GetInProcessTransportOps();
        MobileGLTransportConfig config{};
        config.structSize = sizeof(MobileGLTransportConfig);
        config.kind = MobileGLTransportKindInProcess;
        ASSERT_TRUE(ops.Start(client, &config));
        ASSERT_TRUE(ops.Start(server, &config));

        MobileGL::FullServer::ServerCore core(&ops, server, &backend, &vtable);
        ASSERT_TRUE(core.Start());
        core.SetWireDispatch(&FakeWireDispatch);
        ASSERT_TRUE(Client::InitializeWithTransport(client, &ops));

        Bool serverOk = false;
        std::thread serverThread([&] {
            for (int i = 0; i < 2; ++i) {
                if (!core.ServiceOnce()) return;
            }
            serverOk = true;
        });

        EXPECT_TRUE(Client::SubmitSessionControl(kSessionId, true, 1));
        EXPECT_TRUE(Client::WaitResponseForToken(1, 5000));

        EXPECT_TRUE(MobileGL::Client::Wire::SendGlClear(kSessionId, 0xAB'CD'EF'01u, kToken));

        serverThread.join();
        EXPECT_TRUE(serverOk);

        core.Shutdown();
        Client::Shutdown();
        DestroyInProcessTransport(server);
        DestroyInProcessTransport(client);
    }
} // namespace MobileGL::Transport

// End of File
