// MobileGL - MobileGL/Protocol/bfa.h
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

// ---------------------------------------------------------------------------
// ABI version
// ---------------------------------------------------------------------------

#define MOBILEGL_BFA_ABI_MAJOR 1u
#define MOBILEGL_BFA_ABI_MINOR 0u

// ---------------------------------------------------------------------------
// IDs / handles. All values are allocated by FullServer and are never reused
// for the lifetime of the process. Zero is the invalid value.
// ---------------------------------------------------------------------------

typedef uint64_t MobileGLDisplayId;
typedef uint64_t MobileGLSharedGroupId;
typedef uint64_t MobileGLSessionId;
typedef uint64_t MobileGLBackendHandle;

typedef enum MobileGLObjectKind {
    MobileGLObjectKindBuffer = 1,
    MobileGLObjectKindTexture,
    MobileGLObjectKindSampler,
    MobileGLObjectKindVertexArray,
    MobileGLObjectKindProgram,
    MobileGLObjectKindShader,
    MobileGLObjectKindProgramPipeline,
    MobileGLObjectKindFramebuffer,
    MobileGLObjectKindRenderbuffer,
    MobileGLObjectKindTransformFeedback,
    MobileGLObjectKindQuery,
    MobileGLObjectKindSync,
    MobileGLObjectKindCount
} MobileGLObjectKind;

typedef enum MobileGLBackendType {
    MobileGLBackendTypeDirectGLES = 1,
    MobileGLBackendTypeDirectVulkan = 2
} MobileGLBackendType;

// Forward declaration; the full definition lives in mgruntime_api.h and is
// injected by FullServer at Create time.
typedef struct MobileGLUtilApi MobileGLUtilApi;

typedef enum MobileGLShaderStage {
    MobileGLShaderStageVertex = 1,
    MobileGLShaderStageTessellationControl,
    MobileGLShaderStageTessellationEvaluation,
    MobileGLShaderStageGeometry,
    MobileGLShaderStageFragment,
    MobileGLShaderStageCompute
} MobileGLShaderStage;

// ---------------------------------------------------------------------------
// Session snapshot structures. Each table is POD, starts with structSize, and
// carries the generations the backend cache keys on. FullServer owns the data;
// the pointer stays valid only for the duration of the Host call.
// ---------------------------------------------------------------------------

#define MOBILEGL_MAX_TEXTURE_UNITS 32u
#define MOBILEGL_MAX_UNIFORM_BUFFER_BINDINGS 24u
#define MOBILEGL_MAX_SHADER_STORAGE_BUFFER_BINDINGS 32u
#define MOBILEGL_MAX_VERTEX_ATTRIBUTES 32u
#define MOBILEGL_MAX_FRAMEBUFFER_ATTACHMENTS 8u
#define MOBILEGL_MAX_PROGRAM_STAGES 6u

typedef struct MobileGLBufferInfo {
    uint32_t structSize;
    uint64_t generation;
    uint64_t contentGeneration;
    uint64_t bindingGeneration;
    uint64_t size;
    uint32_t target;
    uint32_t usage;
    uint32_t storageFlags;
    uint32_t reserved;
} MobileGLBufferInfo;

typedef struct MobileGLTextureMipInfo {
    uint32_t structSize;
    uint64_t generation;
    uint64_t size;
    uint32_t format;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t reserved;
} MobileGLTextureMipInfo;

typedef struct MobileGLTextureInfo {
    uint32_t structSize;
    uint64_t generation;
    uint64_t contentGeneration;
    uint64_t paramsGeneration;
    uint64_t bindGeneration;
    uint64_t samplingGeneration;
    uint32_t kind;          // MobileGLObjectKindTexture
    uint32_t format;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t levels;
    uint32_t layers;
    uint32_t samples;
    uint32_t reserved;
} MobileGLTextureInfo;

typedef struct MobileGLFramebufferAttachment {
    uint64_t generation;
    uint64_t objectHandle;
    uint32_t level;
    uint32_t layer;
} MobileGLFramebufferAttachment;

