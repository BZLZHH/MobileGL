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
#include "MG_State/GLState/TextureState/TextureState.h"
#include "MG_State/GLState/SamplerState/SamplerState.h"
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

    // Shared texture object table (object map + name generator). Default
    // texture objects (name 0) and texture-unit bindings stay per-context.
    class SharedTextureObjectTable {
    public:
        SharedTextureObjectTable() : m_indexGenerator(1024, 1) {}

        const SharedPtr<ITextureObject>& GetObject(Uint index) const;
        void GenerateNames(Uint number, Vector<Uint>& textures);
        const SharedPtr<ITextureObject>& CreateObject(Uint index, TextureTarget target);
        const SharedPtr<ITextureObject>& CreateTextureViewObject(
            Uint index, TextureTarget target, const SharedPtr<ITextureObject>& storageOwner,
            Uint minLevel, Uint numLevels, Uint minLayer, Uint numLayers);
        void MarkObjectForDeletion(Uint index, Bool keepUnboundReservation);
        Bool ValidateName(Uint index) const;
        Bool ValidateObject(Uint index) const;

    private:
        UnorderedMap<Uint, SharedPtr<ITextureObject>> m_textureObjects;
        IndexGenerator<Uint> m_indexGenerator;
    };

    // Shared sampler object table (object map + name generator).
    class SharedSamplerObjectTable {
    public:
        SharedSamplerObjectTable() : m_indexGenerator(1024, 1) {}

        const SharedPtr<SamplerObject>& GetObject(Uint index) const;
        void GenerateNames(Uint number, Vector<Uint>& samplers);
        const SharedPtr<SamplerObject>& CreateObject(Uint index);
        void MarkObjectForDeletion(Uint index);
        Bool ValidateName(Uint index) const;
        Bool ValidateObject(Uint index) const;

    private:
        UnorderedMap<Uint, SharedPtr<SamplerObject>> m_samplerObjects;
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

        SharedPtr<SharedTextureObjectTable>& GetSharedTextureObjects() {
            return m_sharedTextureObjects;
        }
        const SharedPtr<SharedTextureObjectTable>& GetSharedTextureObjects() const {
            return m_sharedTextureObjects;
        }

        SharedPtr<SharedSamplerObjectTable>& GetSharedSamplerObjects() {
            return m_sharedSamplerObjects;
        }
        const SharedPtr<SharedSamplerObjectTable>& GetSharedSamplerObjects() const {
            return m_sharedSamplerObjects;
        }

        // Legacy per-context states; kept until every object access is
        // routed through the GetShared*Objects() accessors.
        BufferState& GetBufferState() { return m_bufferState; }
        const BufferState& GetBufferState() const { return m_bufferState; }

    private:
        SharedPtr<SharedBufferObjectTable> m_sharedBufferObjects = MakeShared<SharedBufferObjectTable>();
        SharedPtr<SharedTextureObjectTable> m_sharedTextureObjects = MakeShared<SharedTextureObjectTable>();
        SharedPtr<SharedSamplerObjectTable> m_sharedSamplerObjects = MakeShared<SharedSamplerObjectTable>();
        BufferState m_bufferState;
    };
} // namespace MobileGL::MG_State::GLState

// End of File
