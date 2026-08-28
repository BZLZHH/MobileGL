cmake_minimum_required(VERSION 3.24)

add_executable(
        InProcessTransportTest
        InProcessTransportTest.cpp
)

target_include_directories(InProcessTransportTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        InProcessTransportTest PRIVATE
        GTest::gtest_main
        MobileGL_Transport
)

if (MSVC)
        target_compile_options(InProcessTransportTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(InProcessTransportTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)
