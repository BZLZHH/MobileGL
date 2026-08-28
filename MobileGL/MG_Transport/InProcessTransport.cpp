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
        struct InProcessLink {
            std::mutex mutex;
            std::deque<Vector<Uint8>> clientToServer;
            std::deque<Vector<Uint8>> serverToClient;
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
                Vector<Uint8> data(batch->flatBufferSize);
                memcpy(data.data(), batch->flatBufferData, batch->flatBufferSize);

                std::lock_guard<std::mutex> lock(m_link->mutex);
                (m_isClient ? m_link->clientToServer : m_link->serverToClient).push_back(Move(data));
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
                            m_received = Move(queue.front());
                            queue.pop_front();
                            *out = MobileGLResponseQueue{};
                            out->structSize = sizeof(MobileGLResponseQueue);
                            out->count = 1;
                            out->flatBufferData = m_received.data();
                            out->flatBufferSize = static_cast<uint32_t>(m_received.size());
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
                (void)out;
                m_lastError = "In-process transport does not allocate shared memory.";
                return false;
            }

            void ReleaseSharedMemory(MobileGLShmHandle* handle) {
                (void)handle;
            }

            const char* GetLastError() const {
                return m_lastError.c_str();
            }

        private:
            SharedPtr<InProcessLink> m_link;
            Bool m_isClient = false;
            Vector<Uint8> m_received;
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
