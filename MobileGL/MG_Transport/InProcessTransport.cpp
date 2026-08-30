// MobileGL - MobileGL/MG_Transport/InProcessTransport.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "InProcessTransport.h"
#include "MG_Transport/TransportInternal.h"

#include <deque>
#include <mutex>

namespace MobileGL::Transport {
    namespace {
        // A shared-memory slot lives in the link until both transports are
        // destroyed; in-process transports are session-scoped, so the leak is
        // bounded by session lifetime and the arena is freed when the link is
        // released (both ends destroyed).
        struct InProcessSlot {
            Vector<Uint8> bytes;
        };

        struct InProcessFrame {
            Vector<Uint8> bytes;
            Vector<MobileGLShmHandle> shm;
        };

        struct InProcessLink {
            std::mutex mutex;
            std::deque<InProcessFrame> clientToServer;
            std::deque<InProcessFrame> serverToClient;
            Uint64 nextSlotId = 1;
            UnorderedMap<Uint64, SharedPtr<InProcessSlot>> slots;
        };

        class InProcessTransport {
        public:
            InProcessTransport(SharedPtr<InProcessLink> link, Bool isClient)
                : m_link(Move(link)),
                  m_isClient(isClient) {
            }

            Bool Start(const MobileGLTransportConfig* cfg) {
                if (cfg == nullptr || cfg->kind != MobileGLTransportKindInProcess) {
                    m_lastError = "In-process transport requires an InProcess config.";
                    return false;
                }
                m_maxShmArenaSize = cfg->maxShmArenaSize;
                m_lastError.clear();
                return true;
            }

            void Shutdown() {
                m_lastError.clear();
            }

            Bool SubmitCommands(MobileGLCommandBatch* batch) {
                if (batch == nullptr || batch->flatBufferData == nullptr || batch->flatBufferSize == 0) {
                    m_lastError = "SubmitCommands requires a non-empty batch.";
                    return false;
                }
                InProcessFrame frame{};
                frame.bytes.resize(batch->flatBufferSize);
                memcpy(frame.bytes.data(), batch->flatBufferData, batch->flatBufferSize);
                if (batch->shmHandleCount > 0 && batch->shmHandles != nullptr) {
                    frame.shm.assign(batch->shmHandles,
                                     batch->shmHandles + batch->shmHandleCount);
                }

                std::lock_guard<std::mutex> lock(m_link->mutex);
                (m_isClient ? m_link->clientToServer : m_link->serverToClient)
                    .push_back(Move(frame));
                m_lastError.clear();
                return true;
            }

