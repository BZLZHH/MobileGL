# MobileGL - MobileGL/MG_Client/CMakeLists.txt
# Copyright (c) 2025-2026 MobileGL-Dev
# Licensed under the GNU Lesser General Public License v3.0:
#   https://www.gnu.org/licenses/gpl-3.0.txt
#   https://www.gnu.org/licenses/lgpl-3.0.txt
# SPDX-License-Identifier: LGPL-3.0-only
# End of Source File Header

add_library(MobileGL_Client SHARED
    Client.cpp
)

target_link_libraries(MobileGL_Client PRIVATE
    MobileGL_Protocol
    MobileGL_Transport
)

target_include_directories(MobileGL_Client PRIVATE
    ${CMAKE_SOURCE_DIR}/MobileGL
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/3rdparty/FlatBuffers/include
)

if (TARGET MG_FlatBuffersGen)
    add_dependencies(MobileGL_Client MG_FlatBuffersGen)
endif()
if (TARGET MG_WireClientGen)
    add_dependencies(MobileGL_Client MG_WireClientGen)
endif()

# End of File
