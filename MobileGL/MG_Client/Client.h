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

    // Sends a typed glDrawElements command whose indices come from shared memory.
    Bool SendDrawElements(Uint64 sessionId, uint32_t mode, int32_t count, uint32_t type,
                          uint64_t shmOffset, MobileGLShmHandle* shm, Uint64 token);

    // Sends a typed glBufferSubData command whose data comes from shared memory.
    Bool SendBufferSubData(Uint64 sessionId, uint64_t bufferHandle, uint64_t offset,
                           uint64_t size, MobileGLShmHandle* shm, Uint64 token);

    // Sends a typed glMemoryBarrier command (awaits response).
    Bool SendMemoryBarrier(Uint64 sessionId, uint32_t barriers, Uint64 token);

    // Sends a typed glMemoryBarrierByRegion command (awaits response).
    Bool SendMemoryBarrierByRegion(Uint64 sessionId, uint32_t barriers, Uint64 token);

    // Sends a typed glPatchParameteri command (awaits response).
    Bool SendPatchParameteri(Uint64 sessionId, uint32_t pname, int32_t value, Uint64 token);

    // Sends a typed glGenerateMipmap command (awaits response).
    Bool SendGenerateMipmap(Uint64 sessionId, uint32_t target, Uint64 token);

    // Sends a typed glDispatchCompute command (awaits response).
    Bool SendDispatchCompute(Uint64 sessionId, uint32_t numGroupsX, uint32_t numGroupsY,
                             uint32_t numGroupsZ, Uint64 token);

    // Sends a typed glBeginTransformFeedback command (awaits response).
    Bool SendBeginTransformFeedback(Uint64 sessionId, uint32_t primitiveMode, Uint64 token);

    // Sends an opcode-only glEndTransformFeedback command (awaits response).
    Bool SendEndTransformFeedback(Uint64 sessionId, Uint64 token);

    // Sends opcode-only pause/resume transform feedback commands.
    Bool SendPauseTransformFeedback(Uint64 sessionId, Uint64 token);
    Bool SendResumeTransformFeedback(Uint64 sessionId, Uint64 token);

    // Sends a typed glBindTransformFeedback command (awaits response).
    Bool SendBindTransformFeedback(Uint64 sessionId, uint32_t name, Uint64 token);

    // Sends a typed glBlitFramebuffer command (awaits response).
    Bool SendBlitFramebuffer(Uint64 sessionId, uint64_t readFramebuffer, uint64_t drawFramebuffer,
                             int32_t srcX0, int32_t srcY0, int32_t srcX1,
                             int32_t srcY1, int32_t dstX0, int32_t dstY0, int32_t dstX1,
                             int32_t dstY1, uint32_t mask, uint32_t filter, Uint64 token);

    // Sends an eglSwapBuffers command (awaits response).
    Bool SendSwapBuffers(Uint64 sessionId, uint64_t draw, Uint64 token);

    // Sends a typed glDispatchComputeIndirect command (awaits response).
    Bool SendDispatchComputeIndirect(Uint64 sessionId, uint64_t indirectOffset, Uint64 token);

    // Sends a glFenceSync command and returns the backend sync handle.
    Bool SendFenceSync(Uint64 sessionId, uint32_t condition, uint32_t flags,
                       uint64_t token, uint64_t* outSync);

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

    // Sync handle echoed back by the server's response sync field.
    Uint64 GetLastResponseSync();

    void Shutdown();
    const String& GetLastError();
} // namespace MobileGL::Client

// End of File
