// MobileGL - MobileGL/MG_Test/State/ContextRegistryTest.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include <Includes.h>
#include <gtest/gtest.h>
#include "MG_State/GLState/ContextRegistry.h"

namespace MobileGL::MG_State::GLState {
    TEST(ContextRegistryTest, CreatesSharedGroupAndSession) {
        const DisplayId displayId = 1;
        const SharedGroupId groupId = GLContextRegistry::GetOrCreateSharedGroup(displayId, 0);
        EXPECT_NE(groupId, 0u);

        const Uint64 contextHandle = 100;
        EXPECT_TRUE(GLContextRegistry::CreateSession(displayId, groupId, contextHandle));
        EXPECT_FALSE(GLContextRegistry::CreateSession(displayId, groupId, contextHandle));

        auto* session = GLContextRegistry::FindSession(contextHandle);
        ASSERT_NE(session, nullptr);
        EXPECT_EQ(session->GetGroupId(), groupId);
        EXPECT_EQ(session->GetEglContextHandle(), contextHandle);
    }

    TEST(ContextRegistryTest, ShareContextReusesGroup) {
        const DisplayId displayId = 2;
        const SharedGroupId firstGroup = GLContextRegistry::GetOrCreateSharedGroup(displayId, 0);
        const Uint64 firstContext = 201;
        ASSERT_TRUE(GLContextRegistry::CreateSession(displayId, firstGroup, firstContext));

        const SharedGroupId secondGroup = GLContextRegistry::GetOrCreateSharedGroup(displayId, firstContext);
        EXPECT_EQ(secondGroup, firstGroup);
    }

    TEST(ContextRegistryTest, SessionsInGroupShareObjectTables) {
        const DisplayId displayId = 5;
        const SharedGroupId groupId = GLContextRegistry::GetOrCreateSharedGroup(displayId, 0);
        const Uint64 firstContext = 501;
        const Uint64 secondContext = 502;
        ASSERT_TRUE(GLContextRegistry::CreateSession(displayId, groupId, firstContext));
        ASSERT_TRUE(GLContextRegistry::CreateSession(displayId, groupId, secondContext));

        auto* first = GLContextRegistry::FindSession(firstContext);
        auto* second = GLContextRegistry::FindSession(secondContext);
        ASSERT_NE(first, nullptr);
        ASSERT_NE(second, nullptr);
        ASSERT_NE(first->GetSharedTables(), nullptr);
        EXPECT_EQ(first->GetSharedTables(), second->GetSharedTables());

        // Buffer object tables are shared: an object created through one
        // session is visible through the other.
        const auto& created = first->GetContext().CreateBufferObject(42);
        ASSERT_TRUE(created);
        const auto& seen = second->GetContext().GetBufferObject(42);
        ASSERT_TRUE(seen);
        EXPECT_EQ(created, seen);

        // Texture object tables are shared too.
        const auto& createdTexture =
            first->GetContext().CreateTextureObject(43, TextureTarget::Texture2D);
        ASSERT_TRUE(createdTexture);
        const auto& seenTexture = second->GetContext().GetTextureObject(43);
        ASSERT_TRUE(seenTexture);
        EXPECT_EQ(createdTexture, seenTexture);

        // Sampler object tables are shared too.
        const auto& createdSampler = first->GetContext().CreateSamplerObject(44);
        ASSERT_TRUE(createdSampler);
        const auto& seenSampler = second->GetContext().GetSamplerObject(44);
        ASSERT_TRUE(seenSampler);
        EXPECT_EQ(createdSampler, seenSampler);

        // Renderbuffer object tables are shared too.
        const auto& createdRenderbuffer = first->GetContext().CreateRenderbufferObject(45);
        ASSERT_TRUE(createdRenderbuffer);
        const auto& seenRenderbuffer = second->GetContext().GetRenderbufferObject(45);
        ASSERT_TRUE(seenRenderbuffer);
        EXPECT_EQ(createdRenderbuffer, seenRenderbuffer);
    }

    TEST(ContextRegistryTest, TracksCurrentSessionPerThread) {
        const DisplayId displayId = 3;
        const SharedGroupId groupId = GLContextRegistry::GetOrCreateSharedGroup(displayId, 0);
        const Uint64 contextHandle = 301;
        ASSERT_TRUE(GLContextRegistry::CreateSession(displayId, groupId, contextHandle));

        const Uint64 clientThreadId = 42;
        GLContextRegistry::SetCurrent(clientThreadId, contextHandle);
        ASSERT_NE(GLContextRegistry::GetCurrentSession(clientThreadId), nullptr);
        EXPECT_EQ(GLContextRegistry::GetCurrentSession(clientThreadId)->GetEglContextHandle(), contextHandle);

        GLContextRegistry::SetCurrent(clientThreadId, 0);
        EXPECT_EQ(GLContextRegistry::GetCurrentSession(clientThreadId), nullptr);
    }

    TEST(ContextRegistryTest, DestroySessionRemovesEntry) {
        const DisplayId displayId = 4;
        const SharedGroupId groupId = GLContextRegistry::GetOrCreateSharedGroup(displayId, 0);
        const Uint64 contextHandle = 401;
        ASSERT_TRUE(GLContextRegistry::CreateSession(displayId, groupId, contextHandle));

        const SizeT before = GLContextRegistry::GetSessionCount();
        EXPECT_TRUE(GLContextRegistry::DestroySession(contextHandle));
        EXPECT_EQ(GLContextRegistry::GetSessionCount(), before - 1);
        EXPECT_EQ(GLContextRegistry::FindSession(contextHandle), nullptr);
    }
} // namespace MobileGL::MG_State::GLState

// End of File
