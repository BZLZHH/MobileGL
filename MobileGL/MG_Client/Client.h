// MobileGL - MobileGL/MG_Client/Client.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#pragma once

#include <Includes.h>
#include "MG_Protocol/transport.h"

namespace MobileGL::Client {
    // Thin client-side configuration. In v1 the client connects to one
    // FullServer endpoint and owns exactly one control connection.
    struct ClientConfig {
        String Endpoint;
        SizeT MaxShmArenaSize = 64 * 1024 * 1024;
        Uint32 TimeoutMs = 1000;
    };

    // Opens the control connection + shared-memory arena. This is the only
    // state the client keeps; there is no GL state on this side.
    Bool Initialize(const ClientConfig& config);

    // Test/embedded path: attach an already-created transport endpoint.
    Bool InitializeWithTransport(MobileGLTransport* transport, const MobileGLTransportOps* ops);

    // Sends one command (opcode from MG_Protocol/generated_opcodes.h) and waits
    // for its response. Returns true when the response status is OK and the
    // echoed token matches `token`.
    Bool SendCommand(Uint32 sessionId, Uint32 opcode, Uint64 token = 0);

    // Sends a typed glClearColor command (awaits response).
    Bool SendClearColor(Uint64 sessionId, float red, float green, float blue, float alpha,
                        Uint64 token);

    // Sends a typed glDrawArrays command (awaits response).
    Bool SendDrawArrays(Uint64 sessionId, uint32_t mode, int32_t first, int32_t count,
                        Uint64 token);

    // Submits a command without waiting for its response.
    Bool SubmitCommand(Uint32 sessionId, Uint32 opcode, Uint64 token);

    // Submits a command that carries one shared-memory payload; the shm handle
    // must already be allocated with the transport's OpenSharedMemory.
    Bool SubmitDataCommand(Uint32 sessionId, Uint32 opcode, Uint64 token,
                           Uint64 shmOffset, Uint64 shmSize, MobileGLShmHandle* shm);

    // Submits a session lifecycle control command (SessionCreate/Destroy).
    Bool SubmitSessionControl(Uint64 sessionId, Bool create, Uint64 token);

    // Submits a display lifecycle control command (DisplayCreate/Destroy).
    Bool SubmitDisplayControl(Uint64 displayId, Bool create, Uint64 token);

    // Submits a shared-group lifecycle control command.
    Bool SubmitSharedGroupControl(Uint64 groupId, Bool create, Uint64 token);

    // Waits for the response whose echoed token equals `token`.
    Bool WaitResponseForToken(Uint64 token, Uint32 timeoutMs);

    // Byte echoed back by the server's response data_byte field (payload
    // readback verification).
    Uint32 GetLastResponseDataByte();

    void Shutdown();
    const String& GetLastError();
} // namespace MobileGL::Client

// End of File
