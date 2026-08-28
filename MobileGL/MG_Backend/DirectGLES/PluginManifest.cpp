// MobileGL - MobileGL/MG_Backend/DirectGLES/PluginManifest.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include <Includes.h>
#include "MG_Protocol/bfa.h"

// bfa.h declares MobileGLBackend as an opaque C type; this plugin TU is the
// first (and only) place that completes it for the DirectGLES adapter.
struct MobileGLBackend {
    const MobileGLBackendVTable* VTable;
};

// Phase 5 wires the real DirectGLES backend behind the BFA vtable. The null
// adapter below lets FullServer negotiate ABI, create a backend and run the
// lifecycle without a GPU; the real DirectGLES sources replace it in Phase 5.
namespace {
    uint32_t GetAbiVersion() {
        return (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR;
    }

    bool InitializeBackend(MobileGLBackend* self, const MobileGLBackendInitInfo* info) {
        (void)self;
        (void)info;
        return true;
    }

    void ShutdownBackend(MobileGLBackend* self) {
        (void)self;
    }

    void ClearBackend(MobileGLBackend* self, MobileGLSessionId session, uint32_t mask) {
        (void)self;
        (void)session;
        (void)mask;
    }

    void ClearColorBackend(MobileGLBackend* self, MobileGLSessionId session,
                           float red, float green, float blue, float alpha) {
        (void)self;
        (void)session;
        (void)red;
        (void)green;
        (void)blue;
        (void)alpha;
    }

    void DrawArraysBackend(MobileGLBackend* self, MobileGLSessionId session,
                           uint32_t mode, int32_t first, int32_t count) {
        (void)self;
        (void)session;
        (void)mode;
        (void)first;
        (void)count;
    }

    void DrawElementsBackend(MobileGLBackend* self, MobileGLSessionId session,
                             uint32_t mode, int32_t count, uint32_t type, const void* indices) {
        (void)self;
        (void)session;
        (void)mode;
        (void)count;
        (void)type;
        (void)indices;
    }

    void BufferSubDataBackend(MobileGLBackend* self, MobileGLSessionId session,
                              MobileGLBackendHandle buffer, uint64_t offset, uint64_t size,
                              const void* data) {
        (void)self;
        (void)session;
        (void)buffer;
        (void)offset;
        (void)size;
        (void)data;
    }

    void MemoryBarrierBackend(MobileGLBackend* self, MobileGLSessionId session,
                              uint32_t barriers) {
        (void)self;
        (void)session;
        (void)barriers;
    }

    void MemoryBarrierByRegionBackend(MobileGLBackend* self, MobileGLSessionId session,
                                      uint32_t barriers) {
        (void)self;
        (void)session;
        (void)barriers;
    }

    void PatchParameteriBackend(MobileGLBackend* self, MobileGLSessionId session,
                               uint32_t pname, int32_t value) {
        (void)self;
        (void)session;
        (void)pname;
        (void)value;
    }

    void GenerateMipmapBackend(MobileGLBackend* self, MobileGLSessionId session,
                               uint32_t target) {
        (void)self;
        (void)session;
        (void)target;
    }

    void DispatchComputeBackend(MobileGLBackend* self, MobileGLSessionId session,
                                uint32_t x, uint32_t y, uint32_t z) {
        (void)self;
        (void)session;
        (void)x;
        (void)y;
        (void)z;
    }

    void DispatchComputeIndirectBackend(MobileGLBackend* self, MobileGLSessionId session,
                                        int64_t indirectOffset) {
        (void)self;
        (void)session;
        (void)indirectOffset;
    }

    void BeginTransformFeedbackBackend(MobileGLBackend* self, MobileGLSessionId session,
                                       uint32_t primitiveMode) {
        (void)self;
        (void)session;
        (void)primitiveMode;
    }

    void EndTransformFeedbackBackend(MobileGLBackend* self, MobileGLSessionId session) {
        (void)self;
        (void)session;
    }

    void PauseTransformFeedbackBackend(MobileGLBackend* self, MobileGLSessionId session) {
        (void)self;
        (void)session;
    }

    void ResumeTransformFeedbackBackend(MobileGLBackend* self, MobileGLSessionId session) {
        (void)self;
        (void)session;
    }

    void BindTransformFeedbackBackend(MobileGLBackend* self, MobileGLSessionId session,
                                      uint32_t name) {
        (void)self;
        (void)session;
        (void)name;
    }

    void BlitFramebufferBackend(MobileGLBackend* self, MobileGLSessionId session,
                                MobileGLBackendHandle readFramebuffer,
                                MobileGLBackendHandle drawFramebuffer,
                                int32_t srcX0, int32_t srcY0, int32_t srcX1, int32_t srcY1,
                                int32_t dstX0, int32_t dstY0, int32_t dstX1, int32_t dstY1,
                                uint32_t mask, uint32_t filter) {
        (void)self;
        (void)session;
        (void)readFramebuffer;
        (void)drawFramebuffer;
        (void)srcX0;
        (void)srcY0;
        (void)srcX1;
        (void)srcY1;
        (void)dstX0;
        (void)dstY0;
        (void)dstX1;
        (void)dstY1;
        (void)mask;
        (void)filter;
    }

    bool OnSessionCreatedBackend(MobileGLBackend* self, MobileGLSessionId session,
                                 const MobileGLBackendInitInfo* info) {
        (void)self;
        (void)session;
        (void)info;
        return true;
    }

    void OnSessionDestroyedBackend(MobileGLBackend* self, MobileGLSessionId session) {
        (void)self;
        (void)session;
    }

    bool OnDisplayCreatedBackend(MobileGLBackend* self, MobileGLDisplayId display,
                                 const void* nativeDisplay) {
        (void)self;
        (void)display;
        (void)nativeDisplay;
        return true;
    }

    void OnDisplayDestroyedBackend(MobileGLBackend* self, MobileGLDisplayId display) {
        (void)self;
        (void)display;
    }

    bool OnSharedGroupCreatedBackend(MobileGLBackend* self, MobileGLSharedGroupId group,
                                     const void* shareInfo) {
        (void)self;
        (void)group;
        (void)shareInfo;
        return true;
    }

    void OnSharedGroupDestroyedBackend(MobileGLBackend* self, MobileGLSharedGroupId group) {
        (void)self;
        (void)group;
    }

    // Skeleton vtable: lifecycle + Clear entries live, everything else null
    // until the real DirectGLES adapter lands.
    bool SwapBuffersBackend(MobileGLBackend* self, MobileGLSessionId session,
                            MobileGLBackendHandle draw);
    const MobileGLBackendVTable s_backendVTable = {
        .structSize = sizeof(MobileGLBackendVTable),
        .apiVersion = (MOBILEGL_BFA_ABI_MAJOR << 16) | MOBILEGL_BFA_ABI_MINOR,
        .Initialize = &InitializeBackend,
        .Shutdown = &ShutdownBackend,
        .OnDisplayCreated = &OnDisplayCreatedBackend,
        .OnDisplayDestroyed = &OnDisplayDestroyedBackend,
        .OnSharedGroupCreated = &OnSharedGroupCreatedBackend,
        .OnSharedGroupDestroyed = &OnSharedGroupDestroyedBackend,
        .OnSessionCreated = &OnSessionCreatedBackend,
        .OnSessionDestroyed = &OnSessionDestroyedBackend,
        .Clear = &ClearBackend,
        .ClearColor = &ClearColorBackend,
        .DrawArrays = &DrawArraysBackend,
        .DrawElements = &DrawElementsBackend,
        .BufferSubData = &BufferSubDataBackend,
        .MemoryBarrier = &MemoryBarrierBackend,
        .MemoryBarrierByRegion = &MemoryBarrierByRegionBackend,
        .PatchParameteri = &PatchParameteriBackend,
        .GenerateMipmap = &GenerateMipmapBackend,
        .DispatchCompute = &DispatchComputeBackend,
        .DispatchComputeIndirect = &DispatchComputeIndirectBackend,
        .BeginTransformFeedback = &BeginTransformFeedbackBackend,
        .EndTransformFeedback = &EndTransformFeedbackBackend,
        .PauseTransformFeedback = &PauseTransformFeedbackBackend,
        .ResumeTransformFeedback = &ResumeTransformFeedbackBackend,
        .BindTransformFeedback = &BindTransformFeedbackBackend,
        .BlitFramebuffer = &BlitFramebufferBackend,
        .SwapBuffers = &SwapBuffersBackend
    };

    bool SwapBuffersBackend(MobileGLBackend* self, MobileGLSessionId session,
                            MobileGLBackendHandle draw) {
        (void)self;
        (void)session;
        (void)draw;
        return true;
    }

    const MobileGLBackendVTable* GetBackendVTable() {
        return &s_backendVTable;
    }

    MobileGLBackend* CreateBackend(const MobileGLBackendHost* host,
                                   const MobileGLUtilApi* utilApi,
                                   const MobileGLBackendInitInfo* initInfo) {
        (void)host;
        (void)utilApi;
        (void)initInfo;
        return new MobileGLBackend{&s_backendVTable};
    }

    const MobileGLBackendManifest s_manifest = {
        sizeof(MobileGLBackendManifest),
        MOBILEGL_BFA_ABI_MAJOR,
        MOBILEGL_BFA_ABI_MINOR,
        MobileGLBackendTypeDirectGLES,
        "DirectGLES BackendObject",
        "0.1.0",
        &GetAbiVersion,
        &GetBackendVTable,
        &CreateBackend
    };
} // namespace

extern "C" const MobileGLBackendManifest* mobilegl_backend_manifest(void) {
    return &s_manifest;
}

// End of File