            Bool WaitResponses(MobileGLResponseQueue* out, uint32_t timeoutMs) {
                (void)timeoutMs;
                if (out == nullptr) {
                    m_lastError = "WaitResponses requires an output queue.";
                    return false;
                }
                const auto deadline = std::chrono::steady_clock::now() +
                                      std::chrono::milliseconds(timeoutMs);
                for (;;) {
                    {
                        std::lock_guard<std::mutex> lock(m_link->mutex);
                        auto& queue = m_isClient ? m_link->serverToClient : m_link->clientToServer;
                        if (!queue.empty()) {
                            InProcessFrame frame = Move(queue.front());
                            queue.pop_front();
                            m_received = Move(frame.bytes);
                            *out = MobileGLResponseQueue{};
                            out->structSize = sizeof(MobileGLResponseQueue);
                            out->count = 1;
                            out->flatBufferData = m_received.data();
                            out->flatBufferSize = static_cast<uint32_t>(m_received.size());
                            if (m_isClient) {
                                // Server-to-client responses carry their shm
                                // handles inline (LocalSocketShm semantics).
                                if (!frame.shm.empty()) {
                                    m_receivedShm = Move(frame.shm);
                                    out->shmHandleCount =
                                        static_cast<uint32_t>(m_receivedShm.size());
                                    out->shmHandles = m_receivedShm.data();
                                }
                            } else {
                                // Server side: request shm handles are drained
                                // one-by-one via ReceiveShmHandle after the
                                // frame is delivered.
                                if (!frame.shm.empty()) {
                                    m_pendingRequestShm.push_back(Move(frame.shm));
                                }
                            }
                            m_lastError.clear();
                            return true;
                        }
                    }
                    // timeoutMs == 0 means "wait indefinitely"; otherwise poll up
                    // to the requested deadline.
                    if (timeoutMs > 0 && std::chrono::steady_clock::now() >= deadline) {
                        m_lastError = "WaitResponses: no data available.";
                        return false;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }

            Bool OpenSharedMemory(MobileGLShmHandle* out) {
                if (out == nullptr) {
                    m_lastError = "OpenSharedMemory requires an output handle.";
                    return false;
                }
                const Uint64 capacity =
                    m_maxShmArenaSize != 0 ? m_maxShmArenaSize : (64u * 1024u * 1024u);
                auto slot = MakeShared<InProcessSlot>();
                slot->bytes.resize(static_cast<SizeT>(capacity));
                const Uint64 slotId = m_link->nextSlotId++;
                {
                    std::lock_guard<std::mutex> lock(m_link->mutex);
                    m_link->slots[slotId] = slot;
                }

                *out = MobileGLShmHandle{};
                out->structSize = sizeof(MobileGLShmHandle);
                out->platformHandle = static_cast<int64_t>(slotId);
                out->offset = 0;
                out->size = capacity;
                out->capacity = capacity;
                out->mappedAddress = slot->bytes.data();
                m_lastError.clear();
                return true;
            }

            void ReleaseSharedMemory(MobileGLShmHandle* handle) {
                // Slots stay alive in the link until both transports are
                // destroyed; this is the in-process analog of munmap.
                if (handle != nullptr) {
                    *handle = MobileGLShmHandle{};
                }
            }

            Bool ReceiveShmHandle(MobileGLShmHandle* out) {
                if (out == nullptr || m_isClient) {
                    m_lastError = "ReceiveShmHandle is only valid on the server half.";
                    return false;
                }
                std::lock_guard<std::mutex> lock(m_link->mutex);
                if (m_pendingRequestShm.empty() || m_pendingRequestShm.front().empty()) {
                    if (!m_pendingRequestShm.empty()) m_pendingRequestShm.pop_front();
                    m_lastError = "No pending shared-memory handle.";
                    return false;
                }
                auto& frame = m_pendingRequestShm.front();
                *out = frame.front();
                frame.erase(frame.begin());
                if (frame.empty()) {
                    m_pendingRequestShm.pop_front();
                }
                m_lastError.clear();
                return true;
            }

            const char* GetLastError() const {
                return m_lastError.c_str();
            }

        private:
            SharedPtr<InProcessLink> m_link;
            Bool m_isClient = false;
            Vector<Uint8> m_received;
            Vector<MobileGLShmHandle> m_receivedShm;
            std::deque<Vector<MobileGLShmHandle>> m_pendingRequestShm;
            Uint32 m_maxShmArenaSize = 0;
            String m_lastError;
        };

        Bool StartImpl(MobileGLTransport* t, const MobileGLTransportConfig* cfg) {
            if (t == nullptr || t->Implementation == nullptr) return false;
            return static_cast<InProcessTransport*>(t->Implementation)->Start(cfg);
        }

        void ShutdownImpl(MobileGLTransport* t) {
            if (t != nullptr && t->Implementation != nullptr) {
                static_cast<InProcessTransport*>(t->Implementation)->Shutdown();
            }
        }

        Bool SubmitCommandsImpl(MobileGLTransport* t, MobileGLCommandBatch* batch) {
            if (t == nullptr || t->Implementation == nullptr) return false;
            return static_cast<InProcessTransport*>(t->Implementation)->SubmitCommands(batch);
        }

        Bool WaitResponsesImpl(MobileGLTransport* t, MobileGLResponseQueue* out, uint32_t timeoutMs) {
            if (t == nullptr || t->Implementation == nullptr) return false;
            return static_cast<InProcessTransport*>(t->Implementation)->WaitResponses(out, timeoutMs);
        }

        Bool OpenSharedMemoryImpl(MobileGLTransport* t, MobileGLShmHandle* out) {
            if (t == nullptr || t->Implementation == nullptr) return false;
            return static_cast<InProcessTransport*>(t->Implementation)->OpenSharedMemory(out);
        }

        void ReleaseSharedMemoryImpl(MobileGLTransport* t, MobileGLShmHandle* handle) {
            if (t != nullptr && t->Implementation != nullptr) {
                static_cast<InProcessTransport*>(t->Implementation)->ReleaseSharedMemory(handle);
            }
        }

        Bool ReceiveShmHandleImpl(MobileGLTransport* t, MobileGLShmHandle* out) {
            if (t == nullptr || t->Implementation == nullptr) return false;
            return static_cast<InProcessTransport*>(t->Implementation)->ReceiveShmHandle(out);
        }

        const char* GetLastErrorImpl(MobileGLTransport* t) {
            if (t == nullptr || t->Implementation == nullptr) return "null transport.";
            return static_cast<InProcessTransport*>(t->Implementation)->GetLastError();
        }

        const MobileGLTransportOps g_inProcessOps = {
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

    void CreateInProcessTransportPair(MobileGLTransport** client, MobileGLTransport** server) {
        auto link = MakeShared<InProcessLink>();

        auto* clientTransport = new MobileGLTransport();
        clientTransport->Implementation = new InProcessTransport(link, true);

        auto* serverTransport = new MobileGLTransport();
        serverTransport->Implementation = new InProcessTransport(link, false);

        if (client != nullptr) *client = clientTransport;
        if (server != nullptr) *server = serverTransport;
    }

    void DestroyInProcessTransport(MobileGLTransport* t) {
        if (t == nullptr) return;
        if (t->Implementation != nullptr) {
            delete static_cast<InProcessTransport*>(t->Implementation);
        }
        delete t;
    }

    const MobileGLTransportOps& GetInProcessTransportOps() {
        return g_inProcessOps;
    }
} // namespace MobileGL::Transport

// End of File
