// MobileGL - MobileGL/MG_Benchmark/Transport/CsRoundTripBench.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

// Cross-process C/S round-trip benchmark:
//   C++ flatbuffers client -> AF_UNIX socket -> libMobileGL_FullServer.so
//   (mobilegl_fullserver_run_socket) -> BackendObject vtable -> Response.
// This is the mg_bench-native replacement for the former scripts/bench_cs_e2e.py.

#include <benchmark/benchmark.h>
#include <flatbuffers/flatbuffers.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>

#include "MG_Protocol/gen/wire_generated.h"
#include "MG_Protocol/generated_opcodes.h"

namespace {
    constexpr uint32_t kSessionCreate = 1'000'000;
    constexpr uint64_t kSessionId = 5;
    const char* kEndpoint = "/tmp/mobilegl_cs_bench.sock";

    using FullServerCreate = void* (*)(const char*, const char*);
    using FullServerStart = int (*)(void*);
    using FullServerRunSocket = int (*)(void*, const char*, uint32_t);
    using FullServerDestroy = void (*)(void*);

    struct CsBench {
        void* Lib = nullptr;
        void* Handle = nullptr;
        FullServerStart Start = nullptr;
        FullServerRunSocket RunSocket = nullptr;
        FullServerDestroy Destroy = nullptr;
        int Socket = -1;
        std::thread Server;
        bool Ok = false;
    };

    CsBench& Bench() {
        static CsBench bench;
        return bench;
    }

    bool InitBench() {
        CsBench& b = Bench();
        if (b.Ok) {
            return true;
        }
        const char* repo = getenv("MGL_REPO_ROOT");
        std::string root = repo != nullptr ? repo : ".";
        std::string fullserver = root + "/build_agent/MobileGL/MG_FullServer/libMobileGL_FullServer.so";
        std::string util = root + "/build_agent/MobileGL/MG_UtilRuntime/libMobileGL_UtilRuntime.so";
        std::string backend =
            root + "/build_agent/MobileGL/MG_Backend/BackendObject_DirectGLES.so";

        b.Lib = dlopen(fullserver.c_str(), RTLD_LAZY | RTLD_GLOBAL);
        if (b.Lib == nullptr) {
            return false;
        }
        auto create = (FullServerCreate)dlsym(b.Lib, "mobilegl_fullserver_create");
        b.Start = (FullServerStart)dlsym(b.Lib, "mobilegl_fullserver_start");
        b.RunSocket = (FullServerRunSocket)dlsym(b.Lib, "mobilegl_fullserver_run_socket");
        b.Destroy = (FullServerDestroy)dlsym(b.Lib, "mobilegl_fullserver_destroy");
        if (create == nullptr || b.Start == nullptr || b.RunSocket == nullptr || b.Destroy == nullptr) {
            return false;
        }
        b.Handle = create(util.c_str(), backend.c_str());
        if (b.Handle == nullptr || b.Start(b.Handle) != 0) {
            return false;
        }
        unlink(kEndpoint);
        b.Server = std::thread([&] {
            b.RunSocket(b.Handle, kEndpoint, 1'000'002);
        });
        // Wait for the socket to appear.
        bool socketUp = false;
        for (int i = 0; i < 500; ++i) {
            if (access(kEndpoint, F_OK) == 0) {
                socketUp = true;
                break;
            }
            usleep(100 * 1000);
        }
        if (!socketUp) {
            return false;
        }
        b.Socket = socket(AF_UNIX, SOCK_STREAM, 0);
        if (b.Socket < 0) {
            return false;
        }
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, kEndpoint, sizeof(addr.sun_path) - 1);
        if (connect(b.Socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            return false;
        }
        b.Ok = true;
        return true;
    }

    bool SendRaw(int fd, const void* data, uint32_t size) {
        uint8_t header[4] = {};
        header[0] = static_cast<uint8_t>(size & 0xff);
        header[1] = static_cast<uint8_t>((size >> 8) & 0xff);
        header[2] = static_cast<uint8_t>((size >> 16) & 0xff);
        header[3] = static_cast<uint8_t>((size >> 24) & 0xff);
        if (send(fd, header, 4, MSG_NOSIGNAL) != 4) {
            return false;
        }
        return send(fd, data, size, MSG_NOSIGNAL) == static_cast<ssize_t>(size);
    }

