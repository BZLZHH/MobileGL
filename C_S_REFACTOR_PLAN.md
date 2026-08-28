# MobileGL C/S 架构重构计划

> **状态**：规划文档（只读分析，不实施）
> **范围**：MobileGL 项目从"单进程 monolith 库"重构为"Client / FullServer / BackendObject 插件"的 C/S 架构
> **日期**：2026-08-28
> **目标读者**：MobileGL 核心开发者

---

## 1. 背景与总体目标

### 1.1 原始需求

> 把项目改为 C/S 架构，且 **Server(per BackendObject) 单独一个 target**。

### 1.2 当前架构问题

- 当前所有代码编译进同一个 `MobileGL.so` / `MobileGL_s.a`（`CMakeLists.txt:483-582`）。
- `MG_Impl`（GL/EGL 实现）、`MG_State`（GL 状态机）、`MG_Util`、`MG_Backend`（DirectGLES / DirectVulkan）全部耦合在一个库中。
- `BackendObject` 已经是中间层雏形，但它是进程内 C++ 多态接口：
  - 函数参数包含 `SharedPtr<MG_State::...>`（`BackendObject.h:158-188`）；
  - 后端直接读取全局 `MG_State::pGLContext`（数百处访问）；
  - 后端状态是进程全局（`pVulkanRenderer`、`g_activeBufferManager`、`g_GLESFuncs` 等）；
  - `MG_State::pGLContext` 是进程级单例（`MG_State/GLState/Core.cpp:20`），不支持多 context。

### 1.3 核心判断

> 正确的 C/S 切点是 **公开 EGL/GL API 层**，而不是 `BackendObject` 接口。
> Server 侧运行"完整现有 frontend + 一个 BackendObject"，Client 侧只是薄 API shim。
> "Server(per BackendObject) 单独 target" 落地为：**每个 BackendObject 一个独立插件 `.so` target**。

---

## 2. 术语表

| 术语 | 定义 |
|---|---|
| **Client** | thin API shim，位于应用进程；只做 trampoline、传输、共享内存管理，无 GL 状态 |
| **FullServer** | 通用 server 进程，含 MG_Impl + MG_State + BFA Host + plugin loader + transport；不含任何 backend |
| **BackendObject** | 一个具体后端插件（DirectGLES / DirectVulkan），独立 `.so` 产物 |
| **BigServer** | FullServer + 运行时加载的一个 BackendObject 插件（运行时组合，不是额外 target） |
| **BFA** | Backend Frontend API：FullServer ⇄ BackendObject 的 strict C ABI 插件契约 |
| **UtilRuntime / RuntimeApi** | `MobileGL_UtilRuntime.so` 及其 C ABI 服务接口（MG_Util 全量 C 化） |
| **Session** | 一个 EGLContext 对应的 GL 上下文（current state / bindings / error queue） |
| **SharedGroup** | 一组通过 `shareCtx` 共享 GL 对象的 context 组 |
| **Display** | 一个 EGLDisplay |
| **ObjectHandle** | 不透明 `uint64_t`，代表一个 GL 对象；共享对象 per-SharedGroup，上下文私有对象 per-Session |
| **Generation** | 对象/状态的版本号，用于 backend 缓存失效 |

---

## 3. 总体架构

### 3.1 逻辑分层图

```text
[应用进程]
  MobileGL_Client (SHARED)
   ├─ 导出 EGL / GL / GLX 符号
   ├─ trampoline → FlatBuffer 编码
   ├─ transport: socket + shm client
   └─ 无 frontend 状态、无 backend
        │
        │ 外部 C/S 协议（FlatBuffers + 共享内存）
        ▼
[BigServer 进程]
  FullServer (EXE)
   ├─ MG_Impl + MG_State（完整 frontend，per-session 化）
   ├─ BFA Host 实现（BackendHostVTable）
   ├─ plugin loader（dlopen BackendObject.so + manifest 校验）
   ├─ transport server + session 路由
   └─ UtilRuntime Host（加载 MobileGL_UtilRuntime.so，注入 RuntimeApi*）
        │
        │ BFA（strict C ABI，handle + session + generation）
        ▼
  BackendObject_DirectGLES.so / BackendObject_DirectVulkan.so
   ├─ 各自原生 GPU 状态 / twin registry / device / session state
   └─ 通过注入的 RuntimeApi* 调用 MG_Util 服务
```

### 3.2 两条独立边界

| 边界 | 所在 | 序列化？ | 形态 |
|---|---|---|---|
| Client ⇄ BigServer | 外部 | 是（FlatBuffers） | `Message` + union + shm payload |
| FullServer ⇄ BackendObject | 内部插件 | 否 | strict C ABI 函数指针 + handle/session |
| FullServer/Backend ⇄ UtilRuntime | 内部共享服务 | 否（对外是 C ABI） | 根 `MobileGLUtilApi` + 分域 sub-vtable |

### 3.3 进程/线程模型

