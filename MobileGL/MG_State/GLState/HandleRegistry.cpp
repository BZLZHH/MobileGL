// MobileGL - MobileGL/MG_State/GLState/HandleRegistry.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "HandleRegistry.h"

namespace MobileGL::MG_State::GLState {
    namespace {
        std::recursive_mutex s_mutex;
        Uint64 s_nextHandle = 1;

        using HandleMap = UnorderedMap<Uint64, ObjectHandleEntry>;
        using KeyMap = UnorderedMap<ObjectHandleKey, Uint64, ObjectHandleKeyHash>;

        HandleMap& HandleEntries() {
            static HandleMap map;
            return map;
        }

        KeyMap& KeyEntries() {
            static KeyMap map;
            return map;
        }
    } // namespace

    MobileGLBackendHandle ObjectHandleRegistry::Allocate(ObjectHandleScope scope, Uint64 scopeId,
                                                         MobileGLObjectKind kind, Uint32 glName) {
        const std::lock_guard<std::recursive_mutex> lock(s_mutex);
        const ObjectHandleKey key{scopeId, static_cast<Uint32>(kind), glName};
        auto keyIt = KeyEntries().find(key);
        if (keyIt != KeyEntries().end()) {
            return keyIt->second;
        }

        const Uint64 handle = s_nextHandle++;
        ObjectHandleEntry entry{};
        entry.Handle = handle;
        entry.Scope = scope;
        entry.ScopeId = scopeId;
        entry.Kind = static_cast<Uint32>(kind);
        entry.GlName = glName;
        entry.Generation = 1;

        HandleEntries()[handle] = entry;
        KeyEntries()[key] = handle;
        return handle;
    }

    MobileGLBackendHandle ObjectHandleRegistry::Lookup(ObjectHandleScope scope, Uint64 scopeId,
                                                       MobileGLObjectKind kind, Uint32 glName) {
        const std::lock_guard<std::recursive_mutex> lock(s_mutex);
        const ObjectHandleKey key{scopeId, static_cast<Uint32>(kind), glName};
        auto it = KeyEntries().find(key);
        return it == KeyEntries().end() ? 0 : it->second;
    }

    const ObjectHandleEntry* ObjectHandleRegistry::Get(MobileGLBackendHandle handle) {
        const std::lock_guard<std::recursive_mutex> lock(s_mutex);
        auto it = HandleEntries().find(handle);
        return it == HandleEntries().end() ? nullptr : &it->second;
    }

    Uint64 ObjectHandleRegistry::GetGeneration(MobileGLBackendHandle handle) {
        const auto* entry = Get(handle);
        return entry == nullptr ? 0 : entry->Generation;
    }

    void ObjectHandleRegistry::Destroy(MobileGLBackendHandle handle) {
        const std::lock_guard<std::recursive_mutex> lock(s_mutex);
        auto it = HandleEntries().find(handle);
        if (it == HandleEntries().end()) {
            return;
        }
        const ObjectHandleKey key{it->second.ScopeId, it->second.Kind, it->second.GlName};
        KeyEntries().erase(key);
        HandleEntries().erase(it);
    }

    SizeT ObjectHandleRegistry::GetCount() {
        const std::lock_guard<std::recursive_mutex> lock(s_mutex);
        return HandleEntries().size();
    }
} // namespace MobileGL::MG_State::GLState

// End of File
