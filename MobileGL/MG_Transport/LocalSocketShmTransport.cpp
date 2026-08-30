// MobileGL - MobileGL/MG_Transport/LocalSocketShmTransport.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "LocalSocketShmTransport.h"
#include "MG_Transport/TransportInternal.h"
#include "MG_Protocol/gen/wire_generated.h"

#if defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__)
#include <poll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <unistd.h>
#endif

#if defined(__ANDROID__)
#include <linux/memfd.h>
#include <sys/syscall.h>
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
                m_maxShmArenaSize = cfg->maxShmArenaSize;
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
                for (Uint32 i = 0; i < batch->shmHandleCount; ++i) {
                    if (batch->shmHandles == nullptr ||
                        !SendShmHandle(&batch->shmHandles[i])) {
                        m_lastError = "send shm handle failed.";
                        return false;
                    }
                }
                m_lastError.clear();
                return true;
            }

            Bool WaitResponses(MobileGLResponseQueue* out, uint32_t timeoutMs) {
                if (out == nullptr || m_socketFd < 0) {
                    m_lastError = "WaitResponses requires an output queue and a connected socket.";
                    return false;
                }

                if (timeoutMs > 0) {
                    pollfd descriptor{};
                    descriptor.fd = m_socketFd;
                    descriptor.events = POLLIN;
                    const int pollResult = poll(&descriptor, 1, static_cast<int>(timeoutMs));
                    if (pollResult == 0) {
                        m_lastError = "WaitResponses: timed out.";
                        return false;
                    }
                    if (pollResult < 0 || (descriptor.revents & POLLIN) == 0) {
                        m_lastError = "WaitResponses: poll failed or socket is not readable.";
                        return false;
                    }
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

                m_receivedShmHandles.clear();
                Uint32 shmCount = 0;
                if (payloadSize >= sizeof(flatbuffers::uoffset_t) + sizeof(flatbuffers::voffset_t)) {
                    flatbuffers::uoffset_t rootOffset = 0;
                    memcpy(&rootOffset, m_received.data(), sizeof(rootOffset));
                    if (rootOffset + sizeof(flatbuffers::uoffset_t) <= payloadSize) {
                        const auto* response =
                            flatbuffers::GetRoot<MobileGL::Protocol::Wire::Response>(m_received.data());
                        flatbuffers::Verifier verifier(m_received.data(), payloadSize);
                        if (response != nullptr && response->Verify(verifier)) {
                            shmCount = response->ret_shm_count();
                        }
                    }
                }
                if (shmCount > 0) {
                    m_receivedShmHandles.resize(shmCount);
                    for (Uint32 i = 0; i < shmCount; ++i) {
                        MobileGLShmHandle handle{};
                        if (!RecvShmHandle(&handle)) {
                            m_receivedShmHandles.clear();
                            m_lastError = "response shm receive failed.";
                            return false;
                        }
                        m_receivedShmHandles[i] = handle;
                    }
                    out->shmHandleCount = shmCount;
                    out->shmHandles = m_receivedShmHandles.data();
                }
                m_lastError.clear();
                return true;
            }

            Bool OpenSharedMemory(MobileGLShmHandle* out) {
                if (out == nullptr || m_socketFd < 0) {
                    m_lastError = "OpenSharedMemory requires a connected transport.";
                    return false;
                }
#if defined(__linux__)
                const Uint32 arenaSize = m_maxShmArenaSize != 0 ? m_maxShmArenaSize : (64u * 1024u * 1024u);
#if defined(__ANDROID__)
                // memfd_create is only declared by bionic from API 30; the
                // plugin targets minSdk 26, so go through the syscall (the
                // kernel supports memfd since 3.17, which every Android
                // device ships).
                const int fd = static_cast<int>(
                    syscall(SYS_memfd_create, "mobilegl_shm", static_cast<unsigned int>(MFD_CLOEXEC)));
#else
                const int fd = memfd_create("mobilegl_shm", MFD_CLOEXEC);
#endif
                if (fd < 0 || ftruncate(fd, arenaSize) != 0) {
                    if (fd >= 0) close(fd);
                    m_lastError = "memfd_create/ftruncate failed.";
                    return false;
                }
                auto* address = mmap(nullptr, arenaSize, PROT_READ | PROT_WRITE,
                                     MAP_SHARED, fd, 0);
                if (address == MAP_FAILED) {
                    close(fd);
                    m_lastError = "mmap failed.";
                    return false;
                }

                *out = MobileGLShmHandle{};
                out->structSize = sizeof(MobileGLShmHandle);
                out->platformHandle = fd;
                out->offset = 0;
                out->size = arenaSize;
                out->capacity = arenaSize;
                out->mappedAddress = address;
                m_lastError.clear();
                return true;
#else
                (void)out;
                m_lastError = "OpenSharedMemory is not implemented on this platform.";
                return false;
#endif
            }

            void ReleaseSharedMemory(MobileGLShmHandle* handle) {
                if (handle == nullptr) {
                    return;
                }
#if defined(__linux__)
                if (handle->mappedAddress != nullptr) {
                    munmap(handle->mappedAddress, handle->size);
                }
                if (handle->platformHandle >= 0) {
                    close(static_cast<int>(handle->platformHandle));
                }
#endif
                *handle = MobileGLShmHandle{};
            }

            Bool SendShmHandle(MobileGLShmHandle* handle) {
                if (handle == nullptr || handle->platformHandle < 0 || m_socketFd < 0) {
                    m_lastError = "SendShmHandle requires a valid handle and connected socket.";
                    return false;
                }
#if defined(__linux__)
                struct {
                    uint64_t size;
                } meta{handle->size};
                char control[CMSG_SPACE(sizeof(int))]{};
                iovec iov{&meta, sizeof(meta)};
                msghdr message{};
                message.msg_iov = &iov;
                message.msg_iovlen = 1;
                message.msg_control = control;
                message.msg_controllen = sizeof(control);

                cmsghdr* header = CMSG_FIRSTHDR(&message);
                header->cmsg_level = SOL_SOCKET;
                header->cmsg_type = SCM_RIGHTS;
                header->cmsg_len = CMSG_LEN(sizeof(int));
                const int fd = static_cast<int>(handle->platformHandle);
                memcpy(CMSG_DATA(header), &fd, sizeof(fd));

                if (sendmsg(m_socketFd, &message, 0) < 0) {
                    m_lastError = "sendmsg(SCM_RIGHTS) failed.";
                    return false;
                }
                m_lastError.clear();
                return true;
#else
                m_lastError = "SendShmHandle is not implemented on this platform.";
                return false;
#endif
            }

            Bool RecvShmHandle(MobileGLShmHandle* out) {
                if (out == nullptr || m_socketFd < 0) {
                    m_lastError = "RecvShmHandle requires an output handle and connected socket.";
                    return false;
                }
#if defined(__linux__)
                struct {
                    uint64_t size;
                } meta{0};
                char control[CMSG_SPACE(sizeof(int))]{};
                iovec iov{&meta, sizeof(meta)};
                msghdr message{};
                message.msg_iov = &iov;
                message.msg_iovlen = 1;
                message.msg_control = control;
                message.msg_controllen = sizeof(control);

                const ssize_t received = recvmsg(m_socketFd, &message, 0);
                if (received < 0 || message.msg_controllen < CMSG_LEN(sizeof(int))) {
                    m_lastError = "recvmsg(SCM_RIGHTS) failed.";
                    return false;
                }
                cmsghdr* header = CMSG_FIRSTHDR(&message);
                if (header == nullptr || header->cmsg_type != SCM_RIGHTS) {
                    m_lastError = "No SCM_RIGHTS descriptor received.";
                    return false;
                }
                const int fd = *reinterpret_cast<const int*>(CMSG_DATA(header));
                auto* address = mmap(nullptr, meta.size, PROT_READ | PROT_WRITE,
                                     MAP_SHARED, fd, 0);
                if (address == MAP_FAILED) {
                    close(fd);
                    m_lastError = "mmap of received shm fd failed.";
                    return false;
                }

                *out = MobileGLShmHandle{};
                out->structSize = sizeof(MobileGLShmHandle);
                out->platformHandle = fd;
                out->offset = 0;
                out->size = meta.size;
                out->capacity = meta.size;
                out->mappedAddress = address;
                m_lastError.clear();
                return true;
#else
                (void)out;
                m_lastError = "RecvShmHandle is not implemented on this platform.";
                return false;
#endif
            }

            const char* GetLastError() const {
                return m_lastError.c_str();
            }

        private:
            Int32 m_socketFd = -1;
            Int32 m_serverFd = -1;
            Uint32 m_maxShmArenaSize = 0;
            String m_serverEndpoint;
            Vector<Uint8> m_received;
            Vector<MobileGLShmHandle> m_receivedShmHandles;
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

        Bool ReceiveShmHandleImpl(MobileGLTransport* t, MobileGLShmHandle* out) {
            if (t == nullptr || t->Implementation == nullptr) return false;
            return static_cast<LocalSocketShmTransport*>(t->Implementation)->RecvShmHandle(out);
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
            &ReceiveShmHandleImpl,
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

    Bool SendShmHandle(MobileGLTransport* t, MobileGLShmHandle* handle) {
        if (t == nullptr || t->Implementation == nullptr) return false;
        return static_cast<LocalSocketShmTransport*>(t->Implementation)->SendShmHandle(handle);
    }

    Bool RecvShmHandle(MobileGLTransport* t, MobileGLShmHandle* out) {
        if (t == nullptr || t->Implementation == nullptr) return false;
        return static_cast<LocalSocketShmTransport*>(t->Implementation)->RecvShmHandle(out);
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
