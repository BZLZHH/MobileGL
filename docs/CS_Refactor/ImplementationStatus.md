# C/S Refactor Implementation Status

> 更新：Phase 0 完成，Phase 1-4 进行中（见下方清单）
> 分支：`Feat/cs-refactor`（从 `dev@81b17c0` 创建）
> 首次提交：`9be8bcff [Feat] (All): Add C/S refactor Phase 0-5 scaffolding and state/handle registry.`
> 源码目录约定：新 C/S 模块统一使用项目原有的 `MG_*` 前缀（`MG_Protocol` / `MG_Client` / `MG_FullServer` / `MG_Transport` / `MG_UtilRuntime`），保持 `MobileGL/` 下模块分层一致。

## 本阶段验证记录（已通过）

- 配置：`cmake -S . -B build_agent -DMOBILEGL_BUILD_TEST=ON -DMOBILEGL_BUILD_CS_REFACTOR=ON` ✅
- monolith（`MobileGL` / `MobileGL_s`）编译通过 ✅
- C/S 目标编译通过：`libMobileGL_FullServer.so`（链接 `libMobileGL_MG_FullServerCore.a`）、`libMobileGL_Client.so`、`libMobileGL_UtilRuntime.so`、`BackendObject_DirectGLES.so`、`BackendObject_DirectVulkan.so`、`libMobileGL_Transport.a` ✅
- 单元测试：`ContextRegistryTest` 5/5、`HandleRegistryTest` 5/5 通过 ✅
- **目录重构后全量验证**：C/S 目标与全部相关测试重编译通过；`SanityTest` 82/82、`InProcessTransportTest` 1/1、`BigServerE2ETest` 1/1 通过；`libMobileGL_FullServer.so` 构建成功
- **全量 ctest（unit）**：1411/1411 通过（3 skipped）✅（含 ClientMemoryBarrier 等新测试）
- 第二次构建（共享 Buffer 表迁移后）：`BufferState` 委托 group 级 `SharedBufferObjectTable`，跨 session 可见性测试通过 ✅

## Phase 0 — 契约定稿 ✅

- [x] `docs/CS_Refactor/HandleSessionGeneration.md` — Handle/Session/Generation 语义
- [x] `MobileGL/MG_Protocol/protocol.fbs` — FlatBuffers `Message` + `Payload` union 草案
- [x] `MobileGL/MG_Protocol/bfa.h` — Backend Frontend API strict C ABI
- [x] `MobileGL/MG_Protocol/mgruntime_api.h` — UtilRuntime 分域 sub-vtable C ABI
- [x] `MobileGL/MG_Protocol/transport.h` — Transport ops 定义
- [x] `MobileGL/MG_Protocol/api_manifest.txt` + `scripts/extract_api_manifest.py` — API 清单提取器（1720 条）
- [x] `scripts/generate_protocol_fbs.py` — 从 Definitions.cpp 生成 `protocol_generated.fbs`（2750 个 `Gl*` payload 表）
- [x] `MobileGL/MG_Protocol/CMakeLists.txt` — `MobileGL_Protocol` / `MobileGL_BFA` / `MobileGL_RuntimeApi` INTERFACE targets

## Phase 1 — 构建拆分（脚手架，待完整化）

