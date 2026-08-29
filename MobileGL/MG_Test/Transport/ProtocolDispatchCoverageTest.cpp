// MobileGL - MobileGL/MG_Test/Transport/ProtocolDispatchCoverageTest.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include <Includes.h>
#include <gtest/gtest.h>
#include "MG_Client/generated_wire_client.h"
#include "MG_Protocol/generated_dispatch.h"
#include "MG_Protocol/generated_opcodes.h"
#include "MG_Protocol/generated_wire_dispatch.h"

namespace MobileGL::Protocol::Wire {
    TEST(ProtocolDispatchCoverageTest, AllSourceListOpcodesAreRegistered) {
        EXPECT_EQ(kWireDispatchTotal, kMobileGLDispatchCount);
        EXPECT_EQ(kWireDispatchSupported + kWireDispatchUnsupported, kWireDispatchTotal);
        EXPECT_GT(kWireDispatchSupported, 0u);

        for (uint32_t i = 0; i < kMobileGLDispatchCount; ++i) {
            const auto& entry = kMobileGLDispatch[i];
            EXPECT_STREQ(WireDispatchApi(entry.opcode), entry.api);
            EXPECT_STREQ(WireDispatchPayloadTable(entry.opcode), entry.payloadTable);
            // Every opcode answers the coverage query without crashing; the
            // supported/unsupported split is the honest gap report.
            (void)WireDispatchIsSupported(entry.opcode);
        }
    }

    TEST(ProtocolDispatchCoverageTest, ClearAndStateCommandsAreSupported) {
        const uint32_t clearOpcode = static_cast<uint32_t>(MobileGLOpcode::glClear);
        const uint32_t clearColorOpcode = static_cast<uint32_t>(MobileGLOpcode::glClearColor);
        EXPECT_TRUE(WireDispatchIsSupported(clearOpcode));
        EXPECT_TRUE(WireDispatchIsSupported(clearColorOpcode));
        EXPECT_STREQ(WireDispatchApi(clearOpcode), "glClear");
    }

    TEST(ProtocolDispatchCoverageTest, ClientWireWrappersAreGenerated) {
        EXPECT_GT(MobileGL::Client::Wire::kWireClientWrappedCount, 0u);
        EXPECT_LE(MobileGL::Client::Wire::kWireClientWrappedCount, kWireDispatchSupported);
    }
} // namespace MobileGL::Protocol::Wire

// End of File
