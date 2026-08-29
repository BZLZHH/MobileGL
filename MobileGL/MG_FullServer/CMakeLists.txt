# MobileGL - MobileGL/MG_FullServer/CMakeLists.txt
# Copyright (c) 2025-2026 MobileGL-Dev
# Licensed under the GNU Lesser General Public License v3.0:
#   https://www.gnu.org/licenses/gpl-3.0.txt
#   https://www.gnu.org/licenses/lgpl-3.0.txt
# SPDX-License-Identifier: LGPL-3.0-only
# End of Source File Header

# MobileGL_FullServer is delivered as a SHARED library (.so), following the
# MobileGL_* library naming (libMobileGL_FullServer.so). The BigServer host is
# a component that a host process (Android app / desktop launcher) dlopens.
add_library(MobileGL_FullServer SHARED
    BackendHost.cpp
    BackendPluginLoader.cpp
    UtilRuntimeLoader.cpp
    ServerCore.cpp
    FullServerEntry.cpp
    BfaFrontendShim.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../MG_Protocol/generated_wire_dispatch.cpp
)

target_link_libraries(MobileGL_FullServer PRIVATE
    MobileGL_MG_FullServerCore
    MobileGL_Protocol
    MobileGL_BFA
    MobileGL_RuntimeApi
    MobileGL_Transport
)

target_include_directories(MobileGL_FullServer PRIVATE
    ${CMAKE_SOURCE_DIR}/MobileGL
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/3rdparty/SPIRV-Reflect
    ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
    ${CMAKE_SOURCE_DIR}/3rdparty/FlatBuffers/include
)

if (UNIX AND NOT APPLE AND NOT ANDROID)
    target_link_libraries(MobileGL_FullServer PRIVATE ${CMAKE_DL_LIBS})
endif()

if (TARGET MG_FlatBuffersGen)
    add_dependencies(MobileGL_FullServer MG_FlatBuffersGen)
endif()
if (TARGET MG_WireDispatchGen)
    add_dependencies(MobileGL_FullServer MG_WireDispatchGen)
endif()

# End of File