typedef struct MobileGLFramebufferInfo {
    uint32_t structSize;
    uint64_t generation;
    uint32_t width;
    uint32_t height;
    uint32_t samples;
    uint32_t attachmentCount;
    uint32_t reserved;
    MobileGLFramebufferAttachment Attachments[MOBILEGL_MAX_FRAMEBUFFER_ATTACHMENTS];
} MobileGLFramebufferInfo;

typedef struct MobileGLVertexArrayAttribute {
    uint64_t generation;
    uint64_t bufferHandle;
    uint32_t format;
    uint32_t offset;
    uint32_t stride;
    uint32_t divisor;
    uint32_t size;
    uint32_t normalized;
} MobileGLVertexArrayAttribute;

typedef struct MobileGLVertexArrayInfo {
    uint32_t structSize;
    uint64_t generation;
    uint32_t attributeCount;
    uint32_t elementBufferHandle;
    uint32_t reserved;
    MobileGLVertexArrayAttribute Attributes[MOBILEGL_MAX_VERTEX_ATTRIBUTES];
} MobileGLVertexArrayInfo;

typedef struct MobileGLSamplerInfo {
    uint32_t structSize;
    uint64_t generation;
    uint32_t minFilter;
    uint32_t magFilter;
    uint32_t wrapS;
    uint32_t wrapT;
    uint32_t wrapR;
    uint32_t compareMode;
    uint32_t compareFunc;
    uint32_t maxAnisotropy;
    uint32_t reserved;
} MobileGLSamplerInfo;

typedef struct MobileGLProgramStageModule {
    uint32_t structSize;
    uint64_t generation;
    const void* spirvCode;
    uint64_t spirvSize;
    uint32_t stage;
    uint32_t reserved;
    const char* entryPoint;
} MobileGLProgramStageModule;

typedef struct MobileGLProgramInfo {
    uint32_t structSize;
    uint64_t generation;
    uint64_t linkGeneration;
    uint64_t backendStateGeneration;
    uint64_t uniformWriteSetGeneration;
    uint64_t uboContentGeneration;
    uint64_t blockBindingGeneration;
    uint32_t stageCount;
    uint32_t reserved;
    MobileGLProgramStageModule Stages[MOBILEGL_MAX_PROGRAM_STAGES];
} MobileGLProgramInfo;

typedef struct MobileGLRenderbufferInfo {
    uint32_t structSize;
    uint64_t generation;
    uint32_t format;
    uint32_t width;
    uint32_t height;
    uint32_t samples;
    uint32_t reserved;
} MobileGLRenderbufferInfo;

typedef struct MobileGLPixelStore {
    uint32_t structSize;
    uint32_t alignment;
    int32_t rowLength;
    int32_t imageHeight;
    int32_t skipRows;
    int32_t skipPixels;
    int32_t skipImages;
    uint32_t reserved;
} MobileGLPixelStore;

typedef struct MobileGLRenderState {
    uint32_t structSize;
    uint64_t version;
    uint64_t pipelineVersion;
    uint32_t frontFace;
    uint32_t cullFaceMode;
    uint32_t depthFunc;
    uint32_t stencilFuncFront;
    uint32_t stencilFuncBack;
    uint32_t blendEquationRgb;
    uint32_t blendEquationAlpha;
    uint32_t blendFuncSrcRgb;
    uint32_t blendFuncDstRgb;
    uint32_t blendFuncSrcAlpha;
    uint32_t blendFuncDstAlpha;
    uint32_t polygonModeFront;
    uint32_t polygonModeBack;
    uint32_t colorMask;
    uint32_t depthMask;
    uint32_t stencilMaskFront;
    uint32_t stencilMaskBack;
    uint32_t sampleMask;
    uint32_t reserved;
} MobileGLRenderState;

