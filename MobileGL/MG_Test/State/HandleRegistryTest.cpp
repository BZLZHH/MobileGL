// MobileGL - MobileGL/MG_Test/State/HandleRegistryTest.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include <Includes.h>
#include <gtest/gtest.h>
#include "MG_State/GLState/HandleRegistry.h"
#include "MG_State/GLState/Core.h"

namespace MobileGL::MG_State::GLState {
    TEST(HandleRegistryTest, AllocatesStableHandleForKey) {
        const Uint64 groupId = 7;
        const Uint32 glName = 3;

        const auto first = ObjectHandleRegistry::Allocate(
            ObjectHandleScope::SharedGroup, groupId, MobileGLObjectKindBuffer, glName);
        EXPECT_NE(first, MobileGLBackendHandle(0));

        const auto second = ObjectHandleRegistry::Allocate(
            ObjectHandleScope::SharedGroup, groupId, MobileGLObjectKindBuffer, glName);
        EXPECT_EQ(first, second);

        const auto lookedUp = ObjectHandleRegistry::Lookup(
            ObjectHandleScope::SharedGroup, groupId, MobileGLObjectKindBuffer, glName);
        EXPECT_EQ(lookedUp, first);
    }

    TEST(HandleRegistryTest, DifferentScopesDoNotCollide) {
        const Uint64 groupId = 8;
        const Uint64 sessionId = 9;
        const Uint32 glName = 5;

        const auto shared = ObjectHandleRegistry::Allocate(
            ObjectHandleScope::SharedGroup, groupId, MobileGLObjectKindTexture, glName);
        const auto privateHandle = ObjectHandleRegistry::Allocate(
            ObjectHandleScope::Session, sessionId, MobileGLObjectKindQuery, glName);
        EXPECT_NE(shared, privateHandle);
    }

    TEST(HandleRegistryTest, LookupMissesReturnZero) {
        EXPECT_EQ(ObjectHandleRegistry::Lookup(
                      ObjectHandleScope::SharedGroup, 12345, MobileGLObjectKindBuffer, 99),
                  MobileGLBackendHandle(0));
    }

    TEST(HandleRegistryTest, DestroyRemovesEntry) {
        const Uint64 groupId = 10;
        const Uint32 glName = 11;
        const auto handle = ObjectHandleRegistry::Allocate(
            ObjectHandleScope::SharedGroup, groupId, MobileGLObjectKindBuffer, glName);

        const SizeT countBefore = ObjectHandleRegistry::GetCount();
        ObjectHandleRegistry::Destroy(handle);
        EXPECT_EQ(ObjectHandleRegistry::GetCount(), countBefore - 1);
        EXPECT_EQ(ObjectHandleRegistry::Get(handle), nullptr);
    }

    TEST(HandleRegistryTest, GLContextAllocatesSharedAndSessionHandles) {
        GLContext context;
        context.SetSharedGroupId(20);
        context.SetSessionId(21);

        const Uint64 bufferHandle = context.GetObjectHandle(
            static_cast<Uint32>(MobileGLObjectKindBuffer), 1);
        const Uint64 queryHandle = context.GetObjectHandle(
            static_cast<Uint32>(MobileGLObjectKindQuery), 2);

        EXPECT_NE(bufferHandle, Uint64(0));
        EXPECT_NE(queryHandle, Uint64(0));
        ASSERT_NE(ObjectHandleRegistry::Get(bufferHandle), nullptr);
        ASSERT_NE(ObjectHandleRegistry::Get(queryHandle), nullptr);
        EXPECT_EQ(ObjectHandleRegistry::Get(bufferHandle)->ScopeId, 20);
        EXPECT_EQ(ObjectHandleRegistry::Get(queryHandle)->ScopeId, 21);
    }
} // namespace MobileGL::MG_State::GLState

// End of File
