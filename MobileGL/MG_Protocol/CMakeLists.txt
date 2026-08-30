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
# wire_full.fbs -> gen/wire_full_generated.h (generic payload_bytes path)
# Generated at build time (cached via the MOBILEGL_FLATC cache variable); the
# output headers are not tracked in git.
# -----------------------------------------------------------------------------
find_program(MOBILEGL_FLATC NAMES flatc PATHS /tmp/flatc_bin)
find_program(MOBILEGL_PYTHON NAMES python3 python)
if(MOBILEGL_FLATC AND MOBILEGL_PYTHON)
    set(_WIRE_GEN_H ${CMAKE_CURRENT_SOURCE_DIR}/gen/wire_generated.h)
    add_custom_command(
        OUTPUT ${_WIRE_GEN_H}
        COMMAND ${MOBILEGL_FLATC} --cpp --filename-suffix _generated
                -o ${CMAKE_CURRENT_SOURCE_DIR}/gen ${CMAKE_CURRENT_SOURCE_DIR}/wire.fbs
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/wire.fbs
        COMMENT "Generating wire_generated.h from wire.fbs"
        VERBATIM
    )

    set(_PROTO_GEN_FBS ${CMAKE_CURRENT_SOURCE_DIR}/protocol_generated.fbs)
    add_custom_command(
        OUTPUT ${_PROTO_GEN_FBS}
        COMMAND ${MOBILEGL_PYTHON} ${CMAKE_SOURCE_DIR}/scripts/generate_protocol_fbs.py
        DEPENDS ${CMAKE_SOURCE_DIR}/scripts/generate_protocol_fbs.py
                ${CMAKE_SOURCE_DIR}/MobileGL/MG_Impl/GLImpl/Exporting/Definitions.cpp
                ${CMAKE_SOURCE_DIR}/MobileGL/MG_Impl/EGLImpl/Exporting/Definitions.cpp
        COMMENT "Regenerating protocol_generated.fbs from Definitions.cpp"
        VERBATIM
    )

    set(_WIRE_FULL_FBS ${CMAKE_CURRENT_SOURCE_DIR}/wire_full.fbs)
    set(_WIRE_FULL_GEN_H ${CMAKE_CURRENT_SOURCE_DIR}/gen/wire_full_generated.h)
    add_custom_command(
        OUTPUT ${_WIRE_FULL_FBS}
        COMMAND ${MOBILEGL_PYTHON} ${CMAKE_SOURCE_DIR}/scripts/generate_wire_full_fbs.py
        DEPENDS ${CMAKE_SOURCE_DIR}/scripts/generate_wire_full_fbs.py
                ${_PROTO_GEN_FBS}
        COMMENT "Generating wire_full.fbs from protocol_generated.fbs"
        VERBATIM
    )
    add_custom_command(
        OUTPUT ${_WIRE_FULL_GEN_H}
        COMMAND ${MOBILEGL_FLATC} --cpp --filename-suffix _generated
                -o ${CMAKE_CURRENT_SOURCE_DIR}/gen ${_WIRE_FULL_FBS}
        DEPENDS ${_WIRE_FULL_FBS}
        COMMENT "Generating wire_full_generated.h from wire_full.fbs"
        VERBATIM
    )

    add_custom_target(MG_FlatBuffersGen ALL DEPENDS ${_WIRE_GEN_H} ${_WIRE_FULL_GEN_H})

    set(_WIRE_DISPATCH_H ${CMAKE_CURRENT_SOURCE_DIR}/generated_wire_dispatch.h)
    set(_WIRE_DISPATCH_CPP ${CMAKE_CURRENT_SOURCE_DIR}/generated_wire_dispatch.cpp)
    add_custom_command(
        OUTPUT ${_WIRE_DISPATCH_H} ${_WIRE_DISPATCH_CPP}
        COMMAND ${MOBILEGL_PYTHON} ${CMAKE_SOURCE_DIR}/scripts/generate_wire_dispatch.py
        DEPENDS ${CMAKE_SOURCE_DIR}/scripts/generate_wire_dispatch.py
                ${CMAKE_SOURCE_DIR}/MobileGL/MG_Impl/GLImpl/Exporting/Definitions.cpp
                ${CMAKE_SOURCE_DIR}/MobileGL/MG_Impl/EGLImpl/Exporting/Definitions.cpp
                ${_WIRE_FULL_FBS}
                ${CMAKE_CURRENT_SOURCE_DIR}/generated_opcodes.h
                ${CMAKE_CURRENT_SOURCE_DIR}/generated_dispatch.h
        COMMENT "Generating generated_wire_dispatch.h/.cpp"
        VERBATIM
    )
    add_custom_target(MG_WireDispatchGen ALL DEPENDS ${_WIRE_DISPATCH_H} ${_WIRE_DISPATCH_CPP})

    set(_WIRE_CLIENT_H ${CMAKE_SOURCE_DIR}/MobileGL/MG_Client/generated_wire_client.h)
    add_custom_command(
        OUTPUT ${_WIRE_CLIENT_H}
        COMMAND ${MOBILEGL_PYTHON} ${CMAKE_SOURCE_DIR}/scripts/generate_wire_client.py
        DEPENDS ${CMAKE_SOURCE_DIR}/scripts/generate_wire_client.py
                ${CMAKE_SOURCE_DIR}/MobileGL/MG_Impl/GLImpl/Exporting/Definitions.cpp
                ${CMAKE_SOURCE_DIR}/MobileGL/MG_Impl/EGLImpl/Exporting/Definitions.cpp
                ${_WIRE_FULL_FBS}
                ${CMAKE_CURRENT_SOURCE_DIR}/generated_opcodes.h
                ${CMAKE_CURRENT_SOURCE_DIR}/generated_dispatch.h
        COMMENT "Generating generated_wire_client.h"
        VERBATIM
    )
    add_custom_target(MG_WireClientGen ALL DEPENDS ${_WIRE_CLIENT_H})

    set(_TRAMPOLINE_H ${CMAKE_SOURCE_DIR}/MobileGL/MG_Client/generated_wire_trampoline.h)
    set(_TRAMPOLINE_CPP ${CMAKE_SOURCE_DIR}/MobileGL/MG_Client/generated_wire_trampoline.cpp)
    set(_TRAMPOLINE_COVERAGE ${CMAKE_SOURCE_DIR}/MobileGL/MG_Client/generated_trampoline_coverage.txt)
    add_custom_command(
        OUTPUT ${_TRAMPOLINE_H} ${_TRAMPOLINE_CPP} ${_TRAMPOLINE_COVERAGE}
        COMMAND ${MOBILEGL_PYTHON} ${CMAKE_SOURCE_DIR}/scripts/generate_client_trampoline.py
        DEPENDS ${CMAKE_SOURCE_DIR}/scripts/generate_client_trampoline.py
                ${CMAKE_SOURCE_DIR}/MobileGL/MG_Impl/GLImpl/Exporting/Definitions.cpp
                ${_WIRE_FULL_FBS}
                ${CMAKE_CURRENT_SOURCE_DIR}/generated_opcodes.h
                ${CMAKE_CURRENT_SOURCE_DIR}/generated_dispatch.h
        COMMENT "Generating generated_wire_trampoline.h/.cpp"
        VERBATIM
    )
    add_custom_target(MG_TrampolineGen ALL DEPENDS ${_TRAMPOLINE_H} ${_TRAMPOLINE_CPP} ${_TRAMPOLINE_COVERAGE})
else()
    message(WARNING
        "flatc not found on PATH or /tmp/flatc_bin; "
        "MobileGL/MG_Protocol/gen/wire_generated.h must already exist "
        "for the C/S targets to compile (set -DMOBILEGL_FLATC=<path> to fix).")
endif()

# End of File
