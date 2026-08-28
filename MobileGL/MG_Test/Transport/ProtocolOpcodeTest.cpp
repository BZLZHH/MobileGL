// MobileGL - MobileGL/MG_Test/Transport/ProtocolOpcodeTest.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include <Includes.h>
#include <gtest/gtest.h>
#include "MG_Protocol/generated_opcodes.h"

namespace MobileGL::Protocol {
    TEST(ProtocolOpcodeTest, GeneratedCountAndLookup) {
        EXPECT_GT(kMobileGLOpcodeCount, 0u);
        EXPECT_GT(static_cast<uint32_t>(MobileGLOpcode::glClear), 0u);
        EXPECT_STREQ(MobileGLOpcodeName(MobileGLOpcode::glClear), "glClear");
    }
} // namespace MobileGL::Protocol

// End of File
