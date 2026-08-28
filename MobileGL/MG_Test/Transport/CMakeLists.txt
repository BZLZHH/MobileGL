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

add_executable(
        ClientSessionLifecycleTest
        ClientSessionLifecycleTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientSessionLifecycleTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientSessionLifecycleTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientSessionLifecycleTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientSessionLifecycleTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientHierarchyLifecycleTest
        ClientHierarchyLifecycleTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientHierarchyLifecycleTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientHierarchyLifecycleTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientHierarchyLifecycleTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientHierarchyLifecycleTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientClearColorTest
        ClientClearColorTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientClearColorTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientClearColorTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientClearColorTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientClearColorTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientDrawArraysTest
        ClientDrawArraysTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientDrawArraysTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientDrawArraysTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientDrawArraysTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientDrawArraysTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientDrawElementsShmTest
        ClientDrawElementsShmTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientDrawElementsShmTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientDrawElementsShmTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientDrawElementsShmTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientDrawElementsShmTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientBufferSubDataShmTest
        ClientBufferSubDataShmTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientBufferSubDataShmTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientBufferSubDataShmTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientBufferSubDataShmTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientBufferSubDataShmTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientMemoryBarrierTest
        ClientMemoryBarrierTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientMemoryBarrierTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientMemoryBarrierTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientMemoryBarrierTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientMemoryBarrierTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientPatchParameteriTest
        ClientPatchParameteriTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientPatchParameteriTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientPatchParameteriTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientPatchParameteriTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientPatchParameteriTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientGenerateMipmapTest
        ClientGenerateMipmapTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientGenerateMipmapTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientGenerateMipmapTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientGenerateMipmapTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientGenerateMipmapTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientDispatchComputeTest
        ClientDispatchComputeTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientDispatchComputeTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientDispatchComputeTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientDispatchComputeTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientDispatchComputeTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientBeginTransformFeedbackTest
        ClientBeginTransformFeedbackTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientBeginTransformFeedbackTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientBeginTransformFeedbackTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientBeginTransformFeedbackTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientBeginTransformFeedbackTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientEndTransformFeedbackTest
        ClientEndTransformFeedbackTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientEndTransformFeedbackTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientEndTransformFeedbackTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientEndTransformFeedbackTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientEndTransformFeedbackTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientPauseResumeTransformFeedbackTest
        ClientPauseResumeTransformFeedbackTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientPauseResumeTransformFeedbackTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientPauseResumeTransformFeedbackTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientPauseResumeTransformFeedbackTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientPauseResumeTransformFeedbackTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientBindTransformFeedbackTest
        ClientBindTransformFeedbackTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientBindTransformFeedbackTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientBindTransformFeedbackTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientBindTransformFeedbackTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientBindTransformFeedbackTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientBlitFramebufferTest
        ClientBlitFramebufferTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientBlitFramebufferTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientBlitFramebufferTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientBlitFramebufferTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientBlitFramebufferTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientMemoryBarrierByRegionTest
        ClientMemoryBarrierByRegionTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientMemoryBarrierByRegionTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientMemoryBarrierByRegionTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientMemoryBarrierByRegionTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientMemoryBarrierByRegionTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientSwapBuffersTest
        ClientSwapBuffersTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientSwapBuffersTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientSwapBuffersTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientSwapBuffersTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientSwapBuffersTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientDispatchComputeIndirectTest
        ClientDispatchComputeIndirectTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientDispatchComputeIndirectTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientDispatchComputeIndirectTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientDispatchComputeIndirectTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientDispatchComputeIndirectTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientFenceSyncTest
        ClientFenceSyncTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientFenceSyncTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientFenceSyncTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientFenceSyncTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientFenceSyncTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)



















add_executable(
        ShmPayloadBenchmark
        ShmPayloadBenchmark.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ShmPayloadBenchmark PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ShmPayloadBenchmark PRIVATE
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ShmPayloadBenchmark PRIVATE /Zc:preprocessor)
endif()







