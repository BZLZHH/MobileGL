# MobileGL - MobileGL/MG_Client/EGLStub/CMakeLists.txt
# Copyright (c) 2025-2026 MobileGL-Dev
# Licensed under the GNU Lesser General Public License v3.0:
#   https://www.gnu.org/licenses/gpl-3.0.txt
#   https://www.gnu.org/licenses/lgpl-3.0.txt
# SPDX-License-Identifier: LGPL-3.0-only
# End of Source File Header
#
# libEGL.so forwarding stub (Android only): LWJGL's OpenGL loader resolves its
# EGL function provider with System.loadLibrary("libEGL.so"); without this
# library in the plugin APK the loader falls through to the *system* EGL and
# Minecraft renders through the platform driver, bypassing the C/S client.
#
# The stub ships as lib/arm64-v8a/libEGL.so and forwards every entry point to
# libMobileGL_Client.so, which FCL already dlopens with RTLD_GLOBAL.

add_library(MobileGL_EGLStub SHARED
    egl_stub.c
)

set_target_properties(MobileGL_EGLStub PROPERTIES
    OUTPUT_NAME "EGL"
    C_VISIBILITY_PRESET default
    VISIBILITY_INLINES_HIDDEN OFF
)

target_include_directories(MobileGL_EGLStub PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

if (ANDROID)
    target_link_libraries(MobileGL_EGLStub PRIVATE dl)
endif()

# End of File