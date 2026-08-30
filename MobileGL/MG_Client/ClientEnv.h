// MobileGL - MobileGL/MG_Client/ClientEnv.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#pragma once

#include <Includes.h>

namespace MobileGL::Client {
    // Runtime selection driven by the MOBILEGL_CS_* environment variables.
    //
    // FCL zero-change requirement: FCL only knows how to dlopen a renderer
    // plugin and forward environment variables, so every integration choice
    // (auto-host a FullServer here vs. connect to an existing one, which
    // backend plugin to load, which sibling libraries to use) is expressed
    // as an environment variable instead of a launcher API.

    enum class RuntimeMode : Uint32 {
        // Auto-host a FullServer instance in this process (default). The
        // client locates MobileGL_FullServer.so next to itself and starts it
        // through the in-process transport pair.
        InProcess = 0,
        // Connect to an already-running FullServer at RuntimeConfig::Endpoint.
        Connect = 1
    };

    struct RuntimeConfig {
        RuntimeMode Mode = RuntimeMode::InProcess;
        String Endpoint;          // MOBILEGL_CS_ENDPOINT (Connect mode only)
        String Backend;           // MOBILEGL_CS_BACKEND, e.g. "DirectGLES"
        String ServerUtilPath;    // MOBILEGL_CS_SERVER_UTIL (empty = auto)
        String ServerBackendPath; // MOBILEGL_CS_SERVER_BACKEND (empty = auto)
        Bool Debug = false;       // MOBILEGL_CS_DEBUG=1
    };

    // Reads MOBILEGL_CS_MODE / _ENDPOINT / _BACKEND / _SERVER_UTIL /
    // _SERVER_BACKEND / _DEBUG with safe defaults (in-process + DirectGLES).
    RuntimeConfig LoadRuntimeConfig();
} // namespace MobileGL::Client

// End of File