- Client 进程与 BigServer 进程**同一设备**（第一版 headless/pbuffer）。
- 一条控制连接承载多个 Session；每条消息带 `clientThreadId`。
- BigServer 按 `(sessionId, clientThreadId)` 维护 current context / error queue / 命令流，严格 FIFO。
- Backend 同一 session 的命令串行；不同 session/thread 可并发。
- 异步命令流 + 显式同步点（`glFlush` / `glFinish` / `eglSwapBuffers` / 查询命令）。

---

## 4. 核心决策记录

| # | 决策 |
|---|---|
| D1 | C/S 切点：公开 EGL/GL API 层；Client thin，FullServer 完整 |
| D2 | BigServer = FullServer + 一个 BackendObject.so |
| D3 | BackendObject 每类型独立 `.so` target，独立编译/版本化/发布 |
| D4 | BackendObject .so 即"Server(per BackendObject) 单独 target"的落地；BigServer 为运行时组合 |
| D5 | BFA = strict C ABI；禁止 C++ 类型/异常/RTTI 跨边界 |
| D6 | Backend 缓存/资源全部搬进 backend 内部 handle-keyed map |
| D7 | 外部协议 = FlatBuffers；统一 `Message` + `Payload` union |
| D8 | 外部协议全量 API codegen；API 清单从现有源码提取 |
| D9 | 传输 = 本地 socket + 共享内存；Transport 可抽象 |
| D10 | 共享内存由 FullServer 创建管理 arena；Client mmap |
| D11 | 单连接多 Session；每条消息带 `clientThreadId` |
| D12 | 执行模型 = 语义批处理+同步查询，实现异步流水线 |
| D13 | 同步点标准语义：`glFlush`=flush ring；`glFinish`=flush+barrier；`eglSwapBuffers`=flush+present ack |
| D14 | Handle = 不透明 uint64；共享对象 per-SharedGroup，上下文私有对象 per-Session |
| D15 | v1 支持多 Session、完整 GL share group、多 EGLDisplay |
| D16 | MG_Util 全量 C ABI 化 → `MobileGL_UtilRuntime.so`，FullServer 与 Backend 共用 |
| D17 | RuntimeApi = 根 `MobileGLUtilApi` + 分域 sub-vtable；FullServer Host 注入 |
| D18 | 最终只保留 C/S 产物，不保留 monolith（迁移期用现有产物做对照） |
| D19 | eglGetProcAddress v1：仅已知 codegen API；未知返回 NULL |
| D20 | GL/EGL 对象 name 与 ObjectHandle 解耦；GL name + kind + group/session 映射到 handle |

---

## 5. Handle / Session / Generation 统一语义

### 5.1 ID 层级

```text
DisplayId
  └─ SharedGroupId
       └─ SessionId
            └─ ObjectHandle（共享对象在 SharedGroup 级；上下文私有对象在 Session 级）
```

### 5.2 归属规则

| 状态 | 归属 |
|---|---|
| EGL display / config / surface | DisplayId |
| GL 共享对象表（buffer/texture/sampler/VAO/program/shader/pipeline/FBO/RBO/TF） | SharedGroupId |
| 上下文私有对象（default FBO/VAO/texture 0、query、sync 等） | SessionId |
| current bindings / render state / viewport / error queue | SessionId |
| backend native 共享资源（VkBuffer/VkImage/pipeline/sampler） | SharedGroupId |
| command pool / descriptor pool / frame tracking / deferred release | SessionId |
| 硬件设备 / loader | DisplayId |

### 5.3 Handle 模型

```text
ObjectHandle = uint64_t
  - FullServer 分配，进程生命周期内不复用
  - 共享对象：同一 handle 在 SharedGroup 内所有 Session 可见
  - 上下文私有对象：仅所属 Session 有效
  - 与 GL name 解耦
  - 映射：
      共享对象     (SharedGroupId, ObjectKind, GLname)  → handle
      上下文私有  (SessionId, ObjectKind, name/slot)   → handle
```

设计理由：

- 天然支持 name 0 默认对象、proxy texture、texture view、reserved-but-not-materialized 对象；
- 避免 `SharedPtr<T>` / 裸指针跨插件；
- 未来 share group 语义扩展不需要改接口。

### 5.4 Generation 机制

| frontend 现状 | BFA 字段 |
|---|---|
| `GetShapeVersion / GetContentVersion / GetTextureParamsVersion` | `TextureInfo.generation / contentGeneration / paramsGeneration` |
| `GetTextureBindGeneration / GetSamplingResolutionGeneration` | `TextureInfo.bindGeneration / samplingGeneration` |
| `GetRenderStateParametersVersion / GetPipelineStateVersion` | `RenderState.version / pipelineVersion` |
| `GetLinkVersion / GetBackendStateVersion / GetUniformWriteSetVersion / GetUBOContentVersion / GetBlockBindingVersion` | `ProgramInfo.*Generation` |
| `GetConfigVersion / GetAttributeVersion` | `VertexArrayInfo.generation / attribs[].generation` |
| `GetObjectVersion / GetAllFramebufferAttachmentVersions` | `FramebufferInfo.generation` |
| `SamplerObject::GetVersion` | `SamplerInfo.generation` |
| `GetTransformFeedbackGeneration` | `TransformFeedbackInfo.generation` |
| `GetTextureContextId` | `SessionId` 来源 |