- [x] 根 CMake 增加 `MOBILEGL_BUILD_CS_REFACTOR`（默认 OFF，保留 monolith 对照）
- [x] `MobileGL/MG_Client` — `MobileGL_Client` SHARED 骨架
- [x] `MobileGL/MG_FullServer` — `MobileGL_FullServer` SHARED（`libMobileGL_FullServer.so`，BigServer host 组件）
- [x] `FullServerEntry.h/.cpp` — C ABI 宿主入口：`mobilegl_fullserver_create/start/attach_transport/service_once/run_socket/destroy`
- [x] **跨进程 E2E（Python 客户端）**：Python + flatbuffers 构建 `Message` → ABC socket → `libMobileGL_FullServer.so` `run_socket` → null backend `Clear` → `Response{status=0}` 返回；验证成功
- [x] **dlopen 验证**：Python ctypes 加载 `libMobileGL_FullServer.so` → create(UtilRuntime.so, BackendObject_DirectGLES.so) → start → destroy 成功
- [x] `MobileGL/MG_UtilRuntime` — `MobileGL_UtilRuntime` SHARED 骨架（`mobilegl_util_api` 导出）
- [x] `MobileGL/MG_Backend/CMakeLists.txt` — `BackendObject_DirectGLES` / `BackendObject_DirectVulkan` MODULE 骨架 + manifest
- [x] 把真实 `MG_Impl / MG_State / MG_Util` 源列表拆入 `MobileGL_MG_FullServerCore`（STATIC，不含 MG_Backend），`MobileGL_FullServer.so` 链接它；monolith 仍保留作对照
- [ ] 最终从 monolith 删除（等 Phase 3 后端迁移完成后再切）

## Phase 2 — FullServer 状态模型（共享对象表迁移 + pGLContext 路由完成）

- [x] `MobileGL/MG_State/GLState/ContextRegistry.h/.cpp` — `GLSharedGroup` / `GLContextSession` / `GLContextRegistry`
- [x] `MobileGL/MG_State/GLState/SharedObjectTables.h` — group 级共享对象表容器（BufferState 起步）
- [x] EGL `CreateContext` / `DestroyContext` / `MakeCurrent` / `ReleaseThread` 接入 registry 生命周期
- [x] `MobileGL/MG_State/GLState/HandleRegistry.h/.cpp` — `(groupId/sessionId, kind, glName) → MobileGLBackendHandle`
- [x] `GLContext::GetObjectHandle` — 前端对象首次访问时分配/复用 C/S handle
- [x] Buffer/Texture 对象创建时分配 handle、删除时释放（`CreateBufferObject` / `CreateTextureObject` / `CreateTextureViewObject` / 对应 Mark*ForDeletion）
- [x] **Buffer 共享对象表迁移**：`SharedBufferObjectTable` 由 `SharedObjectTables` 持有，`BufferState` 的 name/object 表委托给 group；binding slots 仍 per-context（GL shareCtx 语义）
- [x] **Texture 共享对象表迁移**：`SharedTextureObjectTable` 由 `SharedObjectTables` 持有，`TextureState` 的 name/object 表委托给 group；default texture 与 unit bindings 仍 per-context
- [x] **Sampler 共享对象表迁移**：`SharedSamplerObjectTable` 由 `SharedObjectTables` 持有，`SamplerState` 的 name/object 表委托给 group
- [x] **Renderbuffer 共享对象表迁移**：`SharedRenderbufferObjectTable` 由 `SharedObjectTables` 持有，`RenderbufferState` 的 name/object 表委托给 group；binding slots 仍 per-context
- [x] **Framebuffer 共享对象表迁移**：`SharedFramebufferObjectTable` 由 `SharedObjectTables` 持有，`FramebufferState` 的 name/object 表委托给 group（含 FBO 0 语义）；binding slots 仍 per-context
- [x] **VertexArray 共享对象表迁移**：`SharedVertexArrayObjectTable`（vector + name generator）由 `SharedObjectTables` 持有，`VertexArrayState` 的 object 表委托给 group；bound index/detached 仍 per-context
- [x] **Program/Shader 共享对象表迁移**：`SharedProgramObjectTable`（program/shaders 两个 vector + 联合 name generator）由 `SharedObjectTables` 持有，`ProgramState` 的 object 表委托给 group；`m_currentProgram` 与 compile caches 仍 per-context
- [x] **TransformFeedback 共享对象表迁移**：`SharedTransformFeedbackObjectTable`（对象 map + name generator）由 `SharedObjectTables` 持有；bound index / live capture state / buffer binding points 仍 per-context
- [x] **ProgramPipeline 共享对象表迁移**：`SharedProgramPipelineObjectTable`（pipeline map + name generator）由 `SharedObjectTables` 持有；bound pipeline index 仍 per-context
- [x] `MobileGL/MG_Test/State/ContextRegistryTest.cpp` / `HandleRegistryTest.cpp` — 单元测试（Buffer/Texture/Sampler/Renderbuffer/Framebuffer/VAO/Program/Shader/TransformFeedback/ProgramPipeline 跨 session 可见性验证）
- [x] **pGLContext 路由完成**：`pGLContext` 改为非拥有 `GLContext*`，EGL `MakeCurrent`/`ReleaseThread` 将其指向/清空当前 session；`SanityTest` 82/82 通过
- [ ] Query/Sync session-private 状态层落地（HandleRegistry 已按此分类）

