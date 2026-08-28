// MobileGL - MobileGL/Transport/InProcessTransport.h
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
    // In-process transport: a paired client/server that exchanges command and
    // response byte blobs directly in memory. Used for tests and as the
    // reference implementation of the C/S transport contract.
    void CreateInProcessTransportPair(MobileGLTransport** client, MobileGLTransport** server);
    void DestroyInProcessTransport(MobileGLTransport* t);
    const MobileGLTransportOps& GetInProcessTransportOps();
} // namespace MobileGL::Transport

// End of File
