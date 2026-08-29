// MobileGL - MobileGL/MG_Client/Client.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "Client.h"
#include "MG_Protocol/control.h"
#include "MG_Protocol/gen/wire_generated.h"
#include "MG_Protocol/generated_opcodes.h"
#include "MG_Transport/LocalSocketShmTransport.h"

#include <flatbuffers/flatbuffers.h>

namespace MobileGL::Client {
    namespace {
        String s_lastError;
        Bool s_initialized = false;
        Bool s_ownsTransport = false;
        MobileGLTransport* s_transport = nullptr;
        const MobileGLTransportOps* s_ops = nullptr;
        Uint32 s_lastResponseByte = 0;
        Uint64 s_lastResponseSync = 0;
        Uint64 s_lastResponseQueryNs = 0;
        String s_lastResponseString;
        Vector<MobileGLShmHandle> s_lastResponseShm;
        MobileGLShmHandle s_mappedShm{};
        Bool s_mappedActive = false;
        Uint64 s_mappedBuffer = 0;
        Uint64 s_mappedOffset = 0;
        Uint64 s_mappedSize = 0;
        void* s_mappedPtr = nullptr;

        Bool SendOpcodesOnly(Uint32 opcode, Uint64 sessionId, Uint64 token) {
            if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
                s_lastError = "Client is not initialized.";
                return false;
            }
            flatbuffers::FlatBufferBuilder builder;
            const auto command =
                MobileGL::Protocol::Wire::CreateCommand(builder, opcode, sessionId, token);
            const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
            builder.Finish(message);

            MobileGLCommandBatch batch{};
            batch.structSize = sizeof(MobileGLCommandBatch);
            batch.flatBufferData = builder.GetBufferPointer();
            batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
            if (!s_ops->SubmitCommands(s_transport, &batch)) {
                s_lastError = s_ops->GetLastError(s_transport);
                return false;
            }
            return WaitResponseForToken(token, 0);
        }

