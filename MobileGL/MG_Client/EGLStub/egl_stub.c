// MobileGL - MobileGL/MG_Client/EGLStub/egl_stub.c
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header
//
// libEGL.so forwarding stub for the C/S plugin APK.
//
// Why this file exists: LWJGL's OpenGL loader resolves its EGL function
// provider with System.loadLibrary("libEGL.so") (or dlopen), which on Android
// hits the *system* EGL - whose eglGetProcAddress returns the real driver's
// GL entry points. Minecraft then renders through the platform driver instead
// of libMobileGL_Client.so, silently bypassing the C/S pipeline ("fallback"
// renderer, our client logs never show glGetString traffic).
//
// Shipping this stub as lib/arm64-v8a/libEGL.so makes the loader land here;
// every entry point forwards to the already-loaded libMobileGL_Client.so
// (FCL dlopens it with RTLD_GLOBAL before the JVM starts). The stub itself is
// dependency-free (dlopen/dlsym only) so it can never lose to the system
// library at link time.

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <dlfcn.h>
#include <string.h>

static void* s_clientHandle = NULL;

static void* ResolveClient(const char* name) {
    if (s_clientHandle == NULL) {
        s_clientHandle = dlopen("libMobileGL_Client.so", RTLD_NOW | RTLD_GLOBAL);
    }
    if (s_clientHandle == NULL) {
        return NULL;
    }
    return dlsym(s_clientHandle, name);
}

#define EGL_FWD(rt, name, params, call)                                        \
    rt name params {                                                            \
        typedef rt (*Fn) params;                                                  \
        static Fn fn = NULL;                                                     \
        if (fn == NULL) {                                                        \
            fn = (Fn)ResolveClient(#name);                                        \
        }                                                                        \
        if (fn != NULL) {                                                        \
            return fn call;                                                       \
        }                                                                        \
        return (rt)0;                                                             \
    }

EGL_FWD(EGLBoolean, eglBindAPI, (EGLenum api), (api))
EGL_FWD(EGLBoolean, eglChooseConfig,
        (EGLDisplay dpy, const EGLint* attrib_list, EGLConfig* configs, EGLint config_size,
         EGLint* num_config),
        (dpy, attrib_list, configs, config_size, num_config))
EGL_FWD(EGLContext, eglCreateContext,
        (EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint* attrib_list),
        (dpy, config, share_context, attrib_list))
EGL_FWD(EGLSurface, eglCreatePbufferSurface,
        (EGLDisplay dpy, EGLConfig config, const EGLint* attrib_list),
        (dpy, config, attrib_list))
EGL_FWD(EGLSurface, eglCreatePlatformWindowSurface,
        (EGLDisplay dpy, EGLConfig config, void* native_window, const EGLAttrib* attrib_list),
        (dpy, config, native_window, attrib_list))
EGL_FWD(EGLSurface, eglCreateWindowSurface,
        (EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint* attrib_list),
        (dpy, config, win, attrib_list))
EGL_FWD(EGLBoolean, eglDestroyContext, (EGLDisplay dpy, EGLContext ctx), (dpy, ctx))
EGL_FWD(EGLBoolean, eglDestroySurface, (EGLDisplay dpy, EGLSurface surface), (dpy, surface))
EGL_FWD(EGLBoolean, eglGetConfigAttrib,
        (EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint* value),
        (dpy, config, attribute, value))
EGL_FWD(EGLBoolean, eglGetConfigs,
        (EGLDisplay dpy, EGLConfig* configs, EGLint config_size, EGLint* num_config),
        (dpy, configs, config_size, num_config))
EGL_FWD(EGLDisplay, eglGetCurrentDisplay, (void), ())
EGL_FWD(EGLSurface, eglGetCurrentSurface, (EGLint readdraw), (readdraw))
EGL_FWD(EGLContext, eglGetCurrentContext, (void), ())
EGL_FWD(EGLDisplay, eglGetDisplay, (EGLNativeDisplayType display_id), (display_id))
EGL_FWD(EGLint, eglGetError, (void), ())
EGL_FWD(__eglMustCastToProperFunctionPointerType, eglGetProcAddress, (const char* procname),
        (procname))
EGL_FWD(EGLBoolean, eglInitialize, (EGLDisplay dpy, EGLint* major, EGLint* minor),
        (dpy, major, minor))
EGL_FWD(EGLBoolean, eglMakeCurrent,
        (EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx),
        (dpy, draw, read, ctx))
EGL_FWD(EGLBoolean, eglReleaseThread, (void), ())
EGL_FWD(EGLBoolean, eglSwapBuffers, (EGLDisplay dpy, EGLSurface surface), (dpy, surface))
EGL_FWD(EGLBoolean, eglSwapInterval, (EGLDisplay dpy, EGLint interval), (dpy, interval))
EGL_FWD(EGLBoolean, eglTerminate, (EGLDisplay dpy), (dpy))
EGL_FWD(char const*, eglQueryString, (EGLDisplay dpy, EGLint name), (dpy, name))
EGL_FWD(EGLBoolean, eglWaitClient, (void), ())
EGL_FWD(EGLBoolean, eglWaitGL, (void), ())
EGL_FWD(EGLBoolean, eglWaitNative, (EGLint engine), (engine))
EGL_FWD(EGLSync, eglCreateSync,
        (EGLDisplay dpy, EGLenum type, const EGLAttrib* attrib_list),
        (dpy, type, attrib_list))
EGL_FWD(EGLBoolean, eglDestroySync, (EGLDisplay dpy, EGLSync sync), (dpy, sync))
EGL_FWD(EGLint, eglClientWaitSync,
        (EGLDisplay dpy, EGLSync sync, EGLint flags, EGLTime timeout),
        (dpy, sync, flags, timeout))
EGL_FWD(EGLBoolean, eglGetSyncAttrib,
        (EGLDisplay dpy, EGLSync sync, EGLint attribute, EGLAttrib* value),
        (dpy, sync, attribute, value))

#undef EGL_FWD