typedef struct MobileGLSessionDrawState {
    uint32_t structSize;
    uint64_t currentProgram;
    uint64_t currentPipeline;
    uint64_t currentVertexArray;
    uint64_t currentDrawFramebuffer;
    uint64_t currentReadFramebuffer;
    uint32_t activeTexture;
    uint32_t drawStateVersion;
    uint32_t reserved;

    uint64_t BoundUniformBuffers[MOBILEGL_MAX_UNIFORM_BUFFER_BINDINGS];
    uint64_t BoundShaderStorageBuffers[MOBILEGL_MAX_SHADER_STORAGE_BUFFER_BINDINGS];
    uint64_t BoundTextures[MOBILEGL_MAX_TEXTURE_UNITS];
    uint64_t BoundSamplers[MOBILEGL_MAX_TEXTURE_UNITS];
} MobileGLSessionDrawState;

// ---------------------------------------------------------------------------
// Capabilities
// ---------------------------------------------------------------------------

typedef struct MobileGLRendererInfo {
    uint32_t structSize;
    const char* name;
    const char* vendor;
    const char* version;
    const char* shaderLanguageVersion;
    const char* const* extensions;
    uint32_t extensionCount;
    uint32_t reserved;
} MobileGLRendererInfo;

typedef struct MobileGLDynamicParameters {
    uint32_t structSize;
    uint64_t uniformBufferOffsetAlignment;
    uint64_t shaderStorageBufferOffsetAlignment;
    uint64_t maxShaderStorageBlockSize;
    uint32_t maxTextureSize;
    uint32_t max3DTextureSize;
    uint32_t maxArrayTextureLayers;
    uint32_t maxCubeMapTextureSize;
    uint32_t maxRenderbufferSize;
    uint32_t maxFramebufferWidth;
    uint32_t maxFramebufferHeight;
    uint32_t maxFramebufferLayers;
    uint32_t maxColorTextureSamples;
    uint32_t maxDepthTextureSamples;
    uint32_t maxFramebufferSamples;
    uint32_t maxIntegerSamples;
    uint32_t maxSamples;
    uint32_t maxTextureImageUnits;
    uint32_t maxVertexTextureImageUnits;
    uint32_t maxComputeTextureImageUnits;
    uint32_t maxCombinedTextureImageUnits;
    uint32_t maxVertexAttribs;
    uint32_t maxViewports;
    uint32_t maxClipDistances;
    uint32_t maxCullDistances;
    uint32_t maxCombinedClipAndCullDistances;
    uint32_t maxPatchVertices;
    uint32_t maxTessGenLevel;
    uint32_t maxDrawBuffers;
    uint32_t maxColorAttachments;
    uint32_t maxUniformBufferBindings;
    uint32_t maxShaderStorageBufferBindings;
    uint32_t maxTextureBufferSize;
    uint32_t maxImageUnits;
    uint32_t maxCombinedImageUniforms;
    uint32_t maxComputeWorkGroupInvocations;
    uint32_t maxUniformBlockSize;
    uint32_t subgroupSize;
    uint32_t vkSubgroupSupportedStages;
    uint32_t vkSubgroupSupportedFeatures;
    float    maxTextureMaxAnisotropy;
    float    pointSizeRangeMin;
    float    pointSizeRangeMax;
    float    pointSizeGranularity;
    float    lineWidthRangeMin;
    float    lineWidthRangeMax;
    float    smoothLineWidthRangeMin;
    float    smoothLineWidthRangeMax;
    float    smoothLineWidthGranularity;
    float    viewportBoundsRangeMin;
    float    viewportBoundsRangeMax;
    float    minFragmentInterpolationOffset;
    float    maxFragmentInterpolationOffset;
    uint32_t fragmentInterpolationOffsetBits;
    uint32_t viewportSubpixelBits;
    uint32_t textureBufferOffsetAlignment;
    uint32_t layerProvokingVertex;
    uint32_t viewportIndexProvokingVertex;
    uint32_t perLayerFramebufferAttachmentTargets;
    uint32_t gpuVendorKind;
    uint32_t supportsWideLines;
    uint32_t supportsDistinctDepthStencilAttachments;
    uint32_t supportsShaderFloat64;
    uint32_t supportsFloat64VertexAttributes;
    uint32_t supportsTessellationPointSize;
    uint32_t supportsGeometryPointSize;
    uint32_t subgroupQuadOperationsInAllStages;
    uint32_t reserved;
} MobileGLDynamicParameters;

