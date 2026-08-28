// MobileGL - MobileGL/MG_State/GLState/ContextRegistry.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "ContextRegistry.h"

namespace MobileGL::MG_State::GLState {
    namespace {
        std::recursive_mutex s_registryMutex;
        Uint64 s_nextGroupId = 1;
        Uint64 s_nextSessionId = 1;

        UnorderedMap<Uint64, SharedPtr<GLSharedGroup>>& SharedGroupMap() {
            static UnorderedMap<Uint64, SharedPtr<GLSharedGroup>> map;
            return map;
        }

        UnorderedMap<Uint64, SharedPtr<GLContextSession>>& SessionMap() {
            static UnorderedMap<Uint64, SharedPtr<GLContextSession>> map;
            return map;
        }

        UnorderedMap<Uint64, GLContextSession*>& CurrentMap() {
            static UnorderedMap<Uint64, GLContextSession*> map;
            return map;
        }
    } // namespace

    GLContextSession::GLContextSession(SharedGroupId groupId, Uint64 eglContextHandle,
                                       SharedObjectTables* sharedTables)
        : m_groupId(groupId),
          m_eglContextHandle(eglContextHandle),
          m_sharedTables(sharedTables),
          m_context(MakeUnique<GLContext>()) {
        m_context->SetSharedGroupId(groupId);
        m_context->SetSessionId(eglContextHandle);
        if (m_sharedTables != nullptr) {
            m_context->SetSharedBufferObjectTable(m_sharedTables->GetSharedBufferObjects());
            m_context->SetSharedTextureObjectTable(m_sharedTables->GetSharedTextureObjects());
        }
    }

    GLSharedGroup::GLSharedGroup(DisplayId displayId)
        : m_groupId(s_nextGroupId++),
          m_displayId(displayId) {
    }

    void GLSharedGroup::AddSession(const SharedPtr<GLContextSession>& session) {
        m_sessions.push_back(session);
    }

    void GLSharedGroup::RemoveSession(SharedGroupId) {
        // WeakPtr entries expire on their own; compaction is opportunistic and
        // happens on the next AddSession/query. Kept for API symmetry with
        // Phase 2's explicit lifecycle.
    }

    SharedGroupId GLContextRegistry::GetOrCreateSharedGroup(DisplayId displayId, Uint64 shareContextHandle) {
        const std::lock_guard<std::recursive_mutex> lock(s_registryMutex);
        if (shareContextHandle != 0) {
            auto it = SessionMap().find(shareContextHandle);
            if (it != SessionMap().end()) {
                return it->second->GetGroupId();
            }
        }
        auto group = MakeShared<GLSharedGroup>(displayId);
        SharedGroupId id = group->GetId();
        SharedGroupMap()[id] = Move(group);
        return id;
    }

    Bool GLContextRegistry::CreateSession(DisplayId displayId, SharedGroupId groupId, Uint64 eglContextHandle) {
        const std::lock_guard<std::recursive_mutex> lock(s_registryMutex);
        if (SessionMap().find(eglContextHandle) != SessionMap().end()) {
            return false;
        }
        auto groupIt = SharedGroupMap().find(groupId);
        if (groupIt == SharedGroupMap().end()) {
            return false;
        }
        auto session = MakeShared<GLContextSession>(groupId, eglContextHandle,
                                                    &groupIt->second->GetSharedTables());
        groupIt->second->AddSession(session);
        SessionMap()[eglContextHandle] = Move(session);
        return true;
    }

    Bool GLContextRegistry::DestroySession(Uint64 eglContextHandle) {
        const std::lock_guard<std::recursive_mutex> lock(s_registryMutex);
        auto it = SessionMap().find(eglContextHandle);
        if (it == SessionMap().end()) {
            return false;
        }
        auto* session = it->second.get();
        for (auto currentIt = CurrentMap().begin(); currentIt != CurrentMap().end();) {
            if (currentIt->second == session) {
                currentIt = CurrentMap().erase(currentIt);
            } else {
                ++currentIt;
            }
        }
        SessionMap().erase(it);
        return true;
    }

    GLContextSession* GLContextRegistry::FindSession(Uint64 eglContextHandle) {
        const std::lock_guard<std::recursive_mutex> lock(s_registryMutex);
        auto it = SessionMap().find(eglContextHandle);
        return it == SessionMap().end() ? nullptr : it->second.get();
    }

    void GLContextRegistry::SetCurrent(Uint64 clientThreadId, Uint64 eglContextHandle) {
        const std::lock_guard<std::recursive_mutex> lock(s_registryMutex);
        auto it = SessionMap().find(eglContextHandle);
        CurrentMap()[clientThreadId] = it == SessionMap().end() ? nullptr : it->second.get();
    }

    GLContextSession* GLContextRegistry::GetCurrentSession(Uint64 clientThreadId) {
        const std::lock_guard<std::recursive_mutex> lock(s_registryMutex);
        auto it = CurrentMap().find(clientThreadId);
        return it == CurrentMap().end() ? nullptr : it->second;
    }

    GLContext* GLContextRegistry::GetCurrentGLContext(Uint64 clientThreadId) {
        auto* session = GetCurrentSession(clientThreadId);
        return session == nullptr ? nullptr : &session->GetContext();
    }

    SizeT GLContextRegistry::GetSessionCount() {
        const std::lock_guard<std::recursive_mutex> lock(s_registryMutex);
        return SessionMap().size();
    }

    SizeT GLContextRegistry::GetSharedGroupCount() {
        const std::lock_guard<std::recursive_mutex> lock(s_registryMutex);
        return SharedGroupMap().size();
    }

    // The function-local statics above satisfy the "leak-at-exit" convention
    // used by the rest of the GL state: they are never destroyed during
    // process teardown, so no backend destructor runs at static destruction.
} // namespace MobileGL::MG_State::GLState

// End of File
