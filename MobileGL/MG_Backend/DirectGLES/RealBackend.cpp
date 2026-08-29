// MobileGL - MobileGL/MG_Backend/DirectGLES/RealBackend.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include <Includes.h>
#include "RealBackend.h"
#include <EGL/eglext.h>
#include "MG_Util/Types.h"

namespace {
    using MobileGL::Bool;
    using MobileGL::Int;
    using MobileGL::String;
    using MobileGL::Vector;

    constexpr const char* kEglLibraryNames[] = {
#if defined(_WIN32)
        "libEGL.dll", "EGL.dll"
#else
        "libEGL.so.1", "libEGL.so.0", "libEGL.so"
#endif
    };

    constexpr const char* kGlesLibraryNames[] = {
#if defined(_WIN32)
        "libGLESv2.dll", "GLESv2.dll"
#else
        "libGLESv2.so.2", "libGLESv2.so.1", "libGLESv2.so"
#endif
    };

    void* OpenFirstLibrary(const char* const* names, MobileGL::SizeT count) {
        for (MobileGL::SizeT i = 0; i < count; ++i) {
#if defined(_WIN32)
            HMODULE lib = LoadLibraryA(names[i]);
            if (lib != nullptr) {
                return reinterpret_cast<void*>(lib);
            }
#else
            void* lib = dlopen(names[i], RTLD_NOW | RTLD_LOCAL);
            if (lib != nullptr) {
                return lib;
            }
#endif
        }
        return nullptr;
    }

    void CloseLibrary(void* library) {
        if (library == nullptr) {
            return;
        }
#if defined(_WIN32)
        FreeLibrary(reinterpret_cast<HMODULE>(library));
#else
        dlclose(library);
#endif
    }

    void* GetSymbol(void* library, const char* name) {
        if (library == nullptr) {
            return nullptr;
        }
#if defined(_WIN32)
        return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(library), name));
#else
        return dlsym(library, name);