        Bool SubmitFlatBuffer(const uint8_t* data, Uint32 size, Uint64 token) {
            if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
                s_lastError = "Client is not initialized.";
                return false;
            }
            MobileGLCommandBatch batch{};
            batch.structSize = sizeof(MobileGLCommandBatch);
            batch.flatBufferData = const_cast<uint8_t*>(data);
            batch.flatBufferSize = size;
            if (!s_ops->SubmitCommands(s_transport, &batch)) {
                s_lastError = s_ops->GetLastError(s_transport);
                return false;
            }
            return WaitResponseForToken(token, 0);
        }
    } // namespace

    Bool Initialize(const ClientConfig& config) {
        if (s_initialized) {
            s_lastError = "Client already initialized.";
            return false;
        }

        s_transport = Transport::CreateLocalSocketShmTransport();
        const MobileGLTransportOps& ops = Transport::GetLocalSocketShmTransportOps();

        MobileGLTransportConfig transportConfig{};
        transportConfig.structSize = sizeof(MobileGLTransportConfig);
        transportConfig.kind = MobileGLTransportKindLocalSocketShm;
        transportConfig.endpoint = config.Endpoint.empty() ? "/tmp/mobilegl.sock" : config.Endpoint.c_str();
        transportConfig.maxBatchCommands = 256;
        transportConfig.maxShmArenaSize = static_cast<Uint32>(config.MaxShmArenaSize);
        transportConfig.timeoutMs = config.TimeoutMs;

        if (!ops.Start(s_transport, &transportConfig)) {
            s_lastError = ops.GetLastError(s_transport);
            Transport::DestroyLocalSocketShmTransport(s_transport);
            s_transport = nullptr;
            return false;
        }

        s_ops = &ops;
        s_ownsTransport = true;
        s_initialized = true;
        s_lastError.clear();
        return true;
    }

    Bool InitializeWithTransport(MobileGLTransport* transport, const MobileGLTransportOps* ops) {
        if (s_initialized || transport == nullptr || ops == nullptr) {
            s_lastError = "Client already initialized or invalid transport.";
            return false;
        }
        s_transport = transport;
        s_ops = ops;
        s_ownsTransport = false;
        s_initialized = true;
        s_lastError.clear();
        return true;
    }

    Bool SendClearColor(Uint64 sessionId, float red, float green, float blue, float alpha,
                        Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto color = MobileGL::Protocol::Wire::CreateClearColor(builder, red, green, blue, alpha);
        const auto command = MobileGL::Protocol::Wire::CreateCommand(
            builder,
            static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glClearColor),
            sessionId, token, 0, color, 0);
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SendDrawArrays(Uint64 sessionId, uint32_t mode, int32_t first, int32_t count,
                        Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto draw = MobileGL::Protocol::Wire::CreateDrawArrays(builder, mode, first, count);
        const auto command = MobileGL::Protocol::Wire::CreateCommand(
            builder,
            static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glDrawArrays),
            sessionId, token, 0, 0, draw, 0);
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SendDrawElements(Uint64 sessionId, uint32_t mode, int32_t count, uint32_t type,
                          uint64_t shmOffset, MobileGLShmHandle* shm, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr || shm == nullptr) {
            s_lastError = "Client is not initialized or shm missing.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto draw =
            MobileGL::Protocol::Wire::CreateDrawElements(builder, mode, count, type, shmOffset);
        const auto command = MobileGL::Protocol::Wire::CreateCommand(
            builder,
            static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glDrawElements),
            sessionId, token, 0, 0, 0, draw, 0);
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 1);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        batch.shmHandleCount = 1;
        batch.shmHandles = shm;
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SendBufferSubData(Uint64 sessionId, uint64_t bufferHandle, uint64_t offset,
                           uint64_t size, MobileGLShmHandle* shm, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr || shm == nullptr) {
            s_lastError = "Client is not initialized or shm missing.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto sub = MobileGL::Protocol::Wire::CreateBufferSubData(builder, bufferHandle, offset, size);
        const auto command = MobileGL::Protocol::Wire::CreateCommand(
            builder,
            static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glBufferSubData),
            sessionId, token, 0, 0, 0, 0, sub, 0);
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 1);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        batch.shmHandleCount = 1;
        batch.shmHandles = shm;
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SendMemoryBarrier(Uint64 sessionId, uint32_t barriers, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto mb = MobileGL::Protocol::Wire::CreateMemoryBarrier(builder, barriers);
        const auto command = MobileGL::Protocol::Wire::CreateCommand(
            builder,
            static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glMemoryBarrier),
            sessionId, token, 0, 0, 0, 0, 0, mb, 0);
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SendPatchParameteri(Uint64 sessionId, uint32_t pname, int32_t value, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto pp = MobileGL::Protocol::Wire::CreatePatchParameteri(builder, pname, value);
        const auto command = MobileGL::Protocol::Wire::CreateCommand(
            builder,
            static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glPatchParameteri),
            sessionId, token, 0, 0, 0, 0, 0, 0, 0, pp, 0);
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SendGenerateMipmap(Uint64 sessionId, uint32_t target, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto gm = MobileGL::Protocol::Wire::CreateGenerateMipmap(builder, target);
        const auto command = MobileGL::Protocol::Wire::CreateCommand(
            builder,
            static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glGenerateMipmap),
            sessionId, token, 0, 0, 0, 0, 0, 0, 0, 0, gm, 0);
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SendDispatchCompute(Uint64 sessionId, uint32_t numGroupsX, uint32_t numGroupsY,
                             uint32_t numGroupsZ, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto dc = MobileGL::Protocol::Wire::CreateDispatchCompute(builder, numGroupsX, numGroupsY,
                                                                        numGroupsZ);
        const auto command = MobileGL::Protocol::Wire::CreateCommand(
            builder,
            static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glDispatchCompute),
            sessionId, token, 0, 0, 0, 0, 0, 0, 0, 0, 0, dc, 0);
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SendBeginTransformFeedback(Uint64 sessionId, uint32_t primitiveMode, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto btf = MobileGL::Protocol::Wire::CreateBeginTransformFeedback(builder, primitiveMode);
        const auto command = MobileGL::Protocol::Wire::CreateCommand(
            builder,
            static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glBeginTransformFeedback),
            sessionId, token, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, btf, 0);
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SendEndTransformFeedback(Uint64 sessionId, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto command = MobileGL::Protocol::Wire::CreateCommand(
            builder,
            static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glEndTransformFeedback),
            sessionId, token);
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SendPauseTransformFeedback(Uint64 sessionId, Uint64 token) {
        return SendOpcodesOnly(static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glPauseTransformFeedback),
                               sessionId, token);
    }

    Bool SendResumeTransformFeedback(Uint64 sessionId, Uint64 token) {
        return SendOpcodesOnly(static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glResumeTransformFeedback),
                               sessionId, token);
    }

    Bool SendBindTransformFeedback(Uint64 sessionId, uint32_t name, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto btf = MobileGL::Protocol::Wire::CreateBindTransformFeedback(builder, name);
        const auto command = MobileGL::Protocol::Wire::CreateCommand(
            builder,
            static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glBindTransformFeedback),
            sessionId, token, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, btf, 0);
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SendBlitFramebuffer(Uint64 sessionId, uint64_t readFramebuffer, uint64_t drawFramebuffer,
                             int32_t srcX0, int32_t srcY0, int32_t srcX1,
                             int32_t srcY1, int32_t dstX0, int32_t dstY0, int32_t dstX1,
                             int32_t dstY1, uint32_t mask, uint32_t filter, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto blit = MobileGL::Protocol::Wire::CreateBlitFramebuffer(
            builder, readFramebuffer, drawFramebuffer, srcX0, srcY0, srcX1, srcY1,
            dstX0, dstY0, dstX1, dstY1, mask, filter);
        const auto command = MobileGL::Protocol::Wire::CreateCommand(
            builder,
            static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glBlitFramebuffer),
            sessionId, token, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, blit, 0);
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SendMemoryBarrierByRegion(Uint64 sessionId, uint32_t barriers, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto mb = MobileGL::Protocol::Wire::CreateMemoryBarrierByRegion(builder, barriers);
        const auto command = MobileGL::Protocol::Wire::CreateCommand(
            builder,
            static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glMemoryBarrierByRegion),
            sessionId, token, 0, 0, 0, 0, 0, 0, mb, 0, 0, 0, 0, 0, 0);
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SendSwapBuffers(Uint64 sessionId, uint64_t draw, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto swp = MobileGL::Protocol::Wire::CreateSwapBuffers(builder, draw);
        const auto command = MobileGL::Protocol::Wire::CreateCommand(
            builder,
            static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::eglSwapBuffers),
            sessionId, token, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, swp, 0);
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SendDispatchComputeIndirect(Uint64 sessionId, uint64_t indirectOffset, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto dci = MobileGL::Protocol::Wire::CreateDispatchComputeIndirect(builder, indirectOffset);
        const auto command = MobileGL::Protocol::Wire::CreateCommand(
            builder,
            static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glDispatchComputeIndirect),
            sessionId, token, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, dci);
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SendFenceSync(Uint64 sessionId, uint32_t condition, uint32_t flags,
                       Uint64 token, uint64_t* outSync) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto fs = MobileGL::Protocol::Wire::CreateFenceSync(builder, condition, flags);
        const auto command = MobileGL::Protocol::Wire::CreateCommand(
            builder,
            static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glFenceSync),
            sessionId, token, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, fs);
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        if (!WaitResponseForToken(token, 0)) {
            return false;
        }
        if (outSync != nullptr) {
            *outSync = s_lastResponseSync;
        }
        return true;
    }

    Bool SendDeleteSync(Uint64 sessionId, uint64_t sync, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto ds = MobileGL::Protocol::Wire::CreateDeleteSync(builder, sync);
        const auto command = MobileGL::Protocol::Wire::CreateCommand(
            builder,
            static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glDeleteSync),
            sessionId, token, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ds);
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SendWaitSync(Uint64 sessionId, uint64_t sync, uint32_t flags, uint64_t timeout,
                      Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto ws = MobileGL::Protocol::Wire::CreateWaitSync(builder, sync, flags, timeout);
        const auto command = MobileGL::Protocol::Wire::CreateCommand(
            builder,
            static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glWaitSync),
            sessionId, token, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ws);
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SendClientWaitSync(Uint64 sessionId, uint64_t sync, uint32_t flags, uint64_t timeout,
                            Uint64 token, uint32_t* outResult) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto cws = MobileGL::Protocol::Wire::CreateClientWaitSync(builder, sync, flags, timeout);
        MobileGL::Protocol::Wire::CommandBuilder cb(builder);
        cb.add_opcode(static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glClientWaitSync));
        cb.add_session_id(sessionId);
        cb.add_token(token);
        cb.add_client_wait_sync(cws);
        const auto command = cb.Finish();
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);
        if (!SubmitFlatBuffer(builder.GetBufferPointer(), static_cast<Uint32>(builder.GetSize()),
                              token)) {
            return false;
        }
        if (outResult != nullptr) {
            *outResult = static_cast<uint32_t>(GetLastResponseSync());
        }
        return true;
    }

    Bool SendBeginTimeElapsedQuery(Uint64 sessionId, Uint64 token, uint64_t* outHandle) {
        if (!SendOpcodesOnly(
                static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glBeginQueryIndexed),
                sessionId, token)) {
            return false;
        }
        if (outHandle != nullptr) {
            *outHandle = GetLastResponseSync();
        }
        return true;
    }

    Bool SendEndTimeElapsedQuery(Uint64 sessionId, uint64_t query, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto eq = MobileGL::Protocol::Wire::CreateEndTimeElapsedQuery(builder, query);
        MobileGL::Protocol::Wire::CommandBuilder cb(builder);
        cb.add_opcode(static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glEndQueryIndexed));
        cb.add_session_id(sessionId);
        cb.add_token(token);
        cb.add_end_time_elapsed_query(eq);
        const auto command = cb.Finish();
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);
        return SubmitFlatBuffer(builder.GetBufferPointer(), static_cast<Uint32>(builder.GetSize()),
                                token);
    }

    Bool SendDeleteBackendQuery(Uint64 sessionId, uint64_t query, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto dq = MobileGL::Protocol::Wire::CreateDeleteBackendQuery(builder, query);
        MobileGL::Protocol::Wire::CommandBuilder cb(builder);
        cb.add_opcode(static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glDeleteQueries));
        cb.add_session_id(sessionId);
        cb.add_token(token);
        cb.add_delete_backend_query(dq);
        const auto command = cb.Finish();
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);
        return SubmitFlatBuffer(builder.GetBufferPointer(), static_cast<Uint32>(builder.GetSize()),
                                token);
    }

    Bool SendIsQueryResultAvailable(Uint64 sessionId, uint64_t query, Uint64 token,
                                    Bool* outAvailable) {
        flatbuffers::FlatBufferBuilder builder;
        const auto q = MobileGL::Protocol::Wire::CreateIsQueryResultAvailable(builder, query);
        MobileGL::Protocol::Wire::CommandBuilder cb(builder);
        cb.add_opcode(static_cast<uint32_t>(
            MobileGL::Protocol::MobileGLControlOpcode::QueryResultAvailable));
        cb.add_session_id(sessionId);
        cb.add_token(token);
        cb.add_is_query_result_available(q);
        const auto command = cb.Finish();
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);
        if (!SubmitFlatBuffer(builder.GetBufferPointer(), static_cast<Uint32>(builder.GetSize()),
                              token)) {
            return false;
        }
        if (outAvailable != nullptr) {
            *outAvailable = GetLastResponseDataByte() != 0 ? 1 : 0;
        }
        return true;
    }

    Bool SendGetQueryResult64(Uint64 sessionId, uint64_t query, Bool wait, Uint64 token,
                              Bool* outAvailable, uint64_t* outNanoseconds) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto q = MobileGL::Protocol::Wire::CreateGetQueryResult64(builder, query, wait ? 1 : 0);
        MobileGL::Protocol::Wire::CommandBuilder cb(builder);
        cb.add_opcode(static_cast<uint32_t>(MobileGL::Protocol::MobileGLControlOpcode::QueryResult64));
        cb.add_session_id(sessionId);
        cb.add_token(token);
        cb.add_get_query_result64(q);
        const auto command = cb.Finish();
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);
        if (!SubmitFlatBuffer(builder.GetBufferPointer(), static_cast<Uint32>(builder.GetSize()),
                              token)) {
            return false;
        }
        if (outAvailable != nullptr) {
            *outAvailable = GetLastResponseDataByte() != 0 ? 1 : 0;
        }
        if (outNanoseconds != nullptr) {
            *outNanoseconds = GetLastResponseQueryNs();
        }
        return true;
    }

    Bool SendBeginOcclusionQuery(Uint64 sessionId, Uint64 token, uint64_t* outHandle) {
        if (!SendOpcodesOnly(
                static_cast<uint32_t>(MobileGL::Protocol::MobileGLControlOpcode::BeginOcclusionQuery),
                sessionId, token)) {
            return false;
        }
        if (outHandle != nullptr) {
            *outHandle = GetLastResponseSync();
        }
        return true;
    }

    Bool SendEndOcclusionQuery(Uint64 sessionId, uint64_t query, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto q = MobileGL::Protocol::Wire::CreateEndOcclusionQuery(builder, query);
        MobileGL::Protocol::Wire::CommandBuilder cb(builder);
        cb.add_opcode(static_cast<uint32_t>(
            MobileGL::Protocol::MobileGLControlOpcode::EndOcclusionQuery));
        cb.add_session_id(sessionId);
        cb.add_token(token);
        cb.add_end_occlusion_query(q);
        const auto command = cb.Finish();
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);
        return SubmitFlatBuffer(builder.GetBufferPointer(), static_cast<Uint32>(builder.GetSize()),
                                token);
    }

    Bool SendBeginXfbPrimitivesQuery(Uint64 sessionId, Bool generated, Uint64 token,
                                     uint64_t* outHandle) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto q =
            MobileGL::Protocol::Wire::CreateBeginXfbPrimitivesQuery(builder, generated ? 1 : 0);
        MobileGL::Protocol::Wire::CommandBuilder cb(builder);
        cb.add_opcode(static_cast<uint32_t>(
            MobileGL::Protocol::MobileGLControlOpcode::BeginXfbPrimitivesQuery));
        cb.add_session_id(sessionId);
        cb.add_token(token);
        cb.add_begin_xfb_primitives_query(q);
        const auto command = cb.Finish();
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);
        if (!SubmitFlatBuffer(builder.GetBufferPointer(), static_cast<Uint32>(builder.GetSize()),
                              token)) {
            return false;
        }
        if (outHandle != nullptr) {
            *outHandle = GetLastResponseSync();
        }
        return true;
    }

    Bool SendEndXfbPrimitivesQuery(Uint64 sessionId, uint64_t query, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto q = MobileGL::Protocol::Wire::CreateEndXfbPrimitivesQuery(builder, query);
        MobileGL::Protocol::Wire::CommandBuilder cb(builder);
        cb.add_opcode(static_cast<uint32_t>(
            MobileGL::Protocol::MobileGLControlOpcode::EndXfbPrimitivesQuery));
        cb.add_session_id(sessionId);
        cb.add_token(token);
        cb.add_end_xfb_primitives_query(q);
        const auto command = cb.Finish();
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);
        return SubmitFlatBuffer(builder.GetBufferPointer(), static_cast<Uint32>(builder.GetSize()),
                                token);
    }

    Bool SendDrawArraysInstanced(Uint64 sessionId, uint32_t mode, int32_t first, int32_t count,
                                 int32_t primcount, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto dai = MobileGL::Protocol::Wire::CreateDrawArraysInstanced(
            builder, mode, first, count, primcount);
        const auto command = MobileGL::Protocol::Wire::CreateCommand(
            builder,
            static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glDrawArraysInstanced),
            sessionId, token, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, dai);
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SendDrawElementsInstanced(Uint64 sessionId, uint32_t mode, int32_t count, uint32_t type,
                                   uint64_t shmOffset, int32_t instanceCount,
                                   MobileGLShmHandle* shm, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr || shm == nullptr) {
            s_lastError = "Client is not initialized or shm missing.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto dei = MobileGL::Protocol::Wire::CreateDrawElementsInstanced(
            builder, mode, count, type, shmOffset, instanceCount);
        const auto command = MobileGL::Protocol::Wire::CreateCommand(
            builder,
            static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glDrawElementsInstanced),
            sessionId, token, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, dei);
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 1);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        batch.shmHandleCount = 1;
        batch.shmHandles = shm;
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SendDrawRangeElements(Uint64 sessionId, uint32_t mode, uint32_t start, uint32_t end,
                               int32_t count, uint32_t type, uint64_t shmOffset,
                               MobileGLShmHandle* shm, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr || shm == nullptr) {
            s_lastError = "Client is not initialized or shm missing.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto dre = MobileGL::Protocol::Wire::CreateDrawRangeElements(
            builder, mode, start, end, count, type, shmOffset);
        MobileGL::Protocol::Wire::CommandBuilder commandBuilder(builder);
        commandBuilder.add_opcode(static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glDrawRangeElements));
        commandBuilder.add_session_id(sessionId);
        commandBuilder.add_token(token);
        commandBuilder.add_draw_range_elements(dre);
        const auto command = commandBuilder.Finish();
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 1);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        batch.shmHandleCount = 1;
        batch.shmHandles = shm;
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SendBufferRespecify(Uint64 sessionId, uint64_t bufferHandle, uint64_t size,
                             uint32_t usage, MobileGLShmHandle* shm, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto br = MobileGL::Protocol::Wire::CreateBufferRespecify(builder, bufferHandle, size, usage);
        MobileGL::Protocol::Wire::CommandBuilder commandBuilder(builder);
        commandBuilder.add_opcode(static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glBufferData));
        commandBuilder.add_session_id(sessionId);
        commandBuilder.add_token(token);
        commandBuilder.add_buffer_respecify(br);
        const auto command = commandBuilder.Finish();
        const auto message =
            MobileGL::Protocol::Wire::CreateMessage(builder, command, shm == nullptr ? 0 : 1);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        batch.shmHandleCount = shm == nullptr ? 0 : 1;
        batch.shmHandles = shm;
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SendDrawArraysIndirect(Uint64 sessionId, uint32_t mode, uint64_t shmOffset,
                                MobileGLShmHandle* shm, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr || shm == nullptr) {
            s_lastError = "Client is not initialized or shm missing.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto indirect = MobileGL::Protocol::Wire::CreateDrawArraysIndirect(builder, mode, shmOffset);
        MobileGL::Protocol::Wire::CommandBuilder commandBuilder(builder);
        commandBuilder.add_opcode(static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glDrawArraysIndirect));
        commandBuilder.add_session_id(sessionId);
        commandBuilder.add_token(token);
        commandBuilder.add_draw_arrays_indirect(indirect);
        const auto command = commandBuilder.Finish();
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 1);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        batch.shmHandleCount = 1;
        batch.shmHandles = shm;
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SendDrawElementsIndirect(Uint64 sessionId, uint32_t mode, uint32_t type,
                                  uint64_t shmOffset, MobileGLShmHandle* shm, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr || shm == nullptr) {
            s_lastError = "Client is not initialized or shm missing.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto indirect = MobileGL::Protocol::Wire::CreateDrawElementsIndirect(builder, mode, type, shmOffset);
        MobileGL::Protocol::Wire::CommandBuilder commandBuilder(builder);
        commandBuilder.add_opcode(static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glDrawElementsIndirect));
        commandBuilder.add_session_id(sessionId);
        commandBuilder.add_token(token);
        commandBuilder.add_draw_elements_indirect(indirect);
        const auto command = commandBuilder.Finish();
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 1);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        batch.shmHandleCount = 1;
        batch.shmHandles = shm;
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SendGetString(Uint64 sessionId, uint32_t pname, Uint64 token, String* outString) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto gs = MobileGL::Protocol::Wire::CreateGetString(builder, pname);
        MobileGL::Protocol::Wire::CommandBuilder commandBuilder(builder);
        commandBuilder.add_opcode(static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glGetString));
        commandBuilder.add_session_id(sessionId);
        commandBuilder.add_token(token);
        commandBuilder.add_get_string(gs);
        const auto command = commandBuilder.Finish();
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        if (!WaitResponseForToken(token, 0)) {
            return false;
        }
        if (outString != nullptr) {
            *outString = s_lastResponseString;
        }
        return true;
    }

    Bool SendTextureRespecify(Uint64 sessionId, uint64_t texture, uint32_t level, uint32_t format,
                              uint32_t type, uint32_t width, uint32_t height, uint32_t depth,
                              uint64_t dataSize, MobileGLShmHandle* shm, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto tr = MobileGL::Protocol::Wire::CreateTextureRespecify(
            builder, texture, level, format, type, width, height, depth, dataSize);
        MobileGL::Protocol::Wire::CommandBuilder commandBuilder(builder);
        commandBuilder.add_opcode(static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glTexImage2D));
        commandBuilder.add_session_id(sessionId);
        commandBuilder.add_token(token);
        commandBuilder.add_texture_respecify(tr);
        const auto command = commandBuilder.Finish();
        const auto message =
            MobileGL::Protocol::Wire::CreateMessage(builder, command, shm == nullptr ? 0 : 1);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        batch.shmHandleCount = shm == nullptr ? 0 : 1;
        batch.shmHandles = shm;
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SendTextureSubImage(Uint64 sessionId, uint64_t texture, uint32_t level, uint32_t format,
                             uint32_t type, uint32_t width, uint32_t height, uint32_t depth,
                             uint64_t dataSize, MobileGLShmHandle* shm, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto ts = MobileGL::Protocol::Wire::CreateTextureSubImage(
            builder, texture, level, format, type, width, height, depth, dataSize);
        MobileGL::Protocol::Wire::CommandBuilder commandBuilder(builder);
        commandBuilder.add_opcode(static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glTexSubImage2D));
        commandBuilder.add_session_id(sessionId);
        commandBuilder.add_token(token);
        commandBuilder.add_texture_sub_image(ts);
        const auto command = commandBuilder.Finish();
        const auto message =
            MobileGL::Protocol::Wire::CreateMessage(builder, command, shm == nullptr ? 0 : 1);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        batch.shmHandleCount = shm == nullptr ? 0 : 1;
        batch.shmHandles = shm;
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SendReadPixels(Uint64 sessionId, int32_t x, int32_t y, int32_t width, int32_t height,
                        uint32_t format, uint32_t type, void* outPixels, Uint64 outSize,
                        Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto rp = MobileGL::Protocol::Wire::CreateReadPixels(builder, x, y, width, height, format, type);
        MobileGL::Protocol::Wire::CommandBuilder commandBuilder(builder);
        commandBuilder.add_opcode(static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glReadPixels));
        commandBuilder.add_session_id(sessionId);
        commandBuilder.add_token(token);
        commandBuilder.add_read_pixels(rp);
        const auto command = commandBuilder.Finish();
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        if (!WaitResponseForToken(token, 0)) {
            return false;
        }
        if (s_lastResponseShm.empty() || s_lastResponseShm[0].mappedAddress == nullptr) {
            s_lastError = "Response did not carry pixel data.";
            return false;
        }
        const Uint64 available = s_lastResponseShm[0].size;
        if (outPixels == nullptr || outSize > available) {
            s_lastError = "Pixel buffer too small for server response.";
            return false;
        }
        Memcpy(outPixels, s_lastResponseShm[0].mappedAddress, static_cast<SizeT>(outSize));
        return true;
    }

    Bool SendBufferReadbackFromGpu(Uint64 sessionId, uint64_t bufferHandle, uint64_t offset,
                                   uint64_t size, void* outBytes, Uint64 outSize, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto rb = MobileGL::Protocol::Wire::CreateBufferReadbackFromGpu(builder, bufferHandle, offset, size);
        MobileGL::Protocol::Wire::CommandBuilder commandBuilder(builder);
        commandBuilder.add_opcode(static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glGetBufferSubData));
        commandBuilder.add_session_id(sessionId);
        commandBuilder.add_token(token);
        commandBuilder.add_buffer_readback_from_gpu(rb);
        const auto command = commandBuilder.Finish();
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        if (!WaitResponseForToken(token, 0)) {
            return false;
        }
        if (s_lastResponseShm.empty() || s_lastResponseShm[0].mappedAddress == nullptr) {
            s_lastError = "Response did not carry buffer data.";
            return false;
        }
        const Uint64 available = s_lastResponseShm[0].size;
        if (outBytes == nullptr || outSize > available) {
            s_lastError = "Buffer output buffer too small for server response.";
            return false;
        }
        Memcpy(outBytes, s_lastResponseShm[0].mappedAddress, static_cast<SizeT>(outSize));
        return true;
    }

    Bool MapBufferRange(Uint64 sessionId, uint64_t bufferHandle, uint64_t offset,
                        uint64_t size, uint32_t access, Uint64 token, void** outMappedPtr) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        if (s_mappedActive) {
            s_lastError = "A buffer range is already mapped.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto mr = MobileGL::Protocol::Wire::CreateMapBufferRange(builder, bufferHandle, offset, size, access);
        MobileGL::Protocol::Wire::CommandBuilder commandBuilder(builder);
        commandBuilder.add_opcode(static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glMapBufferRange));
        commandBuilder.add_session_id(sessionId);
        commandBuilder.add_token(token);
        commandBuilder.add_map_buffer_range(mr);
        const auto command = commandBuilder.Finish();
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        if (!WaitResponseForToken(token, 0)) {
            return false;
        }
        if (s_lastResponseShm.empty() || s_lastResponseShm[0].mappedAddress == nullptr ||
            s_lastResponseShm[0].size < size) {
            s_lastError = "Response did not carry a large enough mapping.";
            return false;
        }
        s_mappedShm = s_lastResponseShm[0];
        s_lastResponseShm.clear();
        s_mappedBuffer = bufferHandle;
        s_mappedOffset = offset;
        s_mappedSize = size;
        s_mappedPtr = s_mappedShm.mappedAddress;
        s_mappedActive = true;
        if (outMappedPtr != nullptr) {
            *outMappedPtr = s_mappedPtr;
        }
        return true;
    }

    Bool UnmapBuffer(Uint64 sessionId, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        if (!s_mappedActive || s_mappedPtr == nullptr || s_ops->OpenSharedMemory == nullptr) {
            s_lastError = "No active buffer mapping.";
            return false;
        }
        MobileGLShmHandle flush{};
        if (!s_ops->OpenSharedMemory(s_transport, &flush) || flush.mappedAddress == nullptr) {
            s_lastError = "Failed to allocate unmap flush shm.";
            return false;
        }
        Memcpy(flush.mappedAddress, s_mappedPtr, static_cast<SizeT>(s_mappedSize));

        flatbuffers::FlatBufferBuilder builder;
        const auto um = MobileGL::Protocol::Wire::CreateUnmapBuffer(builder, s_mappedBuffer, s_mappedOffset,
                                                                    s_mappedSize);
        MobileGL::Protocol::Wire::CommandBuilder commandBuilder(builder);
        commandBuilder.add_opcode(static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::glUnmapBuffer));
        commandBuilder.add_session_id(sessionId);
        commandBuilder.add_token(token);
        commandBuilder.add_unmap_buffer(um);
        const auto command = commandBuilder.Finish();
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 1);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        batch.shmHandleCount = 1;
        batch.shmHandles = &flush;
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            s_ops->ReleaseSharedMemory(s_transport, &flush);
            return false;
        }
        if (!WaitResponseForToken(token, 0)) {
            s_ops->ReleaseSharedMemory(s_transport, &flush);
            return false;
        }
        s_ops->ReleaseSharedMemory(s_transport, &flush);
        s_ops->ReleaseSharedMemory(s_transport, &s_mappedShm);
        s_mappedActive = false;
        s_mappedPtr = nullptr;
        s_mappedBuffer = 0;
        s_mappedOffset = 0;
        s_mappedSize = 0;
        s_mappedShm = {};
        return true;
    }

    Bool SendEglCreatePbufferSurface(Uint64 displayId, uint64_t surface, int32_t width,
                                     int32_t height, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto ps = MobileGL::Protocol::Wire::CreateEglCreatePbufferSurface(builder, displayId, surface, width, height);
        MobileGL::Protocol::Wire::CommandBuilder commandBuilder(builder);
        commandBuilder.add_opcode(static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::eglCreatePbufferSurface));
        commandBuilder.add_session_id(displayId);
        commandBuilder.add_token(token);
        commandBuilder.add_egl_create_pbuffer_surface(ps);
        const auto command = commandBuilder.Finish();
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SendEglDestroySurface(Uint64 displayId, uint64_t surface, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto ds = MobileGL::Protocol::Wire::CreateEglDestroySurface(builder, displayId, surface);
        MobileGL::Protocol::Wire::CommandBuilder commandBuilder(builder);
        commandBuilder.add_opcode(static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::eglDestroySurface));
        commandBuilder.add_session_id(displayId);
        commandBuilder.add_token(token);
        commandBuilder.add_egl_destroy_surface(ds);
        const auto command = commandBuilder.Finish();
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SendEglMakeCurrent(Uint64 sessionId, uint64_t draw, uint64_t read, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto mc = MobileGL::Protocol::Wire::CreateEglMakeCurrent(builder, sessionId, draw, read);
        MobileGL::Protocol::Wire::CommandBuilder commandBuilder(builder);
        commandBuilder.add_opcode(static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::eglMakeCurrent));
        commandBuilder.add_session_id(sessionId);
        commandBuilder.add_token(token);
        commandBuilder.add_egl_make_current(mc);
        const auto command = commandBuilder.Finish();
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SendEglSetSwapInterval(Uint64 sessionId, int32_t interval, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto si = MobileGL::Protocol::Wire::CreateEglSwapInterval(builder, interval);
        MobileGL::Protocol::Wire::CommandBuilder commandBuilder(builder);
        commandBuilder.add_opcode(static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::eglSwapInterval));
        commandBuilder.add_session_id(sessionId);
        commandBuilder.add_token(token);
        commandBuilder.add_egl_swap_interval(si);
        const auto command = commandBuilder.Finish();
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SendEglResizeSurface(Uint64 displayId, uint64_t surface, uint32_t width,
                              uint32_t height, Uint64 token) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        flatbuffers::FlatBufferBuilder builder;
        const auto rs = MobileGL::Protocol::Wire::CreateEglResizeSurface(builder, displayId, surface, width, height);
        MobileGL::Protocol::Wire::CommandBuilder commandBuilder(builder);
        commandBuilder.add_opcode(static_cast<uint32_t>(MobileGL::Protocol::MobileGLOpcode::eglSurfaceAttrib));
        commandBuilder.add_session_id(displayId);
        commandBuilder.add_token(token);
        commandBuilder.add_egl_resize_surface(rs);
        const auto command = commandBuilder.Finish();
        const auto message = MobileGL::Protocol::Wire::CreateMessage(builder, command, 0);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    Bool SubmitCommand(Uint32 sessionId, Uint32 opcode, Uint64 token) {
        return SubmitDataCommand(sessionId, opcode, token, 0, 0, nullptr);
    }

    Bool SubmitDataCommand(Uint32 sessionId, Uint32 opcode, Uint64 token,
                           Uint64 shmOffset, Uint64 shmSize, MobileGLShmHandle* shm) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }

        flatbuffers::FlatBufferBuilder builder;
        const auto clear = MobileGL::Protocol::Wire::CreateGlClear(builder, 0);
        flatbuffers::Offset<MobileGL::Protocol::Wire::DataBlob> data;
        if (shm != nullptr) {
            data = MobileGL::Protocol::Wire::CreateDataBlob(builder, shmOffset, shmSize);
        }
        const auto command = MobileGL::Protocol::Wire::CreateCommand(
            builder, opcode, sessionId, token, clear, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, data);
        const auto message =
            MobileGL::Protocol::Wire::CreateMessage(builder, command, shm == nullptr ? 0 : 1);
        builder.Finish(message);

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = builder.GetBufferPointer();
        batch.flatBufferSize = static_cast<Uint32>(builder.GetSize());
        batch.shmHandleCount = shm == nullptr ? 0 : 1;
        batch.shmHandles = shm;
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        s_lastError.clear();
        return true;
    }

    Bool SubmitSessionControl(Uint64 sessionId, Bool create, Uint64 token) {
        const uint32_t opcode = create
            ? static_cast<uint32_t>(MobileGL::Protocol::MobileGLControlOpcode::SessionCreate)
            : static_cast<uint32_t>(MobileGL::Protocol::MobileGLControlOpcode::SessionDestroy);
        return SubmitCommand(static_cast<Uint32>(sessionId), opcode, token);
    }

    Bool SubmitDisplayControl(Uint64 displayId, Bool create, Uint64 token) {
        const uint32_t opcode = create
            ? static_cast<uint32_t>(MobileGL::Protocol::MobileGLControlOpcode::DisplayCreate)
            : static_cast<uint32_t>(MobileGL::Protocol::MobileGLControlOpcode::DisplayDestroy);
        return SubmitCommand(static_cast<Uint32>(displayId), opcode, token);
    }

    Bool SubmitSharedGroupControl(Uint64 groupId, Bool create, Uint64 token) {
        const uint32_t opcode = create
            ? static_cast<uint32_t>(MobileGL::Protocol::MobileGLControlOpcode::SharedGroupCreate)
            : static_cast<uint32_t>(MobileGL::Protocol::MobileGLControlOpcode::SharedGroupDestroy);
        return SubmitCommand(static_cast<Uint32>(groupId), opcode, token);
    }

    Bool WaitResponseForToken(Uint64 token, Uint32 timeoutMs) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }
        for (auto& handle : s_lastResponseShm) {
            if (s_ops->ReleaseSharedMemory != nullptr) {
                s_ops->ReleaseSharedMemory(s_transport, &handle);
            }
        }
        s_lastResponseShm.clear();

        MobileGLResponseQueue response{};
        if (!s_ops->WaitResponses(s_transport, &response, timeoutMs)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }

        const auto* parsed =
            flatbuffers::GetRoot<MobileGL::Protocol::Wire::Response>(response.flatBufferData);
        if (parsed == nullptr) {
            s_lastError = "Invalid response.";
            return false;
        }
        if (parsed->token() != token) {
            s_lastError = "Response token mismatch.";
            return false;
        }
        s_lastResponseByte = parsed->data_byte();
        s_lastResponseSync = parsed->sync();
        s_lastResponseQueryNs = parsed->query_ns();
        if (parsed->string_value() != nullptr) {
            s_lastResponseString = parsed->string_value()->str();
        } else {
            s_lastResponseString.clear();
        }
        if (response.shmHandleCount > 0 && response.shmHandles != nullptr) {
            s_lastResponseShm.reserve(response.shmHandleCount);
            for (Uint32 i = 0; i < response.shmHandleCount; ++i) {
                s_lastResponseShm.push_back(response.shmHandles[i]);
            }
        }
        s_lastError.clear();
        return parsed->status() == 0;
    }

    Uint64 GetLastResponseSync() {
        return s_lastResponseSync;
    }

    Uint64 GetLastResponseQueryNs() {
        return s_lastResponseQueryNs;
    }

    const String& GetLastResponseString() {
        return s_lastResponseString;
    }

    Uint32 GetLastResponseDataByte() {
        return s_lastResponseByte;
    }

    Bool SendCommand(Uint32 sessionId, Uint32 opcode, Uint64 token) {
        if (!SubmitCommand(sessionId, opcode, token)) {
            return false;
        }
        return WaitResponseForToken(token, 0);
    }

    void Shutdown() {
        for (auto& handle : s_lastResponseShm) {
            if (s_ops != nullptr && s_ops->ReleaseSharedMemory != nullptr) {
                s_ops->ReleaseSharedMemory(s_transport, &handle);
            }
        }
        s_lastResponseShm.clear();
        if (s_mappedActive && s_ops != nullptr && s_ops->ReleaseSharedMemory != nullptr) {
            s_ops->ReleaseSharedMemory(s_transport, &s_mappedShm);
        }
        s_mappedActive = false;
        s_mappedPtr = nullptr;
        s_mappedBuffer = 0;
        s_mappedOffset = 0;
        s_mappedSize = 0;
        s_mappedShm = {};
        if (s_transport != nullptr && s_ownsTransport) {
            Transport::DestroyLocalSocketShmTransport(s_transport);
        }
        s_transport = nullptr;
        s_ops = nullptr;
        s_ownsTransport = false;
        s_initialized = false;
    }

    const String& GetLastError() {
        return s_lastError;
    }
} // namespace MobileGL::Client

// End of File