Backend 缓存 key：

```text
{ SharedGroupId, ObjectHandle, objectGeneration }
```

### 5.5 当前代码缺口

| 现状 | 缺口 |
|---|---|
| `pGLContext` 进程级单例（`GLState/Core.cpp:20` 唯一创建点） | 必须拆成 `GLSharedGroup` + `GLContextSession` |
| `shareCtx` 只存于 `ContextObject.SharedContext`（`EGLState/Core.h:163`），GLState 从未使用 | 完整 share group 是**新增实现** |
| `pVulkanRenderer` 进程级单体（`DirectVulkan/DirectVulkan.cpp:25`） | multi-display 下需要 per-device/per-display |
| `EGLState` 已支持多 display/context/surface（`Core.h:254-265`） | EGL 层已在，GLState 层缺失 |
| `GetProcAddress.cpp` 已有集中式 API 注册表 | 可作为 codegen 输入源 |

---

## 6. 外部 C/S 协议

### 6.1 Transport 抽象

```c
typedef struct MobileGLTransportOps {
    uint32_t structSize;
    bool (*Start)(MobileGLTransport* t, const MobileGLTransportConfig* cfg);
    void (*Shutdown)(MobileGLTransport* t);
    bool (*SubmitCommands)(MobileGLTransport* t, MobileGLCommandBatch* batch);
    bool (*WaitResponses)(MobileGLTransport* t, MobileGLResponseQueue* out, uint32_t timeoutMs);
    bool (*OpenSharedMemory)(MobileGLTransport* t, MobileGLShmHandle* out);
} MobileGLTransportOps;
```

实现计划：

| 实现 | 用途 |
|---|---|
| `LocalSocketShmTransport`（v1） | Unix domain socket / Windows Named Pipe + memfd / shm arena |
| `InProcessTransport`（测试） | 同进程直调，无序列化 |
| `RemoteTcpTransport`（后续） | 跨设备/网络 |
| `AndroidBinderTransport`（后续） | Android Surface/服务场景 |

### 6.2 FlatBuffers 消息

统一 root `Message` + union `Payload`：

```fbs
namespace MobileGL.Protocol;

table ShmRegion {
    offset: ulong;
    size: ulong;
}

union Payload {
    EglInitialize,
    EglCreateContext,
    GlDrawArrays,
    GlReadPixels,
    GlMapBufferRange,
    GlGenTextures,
    // ... 由 API 清单 codegen 生成
}

table Command {
    sessionId: ulong;
    clientThreadId: uint;
    seq: uint;
    flags: uint;          // BATCH / SYNC / PAYLOAD...
    opcode: ushort;
    payload: Payload;
    payloadShm: ShmRegion;
}

table Message {
    version: uint;
    command: Command;
}

root_type Message;
```

响应：

```fbs
table Response {
    seq: uint;
    status: uint;        // 0 = OK, 1 = GL/EGL error, 2 = protocol error
    errorCode: int;
    ret: ReturnValue;    // union
    retShm: ShmRegion;   // 大结果（readback/数组/字符串）
}
```

### 6.3 API Codegen 管线

输入：从现有源码提取的规范化 API 清单。

```text
[输入] API 签名清单（name / signature / category / alias）
   ├── 1. protocol.fbs 生成（统一 Message + union payload）
   ├── 2. opcode 表生成（name→opcode，alias→core opcode）
   ├── 3. client trampoline 生成（FlatBuffer + shm）
   ├── 4. server dispatch 生成（解析 → 调用 MG_Impl）
   └── 5. 函数分类表生成
```

函数分类：

| 分类 | 例子 | 生成策略 |
|---|---|---|
| VOID 命令 | `glDrawArrays` / `glBindTexture` | 无 response（错误除外） |
| GETTER | `glGetIntegerv` | request + response |
| POINTER_RETURN | `glGetString` / `glMapBufferRange` | response 返回 shm 区域/稳定指针 |
| ARRAY_OUT | `glGenTextures` | response 带 names |
| MAP / UNMAP | `glMapBufferRange` / `glUnmapBuffer` | 专用数据流 + client 映射表 |

Alias 规则：ARB/EXT 别名映射到同一 core opcode，消息不区分。

### 6.4 线程亲和

- 每条消息带 `clientThreadId`（client 库分配，`thread_local`）。
- Server 维护：
  ```text
  map<{ sessionId, clientThreadId }, CurrentContextState>
  map<{ sessionId, clientThreadId }, ErrorQueue>
  ```