#endif
    }

    void BackendLog(MobileGLBackend* backend, int level, const char* format, ...) {
        if (backend == nullptr || backend->Host == nullptr || backend->Host->Log == nullptr) {
            return;
        }
        char buffer[512] = {};
        va_list args;
        va_start(args, format);
        (void)std::vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        backend->Host->Log(level, buffer);
    }

    Bool LoadEglFunctions(MobileGLBackend* backend) {
        backend->EglLibrary =
            OpenFirstLibrary(kEglLibraryNames, sizeof(kEglLibraryNames) / sizeof(kEglLibraryNames[0]));
        if (backend->EglLibrary == nullptr) {
            BackendLog(backend, 3, "DirectGLES BFA: no EGL library could be opened");
            return false;
        }

#define MOBILEGL_LOAD_EGL(name)                                                                                        \
    backend->Egl.name = reinterpret_cast<decltype(backend->Egl.name)>(GetSymbol(backend->EglLibrary, #name))
        MOBILEGL_LOAD_EGL(eglGetDisplay);
        MOBILEGL_LOAD_EGL(eglGetPlatformDisplay);
        MOBILEGL_LOAD_EGL(eglInitialize);
        MOBILEGL_LOAD_EGL(eglTerminate);
        MOBILEGL_LOAD_EGL(eglBindAPI);
        MOBILEGL_LOAD_EGL(eglChooseConfig);
        MOBILEGL_LOAD_EGL(eglCreateContext);
        MOBILEGL_LOAD_EGL(eglDestroyContext);
        MOBILEGL_LOAD_EGL(eglCreatePbufferSurface);
        MOBILEGL_LOAD_EGL(eglCreateWindowSurface);
        MOBILEGL_LOAD_EGL(eglCreatePlatformWindowSurface);
        MOBILEGL_LOAD_EGL(eglDestroySurface);
        MOBILEGL_LOAD_EGL(eglMakeCurrent);
        MOBILEGL_LOAD_EGL(eglSwapBuffers);
        MOBILEGL_LOAD_EGL(eglSwapInterval);
        MOBILEGL_LOAD_EGL(eglGetCurrentContext);
        MOBILEGL_LOAD_EGL(eglReleaseThread);
        MOBILEGL_LOAD_EGL(eglGetProcAddress);
#undef MOBILEGL_LOAD_EGL

        return backend->Egl.eglGetDisplay != nullptr && backend->Egl.eglGetProcAddress != nullptr &&
               backend->Egl.eglInitialize != nullptr && backend->Egl.eglMakeCurrent != nullptr;
    }

    void* GetGlesProc(MobileGLBackend* backend, const char* name) {
        if (backend->Egl.eglGetProcAddress != nullptr) {
            const auto proc = backend->Egl.eglGetProcAddress(name);
            if (proc != nullptr) {
                return reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(proc));
            }
        }
        return GetSymbol(backend->GlesLibrary, name);
    }

    Bool LoadGlesFunctions(MobileGLBackend* backend) {
        backend->GlesLibrary =
            OpenFirstLibrary(kGlesLibraryNames, sizeof(kGlesLibraryNames) / sizeof(kGlesLibraryNames[0]));
        if (backend->GlesLibrary == nullptr) {
            BackendLog(backend, 2,
                       "DirectGLES BFA: no GLESv2 library could be opened; falling back to eglGetProcAddress only");
        }

#define MOBILEGL_LOAD_GLES(name)                                                                                       \
    backend->Gl.name = reinterpret_cast<decltype(backend->Gl.name)>(GetGlesProc(backend, #name))
        MOBILEGL_LOAD_GLES(glGetError);
        MOBILEGL_LOAD_GLES(glFinish);
        MOBILEGL_LOAD_GLES(glFlush);
        MOBILEGL_LOAD_GLES(glGetString);
        MOBILEGL_LOAD_GLES(glGetStringi);
        MOBILEGL_LOAD_GLES(glGetIntegerv);
        MOBILEGL_LOAD_GLES(glClear);
        MOBILEGL_LOAD_GLES(glClearColor);
        MOBILEGL_LOAD_GLES(glDrawArrays);
        MOBILEGL_LOAD_GLES(glDrawElements);
        MOBILEGL_LOAD_GLES(glDrawArraysInstanced);
        MOBILEGL_LOAD_GLES(glDrawElementsInstanced);
        MOBILEGL_LOAD_GLES(glDrawRangeElements);
        MOBILEGL_LOAD_GLES(glDrawArraysIndirect);
        MOBILEGL_LOAD_GLES(glDrawElementsIndirect);
        MOBILEGL_LOAD_GLES(glGenBuffers);
        MOBILEGL_LOAD_GLES(glDeleteBuffers);
        MOBILEGL_LOAD_GLES(glBindBuffer);
        MOBILEGL_LOAD_GLES(glBufferData);
        MOBILEGL_LOAD_GLES(glBufferSubData);
        MOBILEGL_LOAD_GLES(glMemoryBarrier);
        MOBILEGL_LOAD_GLES(glMemoryBarrierByRegion);
        MOBILEGL_LOAD_GLES(glPatchParameteri);
        MOBILEGL_LOAD_GLES(glGenerateMipmap);
        MOBILEGL_LOAD_GLES(glDispatchCompute);
        MOBILEGL_LOAD_GLES(glDispatchComputeIndirect);
        MOBILEGL_LOAD_GLES(glBlitFramebuffer);
        MOBILEGL_LOAD_GLES(glReadPixels);
        MOBILEGL_LOAD_GLES(glTexImage2D);
        MOBILEGL_LOAD_GLES(glTexImage3D);
        MOBILEGL_LOAD_GLES(glGenTextures);
        MOBILEGL_LOAD_GLES(glBindTexture);
        MOBILEGL_LOAD_GLES(glBeginTransformFeedback);
        MOBILEGL_LOAD_GLES(glEndTransformFeedback);
        MOBILEGL_LOAD_GLES(glPauseTransformFeedback);
        MOBILEGL_LOAD_GLES(glResumeTransformFeedback);
        MOBILEGL_LOAD_GLES(glBindTransformFeedback);
        MOBILEGL_LOAD_GLES(glGenTransformFeedbacks);
        MOBILEGL_LOAD_GLES(glDeleteTransformFeedbacks);
        MOBILEGL_LOAD_GLES(glFenceSync);
        MOBILEGL_LOAD_GLES(glDeleteSync);
        MOBILEGL_LOAD_GLES(glClientWaitSync);
        MOBILEGL_LOAD_GLES(glWaitSync);
        MOBILEGL_LOAD_GLES(glGetSynciv);
#undef MOBILEGL_LOAD_GLES

        return true;
    }

    Int QueryGlInt(const MobileGL::MG_External::GLESFunctionsTable& gl, GLenum pname, Int fallback) {
        if (gl.glGetIntegerv == nullptr) {
            return fallback;
        }
        GLint value = static_cast<GLint>(fallback);
        gl.glGetIntegerv(pname, &value);
        return static_cast<Int>(value);
    }

    String QueryGlString(const MobileGL::MG_External::GLESFunctionsTable& gl, GLenum name) {
        if (gl.glGetString == nullptr) {
            return {};
        }
        const GLubyte* text = gl.glGetString(name);
        return text == nullptr ? String() : String(reinterpret_cast<const char*>(text));
    }

    Bool EnsureDisplay(MobileGLBackend* backend) {
        if (backend->DisplayInitialized && backend->Display != EGL_NO_DISPLAY) {
            return true;
        }
        if (backend->Egl.eglGetPlatformDisplay != nullptr) {
            backend->Display =
                backend->Egl.eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
        }
        if (backend->Display == EGL_NO_DISPLAY && backend->Egl.eglGetDisplay != nullptr) {
            backend->Display = backend->Egl.eglGetDisplay(EGL_DEFAULT_DISPLAY);
        }
        if (backend->Display == EGL_NO_DISPLAY || backend->Egl.eglInitialize == nullptr) {
            BackendLog(backend, 3, "DirectGLES BFA: failed to obtain an EGLDisplay");
            return false;
        }

        EGLint major = 0;
        EGLint minor = 0;
        if (!backend->Egl.eglInitialize(backend->Display, &major, &minor)) {
            BackendLog(backend, 3, "DirectGLES BFA: eglInitialize failed (error 0x%x)",
                       backend->Egl.eglGetError == nullptr ? 0u : backend->Egl.eglGetError());
            return false;
        }

        if (backend->Egl.eglChooseConfig == nullptr) {
            return false;
        }
        const EGLint attribs[] = {EGL_SURFACE_TYPE,
                                  EGL_PBUFFER_BIT,
                                  EGL_RENDERABLE_TYPE,
                                  EGL_OPENGL_ES3_BIT | EGL_OPENGL_ES2_BIT,
                                  EGL_RED_SIZE,
                                  8,
                                  EGL_GREEN_SIZE,
                                  8,
                                  EGL_BLUE_SIZE,
                                  8,
                                  EGL_ALPHA_SIZE,
                                  8,
                                  EGL_DEPTH_SIZE,
                                  24,
                                  EGL_STENCIL_SIZE,
                                  8,
                                  EGL_NONE};
        const EGLint es2Attribs[] = {EGL_SURFACE_TYPE,
                                     EGL_PBUFFER_BIT,
                                     EGL_RENDERABLE_TYPE,
                                     EGL_OPENGL_ES2_BIT,
                                     EGL_RED_SIZE,
                                     8,
                                     EGL_GREEN_SIZE,
                                     8,
                                     EGL_BLUE_SIZE,
                                     8,
                                     EGL_ALPHA_SIZE,
                                     8,
                                     EGL_NONE};
        EGLConfig configs[1] = {nullptr};
        EGLint count = 0;
        if (!backend->Egl.eglChooseConfig(backend->Display, attribs, configs, 1, &count) || count < 1) {
            if (!backend->Egl.eglChooseConfig(backend->Display, es2Attribs, configs, 1, &count) || count < 1) {
                BackendLog(backend, 3, "DirectGLES BFA: no matching EGL config");
                return false;
            }
        }
        backend->Config = configs[0];
        backend->DisplayInitialized = true;
        return true;
    }

    Bool MakeCurrentForSession(MobileGLBackend* backend, const MobileGLBackend::NativeSession& session) {
        if (backend->Egl.eglMakeCurrent == nullptr || session.Context == EGL_NO_CONTEXT) {
            return false;
        }
        const EGLSurface draw = session.Surface == EGL_NO_SURFACE ? EGL_NO_SURFACE : session.Surface;
        return backend->Egl.eglMakeCurrent(backend->Display, draw, draw, session.Context) != EGL_FALSE;
    }

    Bool EnsureCurrent(MobileGLBackend* backend, MobileGLSessionId session) {
        const auto it = backend->Sessions.find(session);
        if (it == backend->Sessions.end() || it->second.Context == EGL_NO_CONTEXT) {
            return false;
        }
        if (backend->CurrentSessionId == session && backend->Egl.eglGetCurrentContext != nullptr &&
            backend->Egl.eglGetCurrentContext() == it->second.Context) {
            return true;
        }
        if (!MakeCurrentForSession(backend, it->second)) {
            return false;
        }
        backend->CurrentSessionId = session;
        return true;
    }

    void CollectExtensions(MobileGLBackend* backend) {
        backend->ExtensionStrings.clear();
        backend->ExtensionPointers.clear();
        if (backend->Gl.glGetIntegerv != nullptr && backend->Gl.glGetStringi != nullptr) {
            GLint count = 0;
            backend->Gl.glGetIntegerv(GL_NUM_EXTENSIONS, &count);
            for (GLint i = 0; i < count; ++i) {
                const GLubyte* extension = backend->Gl.glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i));
                if (extension != nullptr) {
                    backend->ExtensionStrings.emplace_back(reinterpret_cast<const char*>(extension));
                }
            }
        } else if (backend->Gl.glGetString != nullptr) {
            const GLubyte* text = backend->Gl.glGetString(GL_EXTENSIONS);
            if (text != nullptr) {
                String extensions(reinterpret_cast<const char*>(text));
                MobileGL::SizeT start = 0;
                while (start < extensions.size()) {
                    const MobileGL::SizeT end = extensions.find(' ', start);
                    if (end == String::npos) {
                        backend->ExtensionStrings.push_back(extensions.substr(start));
                        break;
                    }
                    if (end > start) {
                        backend->ExtensionStrings.push_back(extensions.substr(start, end - start));
                    }
                    start = end + 1;
                }
            }
        }
        for (const String& extension : backend->ExtensionStrings) {
            backend->ExtensionPointers.push_back(extension.c_str());
        }
    }

    void FillCapabilities(MobileGLBackend* backend) {
        backend->RendererName = QueryGlString(backend->Gl, GL_RENDERER);
        backend->VendorName = QueryGlString(backend->Gl, GL_VENDOR);
        backend->VersionString = QueryGlString(backend->Gl, GL_VERSION);
        backend->ShaderLanguageString = QueryGlString(backend->Gl, GL_SHADING_LANGUAGE_VERSION);
        CollectExtensions(backend);

        backend->DynamicParameters = {};
        backend->DynamicParameters.structSize = sizeof(MobileGLDynamicParameters);
        backend->DynamicParameters.maxTextureSize =
            static_cast<uint32_t>(QueryGlInt(backend->Gl, GL_MAX_TEXTURE_SIZE, 16384));
        backend->DynamicParameters.max3DTextureSize =
            static_cast<uint32_t>(QueryGlInt(backend->Gl, GL_MAX_3D_TEXTURE_SIZE, 2048));
        backend->DynamicParameters.maxArrayTextureLayers =
            static_cast<uint32_t>(QueryGlInt(backend->Gl, GL_MAX_ARRAY_TEXTURE_LAYERS, 256));
        backend->DynamicParameters.maxCubeMapTextureSize =
            static_cast<uint32_t>(QueryGlInt(backend->Gl, GL_MAX_CUBE_MAP_TEXTURE_SIZE, 16384));
        backend->DynamicParameters.maxRenderbufferSize =
            static_cast<uint32_t>(QueryGlInt(backend->Gl, GL_MAX_RENDERBUFFER_SIZE, 16384));
        backend->DynamicParameters.maxFramebufferWidth =
            static_cast<uint32_t>(QueryGlInt(backend->Gl, GL_MAX_FRAMEBUFFER_WIDTH, 16384));
        backend->DynamicParameters.maxFramebufferHeight =
            static_cast<uint32_t>(QueryGlInt(backend->Gl, GL_MAX_FRAMEBUFFER_HEIGHT, 16384));
        backend->DynamicParameters.maxFramebufferLayers =
            static_cast<uint32_t>(QueryGlInt(backend->Gl, GL_MAX_FRAMEBUFFER_LAYERS, 2048));
        backend->DynamicParameters.maxColorTextureSamples =
            static_cast<uint32_t>(QueryGlInt(backend->Gl, GL_MAX_COLOR_TEXTURE_SAMPLES, 1));
        backend->DynamicParameters.maxDepthTextureSamples =
            static_cast<uint32_t>(QueryGlInt(backend->Gl, GL_MAX_DEPTH_TEXTURE_SAMPLES, 1));
        backend->DynamicParameters.maxFramebufferSamples =
            static_cast<uint32_t>(QueryGlInt(backend->Gl, GL_MAX_FRAMEBUFFER_SAMPLES, 1));
        backend->DynamicParameters.maxIntegerSamples =
            static_cast<uint32_t>(QueryGlInt(backend->Gl, GL_MAX_INTEGER_SAMPLES, 1));
        backend->DynamicParameters.maxSamples = static_cast<uint32_t>(QueryGlInt(backend->Gl, GL_MAX_SAMPLES, 1));
        backend->DynamicParameters.maxTextureImageUnits =
            static_cast<uint32_t>(QueryGlInt(backend->Gl, GL_MAX_TEXTURE_IMAGE_UNITS, 16));
        backend->DynamicParameters.maxVertexTextureImageUnits =
            static_cast<uint32_t>(QueryGlInt(backend->Gl, GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, 16));
        backend->DynamicParameters.maxComputeTextureImageUnits =
            static_cast<uint32_t>(QueryGlInt(backend->Gl, GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS, 16));
        backend->DynamicParameters.maxCombinedTextureImageUnits =
            static_cast<uint32_t>(QueryGlInt(backend->Gl, GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, 32));
        backend->DynamicParameters.maxVertexAttribs =
            static_cast<uint32_t>(QueryGlInt(backend->Gl, GL_MAX_VERTEX_ATTRIBS, 16));
        backend->DynamicParameters.maxViewports = static_cast<uint32_t>(QueryGlInt(backend->Gl, GL_MAX_VIEWPORTS, 1));
        backend->DynamicParameters.maxDrawBuffers =
            static_cast<uint32_t>(QueryGlInt(backend->Gl, GL_MAX_DRAW_BUFFERS, 1));
        backend->DynamicParameters.maxColorAttachments =
            static_cast<uint32_t>(QueryGlInt(backend->Gl, GL_MAX_COLOR_ATTACHMENTS, 1));
        backend->DynamicParameters.maxUniformBufferBindings =
            static_cast<uint32_t>(QueryGlInt(backend->Gl, GL_MAX_UNIFORM_BUFFER_BINDINGS, 24));
        backend->DynamicParameters.maxShaderStorageBufferBindings =
            static_cast<uint32_t>(QueryGlInt(backend->Gl, GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, 8));
        backend->DynamicParameters.maxImageUnits =
            static_cast<uint32_t>(QueryGlInt(backend->Gl, GL_MAX_IMAGE_UNITS, 8));
        backend->DynamicParameters.maxPatchVertices =
            static_cast<uint32_t>(QueryGlInt(backend->Gl, GL_MAX_PATCH_VERTICES, 32));
        backend->DynamicParameters.maxTessGenLevel =
            static_cast<uint32_t>(QueryGlInt(backend->Gl, GL_MAX_TESS_GEN_LEVEL, 64));
        backend->DynamicParameters.supportsTessellationPointSize = 0;
        backend->DynamicParameters.supportsGeometryPointSize = 0;
        backend->DynamicParameters.supportsShaderFloat64 = 0;
        backend->DynamicParameters.supportsFloat64VertexAttributes = 0;

        backend->RendererInfo = {};
        backend->RendererInfo.structSize = sizeof(MobileGLRendererInfo);
        backend->RendererInfo.name = backend->RendererName.c_str();
        backend->RendererInfo.vendor = backend->VendorName.c_str();
        backend->RendererInfo.version = backend->VersionString.c_str();
        backend->RendererInfo.shaderLanguageVersion = backend->ShaderLanguageString.c_str();
        backend->RendererInfo.extensions =
            backend->ExtensionPointers.empty() ? nullptr : backend->ExtensionPointers.data();
        backend->RendererInfo.extensionCount = static_cast<uint32_t>(backend->ExtensionPointers.size());

        backend->CapabilitiesInitialized = true;
    }

    GLuint GetOrCreateBuffer(MobileGLBackend* backend, MobileGLBackendHandle handle) {
        const auto it = backend->BufferNames.find(handle);
        if (it != backend->BufferNames.end()) {
            return it->second;
        }
        GLuint name = 0;
        if (backend->Gl.glGenBuffers != nullptr && backend->Gl.glBindBuffer != nullptr &&
            backend->Gl.glBufferData != nullptr) {
            backend->Gl.glGenBuffers(1, &name);
            if (name != 0) {
                backend->Gl.glBindBuffer(GL_ARRAY_BUFFER, name);
                backend->Gl.glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
                backend->Gl.glBindBuffer(GL_ARRAY_BUFFER, 0);
            }
        }
        backend->BufferNames[handle] = name;
        return name;
    }

    GLuint GetOrCreateTexture(MobileGLBackend* backend, MobileGLBackendHandle handle) {
        const auto it = backend->TextureNames.find(handle);
        if (it != backend->TextureNames.end()) {
            return it->second;
        }
        GLuint name = 0;
        if (backend->Gl.glGenTextures != nullptr && backend->Gl.glBindTexture != nullptr) {
            backend->Gl.glGenTextures(1, &name);
            if (name != 0) {
                backend->Gl.glBindTexture(GL_TEXTURE_2D, name);
                backend->Gl.glBindTexture(GL_TEXTURE_2D, 0);
            }
        }
        backend->TextureNames[handle] = name;
        return name;
    }

    bool InitializeBackend(MobileGLBackend* backend, const MobileGLBackendInitInfo* info) {
        (void)info;
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (backend->Initialized) {
            return true;
        }
        if (!LoadEglFunctions(backend)) {
            return false;
        }
        (void)LoadGlesFunctions(backend);
        backend->Initialized = true;
        return true;
    }

    void ShutdownBackend(MobileGLBackend* backend) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        for (auto& entry : backend->Sessions) {
            MobileGLBackend::NativeSession& session = entry.second;
            if (session.Context != EGL_NO_CONTEXT && backend->Egl.eglDestroyContext != nullptr) {
                backend->Egl.eglDestroyContext(backend->Display, session.Context);
            }
            if (session.Surface != EGL_NO_SURFACE && backend->Egl.eglDestroySurface != nullptr) {
                backend->Egl.eglDestroySurface(backend->Display, session.Surface);
            }
        }
        backend->Sessions.clear();
        backend->BufferNames.clear();
        backend->TextureNames.clear();
        backend->SyncNames.clear();
        if (backend->Display != EGL_NO_DISPLAY && backend->Egl.eglTerminate != nullptr) {
            backend->Egl.eglTerminate(backend->Display);
        }
        backend->Display = EGL_NO_DISPLAY;
        backend->DisplayInitialized = false;
        CloseLibrary(backend->GlesLibrary);
        CloseLibrary(backend->EglLibrary);
        backend->GlesLibrary = nullptr;
        backend->EglLibrary = nullptr;
        backend->Initialized = false;
    }

    bool OnDisplayCreatedBackend(MobileGLBackend* backend, MobileGLDisplayId display, const void* nativeDisplay) {
        (void)nativeDisplay;
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        backend->CurrentDisplayId = display;
        return EnsureDisplay(backend);
    }

    void OnDisplayDestroyedBackend(MobileGLBackend* backend, MobileGLDisplayId display) {
        (void)display;
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        for (auto& entry : backend->Sessions) {
            MobileGLBackend::NativeSession& session = entry.second;
            if (session.Context != EGL_NO_CONTEXT && backend->Egl.eglDestroyContext != nullptr) {
                backend->Egl.eglDestroyContext(backend->Display, session.Context);
            }
            if (session.Surface != EGL_NO_SURFACE && backend->Egl.eglDestroySurface != nullptr) {
                backend->Egl.eglDestroySurface(backend->Display, session.Surface);
            }
        }
        backend->Sessions.clear();
    }

    bool OnSharedGroupCreatedBackend(MobileGLBackend* backend, MobileGLSharedGroupId group, const void* shareInfo) {
        (void)shareInfo;
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        backend->CurrentSharedGroupId = group;
        return true;
    }

    void OnSharedGroupDestroyedBackend(MobileGLBackend* backend, MobileGLSharedGroupId group) {
        (void)group;
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (backend->CurrentSharedGroupId == group) {
            backend->CurrentSharedGroupId = 0;
        }
    }

    bool OnSessionCreatedBackend(MobileGLBackend* backend, MobileGLSessionId session,
                                 const MobileGLBackendInitInfo* info) {
        (void)info;
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        backend->CurrentSessionId = session;
        if (!EnsureDisplay(backend)) {
            return false;
        }
        if (backend->Sessions.find(session) != backend->Sessions.end()) {
            return true;
        }

        if (backend->Egl.eglBindAPI != nullptr) {
            backend->Egl.eglBindAPI(EGL_OPENGL_ES_API);
        }
        const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
        const EGLint es2ContextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
        EGLContext nativeContext =
            backend->Egl.eglCreateContext(backend->Display, backend->Config, EGL_NO_CONTEXT, contextAttribs);
        if (nativeContext == EGL_NO_CONTEXT && backend->Egl.eglCreateContext != nullptr) {
            nativeContext =
                backend->Egl.eglCreateContext(backend->Display, backend->Config, EGL_NO_CONTEXT, es2ContextAttribs);
        }
        if (nativeContext == EGL_NO_CONTEXT) {
            BackendLog(backend, 3, "DirectGLES BFA: eglCreateContext failed (error 0x%x)",
                       backend->Egl.eglGetError == nullptr ? 0u : backend->Egl.eglGetError());
            return false;
        }

        const EGLint surfaceAttribs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
        EGLSurface nativeSurface = EGL_NO_SURFACE;
        if (backend->Egl.eglCreatePbufferSurface != nullptr) {
            nativeSurface = backend->Egl.eglCreatePbufferSurface(backend->Display, backend->Config, surfaceAttribs);
        }

        MobileGLBackend::NativeSession nativeSession;
        nativeSession.Context = nativeContext;
        nativeSession.Surface = nativeSurface;
        const EGLSurface draw = nativeSurface == EGL_NO_SURFACE ? EGL_NO_SURFACE : nativeSurface;
        if (!backend->Egl.eglMakeCurrent(backend->Display, draw, draw, nativeContext)) {
            BackendLog(backend, 3, "DirectGLES BFA: eglMakeCurrent failed (error 0x%x)",
                       backend->Egl.eglGetError == nullptr ? 0u : backend->Egl.eglGetError());
            if (nativeSurface != EGL_NO_SURFACE && backend->Egl.eglDestroySurface != nullptr) {
                backend->Egl.eglDestroySurface(backend->Display, nativeSurface);
            }
            if (backend->Egl.eglDestroyContext != nullptr) {
                backend->Egl.eglDestroyContext(backend->Display, nativeContext);
            }
            return false;
        }

        backend->Sessions[session] = MobileGL::Move(nativeSession);
        backend->CurrentSessionId = session;
        if (!backend->CapabilitiesInitialized) {
            FillCapabilities(backend);
        }
        BackendLog(backend, 1, "DirectGLES BFA: created session %llu (EGL %p, renderer %s)",
                   static_cast<unsigned long long>(session), static_cast<void*>(nativeContext),
                   backend->RendererName.c_str());
        return true;
    }

    void OnSessionDestroyedBackend(MobileGLBackend* backend, MobileGLSessionId session) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        const auto it = backend->Sessions.find(session);
        if (it == backend->Sessions.end()) {
            return;
        }
        MobileGLBackend::NativeSession& nativeSession = it->second;
        if (backend->CurrentSessionId == session && backend->Egl.eglMakeCurrent != nullptr) {
            backend->Egl.eglMakeCurrent(backend->Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            backend->CurrentSessionId = 0;
        }
        if (nativeSession.Surface != EGL_NO_SURFACE && backend->Egl.eglDestroySurface != nullptr) {
            backend->Egl.eglDestroySurface(backend->Display, nativeSession.Surface);
        }
        if (nativeSession.Context != EGL_NO_CONTEXT && backend->Egl.eglDestroyContext != nullptr) {
            backend->Egl.eglDestroyContext(backend->Display, nativeSession.Context);
        }
        backend->Sessions.erase(it);
    }

    void OnObjectCreatedBackend(MobileGLBackend* backend, MobileGLObjectKind kind, MobileGLBackendHandle handle) {
        (void)kind;
        (void)handle;
        (void)backend;
    }

    void OnObjectDestroyedBackend(MobileGLBackend* backend, MobileGLObjectKind kind, MobileGLBackendHandle handle) {
        (void)kind;
        (void)handle;
        (void)backend;
    }

    bool InitializeEGLDisplayBackend(MobileGLBackend* backend, MobileGLDisplayId display, int32_t* major,
                                     int32_t* minor) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        backend->CurrentDisplayId = display;
        if (!EnsureDisplay(backend)) {
            return false;
        }
        if (major != nullptr) {
            *major = 1;
        }
        if (minor != nullptr) {
            *minor = 5;
        }
        return true;
    }

    bool CreatePbufferSurfaceBackend(MobileGLBackend* backend, MobileGLDisplayId display, MobileGLBackendHandle surface,
                                     int32_t width, int32_t height) {
        (void)surface;
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        backend->CurrentDisplayId = display;
        if (!EnsureDisplay(backend) || backend->Egl.eglCreatePbufferSurface == nullptr) {
            return false;
        }
        const EGLint attribs[] = {EGL_WIDTH, width, EGL_HEIGHT, height, EGL_NONE};
        backend->Surface = backend->Egl.eglCreatePbufferSurface(backend->Display, backend->Config, attribs);
        return backend->Surface != EGL_NO_SURFACE;
    }

    bool CreateWindowSurfaceBackend(MobileGLBackend* backend, MobileGLDisplayId display, MobileGLBackendHandle surface,
                                    const MobileGLSurfaceCreateInfo* info) {
        (void)surface;
        (void)info;
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        backend->CurrentDisplayId = display;
        BackendLog(backend, 2, "DirectGLES BFA: window surfaces are not supported in the headless path");
        return false;
    }

    bool ResizeSurfaceBackend(MobileGLBackend* backend, MobileGLDisplayId display, MobileGLBackendHandle surface,
                              uint32_t width, uint32_t height) {
        (void)surface;
        (void)width;
        (void)height;
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        (void)display;
        return false;
    }

    bool MakeEGLCurrentBackend(MobileGLBackend* backend, MobileGLSessionId session, MobileGLBackendHandle draw,
                               MobileGLBackendHandle read) {
        (void)draw;
        (void)read;
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        const auto it = backend->Sessions.find(session);
        return it != backend->Sessions.end() && MakeCurrentForSession(backend, it->second);
    }

    bool SwapBuffersBackend(MobileGLBackend* backend, MobileGLSessionId session, MobileGLBackendHandle draw) {
        (void)draw;
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (!EnsureCurrent(backend, session) || backend->Egl.eglSwapBuffers == nullptr) {
            return false;
        }
        const auto it = backend->Sessions.find(session);
        if (it == backend->Sessions.end() || it->second.Surface == EGL_NO_SURFACE) {
            return true;
        }
        return backend->Egl.eglSwapBuffers(backend->Display, it->second.Surface) != EGL_FALSE;
    }

    void SetSwapIntervalBackend(MobileGLBackend* backend, MobileGLSessionId session, int32_t interval) {
        (void)session;
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (backend->Egl.eglSwapInterval != nullptr) {
            backend->Egl.eglSwapInterval(backend->Display, interval);
        }
    }

    void ReleaseEGLSurfaceBackend(MobileGLBackend* backend, MobileGLDisplayId display, MobileGLBackendHandle surface) {
        (void)display;
        (void)surface;
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (backend->Surface != EGL_NO_SURFACE && backend->Egl.eglDestroySurface != nullptr) {
            backend->Egl.eglDestroySurface(backend->Display, backend->Surface);
        }
        backend->Surface = EGL_NO_SURFACE;
    }

    void ReleaseEGLResourcesBackend(MobileGLBackend* backend, MobileGLDisplayId display, MobileGLSessionId session) {
        (void)display;
        (void)session;
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (backend->Surface != EGL_NO_SURFACE && backend->Egl.eglDestroySurface != nullptr) {
            backend->Egl.eglDestroySurface(backend->Display, backend->Surface);
        }
        backend->Surface = EGL_NO_SURFACE;
    }

    void ClearBackend(MobileGLBackend* backend, MobileGLSessionId session, uint32_t mask) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (!EnsureCurrent(backend, session) || backend->Gl.glClear == nullptr) {
            return;
        }
        backend->Gl.glClear(static_cast<GLbitfield>(mask));
    }

    void ClearColorBackend(MobileGLBackend* backend, MobileGLSessionId session, float red, float green, float blue,
                           float alpha) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (!EnsureCurrent(backend, session) || backend->Gl.glClearColor == nullptr) {
            return;
        }
        backend->Gl.glClearColor(red, green, blue, alpha);
    }

    void DrawArraysBackend(MobileGLBackend* backend, MobileGLSessionId session, uint32_t mode, int32_t first,
                           int32_t count) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (!EnsureCurrent(backend, session) || backend->Gl.glDrawArrays == nullptr) {
            return;
        }
        backend->Gl.glDrawArrays(static_cast<GLenum>(mode), first, count);
    }

    void DrawElementsBackend(MobileGLBackend* backend, MobileGLSessionId session, uint32_t mode, int32_t count,
                             uint32_t type, const void* indices) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (!EnsureCurrent(backend, session) || backend->Gl.glDrawElements == nullptr) {
            return;
        }
        backend->Gl.glDrawElements(static_cast<GLenum>(mode), count, static_cast<GLenum>(type), indices);
    }

    void DrawArraysInstancedBackend(MobileGLBackend* backend, MobileGLSessionId session, uint32_t mode, int32_t first,
                                    int32_t count, int32_t instanceCount) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (!EnsureCurrent(backend, session) || backend->Gl.glDrawArraysInstanced == nullptr) {
            return;
        }
        backend->Gl.glDrawArraysInstanced(static_cast<GLenum>(mode), first, count, instanceCount);
    }

    void DrawElementsInstancedBackend(MobileGLBackend* backend, MobileGLSessionId session, uint32_t mode, int32_t count,
                                      uint32_t type, const void* indices, int32_t instanceCount) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (!EnsureCurrent(backend, session) || backend->Gl.glDrawElementsInstanced == nullptr) {
            return;
        }
        backend->Gl.glDrawElementsInstanced(static_cast<GLenum>(mode), count, static_cast<GLenum>(type), indices,
                                            instanceCount);
    }

    void DrawRangeElementsBackend(MobileGLBackend* backend, MobileGLSessionId session, uint32_t mode, uint32_t start,
                                  uint32_t end, int32_t count, uint32_t type, const void* indices) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (!EnsureCurrent(backend, session) || backend->Gl.glDrawRangeElements == nullptr) {
            return;
        }
        backend->Gl.glDrawRangeElements(static_cast<GLenum>(mode), start, end, count, static_cast<GLenum>(type),
                                        indices);
    }

    void DrawArraysIndirectBackend(MobileGLBackend* backend, MobileGLSessionId session, uint32_t mode,
                                   const void* indirect) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (!EnsureCurrent(backend, session) || backend->Gl.glDrawArraysIndirect == nullptr || indirect == nullptr) {
            return;
        }
        backend->Gl.glDrawArraysIndirect(static_cast<GLenum>(mode), indirect);
    }

    void DrawElementsIndirectBackend(MobileGLBackend* backend, MobileGLSessionId session, uint32_t mode, uint32_t type,
                                     const void* indirect) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (!EnsureCurrent(backend, session) || backend->Gl.glDrawElementsIndirect == nullptr || indirect == nullptr) {
            return;
        }
        backend->Gl.glDrawElementsIndirect(static_cast<GLenum>(mode), static_cast<GLenum>(type), indirect);
    }

    void TextureRespecifyBackend(MobileGLBackend* backend, MobileGLSessionId session, MobileGLBackendHandle texture,
                                 const MobileGLTextureUpload* levels, uint32_t levelCount) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (!EnsureCurrent(backend, session) || levels == nullptr || levelCount == 0) {
            return;
        }
        const MobileGLTextureUpload& upload = levels[0];
        const GLuint name = GetOrCreateTexture(backend, texture);
        if (name == 0) {
            return;
        }
        if (upload.depth > 1) {
            if (backend->Gl.glTexImage3D == nullptr) {
                return;
            }
            backend->Gl.glBindTexture(GL_TEXTURE_3D, name);
            backend->Gl.glTexImage3D(GL_TEXTURE_3D, static_cast<GLint>(upload.level), static_cast<GLint>(upload.format),
                                     static_cast<GLsizei>(upload.width), static_cast<GLsizei>(upload.height),
                                     static_cast<GLsizei>(upload.depth), 0, static_cast<GLenum>(upload.format),
                                     static_cast<GLenum>(upload.type), upload.data);
            backend->Gl.glBindTexture(GL_TEXTURE_3D, 0);
        } else {
            if (backend->Gl.glTexImage2D == nullptr) {
                return;
            }
            backend->Gl.glBindTexture(GL_TEXTURE_2D, name);
            backend->Gl.glTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(upload.level), static_cast<GLint>(upload.format),
                                     static_cast<GLsizei>(upload.width), static_cast<GLsizei>(upload.height), 0,
                                     static_cast<GLenum>(upload.format), static_cast<GLenum>(upload.type), upload.data);
            backend->Gl.glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    void BufferRespecifyBackend(MobileGLBackend* backend, MobileGLSessionId session, MobileGLBackendHandle buffer,
                                uint64_t size, uint32_t usage, const MobileGLBufferOps* ops) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (!EnsureCurrent(backend, session) || backend->Gl.glGenBuffers == nullptr ||
            backend->Gl.glBindBuffer == nullptr || backend->Gl.glBufferData == nullptr) {
            return;
        }
        GLuint name = 0;
        const auto it = backend->BufferNames.find(buffer);
        if (it != backend->BufferNames.end() && it->second != 0 && backend->Gl.glDeleteBuffers != nullptr) {
            backend->Gl.glDeleteBuffers(1, &it->second);
            it->second = 0;
        }
        if (name == 0) {
            backend->Gl.glGenBuffers(1, &name);
        }
        if (name == 0) {
            return;
        }
        backend->Gl.glBindBuffer(GL_ARRAY_BUFFER, name);
        backend->Gl.glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(size),
                                 ops == nullptr ? nullptr : ops->initialData, static_cast<GLenum>(usage));
        backend->Gl.glBindBuffer(GL_ARRAY_BUFFER, 0);
        backend->BufferNames[buffer] = name;
    }

    void BufferSubDataBackend(MobileGLBackend* backend, MobileGLSessionId session, MobileGLBackendHandle buffer,
                              uint64_t offset, uint64_t size, const void* data) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (!EnsureCurrent(backend, session) || data == nullptr || backend->Gl.glBindBuffer == nullptr ||
            backend->Gl.glBufferSubData == nullptr) {
            return;
        }
        const GLuint name = GetOrCreateBuffer(backend, buffer);
        if (name == 0) {
            return;
        }
        backend->Gl.glBindBuffer(GL_ARRAY_BUFFER, name);
        backend->Gl.glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size),
                                    data);
        backend->Gl.glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void MemoryBarrierBackend(MobileGLBackend* backend, MobileGLSessionId session, uint32_t barriers) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (!EnsureCurrent(backend, session) || backend->Gl.glMemoryBarrier == nullptr) {
            return;
        }
        backend->Gl.glMemoryBarrier(static_cast<GLbitfield>(barriers));
    }

    void MemoryBarrierByRegionBackend(MobileGLBackend* backend, MobileGLSessionId session, uint32_t barriers) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (!EnsureCurrent(backend, session) || backend->Gl.glMemoryBarrierByRegion == nullptr) {
            return;
        }
        backend->Gl.glMemoryBarrierByRegion(static_cast<GLbitfield>(barriers));
    }

    void PatchParameteriBackend(MobileGLBackend* backend, MobileGLSessionId session, uint32_t pname, int32_t value) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (!EnsureCurrent(backend, session) || backend->Gl.glPatchParameteri == nullptr) {
            return;
        }
        backend->Gl.glPatchParameteri(static_cast<GLenum>(pname), value);
    }

    void GenerateMipmapBackend(MobileGLBackend* backend, MobileGLSessionId session, uint32_t target) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (!EnsureCurrent(backend, session) || backend->Gl.glGenerateMipmap == nullptr) {
            return;
        }
        backend->Gl.glGenerateMipmap(static_cast<GLenum>(target));
    }

    void DispatchComputeBackend(MobileGLBackend* backend, MobileGLSessionId session, uint32_t x, uint32_t y,
                                uint32_t z) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (!EnsureCurrent(backend, session) || backend->Gl.glDispatchCompute == nullptr) {
            return;
        }
        backend->Gl.glDispatchCompute(x, y, z);
    }

    void DispatchComputeIndirectBackend(MobileGLBackend* backend, MobileGLSessionId session, int64_t indirectOffset) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (!EnsureCurrent(backend, session) || backend->Gl.glDispatchComputeIndirect == nullptr) {
            return;
        }
        backend->Gl.glDispatchComputeIndirect(static_cast<GLintptr>(indirectOffset));
    }

    void BlitFramebufferBackend(MobileGLBackend* backend, MobileGLSessionId session,
                                MobileGLBackendHandle readFramebuffer, MobileGLBackendHandle drawFramebuffer,
                                int32_t srcX0, int32_t srcY0, int32_t srcX1, int32_t srcY1, int32_t dstX0,
                                int32_t dstY0, int32_t dstX1, int32_t dstY1, uint32_t mask, uint32_t filter) {
        (void)readFramebuffer;
        (void)drawFramebuffer;
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (!EnsureCurrent(backend, session) || backend->Gl.glBlitFramebuffer == nullptr) {
            return;
        }
        backend->Gl.glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1,
                                      static_cast<GLbitfield>(mask), static_cast<GLenum>(filter));
    }

    void ReadPixelsBackend(MobileGLBackend* backend, MobileGLSessionId session, int32_t x, int32_t y,
                           int32_t width, int32_t height, uint32_t format, uint32_t type, void* pixels) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (!EnsureCurrent(backend, session) || backend->Gl.glReadPixels == nullptr || pixels == nullptr) {
            return;
        }
        backend->Gl.glReadPixels(x, y, width, height, static_cast<GLenum>(format),
                                 static_cast<GLenum>(type), pixels);
    }

    void BeginTransformFeedbackBackend(MobileGLBackend* backend, MobileGLSessionId session, uint32_t primitiveMode) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (!EnsureCurrent(backend, session) || backend->Gl.glBeginTransformFeedback == nullptr) {
            return;
        }
        backend->Gl.glBeginTransformFeedback(static_cast<GLenum>(primitiveMode));
    }

    void EndTransformFeedbackBackend(MobileGLBackend* backend, MobileGLSessionId session) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (!EnsureCurrent(backend, session) || backend->Gl.glEndTransformFeedback == nullptr) {
            return;
        }
        backend->Gl.glEndTransformFeedback();
    }

    void PauseTransformFeedbackBackend(MobileGLBackend* backend, MobileGLSessionId session) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (!EnsureCurrent(backend, session) || backend->Gl.glPauseTransformFeedback == nullptr) {
            return;
        }
        backend->Gl.glPauseTransformFeedback();
    }

    void ResumeTransformFeedbackBackend(MobileGLBackend* backend, MobileGLSessionId session) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (!EnsureCurrent(backend, session) || backend->Gl.glResumeTransformFeedback == nullptr) {
            return;
        }
        backend->Gl.glResumeTransformFeedback();
    }

    void BindTransformFeedbackBackend(MobileGLBackend* backend, MobileGLSessionId session, uint32_t name) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (!EnsureCurrent(backend, session) || backend->Gl.glBindTransformFeedback == nullptr) {
            return;
        }
        backend->Gl.glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, static_cast<GLuint>(name));
    }

    void* FenceSyncBackend(MobileGLBackend* backend, MobileGLSessionId session, uint32_t condition, uint32_t flags) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (!EnsureCurrent(backend, session) || backend->Gl.glFenceSync == nullptr) {
            return nullptr;
        }
        GLsync sync = backend->Gl.glFenceSync(static_cast<GLenum>(condition), static_cast<GLbitfield>(flags));
        if (sync != nullptr) {
            backend->SyncNames[reinterpret_cast<MobileGLBackendHandle>(sync)] = sync;
        }
        return reinterpret_cast<void*>(sync);
    }

    uint32_t ClientWaitSyncBackend(MobileGLBackend* backend, MobileGLSessionId session, void* sync, uint32_t flags,
                                   uint64_t timeout) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (!EnsureCurrent(backend, session) || backend->Gl.glClientWaitSync == nullptr || sync == nullptr) {
            return static_cast<uint32_t>(GL_ALREADY_SIGNALED);
        }
        return static_cast<uint32_t>(
            backend->Gl.glClientWaitSync(static_cast<GLsync>(sync), static_cast<GLbitfield>(flags), timeout));
    }

    void WaitSyncBackend(MobileGLBackend* backend, MobileGLSessionId session, void* sync, uint32_t flags,
                         uint64_t timeout) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (!EnsureCurrent(backend, session) || backend->Gl.glWaitSync == nullptr || sync == nullptr) {
            return;
        }
        backend->Gl.glWaitSync(static_cast<GLsync>(sync), static_cast<GLbitfield>(flags), timeout);
    }

    void DeleteSyncBackend(MobileGLBackend* backend, MobileGLSessionId session, void* sync) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (sync == nullptr || backend->Gl.glDeleteSync == nullptr) {
            return;
        }
        backend->Gl.glDeleteSync(static_cast<GLsync>(sync));
        backend->SyncNames.erase(reinterpret_cast<MobileGLBackendHandle>(sync));
    }

    bool GetSyncStatusBackend(MobileGLBackend* backend, MobileGLSessionId session, void* sync) {
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        if (sync == nullptr || !EnsureCurrent(backend, session) || backend->Gl.glGetSynciv == nullptr) {
            return true;
        }
        GLint status = 0;
        backend->Gl.glGetSynciv(static_cast<GLsync>(sync), GL_SYNC_STATUS, 1, nullptr, &status);
        return status == GL_SIGNALED;
    }

    const MobileGLRendererInfo* GetRendererInfoBackend(MobileGLBackend* backend, MobileGLSessionId session) {
        (void)session;
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        return &backend->RendererInfo;
    }

    const MobileGLDynamicParameters* GetDynamicParametersBackend(MobileGLBackend* backend, MobileGLSessionId session) {
        (void)session;
        const std::lock_guard<std::recursive_mutex> lock(backend->Mutex);
        return &backend->DynamicParameters;
    }

    bool GetFormatCapabilitiesBackend(MobileGLBackend* backend, MobileGLSessionId session,
                                      MobileGLFormatCapabilityCache* out) {
        (void)backend;
        (void)session;
        (void)out;
        return false;
    }

    bool GetFormatSampleCountsBackend(MobileGLBackend* backend, MobileGLSessionId session, uint32_t target,
                                      uint32_t format, const uint32_t** counts, uint32_t* outCount) {
        (void)backend;
        (void)session;
        (void)target;
        (void)format;
        (void)counts;
        if (outCount != nullptr) {
            *outCount = 0;
        }
        return false;
    }

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
        .OnObjectCreated = &OnObjectCreatedBackend,
        .OnObjectDestroyed = &OnObjectDestroyedBackend,
        .InitializeEGLDisplay = &InitializeEGLDisplayBackend,
        .CreateWindowSurface = &CreateWindowSurfaceBackend,
        .CreatePbufferSurface = &CreatePbufferSurfaceBackend,
        .ResizeSurface = &ResizeSurfaceBackend,
        .MakeEGLCurrent = &MakeEGLCurrentBackend,
        .SwapBuffers = &SwapBuffersBackend,
        .SetSwapInterval = &SetSwapIntervalBackend,
        .ReleaseEGLSurface = &ReleaseEGLSurfaceBackend,
        .ReleaseEGLResources = &ReleaseEGLResourcesBackend,
        .DrawArrays = &DrawArraysBackend,
        .DrawElements = &DrawElementsBackend,
        .DrawArraysInstanced = &DrawArraysInstancedBackend,
        .DrawElementsInstanced = &DrawElementsInstancedBackend,
        .DrawRangeElements = &DrawRangeElementsBackend,
        .DrawArraysIndirect = &DrawArraysIndirectBackend,
        .DrawElementsIndirect = &DrawElementsIndirectBackend,
        .Clear = &ClearBackend,
        .ClearColor = &ClearColorBackend,
        .BlitFramebuffer = &BlitFramebufferBackend,
        .GenerateMipmap = &GenerateMipmapBackend,
        .ReadPixels = &ReadPixelsBackend,
        .DispatchCompute = &DispatchComputeBackend,
        .DispatchComputeIndirect = &DispatchComputeIndirectBackend,
        .MemoryBarrier = &MemoryBarrierBackend,
        .MemoryBarrierByRegion = &MemoryBarrierByRegionBackend,
        .PatchParameteri = &PatchParameteriBackend,
        .BeginTransformFeedback = &BeginTransformFeedbackBackend,
        .EndTransformFeedback = &EndTransformFeedbackBackend,
        .PauseTransformFeedback = &PauseTransformFeedbackBackend,
        .ResumeTransformFeedback = &ResumeTransformFeedbackBackend,
        .BindTransformFeedback = &BindTransformFeedbackBackend,
        .BufferRespecify = &BufferRespecifyBackend,
        .BufferSubData = &BufferSubDataBackend,
        .TextureRespecify = &TextureRespecifyBackend,
        .FenceSync = &FenceSyncBackend,
        .ClientWaitSync = &ClientWaitSyncBackend,
        .WaitSync = &WaitSyncBackend,
        .DeleteSync = &DeleteSyncBackend,
        .GetSyncStatus = &GetSyncStatusBackend,
        .GetRendererInfo = &GetRendererInfoBackend,
        .GetDynamicParameters = &GetDynamicParametersBackend,
        .GetFormatCapabilities = &GetFormatCapabilitiesBackend,
        .GetFormatSampleCounts = &GetFormatSampleCountsBackend,
    };
} // namespace

const MobileGLBackendVTable* RealBackendGetVTable() {
    return &s_backendVTable;
}

MobileGLBackend* RealBackendCreate(const MobileGLBackendHost* host, const MobileGLUtilApi* utilApi,
                                   const MobileGLBackendInitInfo* initInfo) {
    (void)initInfo;
    auto* backend = new MobileGLBackend();
    backend->VTable = RealBackendGetVTable();
    backend->Host = host;
    backend->UtilApi = utilApi;
    return backend;
}

// End of File
