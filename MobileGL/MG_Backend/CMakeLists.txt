# MobileGL - MobileGL/MG_Backend/CMakeLists.txt
# Copyright (c) 2025-2026 MobileGL-Dev
# Licensed under the GNU Lesser General Public License v3.0:
#   https://www.gnu.org/licenses/gpl-3.0.txt
#   https://www.gnu.org/licenses/lgpl-3.0.txt
# SPDX-License-Identifier: LGPL-3.0-only
# End of Source File Header

# Server(per BackendObject) 单独一个 target：每个后端一个独立 MODULE .so。
# FullServer 在运行时 dlopen 这些插件，构建期不链接。

add_library(BackendObject_DirectGLES MODULE
    DirectGLES/PluginManifest.cpp
)

target_link_libraries(BackendObject_DirectGLES PRIVATE
    MobileGL_BFA
)

target_include_directories(BackendObject_DirectGLES PRIVATE
    ${CMAKE_SOURCE_DIR}/MobileGL
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(BackendObject_DirectGLES PROPERTIES
    PREFIX ""
    SUFFIX ".so"
)

add_library(BackendObject_DirectVulkan MODULE
    DirectVulkan/PluginManifest.cpp
)

target_link_libraries(BackendObject_DirectVulkan PRIVATE
    MobileGL_BFA
)

target_include_directories(BackendObject_DirectVulkan PRIVATE
    ${CMAKE_SOURCE_DIR}/MobileGL
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(BackendObject_DirectVulkan PROPERTIES
    PREFIX ""
    SUFFIX ".so"
)

# End of File