- `eglMakeCurrent` 是控制消息，写入该映射。
- 同一 `(sessionId, clientThreadId)` 流严格 FIFO。
- 一个 EGLContext 同一时刻只能有一个 owner thread（复用现有 `m_contextOwners` 语义）。

### 6.5 执行模型（异步流水线 + 同步语义）

| 语义基准 | 实现 |
|---|---|
| 命令按序执行 | 每条流 FIFO |
| 查询看到之前全部状态 | 查询命令是 barrier |
| 无返回值命令不逐条等待 | 异步 command ring |
| 同步点 | `glFlush` / `glFinish` / `eglSwapBuffers` |

```text
CommandBatch（异步提交）
  ├── 无响应命令：glDraw / glBind / glTexParameter...
  └── 查询/barrier 命令：带 seq，等 response
ResponseQueue（同步等待）
```

`glFlush` / `glFinish` / `eglSwapBuffers` 语义（标准）：

- `glFlush`：flush command ring（不等待执行完成）；
- `glFinish`：flush + barrier ack（等待之前命令全部完成）；
- `eglSwapBuffers`：flush + present 完成 ack。

### 6.6 共享内存 Arena

- 由 **FullServer 创建和管理**大 arena，通过 fd/句柄传给 Client mmap；
- Server 分配槽位、引用计数、复用；
- Client 只读写/释放；
- FlatBuffer 本体不承载大 blob，只携带 `ShmRegion { offset, size }`。

### 6.7 数据通路

| 场景 | 通路 |
|---|---|
| `glBufferData` / `glTexImage2D` / shader source | Client 写 shm payload → server 读取 |
| `glReadPixels` / `glGetTexImage` / `glGetIntegerv` | Server 写 shm → Client 读取 |
| `glMapBufferRange` | Server 分配/复用 shm/GPU 映射区域 → response 带回区域 → Client 返回对齐指针 |
| `glGetString` | Server 返回稳定字符串 → Client thread_local 缓存并返回指针 |
| Persistent map | 复用 server 的 host-visible 映射，Client 直接 mmap 同一区域 |

### 6.8 Client Token 模型

- Client 不维护 GL 状态；
- EGL/GL 对象 token 由 Server 分配，Client 只透传；
- `eglGetDisplay(native)` 作为远程命令，Server 返回 display token；
- `glGenTextures` 等由 Server 分配 GL name 并返回；
- `eglGetProcAddress`：仅已知 API 返回 client trampoline 指针，未知返回 NULL。

### 6.9 会话生命周期

```text
eglGetDisplay ──> displayToken
eglInitialize ──> 初始化 display
eglBindAPI / eglChooseConfig / eglCreateContext
  ├─ 无 shareCtx → 新建 SharedGroupId
  └─ 有 shareCtx → 加入其 SharedGroupId
eglMakeCurrent ──> 写 (sessionId, clientThreadId) → current
glXxx / eglSwapBuffers / glFinish ... 
eglDestroyContext / eglTerminate ──> 清理 session/group/display
```

---

## 7. BFA（Backend Frontend API）

### 7.1 定位与原则

- 边界：FullServer ⇄ BackendObject.so；
- 同进程，无序列化，直接函数指针；
- 严格 C ABI；
- 不暴露任何 frontend C++ 类型；
- 后端 state 内部化：`device` / `sharedgroup` / `session` 三层。

### 7.2 Plugin Manifest

```c
#define MOBILEGL_BFA_ABI_MAJOR 1
#define MOBILEGL_BFA_ABI_MINOR 0

typedef struct MobileGLBackendManifest {
    uint32_t structSize;
    uint32_t abiMajor;
    uint32_t abiMinor;
    uint32_t backendType;          // 1=DirectGLES, 2=DirectVulkan
    const char* name;
    const char* version;
    uint32_t (*GetAbiVersion)(void);
    MobileGLBackend* (*Create)(const MobileGLBackendHost* host,
                               const MobileGLUtilApi* utilApi,
                               const MobileGLBackendInitInfo* initInfo);
} MobileGLBackendManifest;

extern "C" const MobileGLBackendManifest* mobilegl_backend_manifest(void);
```

### 7.3 BackendObjectVTable（Server → Backend）

