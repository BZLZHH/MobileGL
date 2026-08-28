// MobileGL - MobileGL/Protocol/mgruntime_api.h
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

#define MOBILEGL_RUNTIME_ABI_MAJOR 1u
#define MOBILEGL_RUNTIME_ABI_MINOR 0u

// ---------------------------------------------------------------------------
// Opaque handles. UtilRuntime owns the actual C++ objects; plugins only ever
// see these pointer-sized handles.
// ---------------------------------------------------------------------------

typedef void* MobileGLCompileEnvHandle;
typedef void* MobileGLSpvcSessionHandle;
typedef void* MobileGLTranslationCacheHandle;
typedef void* MobileGLShaderCompilePoolHandle;
typedef void* MobileGLTextureProcessorHandle;

// ---------------------------------------------------------------------------
// Loader domain: OpenGL / Vulkan dynamic loading.
// ---------------------------------------------------------------------------

typedef struct MobileGLLoaderApi {
    uint32_t structSize;
    uint32_t apiVersion;

    void* (*LoadOpenGL)(const char* libraryName);
    void* (*LoadVulkan)(const char* libraryName);
    void* (*GetGLFunction)(void* loader, const char* name);
    void* (*GetVulkanFunction)(void* loader, const char* name);
    bool  (*QueryVersion)(void* loader, uint32_t* major, uint32_t* minor);
    void  (*Unload)(void* loader);
} MobileGLLoaderApi;

// ---------------------------------------------------------------------------
// Shader domain: shader compile / transpile services.
// ---------------------------------------------------------------------------

typedef enum MobileGLShaderCompileTarget {
    MobileGLShaderCompileTargetSPIRV = 1,
    MobileGLShaderCompileTargetGLSLES,
    MobileGLShaderCompileTargetGLSL
} MobileGLShaderCompileTarget;

typedef struct MobileGLShaderCompileInput {
    uint32_t structSize;
    const char* source;
    uint64_t sourceSize;
    uint32_t stage;          // MobileGLShaderStage
    uint32_t target;
    uint32_t entryPoint;
    uint32_t reserved;
    const char* sourceName;
} MobileGLShaderCompileInput;

typedef struct MobileGLShaderCompileOutput {
    uint32_t structSize;
    const void* binary;
    uint64_t binarySize;
    const char* infoLog;
    uint32_t errorCode;
    uint32_t reserved;
} MobileGLShaderCompileOutput;

typedef struct MobileGLShaderApi {
    uint32_t structSize;
    uint32_t apiVersion;

    MobileGLCompileEnvHandle (*CreateCompileEnv)(const void* envOptions);
    void (*DestroyCompileEnv)(MobileGLCompileEnvHandle env);
    bool (*CompileShader)(MobileGLCompileEnvHandle env,
                          const MobileGLShaderCompileInput* input,
                          MobileGLShaderCompileOutput* output);
    MobileGLSpvcSessionHandle (*CreateSpvcSession)(MobileGLCompileEnvHandle env,
                                                   const void* spirvCode, uint64_t spirvSize);
    void (*DestroySpvcSession)(MobileGLSpvcSessionHandle session);
    bool (*SpvcTranspile)(MobileGLSpvcSessionHandle session, const char* entryPoint,
                          uint32_t target, const char** outSource, uint64_t* outSize);
    MobileGLTranslationCacheHandle (*CreateTranslationCache)(const char* cacheDir);
    void (*DestroyTranslationCache)(MobileGLTranslationCacheHandle cache);
    bool (*TranslationCacheGet)(MobileGLTranslationCacheHandle cache, uint64_t key,
                                const void** outBinary, uint64_t* outSize);
    bool (*TranslationCachePut)(MobileGLTranslationCacheHandle cache, uint64_t key,
                                const void* binary, uint64_t size);
} MobileGLShaderApi;

// ---------------------------------------------------------------------------
// Converters domain: enum conversions across GL / MG / Vk / string.
// ---------------------------------------------------------------------------

typedef enum MobileGLEnumConverterFamily {
    MobileGLEnumConverterFamilyBufferTarget = 1,
    MobileGLEnumConverterFamilyTextureTarget,
    MobileGLEnumConverterFamilyTextureInternalFormat,
    MobileGLEnumConverterFamilyPixelFormat,
    MobileGLEnumConverterFamilyPixelType,
    MobileGLEnumConverterFamilyFramebufferTarget,
    MobileGLEnumConverterFamilyRenderState,
    MobileGLEnumConverterFamilyProgram,
    MobileGLEnumConverterFamilyErrorCode,
    MobileGLEnumConverterFamilyEGL,
    MobileGLEnumConverterFamilyVkFormat
} MobileGLEnumConverterFamily;

typedef enum MobileGLEnumConverterDirection {
    MobileGLEnumConverterDirectionGLToMG = 1,
    MobileGLEnumConverterDirectionMGToGL,
    MobileGLEnumConverterDirectionGLToVk,
    MobileGLEnumConverterDirectionMGToVk,
    MobileGLEnumConverterDirectionGLToStr,
    MobileGLEnumConverterDirectionMGToStr,
    MobileGLEnumConverterDirectionMGToGlslang
} MobileGLEnumConverterDirection;

