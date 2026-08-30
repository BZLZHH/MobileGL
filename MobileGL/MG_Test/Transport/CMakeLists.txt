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
        ProtocolDispatchCoverageTest
        ProtocolDispatchCoverageTest.cpp
)

target_include_directories(ProtocolDispatchCoverageTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
        ${MGL_ROOT}/3rdparty/FlatBuffers/include
)

target_link_libraries(
        ProtocolDispatchCoverageTest PRIVATE
        GTest::gtest_main
        MobileGL_Protocol
)

if (MSVC)
        target_compile_options(ProtocolDispatchCoverageTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ProtocolDispatchCoverageTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

if (TARGET MG_WireDispatchGen)
    add_dependencies(ProtocolDispatchCoverageTest MG_WireDispatchGen)
endif()
if (TARGET MG_WireClientGen)
    add_dependencies(ProtocolDispatchCoverageTest MG_WireClientGen)
endif()
if (TARGET MG_FlatBuffersGen)
    add_dependencies(ProtocolDispatchCoverageTest MG_FlatBuffersGen)
endif()

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
        ClientDeleteSyncTest
        ClientDeleteSyncTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientDeleteSyncTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientDeleteSyncTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientDeleteSyncTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientDeleteSyncTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientWaitSyncTest
        ClientWaitSyncTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientWaitSyncTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientWaitSyncTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientWaitSyncTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientWaitSyncTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientDrawArraysInstancedTest
        ClientDrawArraysInstancedTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientDrawArraysInstancedTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientDrawArraysInstancedTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientDrawArraysInstancedTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientDrawArraysInstancedTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientDrawElementsInstancedTest
        ClientDrawElementsInstancedTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientDrawElementsInstancedTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientDrawElementsInstancedTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientDrawElementsInstancedTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientDrawElementsInstancedTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientDrawRangeElementsTest
        ClientDrawRangeElementsTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientDrawRangeElementsTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientDrawRangeElementsTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientDrawRangeElementsTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientDrawRangeElementsTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientBufferRespecifyTest
        ClientBufferRespecifyTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientBufferRespecifyTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientBufferRespecifyTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientBufferRespecifyTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientBufferRespecifyTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientIndirectDrawsTest
        ClientIndirectDrawsTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientIndirectDrawsTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientIndirectDrawsTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientIndirectDrawsTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientIndirectDrawsTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientGetStringTest
        ClientGetStringTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientGetStringTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientGetStringTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientGetStringTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientGetStringTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientTextureRespecifyTest
        ClientTextureRespecifyTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientTextureRespecifyTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientTextureRespecifyTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientTextureRespecifyTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientTextureRespecifyTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientReadPixelsTest
        ClientReadPixelsTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientReadPixelsTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientReadPixelsTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientReadPixelsTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientReadPixelsTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientTextureSubImageTest
        ClientTextureSubImageTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientTextureSubImageTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientTextureSubImageTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientTextureSubImageTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientTextureSubImageTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientBufferReadbackTest
        ClientBufferReadbackTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientBufferReadbackTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientBufferReadbackTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientBufferReadbackTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientBufferReadbackTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientMapUnmapTest
        ClientMapUnmapTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientMapUnmapTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientMapUnmapTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientMapUnmapTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientMapUnmapTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientEglSurfaceTest
        ClientEglSurfaceTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientEglSurfaceTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientEglSurfaceTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientEglSurfaceTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientEglSurfaceTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientEglCommandsTest
        ClientEglCommandsTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientEglCommandsTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientEglCommandsTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientEglCommandsTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientEglCommandsTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientTokenSessionTest
        ClientTokenSessionTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientTokenSessionTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientTokenSessionTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientTokenSessionTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientTokenSessionTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientWirePayloadTest
        ClientWirePayloadTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientWirePayloadTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
        ${MGL_ROOT}/3rdparty/FlatBuffers/include
)