```c
typedef struct MobileGLBackendVTable {
    uint32_t structSize;
    uint32_t apiVersion;

    // 生命周期
    bool (*Initialize)(MobileGLBackend* self, const MobileGLBackendInitInfo* info);
    void (*Shutdown)(MobileGLBackend* self);

    // Session / Display / SharedGroup
    bool (*OnDisplayCreated)(MobileGLBackend* self, MobileGLDisplayId display, ...);
    void (*OnDisplayDestroyed)(MobileGLBackend* self, MobileGLDisplayId display);
    bool (*OnSharedGroupCreated)(MobileGLBackend* self, MobileGLSharedGroupId group, ...);
    void (*OnSharedGroupDestroyed)(MobileGLBackend* self, MobileGLSharedGroupId group);
    bool (*OnSessionCreated)(MobileGLBackend* self, MobileGLSessionId session,
                             const MobileGLSessionCreateInfo* info);
    void (*OnSessionDestroyed)(MobileGLBackend* self, MobileGLSessionId session);

    // 对象生命周期
    void (*OnObjectCreated)(MobileGLBackend* self, MobileGLObjectKind kind,
                            MobileGLBackendHandle handle);
    void (*OnObjectDestroyed)(MobileGLBackend* self, MobileGLObjectKind kind,
                              MobileGLBackendHandle handle);

    // EGL / Surface
    bool (*InitializeEGLDisplay)(...);
    bool (*CreateWindowSurface)(...);
    bool (*CreatePbufferSurface)(...);
    bool (*MakeEGLCurrent)(...);
    bool (*SwapBuffers)(...);
    void (*SetSwapInterval)(...);
    void (*ReleaseEGLResources)(...);

    // GL 命令（原 GlobalBackendFunctionsTable + session/handle）
    void (*DrawArrays)(MobileGLBackend* self, MobileGLSessionId session,
                       uint32_t mode, int32_t first, int32_t count);
    void (*Clear)(MobileGLBackend* self, MobileGLSessionId session, uint32_t mask);
    void (*ClearNamedFramebufferfv)(MobileGLBackend* self, MobileGLSessionId session,
                                    MobileGLBackendHandle framebuffer, ...);
    void (*ReadPixels)(...);
    // ... 其余 GL 命令由现有表机械迁移

    // 资源操作（原 BufferBackendOps 等）
    void (*BufferRespecify)(...);
    void (*BufferSubData)(...);
    void (*BufferFlushMappedRange)(...);
    void* (*BufferAcquirePersistentMap)(...);
    void (*BufferReadbackFromGpu)(...);

    // Query / Sync / Timer
    // ...

    // Capabilities
    const MobileGLRendererInfo* (*GetRendererInfo)(MobileGLBackend* self);
    const MobileGLDynamicParameters* (*GetDynamicParameters)(MobileGLBackend* self);
    bool (*GetFormatCapabilities)(MobileGLBackend* self, MobileGLFormatCapabilityCache* out);
    bool (*GetFormatSampleCounts)(MobileGLBackend* self, ...);
} MobileGLBackendVTable;
```

### 7.4 BackendHostVTable（Backend → Server）

```c
typedef struct MobileGLBackendHost {
    uint32_t structSize;
    uint32_t apiVersion;

    // Session 状态查询
    bool (*GetSessionDrawState)(MobileGLSessionId session, MobileGLSessionDrawState* out);
    bool (*GetRenderState)(MobileGLSessionId session, uint32_t desiredVersion,
                           MobileGLRenderState* out);
    bool (*GetPixelStore)(MobileGLSessionId session, int isUnpack,
                          MobileGLPixelStore* out);

    // 对象信息（带 generation）
    bool (*GetBufferInfo)(MobileGLBackendHandle h, MobileGLBufferInfo* out);
    bool (*GetTextureInfo)(MobileGLBackendHandle h, MobileGLTextureInfo* out);
    bool (*GetTextureMipInfo)(MobileGLBackendHandle h, uint32_t level,
                              MobileGLTextureMipInfo* out);
    bool (*GetFramebufferInfo)(MobileGLBackendHandle h, MobileGLFramebufferInfo* out);
    bool (*GetVertexArrayInfo)(MobileGLBackendHandle h, MobileGLVertexArrayInfo* out);
    bool (*GetSamplerInfo)(MobileGLBackendHandle h, MobileGLSamplerInfo* out);
    bool (*GetProgramInfo)(MobileGLBackendHandle h, MobileGLProgramInfo* out);
    bool (*GetProgramModules)(MobileGLBackendHandle h, MobileGLProgramStageModule* out,
                              uint32_t* inOutCount);
    bool (*GetRenderbufferInfo)(MobileGLBackendHandle h, MobileGLRenderbufferInfo* out);

    // 大块数据
    bool (*GetBufferShadow)(MobileGLSessionId session, MobileGLBackendHandle h,
                            const void** data, uint64_t* size);
    bool (*ReadBufferRange)(MobileGLSessionId session, MobileGLBackendHandle h,
                            uint64_t offset, uint64_t size, void* dst);
    bool (*GetTextureMipData)(MobileGLBackendHandle h, uint32_t level, uint32_t layer,
                              const void** data, uint64_t* size);

    // 错误 / 日志
    void (*RecordError)(MobileGLSessionId session, uint32_t glErrorCode, const char* message);
    void (*Log)(int level, const char* message);

    // 能力变化
    void (*CapabilityChanged)(MobileGLBackend* backend);
} MobileGLBackendHost;
```

### 7.5 Snapshot 结构与 Generation

