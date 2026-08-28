// MobileGL - MobileGL/MG_Client/Client.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "Client.h"
#include "MG_Transport/LocalSocketShmTransport.h"

namespace MobileGL::Client {
    namespace {
        // Minimal command/response envelope; layout must match
        // MobileGL::FullServer::CommandHeader / ResponseHeader.
        struct ClientCommandHeader {
            Uint32 Opcode;
            Uint32 SessionId;
        };

        struct ClientResponseHeader {
            Uint32 Status; // 0 = OK
        };

        String s_lastError;
        Bool s_initialized = false;
        Bool s_ownsTransport = false;
        MobileGLTransport* s_transport = nullptr;
        const MobileGLTransportOps* s_ops = nullptr;
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

    Bool SendCommand(Uint32 sessionId, Uint32 opcode) {
        if (!s_initialized || s_transport == nullptr || s_ops == nullptr) {
            s_lastError = "Client is not initialized.";
            return false;
        }

        ClientCommandHeader command{};
        command.Opcode = opcode;
        command.SessionId = sessionId;

        MobileGLCommandBatch batch{};
        batch.structSize = sizeof(MobileGLCommandBatch);
        batch.flatBufferData = &command;
        batch.flatBufferSize = static_cast<Uint32>(sizeof(command));
        if (!s_ops->SubmitCommands(s_transport, &batch)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }

        MobileGLResponseQueue response{};
        if (!s_ops->WaitResponses(s_transport, &response, 0)) {
            s_lastError = s_ops->GetLastError(s_transport);
            return false;
        }
        if (response.flatBufferSize < sizeof(ClientResponseHeader)) {
            s_lastError = "Short response.";
            return false;
        }

        ClientResponseHeader header{};
        memcpy(&header, response.flatBufferData, sizeof(header));
        s_lastError.clear();
        return header.Status == 0;
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
