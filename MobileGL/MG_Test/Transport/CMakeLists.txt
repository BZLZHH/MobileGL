cmake_minimum_required(VERSION 3.24)

include_directories(${MGL_ROOT}/3rdparty/FlatBuffers/include)

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

add_executable(
        BigServerE2ETest
        BigServerE2ETest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(BigServerE2ETest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        BigServerE2ETest PRIVATE
        GTest::gtest_main
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(BigServerE2ETest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(BigServerE2ETest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ProtocolOpcodeTest
        ProtocolOpcodeTest.cpp
)

target_include_directories(ProtocolOpcodeTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ProtocolOpcodeTest PRIVATE
        GTest::gtest_main
        MobileGL_Protocol
)

if (MSVC)
        target_compile_options(ProtocolOpcodeTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ProtocolOpcodeTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientServerEndToEndTest
        ClientServerEndToEndTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientServerEndToEndTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientServerEndToEndTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientServerEndToEndTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientServerEndToEndTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientAsyncBatchTest
        ClientAsyncBatchTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientAsyncBatchTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientAsyncBatchTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientAsyncBatchTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientAsyncBatchTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientShmPayloadTest
        ClientShmPayloadTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientShmPayloadTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientShmPayloadTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientShmPayloadTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientShmPayloadTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)