target_link_libraries(
        ClientWirePayloadTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientWirePayloadTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientWirePayloadTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

if (TARGET MG_FlatBuffersGen)
        add_dependencies(ClientWirePayloadTest MG_FlatBuffersGen)
endif()
if (TARGET MG_WireClientGen)
        add_dependencies(ClientWirePayloadTest MG_WireClientGen)
endif()
























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

add_executable(
        BfaFrontendShimTest
        BfaFrontendShimTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/BfaFrontendShim.cpp
)

target_include_directories(BfaFrontendShimTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        BfaFrontendShimTest PRIVATE
        GTest::gtest_main
)

if (MSVC)
        target_compile_options(BfaFrontendShimTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(BfaFrontendShimTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientClientWaitSyncTest
        ClientClientWaitSyncTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientClientWaitSyncTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientClientWaitSyncTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientClientWaitSyncTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientClientWaitSyncTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

add_executable(
        ClientQueryResultsTest
        ClientQueryResultsTest.cpp
        ${MGL_ROOT}/MobileGL/MG_FullServer/ServerCore.cpp
)

target_include_directories(ClientQueryResultsTest PRIVATE
        ${MGL_ROOT}/include
        ${MGL_ROOT}/MobileGL
        ${MGL_ROOT}/3rdparty/xxHash
        ${MGL_ROOT}/3rdparty/Vulkan-Headers/include
        ${MGL_ROOT}/3rdparty/SPIRV-Reflect
)

target_link_libraries(
        ClientQueryResultsTest PRIVATE
        GTest::gtest_main
        MobileGL_Client
        MobileGL_Transport
        MobileGL_BFA
)

if (MSVC)
        target_compile_options(ClientQueryResultsTest PRIVATE /Zc:preprocessor)
endif()

gtest_discover_tests(ClientQueryResultsTest DISCOVERY_TIMEOUT 30 PROPERTIES LABELS unit)

# Every test in this directory compiles ServerCore.cpp / includes Client.h,
# which transitively consume gen/wire_generated.h; make them wait for the
# flatbuffers codegen target (MG_FlatBuffersGen) to finish before compiling.
if (TARGET MG_FlatBuffersGen)
    get_property(_mobilegl_dir_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(_t IN LISTS _mobilegl_dir_targets)
        if (NOT _t MATCHES "^MobileGL_")
            add_dependencies(${_t} MG_FlatBuffersGen)
        endif()
    endforeach()
endif()








# ServerCore now references the frontend (GLImpl::GetString / pActiveBackendObject);
# link the FullServer core when it exists (CS refactor builds).
if (TARGET MobileGL_MG_FullServerCore)
    foreach(MGL_SERVERCORE_TEST
            BigServerE2ETest
            ClientAsyncBatchTest
            ClientBeginTransformFeedbackTest
            ClientBindTransformFeedbackTest
            ClientBlitFramebufferTest
            ClientBufferReadbackTest
            ClientBufferRespecifyTest
            ClientBufferSubDataShmTest
            ClientClearColorTest
            ClientClientWaitSyncTest
            ClientDeleteSyncTest
            ClientDispatchComputeIndirectTest
            ClientDispatchComputeTest
            ClientDrawArraysInstancedTest
            ClientDrawArraysTest
            ClientDrawElementsInstancedTest
            ClientDrawElementsShmTest
            ClientDrawRangeElementsTest
            ClientEglCommandsTest
            ClientEglSurfaceTest
            ClientEndTransformFeedbackTest
            ClientFenceSyncTest
            ClientGenerateMipmapTest
            ClientGetStringTest
            ClientHierarchyLifecycleTest
            ClientIndirectDrawsTest
            ClientMapUnmapTest
            ClientMemoryBarrierByRegionTest
            ClientMemoryBarrierTest
            ClientPatchParameteriTest
            ClientPauseResumeTransformFeedbackTest
            ClientQueryResultsTest
            ClientReadPixelsTest
            ClientServerEndToEndTest
            ClientSessionLifecycleTest
            ClientShmPayloadTest
            ClientSwapBuffersTest
            ClientTextureRespecifyTest
            ClientTextureSubImageTest
            ClientTokenSessionTest
            ClientWaitSyncTest
            ClientWirePayloadTest
            ShmPayloadBenchmark
)
        target_link_libraries(${MGL_SERVERCORE_TEST} PRIVATE MobileGL_MG_FullServerCore)
    endforeach()
endif()
