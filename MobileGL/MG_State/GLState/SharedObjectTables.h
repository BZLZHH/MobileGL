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
#include "MG_State/GLState/RenderbufferState/RenderbufferState.h"
#include "MG_State/GLState/FramebufferState/FramebufferState.h"
#include "MG_State/GLState/VertexArrayState/VertexArrayState.h"
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

    // Shared renderbuffer object table (object map + name generator). Binding
    // slots stay per-context.
    class SharedRenderbufferObjectTable {
    public:
        SharedRenderbufferObjectTable() : m_indexGenerator(1024, 1) {}

        const SharedPtr<RenderbufferObject>& GetObject(Uint index) const;
        void GenerateNames(Uint number, Vector<Uint>& renderbuffers);
        const SharedPtr<RenderbufferObject>& CreateObject(Uint index);
        void MarkObjectForDeletion(Uint index);
        Bool ValidateName(Uint index) const;
        Bool ValidateObject(Uint index) const;

    private:
        UnorderedMap<Uint, SharedPtr<RenderbufferObject>> m_renderbufferObjects;
        IndexGenerator<Uint> m_indexGenerator;
    };

    // Shared framebuffer object table (object map + name generator). Binding
    // slots stay per-context.
    class SharedFramebufferObjectTable {
    public:
        SharedFramebufferObjectTable() : m_indexGenerator(1024, 1) {}

        const SharedPtr<FramebufferObject>& GetObject(Uint index) const;
        void GenerateNames(Uint number, Vector<Uint>& framebuffers);
        const SharedPtr<FramebufferObject>& CreateObject(Uint index);
        void MarkObjectForDeletion(Uint index);
        Bool ValidateName(Uint index) const;
        Bool ValidateObject(Uint index) const;

    private:
        UnorderedMap<Uint, SharedPtr<FramebufferObject>> m_framebufferObjects;
        IndexGenerator<Uint> m_indexGenerator;
    };

    // Shared vertex-array object table. The bound VAO index/detached object
    // remain per-context in VertexArrayState; only the object vector and name
    // generator are shared.
    class SharedVertexArrayObjectTable {
    public:
        SharedVertexArrayObjectTable() : m_indexGenerator(1024, 1) {
            m_indexGenerator.Insert(0);
            m_vertexArrays.push_back(MakeShared<VertexArrayObject>(0));
        }

        SizeT GetSize() const { return m_vertexArrays.size(); }
        SharedPtr<VertexArrayObject>& GetSlot(Uint index) { return m_vertexArrays[index]; }
        SharedPtr<VertexArrayObject>& GetSlice(Uint index) { return m_vertexArrays[index]; }
        const SharedPtr<VertexArrayObject>& GetSliceRef(Uint index) const { return m_vertexArrays[index]; }
        const SharedPtr<VertexArrayObject>& GetObject(Uint index) const {
            if (index >= m_vertexArrays.size()) {
                static SharedPtr<VertexArrayObject> nullObject = nullptr;
                return nullObject;
            }
            return m_vertexArrays[index];
        }
        Vector<SharedPtr<VertexArrayObject>>& GetAllVertexArrays() { return m_vertexArrays; }
        void GenerateNames(Uint number, Vector<Uint>& arrays) {
            arrays.resize(number);
            m_indexGenerator.Generate(number, arrays.data());
        }
        SharedPtr<VertexArrayObject>& CreateObject(Uint index) {
            if (index >= m_vertexArrays.size()) {
                m_vertexArrays.reserve(std::bit_ceil(index + 1));
                m_vertexArrays.resize(index + 1, nullptr);
            }
            auto& vao = m_vertexArrays[index];
            if (!vao) {
                vao = MakeShared<VertexArrayObject>(index);
            }
            return vao;
        }
        void MarkObjectForDeletion(Uint index) {
            if (m_indexGenerator.IsValid(index)) {
                if (index < m_vertexArrays.size()) {
                    m_vertexArrays[index] = nullptr;
                }
                m_indexGenerator.Delete(index);
            }
        }
        Bool ValidateName(Uint index) const { return m_indexGenerator.IsValid(index); }
        Bool ValidateObject(Uint index) const {
            return index < m_vertexArrays.size() && m_vertexArrays[index] != nullptr;
        }

    private:
        Vector<SharedPtr<VertexArrayObject>> m_vertexArrays;
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

        SharedPtr<SharedRenderbufferObjectTable>& GetSharedRenderbufferObjects() {
            return m_sharedRenderbufferObjects;
        }
        const SharedPtr<SharedRenderbufferObjectTable>& GetSharedRenderbufferObjects() const {
            return m_sharedRenderbufferObjects;
        }

        SharedPtr<SharedFramebufferObjectTable>& GetSharedFramebufferObjects() {
            return m_sharedFramebufferObjects;
        }
        const SharedPtr<SharedFramebufferObjectTable>& GetSharedFramebufferObjects() const {
            return m_sharedFramebufferObjects;
        }

        SharedPtr<SharedVertexArrayObjectTable>& GetSharedVertexArrayObjects() {
            return m_sharedVertexArrayObjects;
        }
        const SharedPtr<SharedVertexArrayObjectTable>& GetSharedVertexArrayObjects() const {
            return m_sharedVertexArrayObjects;
        }

        // Legacy per-context states; kept until every object access is
        // routed through the GetShared*Objects() accessors.
        BufferState& GetBufferState() { return m_bufferState; }
        const BufferState& GetBufferState() const { return m_bufferState; }

    private:
        SharedPtr<SharedBufferObjectTable> m_sharedBufferObjects = MakeShared<SharedBufferObjectTable>();
        SharedPtr<SharedTextureObjectTable> m_sharedTextureObjects = MakeShared<SharedTextureObjectTable>();
        SharedPtr<SharedSamplerObjectTable> m_sharedSamplerObjects = MakeShared<SharedSamplerObjectTable>();
        SharedPtr<SharedRenderbufferObjectTable> m_sharedRenderbufferObjects = MakeShared<SharedRenderbufferObjectTable>();
        SharedPtr<SharedFramebufferObjectTable> m_sharedFramebufferObjects = MakeShared<SharedFramebufferObjectTable>();
        SharedPtr<SharedVertexArrayObjectTable> m_sharedVertexArrayObjects = MakeShared<SharedVertexArrayObjectTable>();
        BufferState m_bufferState;
    };
} // namespace MobileGL::MG_State::GLState

// End of File
