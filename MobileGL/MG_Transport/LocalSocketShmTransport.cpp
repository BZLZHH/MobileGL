// MobileGL - MobileGL/MG_Transport/LocalSocketShmTransport.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "LocalSocketShmTransport.h"
#include "MG_Transport/TransportInternal.h"

#if defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__)
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace MobileGL::Transport {
    namespace {
        // Framing: 4-byte little-endian payload length followed by the payload.
        constexpr Uint32 kFrameHeaderSize = 4;

        Bool SendAll(Int32 fd, const void* data, SizeT size) {
            const auto* bytes = static_cast<const Uint8*>(data);
            SizeT sentTotal = 0;
            while (sentTotal < size) {
                const ssize_t sent = send(fd, bytes + sentTotal, size - sentTotal, 0);
                if (sent <= 0) {
                    return false;
                }
                sentTotal += static_cast<SizeT>(sent);
            }
            return true;
        }

        Bool RecvAll(Int32 fd, void* data, SizeT size) {
            auto* bytes = static_cast<Uint8*>(data);
            SizeT receivedTotal = 0;
            while (receivedTotal < size) {
                const ssize_t received = recv(fd, bytes + receivedTotal, size - receivedTotal, 0);
                if (received <= 0) {
                    return false;
                }
                receivedTotal += static_cast<SizeT>(received);
            }
            return true;
        }

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

            Bool Listen(const char* endpoint) {
#ifdef _WIN32
                m_lastError = "LocalSocketShm server is not implemented on Windows yet.";
                return false;
#else
                if (m_serverFd >= 0) {
                    m_lastError = "server is already listening.";
                    return false;
                }

                const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
                if (fd < 0) {
                    m_lastError = "socket(AF_UNIX) failed.";
                    return false;
                }

                sockaddr_un address{};
                address.sun_family = AF_UNIX;
                const SizeT pathLen = strlen(endpoint);
                if (pathLen >= sizeof(address.sun_path)) {
                    close(fd);
                    m_lastError = "socket endpoint is too long.";
                    return false;
                }
                memcpy(address.sun_path, endpoint, pathLen + 1);
                unlink(endpoint);

                if (bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
                    listen(fd, 1) != 0) {
                    close(fd);
                    m_lastError = "bind()/listen() failed.";
                    return false;
                }

                m_serverFd = fd;
                m_serverEndpoint = endpoint;
                m_lastError.clear();
                return true;
#endif
            }

            MobileGLTransport* Accept() {
#ifdef _WIN32
                m_lastError = "LocalSocketShm server is not implemented on Windows yet.";
                return nullptr;
#else
                if (m_serverFd < 0) {
                    m_lastError = "no listening socket.";
                    return nullptr;
                }
                const int fd = accept(m_serverFd, nullptr, nullptr);
                if (fd < 0) {
                    m_lastError = "accept() failed.";
                    return nullptr;
                }
                auto* clientImpl = new LocalSocketShmTransport();
                clientImpl->m_socketFd = fd;
                auto* transport = new MobileGLTransport();
                transport->Implementation = clientImpl;
                return transport;
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
                if (m_serverFd >= 0) {
                    close(m_serverFd);
                    m_serverFd = -1;
                    if (!m_serverEndpoint.empty()) {
                        unlink(m_serverEndpoint.c_str());
                    }
                }
#endif
                m_lastError.clear();
            }

            Bool SubmitCommands(MobileGLCommandBatch* batch) {
                if (batch == nullptr || batch->flatBufferData == nullptr || batch->flatBufferSize == 0) {
                    m_lastError = "SubmitCommands requires a non-empty batch.";
                    return false;
                }
                if (m_socketFd < 0) {
                    m_lastError = "no connected socket.";
                    return false;
                }
                const Uint32 payloadSize = batch->flatBufferSize;
                if (!SendAll(m_socketFd, &payloadSize, kFrameHeaderSize) ||
                    !SendAll(m_socketFd, batch->flatBufferData, payloadSize)) {
                    m_lastError = "send() failed.";
                    return false;
                }
                m_lastError.clear();
                return true;
            }

            Bool WaitResponses(MobileGLResponseQueue* out, uint32_t timeoutMs) {
                (void)timeoutMs;
                if (out == nullptr || m_socketFd < 0) {
                    m_lastError = "WaitResponses requires an output queue and a connected socket.";
                    return false;
                }

                Uint32 payloadSize = 0;
                if (!RecvAll(m_socketFd, &payloadSize, kFrameHeaderSize) || payloadSize > 64u * 1024u * 1024u) {
                    m_lastError = "recv() failed or bad frame size.";
                    return false;
                }

                m_received.resize(payloadSize);
                if (!RecvAll(m_socketFd, m_received.data(), payloadSize)) {
                    m_lastError = "recv() payload failed.";
                    return false;
                }

                *out = MobileGLResponseQueue{};
                out->structSize = sizeof(MobileGLResponseQueue);
                out->count = 1;
                out->flatBufferData = m_received.data();
                out->flatBufferSize = payloadSize;
                m_lastError.clear();
                return true;
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

        private:
            Int32 m_socketFd = -1;
            Int32 m_serverFd = -1;
            String m_serverEndpoint;
            Vector<Uint8> m_received;
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

    MobileGLTransport* CreateLocalSocketShmServer(const char* endpoint) {
        auto* transport = new MobileGLTransport();
        auto* impl = new LocalSocketShmTransport();
        transport->Implementation = impl;
        if (!impl->Listen(endpoint)) {
            delete impl;
            delete transport;
            return nullptr;
        }
        return transport;
    }

    MobileGLTransport* AcceptLocalSocketShmConnection(MobileGLTransport* server) {
        if (server == nullptr || server->Implementation == nullptr) {
            return nullptr;
        }
        return static_cast<LocalSocketShmTransport*>(server->Implementation)->Accept();
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
