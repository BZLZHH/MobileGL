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
            sessionId, token, 0, 0, 0, 0, 0, 0, pp, 0);
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
            sessionId, token, 0, 0, 0, 0, 0, 0, 0, gm, 0);
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
            builder, opcode, sessionId, token, clear, 0, 0, 0, 0, 0, 0, 0, data);
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
        s_lastError.clear();
        return parsed->status() == 0;
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