## Phase 3 — BFA 落地（进行中）

- [x] `MobileGL/MG_FullServer/BackendHost.h/.cpp` — FullServer 侧的 BackendHost vtable 骨架
- [x] `MobileGL/MG_FullServer/BackendPluginLoader.h/.cpp` — dlopen + manifest ABI 校验 + Create
- [x] `StateBackendObjectRegistry` 增加 handle-key 并行查找：`RegisterHandle` / `FindByHandle` / `UnregisterHandle`（主 map 仍按 state 指针走热路径；原生 Monolith 编译通过）
- [x] **Display / SharedGroup 生命周期控制**：`control.h` 增加 `DisplayCreate/Destroy` + `SharedGroupCreate/Destroy`；`Client::SubmitDisplayControl/SubmitSharedGroupControl`；`ServerCore` 调 `OnDisplayCreated/Destroyed/OnSharedGroupCreated/Destroyed`；`ClientHierarchyLifecycleTest` 1/1 通过
- [ ] DirectGLES / DirectVulkan 后端迁移到 BFA vtable（device/sharedgroup/session 三层）

## Phase 4 — 外部协议落地（进行中）

- [x] `MobileGL/MG_Transport/LocalSocketShmTransport.h/.cpp` — v1 transport：AF_UNIX socket connect / bind+listen / accept + 长度前缀字节流收发
- [x] **shm arena**：`OpenSharedMemory`（memfd_create + mmap）/ `ReleaseSharedMemory`；`InProcessTransportTest.LocalSocketShmAllocatesSharedMemory` 3/3 通过
- [x] **shm fd 跨进程传递**：`SendShmHandle` / `RecvShmHandle`（SCM_RIGHTS + 控制消息携带 size）；`LocalSocketShmTransfersShmFd` 双向读写验证，InProcessTransportTest 4/4 通过
- [x] **shm payload 回读通路**：`Client::SubmitDataCommand` → batch 携带 shm fd → `ServerCore` 用 `ReceiveShmHandle` 接收并读首字节 → `Response.data_byte` 回显；`ClientShmPayloadTest` 1/1 通过
- [x] transport ops 新增 `ReceiveShmHandle`（ABI minor+0.1 兼容追加）
- [x] `MobileGL/MG_Transport/InProcessTransport.h/.cpp` — 同进程 transport 实现（client/server 配对 + 批消息队列）
- [x] `MobileGL/MG_Transport/TransportInternal.h` — 统一 `MobileGLTransport` 内部完成类型（避免 ODR 冲突）
- [x] `MobileGL/MG_Test/Transport/InProcessTransportTest.cpp` — in-process + LocalSocketShm 往返 + shm arena + fd 传递测试 4/4 通过
- [x] `MobileGL/MG_Client` — Client 命令 API：`InitializeWithTransport` + `SendCommand(sessionId, opcode)`（提交/等待响应）+ 异步 `SubmitCommand` / `WaitResponseForToken`
- [x] `MobileGL/MG_Test/Transport/ClientAsyncBatchTest.cpp` — 8 条命令先全部提交、再按 token 收响应，1/1 通过
- [x] `MobileGL/MG_Test/Transport/ClientServerEndToEndTest.cpp` — Client 命令 → InProcess → ServerCore → VTable → 响应，1/1 通过
- [x] `InProcessTransport::WaitResponses` 改为等待语义（timeoutMs=0 无限等待）
- [x] `MobileGL/MG_Transport/CMakeLists.txt` — `MobileGL_Transport` static library
- [x] `scripts/generate_protocol_fbs.py` → `MobileGL/MG_Protocol/protocol_generated.fbs`（2750 个 `Gl*` payload 表）
- [x] `scripts/generate_opcode_table.py` → `MobileGL/MG_Protocol/generated_opcodes.h`（1396 个 GL/EGL opcode + 名称表）
- [x] `MobileGL/MG_Test/Transport/ProtocolOpcodeTest.cpp` — 生成表计数与名称查询 1/1 通过
- [x] `MobileGL/MG_Protocol/wire.fbs` — 最小 wire schema + flatc 生成 `gen/wire_generated.h`（8KB）
- [x] `3rdparty/FlatBuffers/include` — vendored flatbuffers 头（flatc v25.12.19 兼容）
- [x] **Client/ServerCore 改用 FlatBuffers 实际编解码**：`Client::SendCommand` 构建 `Message`；`ServerCore` 解析 `Message` 并按 opcode 分发、构建 `Response`
- [x] **typed payload trampoline 起步**：wire 增加 `ClearColor{red,green,blue,alpha}`；`Client::SendClearColor` + `ServerCore` 解包调 BFA `ClearColor`；`ClientClearColorTest` 1/1 通过（float 字段精确校验）
- [x] **DrawArrays typed payload**：wire 增加 `DrawArrays{mode,first,count}`；`Client::SendDrawArrays` + `ServerCore` 解包调 BFA `DrawArrays` + null adapter 存根；`ClientDrawArraysTest` 1/1 通过
- [x] FlatBuffers 迁移后回归：`BigServerE2ETest` 2/2、`ClientServerEndToEndTest` 1/1、`InProcessTransportTest` 2/2、`ProtocolOpcodeTest` 1/1 全部通过
- [x] **完整 API merge 流水线**：`scripts/merge_protocol_fbs.py` 将 2750 个生成 payload 表并入 `protocol.fbs` 的插入点，产出 `protocol_full.fbs`（无重复、flatc 校验通过；`GeneratedPayload` union 因 >255 成员改为 opcode 分发）
- [x] `scripts/generate_dispatch_table.py` → `generated_dispatch.h`（1396 条 opcode→API→payload 表元数据）
- [x] `ProtocolOpcodeTest` 增加 dispatch 元数据校验，2/2 通过
- [ ] 完整 source-list 运行时分发（trampoline / dispatch / 分类表）
- [x] **异步命令提交 + 按 token 等待**（`Client::SubmitCommand` / `WaitResponseForToken`；`ClientAsyncBatchTest` 1/1 通过）
- [x] **Token 透传**：`Command.token` / `Response.token`；`Client::SendCommand` 校验回显 token；Python 跨进程 E2E 仍 status=0
- [x] **shm payload 回读**：`ClientShmPayloadTest` 已验证（0xAB 写入 → fd → server 读回 → data_byte=0xAB）
- [x] **会话生命周期**：`control.h` 定义 `SessionCreate/Destroy` 控制 opcode；`Client::SubmitSessionControl` + `ServerCore` 调 `OnSessionCreated/OnSessionDestroyed`；ServerCore 维护 live-session 集合，destroy 后同一 session 命令被拒绝（status!=0）；`ClientSessionLifecycleTest` 1/1 通过
- [ ] Token↔session 完整失效模型（多 socket/超时）

