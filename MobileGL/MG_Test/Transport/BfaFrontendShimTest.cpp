// MobileGL - MobileGL/MG_Test/Transport/BfaFrontendShimTest.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include <Includes.h>
#include <gtest/gtest.h>
#include "MG_Backend/BackendObject.h"
#include "MG_FullServer/BfaFrontendShim.h"

struct MobileGLBackend {
    uint32_t Nonce = 0;
};

namespace MobileGL::MG_Backend {
    GlobalBackendFunctionsTable gBackendFunctionsTable;
} // namespace MobileGL::MG_Backend

namespace {
    MobileGLSessionId g_session = 0;
    uint32_t g_mask = 0;
    uint32_t g_mode = 0;
    int32_t g_first = 0, g_count = 0;
    const void* g_indices = nullptr;
    uint32_t g_primcount = 0;
    uint32_t g_start = 0, g_end = 0, g_type = 0;
    uint32_t g_barriers = 0, g_flags = 0;
    uint64_t g_timeout = 0;
    void* g_deletedSync = nullptr;

    void C_Clear(MobileGLBackend* backend, MobileGLSessionId session, uint32_t mask) {
        (void)backend;
        g_session = session;
        g_mask = mask;
    }

    void C_DrawArrays(MobileGLBackend* backend, MobileGLSessionId session, uint32_t mode,
                      int32_t first, int32_t count) {
        (void)backend;
        g_session = session;
        g_mode = mode;
        g_first = first;
        g_count = count;
    }

    void C_DrawElements(MobileGLBackend* backend, MobileGLSessionId session, uint32_t mode,
                        int32_t count, uint32_t type, const void* indices) {
        (void)backend;
        g_session = session;
        g_mode = mode;
        g_count = count;
        g_type = type;
        g_indices = indices;
    }

    void C_DrawArraysInstanced(MobileGLBackend* backend, MobileGLSessionId session, uint32_t mode,
                               int32_t first, int32_t count, int32_t instanceCount) {
        (void)backend;
        g_session = session;
        g_mode = mode;
        g_first = first;
        g_count = count;
        g_primcount = static_cast<uint32_t>(instanceCount);
    }

    void C_DrawElementsInstanced(MobileGLBackend* backend, MobileGLSessionId session,
                                 uint32_t mode, int32_t count, uint32_t type,
                                 const void* indices, int32_t instanceCount) {
        (void)backend;
        g_session = session;
        g_mode = mode;
        g_count = count;
        g_type = type;
        g_indices = indices;
        g_primcount = static_cast<uint32_t>(instanceCount);
    }

    void C_DrawRangeElements(MobileGLBackend* backend, MobileGLSessionId session, uint32_t mode,
                             uint32_t start, uint32_t end, int32_t count, uint32_t type,
                             const void* indices) {
        (void)backend;
        g_session = session;
        g_mode = mode;
        g_start = start;
        g_end = end;
        g_count = count;
        g_type = type;
        g_indices = indices;
    }

    void C_MemoryBarrier(MobileGLBackend* backend, MobileGLSessionId session, uint32_t barriers) {
        (void)backend;
        g_session = session;
        g_barriers = barriers;
    }

    void* C_FenceSync(MobileGLBackend* backend, MobileGLSessionId session,
                      uint32_t condition, uint32_t flags) {
        (void)backend;
        (void)condition;
        (void)flags;
        g_session = session;
        return reinterpret_cast<void*>(0x1234);
    }

    void C_DeleteSync(MobileGLBackend* backend, MobileGLSessionId session, void* sync) {
        (void)backend;
        g_session = session;
        g_deletedSync = sync;
    }

    void C_WaitSync(MobileGLBackend* backend, MobileGLSessionId session, void* sync,
                    uint32_t flags, uint64_t timeout) {
        (void)backend;
        (void)sync;
        g_session = session;
        g_flags = flags;
        g_timeout = timeout;
    }
} // namespace

namespace MobileGL::Transport {
    TEST(BfaFrontendShimTest, ForwardsToBfaVTableWithCurrentSession) {
        g_session = 0;
        g_mask = 0;
        g_mode = 0;
        g_first = 0;
        g_count = 0;
        g_indices = nullptr;
        g_primcount = 0;
        g_start = 0;
        g_end = 0;
        g_type = 0;
        g_barriers = 0;
        g_flags = 0;
        g_timeout = 0;
        g_deletedSync = nullptr;

        MobileGLBackend backend{};
        MobileGLBackendVTable vtable{};
        vtable.structSize = sizeof(MobileGLBackendVTable);
        vtable.apiVersion = (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR;
        vtable.Clear = &C_Clear;
        vtable.DrawArrays = &C_DrawArrays;
        vtable.DrawElements = &C_DrawElements;
        vtable.DrawArraysInstanced = &C_DrawArraysInstanced;
        vtable.DrawElementsInstanced = &C_DrawElementsInstanced;
        vtable.DrawRangeElements = &C_DrawRangeElements;
        vtable.MemoryBarrier = &C_MemoryBarrier;
        vtable.FenceSync = &C_FenceSync;
        vtable.DeleteSync = &C_DeleteSync;
        vtable.WaitSync = &C_WaitSync;

        MobileGL::MG_Backend::GlobalBackendFunctionsTable table{};
        MobileGL::FullServer::BfaFrontendShim& shim = MobileGL::FullServer::BfaFrontendShim::Get();
        shim.Install(&backend, &vtable, &table);
        shim.SetCurrentSession(42);

        ASSERT_NE(table.GL.Clear, nullptr);
        table.GL.Clear(0x11);
        EXPECT_EQ(g_session, 42u);
        EXPECT_EQ(g_mask, 0x11u);

        table.GL.DrawArrays(4, 3, 6);
        EXPECT_EQ(g_session, 42u);
        EXPECT_EQ(g_mode, 4u);
        EXPECT_EQ(g_first, 3);
        EXPECT_EQ(g_count, 6);

        table.GL.DrawElements(4, 3, 5, reinterpret_cast<const void*>(0x77));
        EXPECT_EQ(g_session, 42u);
        EXPECT_EQ(g_mode, 4u);
        EXPECT_EQ(g_count, 3);
        EXPECT_EQ(g_type, 5u);
        EXPECT_EQ(g_indices, reinterpret_cast<const void*>(0x77));

        table.GL.DrawArraysInstanced(4, 3, 6, 2);
        EXPECT_EQ(g_primcount, 2u);

        table.GL.DrawRangeElements(4, 2, 5, 3, 5, reinterpret_cast<const void*>(0x88));
        EXPECT_EQ(g_start, 2u);
        EXPECT_EQ(g_end, 5u);
        EXPECT_EQ(g_indices, reinterpret_cast<const void*>(0x88));

        table.GL.MemoryBarrier(0x1010);
        EXPECT_EQ(g_barriers, 0x1010u);

        const MobileGL::MG_Backend::BackendSyncHandle sync = table.GL.FenceSync();
        EXPECT_EQ(sync, reinterpret_cast<void*>(0x1234));
        table.GL.DeleteSync(sync);
        EXPECT_EQ(g_deletedSync, reinterpret_cast<void*>(0x1234));
        table.GL.WaitSync(sync, 0x33, 0x55);
        EXPECT_EQ(g_flags, 0x33u);
        EXPECT_EQ(g_timeout, 0x55u);

        shim.Clear();
    }
} // namespace MobileGL::Transport

// End of File