typedef struct MobileGLConvertersApi {
    uint32_t structSize;
    uint32_t apiVersion;

    bool (*ConvertEnum)(MobileGLEnumConverterDirection direction,
                        MobileGLEnumConverterFamily family,
                        uint32_t input, uint32_t* output);
    const char* (*EnumToString)(MobileGLEnumConverterFamily family, uint32_t value);
    bool (*IsValidEnum)(MobileGLEnumConverterFamily family, uint32_t value);
} MobileGLConvertersApi;

// ---------------------------------------------------------------------------
// Metrics domain: texture / buffer metrics.
// ---------------------------------------------------------------------------

typedef struct MobileGLBufferMetrics {
    uint64_t totalSizeBytes;
    uint32_t activeObjectCount;
    uint32_t transientObjectCount;
    uint32_t mappedObjectCount;
    uint32_t reserved;
} MobileGLBufferMetrics;

typedef struct MobileGLTextureMetrics {
    uint64_t totalSizeBytes;
    uint32_t activeObjectCount;
    uint32_t transientObjectCount;
    uint32_t shareGroupCount;
    uint32_t reserved;
} MobileGLTextureMetrics;

typedef struct MobileGLMetricsApi {
    uint32_t structSize;
    uint32_t apiVersion;

    bool (*GetBufferMetrics)(MobileGLBufferMetrics* out);
    bool (*GetTextureMetrics)(MobileGLTextureMetrics* out);
} MobileGLMetricsApi;

// ---------------------------------------------------------------------------
// Math domain: half-float, fixed point, vector helpers.
// ---------------------------------------------------------------------------

typedef struct MobileGLMathApi {
    uint32_t structSize;
    uint32_t apiVersion;

    float (*HalfFloatToFloat)(uint16_t value);
    uint16_t (*FloatToHalfFloat)(float value);
    uint16_t (*FixedToHalfFloat)(int32_t value);
    int32_t (*HalfFloatToFixed)(uint16_t value);
} MobileGLMathApi;

// ---------------------------------------------------------------------------
// Texture domain: format processor / pixel store processor.
// ---------------------------------------------------------------------------

typedef struct MobileGLTextureApi {
    uint32_t structSize;
    uint32_t apiVersion;

    MobileGLTextureProcessorHandle (*CreateTextureFormatProcessor)(uint32_t internalFormat);
    void (*DestroyTextureFormatProcessor)(MobileGLTextureProcessorHandle processor);
    bool (*TextureFormatQuery)(MobileGLTextureProcessorHandle processor,
                               uint32_t query, uint32_t* outValue);
    bool (*ComputeTextureDataSize)(MobileGLTextureProcessorHandle processor,
                                   uint32_t width, uint32_t height, uint32_t depth,
                                   uint32_t format, uint32_t type, uint64_t* outSize);
    bool (*PixelStoreValidate)(const void* pixelStore, uint32_t pixelStoreSize,
                               uint32_t width, uint32_t height, uint32_t format, uint32_t type);
    bool (*PixelStoreProcess)(const void* pixelStore, uint32_t pixelStoreSize,
                              const void* src, void* dst,
                              uint32_t width, uint32_t height, uint32_t format, uint32_t type);
} MobileGLTextureApi;

// ---------------------------------------------------------------------------
// Async domain: shader compile pool.
// ---------------------------------------------------------------------------

typedef struct MobileGLAsyncApi {
    uint32_t structSize;
    uint32_t apiVersion;

    MobileGLShaderCompilePoolHandle (*CreateShaderCompilePool)(uint32_t maxConcurrentJobs);
    void (*DestroyShaderCompilePool)(MobileGLShaderCompilePoolHandle pool);
    bool (*SubmitCompile)(MobileGLShaderCompilePoolHandle pool, MobileGLCompileEnvHandle env,
                          const MobileGLShaderCompileInput* input, uint64_t* outJobId);
    bool (*WaitCompile)(MobileGLShaderCompilePoolHandle pool, uint64_t jobId,
                        MobileGLShaderCompileOutput* output, uint32_t timeoutMs);
    bool (*PollCompile)(MobileGLShaderCompilePoolHandle pool, uint64_t jobId, bool* finished);
    void (*WaitAll)(MobileGLShaderCompilePoolHandle pool);
} MobileGLAsyncApi;

// ---------------------------------------------------------------------------
// Root RuntimeApi. FullServer dlopens MobileGL_UtilRuntime.so, obtains the
// root vtable, and injects the pointer into BackendObject plugins at Create.
// ---------------------------------------------------------------------------

typedef struct MobileGLUtilApi {
    uint32_t structSize;
    uint32_t abiMajor;
    uint32_t abiMinor;

    const MobileGLLoaderApi* loaders;
    const MobileGLShaderApi* shader;
    const MobileGLConvertersApi* converters;
    const MobileGLMetricsApi* metrics;
    const MobileGLMathApi* math;
    const MobileGLTextureApi* texture;
    const MobileGLAsyncApi* async;
} MobileGLUtilApi;

// UtilRuntime export: returns the root API table with the requested ABI.
// FullServer must validate structSize / abiMajor before use.
const MobileGLUtilApi* mobilegl_util_api(uint32_t requestedStructSize,
                                         uint32_t requestedAbiMajor);

#ifdef __cplusplus
} // extern "C"
#endif

// End of File
