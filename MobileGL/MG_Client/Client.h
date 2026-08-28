// MobileGL - MobileGL/MG_Client/Client.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#pragma once

#include <Includes.h>

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
    void Shutdown();
    const String& GetLastError();
} // namespace MobileGL::Client

// End of File