- `MobileGLSessionDrawState`：当前 program/pipeline/VAO/FBO/buffer/texture/sampler/image 绑定 + draw/renderState 版本；
- `MobileGLBufferInfo` / `MobileGLTextureInfo` / `MobileGLFramebufferInfo` / `MobileGLVertexArrayInfo` / `MobileGLSamplerInfo` / `MobileGLProgramInfo` / `MobileGLRenderbufferInfo`：全部为 POD + `structSize` + generation 字段；
- `MobileGLProgramStageModule`：每 stage SPIR-V 指针 + 大小 + entryPoint + generation；
- Capabilities：
  - `RendererInfo`：POD + plugin-owned 字符串指针 + 扩展数组；
  - `DynamicBackendParameters`：POD 按值/指针；
  - `FormatCapabilityCache`：固定掩码 POD；可变 SampleCounts 用两段式查询。

### 7.6 数据通路（BFA 内部）

| 数据 | 通路 |
|---|---|
| Buffer shadow | `Host_GetBufferShadow`（调用期有效，必须 copy） |
| Buffer range | `Host_ReadBufferRange` |
| Texture mip | `Host_GetTextureMipInfo` + `Host_GetTextureMipData` |
| Program 模块 | `Host_GetProgramModules`（SPIR-V 指针） |
| Persistent map | `BackendVTable::BufferAcquirePersistentMap` 返回 GPU 映射指针 |

v1 不做 dirty-rect 增量：`generation` 变了就整级/整对象重传。

### 7.7 Backend 内部改造

- `BackendDevice`（per Display）：VkInstance/VkDevice/VMA allocator / GLES loader；
- `BackendSharedGroupState`：共享对象 native 资源 + twin registry；
- `BackendSessionState`：command pool / descriptor pool / current bindings / deferred release；
- 删除 frontend 对象上的 backend 挂载点：
  - `PipeResource::SetBackend/GetBackend`（`PipeResource.h:131-133`）；
  - `SetBackendHashMemo / SetBackendStateMemo / SetBackendAuxMemo`；
  - `StateBackendObjectRegistry` key 从 `SharedPtr<T>` 改为 `MobileGLBackendHandle`。

### 7.8 线程 / 生命周期 / 错误

- 同一 session 的 backend 调用串行；
- 不同 session 并发，Host 查询线程安全；
- v1 禁止 backend 自建线程回调 Host；
- backend 错误通过 `Host_RecordError` 上报，错误队列仍由 frontend 持有；
- `OnObjectDestroyed` 显式通知，替代当前基于 WeakPtr 的隐式 GC。

### 7.9 ABI / 版本策略

- 所有 table 有 `structSize` + `apiVersion`；
- 尾部加字段 = minor 兼容；
- 修改语义/删除 = major；
- 加载失败不得抛异常，返回结构化错误。

### 7.10 与 UtilRuntime 集成

```c
mobilegl_backend_create(
    const MobileGLBackendHost* host,
    const MobileGLUtilApi* utilApi,
    const MobileGLBackendInitInfo* initInfo);
```

- FullServer 先 dlopen `MobileGL_UtilRuntime.so`；
- 再 dlopen `BackendObject_xxx.so`；
- 把 `RuntimeApi*` 注入插件；
- 插件不链接 runtime.so，不包含 MG_Util 源码。

---

## 8. MobileGL_UtilRuntime

### 8.1 定位

- `MobileGL_UtilRuntime.so`：独立版本化的共享库；
- 包含 MG_Util 全部 backend/frontend 共用服务；
- FullServer 直接通过 C++ adapter 调用（同构建）；
- Backend plugin 通过注入的 C ABI `RuntimeApi*` 调用。

### 8.2 C ABI 结构（分域 sub-vtable）

```c
typedef struct MobileGLUtilApi {
    uint32_t structSize;
    uint32_t abiMajor;
    uint32_t abiMinor;

    const MobileGLLoaderApi*      loaders;
    const MobileGLShaderApi*      shader;
    const MobileGLConvertersApi*  converters;
    const MobileGLMetricsApi*     metrics;
    const MobileGLMathApi*        math;
    const MobileGLTextureApi*     texture;
    const MobileGLAsyncApi*       async;
} MobileGLUtilApi;
```

域划分：

| 域 | 内容 |
|---|---|
| `LoaderApi` | OpenGL / Vulkan 动态库加载、函数表获取、版本查询 |
| `ShaderApi` | ShaderCompiler / SpvcSession / CompileEnv / TranslationCache（不透明 handle） |
| `ConvertersApi` | GL↔MG↔Vk↔Str 全部枚举转换 |
| `MetricsApi` | Texture / Buffer metrics |
| `MathApi` | HalfFloat / VectorTypes / FixedPoint |
| `TextureApi` | TextureFormatProcessor / PixelStoreProcessor |
| `AsyncApi` | ShaderCompilePool（不透明 handle） |

### 8.3 关键设计

