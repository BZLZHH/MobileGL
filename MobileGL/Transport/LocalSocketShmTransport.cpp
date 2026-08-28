// MobileGL - MobileGL/Transport/LocalSocketShmTransport.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "LocalSocketShmTransport.h"

#if defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__)
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

// The header declares MobileGLTransport as an opaque C type. This is the only
// C++ implementation TU that completes the type; all other code must keep
// using the opaque pointer from protocol/transport.h.
struct MobileGLTransport {
    void* Implementation = nullptr;
};

namespace MobileGL::Transport {
    namespace {
        class LocalSocketShmTransport {
        public:
            Bool Start(const MobileGLTransportConfig* cfg) {
                if (cfg == nullptr || cfg->kind != MobileGLTransportKindLocalSocketShm) {
                    m_lastError = "LocalSocketShm transport requires a LocalSocketShm config.";
                    return false;
                }

#ifdef _WIN32
                m_lastError = "LocalSocketShm transport is not implemented on Windows yet.";
                return false;
#else
                const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
                if (fd < 0) {
                    m_lastError = "socket(AF_UNIX) failed.";
                    return false;
                }

                sockaddr_un address{};
                address.sun_family = AF_UNIX;
                const char* endpoint = cfg->endpoint != nullptr ? cfg->endpoint : "/tmp/mobilegl.sock";
                const SizeT pathLen = strlen(endpoint);
                if (pathLen >= sizeof(address.sun_path)) {
                    close(fd);
                    m_lastError = "socket endpoint is too long.";
                    return false;
                }
                memcpy(address.sun_path, endpoint, pathLen + 1);

                if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
                    close(fd);
                    m_lastError = "connect() to FullServer failed.";
                    return false;
                }

                m_socketFd = fd;
                m_lastError.clear();
                return true;
#endif
            }

            void Shutdown() {
#ifdef _WIN32
                // no socket allocated; nothing to do
#else
                if (m_socketFd >= 0) {
                    close(m_socketFd);
                    m_socketFd = -1;
                }
#endif
                m_lastError.clear();
            }

            Bool SubmitCommands(MobileGLCommandBatch* batch) {
                (void)batch;
                // TODO(Phase 4): serialize FlatBuffer batch + shm handles over the socket.
                m_lastError = "SubmitCommands not implemented yet.";
                return false;
            }

            Bool WaitResponses(MobileGLResponseQueue* out, uint32_t timeoutMs) {
                (void)out;
                (void)timeoutMs;
                // TODO(Phase 4): multiplex seq responses over the socket.
                m_lastError = "WaitResponses not implemented yet.";
                return false;
            }

            Bool OpenSharedMemory(MobileGLShmHandle* out) {
                (void)out;
                // TODO(Phase 4): receive the server arena fd via SCM_RIGHTS.
                m_lastError = "OpenSharedMemory not implemented yet.";
                return false;
            }

            void ReleaseSharedMemory(MobileGLShmHandle* handle) {
                (void)handle;
            }

            const char* GetLastError() const {
                return m_lastError.c_str();
            }

            void* GetNativeHandle() { return this; }

        private:
            Int32 m_socketFd = -1;
            String m_lastError;
        };

        Bool StartImpl(MobileGLTransport* t, const MobileGLTransportConfig* cfg) {
            if (t == nullptr || t->Implementation == nullptr) return false;
            return static_cast<LocalSocketShmTransport*>(t->Implementation)->Start(cfg);
        }

        void ShutdownImpl(MobileGLTransport* t) {
            if (t != nullptr && t->Implementation != nullptr) {
                static_cast<LocalSocketShmTransport*>(t->Implementation)->Shutdown();
            }
        }

        Bool SubmitCommandsImpl(MobileGLTransport* t, MobileGLCommandBatch* batch) {
            if (t == nullptr || t->Implementation == nullptr) return false;
            return static_cast<LocalSocketShmTransport*>(t->Implementation)->SubmitCommands(batch);
        }

        Bool WaitResponsesImpl(MobileGLTransport* t, MobileGLResponseQueue* out, uint32_t timeoutMs) {
            if (t == nullptr || t->Implementation == nullptr) return false;
            return static_cast<LocalSocketShmTransport*>(t->Implementation)->WaitResponses(out, timeoutMs);
        }

        Bool OpenSharedMemoryImpl(MobileGLTransport* t, MobileGLShmHandle* out) {
            if (t == nullptr || t->Implementation == nullptr) return false;
            return static_cast<LocalSocketShmTransport*>(t->Implementation)->OpenSharedMemory(out);
        }

        void ReleaseSharedMemoryImpl(MobileGLTransport* t, MobileGLShmHandle* handle) {
            if (t != nullptr && t->Implementation != nullptr) {
                static_cast<LocalSocketShmTransport*>(t->Implementation)->ReleaseSharedMemory(handle);
            }
        }

        const char* GetLastErrorImpl(MobileGLTransport* t) {
            if (t == nullptr || t->Implementation == nullptr) return "null transport.";
            return static_cast<LocalSocketShmTransport*>(t->Implementation)->GetLastError();
        }

        const MobileGLTransportOps g_transportOps = {
            sizeof(MobileGLTransportOps),
            (MOBILEGL_TRANSPORT_ABI_MAJOR << 16) | MOBILEGL_TRANSPORT_ABI_MINOR,
            &StartImpl,
            &ShutdownImpl,
            &SubmitCommandsImpl,
            &WaitResponsesImpl,
            &OpenSharedMemoryImpl,
            &ReleaseSharedMemoryImpl,
            &GetLastErrorImpl
        };
    } // namespace

    MobileGLTransport* CreateLocalSocketShmTransport() {
        auto* transport = new MobileGLTransport();
        transport->Implementation = new LocalSocketShmTransport();
        return transport;
    }

    void DestroyLocalSocketShmTransport(MobileGLTransport* t) {
        if (t == nullptr) {
            return;
        }
        if (t->Implementation != nullptr) {
            auto* impl = static_cast<LocalSocketShmTransport*>(t->Implementation);
            impl->Shutdown();
            delete impl;
        }
        delete t;
    }

    const MobileGLTransportOps& GetLocalSocketShmTransportOps() {
        return g_transportOps;
    }
} // namespace MobileGL::Transport

// End of File
