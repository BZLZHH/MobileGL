# MobileGL - MobileGL/MG_Protocol/CMakeLists.txt
# Copyright (c) 2025-2026 MobileGL-Dev
# Licensed under the GNU Lesser General Public License v3.0:
#   https://www.gnu.org/licenses/gpl-3.0.txt
#   https://www.gnu.org/licenses/lgpl-3.0.txt
# SPDX-License-Identifier: LGPL-3.0-only
# End of Source File Header

# -----------------------------------------------------------------------------
# C/S contract interface targets. These are header-only targets shared by the
# Client, FullServer, UtilRuntime and BackendObject plugins.
# -----------------------------------------------------------------------------

add_library(MobileGL_Protocol INTERFACE)
target_include_directories(MobileGL_Protocol INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(MobileGL_BFA INTERFACE)
target_include_directories(MobileGL_BFA INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
# Plugin sources use <Includes.h>, whose centralized include list pulls in
# <xxhash.h>; the interface target carries it so BackendObject plugins compile.
target_link_libraries(MobileGL_BFA INTERFACE
    xxHash::xxhash
)

add_library(MobileGL_RuntimeApi INTERFACE)
target_include_directories(MobileGL_RuntimeApi INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(MobileGL_RuntimeApi INTERFACE
    xxHash::xxhash
)

# End of File
