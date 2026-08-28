// MobileGL - MobileGL/Transport/LocalSocketShmTransport.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#pragma once

#include <Includes.h>
#include "Protocol/transport.h"

namespace MobileGL::Transport {
    // v1 transport: Unix domain socket (POSIX) / named pipe (Windows) plus a
    // server-managed shared-memory arena. Create returns the opaque
    // MobileGLTransport* consumed by the MobileGLTransportOps table.
    MobileGLTransport* CreateLocalSocketShmTransport();
    void DestroyLocalSocketShmTransport(MobileGLTransport* t);
    const MobileGLTransportOps& GetLocalSocketShmTransportOps();
} // namespace MobileGL::Transport

// End of File
