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

# -----------------------------------------------------------------------------
# wire.fbs -> gen/wire_generated.h
# Generated at build time (cached via the MOBILEGL_FLATC cache variable); the
# output header is not tracked in git.
# -----------------------------------------------------------------------------
find_program(MOBILEGL_FLATC NAMES flatc PATHS /tmp/flatc_bin)
if(MOBILEGL_FLATC)
    set(_WIRE_GEN_H ${CMAKE_CURRENT_SOURCE_DIR}/gen/wire_generated.h)
    add_custom_command(
        OUTPUT ${_WIRE_GEN_H}
        COMMAND ${MOBILEGL_FLATC} --cpp --filename-suffix _generated
                -o ${CMAKE_CURRENT_SOURCE_DIR}/gen ${CMAKE_CURRENT_SOURCE_DIR}/wire.fbs
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/wire.fbs
        COMMENT "Generating wire_generated.h from wire.fbs"
        VERBATIM
    )
    add_custom_target(MG_FlatBuffersGen ALL DEPENDS ${_WIRE_GEN_H})
else()
    message(WARNING
        "flatc not found on PATH or /tmp/flatc_bin; "
        "MobileGL/MG_Protocol/gen/wire_generated.h must already exist "
        "for the C/S targets to compile (set -DMOBILEGL_FLATC=<path> to fix).")
endif()

# End of File
