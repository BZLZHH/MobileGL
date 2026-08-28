# MobileGL - MobileGL/FullServer/CMakeLists.txt
# Copyright (c) 2025-2026 MobileGL-Dev
# Licensed under the GNU Lesser General Public License v3.0:
#   https://www.gnu.org/licenses/gpl-3.0.txt
#   https://www.gnu.org/licenses/lgpl-3.0.txt
# SPDX-License-Identifier: LGPL-3.0-only
# End of Source File Header

add_executable(FullServer
    Main.cpp
    BackendHost.cpp
    BackendPluginLoader.cpp
    UtilRuntimeLoader.cpp
    ServerCore.cpp
)

target_link_libraries(FullServer PRIVATE
    MobileGL_Protocol
    MobileGL_BFA
    MobileGL_RuntimeApi
    MobileGL_Transport
)

target_include_directories(FullServer PRIVATE
    ${CMAKE_SOURCE_DIR}/MobileGL
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/3rdparty/SPIRV-Reflect
)

if (UNIX AND NOT APPLE AND NOT ANDROID)
    target_link_libraries(FullServer PRIVATE ${CMAKE_DL_LIBS})
endif()

# End of File