## Phase 5 — Plugin 化集成（进行中）

- [x] `MobileGL/MG_FullServer/UtilRuntimeLoader.h/.cpp` — dlopen `MobileGL_UtilRuntime.so` + ABI 握手
- [x] `MobileGL/MG_FullServer/BackendPluginLoader.h/.cpp` — dlopen BackendObject + manifest 协商 + Create
- [x] `MobileGL/MG_FullServer/BackendHost.h/.cpp` — Host vtable 注入
- [x] `MobileGL/MG_FullServer/Main.cpp` — 启动流程接线（UtilRuntime→BackendPlugin→Create→Initialize→Shutdown）
- [x] **插件生命周期链路（组件级验证）**：`UtilRuntimeLoader` / `BackendPluginLoader` / `ServerCore` 在 `libMobileGL_FullServer.so` 内编译通过；`BigServerE2ETest` 覆盖 Create→Initialize→command dispatch→Shutdown
- [x] **DrawElements + shm indices（零拷贝 draw 数据路径）**：wire 增加 `DrawElements{mode,count,type,indices_offset}`；`Client::SendDrawElements` 携带 shm fd；`ServerCore` 接收并映射后把 indices 指针传给 BFA `DrawElements`；`ClientDrawElementsShmTest` 1/1 通过（offset=1 读到 0x2B）
- [x] **DrawElements + shm indices（零拷贝 draw 数据路径）**：wire 增加 `DrawElements{mode,count,type,indices_offset}`；`Client::SendDrawElements` 携带 shm fd；`ServerCore` 接收并映射后把 indices 指针传给 BFA `DrawElements`；`ClientDrawElementsShmTest` 1/1 通过（offset=1 读到 0x2B）
- [x] **BufferSubData + shm data（buffer 数据通路）**：wire 增加 `BufferSubData{buffer_handle,offset,size}`；`Client::SendBufferSubData` 携带 shm fd；`ServerCore` 把 shm 指针传给 BFA `BufferSubData`；`ClientBufferSubDataShmTest` 1/1 通过（handle/offset/size/首字节 0x44 精确回查）
- [x] **PatchParameteri scalar 命令**：wire 增加 `PatchParameteri{pname,value}`；`Client::SendPatchParameteri` + `ServerCore` 解包调 BFA `PatchParameteri`；`ClientPatchParameteriTest` 1/1 通过（0x1234/-7 精确回查）
- [x] DirectGLES / DirectVulkan null adapter 提供 `Initialize/Shutdown/Display/SharedGroup/Session/Clear/ClearColor/DrawArrays/DrawElements/BufferSubData/MemoryBarrier/PatchParameteri` 存根
- [x] **BigServer 全链路 E2E（in-process）**：Client 命令 → InProcessTransport → `FullServer::ServerCore` → Backend VTable → Response → Client；`BigServerE2ETest` 1/1 通过
- [x] **BigServer 全链路 E2E（LocalSocketShm）**：client socket → server accept → `ServerCore` 分发 → 响应返回 client；`BigServerE2ETest` 2/2 通过
- [x] ServerCore 分发改为使用生成 opcode 表（`MobileGLOpcode::glClear`），BigServerE2E 通过
- [ ] BigServer 全链路 E2E（socket transport + FlatBuffers 解码）