    bool RecvRaw(int fd, std::vector<uint8_t>& out) {
        uint8_t header[4] = {};
        if (recv(fd, header, 4, 0) != 4) {
            return false;
        }
        uint32_t size = header[0] | (uint32_t(header[1]) << 8) | (uint32_t(header[2]) << 16) |
                        (uint32_t(header[3]) << 24);
        out.resize(size);
        return recv(fd, out.data(), size, 0) == static_cast<ssize_t>(size);
    }

    bool Control(uint32_t opcode, uint64_t session, uint64_t token) {
        flatbuffers::FlatBufferBuilder builder;
        auto command = MobileGL::Protocol::Wire::CreateCommand(builder, opcode, session, token);
        auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);
        std::vector<uint8_t> response;
        if (!SendRaw(Bench().Socket, builder.GetBufferPointer(), builder.GetSize())) {
            return false;
        }
        if (!RecvRaw(Bench().Socket, response)) {
            return false;
        }
        auto* parsed = MobileGL::Protocol::Wire::GetResponse(response.data());
        return parsed != nullptr && parsed->status() == 0;
    }

    bool Clear(uint64_t session, uint64_t token) {
        flatbuffers::FlatBufferBuilder builder;
        auto clear = MobileGL::Protocol::Wire::CreateGlClear(builder, 0);
        auto command = MobileGL::Protocol::Wire::CreateCommand(
            builder,
            static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glClear),
            session, token, clear);
        auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);
        std::vector<uint8_t> response;
        if (!SendRaw(Bench().Socket, builder.GetBufferPointer(), builder.GetSize())) {
            return false;
        }
        if (!RecvRaw(Bench().Socket, response)) {
            return false;
        }
        auto* parsed = MobileGL::Protocol::Wire::GetResponse(response.data());
        return parsed != nullptr && parsed->status() == 0;
    }

    bool BatchClear(uint32_t count, uint64_t tokenBase) {
        std::vector<std::vector<uint8_t>> frames;
        frames.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            flatbuffers::FlatBufferBuilder builder;
            auto clear = MobileGL::Protocol::Wire::CreateGlClear(builder, 0);
            auto command = MobileGL::Protocol::Wire::CreateCommand(
                builder,
                static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glClear),
                kSessionId, tokenBase + i, clear);
            auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
            builder.Finish(message);
            frames.emplace_back(builder.GetBufferPointer(),
                                builder.GetBufferPointer() + builder.GetSize());
        }
        for (const auto& frame : frames) {
            if (!SendRaw(Bench().Socket, frame.data(), static_cast<uint32_t>(frame.size()))) {
                return false;
            }
        }
        std::vector<uint8_t> response;
        for (uint32_t i = 0; i < count; ++i) {
            response.clear();
            if (!RecvRaw(Bench().Socket, response)) {
                return false;
            }
            auto* parsed = MobileGL::Protocol::Wire::GetResponse(response.data());
            if (parsed == nullptr || parsed->status() != 0 || parsed->token() != tokenBase + i) {
                return false;
            }
        }
        return true;
    }
} // namespace

static void CsRoundTrip(benchmark::State& state) {
    if (!InitBench()) {
        state.SkipWithError("FullServer C/S init failed (set MGL_REPO_ROOT)");
        return;
    }
    if (!Control(kSessionCreate, kSessionId, 9000)) {
        state.SkipWithError("SessionCreate failed");
        return;
    }
    for (auto _ : state) {
        if (!Clear(kSessionId, state.iterations() * 7 + 1)) {
            state.SkipWithError("Clear failed");
            return;
        }
    }
    Control(1'000'001, kSessionId, 9001); // SessionDestroy (best effort)
}
BENCHMARK(CsRoundTrip)->MeasureProcessCPUTime()->UseRealTime();

static void CsBatchRoundTrip(benchmark::State& state) {
    if (!InitBench()) {
        state.SkipWithError("FullServer C/S init failed (set MGL_REPO_ROOT)");
        return;
    }
    if (!Control(kSessionCreate, kSessionId, 9100)) {
        state.SkipWithError("SessionCreate failed");
        return;
    }
    const uint32_t batchSize = static_cast<uint32_t>(state.range(0));
    for (auto _ : state) {
        if (!BatchClear(batchSize, state.iterations() * batchSize + 1)) {
            state.SkipWithError("BatchClear failed");
            return;
        }
    }
    Control(1'000'001, kSessionId, 9101); // SessionDestroy (best effort)
}
BENCHMARK(CsBatchRoundTrip)->Args({16, 64, 256})->MeasureProcessCPUTime()->UseRealTime();

BENCHMARK_MAIN();