#define MOBILEGL_FORMAT_CAPABILITY_TARGET_COUNT 64u
#define MOBILEGL_FORMAT_CAPABILITY_FORMAT_COUNT 256u

typedef struct MobileGLFormatCapabilityCache {
    uint32_t structSize;
    uint64_t fullCaps[MOBILEGL_FORMAT_CAPABILITY_TARGET_COUNT][MOBILEGL_FORMAT_CAPABILITY_FORMAT_COUNT];
    uint64_t caveatCaps[MOBILEGL_FORMAT_CAPABILITY_TARGET_COUNT][MOBILEGL_FORMAT_CAPABILITY_FORMAT_COUNT];
} MobileGLFormatCapabilityCache;

// ---------------------------------------------------------------------------
// Backend object (opaque)
// ---------------------------------------------------------------------------

typedef struct MobileGLBackend MobileGLBackend;

typedef struct MobileGLBackendInitInfo {
    uint32_t structSize;
    uint32_t apiVersion;
    MobileGLBackendType backendType;
    const char* pluginPath;
    const char* runtimeVersion;
    uint32_t flags;
    uint32_t reserved;
} MobileGLBackendInitInfo;

// ---------------------------------------------------------------------------
// BackendHostVTable: FullServer -> Backend (queries + errors)
// ---------------------------------------------------------------------------

typedef struct MobileGLBackendHost {
    uint32_t structSize;
    uint32_t apiVersion;

    // Session state queries.
    bool (*GetSessionDrawState)(MobileGLSessionId session, MobileGLSessionDrawState* out);
    bool (*GetRenderState)(MobileGLSessionId session, uint64_t desiredVersion, MobileGLRenderState* out);
    bool (*GetPixelStore)(MobileGLSessionId session, int isUnpack, MobileGLPixelStore* out);

    // Object information with generations.
    bool (*GetBufferInfo)(MobileGLBackendHandle h, MobileGLBufferInfo* out);
    bool (*GetTextureInfo)(MobileGLBackendHandle h, MobileGLTextureInfo* out);
    bool (*GetTextureMipInfo)(MobileGLBackendHandle h, uint32_t level, MobileGLTextureMipInfo* out);
    bool (*GetFramebufferInfo)(MobileGLBackendHandle h, MobileGLFramebufferInfo* out);
    bool (*GetVertexArrayInfo)(MobileGLBackendHandle h, MobileGLVertexArrayInfo* out);
    bool (*GetSamplerInfo)(MobileGLBackendHandle h, MobileGLSamplerInfo* out);
    bool (*GetProgramInfo)(MobileGLBackendHandle h, MobileGLProgramInfo* out);
    bool (*GetProgramModules)(MobileGLBackendHandle h, MobileGLProgramStageModule* out, uint32_t* inOutCount);
    bool (*GetRenderbufferInfo)(MobileGLBackendHandle h, MobileGLRenderbufferInfo* out);

    // Bulk data.
    bool (*GetBufferShadow)(MobileGLSessionId session, MobileGLBackendHandle h,
                            const void** data, uint64_t* size);
    bool (*ReadBufferRange)(MobileGLSessionId session, MobileGLBackendHandle h,
                            uint64_t offset, uint64_t size, void* dst);
    bool (*GetTextureMipData)(MobileGLBackendHandle h, uint32_t level, uint32_t layer,
                              const void** data, uint64_t* size);

    // Errors / logging / capability changes.
    void (*RecordError)(MobileGLSessionId session, uint32_t glErrorCode, const char* message);
    void (*Log)(int level, const char* message);
    void (*CapabilityChanged)(MobileGLBackend* backend);
} MobileGLBackendHost;

// ---------------------------------------------------------------------------
// BackendObjectVTable: Backend -> GPU (lifecycle, EGL, GL commands, resources)
// ---------------------------------------------------------------------------

typedef struct MobileGLSurfaceCreateInfo {
    uint32_t structSize;
    uint32_t backendType;     // native window backend: 1=X11, 2=Win32, 3=Android, 4=MetalLayer
    void* nativeWindow;
    uint32_t width;
    uint32_t height;
    uint32_t reserved;
} MobileGLSurfaceCreateInfo;

