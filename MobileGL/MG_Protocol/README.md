# MobileGL Protocol / BFA / RuntimeApi

Phase 0 C/S contract directory.

| File | Purpose |
|---|---|
| `protocol.fbs` | FlatBuffers schema: unified `Message` + `Payload` union, response types |
| `protocol_generated.fbs` | 由 `scripts/generate_protocol_fbs.py` 生成的 2750 个 `Gl*` payload 表（合并源） |
| `bfa.h` | Strict C ABI Backend Frontend API (FullServer ⇄ BackendObject) |
| `mgruntime_api.h` | C ABI `MobileGLUtilApi` 分域 sub-vtable (FullServer/Backend ⇄ UtilRuntime) |
| `transport.h` | Transport ops (socket + shm, in-process, future TCP/Binder) |
| `CMakeLists.txt` | `MobileGL_Protocol` / `MobileGL_BFA` / `MobileGL_RuntimeApi` INTERFACE targets |

## Codegen pipeline (Phase 4)

Input `api_manifest.txt` is generated from the existing source:

```text
[API 签名清单]
   ├── protocol.fbs 生成（统一 Message + union payload）
   ├── opcode 表生成（name→opcode，alias→core opcode）
   ├── client trampoline 生成（FlatBuffer + shm）
   ├── server dispatch 生成（解析 → 调用 MG_Impl）
   └── 函数分类表生成
```

当前 `protocol.fbs` 是 Phase 0 草案：包含种子 payload/response 形状，
`@INSERTION_POINT:PAYLOAD_CASES@` 指示完整 API 清单插入位置。

## ABI rules

- All tables start with `structSize`; appending fields is minor-compatible.
- No C++ type, exception, or RTTI crosses BFA / RuntimeApi.
- `MobileGLBackendHandle` is opaque to the backend; FullServer owns the mapping.
- Plugins never link `MobileGL_UtilRuntime.so`; the Host injects `MobileGLUtilApi*`.