## Phase 6 — 正确性 / 性能 / 平台

- [x] 多 Session / 多 Display / share group 回归：`ContextRegistryTest` 6/6 通过（含跨 session 对象可见性、不同 Display 分组隔离）
- [x] **命令往返基准**：`scripts/bench_cs_e2e.py`（Python FlatBuffers → socket → FullServer.so → backend），含 SessionCreate/Destroy 生命周期，100 次往返 avg 30.5µs / min 25.2µs / max 107.5µs（null backend）
- [x] **shm payload 零拷贝基准**：`ShmPayloadBenchmark`（C++：SessionCreate → 100× SubmitDataCommand+fd 回读 → destroy），avg 34.1µs / min 25.8µs / max 83.4µs（含 SCM_RIGHTS fd + mmap 回读）
- [ ] 命令批处理、零拷贝 benchmark（shm payload 传输）
- [ ] 平台 Surface：X11 → Win32 → Android Binder

## 下一步

1. 完成 Phase 2 共享对象表迁移（先从 Buffer/Texture 入手）。
2. Phase 3 把 DirectGLES/DirectVulkan 的 device/sharedgroup/session 状态与 BFA 对齐。
3. Phase 4 生成完整 `protocol.fbs` + codegen + `LocalSocketShmTransport`。
4. Phase 5/6 插件化、E2E、回归与 benchmark。