typedef struct MobileGLBufferOps {
    const void* initialData;
    uint64_t initialDataSize;
} MobileGLBufferOps;

typedef struct MobileGLTextureUpload {
    uint32_t level;
    uint32_t layer;
    uint32_t format;
    uint32_t type;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    const void* data;
    uint64_t dataSize;
} MobileGLTextureUpload;

typedef struct MobileGLBackendVTable {
    uint32_t structSize;
    uint32_t apiVersion;

    // Lifecycle.
    bool (*Initialize)(MobileGLBackend* self, const MobileGLBackendInitInfo* info);
    void (*Shutdown)(MobileGLBackend* self);

    // Display / SharedGroup / Session lifecycle.
    bool (*OnDisplayCreated)(MobileGLBackend* self, MobileGLDisplayId display, const void* nativeDisplay);
    void (*OnDisplayDestroyed)(MobileGLBackend* self, MobileGLDisplayId display);
    bool (*OnSharedGroupCreated)(MobileGLBackend* self, MobileGLSharedGroupId group,
                                 const void* shareInfo);
    void (*OnSharedGroupDestroyed)(MobileGLBackend* self, MobileGLSharedGroupId group);
    bool (*OnSessionCreated)(MobileGLBackend* self, MobileGLSessionId session,
                             const MobileGLBackendInitInfo* info);
    void (*OnSessionDestroyed)(MobileGLBackend* self, MobileGLSessionId session);

    // Object lifecycle.
    void (*OnObjectCreated)(MobileGLBackend* self, MobileGLObjectKind kind,
                            MobileGLBackendHandle handle);
    void (*OnObjectDestroyed)(MobileGLBackend* self, MobileGLObjectKind kind,
                              MobileGLBackendHandle handle);

    // EGL / Surface.
    bool (*InitializeEGLDisplay)(MobileGLBackend* self, MobileGLDisplayId display, int32_t* major, int32_t* minor);
    bool (*CreateWindowSurface)(MobileGLBackend* self, MobileGLDisplayId display, MobileGLBackendHandle surface,
                                const MobileGLSurfaceCreateInfo* info);
    bool (*CreatePbufferSurface)(MobileGLBackend* self, MobileGLDisplayId display, MobileGLBackendHandle surface,
                                 int32_t width, int32_t height);
    bool (*ResizeSurface)(MobileGLBackend* self, MobileGLDisplayId display, MobileGLBackendHandle surface,
                          uint32_t width, uint32_t height);
    bool (*MakeEGLCurrent)(MobileGLBackend* self, MobileGLSessionId session,
                           MobileGLBackendHandle draw, MobileGLBackendHandle read);
    bool (*SwapBuffers)(MobileGLBackend* self, MobileGLSessionId session, MobileGLBackendHandle draw);
    void (*SetSwapInterval)(MobileGLBackend* self, MobileGLSessionId session, int32_t interval);
    void (*ReleaseEGLSurface)(MobileGLBackend* self, MobileGLDisplayId display, MobileGLBackendHandle surface);
    void (*ReleaseEGLResources)(MobileGLBackend* self, MobileGLDisplayId display, MobileGLSessionId session);

    // GL commands (session-keyed).
    void (*DrawArrays)(MobileGLBackend* self, MobileGLSessionId session,
                       uint32_t mode, int32_t first, int32_t count);
    void (*DrawElements)(MobileGLBackend* self, MobileGLSessionId session,
                         uint32_t mode, int32_t count, uint32_t type, const void* indices);
    void (*DrawElementsBaseVertex)(MobileGLBackend* self, MobileGLSessionId session,
                                   uint32_t mode, int32_t count, uint32_t type,
                                   const void* indices, int32_t baseVertex);
    void (*MultiDrawArrays)(MobileGLBackend* self, MobileGLSessionId session,
                            uint32_t mode, const int32_t* first, const int32_t* count, int32_t drawCount);
    void (*MultiDrawElements)(MobileGLBackend* self, MobileGLSessionId session,
                              uint32_t mode, const int32_t* count, uint32_t type,
                              const void* const* indices, int32_t drawCount);
    void (*MultiDrawArraysIndirect)(MobileGLBackend* self, MobileGLSessionId session,
                                    uint32_t mode, const void* indirect, int32_t drawCount, int32_t stride);
    void (*MultiDrawElementsIndirect)(MobileGLBackend* self, MobileGLSessionId session,
                                      uint32_t mode, uint32_t type, const void* indirect,
                                      int32_t drawCount, int32_t stride);
    void (*DrawArraysInstanced)(MobileGLBackend* self, MobileGLSessionId session,
                                uint32_t mode, int32_t first, int32_t count, int32_t instanceCount);
    void (*DrawElementsInstanced)(MobileGLBackend* self, MobileGLSessionId session,
                                  uint32_t mode, int32_t count, uint32_t type,
                                  const void* indices, int32_t instanceCount);
    void (*DrawRangeElements)(MobileGLBackend* self, MobileGLSessionId session,
                              uint32_t mode, uint32_t start, uint32_t end,
                              int32_t count, uint32_t type, const void* indices);
    void (*DrawArraysIndirect)(MobileGLBackend* self, MobileGLSessionId session,
                               uint32_t mode, const void* indirect);
    void (*DrawElementsIndirect)(MobileGLBackend* self, MobileGLSessionId session,
                                 uint32_t mode, uint32_t type, const void* indirect);
    void (*Clear)(MobileGLBackend* self, MobileGLSessionId session, uint32_t mask);
    void (*ClearColor)(MobileGLBackend* self, MobileGLSessionId session,
                       float red, float green, float blue, float alpha);
    void (*ClearBufferfv)(MobileGLBackend* self, MobileGLSessionId session,
                          uint32_t buffer, int32_t drawBuffer, const float* value);
    void (*ClearBufferiv)(MobileGLBackend* self, MobileGLSessionId session,
                          uint32_t buffer, int32_t drawBuffer, const int32_t* value);
    void (*ClearBufferuiv)(MobileGLBackend* self, MobileGLSessionId session,
                           uint32_t buffer, int32_t drawBuffer, const uint32_t* value);
    void (*ClearBufferfi)(MobileGLBackend* self, MobileGLSessionId session,
                          uint32_t buffer, int32_t drawBuffer, float depth, int32_t stencil);
    void (*ClearNamedFramebufferfv)(MobileGLBackend* self, MobileGLSessionId session,
                                    MobileGLBackendHandle framebuffer, uint32_t buffer,
                                    int32_t drawBuffer, const float* value);
    void (*ClearNamedFramebufferiv)(MobileGLBackend* self, MobileGLSessionId session,
                                    MobileGLBackendHandle framebuffer, uint32_t buffer,
                                    int32_t drawBuffer, const int32_t* value);
    void (*ClearNamedFramebufferuiv)(MobileGLBackend* self, MobileGLSessionId session,
                                     MobileGLBackendHandle framebuffer, uint32_t buffer,
                                     int32_t drawBuffer, const uint32_t* value);
    void (*ClearNamedFramebufferfi)(MobileGLBackend* self, MobileGLSessionId session,
                                    MobileGLBackendHandle framebuffer, uint32_t buffer,
                                    int32_t drawBuffer, float depth, int32_t stencil);
    void (*BlitFramebuffer)(MobileGLBackend* self, MobileGLSessionId session,
                            MobileGLBackendHandle readFramebuffer, MobileGLBackendHandle drawFramebuffer,
                            int32_t srcX0, int32_t srcY0, int32_t srcX1, int32_t srcY1,
                            int32_t dstX0, int32_t dstY0, int32_t dstX1, int32_t dstY1,
                            uint32_t mask, uint32_t filter);
    void (*CopyTexImage2D)(MobileGLBackend* self, MobileGLSessionId session,
                           uint32_t target, int32_t level, uint32_t internalFormat,
                           int32_t x, int32_t y, int32_t width, int32_t height, int32_t border);
    void (*CopyTexSubImage2D)(MobileGLBackend* self, MobileGLSessionId session,
                              uint32_t target, int32_t level, int32_t xOffset, int32_t yOffset,
                              int32_t x, int32_t y, int32_t width, int32_t height);
    void (*CopyImageSubData)(MobileGLBackend* self, MobileGLSessionId session,
                             MobileGLObjectKind srcKind, MobileGLBackendHandle src,
                             MobileGLObjectKind dstKind, MobileGLBackendHandle dst,
                             int32_t srcLevel, int32_t srcX, int32_t srcY, int32_t srcZ,
                             int32_t dstLevel, int32_t dstX, int32_t dstY, int32_t dstZ,
                             int32_t srcWidth, int32_t srcHeight, int32_t srcDepth);
    void (*GenerateMipmap)(MobileGLBackend* self, MobileGLSessionId session, uint32_t target);
    void (*ReadPixels)(MobileGLBackend* self, MobileGLSessionId session,
                       int32_t x, int32_t y, int32_t width, int32_t height,
                       uint32_t format, uint32_t type, void* pixels);
    void (*GetTexImage)(MobileGLBackend* self, MobileGLSessionId session,
                        uint32_t target, int32_t level, uint32_t format, uint32_t type, void* pixels);
    void (*GetTextureImage)(MobileGLBackend* self, MobileGLSessionId session,
                            MobileGLBackendHandle texture, uint32_t target, int32_t level,
                            uint32_t format, uint32_t type, int32_t bufSize, void* pixels);
    void (*DispatchCompute)(MobileGLBackend* self, MobileGLSessionId session,
                            uint32_t numGroupsX, uint32_t numGroupsY, uint32_t numGroupsZ);
    void (*DispatchComputeIndirect)(MobileGLBackend* self, MobileGLSessionId session,
                                    int64_t indirectOffset);
    void (*MemoryBarrier)(MobileGLBackend* self, MobileGLSessionId session, uint32_t barriers);
    void (*MemoryBarrierByRegion)(MobileGLBackend* self, MobileGLSessionId session, uint32_t barriers);
    void (*BindImageTexture)(MobileGLBackend* self, MobileGLSessionId session,
                             uint32_t unit, MobileGLBackendHandle texture, int32_t level,
                             int32_t layered, int32_t layer, uint32_t access, uint32_t format);
    void (*PatchParameteri)(MobileGLBackend* self, MobileGLSessionId session, uint32_t pname, int32_t value);
    void (*BeginTransformFeedback)(MobileGLBackend* self, MobileGLSessionId session, uint32_t primitiveMode);
    void (*EndTransformFeedback)(MobileGLBackend* self, MobileGLSessionId session);
    void (*PauseTransformFeedback)(MobileGLBackend* self, MobileGLSessionId session);
    void (*ResumeTransformFeedback)(MobileGLBackend* self, MobileGLSessionId session);
    void (*BindTransformFeedback)(MobileGLBackend* self, MobileGLSessionId session, uint32_t name);
    void (*DeleteTransformFeedback)(MobileGLBackend* self, MobileGLSessionId session, uint32_t name);
    void (*ShaderStorageBlockBinding)(MobileGLBackend* self, MobileGLSessionId session,
                                      MobileGLBackendHandle program, const char* storageBlockName,
                                      uint32_t storageBlockBinding);
    int64_t (*GetGpuTimestampNs)(MobileGLBackend* self, MobileGLSessionId session);

    // Buffer / texture resource operations.
    void (*BufferRespecify)(MobileGLBackend* self, MobileGLSessionId session,
                            MobileGLBackendHandle buffer, uint64_t size,
                            uint32_t usage, const MobileGLBufferOps* ops);
    void (*BufferSubData)(MobileGLBackend* self, MobileGLSessionId session,
                          MobileGLBackendHandle buffer, uint64_t offset,
                          uint64_t size, const void* data);
    void (*BufferFlushMappedRange)(MobileGLBackend* self, MobileGLSessionId session,
                                   MobileGLBackendHandle buffer, uint64_t offset, uint64_t size);
    bool (*BufferAcquirePersistentMap)(MobileGLBackend* self, MobileGLSessionId session,
                                       MobileGLBackendHandle buffer, uint32_t accessFlags,
                                       uint64_t offset, uint64_t size, void** outPtr);
    bool (*BufferReadbackFromGpu)(MobileGLBackend* self, MobileGLSessionId session,
                                  MobileGLBackendHandle buffer, uint64_t offset,
                                  uint64_t size, void* dst);
    void (*TextureRespecify)(MobileGLBackend* self, MobileGLSessionId session,
                             MobileGLBackendHandle texture, const MobileGLTextureUpload* levels,
                             uint32_t levelCount);
    void (*TextureSubImage)(MobileGLBackend* self, MobileGLSessionId session,
                            MobileGLBackendHandle texture, const MobileGLTextureUpload* upload);

    // Sync / timer / query.
    void* (*FenceSync)(MobileGLBackend* self, MobileGLSessionId session, uint32_t condition, uint32_t flags);
    uint32_t (*ClientWaitSync)(MobileGLBackend* self, MobileGLSessionId session,
                               void* sync, uint32_t flags, uint64_t timeout);
    void (*WaitSync)(MobileGLBackend* self, MobileGLSessionId session, void* sync, uint32_t flags, uint64_t timeout);
    void (*DeleteSync)(MobileGLBackend* self, MobileGLSessionId session, void* sync);
    bool (*GetSyncStatus)(MobileGLBackend* self, MobileGLSessionId session, void* sync);
    void* (*BeginTimeElapsedQuery)(MobileGLBackend* self, MobileGLSessionId session);
    void (*EndTimeElapsedQuery)(MobileGLBackend* self, MobileGLSessionId session, void* query);
    void* (*QueryCounterTimestamp)(MobileGLBackend* self, MobileGLSessionId session);
    bool (*IsQueryResultAvailable)(MobileGLBackend* self, MobileGLSessionId session, void* query);
    bool (*GetQueryResult64)(MobileGLBackend* self, MobileGLSessionId session,
                             void* query, bool wait, uint64_t* outNanoseconds);
    void (*DeleteBackendQuery)(MobileGLBackend* self, MobileGLSessionId session, void* query);
    void* (*BeginOcclusionQuery)(MobileGLBackend* self, MobileGLSessionId session);
    void (*EndOcclusionQuery)(MobileGLBackend* self, MobileGLSessionId session, void* query);
    void* (*BeginXfbPrimitivesQuery)(MobileGLBackend* self, MobileGLSessionId session, bool generated);
    void (*EndXfbPrimitivesQuery)(MobileGLBackend* self, MobileGLSessionId session, void* query);

    // Capabilities.
    const MobileGLRendererInfo* (*GetRendererInfo)(MobileGLBackend* self, MobileGLSessionId session);
    const MobileGLDynamicParameters* (*GetDynamicParameters)(MobileGLBackend* self, MobileGLSessionId session);
    bool (*GetFormatCapabilities)(MobileGLBackend* self, MobileGLSessionId session,
                                  MobileGLFormatCapabilityCache* out);
    bool (*GetFormatSampleCounts)(MobileGLBackend* self, MobileGLSessionId session,
                                  uint32_t target, uint32_t format, const uint32_t** counts,
                                  uint32_t* outCount);
} MobileGLBackendVTable;

// ---------------------------------------------------------------------------
// Plugin manifest. FullServer dlopens the plugin and calls
// mobilegl_backend_manifest() to negotiate ABI before Create.
// ---------------------------------------------------------------------------

typedef struct MobileGLBackendManifest {
    uint32_t structSize;
    uint32_t abiMajor;
    uint32_t abiMinor;
    uint32_t backendType;
    const char* name;
    const char* version;
    uint32_t (*GetAbiVersion)(void);
    const MobileGLBackendVTable* (*GetBackendVTable)(void);
    MobileGLBackend* (*Create)(const MobileGLBackendHost* host,
                               const MobileGLUtilApi* utilApi,
                               const MobileGLBackendInitInfo* initInfo);
} MobileGLBackendManifest;

#ifdef __cplusplus
} // extern "C"
#endif

// End of File