- `CompileEnv`、`SpvcSession`、`TranslationCache`、`ShaderCompilePool` 在 C 边界上改为 **不透明 handle**；
- FullServer 内部提供 C++ adapter（由 codegen/手工生成）把现有 C++ 调用点转为 RuntimeApi；
- 每个 sub-vtable 独立演进（尾部加字段）；
- RuntimeApi 与 BFA 一样有版本握手。

---

## 9. 构建目标与 CMake 规划

### 9.1 Target 图

```text
MobileGL_Protocol       (static/INTERFACE)  ← FlatBuffers 生成代码 + 消息类型
MobileGL_Client         (SHARED)            ← trampoline + transport + shm client
FullServer              (EXE)               ← MG_Impl + MG_State + BFA Host + plugin loader
MobileGL_UtilRuntime    (SHARED)            ← MG_Util 全量 C 化服务
BackendObject_DirectGLES   (MODULE/SHARED)  ← 后端插件 target
BackendObject_DirectVulkan (MODULE/SHARED)  ← 后端插件 target
MobileGL_BFA            (INTERFACE)         ← bfa.h
mgruntime_api.h         (INTERFACE)         ← MobileGLUtilApi + sub-vtables
MG_Test / MG_Benchmark / MG_IntegrationTest ← 挂到各 target
```

### 9.2 CMake 规划

```cmake
function(mobilegl_add_backend_plugin name)
    add_library(MobileGL_Backend_${name} MODULE
        MobileGL/MG_Backend/${name}/...
        ...)
    target_link_libraries(MobileGL_Backend_${name} PRIVATE MobileGL_BFA)
    # 不链接 UtilRuntime；运行时由 FullServer 注入
endfunction()

function(mobilegl_compile_fbs schema)
    add_custom_command(... flatc --gen-object-api -o ... ${schema} ...)
endfunction()
```

### 9.3 源码归属建议

| 源码 | Target |
|---|---|
| `MG_Impl/*`（实现，不含导出） | FullServer |
| `MG_State/*` | FullServer |
| `MG_Util/*` | 拆分为：FullServer 端 C++ adapter 使用 + UtilRuntime 内实现 |
| `MG_Backend/BackendObject.* / DirectGLES / DirectVulkan` | 各自 BackendObject_xxx.so |
| `Exporting/Definitions.cpp` | 转成 Client trampoline 生成输入，不再直接编入 FullServer |
| 新增 Client 代码 | MobileGL_Client |
| 新增 protocol/runtime api 头 | MobileGL_Protocol / INTERFACE |

---

## 10. 实施阶段

### Phase 0 — 契约定稿（纯文档/头文件）

- [x] 统一 Handle/Session/Generation 语义文档（本文件 §5 → `docs/CS_Refactor/HandleSessionGeneration.md`）
- [x] 从源码提取 API 签名清单 → `protocol.fbs` 草案（`MobileGL/Protocol/protocol.fbs`，完整清单由 `scripts/extract_api_manifest.py` 生成）
- [x] `bfa.h` 定稿（`MobileGL/Protocol/bfa.h`）
- [x] `mgruntime_api.h` 定稿（分域 sub-vtable，`MobileGL/Protocol/mgruntime_api.h`）
- [x] Transport API 定义（`MobileGL/Protocol/transport.h`）

### Phase 1 — 构建拆分

- [ ] CMake target 拆分（Client / FullServer / UtilRuntime / BackendObject_*.so / protocol / BFA）
- [ ] `MobileGL_UtilRuntime` C ABI 适配层（codegen/adapter）
- [ ] 迁移期保留现有产物做对照；最终无 monolith

### Phase 2 — FullServer 状态模型重构

- [ ] `pGLContext` 单例 → `GLSharedGroup` + `GLContextSession`
- [ ] EGLState `m_threadCurrents` 改为 `(sessionId, clientThreadId)`
- [ ] Handle Registry：多 Display / 多 SharedGroup / 多 Session
- [ ] 完整 share group 语义（对象名共享、删除传播、生命周期）

### Phase 3 — BFA 落地

- [ ] Backend 内部 device / sharedgroup / session 化
- [ ] backend 资源/memo 从 frontend 对象搬进 handle-keyed map
- [ ] `StateBackendObjectRegistry` key 改为 handle
- [ ] backends 通过 `BackendHost` 查询前状态

### Phase 4 — 外部协议落地

- [ ] FlatBuffers codegen（trampoline / dispatch / opcode / 分类表）
- [ ] Transport v1 = LocalSocketShm + Server-managed arena
- [ ] 异步命令流 + 同步查询 / barrier
- [ ] map/unmap/readback/字符串返回数据通路
- [ ] Token 透传模型 + 会话生命周期

### Phase 5 — Plugin 化与集成

- [ ] UtilRuntime Host-injected
- [ ] Backend plugin dlopen + manifest 版本协商
- [ ] BigServer 全链路：Client → FullServer → UtilRuntime → Backend.so → GPU
- [ ] headless/pbuffer 闭环 E2E

### Phase 6 — 正确性 / 性能 / 平台

