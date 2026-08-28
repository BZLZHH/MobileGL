// MobileGL - MobileGL/MG_Test/Transport/ShmPayloadBenchmark.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include <Includes.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <numeric>
#include <thread>
#include "MG_Client/Client.h"
#include "MG_FullServer/ServerCore.h"
#include "MG_Protocol/generated_opcodes.h"
#include "MG_Protocol/transport.h"
#include "MG_Transport/LocalSocketShmTransport.h"

#include <unistd.h>

struct MobileGLBackend {
    uint32_t Nonce = 0;
};

namespace {
    void TestClear(MobileGLBackend* backend, MobileGLSessionId session, uint32_t mask) {
        (void)backend;
        (void)session;
        (void)mask;
    }

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
} // namespace

int main(int argc, char** argv) {
    const uint32_t iterations = argc > 1 ? static_cast<uint32_t>(atoi(argv[1])) : 100;
    const uint64_t sessionId = 5;

    const char* endpoint = "/tmp/mobilegl_shm_bench.sock";
    unlink(endpoint);

    MobileGLBackend backend{};
    MobileGLBackendVTable vtable{};
    vtable.structSize = sizeof(MobileGLBackendVTable);
    vtable.apiVersion = (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR;
    vtable.OnSessionCreated = &OnSessionCreated;
    vtable.OnSessionDestroyed = &OnSessionDestroyed;
    vtable.Clear = &TestClear;

    MobileGLTransport* server = MobileGL::Transport::CreateLocalSocketShmServer(endpoint);
    MobileGLTransport* client = MobileGL::Transport::CreateLocalSocketShmTransport();
    const auto& ops = MobileGL::Transport::GetLocalSocketShmTransportOps();

    MobileGLTransportConfig config{};
    config.structSize = sizeof(MobileGLTransportConfig);
    config.kind = MobileGLTransportKindLocalSocketShm;
    config.endpoint = endpoint;
    config.maxShmArenaSize = 1024 * 1024;
    config.timeoutMs = 0;
    if (!ops.Start(client, &config)) {
        return 1;
    }
    MobileGLTransport* accepted = MobileGL::Transport::AcceptLocalSocketShmConnection(server);

    MobileGL::FullServer::ServerCore core(&ops, accepted, &backend, &vtable);
    if (!core.Start()) return 1;
    if (!MobileGL::Client::InitializeWithTransport(client, &ops)) return 1;

    MobileGLShmHandle shm{};
    if (!ops.OpenSharedMemory(client, &shm)) return 1;
    static_cast<uint8_t*>(shm.mappedAddress)[0] = 0x11;

    bool serverOk = false;
    std::thread serverThread([&] {
        for (uint32_t i = 0; i < iterations + 1; ++i) {
            if (!core.ServiceOnce()) return;
        }
        serverOk = true;
    });

    if (!MobileGL::Client::SubmitSessionControl(sessionId, true, 0) ||
        !MobileGL::Client::WaitResponseForToken(0, 5000)) {
        return 1;
    }

    std::vector<uint64_t> latencies;
    latencies.reserve(iterations);
    for (uint32_t i = 0; i < iterations; ++i) {
        const auto started = std::chrono::steady_clock::now();
        if (!MobileGL::Client::SubmitDataCommand(
                static_cast<uint32_t>(sessionId),
                static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glClear),
                i + 1, 0, 1, &shm) ||
            !MobileGL::Client::WaitResponseForToken(i + 1, 5000)) {
            printf("iteration %u failed\n", i);
            return 1;
        }
        latencies.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                std::chrono::steady_clock::now() - started)
                                .count());
    }

    serverThread.join();
    const uint64_t avgNs = std::accumulate(latencies.begin(), latencies.end(), uint64_t{0}) /
                           latencies.size();
    const uint64_t minNs = *std::min_element(latencies.begin(), latencies.end());
    const uint64_t maxNs = *std::max_element(latencies.begin(), latencies.end());

    printf("iterations=%u avg_us=%.1f min_us=%.1f max_us=%.1f serverOk=%d\n",
           iterations, avgNs / 1000.0, minNs / 1000.0, maxNs / 1000.0, serverOk ? 1 : 0);

    ops.ReleaseSharedMemory(client, &shm);
    core.Shutdown();
    MobileGL::Client::Shutdown();
    MobileGL::Transport::DestroyLocalSocketShmTransport(accepted);
    MobileGL::Transport::DestroyLocalSocketShmTransport(client);
    MobileGL::Transport::DestroyLocalSocketShmTransport(server);
    unlink(endpoint);
    return 0;
}

// End of File
