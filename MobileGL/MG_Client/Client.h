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

    // Starts the client from the MOBILEGL_CS_* environment variables.
    //
    //   MOBILEGL_CS_MODE=inprocess (default)
    //       Locates FullServer/UtilRuntime/BackendObject next to this library
    //       and auto-hosts them in-process through an InProcessTransport pair.
    //   MOBILEGL_CS_MODE=connect
    //       Dial MOBILEGL_CS_ENDPOINT (default /tmp/mobilegl.sock) and attach
    //       to an already-running FullServer.
    //
    // FCL zero-change usage: FCL only dlopens the renderer plugin and forwards
    // environment variables, so this entry point is the only bootstrapping
    // the client needs on Android.
    Bool InitializeFromEnvironment();

    // Test/embedded path: attach an already-created transport endpoint.
    Bool InitializeWithTransport(MobileGLTransport* transport, const MobileGLTransportOps* ops);

    // Sends one command (opcode from MG_Protocol/generated_opcodes.h) and waits
    // for its response. Returns true when the response status is OK and the
    // echoed token matches `token`.
    Bool SendCommand(Uint32 sessionId, Uint32 opcode, Uint64 token = 0);

    // Submits `count` commands in one Message frame (zero-copy batch path).
    // Tokens tokenBase .. tokenBase+count-1 are registered; the caller drains
    // them with WaitResponseForToken in the same order.
    Bool SubmitDataCommandBatch(Uint64 sessionId, Uint32 opcode, Uint64 tokenBase,
                                Uint32 count, Uint64 shmOffset, Uint64 shmSize,
                                MobileGLShmHandle* shm);

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

    // Sends a glDeleteSync command (awaits response).
    Bool SendDeleteSync(Uint64 sessionId, uint64_t sync, Uint64 token);

    // Sends a glWaitSync command (awaits response).
    Bool SendWaitSync(Uint64 sessionId, uint64_t sync, uint32_t flags, uint64_t timeout,
                      Uint64 token);

    // Sends a glClientWaitSync command; returns the backend result code.
    Bool SendClientWaitSync(Uint64 sessionId, uint64_t sync, uint32_t flags, uint64_t timeout,
                            Uint64 token, uint32_t* outResult);

    // Sends time-elapsed query lifecycle commands.
    Bool SendBeginTimeElapsedQuery(Uint64 sessionId, Uint64 token, uint64_t* outHandle);
    Bool SendEndTimeElapsedQuery(Uint64 sessionId, uint64_t query, Uint64 token);
    Bool SendDeleteBackendQuery(Uint64 sessionId, uint64_t query, Uint64 token);

    // Query result / occlusion / XFB-primitives query commands.
    Bool SendIsQueryResultAvailable(Uint64 sessionId, uint64_t query, Uint64 token,
                                    Bool* outAvailable);
    Bool SendGetQueryResult64(Uint64 sessionId, uint64_t query, Bool wait, Uint64 token,
                              Bool* outAvailable, uint64_t* outNanoseconds);
    Bool SendBeginOcclusionQuery(Uint64 sessionId, Uint64 token, uint64_t* outHandle);
    Bool SendEndOcclusionQuery(Uint64 sessionId, uint64_t query, Uint64 token);
    Bool SendBeginXfbPrimitivesQuery(Uint64 sessionId, Bool generated, Uint64 token,
                                     uint64_t* outHandle);
    Bool SendEndXfbPrimitivesQuery(Uint64 sessionId, uint64_t query, Uint64 token);

    // Sends a typed glDrawArraysInstanced command (awaits response).
    Bool SendDrawArraysInstanced(Uint64 sessionId, uint32_t mode, int32_t first, int32_t count,
                                 int32_t primcount, Uint64 token);

    // Sends a typed glDrawElementsInstanced command whose indices come from shared memory.
    Bool SendDrawElementsInstanced(Uint64 sessionId, uint32_t mode, int32_t count, uint32_t type,
                                   uint64_t shmOffset, int32_t instanceCount,
                                   MobileGLShmHandle* shm, Uint64 token);

    // Sends a typed glDrawRangeElements command whose indices come from shared memory.
    Bool SendDrawRangeElements(Uint64 sessionId, uint32_t mode, uint32_t start, uint32_t end,
                               int32_t count, uint32_t type, uint64_t shmOffset,
                               MobileGLShmHandle* shm, Uint64 token);

    // Sends a typed glBufferRespecify command; initial data, when present, comes
    // from shared memory.
    Bool SendBufferRespecify(Uint64 sessionId, uint64_t bufferHandle, uint64_t size,
                             uint32_t usage, MobileGLShmHandle* shm, Uint64 token);

    // Sends typed glDrawArraysIndirect / glDrawElementsIndirect commands whose
    // indirect command is provided through shared memory.
    Bool SendDrawArraysIndirect(Uint64 sessionId, uint32_t mode, uint64_t shmOffset,
                                MobileGLShmHandle* shm, Uint64 token);
    Bool SendDrawElementsIndirect(Uint64 sessionId, uint32_t mode, uint32_t type,
                                  uint64_t shmOffset, MobileGLShmHandle* shm, Uint64 token);

    // Sends a typed glGetString query and copies the returned string into outString.
    Bool SendGetString(Uint64 sessionId, uint32_t pname, Uint64 token, String* outString);

    // Sends a typed glGetStringi query (GL_EXTENSIONS enumeration).
    Bool SendGetStringi(Uint64 sessionId, uint32_t pname, uint32_t index, Uint64 token,
                        String* outString);

    // Sends a typed TextureRespecify upload; pixel data comes from shared memory.
    Bool SendTextureRespecify(Uint64 sessionId, uint64_t texture, uint32_t level,
                              uint32_t format, uint32_t type, uint32_t width,
                              uint32_t height, uint32_t depth, uint64_t dataSize,
                              MobileGLShmHandle* shm, Uint64 token);

    // Sends a typed TextureSubImage upload; pixel data comes from shared memory.
    Bool SendTextureSubImage(Uint64 sessionId, uint64_t texture, uint32_t level,
                             uint32_t format, uint32_t type, uint32_t width,
                             uint32_t height, uint32_t depth, uint64_t dataSize,
                             MobileGLShmHandle* shm, Uint64 token);

    // Sends a typed glReadPixels query; server returns pixels via shared memory.
    Bool SendReadPixels(Uint64 sessionId, int32_t x, int32_t y, int32_t width,
                        int32_t height, uint32_t format, uint32_t type,
                        void* outPixels, Uint64 outSize, Uint64 token);

    // Sends a typed glGetBufferSubData-style readback; server returns bytes via
    // shared memory.
    Bool SendBufferReadbackFromGpu(Uint64 sessionId, uint64_t bufferHandle, uint64_t offset,
                                   uint64_t size, void* outBytes, Uint64 outSize, Uint64 token);

    // MapBufferRange: server returns buffer bytes via shared memory; outMappedPtr
    // points into client-local shm until UnmapBuffer flushes it back.
    Bool MapBufferRange(Uint64 sessionId, uint64_t bufferHandle, uint64_t offset,
                        uint64_t size, uint32_t access, Uint64 token, void** outMappedPtr);

    // UnmapBuffer: flushes the previously mapped bytes back to the server buffer.
    Bool UnmapBuffer(Uint64 sessionId, Uint64 token);

    // EGL surface lifecycle commands.
    Bool SendEglCreatePbufferSurface(Uint64 displayId, uint64_t surface, int32_t width,
                                     int32_t height, Uint64 token);
    Bool SendEglCreateWindowSurface(Uint64 displayId, uint64_t surface, uint64_t nativeWindow,
                                    Uint64 token);
    Bool SendEglDestroySurface(Uint64 displayId, uint64_t surface, Uint64 token);
    Bool SendEglMakeCurrent(Uint64 sessionId, uint64_t draw, uint64_t read, Uint64 token);
    Bool SendEglSetSwapInterval(Uint64 sessionId, int32_t interval, Uint64 token);
    Bool SendEglResizeSurface(Uint64 displayId, uint64_t surface, uint32_t width,
                              uint32_t height, Uint64 token);

    // Submits a command without waiting for its response.
    Bool SubmitCommand(Uint32 sessionId, Uint32 opcode, Uint64 token);

    // Submits a command that carries one shared-memory payload; the shm handle
    // must already be allocated with the transport's OpenSharedMemory.
    Bool SubmitDataCommand(Uint32 sessionId, Uint32 opcode, Uint64 token,
                           Uint64 shmOffset, Uint64 shmSize, MobileGLShmHandle* shm);

    // Submits one command whose arguments are already serialized as a
    // WireFull Gen* root table (payloadBytes). This is the generic full
    // source-list path; the wire schema routes payload_bytes to the generated
    // server dispatch. outCapacity caps the server-side out-vector buffer (in
    // elements) for no-size queries (glGetIntegerv family); 0 means "use the
    // payload's own count field".
    Bool SendWirePayload(Uint64 sessionId, Uint32 opcode, Uint64 token,
                         const uint8_t* payloadBytes, Uint32 payloadSize,
                         MobileGLShmHandle* shm = nullptr,
                         Uint32 outCapacity = 0);

    // Active-variable introspection (glGetActiveAttrib/glGetActiveUniform).
    // The server fills a packed blob into Response.ret_bytes:
    // [u32 length][u32 size][u32 type][name NUL-terminated].
    Bool SendGetActiveAttrib(Uint64 sessionId, uint32_t program, uint32_t index,
                             uint32_t bufSize, Uint64 token);
    Bool SendGetActiveUniform(Uint64 sessionId, uint32_t program, uint32_t index,
                              uint32_t bufSize, Uint64 token);

    // Submits a session lifecycle control command (SessionCreate/Destroy).
    Bool SubmitSessionControl(Uint64 sessionId, Bool create, Uint64 token);

    // Submits a display lifecycle control command (DisplayCreate/Destroy).
    Bool SubmitDisplayControl(Uint64 displayId, Bool create, Uint64 token);

    // Submits a shared-group lifecycle control command.
    Bool SubmitSharedGroupControl(Uint64 groupId, Bool create, Uint64 token);

    // Waits for the response whose echoed token equals `token`.
    Bool WaitResponseForToken(Uint64 token, Uint32 timeoutMs);

    // Invalidates every outstanding token bound to `sessionId`: their
    // WaitResponseForToken calls fail immediately (late responses are never
    // mistaken for a newer request because tokens are never reused).
    void InvalidateSession(Uint64 sessionId);

    // Pending (submitted-but-not-yet-completed) token count; useful for
    // tests that exercise token/session invalidation.
    Uint32 GetPendingTokenCount();

    // Byte echoed back by the server's response data_byte field (payload
    // readback verification).
    Uint32 GetLastResponseDataByte();

    // Sync handle echoed back by the server's response sync field.
    Uint64 GetLastResponseSync();

    // Query nanoseconds echoed back by the server's response query_ns field.
    Uint64 GetLastResponseQueryNs();

    // Generic scalar return echoed back by the server's response ret_i64 field
    // (set by the generated wire dispatch for non-void API entries).
    int64_t GetLastResponseRetI64();

    // Out-vector bytes echoed back by the server's response ret_bytes field
    // (set by the generated wire dispatch for glGen* families).
    const Vector<Uint8>& GetLastResponseBytes();

    // String echoed back by the server's response string_value field.
    const String& GetLastResponseString();

    // Client-side current-session slot maintained by the EGL trampoline
    // (eglMakeCurrent). Generated GL trampolines read it to fill the
    // Command.session_id field.
    void SetCurrentSession(Uint64 sessionId);
    Uint64 GetCurrentSessionId();

    // Allocates one transport shared-memory region. On the in-process
    // transport the region is an in-heap slot; on LocalSocketShm it is a
    // memfd. Used by generated trampolines for shm-upload entries.
    Bool AllocateShm(Uint64 size, MobileGLShmHandle* out);
    void ReleaseShm(MobileGLShmHandle* handle);

    void Shutdown();
    const String& GetLastError();
} // namespace MobileGL::Client

// End of File
