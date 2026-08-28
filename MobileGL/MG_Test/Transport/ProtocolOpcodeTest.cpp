// MobileGL - MobileGL/MG_Test/Transport/ProtocolOpcodeTest.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include <Includes.h>
#include <gtest/gtest.h>
#include "MG_Protocol/generated_dispatch.h"
#include "MG_Protocol/generated_opcodes.h"

namespace MobileGL::Protocol {
    TEST(ProtocolOpcodeTest, GeneratedCountAndLookup) {
        EXPECT_GT(kMobileGLOpcodeCount, 0u);
        EXPECT_GT(static_cast<uint32_t>(MobileGLOpcode::glClear), 0u);
        EXPECT_STREQ(MobileGLOpcodeName(MobileGLOpcode::glClear), "glClear");
    }

    TEST(ProtocolOpcodeTest, DispatchMetadataMapsOpcodeToApi) {
        EXPECT_EQ(kMobileGLDispatchCount, kMobileGLOpcodeCount);
        const uint32_t clear = static_cast<uint32_t>(MobileGLOpcode::glClear);
        EXPECT_STREQ(MobileGLDispatchApi(clear), "glClear");
        bool foundTable = false;
        for (uint32_t i = 0; i < kMobileGLDispatchCount; ++i) {
            if (kMobileGLDispatch[i].opcode == clear) {
                EXPECT_STREQ(kMobileGLDispatch[i].payloadTable, "GlClear");
                foundTable = true;
                break;
            }
        }
        EXPECT_TRUE(foundTable);
    }
} // namespace MobileGL::Protocol

// End of File