- [ ] 多 Session / 多 Display / share group 回归
- [ ] 命令批处理、零拷贝、benchmark
- [ ] 平台 Surface：X11 → Win32 → Android Binder（后置）

---

## 11. 验收标准

- [ ] `BackendObject_*.so` 可在不含 MG_Impl / MG_State 源码的情况下独立编译/版本化
- [ ] `FullServer` 不链接任何 backend
- [ ] BFA 与 RuntimeApi 边界无任何 C++ 类型（`SharedPtr` / `GLContext&` / `BufferObject&` 不出现在契约头之外）
- [ ] FullServer 支持多 Display / 多 Session / 完整 share group
- [ ] Client 无前端状态，仅 token 透传
- [ ] FlatBuffers 统一 Message + union 覆盖全量 API（含 alias 映射）
- [ ] 同一 `(sessionId, clientThreadId)` 流内命令严格 FIFO
- [ ] `glFlush / glFinish / eglSwapBuffers` 语义符合 GL
- [ ] 共享内存由 Server 管理，readback / map 走 shm
- [ ] 现有测试在 plugin 模式下全绿（headless 范围）
- [ ] 性能：无返回值命令无逐条 syscall，只在查询/同步点等响应

---

## 12. 风险与对策

| 风险 | 说明 | 对策 |
|---|---|---|
| GLContext 拆分 + share group | 当前单例 + shareCtx 未实现，是最重重构 | 拆分 `GLSharedGroup` / `GLContextSession`，先实现对象表共享，再补跨 context 删除语义 |
| multi-display 下 backend 全局状态 | `pVulkanRenderer` 等需 per-device | Backend Device/SharedGroup/Session 三层状态 |
| 全量 MG_Util C ABI 化 | C ABI 覆盖广 | 分域 sub-vtable + codegen + 每域独立演进 |
| FlatBuffers codegen 依赖 | flatc 进构建链 | API 清单单源；生成物纳入 CI 校验 |
| 异步协议正确性 | 顺序/错误/响应匹配 | seq + 每流 FIFO + contract tests |
| 测试迁移 | 现有测试直连 `gBackendFunctionsTable` / `pGLContext` | mock BFA / Runtime / transport |
| 共享运行时 ABI | 插件与 runtime 独立版本 | Host 注入 RuntimeApi；插件不链接 runtime |

---

## 13. 开放事项（后续细化）

- [ ] BFA 每个 `MobileGLXxxInfo` 的最终字段（按 access matrix 补齐）
- [ ] RuntimeApi 各域的具体函数清单（从 backend/frontend 的 MG_Util 使用点提取）
- [ ] `protocol.fbs` 具体 schema（依赖 API 清单）
- [ ] Transport 的 fd 传递机制（SCM_RIGHTS / Windows 句柄）
- [ ] 现有测试迁移顺序
- [ ] Android Surface 跨进程传递（后置）

---

## 14. 附录：关键代码证据

| 证据 | 文件 |
|---|---|
| 单一库构建 | `CMakeLists.txt:300-416, 483-582` |
| BackendObject 抽象 | `MobileGL/MG_Backend/BackendObject.h:523-599` |
| GlobalBackendFunctionsTable（含 SharedPtr 参数） | `MobileGL/MG_Backend/BackendObject.h:117-285` |
| Runtime 选后端 switch | `MobileGL/MG_Backend/Init.cpp:48-70` |
| pGLContext 单例 | `MobileGL/MG_State/GLState/Core.cpp:20, 1487` |
| EGL 编码句柄 | `MobileGL/MG_State/EGLState/Core.h:212-215, 254-265` |
| shareCtx 仅存储未使用 | `MobileGL/MG_State/EGLState/Core.h:163`、`Core.cpp:632-640` |
| StateBackendObjectRegistry（SharedPtr key） | `MobileGL/MG_Backend/DirectGLES/Managers.h:270-390` |
| BufferBackendOps（C++ 引用参数） | `MobileGL/MG_State/GLState/BufferState/BufferObject.h:76-109` |
| BackendBufferResource 存于 frontend | `MobileGL/MG_State/GLState/BufferState/PipeResource.h:66-141` |
| VkBufferManager 全局 | `MobileGL/MG_Backend/DirectVulkan/Renderer/VkBufferManager.cpp:54-130` |
| VulkanRenderer 全局单体 | `MobileGL/MG_Backend/DirectVulkan/DirectVulkan.cpp:25` |
| CompileEnv（快照雏形） | `MobileGL/MG_Util/ShaderTranspiler/CompileEnv.h:46-164` |
| API 集中注册表 | `MobileGL/MG_Impl/GetProcAddress.cpp`（1409 行） |
| GL/EGL 导出定义 | `MobileGL/MG_Impl/GLImpl/Exporting/Definitions.cpp`、`EGLImpl/Exporting/Definitions.cpp` |
| 现有 API 测试打桩 | `MobileGL/MG_Test/.../ScopedBackendFunctionsOverride` |
