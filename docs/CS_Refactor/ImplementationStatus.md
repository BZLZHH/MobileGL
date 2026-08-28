# C/S Refactor Implementation Status

> 更新：Phase 0 完成，Phase 1-4 进行中（见下方清单）
> 分支：`Feat/cs-refactor`（从 `dev@81b17c0` 创建）
> 首次提交：`9be8bcff [Feat] (All): Add C/S refactor Phase 0-5 scaffolding and state/handle registry.`
> 源码目录约定：新 C/S 模块统一使用项目原有的 `MG_*` 前缀（`MG_Protocol` / `MG_Client` / `MG_FullServer` / `MG_Transport` / `MG_UtilRuntime`），保持 `MobileGL/` 下模块分层一致。

## 本阶段验证记录（已通过）

- 配置：`cmake -S . -B build_agent -DMOBILEGL_BUILD_TEST=ON -DMOBILEGL_BUILD_CS_REFACTOR=ON` ✅
- monolith（`MobileGL` / `MobileGL_s`）编译通过 ✅
- C/S 目标编译通过：`FullServer.so`、`MobileGL_Client`、`MobileGL_UtilRuntime`、`BackendObject_DirectGLES.so`、`BackendObject_DirectVulkan.so`、`MobileGL_Transport` ✅
- 单元测试：`ContextRegistryTest` 5/5、`HandleRegistryTest` 5/5 通过 ✅
- **目录重构后全量验证**：C/S 目标与全部相关测试重编译通过；`SanityTest` 82/82、`InProcessTransportTest` 1/1、`BigServerE2ETest` 1/1 通过；`FullServer` 插件生命周期 E2E 退出码 0
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
- [x] `MobileGL/MG_FullServer` — `FullServer` SHARED（`.so`，BigServer host 组件）
- [x] `MobileGL/MG_UtilRuntime` — `MobileGL_UtilRuntime` SHARED 骨架（`mobilegl_util_api` 导出）
- [x] `MobileGL/MG_Backend/CMakeLists.txt` — `BackendObject_DirectGLES` / `BackendObject_DirectVulkan` MODULE 骨架 + manifest
- [ ] 把真实 `MG_Impl / MG_State / MG_Util` 源列表拆入 `FullServerCore`，并从 monolith 删除（等 Phase 2/3 完成后再切）

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
- [ ] DirectGLES / DirectVulkan 后端迁移到 BFA vtable（device/sharedgroup/session 三层）
- [ ] `StateBackendObjectRegistry` key 改为 handle

## Phase 4 — 外部协议落地（进行中）

- [x] `MobileGL/MG_Transport/LocalSocketShmTransport.h/.cpp` — v1 transport 骨架（AF_UNIX socket + 共享内存接收接口）
- [x] `MobileGL/MG_Transport/InProcessTransport.h/.cpp` — 同进程 transport 实现（client/server 配对 + 批消息队列）
- [x] `MobileGL/MG_Transport/TransportInternal.h` — 统一 `MobileGLTransport` 内部完成类型（避免 ODR 冲突）
- [x] `MobileGL/MG_Test/Transport/InProcessTransportTest.cpp` — client→server→client 字节往来测试 1/1 通过
- [x] `MobileGL/MG_Transport/CMakeLists.txt` — `MobileGL_Transport` static library
- [x] `scripts/generate_protocol_fbs.py` → `MobileGL/MG_Protocol/protocol_generated.fbs`（2750 个 `Gl*` payload 表）
- [ ] FlatBuffers codegen 合并进 `protocol.fbs`（trampoline / dispatch / opcode / 分类表）
- [ ] 异步命令流 + 同步查询 / barrier
- [ ] map/unmap/readback/字符串返回数据通路
- [ ] Token 透传模型 + 会话生命周期

## Phase 5 — Plugin 化集成（进行中）

- [x] `MobileGL/MG_FullServer/UtilRuntimeLoader.h/.cpp` — dlopen `MobileGL_UtilRuntime.so` + ABI 握手
- [x] `MobileGL/MG_FullServer/BackendPluginLoader.h/.cpp` — dlopen BackendObject + manifest 协商 + Create
- [x] `MobileGL/MG_FullServer/BackendHost.h/.cpp` — Host vtable 注入
- [x] `MobileGL/MG_FullServer/Main.cpp` — 启动流程接线（UtilRuntime→BackendPlugin→Create→Initialize→Shutdown）
- [x] **插件生命周期 E2E 验证**：`FullServer <UtilRuntime.so> <BackendObject_DirectGLES.so>` 运行退出码 0
- [x] **BigServer 全链路 E2E（in-process）**：Client 命令 → InProcessTransport → `FullServer::ServerCore` → Backend VTable → Response → Client；`BigServerE2ETest` 1/1 通过
- [ ] BigServer 全链路 E2E（socket transport + FlatBuffers 解码）

## Phase 6 — 正确性 / 性能 / 平台

- [ ] 多 Session / 多 Display / share group 回归
- [ ] 命令批处理、零拷贝 benchmark
- [ ] 平台 Surface：X11 → Win32 → Android Binder

## 下一步

1. 完成 Phase 2 共享对象表迁移（先从 Buffer/Texture 入手）。
2. Phase 3 把 DirectGLES/DirectVulkan 的 device/sharedgroup/session 状态与 BFA 对齐。
3. Phase 4 生成完整 `protocol.fbs` + codegen + `LocalSocketShmTransport`。
4. Phase 5/6 插件化、E2E、回归与 benchmark。
