// MobileGL - MobileGL/MG_State/GLState/SessionPrivateState.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#pragma once

#include <Includes.h>

namespace MobileGL::MG_State::GLState {
    // Per-session state that GLContext owns: sync objects and the query-object
    // registry. This is the C/S phase-2 layer that stops MobileGL's sync/query
    // objects from leaking across GL contexts when FullServer carries multiple
    // sessions.
    class SessionPrivateState {
    public:
        // ------------------------------------------------------------------
        // Sync objects (session-private).
        // ------------------------------------------------------------------
        struct SyncObject {
            void* BackendHandle = nullptr;
            GLenum Condition = GL_SYNC_GPU_COMMANDS_COMPLETE;
            GLbitfield Flags = 0;
        };

        SyncObject* FindSync(void* handle) {
            const auto it = m_syncs.find(handle);
            return it == m_syncs.end() ? nullptr : &it->second;
        }

        void InsertSync(void* handle, SyncObject&& object) {
            m_syncs[handle] = Move(object);
        }

        void EraseSync(void* handle) {
            m_syncs.erase(handle);
        }

        void ClearSyncs() {
            m_syncs.clear();
        }

        SizeT GetSyncCount() const {
            return m_syncs.size();
        }

        Vector<void*> GetSyncHandles() const {
            Vector<void*> handles;
            handles.reserve(m_syncs.size());
            for (const auto& entry : m_syncs) {
                handles.push_back(entry.first);
            }
            return handles;
        }

        void* AllocateSyncHandle() {
            return reinterpret_cast<void*>(m_nextSyncHandle++);
        }

        // ------------------------------------------------------------------
        // Query objects (session-private).
        // ------------------------------------------------------------------
        struct QueryObject {
            GLuint id = 0;
            GLenum target = 0; // 0 = gen'd but never used with BeginQuery/QueryCounter
            Bool created = false;
            void* backendHandle = nullptr;
            Bool active = false;
            Bool ended = false;
            Bool resultCached = false;
            Uint64 cachedResult = 0;
            Uint64 counterSnapshot = 0;
            Uint64 accountedCaptureDrawSnapshot = 0;
            Uint64 geometryCaptureDrawSnapshot = 0;
        };

        QueryObject* FindQuery(GLuint id) {
            const auto it = m_queries.find(id);
            return it == m_queries.end() ? nullptr : &it->second;
        }

        QueryObject& AllocateQuery(GLuint id) {
            QueryObject object;
            object.id = id;
            return m_queries[id] = Move(object);
        }

        void EraseQuery(GLuint id) {
            m_queries.erase(id);
        }

        void ClearQueries() {
            m_queries.clear();
        }

        GLuint NextQueryId() {
            return m_nextQueryId++;
        }

        SizeT GetQueryCount() const {
            return m_queries.size();
        }

        Vector<GLuint> GetQueryIds() const {
            Vector<GLuint> ids;
            ids.reserve(m_queries.size());
            for (const auto& entry : m_queries) {
                ids.push_back(entry.first);
            }
            return ids;
        }

        // Context-private "which query is active" state.
        GLuint activeTimeElapsedQueryId = 0;
        GLuint activePrimitivesWrittenQueryId = 0;
        GLuint activePrimitivesGeneratedQueryId = 0;
        GLuint activeSamplesPassedQueryId = 0;
        UnorderedMap<GLenum, GLuint> activePipelineStatisticsQueryIds;

    private:
        UnorderedMap<void*, SyncObject> m_syncs;
        uintptr_t m_nextSyncHandle = 1;
        UnorderedMap<GLuint, QueryObject> m_queries;
        GLuint m_nextQueryId = 1;
    };
} // namespace MobileGL::MG_State::GLState

// End of File
