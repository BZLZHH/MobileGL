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
        String s_lastError;
        Bool s_initialized = false;
        MobileGLTransport* s_transport = nullptr;
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

        s_initialized = true;
        s_lastError.clear();
        return true;
    }

    void Shutdown() {
        if (s_transport != nullptr) {
            Transport::DestroyLocalSocketShmTransport(s_transport);
            s_transport = nullptr;
        }
        s_initialized = false;
    }

    const String& GetLastError() {
        return s_lastError;
    }
} // namespace MobileGL::Client

// End of File
