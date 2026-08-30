// MobileGL - MobileGL/MG_Client/ClientEnv.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "ClientEnv.h"

#include <cstdlib>
#include <cstring>

namespace MobileGL::Client {
    namespace {
        const char* GetEnv(const char* name) {
            if (name == nullptr) return nullptr;
            return std::getenv(name);
        }

        Bool IsFlagSet(const char* name) {
            const char* value = GetEnv(name);
            return value != nullptr && (std::strcmp(value, "1") == 0 ||
                                        std::strcmp(value, "true") == 0 ||
                                        std::strcmp(value, "yes") == 0 ||
                                        std::strcmp(value, "on") == 0);
        }
    } // namespace

    RuntimeConfig LoadRuntimeConfig() {
        RuntimeConfig config;

        const char* mode = GetEnv("MOBILEGL_CS_MODE");
        if (mode != nullptr && std::strcmp(mode, "connect") == 0) {
            config.Mode = RuntimeMode::Connect;
        } else if (mode != nullptr && std::strcmp(mode, "inprocess") == 0) {
            config.Mode = RuntimeMode::InProcess;
        }
        // Unknown values keep the safe default (in-process).

        const char* endpoint = GetEnv("MOBILEGL_CS_ENDPOINT");
        if (endpoint != nullptr && endpoint[0] != '\0') {
            config.Endpoint = String(endpoint);
        }

        const char* backend = GetEnv("MOBILEGL_CS_BACKEND");
        if (backend != nullptr && backend[0] != '\0') {
            config.Backend = String(backend);
        } else {
            config.Backend = String("DirectGLES");
        }

        const char* serverUtil = GetEnv("MOBILEGL_CS_SERVER_UTIL");
        if (serverUtil != nullptr && serverUtil[0] != '\0') {
            config.ServerUtilPath = String(serverUtil);
        }

        const char* serverBackend = GetEnv("MOBILEGL_CS_SERVER_BACKEND");
        if (serverBackend != nullptr && serverBackend[0] != '\0') {
            config.ServerBackendPath = String(serverBackend);
        }

        config.Debug = IsFlagSet("MOBILEGL_CS_DEBUG");
        return config;
    }
} // namespace MobileGL::Client

// End of File
