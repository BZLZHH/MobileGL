// MobileGL - MobileGL/MG_State/GLState/ContextRegistry.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#pragma once

#include <Includes.h>
#include "MG_State/GLState/Core.h"
#include "MG_State/GLState/SharedObjectTables.h"

namespace MobileGL::MG_State::GLState {
    using ContextSessionId = Uint64;
    using SharedGroupId = Uint64;
    using DisplayId = Uint64;

    // One GL context session: owns the per-context GLContext (current state,
    // bindings, error queue) and knows its SharedGroup.
    class GLContextSession {
    public:
        GLContextSession(SharedGroupId groupId, Uint64 eglContextHandle,
                         SharedObjectTables* sharedTables = nullptr);

        SharedGroupId GetGroupId() const { return m_groupId; }
        Uint64 GetEglContextHandle() const { return m_eglContextHandle; }
        SharedObjectTables* GetSharedTables() const { return m_sharedTables; }
        GLContext& GetContext() { return *m_context; }
        const GLContext& GetContext() const { return *m_context; }

    private:
        SharedGroupId m_groupId = 0;
        Uint64 m_eglContextHandle = 0;
        SharedObjectTables* m_sharedTables = nullptr;
        UniquePtr<GLContext> m_context;
    };

    // One shared group: the container for objects shared between sessions.
    // Phase 2 migrates the shared object tables here; the per-context tables
    // stay inside GLContext until the share-group migration lands.
    class GLSharedGroup {
    public:
        explicit GLSharedGroup(DisplayId displayId);

        DisplayId GetDisplayId() const { return m_displayId; }
        SharedGroupId GetId() const { return m_groupId; }
        SharedObjectTables& GetSharedTables() { return m_sharedTables; }
        const SharedObjectTables& GetSharedTables() const { return m_sharedTables; }

        void AddSession(const SharedPtr<GLContextSession>& session);
        void RemoveSession(SharedGroupId groupId);

    private:
        SharedGroupId m_groupId = 0;
        DisplayId m_displayId = 0;
        SharedObjectTables m_sharedTables;
        Vector<WeakPtr<GLContextSession>> m_sessions;
    };

    // Process-wide registry for display -> shared group -> session. Thread safe.
    // FullServer generates the ids; in the migration period the EGL layer calls
    // in here from CreateContext/DestroyContext/MakeCurrent.
    class GLContextRegistry {
    public:
        static SharedGroupId GetOrCreateSharedGroup(DisplayId displayId, Uint64 shareContextHandle);
        static Bool CreateSession(DisplayId displayId, SharedGroupId groupId, Uint64 eglContextHandle);
        static Bool DestroySession(Uint64 eglContextHandle);
        static GLContextSession* FindSession(Uint64 eglContextHandle);

        static void SetCurrent(Uint64 clientThreadId, Uint64 eglContextHandle);
        static GLContextSession* GetCurrentSession(Uint64 clientThreadId);
        static GLContext* GetCurrentGLContext(Uint64 clientThreadId);

        static SizeT GetSessionCount();
        static SizeT GetSharedGroupCount();

    private:
        static UnorderedMap<Uint64, SharedPtr<GLSharedGroup>>& SharedGroups();
        static UnorderedMap<Uint64, SharedPtr<GLContextSession>>& Sessions();
        static UnorderedMap<Uint64, GLContextSession*>& CurrentByThread();
    };
} // namespace MobileGL::MG_State::GLState

// End of File
