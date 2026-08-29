# MobileGL - MobileGL/MG_Benchmark/Transport/CMakeLists.txt
# Copyright (c) 2025-2026 MobileGL-Dev
# Licensed under the GNU Lesser General Public License v3.0:
#   https://www.gnu.org/licenses/gpl-3.0.txt
#   https://www.gnu.org/licenses/lgpl-3.0.txt
# SPDX-License-Identifier: LGPL-3.0-only
# End of Source File Header

# C/S cross-process round-trip benchmark. Loads libMobileGL_FullServer.so at
# runtime (MGL_REPO_ROOT should point at the repo root so it can find
# build_agent output), so it needs no MobileGL link-time dependency.
add_executable(CsRoundTripBench
  CsRoundTripBench.cpp
)

target_include_directories(CsRoundTripBench PRIVATE
  ${MGL_ROOT}/MobileGL
  ${MGL_ROOT}/include
  ${MGL_ROOT}/3rdparty/FlatBuffers/include
)

target_link_libraries(
  CsRoundTripBench PRIVATE
  benchmark::benchmark
  ${CMAKE_DL_LIBS}
)

add_test(NAME CsRoundTripBench COMMAND CsRoundTripBench --benchmark_counters_tabular=true)
set_tests_properties(CsRoundTripBench PROPERTIES LABELS benchmark)
