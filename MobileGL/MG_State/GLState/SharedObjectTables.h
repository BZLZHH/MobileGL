// MobileGL - MobileGL/MG_State/GLState/SharedObjectTables.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#pragma once

#include <Includes.h>
#include "MG_State/GLState/BufferState/BufferState.h"
#include <MG_Util/Miscellany/IndexGenerator.h>

namespace MobileGL::MG_State::GLState {
    // The GL object tables that are shared between every session of a
    // SharedGroup (GL shareCtx semantics): buffers, textures, samplers, VAOs,
    // programs, framebuffers, renderbuffers, transform feedbacks. Session
    // private objects (default objects, queries, syncs) stay in GLContext.
    //
    // Phase 2 migration: the existing GLContext-owned *State instances move
    // into this container incrementally. Buffer object table is the first
    // migrated family; bindings remain per-context in BufferState.

    class SharedBufferObjectTable {
    public:
        SharedBufferObjectTable() : m_indexGenerator(1024, 1) {}

        const SharedPtr<BufferObject>& GetObject(Uint index) const {
            auto it = m_bufferObjects.find(index);
            if (it != m_bufferObjects.end()) {
                return it->second;
            }
            static SharedPtr<BufferObject> nullBufferObject = nullptr;
            return nullBufferObject;
        }

        SharedPtr<BufferObject>& CreateObject(Uint index) {
            auto& bufferObject = m_bufferObjects[index];
            if (!bufferObject) {
                bufferObject = MakeShared<BufferObject>(index);
            }
            return bufferObject;
        }

        void GenerateNames(Uint number, Vector<Uint>& buffers) {
            buffers.resize(number);
            m_indexGenerator.Generate(number, buffers.data());
        }

        void MarkObjectForDeletion(Uint index) {
            if (m_indexGenerator.IsValid(index)) {
                m_bufferObjects.erase(index);
                m_indexGenerator.Delete(index);
            }
        }

        Bool ValidateName(Uint index) const {
            return m_indexGenerator.IsValid(index);
        }

        Bool ValidateObject(Uint index) const {
            return m_bufferObjects.find(index) != m_bufferObjects.end();
        }

    private:
        UnorderedMap<Uint, SharedPtr<BufferObject>> m_bufferObjects;
        IndexGenerator<Uint> m_indexGenerator;
    };

    class SharedObjectTables {
    public:
        SharedObjectTables() = default;

        SharedPtr<SharedBufferObjectTable>& GetSharedBufferObjects() {
            return m_sharedBufferObjects;
        }
        const SharedPtr<SharedBufferObjectTable>& GetSharedBufferObjects() const {
            return m_sharedBufferObjects;
        }

        // Legacy per-context BufferState; kept until every buffer access is
        // routed through GetSharedBufferObjects().
        BufferState& GetBufferState() { return m_bufferState; }
        const BufferState& GetBufferState() const { return m_bufferState; }

    private:
        SharedPtr<SharedBufferObjectTable> m_sharedBufferObjects = MakeShared<SharedBufferObjectTable>();
        BufferState m_bufferState;
    };
} // namespace MobileGL::MG_State::GLState

// End of File
