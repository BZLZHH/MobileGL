// MobileGL - MobileGL/MG_State/GLState/FramebufferState/FramebufferState.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "FramebufferState.h"
#include "MG_State/GLState/SharedObjectTables.h"
#include "MG_State/GLState/FramebufferState/FramebufferObject.h"

namespace MobileGL::MG_State::GLState {
    FramebufferState::FramebufferState() {
        for (SizeT i = 0; i < m_bindingSlots.size(); ++i) {
            m_bindingSlots[i] = BindingSlot<FramebufferObject>(static_cast<FramebufferTarget>(i));
        }
    }

    const SharedPtr<FramebufferObject>& SharedFramebufferObjectTable::GetObject(Uint index) const {
        auto it = m_framebufferObjects.find(index);
        if (it != m_framebufferObjects.end()) {
            return it->second;
        }
        static SharedPtr<FramebufferObject> nullFramebufferObject = nullptr;
        return nullFramebufferObject;
    }

    void SharedFramebufferObjectTable::GenerateNames(Uint number, Vector<Uint>& framebuffers) {
        framebuffers.resize(number);
        m_indexGenerator.Generate(number, framebuffers.data());
    }

    const SharedPtr<FramebufferObject>& SharedFramebufferObjectTable::CreateObject(Uint index) {
        if (index == 0) {
            if (!m_indexGenerator.IsValid(0)) {
                m_indexGenerator.Insert(0);
            } else {
                static SharedPtr<FramebufferObject> nullFramebufferObject = nullptr;
                return nullFramebufferObject;
            }
        }
        auto& framebufferObject = m_framebufferObjects[index];
        if (!framebufferObject) {
            framebufferObject = MakeShared<FramebufferObject>(index);
        }
        return framebufferObject;
    }

    void SharedFramebufferObjectTable::MarkObjectForDeletion(Uint index) {
        if (m_indexGenerator.IsValid(index)) {
            m_framebufferObjects.erase(index);
            m_indexGenerator.Delete(index);
        }
    }

    Bool SharedFramebufferObjectTable::ValidateName(Uint index) const {
        return m_indexGenerator.IsValid(index);
    }

    Bool SharedFramebufferObjectTable::ValidateObject(Uint index) const {
        return m_framebufferObjects.find(index) != m_framebufferObjects.end();
    }

    const SharedPtr<FramebufferObject>& FramebufferState::GetFramebufferObject(Uint index) {
        if (m_sharedObjectTable) {
            return m_sharedObjectTable->GetObject(index);
        }
        auto it = m_framebufferObjects.find(index);
        if (it != m_framebufferObjects.end()) {
            return it->second;
        }
        static SharedPtr<FramebufferObject> nullFramebufferObject = nullptr;
        return nullFramebufferObject;
    }

    void FramebufferState::GenerateNames(Uint number, Vector<Uint>& buffers) {
        if (m_sharedObjectTable) {
            m_sharedObjectTable->GenerateNames(number, buffers);
            return;
        }
        buffers.resize(number);
        m_indexGenerator.Generate(number, buffers.data());
    }

    const SharedPtr<FramebufferObject>& FramebufferState::CreateFramebufferObject(Uint index) {
        if (m_sharedObjectTable) {
            return m_sharedObjectTable->CreateObject(index);
        }
        if (index == 0) {
            if (!m_indexGenerator.IsValid(0)) {
                m_indexGenerator.Insert(0);
            } else {
                static SharedPtr<FramebufferObject> nullFramebufferObject = nullptr;
                return nullFramebufferObject;
            }
        }
        auto& framebufferObject = m_framebufferObjects[index];
        if (!framebufferObject) {
            framebufferObject = MakeShared<FramebufferObject>(index);
        }
        return framebufferObject;
    }

    BindingSlot<FramebufferObject>& FramebufferState::GetBindingSlot(FramebufferTarget target) {
        for (auto& bindingSlot : m_bindingSlots) {
            if (bindingSlot.GetTarget() == target) {
                return bindingSlot;
            }
        }
        MOBILEGL_ASSERT(false, "Invalid FramebufferTarget enum value: %d", static_cast<int>(target));
        return m_bindingSlots[0];
    }

    void FramebufferState::MarkFramebufferObjectForDeletion(Uint index) {
        const SharedPtr<FramebufferObject> framebufferObject =
            m_sharedObjectTable ? m_sharedObjectTable->GetObject(index)
                                : (m_framebufferObjects.find(index) != m_framebufferObjects.end()
                                       ? m_framebufferObjects.find(index)->second
                                       : nullptr);
        if (framebufferObject) {
            for (auto& bindingSlot : m_bindingSlots) {
                if (bindingSlot.GetBoundObject() == framebufferObject) {
                    bindingSlot.Bind(GetFramebufferObject(0));
                }
            }
        }
        if (m_sharedObjectTable) {
            m_sharedObjectTable->MarkObjectForDeletion(index);
            return;
        }
        if (m_indexGenerator.IsValid(index)) {
            auto it = m_framebufferObjects.find(index);
            if (it != m_framebufferObjects.end()) {
                m_framebufferObjects.erase(index);
            }
            m_indexGenerator.Delete(index);
        }
    }

    Bool FramebufferState::ValidateName(Uint index) const {
        if (m_sharedObjectTable) {
            return m_sharedObjectTable->ValidateName(index);
        }
        return m_indexGenerator.IsValid(index);
    }

    Bool FramebufferState::ValidateFramebufferObject(Uint index) const {
        if (m_sharedObjectTable) {
            return m_sharedObjectTable->ValidateObject(index);
        }
        return m_framebufferObjects.find(index) != m_framebufferObjects.end();
    }
} // namespace MobileGL::MG_State::GLState

// End of File
