// MobileGL - MobileGL/MG_State/GLState/RenderbufferState/RenderbufferState.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "RenderbufferState.h"
#include "MG_State/GLState/SharedObjectTables.h"
#include "MG_Util/Types.h"

namespace MobileGL::MG_State::GLState {
    RenderbufferState::RenderbufferState() : m_indexGenerator(1024, 1) {
        for (SizeT i = 0; i < m_bindingSlots.size(); ++i) {
            m_bindingSlots[i] = BindingSlot<RenderbufferObject>(static_cast<RenderbufferTarget>(i));
        }
    }

    const SharedPtr<RenderbufferObject>& SharedRenderbufferObjectTable::GetObject(Uint index) const {
        auto it = m_renderbufferObjects.find(index);
        if (it != m_renderbufferObjects.end()) {
            return it->second;
        }
        static SharedPtr<RenderbufferObject> nullRenderbufferObject = nullptr;
        return nullRenderbufferObject;
    }

    void SharedRenderbufferObjectTable::GenerateNames(Uint number, Vector<Uint>& renderbuffers) {
        renderbuffers.resize(number);
        m_indexGenerator.Generate(number, renderbuffers.data());
    }

    const SharedPtr<RenderbufferObject>& SharedRenderbufferObjectTable::CreateObject(Uint index) {
        auto& bufferObject = m_renderbufferObjects[index];
        if (!bufferObject) {
            bufferObject = MakeShared<RenderbufferObject>(index);
        }
        return bufferObject;
    }

    void SharedRenderbufferObjectTable::MarkObjectForDeletion(Uint index) {
        if (m_indexGenerator.IsValid(index)) {
            m_renderbufferObjects.erase(index);
            m_indexGenerator.Delete(index);
        }
    }

    Bool SharedRenderbufferObjectTable::ValidateName(Uint index) const {
        return m_indexGenerator.IsValid(index);
    }

    Bool SharedRenderbufferObjectTable::ValidateObject(Uint index) const {
        return m_renderbufferObjects.find(index) != m_renderbufferObjects.end();
    }

    const SharedPtr<RenderbufferObject>& RenderbufferState::GetRenderbufferObject(Uint index) {
        if (m_sharedObjectTable) {
            return m_sharedObjectTable->GetObject(index);
        }
        auto it = m_renderbufferObjects.find(index);
        if (it != m_renderbufferObjects.end()) {
            return it->second;
        }
        static SharedPtr<RenderbufferObject> nullRenderbufferObject = nullptr;
        return nullRenderbufferObject;
    }

    void RenderbufferState::GenerateNames(Uint number, Vector<Uint>& renderbuffers) {
        if (m_sharedObjectTable) {
            m_sharedObjectTable->GenerateNames(number, renderbuffers);
            return;
        }
        renderbuffers.resize(number);
        m_indexGenerator.Generate(number, renderbuffers.data());
    }

    const SharedPtr<RenderbufferObject>& RenderbufferState::CreateRenderbufferObject(Uint index) {
        if (m_sharedObjectTable) {
            return m_sharedObjectTable->CreateObject(index);
        }
        auto& bufferObject = m_renderbufferObjects[index];
        if (!bufferObject) {
            bufferObject = MakeShared<RenderbufferObject>(index);
        }
        return bufferObject;
    }

    BindingSlot<RenderbufferObject>& RenderbufferState::GetBindingSlot(RenderbufferTarget target) {
        for (auto& bindingSlot : m_bindingSlots) {
            if (bindingSlot.GetTarget() == target) {
                return bindingSlot;
            }
        }
        MOBILEGL_ASSERT(false, "Invalid RenderbufferTarget enum value: %d", static_cast<int>(target));
        return m_bindingSlots[0];
    }

    void RenderbufferState::MarkRenderbufferObjectForDeletion(Uint index) {
        const SharedPtr<RenderbufferObject> renderbufferObject =
            m_sharedObjectTable ? m_sharedObjectTable->GetObject(index)
                                : (m_renderbufferObjects.find(index) != m_renderbufferObjects.end()
                                       ? m_renderbufferObjects.find(index)->second
                                       : nullptr);
        if (renderbufferObject) {
            for (auto& bindingSlot : m_bindingSlots) {
                if (bindingSlot.GetBoundObject() == renderbufferObject) {
                    bindingSlot.Bind(nullptr);
                }
            }
        }
        if (m_sharedObjectTable) {
            m_sharedObjectTable->MarkObjectForDeletion(index);
            return;
        }
        if (m_indexGenerator.IsValid(index)) {
            auto it = m_renderbufferObjects.find(index);
            if (it != m_renderbufferObjects.end()) {
                m_renderbufferObjects.erase(index);
            }
            m_indexGenerator.Delete(index);
        }
    }

    Bool RenderbufferState::ValidateName(Uint index) const {
        if (m_sharedObjectTable) {
            return m_sharedObjectTable->ValidateName(index);
        }
        return m_indexGenerator.IsValid(index);
    }

    Bool RenderbufferState::ValidateRenderbufferObject(Uint index) const {
        if (m_sharedObjectTable) {
            return m_sharedObjectTable->ValidateObject(index);
        }
        return m_renderbufferObjects.find(index) != m_renderbufferObjects.end();
    }
} // namespace MobileGL::MG_State::GLState

// End of File
