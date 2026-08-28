// MobileGL - MobileGL/MG_State/GLState/HandleRegistry.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#pragma once

#include <Includes.h>
#include "MG_Protocol/bfa.h"

namespace MobileGL::MG_State::GLState {
    enum class ObjectHandleScope {
        SharedGroup,
        Session
    };

    struct ObjectHandleKey {
        Uint64 ScopeId = 0;
        Uint32 Kind = 0;
        Uint32 GlName = 0;

        Bool operator==(const ObjectHandleKey& rhs) const {
            return ScopeId == rhs.ScopeId && Kind == rhs.Kind && GlName == rhs.GlName;
        }
    };

    struct ObjectHandleKeyHash {
        SizeT operator()(const ObjectHandleKey& key) const {
            return std::hash<Uint64>{}(key.ScopeId) ^
                   (std::hash<Uint32>{}(key.Kind) << 1) ^
                   (std::hash<Uint32>{}(key.GlName) << 2);
        }
    };

    struct ObjectHandleEntry {
        Uint64 Handle = 0;
        ObjectHandleScope Scope = ObjectHandleScope::SharedGroup;
        Uint64 ScopeId = 0;
        Uint32 Kind = 0;
        Uint32 GlName = 0;
        Uint64 Generation = 0;
    };

    // FullServer-owned registry mapping GL names to opaque MobileGLBackendHandle.
    // Shared objects are keyed by SharedGroupId; context-private objects by
    // SessionId. Handles are never reused for the process lifetime.
    class ObjectHandleRegistry {
    public:
        static MobileGLBackendHandle Allocate(ObjectHandleScope scope, Uint64 scopeId,
                                              MobileGLObjectKind kind, Uint32 glName);
        static MobileGLBackendHandle Lookup(ObjectHandleScope scope, Uint64 scopeId,
                                            MobileGLObjectKind kind, Uint32 glName);
        static const ObjectHandleEntry* Get(MobileGLBackendHandle handle);
        static Uint64 GetGeneration(MobileGLBackendHandle handle);
        static void Destroy(MobileGLBackendHandle handle);
        static SizeT GetCount();
    };
} // namespace MobileGL::MG_State::GLState

// End of File
