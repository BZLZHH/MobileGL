// MobileGL - MobileGL/MG_FullServer/FullServerEntry.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#pragma once

#include <Includes.h>
#include "MG_Protocol/bfa.h"
#include "MG_Protocol/transport.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle to one FullServer instance (runtime + backend + transport).
typedef void* MobileGLFullServerHandle;

// Creates a FullServer instance: loads MobileGL_UtilRuntime.so and one
// BackendObject plugin, negotiates the ABI and creates the backend object.
// Returns nullptr on any failure. The caller owns the handle.
MobileGLFullServerHandle mobilegl_fullserver_create(const char* utilRuntimePath,
                                                    const char* backendPath);

// Initializes the backend vtable (Initialize).
int mobilegl_fullserver_start(MobileGLFullServerHandle handle);

// Attaches one transport endpoint. The caller keeps ownership of transport
// and ops; they must outlive the handle.
int mobilegl_fullserver_attach_transport(MobileGLFullServerHandle handle,
                                         const MobileGLTransportOps* ops,
                                         MobileGLTransport* transport);

// Services one batch of commands from the attached transport.
int mobilegl_fullserver_service_once(MobileGLFullServerHandle handle);

// Convenience host loop: listen on a LocalSocketShm endpoint, accept one
// connection, service up to maxCommands commands, then release the endpoint.
// Returns 0 when all requested commands were serviced.
int mobilegl_fullserver_run_socket(MobileGLFullServerHandle handle,
                                   const char* endpoint,
                                   uint32_t maxCommands);

// Shuts down and frees the handle.
void mobilegl_fullserver_destroy(MobileGLFullServerHandle handle);

#ifdef __cplusplus
} // extern "C"
#endif

// End of File
