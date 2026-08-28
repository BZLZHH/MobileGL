// MobileGL - MobileGL/MG_Protocol/transport.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOBILEGL_TRANSPORT_ABI_MAJOR 1u
#define MOBILEGL_TRANSPORT_ABI_MINOR 0u

// ---------------------------------------------------------------------------
// Transport kinds. v1 ships LocalSocketShm; the others are reserved and may be
// added behind the same ops table without a breaking change.
// ---------------------------------------------------------------------------

typedef enum MobileGLTransportKind {
    MobileGLTransportKindLocalSocketShm = 1,
    MobileGLTransportKindInProcess,
    MobileGLTransportKindRemoteTcp,
    MobileGLTransportKindAndroidBinder
} MobileGLTransportKind;

// Opaque transport instance (one per control connection).
typedef struct MobileGLTransport MobileGLTransport;

// A shared-memory region handle passed across the transport boundary.
// PlatformHandle is a descriptor suitable for SCM_RIGHTS (Unix fd) or a
// Windows HANDLE value; on in-process transports it is ignored.
typedef struct MobileGLShmHandle {
    uint32_t structSize;
    int64_t platformHandle;
    uint64_t offset;
    uint64_t size;
    uint64_t capacity;
    void* mappedAddress;
} MobileGLShmHandle;

typedef struct MobileGLTransportConfig {
    uint32_t structSize;
    MobileGLTransportKind kind;
    const char* endpoint;       // socket path / pipe name / URL
    uint32_t maxBatchCommands;
    uint32_t maxShmArenaSize;
    uint32_t timeoutMs;
    uint32_t flags;
    uint32_t reserved;
} MobileGLTransportConfig;

// A batch of commands. FlatBuffers data points to a serialized `Message`
// (or an array of them) but never owns big payloads; payload bytes live in
// shmHandles[].
typedef struct MobileGLCommandBatch {
    uint32_t structSize;
    uint32_t count;
    const void* flatBufferData;
    uint32_t flatBufferSize;
    uint32_t shmHandleCount;
    MobileGLShmHandle* shmHandles;
} MobileGLCommandBatch;

typedef struct MobileGLResponseQueue {
    uint32_t structSize;
    uint32_t count;
    const void* flatBufferData;
    uint32_t flatBufferSize;
    uint32_t shmHandleCount;
    MobileGLShmHandle* shmHandles;
} MobileGLResponseQueue;

// ---------------------------------------------------------------------------
// Transport ops. All entry points return false on any failure and set an
// optional error string (owned by the transport instance, valid until the
// next call).
// ---------------------------------------------------------------------------

typedef struct MobileGLTransportOps {
    uint32_t structSize;
    uint32_t apiVersion;

    bool (*Start)(MobileGLTransport* t, const MobileGLTransportConfig* cfg);
    void (*Shutdown)(MobileGLTransport* t);
    bool (*SubmitCommands)(MobileGLTransport* t, MobileGLCommandBatch* batch);
    bool (*WaitResponses)(MobileGLTransport* t, MobileGLResponseQueue* out, uint32_t timeoutMs);
    bool (*OpenSharedMemory)(MobileGLTransport* t, MobileGLShmHandle* out);
    void (*ReleaseSharedMemory)(MobileGLTransport* t, MobileGLShmHandle* handle);
    const char* (*GetLastError)(MobileGLTransport* t);
} MobileGLTransportOps;

#ifdef __cplusplus
} // extern "C"
#endif

// End of File
