#include "ReplicationSystem.hpp"
#include "FirebaseClient.hpp"
#include <algorithm>
#include <sstream>
#include <cstring>
#include <fstream>
#include <nlohmann/json.hpp>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using SocketType = SOCKET;
    #define INVALID_SOCK INVALID_SOCKET
    #define SOCKET_ERROR_CODE WSAGetLastError()
    #define CLOSE_SOCKET closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <netdb.h>
    using SocketType = int;
    #define INVALID_SOCK -1
    #define SOCKET_ERROR_CODE errno
    #define CLOSE_SOCKET close
#endif

namespace Nova {

// ============================================================================
// IReplicable Interface Implementation
// ============================================================================

class IReplicable {
public:
    virtual ~IReplicable() = default;

    virtual uint64_t GetNetworkId() const = 0;
    virtual uint32_t GetOwnerClientId() const = 0;
    virtual void SetOwnerClientId(uint32_t clientId) = 0;

    virtual std::vector<uint8_t> SerializeState() const = 0;
    virtual void DeserializeState(const std::vector<uint8_t>& data) = 0;

    virtual std::vector<uint8_t> SerializeDelta(uint32_t lastAckedTick) const = 0;
    virtual void ApplyDelta(const std::vector<uint8_t>& delta) = 0;

    virtual bool IsDirty() const = 0;
    virtual void ClearDirty() = 0;

    virtual float GetRelevanceScore(const glm::vec3& observerPos) const = 0;
    virtual bool IsRelevantTo(uint32_t clientId) const = 0;
};

// ============================================================================
// DeltaCompressor - Bandwidth Optimization
// ============================================================================

class DeltaCompressor {
public:
    struct CompressionStats {
        uint64_t bytesBeforeCompression = 0;
        uint64_t bytesAfterCompression = 0;
        uint64_t deltasCompressed = 0;
        float compressionRatio = 1.0f;
    };

    static std::vector<uint8_t> CompressDelta(
        const std::vector<uint8_t>& oldState,
        const std::vector<uint8_t>& newState
    ) {
        std::vector<uint8_t> delta;
        delta.reserve(newState.size());

        // Header: delta encoding marker
        delta.push_back(0xDE);
        delta.push_back(0x17);
        delta.push_back(0xA0);

        // Write original size
        uint32_t origSize = static_cast<uint32_t>(newState.size());
        delta.push_back(static_cast<uint8_t>(origSize & 0xFF));
        delta.push_back(static_cast<uint8_t>((origSize >> 8) & 0xFF));
        delta.push_back(static_cast<uint8_t>((origSize >> 16) & 0xFF));
        delta.push_back(static_cast<uint8_t>((origSize >> 24) & 0xFF));

        size_t minSize = std::min(oldState.size(), newState.size());
        size_t runStart = 0;
        bool inCopyRun = false;

        for (size_t i = 0; i < newState.size(); ++i) {
            bool same = (i < minSize && oldState[i] == newState[i]);

            if (same && !inCopyRun) {
                // Start skip run
                if (i > runStart) {
                    // Flush copy run
                    WriteCopyRun(delta, newState, runStart, i - runStart);
                }
                runStart = i;
            } else if (!same && inCopyRun) {
                // End skip run, start copy run
                if (i > runStart) {
                    WriteSkipRun(delta, i - runStart);
                }
                runStart = i;
                inCopyRun = false;
            } else if (same) {
                inCopyRun = true;
            }
        }

        // Flush remaining
        if (runStart < newState.size()) {
            if (inCopyRun) {
                WriteSkipRun(delta, newState.size() - runStart);
            } else {
                WriteCopyRun(delta, newState, runStart, newState.size() - runStart);
            }
        }

        // If delta is larger than original, just send original
        if (delta.size() >= newState.size()) {
            delta.clear();
            delta.push_back(0x00); // Full state marker
            delta.insert(delta.end(), newState.begin(), newState.end());
        }

        return delta;
    }

    static std::vector<uint8_t> DecompressDelta(
        const std::vector<uint8_t>& oldState,
        const std::vector<uint8_t>& delta
    ) {
        if (delta.empty()) return oldState;

        // Check for full state marker
        if (delta[0] == 0x00) {
            return std::vector<uint8_t>(delta.begin() + 1, delta.end());
        }

        // Check delta header
        if (delta.size() < 7 || delta[0] != 0xDE || delta[1] != 0x17 || delta[2] != 0xA0) {
            return delta; // Invalid delta, return as-is
        }

        uint32_t origSize = delta[3] | (delta[4] << 8) | (delta[5] << 16) | (delta[6] << 24);
        std::vector<uint8_t> result;
        result.reserve(origSize);

        size_t offset = 7;
        size_t oldOffset = 0;

        while (offset < delta.size() && result.size() < origSize) {
            uint8_t cmd = delta[offset++];
            uint8_t type = (cmd >> 6) & 0x03;
            uint32_t length = cmd & 0x3F;

            if (length == 0x3F && offset < delta.size()) {
                // Extended length
                length = delta[offset++];
                if (offset < delta.size()) {
                    length |= (static_cast<uint32_t>(delta[offset++]) << 8);
                }
            }

            if (type == 0) {
                // Skip: copy from old state
                for (uint32_t i = 0; i < length && oldOffset < oldState.size(); ++i) {
                    result.push_back(oldState[oldOffset++]);
                }
            } else if (type == 1) {
                // Copy: read new bytes
                for (uint32_t i = 0; i < length && offset < delta.size(); ++i) {
                    result.push_back(delta[offset++]);
                    oldOffset++;
                }
            }
        }

        return result;
    }

    CompressionStats GetStats() const { return m_stats; }
    void ResetStats() { m_stats = CompressionStats{}; }

private:
    static void WriteSkipRun(std::vector<uint8_t>& delta, size_t length) {
        while (length > 0) {
            if (length <= 62) {
                delta.push_back(static_cast<uint8_t>(length)); // type 0 (skip)
                break;
            } else {
                delta.push_back(0x3F); // type 0, extended
                uint16_t len = static_cast<uint16_t>(std::min(length, size_t(65535)));
                delta.push_back(static_cast<uint8_t>(len & 0xFF));
                delta.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
                length -= len;
            }
        }
    }

    static void WriteCopyRun(std::vector<uint8_t>& delta, const std::vector<uint8_t>& data,
                             size_t start, size_t length) {
        while (length > 0) {
            size_t chunkLen = std::min(length, size_t(62));
            delta.push_back(static_cast<uint8_t>(0x40 | chunkLen)); // type 1 (copy)
            for (size_t i = 0; i < chunkLen; ++i) {
                delta.push_back(data[start + i]);
            }
            start += chunkLen;
            length -= chunkLen;
        }
    }

    CompressionStats m_stats;
};

// ============================================================================
// ReplicationChannel - Network Transport (TCP/UDP)
// ============================================================================

class ReplicationChannel {
public:
    enum class Protocol { TCP, UDP };
    enum class State { Disconnected, Connecting, Connected, Error };

    struct ChannelStats {
        uint64_t packetsSent = 0;
        uint64_t packetsReceived = 0;
        uint64_t packetsLost = 0;
        uint64_t bytesOut = 0;
        uint64_t bytesIn = 0;
        float packetLossRate = 0.0f;
        float averageRTT = 0.0f;
        float jitter = 0.0f;
        uint64_t bandwidthUsedBps = 0;
    };

    struct Packet {
        uint32_t sequenceNumber = 0;
        uint32_t ackNumber = 0;
        uint32_t ackBitfield = 0;
        uint64_t sendTime = 0;
        std::vector<uint8_t> data;
        bool reliable = false;
        bool needsAck = false;
    };

    ReplicationChannel(Protocol protocol = Protocol::UDP)
        : m_protocol(protocol), m_socket(INVALID_SOCK) {}

    ~ReplicationChannel() {
        Close();
    }

    bool Listen(uint16_t port) {
        if (!InitializeSockets()) return false;

        m_socket = socket(AF_INET,
            m_protocol == Protocol::TCP ? SOCK_STREAM : SOCK_DGRAM,
            m_protocol == Protocol::TCP ? IPPROTO_TCP : IPPROTO_UDP);

        if (m_socket == INVALID_SOCK) {
            m_lastError = "Failed to create socket";
            m_state = State::Error;
            return false;
        }

        // Set non-blocking
        SetNonBlocking(m_socket);

        // Allow address reuse
        int reuse = 1;
        setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR,
            reinterpret_cast<const char*>(&reuse), sizeof(reuse));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            m_lastError = "Failed to bind socket";
            m_state = State::Error;
            Close();
            return false;
        }

        if (m_protocol == Protocol::TCP) {
            if (listen(m_socket, 16) < 0) {
                m_lastError = "Failed to listen on socket";
                m_state = State::Error;
                Close();
                return false;
            }
        }

        m_port = port;
        m_state = State::Connected;
        m_isServer = true;
        return true;
    }

    bool Connect(const std::string& address, uint16_t port) {
        if (!InitializeSockets()) return false;

        m_socket = socket(AF_INET,
            m_protocol == Protocol::TCP ? SOCK_STREAM : SOCK_DGRAM,
            m_protocol == Protocol::TCP ? IPPROTO_TCP : IPPROTO_UDP);

        if (m_socket == INVALID_SOCK) {
            m_lastError = "Failed to create socket";
            m_state = State::Error;
            return false;
        }

        SetNonBlocking(m_socket);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) <= 0) {
            // Try hostname resolution
            struct addrinfo hints{}, *result;
            hints.ai_family = AF_INET;
            hints.ai_socktype = m_protocol == Protocol::TCP ? SOCK_STREAM : SOCK_DGRAM;

            if (getaddrinfo(address.c_str(), nullptr, &hints, &result) != 0) {
                m_lastError = "Failed to resolve hostname";
                m_state = State::Error;
                Close();
                return false;
            }

            addr.sin_addr = reinterpret_cast<sockaddr_in*>(result->ai_addr)->sin_addr;
            freeaddrinfo(result);
        }

        m_remoteAddr = addr;
        m_state = State::Connecting;

        if (m_protocol == Protocol::TCP) {
            int result = connect(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
            if (result < 0) {
#ifdef _WIN32
                if (WSAGetLastError() != WSAEWOULDBLOCK) {
#else
                if (errno != EINPROGRESS) {
#endif
                    m_lastError = "Failed to connect";
                    m_state = State::Error;
                    Close();
                    return false;
                }
            }
        } else {
            // UDP is "connected" immediately
            m_state = State::Connected;
        }

        m_isServer = false;
        return true;
    }

    void Close() {
        if (m_socket != INVALID_SOCK) {
            CLOSE_SOCKET(m_socket);
            m_socket = INVALID_SOCK;
        }
        for (auto& [id, sock] : m_clientSockets) {
            CLOSE_SOCKET(sock);
        }
        m_clientSockets.clear();
        m_state = State::Disconnected;
    }

    bool Send(const std::vector<uint8_t>& data, uint32_t clientId = 0, bool reliable = true) {
        if (m_state != State::Connected) return false;

        Packet packet;
        packet.sequenceNumber = m_nextSequence++;
        packet.ackNumber = m_lastReceivedSeq;
        packet.ackBitfield = m_ackBitfield;
        packet.data = data;
        packet.reliable = reliable;
        packet.sendTime = GetCurrentTimeMs();

        std::vector<uint8_t> packetData = SerializePacket(packet);

        bool success = false;
        if (m_protocol == Protocol::TCP) {
            SocketType targetSocket = m_socket;
            if (m_isServer && clientId != 0) {
                auto it = m_clientSockets.find(clientId);
                if (it != m_clientSockets.end()) {
                    targetSocket = it->second;
                }
            }
            success = SendTCP(targetSocket, packetData);
        } else {
            if (m_isServer && clientId != 0) {
                auto it = m_clientAddresses.find(clientId);
                if (it != m_clientAddresses.end()) {
                    success = SendUDP(packetData, it->second);
                }
            } else {
                success = SendUDP(packetData, m_remoteAddr);
            }
        }

        if (success) {
            m_stats.packetsSent++;
            m_stats.bytesOut += packetData.size();

            if (reliable) {
                m_pendingAcks[packet.sequenceNumber] = packet;
            }
        }

        return success;
    }

    std::vector<std::pair<uint32_t, std::vector<uint8_t>>> Receive() {
        std::vector<std::pair<uint32_t, std::vector<uint8_t>>> received;

        if (m_state != State::Connected && m_state != State::Connecting) {
            return received;
        }

        // Check for connection completion (TCP)
        if (m_state == State::Connecting && m_protocol == Protocol::TCP) {
            fd_set writeSet;
            FD_ZERO(&writeSet);
            FD_SET(m_socket, &writeSet);
            timeval tv{0, 0};

            if (select(static_cast<int>(m_socket) + 1, nullptr, &writeSet, nullptr, &tv) > 0) {
                int error = 0;
                socklen_t len = sizeof(error);
                getsockopt(m_socket, SOL_SOCKET, SO_ERROR,
                    reinterpret_cast<char*>(&error), &len);

                if (error == 0) {
                    m_state = State::Connected;
                } else {
                    m_state = State::Error;
                    m_lastError = "Connection failed";
                }
            }
        }

        // Accept new TCP connections
        if (m_isServer && m_protocol == Protocol::TCP) {
            AcceptNewConnections();
        }

        // Receive data
        if (m_protocol == Protocol::TCP) {
            received = ReceiveTCP();
        } else {
            received = ReceiveUDP();
        }

        // Process acknowledgments
        for (const auto& [clientId, data] : received) {
            ProcessAcknowledgments(data);
        }

        // Resend unacked reliable packets
        ResendUnackedPackets();

        // Update stats
        UpdateStats();

        return received;
    }

    State GetState() const { return m_state; }
    const std::string& GetLastError() const { return m_lastError; }
    const ChannelStats& GetStats() const { return m_stats; }
    Protocol GetProtocol() const { return m_protocol; }

    std::vector<uint32_t> GetConnectedClients() const {
        std::vector<uint32_t> clients;
        for (const auto& [id, _] : m_clientSockets) {
            clients.push_back(id);
        }
        for (const auto& [id, _] : m_clientAddresses) {
            if (m_clientSockets.find(id) == m_clientSockets.end()) {
                clients.push_back(id);
            }
        }
        return clients;
    }

private:
    bool InitializeSockets() {
#ifdef _WIN32
        static bool initialized = false;
        if (!initialized) {
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                m_lastError = "WSAStartup failed";
                return false;
            }
            initialized = true;
        }
#endif
        return true;
    }

    void SetNonBlocking(SocketType sock) {
#ifdef _WIN32
        u_long mode = 1;
        ioctlsocket(sock, FIONBIO, &mode);
#else
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
    }

    bool SendTCP(SocketType sock, const std::vector<uint8_t>& data) {
        // Prefix with length
        uint32_t len = static_cast<uint32_t>(data.size());
        std::vector<uint8_t> packet(4 + data.size());
        packet[0] = static_cast<uint8_t>(len & 0xFF);
        packet[1] = static_cast<uint8_t>((len >> 8) & 0xFF);
        packet[2] = static_cast<uint8_t>((len >> 16) & 0xFF);
        packet[3] = static_cast<uint8_t>((len >> 24) & 0xFF);
        std::copy(data.begin(), data.end(), packet.begin() + 4);

        int sent = send(sock, reinterpret_cast<const char*>(packet.data()),
            static_cast<int>(packet.size()), 0);
        return sent == static_cast<int>(packet.size());
    }

    bool SendUDP(const std::vector<uint8_t>& data, const sockaddr_in& addr) {
        int sent = sendto(m_socket, reinterpret_cast<const char*>(data.data()),
            static_cast<int>(data.size()), 0,
            reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
        return sent == static_cast<int>(data.size());
    }

    void AcceptNewConnections() {
        sockaddr_in clientAddr{};
        socklen_t addrLen = sizeof(clientAddr);

        SocketType clientSocket = accept(m_socket,
            reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);

        if (clientSocket != INVALID_SOCK) {
            SetNonBlocking(clientSocket);
            uint32_t clientId = m_nextClientId++;
            m_clientSockets[clientId] = clientSocket;
            m_clientAddresses[clientId] = clientAddr;
        }
    }

    std::vector<std::pair<uint32_t, std::vector<uint8_t>>> ReceiveTCP() {
        std::vector<std::pair<uint32_t, std::vector<uint8_t>>> received;

        auto receiveFromSocket = [this](SocketType sock, uint32_t clientId)
            -> std::optional<std::vector<uint8_t>> {
            // Read length prefix
            uint8_t lenBuf[4];
            int bytesRead = recv(sock, reinterpret_cast<char*>(lenBuf), 4, MSG_PEEK);
            if (bytesRead < 4) return std::nullopt;

            uint32_t len = lenBuf[0] | (lenBuf[1] << 8) | (lenBuf[2] << 16) | (lenBuf[3] << 24);
            if (len > 65536) return std::nullopt; // Sanity check

            std::vector<uint8_t> buffer(4 + len);
            bytesRead = recv(sock, reinterpret_cast<char*>(buffer.data()),
                static_cast<int>(buffer.size()), 0);

            if (bytesRead == static_cast<int>(buffer.size())) {
                m_stats.packetsReceived++;
                m_stats.bytesIn += bytesRead;
                return std::vector<uint8_t>(buffer.begin() + 4, buffer.end());
            }
            return std::nullopt;
        };

        if (m_isServer) {
            for (const auto& [clientId, sock] : m_clientSockets) {
                if (auto data = receiveFromSocket(sock, clientId)) {
                    received.emplace_back(clientId, std::move(*data));
                }
            }
        } else {
            if (auto data = receiveFromSocket(m_socket, 0)) {
                received.emplace_back(0, std::move(*data));
            }
        }

        return received;
    }

    std::vector<std::pair<uint32_t, std::vector<uint8_t>>> ReceiveUDP() {
        std::vector<std::pair<uint32_t, std::vector<uint8_t>>> received;

        std::vector<uint8_t> buffer(65536);
        sockaddr_in senderAddr{};
        socklen_t addrLen = sizeof(senderAddr);

        while (true) {
            int bytesRead = recvfrom(m_socket, reinterpret_cast<char*>(buffer.data()),
                static_cast<int>(buffer.size()), 0,
                reinterpret_cast<sockaddr*>(&senderAddr), &addrLen);

            if (bytesRead <= 0) break;

            m_stats.packetsReceived++;
            m_stats.bytesIn += bytesRead;

            // Find or create client ID
            uint32_t clientId = 0;
            for (const auto& [id, addr] : m_clientAddresses) {
                if (addr.sin_addr.s_addr == senderAddr.sin_addr.s_addr &&
                    addr.sin_port == senderAddr.sin_port) {
                    clientId = id;
                    break;
                }
            }

            if (clientId == 0 && m_isServer) {
                clientId = m_nextClientId++;
                m_clientAddresses[clientId] = senderAddr;
            }

            received.emplace_back(clientId,
                std::vector<uint8_t>(buffer.begin(), buffer.begin() + bytesRead));
        }

        return received;
    }

    std::vector<uint8_t> SerializePacket(const Packet& packet) {
        std::vector<uint8_t> data;
        data.reserve(16 + packet.data.size());

        // Header
        auto writeU32 = [&data](uint32_t v) {
            data.push_back(static_cast<uint8_t>(v & 0xFF));
            data.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
            data.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
            data.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
        };

        writeU32(packet.sequenceNumber);
        writeU32(packet.ackNumber);
        writeU32(packet.ackBitfield);
        data.push_back(packet.reliable ? 1 : 0);

        // Payload
        data.insert(data.end(), packet.data.begin(), packet.data.end());

        return data;
    }

    void ProcessAcknowledgments(const std::vector<uint8_t>& data) {
        if (data.size() < 13) return;

        uint32_t ackNum = data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
        uint32_t ackBits = data[8] | (data[9] << 8) | (data[10] << 16) | (data[11] << 24);

        // Process main ack
        if (m_pendingAcks.count(ackNum)) {
            uint64_t rtt = GetCurrentTimeMs() - m_pendingAcks[ackNum].sendTime;
            UpdateRTT(rtt);
            m_pendingAcks.erase(ackNum);
        }

        // Process ack bitfield
        for (int i = 0; i < 32; ++i) {
            if (ackBits & (1 << i)) {
                uint32_t seq = ackNum - i - 1;
                if (m_pendingAcks.count(seq)) {
                    uint64_t rtt = GetCurrentTimeMs() - m_pendingAcks[seq].sendTime;
                    UpdateRTT(rtt);
                    m_pendingAcks.erase(seq);
                }
            }
        }

        // Update received sequence tracking
        uint32_t seq = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
        if (seq > m_lastReceivedSeq) {
            int diff = static_cast<int>(seq - m_lastReceivedSeq);
            if (diff <= 32) {
                m_ackBitfield = (m_ackBitfield << diff) | 1;
            } else {
                m_ackBitfield = 1;
            }
            m_lastReceivedSeq = seq;
        } else {
            int diff = static_cast<int>(m_lastReceivedSeq - seq);
            if (diff < 32) {
                m_ackBitfield |= (1 << diff);
            }
        }
    }

    void ResendUnackedPackets() {
        uint64_t now = GetCurrentTimeMs();
        uint64_t resendThreshold = static_cast<uint64_t>(m_stats.averageRTT * 1.5f + 50);

        for (auto& [seq, packet] : m_pendingAcks) {
            if (now - packet.sendTime > resendThreshold) {
                packet.sendTime = now;
                std::vector<uint8_t> data = SerializePacket(packet);

                if (m_protocol == Protocol::TCP) {
                    SendTCP(m_socket, data);
                } else {
                    SendUDP(data, m_remoteAddr);
                }

                m_stats.packetsSent++;
                m_stats.bytesOut += data.size();
            }
        }
    }

    void UpdateRTT(uint64_t rtt) {
        const float alpha = 0.125f;
        const float beta = 0.25f;

        if (m_stats.averageRTT == 0.0f) {
            m_stats.averageRTT = static_cast<float>(rtt);
            m_stats.jitter = static_cast<float>(rtt) / 2.0f;
        } else {
            float diff = std::abs(static_cast<float>(rtt) - m_stats.averageRTT);
            m_stats.jitter = (1.0f - beta) * m_stats.jitter + beta * diff;
            m_stats.averageRTT = (1.0f - alpha) * m_stats.averageRTT + alpha * static_cast<float>(rtt);
        }
    }

    void UpdateStats() {
        uint64_t now = GetCurrentTimeMs();
        if (now - m_lastStatsUpdate >= 1000) {
            uint64_t elapsed = now - m_lastStatsUpdate;
            m_stats.bandwidthUsedBps = (m_stats.bytesOut * 1000) / elapsed;

            if (m_stats.packetsSent > 0) {
                m_stats.packetLossRate = static_cast<float>(m_pendingAcks.size()) /
                    static_cast<float>(m_stats.packetsSent);
            }

            m_lastStatsUpdate = now;
        }
    }

    static uint64_t GetCurrentTimeMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    Protocol m_protocol;
    State m_state = State::Disconnected;
    SocketType m_socket;
    uint16_t m_port = 0;
    bool m_isServer = false;
    std::string m_lastError;

    sockaddr_in m_remoteAddr{};
    std::unordered_map<uint32_t, SocketType> m_clientSockets;
    std::unordered_map<uint32_t, sockaddr_in> m_clientAddresses;
    uint32_t m_nextClientId = 1;

    // Reliability
    uint32_t m_nextSequence = 1;
    uint32_t m_lastReceivedSeq = 0;
    uint32_t m_ackBitfield = 0;
    std::unordered_map<uint32_t, Packet> m_pendingAcks;

    ChannelStats m_stats;
    uint64_t m_lastStatsUpdate = 0;
};

// ============================================================================
// AuthorityManager - Ownership Management
// ============================================================================

class AuthorityManager {
public:
    enum class AuthorityType {
        ServerAuthoritative,    // Server has full control
        ClientAuthoritative,    // Owning client has control
        SharedAuthority,        // Both can modify (with conflict resolution)
        PredictiveAuthority     // Client predicts, server validates
    };

    struct EntityAuthority {
        uint64_t entityId = 0;
        uint32_t ownerClientId = 0;
        AuthorityType type = AuthorityType::ServerAuthoritative;
        bool pendingTransfer = false;
        uint32_t pendingOwner = 0;
        uint64_t lastUpdateTick = 0;
    };

    void SetAuthority(uint64_t entityId, uint32_t clientId, AuthorityType type) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& auth = m_authorities[entityId];
        auth.entityId = entityId;
        auth.ownerClientId = clientId;
        auth.type = type;
    }

    bool HasAuthority(uint64_t entityId, uint32_t clientId, bool isServer) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_authorities.find(entityId);
        if (it == m_authorities.end()) {
            return isServer; // Default to server authority
        }

        const auto& auth = it->second;
        switch (auth.type) {
            case AuthorityType::ServerAuthoritative:
                return isServer;
            case AuthorityType::ClientAuthoritative:
            case AuthorityType::PredictiveAuthority:
                return auth.ownerClientId == clientId;
            case AuthorityType::SharedAuthority:
                return isServer || auth.ownerClientId == clientId;
        }
        return false;
    }

    uint32_t GetOwner(uint64_t entityId) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_authorities.find(entityId);
        return it != m_authorities.end() ? it->second.ownerClientId : 0;
    }

    AuthorityType GetAuthorityType(uint64_t entityId) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_authorities.find(entityId);
        return it != m_authorities.end() ? it->second.type : AuthorityType::ServerAuthoritative;
    }

    bool RequestTransfer(uint64_t entityId, uint32_t fromClient, uint32_t toClient) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_authorities.find(entityId);
        if (it == m_authorities.end()) return false;

        if (it->second.ownerClientId != fromClient) return false;

        it->second.pendingTransfer = true;
        it->second.pendingOwner = toClient;
        return true;
    }

    bool ConfirmTransfer(uint64_t entityId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_authorities.find(entityId);
        if (it == m_authorities.end() || !it->second.pendingTransfer) return false;

        it->second.ownerClientId = it->second.pendingOwner;
        it->second.pendingTransfer = false;
        it->second.pendingOwner = 0;
        return true;
    }

    void RemoveEntity(uint64_t entityId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_authorities.erase(entityId);
    }

    std::vector<uint64_t> GetEntitiesOwnedBy(uint32_t clientId) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<uint64_t> entities;
        for (const auto& [id, auth] : m_authorities) {
            if (auth.ownerClientId == clientId) {
                entities.push_back(id);
            }
        }
        return entities;
    }

private:
    mutable std::mutex m_mutex;
    std::unordered_map<uint64_t, EntityAuthority> m_authorities;
};

// ============================================================================
// InterestManager - Relevance-based filtering
// ============================================================================

class InterestManager {
public:
    struct InterestArea {
        glm::vec3 center;
        float radius = 100.0f;
        float priority = 1.0f;
    };

    struct ClientInterest {
        uint32_t clientId = 0;
        std::vector<InterestArea> areas;
        std::unordered_set<uint64_t> relevantEntities;
        glm::vec3 lastPosition;
        float updateRadius = 150.0f;
    };

    void SetClientPosition(uint32_t clientId, const glm::vec3& position, float radius = 150.0f) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& interest = m_clientInterests[clientId];
        interest.clientId = clientId;
        interest.lastPosition = position;
        interest.updateRadius = radius;

        // Primary interest area around player
        interest.areas.clear();
        interest.areas.push_back({position, radius, 1.0f});
    }

    void AddInterestArea(uint32_t clientId, const glm::vec3& center, float radius, float priority) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_clientInterests.find(clientId);
        if (it != m_clientInterests.end()) {
            it->second.areas.push_back({center, radius, priority});
        }
    }

    bool IsRelevant(uint64_t entityId, const glm::vec3& entityPos, uint32_t clientId) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_clientInterests.find(clientId);
        if (it == m_clientInterests.end()) return true; // No interest data, assume relevant

        for (const auto& area : it->second.areas) {
            float dist = glm::length(entityPos - area.center);
            if (dist <= area.radius) {
                return true;
            }
        }
        return false;
    }

    float GetRelevanceScore(uint64_t entityId, const glm::vec3& entityPos, uint32_t clientId) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_clientInterests.find(clientId);
        if (it == m_clientInterests.end()) return 1.0f;

        float maxScore = 0.0f;
        for (const auto& area : it->second.areas) {
            float dist = glm::length(entityPos - area.center);
            if (dist <= area.radius) {
                float normalizedDist = 1.0f - (dist / area.radius);
                float score = normalizedDist * area.priority;
                maxScore = std::max(maxScore, score);
            }
        }
        return maxScore;
    }

    std::vector<uint32_t> GetClientsInterestedIn(uint64_t entityId, const glm::vec3& entityPos) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<uint32_t> clients;
        for (const auto& [clientId, interest] : m_clientInterests) {
            if (IsRelevantInternal(entityPos, interest)) {
                clients.push_back(clientId);
            }
        }
        return clients;
    }

    void RemoveClient(uint32_t clientId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_clientInterests.erase(clientId);
    }

private:
    bool IsRelevantInternal(const glm::vec3& entityPos, const ClientInterest& interest) const {
        for (const auto& area : interest.areas) {
            if (glm::length(entityPos - area.center) <= area.radius) {
                return true;
            }
        }
        return false;
    }

    mutable std::mutex m_mutex;
    std::unordered_map<uint32_t, ClientInterest> m_clientInterests;
};

// ============================================================================
// SnapshotInterpolator - Smooth state interpolation
// ============================================================================

class SnapshotInterpolator {
public:
    struct Snapshot {
        uint32_t tick = 0;
        uint64_t timestamp = 0;
        std::unordered_map<uint64_t, std::vector<uint8_t>> entityStates;
    };

    struct InterpolatedState {
        std::vector<uint8_t> state;
        float interpolationFactor = 0.0f;
        bool valid = false;
    };

    void SetInterpolationDelay(float delayMs) {
        m_interpolationDelay = delayMs;
    }

    void AddSnapshot(const Snapshot& snapshot) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_snapshots.push_back(snapshot);

        // Keep only recent snapshots (1 second buffer)
        while (m_snapshots.size() > 60) {
            m_snapshots.erase(m_snapshots.begin());
        }
    }

    InterpolatedState GetInterpolatedState(uint64_t entityId, uint64_t currentTime) const {
        std::lock_guard<std::mutex> lock(m_mutex);

        InterpolatedState result;
        if (m_snapshots.size() < 2) {
            if (!m_snapshots.empty()) {
                auto it = m_snapshots.back().entityStates.find(entityId);
                if (it != m_snapshots.back().entityStates.end()) {
                    result.state = it->second;
                    result.interpolationFactor = 1.0f;
                    result.valid = true;
                }
            }
            return result;
        }

        uint64_t targetTime = currentTime - static_cast<uint64_t>(m_interpolationDelay);

        // Find surrounding snapshots
        const Snapshot* before = nullptr;
        const Snapshot* after = nullptr;

        for (size_t i = 0; i < m_snapshots.size() - 1; ++i) {
            if (m_snapshots[i].timestamp <= targetTime &&
                m_snapshots[i + 1].timestamp >= targetTime) {
                before = &m_snapshots[i];
                after = &m_snapshots[i + 1];
                break;
            }
        }

        if (!before || !after) {
            // Use latest snapshot
            auto it = m_snapshots.back().entityStates.find(entityId);
            if (it != m_snapshots.back().entityStates.end()) {
                result.state = it->second;
                result.interpolationFactor = 1.0f;
                result.valid = true;
            }
            return result;
        }

        auto itBefore = before->entityStates.find(entityId);
        auto itAfter = after->entityStates.find(entityId);

        if (itBefore == before->entityStates.end()) {
            if (itAfter != after->entityStates.end()) {
                result.state = itAfter->second;
                result.interpolationFactor = 1.0f;
                result.valid = true;
            }
            return result;
        }

        if (itAfter == after->entityStates.end()) {
            result.state = itBefore->second;
            result.interpolationFactor = 0.0f;
            result.valid = true;
            return result;
        }

        // Interpolate between snapshots
        float t = static_cast<float>(targetTime - before->timestamp) /
                  static_cast<float>(after->timestamp - before->timestamp);
        t = std::clamp(t, 0.0f, 1.0f);

        result.state = InterpolateStates(itBefore->second, itAfter->second, t);
        result.interpolationFactor = t;
        result.valid = true;

        return result;
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_snapshots.clear();
    }

private:
    std::vector<uint8_t> InterpolateStates(
        const std::vector<uint8_t>& a,
        const std::vector<uint8_t>& b,
        float t
    ) const {
        // Basic interpolation - assumes state starts with position (3 floats)
        if (a.size() < 12 || b.size() < 12 || a.size() != b.size()) {
            return t < 0.5f ? a : b;
        }

        std::vector<uint8_t> result = a;

        // Interpolate first 3 floats (position)
        for (int i = 0; i < 3; ++i) {
            float va, vb;
            std::memcpy(&va, a.data() + i * 4, 4);
            std::memcpy(&vb, b.data() + i * 4, 4);
            float vi = va + (vb - va) * t;
            std::memcpy(result.data() + i * 4, &vi, 4);
        }

        // If there are more floats (rotation as quaternion), interpolate those too
        if (a.size() >= 28) {
            // Quaternion slerp approximation
            for (int i = 3; i < 7; ++i) {
                float va, vb;
                std::memcpy(&va, a.data() + i * 4, 4);
                std::memcpy(&vb, b.data() + i * 4, 4);
                float vi = va + (vb - va) * t;
                std::memcpy(result.data() + i * 4, &vi, 4);
            }
        }

        return result;
    }

    mutable std::mutex m_mutex;
    std::vector<Snapshot> m_snapshots;
    float m_interpolationDelay = 100.0f; // ms
};

// ============================================================================
// ClientPrediction - Client-side prediction and reconciliation
// ============================================================================

class ClientPrediction {
public:
    struct PredictedInput {
        uint32_t inputSequence = 0;
        uint64_t timestamp = 0;
        std::vector<uint8_t> inputData;
        std::vector<uint8_t> predictedState;
    };

    struct ReconciliationResult {
        bool needsCorrection = false;
        float correctionMagnitude = 0.0f;
        std::vector<uint8_t> correctedState;
    };

    void RecordInput(uint32_t sequence, const std::vector<uint8_t>& input,
                     const std::vector<uint8_t>& predictedState) {
        std::lock_guard<std::mutex> lock(m_mutex);

        PredictedInput pred;
        pred.inputSequence = sequence;
        pred.timestamp = GetCurrentTimeMs();
        pred.inputData = input;
        pred.predictedState = predictedState;

        m_pendingInputs.push_back(pred);

        // Limit buffer size
        while (m_pendingInputs.size() > 128) {
            m_pendingInputs.erase(m_pendingInputs.begin());
        }
    }

    ReconciliationResult Reconcile(uint32_t serverAckedSequence,
                                    const std::vector<uint8_t>& serverState) {
        std::lock_guard<std::mutex> lock(m_mutex);

        ReconciliationResult result;

        // Remove all inputs up to and including acked sequence
        while (!m_pendingInputs.empty() &&
               m_pendingInputs.front().inputSequence <= serverAckedSequence) {
            m_pendingInputs.erase(m_pendingInputs.begin());
        }

        if (m_pendingInputs.empty()) {
            result.correctedState = serverState;
            return result;
        }

        // Check if prediction matches server state
        float error = CalculateStateError(serverState, m_pendingInputs.front().predictedState);

        if (error > m_correctionThreshold) {
            result.needsCorrection = true;
            result.correctionMagnitude = error;
            result.correctedState = serverState;

            // Re-apply pending inputs
            // (Caller should re-simulate from server state with pending inputs)
        }

        return result;
    }

    const std::vector<PredictedInput>& GetPendingInputs() const {
        return m_pendingInputs;
    }

    void SetCorrectionThreshold(float threshold) {
        m_correctionThreshold = threshold;
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingInputs.clear();
    }

private:
    float CalculateStateError(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) const {
        if (a.size() < 12 || b.size() < 12) return 0.0f;

        // Compare positions
        glm::vec3 posA, posB;
        std::memcpy(&posA, a.data(), 12);
        std::memcpy(&posB, b.data(), 12);

        return glm::length(posA - posB);
    }

    static uint64_t GetCurrentTimeMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    mutable std::mutex m_mutex;
    std::vector<PredictedInput> m_pendingInputs;
    float m_correctionThreshold = 0.1f; // Position units
};

// ============================================================================
// BandwidthProfiler - Network usage tracking
// ============================================================================

class BandwidthProfiler {
public:
    struct CategoryStats {
        std::string category;
        uint64_t bytesOut = 0;
        uint64_t bytesIn = 0;
        uint64_t packetCount = 0;
        float percentageOfTotal = 0.0f;
    };

    struct ProfileReport {
        uint64_t totalBytesOut = 0;
        uint64_t totalBytesIn = 0;
        uint64_t totalPackets = 0;
        float outgoingBandwidthKbps = 0.0f;
        float incomingBandwidthKbps = 0.0f;
        float peakOutgoingKbps = 0.0f;
        float peakIncomingKbps = 0.0f;
        std::vector<CategoryStats> categoryBreakdown;
    };

    void RecordOutgoing(const std::string& category, size_t bytes) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& stats = m_categoryStats[category];
        stats.category = category;
        stats.bytesOut += bytes;
        stats.packetCount++;
        m_totalBytesOut += bytes;
        m_recentBytesOut += bytes;
    }

    void RecordIncoming(const std::string& category, size_t bytes) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& stats = m_categoryStats[category];
        stats.category = category;
        stats.bytesIn += bytes;
        m_totalBytesIn += bytes;
        m_recentBytesIn += bytes;
    }

    void Update(float deltaTime) {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_sampleTimer += deltaTime;
        if (m_sampleTimer >= 1.0f) {
            // Calculate bandwidth
            m_currentOutgoingKbps = static_cast<float>(m_recentBytesOut * 8) / 1000.0f / m_sampleTimer;
            m_currentIncomingKbps = static_cast<float>(m_recentBytesIn * 8) / 1000.0f / m_sampleTimer;

            m_peakOutgoingKbps = std::max(m_peakOutgoingKbps, m_currentOutgoingKbps);
            m_peakIncomingKbps = std::max(m_peakIncomingKbps, m_currentIncomingKbps);

            // Store history
            m_bandwidthHistory.push_back({m_currentOutgoingKbps, m_currentIncomingKbps});
            if (m_bandwidthHistory.size() > 60) {
                m_bandwidthHistory.erase(m_bandwidthHistory.begin());
            }

            m_recentBytesOut = 0;
            m_recentBytesIn = 0;
            m_sampleTimer = 0.0f;
        }
    }

    ProfileReport GetReport() const {
        std::lock_guard<std::mutex> lock(m_mutex);

        ProfileReport report;
        report.totalBytesOut = m_totalBytesOut;
        report.totalBytesIn = m_totalBytesIn;
        report.outgoingBandwidthKbps = m_currentOutgoingKbps;
        report.incomingBandwidthKbps = m_currentIncomingKbps;
        report.peakOutgoingKbps = m_peakOutgoingKbps;
        report.peakIncomingKbps = m_peakIncomingKbps;

        for (const auto& [name, stats] : m_categoryStats) {
            CategoryStats cs = stats;
            if (m_totalBytesOut > 0) {
                cs.percentageOfTotal = static_cast<float>(stats.bytesOut) /
                    static_cast<float>(m_totalBytesOut) * 100.0f;
            }
            report.categoryBreakdown.push_back(cs);
            report.totalPackets += stats.packetCount;
        }

        // Sort by bytes out descending
        std::sort(report.categoryBreakdown.begin(), report.categoryBreakdown.end(),
            [](const CategoryStats& a, const CategoryStats& b) {
                return a.bytesOut > b.bytesOut;
            });

        return report;
    }

    void Reset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_categoryStats.clear();
        m_totalBytesOut = 0;
        m_totalBytesIn = 0;
        m_recentBytesOut = 0;
        m_recentBytesIn = 0;
        m_peakOutgoingKbps = 0.0f;
        m_peakIncomingKbps = 0.0f;
        m_bandwidthHistory.clear();
    }

private:
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, CategoryStats> m_categoryStats;

    uint64_t m_totalBytesOut = 0;
    uint64_t m_totalBytesIn = 0;
    uint64_t m_recentBytesOut = 0;
    uint64_t m_recentBytesIn = 0;

    float m_currentOutgoingKbps = 0.0f;
    float m_currentIncomingKbps = 0.0f;
    float m_peakOutgoingKbps = 0.0f;
    float m_peakIncomingKbps = 0.0f;

    float m_sampleTimer = 0.0f;
    std::vector<std::pair<float, float>> m_bandwidthHistory;
};

// ============================================================================
// Static instances for new components
// ============================================================================

static std::unique_ptr<ReplicationChannel> g_tcpChannel;
static std::unique_ptr<ReplicationChannel> g_udpChannel;
static std::unique_ptr<AuthorityManager> g_authorityManager;
static std::unique_ptr<InterestManager> g_interestManager;
static std::unique_ptr<SnapshotInterpolator> g_snapshotInterpolator;
static std::unique_ptr<ClientPrediction> g_clientPrediction;
static std::unique_ptr<BandwidthProfiler> g_bandwidthProfiler;
static DeltaCompressor g_deltaCompressor;

// ============================================================================
// NetworkEvent Implementation
// ============================================================================

void NetworkEvent::SetProperty(const std::string& name, const EventValue& value) {
    for (auto& prop : properties) {
        if (prop.name == name) {
            prop.value = value;
            prop.dirty = true;
            return;
        }
    }
    properties.push_back({name, value, true});
}

EventValue NetworkEvent::GetProperty(const std::string& name) const {
    for (const auto& prop : properties) {
        if (prop.name == name) {
            return prop.value;
        }
    }
    return std::monostate{};
}

bool NetworkEvent::HasProperty(const std::string& name) const {
    for (const auto& prop : properties) {
        if (prop.name == name) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// EventTypeRegistry Implementation
// ============================================================================

EventTypeRegistry& EventTypeRegistry::Instance() {
    static EventTypeRegistry instance;
    return instance;
}

void EventTypeRegistry::RegisterType(const EventTypeConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_types[config.typeName] = config;
}

void EventTypeRegistry::UnregisterType(const std::string& typeName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_types.erase(typeName);
}

const EventTypeConfig* EventTypeRegistry::GetConfig(const std::string& typeName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_types.find(typeName);
    return it != m_types.end() ? &it->second : nullptr;
}

std::vector<std::string> EventTypeRegistry::GetTypesByCategory(ReplicationCategory category) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> result;
    for (const auto& [name, config] : m_types) {
        if (config.defaultCategory == category) {
            result.push_back(name);
        }
    }
    return result;
}

void EventTypeRegistry::SetOverride(const std::string& typeName, const std::string& property, const EventValue& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_overrides[typeName][property] = value;
}

void EventTypeRegistry::ClearOverrides(const std::string& typeName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_overrides.erase(typeName);
}

// ============================================================================
// ReplicationSystem Implementation
// ============================================================================

ReplicationSystem& ReplicationSystem::Instance() {
    static ReplicationSystem instance;
    return instance;
}

void ReplicationSystem::Initialize(const Config& config) {
    m_config = config;

    // Initialize networking components
    g_tcpChannel = std::make_unique<ReplicationChannel>(ReplicationChannel::Protocol::TCP);
    g_udpChannel = std::make_unique<ReplicationChannel>(ReplicationChannel::Protocol::UDP);
    g_authorityManager = std::make_unique<AuthorityManager>();
    g_interestManager = std::make_unique<InterestManager>();
    g_snapshotInterpolator = std::make_unique<SnapshotInterpolator>();
    g_clientPrediction = std::make_unique<ClientPrediction>();
    g_bandwidthProfiler = std::make_unique<BandwidthProfiler>();

    // Configure interpolation
    if (config.enableInterpolation) {
        g_snapshotInterpolator->SetInterpolationDelay(config.interpolationDelay * 1000.0f);
    }

    // Register default event types
    RegisterDefaultEventTypes();

    m_initialized = true;
}

void ReplicationSystem::Shutdown() {
    Disconnect();

    g_tcpChannel.reset();
    g_udpChannel.reset();
    g_authorityManager.reset();
    g_interestManager.reset();
    g_snapshotInterpolator.reset();
    g_clientPrediction.reset();
    g_bandwidthProfiler.reset();

    m_initialized = false;
}

void ReplicationSystem::Update(float deltaTime) {
    if (!m_initialized) return;

    // Update bandwidth profiler
    if (g_bandwidthProfiler) {
        g_bandwidthProfiler->Update(deltaTime);
    }

    // Process incoming network packets from both channels
    ProcessIncomingPackets();

    // Process local events
    m_syncTimer += deltaTime;
    if (m_syncTimer >= m_config.syncInterval) {
        m_syncTimer = 0.0f;
        SendOutgoingPackets();
    }

    // Send heartbeats
    m_heartbeatTimer += deltaTime;
    if (m_heartbeatTimer >= m_config.heartbeatInterval) {
        m_heartbeatTimer = 0.0f;
        SendHeartbeats();
        CheckTimeouts();
    }

    // Process Firebase queue
    ProcessFirebaseQueue();

    // Reset per-second rate limiting counters
    static float rateLimitTimer = 0.0f;
    rateLimitTimer += deltaTime;
    if (rateLimitTimer >= 1.0f) {
        rateLimitTimer = 0.0f;
        m_eventCountPerSecond.clear();
    }
}

// ============================================================================
// Event Dispatch
// ============================================================================

uint64_t ReplicationSystem::DispatchEvent(NetworkEvent event) {
    if (!m_initialized) return 0;

    // Assign event ID and timestamp
    event.eventId = m_nextEventId++;
    event.timestamp = GetServerTime();
    event.sourceClientId = m_config.localClientId;

    // Get type config and apply defaults
    if (const auto* config = EventTypeRegistry::Instance().GetConfig(event.eventType)) {
        if (event.category == ReplicationCategory::Custom) {
            event.category = config->defaultCategory;
        }
        if (event.replicationMode == ReplicationMode::ToAll) {
            event.replicationMode = config->defaultReplicationMode;
        }
        if (event.persistenceMode == PersistenceMode::None) {
            event.persistenceMode = config->defaultPersistenceMode;
        }
        if (event.reliabilityMode == ReliabilityMode::Reliable) {
            event.reliabilityMode = config->defaultReliabilityMode;
        }
        if (event.priority == EventPriority::Normal) {
            event.priority = config->defaultPriority;
        }

        // Validate event
        if (config->validator && !config->validator(event)) {
            m_stats.eventsDropped++;
            return 0;
        }

        // Preprocess event
        if (config->preprocessor) {
            config->preprocessor(event);
        }
    }

    // Check rate limiting
    if (!CheckRateLimit(event.eventType, event.sourceClientId)) {
        m_stats.eventsDropped++;
        return 0;
    }

    // Validate event
    if (!ValidateEvent(event)) {
        m_stats.eventsDropped++;
        return 0;
    }

    // Process locally first
    ProcessLocalEvent(event);

    // Handle replication
    if (event.replicationMode != ReplicationMode::None) {
        SendEventToNetwork(event);
    }

    // Handle persistence
    if (event.persistenceMode != PersistenceMode::None) {
        PersistEvent(event);
    }

    if (OnEventSent) OnEventSent(event);
    m_stats.eventsSent++;
    m_stats.eventCountByType[event.eventType]++;

    return event.eventId;
}

uint64_t ReplicationSystem::DispatchWithSettings(
    const std::string& eventType,
    ReplicationMode replication,
    PersistenceMode persistence,
    const std::vector<EventProperty>& properties
) {
    NetworkEvent event;
    event.eventType = eventType;
    event.replicationMode = replication;
    event.persistenceMode = persistence;
    event.properties = properties;
    return DispatchEvent(std::move(event));
}

// ============================================================================
// Event Subscription
// ============================================================================

uint64_t ReplicationSystem::Subscribe(const std::string& eventType, EventHandler handler) {
    std::lock_guard<std::mutex> lock(m_subscriptionMutex);

    Subscription sub;
    sub.id = m_nextSubscriptionId++;
    sub.eventType = eventType;
    sub.handler = std::move(handler);
    m_subscriptions.push_back(std::move(sub));

    return sub.id;
}

uint64_t ReplicationSystem::Subscribe(ReplicationCategory category, EventHandler handler) {
    std::lock_guard<std::mutex> lock(m_subscriptionMutex);

    Subscription sub;
    sub.id = m_nextSubscriptionId++;
    sub.category = category;
    sub.categoryFilter = true;
    sub.handler = std::move(handler);
    m_subscriptions.push_back(std::move(sub));

    return sub.id;
}

uint64_t ReplicationSystem::SubscribeAll(EventHandler handler) {
    std::lock_guard<std::mutex> lock(m_subscriptionMutex);

    Subscription sub;
    sub.id = m_nextSubscriptionId++;
    sub.handler = std::move(handler);
    m_subscriptions.push_back(std::move(sub));

    return sub.id;
}

uint64_t ReplicationSystem::SubscribeFiltered(EventFilter filter, EventHandler handler) {
    std::lock_guard<std::mutex> lock(m_subscriptionMutex);

    Subscription sub;
    sub.id = m_nextSubscriptionId++;
    sub.filter = std::move(filter);
    sub.handler = std::move(handler);
    m_subscriptions.push_back(std::move(sub));

    return sub.id;
}

void ReplicationSystem::Unsubscribe(uint64_t subscriptionId) {
    std::lock_guard<std::mutex> lock(m_subscriptionMutex);

    m_subscriptions.erase(
        std::remove_if(m_subscriptions.begin(), m_subscriptions.end(),
            [subscriptionId](const Subscription& s) { return s.id == subscriptionId; }),
        m_subscriptions.end()
    );
}

// ============================================================================
// Connection Management
// ============================================================================

bool ReplicationSystem::StartHost(uint16_t port) {
    m_config.isHost = true;
    m_config.localClientId = 1;  // Host is always client ID 1

    // Start TCP channel for reliable communication
    if (g_tcpChannel && !g_tcpChannel->Listen(port)) {
        return false;
    }

    // Start UDP channel for unreliable/fast communication
    if (g_udpChannel && !g_udpChannel->Listen(port + 1)) {
        // Fallback: TCP only mode
    }

    // Add self as connection
    ConnectionInfo self;
    self.clientId = m_config.localClientId;
    self.isHost = true;
    self.isLocal = true;
    self.lastHeartbeat = GetServerTime();

    {
        std::lock_guard<std::mutex> lock(m_connectionMutex);
        m_connections[self.clientId] = self;
    }

    m_connected = true;
    return true;
}

bool ReplicationSystem::Connect(const std::string& address, uint16_t port) {
    // Connect TCP channel for reliable communication
    if (g_tcpChannel && !g_tcpChannel->Connect(address, port)) {
        return false;
    }

    // Connect UDP channel for unreliable/fast communication
    if (g_udpChannel) {
        g_udpChannel->Connect(address, port + 1);
    }

    m_connected = true;

    if (OnConnectedToHost) OnConnectedToHost();

    // Request full state sync
    RequestFullSync();

    return true;
}

void ReplicationSystem::Disconnect() {
    if (!m_connected) return;

    // Close network channels
    if (g_tcpChannel) g_tcpChannel->Close();
    if (g_udpChannel) g_udpChannel->Close();

    // Notify subscribers
    if (m_config.isHost) {
        // Notify all clients
        for (const auto& [clientId, conn] : m_connections) {
            if (!conn.isLocal && OnClientDisconnected) {
                OnClientDisconnected(clientId);
            }
        }
    } else {
        if (OnDisconnectedFromHost) OnDisconnectedFromHost();
    }

    m_connected = false;
    m_connections.clear();

    // Clear interpolation and prediction state
    if (g_snapshotInterpolator) g_snapshotInterpolator->Clear();
    if (g_clientPrediction) g_clientPrediction->Clear();
}

const ConnectionInfo* ReplicationSystem::GetConnection(uint32_t clientId) const {
    std::lock_guard<std::mutex> lock(m_connectionMutex);
    auto it = m_connections.find(clientId);
    return it != m_connections.end() ? &it->second : nullptr;
}

std::vector<ConnectionInfo> ReplicationSystem::GetAllConnections() const {
    std::lock_guard<std::mutex> lock(m_connectionMutex);
    std::vector<ConnectionInfo> result;
    result.reserve(m_connections.size());
    for (const auto& [id, conn] : m_connections) {
        result.push_back(conn);
    }
    return result;
}

// ============================================================================
// Entity Ownership (using AuthorityManager)
// ============================================================================

void ReplicationSystem::SetEntityOwner(uint64_t entityId, uint32_t clientId) {
    if (g_authorityManager) {
        g_authorityManager->SetAuthority(entityId, clientId,
            AuthorityManager::AuthorityType::ClientAuthoritative);
    }

    std::lock_guard<std::mutex> lock(m_ownershipMutex);
    m_entityOwnership[entityId] = clientId;

    // Update connection's owned entities
    std::lock_guard<std::mutex> connLock(m_connectionMutex);
    for (auto& [id, conn] : m_connections) {
        conn.ownedEntities.erase(entityId);
    }
    if (auto it = m_connections.find(clientId); it != m_connections.end()) {
        it->second.ownedEntities.insert(entityId);
    }
}

uint32_t ReplicationSystem::GetEntityOwner(uint64_t entityId) const {
    if (g_authorityManager) {
        return g_authorityManager->GetOwner(entityId);
    }

    std::lock_guard<std::mutex> lock(m_ownershipMutex);
    auto it = m_entityOwnership.find(entityId);
    return it != m_entityOwnership.end() ? it->second : 0;
}

bool ReplicationSystem::IsLocallyOwned(uint64_t entityId) const {
    return GetEntityOwner(entityId) == m_config.localClientId;
}

void ReplicationSystem::RequestOwnership(uint64_t entityId) {
    NetworkEvent event;
    event.eventType = "system.ownership.request";
    event.targetEntityId = entityId;
    event.replicationMode = ReplicationMode::ToHost;
    DispatchEvent(std::move(event));
}

// ============================================================================
// Firebase Persistence
// ============================================================================

void ReplicationSystem::SetFirebaseClient(std::shared_ptr<FirebaseClient> client) {
    m_firebaseClient = std::move(client);
}

void ReplicationSystem::PersistToFirebase(const NetworkEvent& event) {
    if (!m_firebaseClient) return;

    std::lock_guard<std::mutex> lock(m_eventMutex);
    m_firebaseQueue.push(event);
}

void ReplicationSystem::LoadFromFirebase(const std::string& path, std::function<void(std::vector<NetworkEvent>)> callback) {
    if (!m_firebaseClient) {
        callback({});
        return;
    }

    m_firebaseClient->Get(path, [callback](const FirebaseResult& result) {
        std::vector<NetworkEvent> events;

        if (result.success && result.data.is_object()) {
            for (const auto& [key, value] : result.data.items()) {
                NetworkEvent event;

                if (value.contains("eventId")) event.eventId = value["eventId"].get<uint64_t>();
                if (value.contains("sourceEntityId")) event.sourceEntityId = value["sourceEntityId"].get<uint64_t>();
                if (value.contains("targetEntityId")) event.targetEntityId = value["targetEntityId"].get<uint64_t>();
                if (value.contains("sourceClientId")) event.sourceClientId = value["sourceClientId"].get<uint32_t>();
                if (value.contains("eventType")) event.eventType = value["eventType"].get<std::string>();
                if (value.contains("category")) event.category = static_cast<ReplicationCategory>(value["category"].get<int>());
                if (value.contains("timestamp")) event.timestamp = value["timestamp"].get<uint64_t>();

                if (value.contains("properties") && value["properties"].is_object()) {
                    for (const auto& [propName, propValue] : value["properties"].items()) {
                        EventProperty prop;
                        prop.name = propName;

                        if (propValue.is_boolean()) {
                            prop.value = propValue.get<bool>();
                        } else if (propValue.is_number_integer()) {
                            prop.value = propValue.get<int64_t>();
                        } else if (propValue.is_number_float()) {
                            prop.value = propValue.get<double>();
                        } else if (propValue.is_string()) {
                            prop.value = propValue.get<std::string>();
                        } else if (propValue.is_array() && propValue.size() >= 2) {
                            if (propValue.size() == 2) {
                                prop.value = glm::vec2(propValue[0].get<float>(), propValue[1].get<float>());
                            } else if (propValue.size() == 3) {
                                prop.value = glm::vec3(propValue[0].get<float>(), propValue[1].get<float>(), propValue[2].get<float>());
                            } else if (propValue.size() == 4) {
                                prop.value = glm::vec4(propValue[0].get<float>(), propValue[1].get<float>(),
                                                       propValue[2].get<float>(), propValue[3].get<float>());
                            }
                        }

                        prop.dirty = false;
                        event.properties.push_back(std::move(prop));
                    }
                }

                events.push_back(std::move(event));
            }
        }

        callback(std::move(events));
    });
}

void ReplicationSystem::SyncTerrainToFirebase() {
    if (!m_firebaseClient) return;

    // Collect all pending terrain events and batch sync them
    std::vector<NetworkEvent> terrainEvents;

    {
        std::lock_guard<std::mutex> lock(m_eventMutex);
        std::queue<NetworkEvent> remaining;

        while (!m_firebaseQueue.empty()) {
            NetworkEvent event = std::move(m_firebaseQueue.front());
            m_firebaseQueue.pop();

            if (event.category == ReplicationCategory::Terrain) {
                terrainEvents.push_back(std::move(event));
            } else {
                remaining.push(std::move(event));
            }
        }

        m_firebaseQueue = std::move(remaining);
    }

    // Batch terrain events
    if (!terrainEvents.empty()) {
        nlohmann::json terrainBatch = nlohmann::json::object();

        for (const auto& event : terrainEvents) {
            std::string key = std::to_string(event.eventId);
            nlohmann::json eventJson;
            eventJson["eventId"] = event.eventId;
            eventJson["eventType"] = event.eventType;
            eventJson["timestamp"] = event.timestamp;

            nlohmann::json propsJson = nlohmann::json::object();
            for (const auto& prop : event.properties) {
                std::visit([&propsJson, &prop](auto&& arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, std::monostate>) {
                        propsJson[prop.name] = nullptr;
                    } else if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, int32_t> ||
                                         std::is_same_v<T, int64_t> || std::is_same_v<T, uint32_t> ||
                                         std::is_same_v<T, uint64_t> || std::is_same_v<T, float> ||
                                         std::is_same_v<T, double> || std::is_same_v<T, std::string>) {
                        propsJson[prop.name] = arg;
                    } else if constexpr (std::is_same_v<T, glm::vec2>) {
                        propsJson[prop.name] = {arg.x, arg.y};
                    } else if constexpr (std::is_same_v<T, glm::vec3>) {
                        propsJson[prop.name] = {arg.x, arg.y, arg.z};
                    } else if constexpr (std::is_same_v<T, glm::vec4>) {
                        propsJson[prop.name] = {arg.x, arg.y, arg.z, arg.w};
                    } else if constexpr (std::is_same_v<T, glm::quat>) {
                        propsJson[prop.name] = {arg.x, arg.y, arg.z, arg.w};
                    }
                }, prop.value);
            }
            eventJson["properties"] = propsJson;
            terrainBatch[key] = eventJson;
        }

        m_firebaseClient->Update("terrain/modifications", terrainBatch);
    }
}

// ============================================================================
// State Synchronization
// ============================================================================

void ReplicationSystem::RequestFullSync() {
    NetworkEvent event;
    event.eventType = "system.sync.request";
    event.replicationMode = ReplicationMode::ToHost;
    DispatchEvent(std::move(event));
}

void ReplicationSystem::SendFullStateTo(uint32_t clientId) {
    // Serialize all replicated entity states
    std::vector<uint8_t> fullState;

    // Header
    uint32_t entityCount = 0;

    // Reserve space for count
    fullState.resize(4);

    // Serialize each owned entity
    std::lock_guard<std::mutex> lock(m_ownershipMutex);
    for (const auto& [entityId, ownerId] : m_entityOwnership) {
        // Entity header
        for (int i = 0; i < 8; ++i) {
            fullState.push_back(static_cast<uint8_t>((entityId >> (i * 8)) & 0xFF));
        }
        for (int i = 0; i < 4; ++i) {
            fullState.push_back(static_cast<uint8_t>((ownerId >> (i * 8)) & 0xFF));
        }

        entityCount++;
    }

    // Write entity count
    fullState[0] = static_cast<uint8_t>(entityCount & 0xFF);
    fullState[1] = static_cast<uint8_t>((entityCount >> 8) & 0xFF);
    fullState[2] = static_cast<uint8_t>((entityCount >> 16) & 0xFF);
    fullState[3] = static_cast<uint8_t>((entityCount >> 24) & 0xFF);

    // Send via TCP (reliable)
    if (g_tcpChannel) {
        g_tcpChannel->Send(fullState, clientId, true);

        if (g_bandwidthProfiler) {
            g_bandwidthProfiler->RecordOutgoing("FullSync", fullState.size());
        }
    }
}

uint64_t ReplicationSystem::GetServerTime() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() + m_serverTimeOffset;
}

// ============================================================================
// Statistics
// ============================================================================

void ReplicationSystem::ResetStats() {
    m_stats = ReplicationStats{};
    if (g_bandwidthProfiler) {
        g_bandwidthProfiler->Reset();
    }
}

// ============================================================================
// Editor Support
// ============================================================================

std::vector<std::string> ReplicationSystem::GetRegisteredEventTypes() const {
    std::vector<std::string> types;
    // Would iterate through EventTypeRegistry
    return types;
}

void ReplicationSystem::SetEventTypeOverride(const std::string& eventType, ReplicationMode mode, PersistenceMode persistence) {
    EventTypeRegistry::Instance().SetOverride(eventType, "replicationMode", static_cast<int32_t>(mode));
    EventTypeRegistry::Instance().SetOverride(eventType, "persistenceMode", static_cast<int32_t>(persistence));
}

void ReplicationSystem::ClearEventTypeOverride(const std::string& eventType) {
    EventTypeRegistry::Instance().ClearOverrides(eventType);
}

// ============================================================================
// Private Methods
// ============================================================================

void ReplicationSystem::ProcessLocalEvent(const NetworkEvent& event) {
    std::lock_guard<std::mutex> lock(m_subscriptionMutex);

    for (const auto& sub : m_subscriptions) {
        bool matches = false;

        if (sub.filter) {
            matches = sub.filter(event);
        } else if (sub.categoryFilter) {
            matches = event.category == sub.category;
        } else if (!sub.eventType.empty()) {
            matches = event.eventType == sub.eventType;
        } else {
            matches = true;  // SubscribeAll
        }

        if (matches && sub.handler) {
            sub.handler(event);
        }
    }
}

void ReplicationSystem::ProcessRemoteEvent(const NetworkEvent& event) {
    m_stats.eventsReceived++;

    if (OnEventReceived) OnEventReceived(event);

    // Process like local event
    ProcessLocalEvent(event);
}

void ReplicationSystem::SendEventToNetwork(const NetworkEvent& event) {
    if (!m_connected) return;

    // Determine which channel to use based on reliability
    ReplicationChannel* channel = nullptr;
    bool reliable = true;

    if (event.reliabilityMode == ReliabilityMode::Unreliable) {
        channel = g_udpChannel.get();
        reliable = false;
    } else {
        channel = g_tcpChannel.get();
        reliable = true;
    }

    if (!channel || channel->GetState() != ReplicationChannel::State::Connected) {
        channel = g_tcpChannel.get(); // Fallback to TCP
    }

    if (!channel) return;

    // Serialize event
    std::vector<uint8_t> data = SerializeEvent(event);

    // Apply delta compression for state events
    if (event.category == ReplicationCategory::EntityState ||
        event.category == ReplicationCategory::EntityMovement) {
        // Get previous state for this entity if available
        auto it = m_lastEntityState.find(event.sourceEntityId);
        if (it != m_lastEntityState.end()) {
            std::vector<uint8_t> delta = DeltaCompressor::CompressDelta(it->second, data);
            if (delta.size() < data.size()) {
                data = std::move(delta);
            }
        }
        m_lastEntityState[event.sourceEntityId] = data;
    }

    switch (event.replicationMode) {
        case ReplicationMode::None:
            break;

        case ReplicationMode::ToHost:
            if (!m_config.isHost) {
                channel->Send(data, 1, reliable);  // Host is always ID 1
                if (g_bandwidthProfiler) {
                    g_bandwidthProfiler->RecordOutgoing(event.eventType, data.size());
                }
            }
            break;

        case ReplicationMode::ToClients:
            if (m_config.isHost) {
                BroadcastEventData(data, event, reliable);
            }
            break;

        case ReplicationMode::ToAll:
            BroadcastEventData(data, event, reliable);
            break;

        case ReplicationMode::ToOwner:
            if (event.targetEntityId != 0) {
                uint32_t owner = GetEntityOwner(event.targetEntityId);
                if (owner != 0 && owner != m_config.localClientId) {
                    channel->Send(data, owner, reliable);
                    if (g_bandwidthProfiler) {
                        g_bandwidthProfiler->RecordOutgoing(event.eventType, data.size());
                    }
                }
            }
            break;

        case ReplicationMode::ToServer:
            if (m_config.isDedicatedServer) {
                // Already on server
            } else {
                channel->Send(data, 1, reliable);
                if (g_bandwidthProfiler) {
                    g_bandwidthProfiler->RecordOutgoing(event.eventType, data.size());
                }
            }
            break;

        case ReplicationMode::Multicast:
            for (uint32_t clientId : event.targetClients) {
                if (clientId != m_config.localClientId) {
                    // Check interest management
                    if (g_interestManager && event.sourceEntityId != 0) {
                        // Would need entity position here
                        // For now, send to all targets
                    }
                    channel->Send(data, clientId, reliable);
                    if (g_bandwidthProfiler) {
                        g_bandwidthProfiler->RecordOutgoing(event.eventType, data.size());
                    }
                }
            }
            break;
    }

    m_stats.bytesOut += data.size();
}

void ReplicationSystem::BroadcastEventData(const std::vector<uint8_t>& data,
                                            const NetworkEvent& event, bool reliable) {
    std::lock_guard<std::mutex> lock(m_connectionMutex);

    ReplicationChannel* channel = reliable ? g_tcpChannel.get() : g_udpChannel.get();
    if (!channel) channel = g_tcpChannel.get();
    if (!channel) return;

    for (const auto& [clientId, conn] : m_connections) {
        if (!conn.isLocal) {
            channel->Send(data, clientId, reliable);
            if (g_bandwidthProfiler) {
                g_bandwidthProfiler->RecordOutgoing(event.eventType, data.size());
            }
        }
    }
}

void ReplicationSystem::BroadcastEvent(const NetworkEvent& event) {
    std::vector<uint8_t> data = SerializeEvent(event);
    bool reliable = event.reliabilityMode != ReliabilityMode::Unreliable;
    BroadcastEventData(data, event, reliable);
}

void ReplicationSystem::SendEventTo(const NetworkEvent& event, uint32_t clientId) {
    std::vector<uint8_t> data = SerializeEvent(event);
    m_stats.bytesOut += data.size();

    bool reliable = event.reliabilityMode != ReliabilityMode::Unreliable;
    ReplicationChannel* channel = reliable ? g_tcpChannel.get() : g_udpChannel.get();
    if (!channel) channel = g_tcpChannel.get();

    if (channel) {
        channel->Send(data, clientId, reliable);
        if (g_bandwidthProfiler) {
            g_bandwidthProfiler->RecordOutgoing(event.eventType, data.size());
        }
    }
}

bool ReplicationSystem::ValidateEvent(const NetworkEvent& event) const {
    // Check ownership requirements using AuthorityManager
    if (const auto* config = EventTypeRegistry::Instance().GetConfig(event.eventType)) {
        if (config->requiresOwnership && event.sourceEntityId != 0) {
            if (g_authorityManager) {
                if (!g_authorityManager->HasAuthority(event.sourceEntityId,
                        event.sourceClientId, m_config.isHost)) {
                    return false;
                }
            } else if (!IsLocallyOwned(event.sourceEntityId)) {
                return false;
            }
        }

        if (config->requiresHost && !m_config.isHost) {
            return false;
        }

        if (!config->allowFromClient && !m_config.isHost) {
            return false;
        }
    }

    return true;
}

bool ReplicationSystem::CheckRateLimit(const std::string& eventType, uint32_t clientId) {
    const auto* config = EventTypeRegistry::Instance().GetConfig(eventType);
    if (!config) return true;

    auto now = std::chrono::steady_clock::now();

    // Check minimum interval
    if (config->minInterval > 0.0f) {
        auto& lastTime = m_lastEventTime[eventType][clientId];
        auto elapsed = std::chrono::duration<float>(now - lastTime).count();
        if (elapsed < config->minInterval) {
            return false;
        }
        lastTime = now;
    }

    // Check max per second
    if (config->maxPerSecond > 0) {
        auto& count = m_eventCountPerSecond[eventType][clientId];
        if (count >= config->maxPerSecond) {
            return false;
        }
        count++;
    }

    return true;
}

std::vector<uint8_t> ReplicationSystem::SerializeEvent(const NetworkEvent& event) const {
    std::vector<uint8_t> data;

    // Simple serialization - would use proper binary format in production
    auto writeU64 = [&data](uint64_t v) {
        for (int i = 0; i < 8; i++) {
            data.push_back(static_cast<uint8_t>(v >> (i * 8)));
        }
    };

    auto writeU32 = [&data](uint32_t v) {
        for (int i = 0; i < 4; i++) {
            data.push_back(static_cast<uint8_t>(v >> (i * 8)));
        }
    };

    auto writeString = [&data, &writeU32](const std::string& s) {
        writeU32(static_cast<uint32_t>(s.size()));
        for (char c : s) {
            data.push_back(static_cast<uint8_t>(c));
        }
    };

    writeU64(event.eventId);
    writeU64(event.sourceEntityId);
    writeU64(event.targetEntityId);
    writeU32(event.sourceClientId);
    writeString(event.eventType);
    data.push_back(static_cast<uint8_t>(event.category));
    data.push_back(static_cast<uint8_t>(event.replicationMode));
    data.push_back(static_cast<uint8_t>(event.persistenceMode));
    data.push_back(static_cast<uint8_t>(event.reliabilityMode));
    data.push_back(static_cast<uint8_t>(event.priority));
    writeU64(event.timestamp);

    // Properties
    writeU32(static_cast<uint32_t>(event.properties.size()));

    auto writeFloat = [&data](float v) {
        uint32_t bits;
        memcpy(&bits, &v, sizeof(bits));
        for (int i = 0; i < 4; i++) {
            data.push_back(static_cast<uint8_t>(bits >> (i * 8)));
        }
    };

    auto writeVec3 = [&writeFloat](const glm::vec3& v) {
        writeFloat(v.x);
        writeFloat(v.y);
        writeFloat(v.z);
    };

    auto writeVec4 = [&writeFloat](const glm::vec4& v) {
        writeFloat(v.x);
        writeFloat(v.y);
        writeFloat(v.z);
        writeFloat(v.w);
    };

    for (const auto& prop : event.properties) {
        writeString(prop.name);

        // Write type index and value
        size_t typeIndex = prop.value.index();
        data.push_back(static_cast<uint8_t>(typeIndex));

        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                // Nothing to write
            } else if constexpr (std::is_same_v<T, bool>) {
                data.push_back(arg ? 1 : 0);
            } else if constexpr (std::is_same_v<T, int32_t>) {
                writeU32(static_cast<uint32_t>(arg));
            } else if constexpr (std::is_same_v<T, int64_t>) {
                writeU64(static_cast<uint64_t>(arg));
            } else if constexpr (std::is_same_v<T, uint32_t>) {
                writeU32(arg);
            } else if constexpr (std::is_same_v<T, uint64_t>) {
                writeU64(arg);
            } else if constexpr (std::is_same_v<T, float>) {
                writeFloat(arg);
            } else if constexpr (std::is_same_v<T, double>) {
                writeFloat(static_cast<float>(arg));
                writeFloat(static_cast<float>(arg)); // Extra precision lost, stored twice for alignment
            } else if constexpr (std::is_same_v<T, std::string>) {
                writeString(arg);
            } else if constexpr (std::is_same_v<T, glm::vec2>) {
                writeFloat(arg.x);
                writeFloat(arg.y);
            } else if constexpr (std::is_same_v<T, glm::vec3>) {
                writeVec3(arg);
            } else if constexpr (std::is_same_v<T, glm::vec4>) {
                writeVec4(arg);
            } else if constexpr (std::is_same_v<T, glm::quat>) {
                writeVec4(glm::vec4(arg.x, arg.y, arg.z, arg.w));
            } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                writeU32(static_cast<uint32_t>(arg.size()));
                for (uint8_t b : arg) {
                    data.push_back(b);
                }
            }
        }, prop.value);
    }

    return data;
}

NetworkEvent ReplicationSystem::DeserializeEvent(const std::vector<uint8_t>& data) const {
    NetworkEvent event;
    if (data.size() < 40) return event;  // Minimum size check

    size_t offset = 0;

    auto readU64 = [&data, &offset]() -> uint64_t {
        if (offset + 8 > data.size()) return 0;
        uint64_t v = 0;
        for (int i = 0; i < 8; i++) {
            v |= static_cast<uint64_t>(data[offset++]) << (i * 8);
        }
        return v;
    };

    auto readU32 = [&data, &offset]() -> uint32_t {
        if (offset + 4 > data.size()) return 0;
        uint32_t v = 0;
        for (int i = 0; i < 4; i++) {
            v |= static_cast<uint32_t>(data[offset++]) << (i * 8);
        }
        return v;
    };

    auto readU8 = [&data, &offset]() -> uint8_t {
        if (offset >= data.size()) return 0;
        return data[offset++];
    };

    auto readFloat = [&data, &offset]() -> float {
        if (offset + 4 > data.size()) return 0.0f;
        uint32_t bits = 0;
        for (int i = 0; i < 4; i++) {
            bits |= static_cast<uint32_t>(data[offset++]) << (i * 8);
        }
        float v;
        memcpy(&v, &bits, sizeof(v));
        return v;
    };

    auto readString = [&data, &offset, &readU32]() -> std::string {
        uint32_t len = readU32();
        if (offset + len > data.size()) return "";
        std::string s(reinterpret_cast<const char*>(&data[offset]), len);
        offset += len;
        return s;
    };

    event.eventId = readU64();
    event.sourceEntityId = readU64();
    event.targetEntityId = readU64();
    event.sourceClientId = readU32();
    event.eventType = readString();
    event.category = static_cast<ReplicationCategory>(readU8());
    event.replicationMode = static_cast<ReplicationMode>(readU8());
    event.persistenceMode = static_cast<PersistenceMode>(readU8());
    event.reliabilityMode = static_cast<ReliabilityMode>(readU8());
    event.priority = static_cast<EventPriority>(readU8());
    event.timestamp = readU64();

    // Read properties
    uint32_t propCount = readU32();
    for (uint32_t i = 0; i < propCount && offset < data.size(); i++) {
        EventProperty prop;
        prop.name = readString();
        uint8_t typeIndex = readU8();

        switch (typeIndex) {
            case 0: prop.value = std::monostate{}; break;
            case 1: prop.value = readU8() != 0; break;
            case 2: prop.value = static_cast<int32_t>(readU32()); break;
            case 3: prop.value = static_cast<int64_t>(readU64()); break;
            case 4: prop.value = readU32(); break;
            case 5: prop.value = readU64(); break;
            case 6: prop.value = readFloat(); break;
            case 7: {
                float f1 = readFloat();
                readFloat(); // Skip second float
                prop.value = static_cast<double>(f1);
                break;
            }
            case 8: prop.value = readString(); break;
            case 9: prop.value = glm::vec2(readFloat(), readFloat()); break;
            case 10: prop.value = glm::vec3(readFloat(), readFloat(), readFloat()); break;
            case 11: prop.value = glm::vec4(readFloat(), readFloat(), readFloat(), readFloat()); break;
            case 12: {
                float x = readFloat(), y = readFloat(), z = readFloat(), w = readFloat();
                prop.value = glm::quat(w, x, y, z);
                break;
            }
            case 13: {
                uint32_t size = readU32();
                std::vector<uint8_t> bytes;
                bytes.reserve(size);
                for (uint32_t j = 0; j < size && offset < data.size(); j++) {
                    bytes.push_back(data[offset++]);
                }
                prop.value = std::move(bytes);
                break;
            }
            default: break;
        }

        prop.dirty = false;
        event.properties.push_back(std::move(prop));
    }

    return event;
}

void ReplicationSystem::ProcessIncomingPackets() {
    // Process TCP channel
    if (g_tcpChannel && g_tcpChannel->GetState() == ReplicationChannel::State::Connected) {
        auto packets = g_tcpChannel->Receive();
        for (const auto& [clientId, data] : packets) {
            if (g_bandwidthProfiler) {
                g_bandwidthProfiler->RecordIncoming("TCP", data.size());
            }

            // Check for delta-compressed data
            std::vector<uint8_t> decompressed = data;
            if (data.size() >= 3 && data[0] == 0xDE && data[1] == 0x17 && data[2] == 0xA0) {
                // This is a delta - need previous state to decompress
                // For now, try to decompress with empty base
                decompressed = DeltaCompressor::DecompressDelta({}, data);
            }

            NetworkEvent event = DeserializeEvent(decompressed);
            if (!event.eventType.empty()) {
                // Update connection info
                if (m_config.isHost) {
                    std::lock_guard<std::mutex> lock(m_connectionMutex);
                    if (m_connections.find(clientId) == m_connections.end()) {
                        ConnectionInfo info;
                        info.clientId = clientId;
                        info.isHost = false;
                        info.isLocal = false;
                        info.lastHeartbeat = GetServerTime();
                        m_connections[clientId] = info;

                        if (OnClientConnected) OnClientConnected(clientId);
                    } else {
                        m_connections[clientId].lastHeartbeat = GetServerTime();
                    }
                }

                ProcessRemoteEvent(event);

                // Add to snapshot interpolator if it's a state update
                if (g_snapshotInterpolator &&
                    (event.category == ReplicationCategory::EntityState ||
                     event.category == ReplicationCategory::EntityMovement)) {
                    SnapshotInterpolator::Snapshot snapshot;
                    snapshot.tick = static_cast<uint32_t>(event.eventId);
                    snapshot.timestamp = event.timestamp;
                    snapshot.entityStates[event.sourceEntityId] = decompressed;
                    g_snapshotInterpolator->AddSnapshot(snapshot);
                }
            }
        }
    }

    // Process UDP channel
    if (g_udpChannel && g_udpChannel->GetState() == ReplicationChannel::State::Connected) {
        auto packets = g_udpChannel->Receive();
        for (const auto& [clientId, data] : packets) {
            if (g_bandwidthProfiler) {
                g_bandwidthProfiler->RecordIncoming("UDP", data.size());
            }

            NetworkEvent event = DeserializeEvent(data);
            if (!event.eventType.empty()) {
                ProcessRemoteEvent(event);
            }
        }
    }

    // Process queued incoming events (for testing/local simulation)
    std::lock_guard<std::mutex> lock(m_eventMutex);
    while (!m_incomingEvents.empty()) {
        NetworkEvent event = std::move(m_incomingEvents.front());
        m_incomingEvents.pop();
        ProcessRemoteEvent(event);
    }
}

void ReplicationSystem::SendOutgoingPackets() {
    std::lock_guard<std::mutex> lock(m_eventMutex);
    while (!m_outgoingEvents.empty()) {
        NetworkEvent event = std::move(m_outgoingEvents.front());
        m_outgoingEvents.pop();
        SendEventToNetwork(event);
    }
}

void ReplicationSystem::SendHeartbeats() {
    if (!m_connected) return;

    NetworkEvent heartbeat;
    heartbeat.eventType = "system.heartbeat";
    heartbeat.replicationMode = ReplicationMode::ToAll;
    heartbeat.reliabilityMode = ReliabilityMode::Unreliable;
    heartbeat.priority = EventPriority::Low;
    DispatchEvent(std::move(heartbeat));
}

void ReplicationSystem::CheckTimeouts() {
    if (!m_config.isHost) return;

    uint64_t now = GetServerTime();
    const uint64_t timeoutMs = 5000;  // 5 second timeout

    std::vector<uint32_t> timedOut;

    {
        std::lock_guard<std::mutex> lock(m_connectionMutex);
        for (const auto& [clientId, conn] : m_connections) {
            if (!conn.isLocal && (now - conn.lastHeartbeat) > timeoutMs) {
                timedOut.push_back(clientId);
            }
        }

        for (uint32_t clientId : timedOut) {
            m_connections.erase(clientId);

            // Clean up interest manager
            if (g_interestManager) {
                g_interestManager->RemoveClient(clientId);
            }
        }
    }

    for (uint32_t clientId : timedOut) {
        if (OnClientDisconnected) OnClientDisconnected(clientId);
    }
}

void ReplicationSystem::PersistEvent(const NetworkEvent& event) {
    switch (event.persistenceMode) {
        case PersistenceMode::None:
            break;

        case PersistenceMode::Firebase:
            PersistToFirebase(event);
            break;

        case PersistenceMode::LocalFile:
            PersistToLocalFile(event);
            break;

        case PersistenceMode::Both:
            PersistToFirebase(event);
            PersistToLocalFile(event);
            break;
    }

    m_stats.eventsPersisted++;
    if (OnEventPersisted) OnEventPersisted(event);
}

void ReplicationSystem::PersistToLocalFile(const NetworkEvent& event) {
    // Serialize event to JSON
    nlohmann::json eventJson;
    eventJson["eventId"] = event.eventId;
    eventJson["sourceEntityId"] = event.sourceEntityId;
    eventJson["targetEntityId"] = event.targetEntityId;
    eventJson["sourceClientId"] = event.sourceClientId;
    eventJson["eventType"] = event.eventType;
    eventJson["category"] = static_cast<int>(event.category);
    eventJson["timestamp"] = event.timestamp;

    nlohmann::json propsJson = nlohmann::json::object();
    for (const auto& prop : event.properties) {
        std::visit([&propsJson, &prop](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                propsJson[prop.name] = nullptr;
            } else if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, int32_t> ||
                                 std::is_same_v<T, int64_t> || std::is_same_v<T, uint32_t> ||
                                 std::is_same_v<T, uint64_t> || std::is_same_v<T, float> ||
                                 std::is_same_v<T, double> || std::is_same_v<T, std::string>) {
                propsJson[prop.name] = arg;
            } else if constexpr (std::is_same_v<T, glm::vec2>) {
                propsJson[prop.name] = {arg.x, arg.y};
            } else if constexpr (std::is_same_v<T, glm::vec3>) {
                propsJson[prop.name] = {arg.x, arg.y, arg.z};
            } else if constexpr (std::is_same_v<T, glm::vec4>) {
                propsJson[prop.name] = {arg.x, arg.y, arg.z, arg.w};
            } else if constexpr (std::is_same_v<T, glm::quat>) {
                propsJson[prop.name] = {arg.x, arg.y, arg.z, arg.w};
            } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                propsJson[prop.name] = nlohmann::json::array();
                for (uint8_t b : arg) {
                    propsJson[prop.name].push_back(b);
                }
            }
        }, prop.value);
    }
    eventJson["properties"] = propsJson;

    // Append to local persistence file
    std::string filename = "persistence/" + event.eventType + ".json";

    // Load existing events
    nlohmann::json allEvents = nlohmann::json::array();
    std::ifstream inFile(filename);
    if (inFile.good()) {
        try {
            inFile >> allEvents;
        } catch (...) {
            allEvents = nlohmann::json::array();
        }
    }
    inFile.close();

    // Add new event
    allEvents.push_back(eventJson);

    // Write back
    std::ofstream outFile(filename);
    if (outFile.good()) {
        outFile << allEvents.dump(2);
    }
}

void ReplicationSystem::ProcessFirebaseQueue() {
    if (!m_firebaseClient) return;

    // Process a few events per frame
    std::lock_guard<std::mutex> lock(m_eventMutex);
    int processed = 0;
    while (!m_firebaseQueue.empty() && processed < 5) {
        NetworkEvent event = std::move(m_firebaseQueue.front());
        m_firebaseQueue.pop();

        // Build Firebase path based on event type
        std::string path = "events/" + event.eventType + "/" + std::to_string(event.eventId);

        // Serialize event to JSON
        nlohmann::json eventJson;
        eventJson["eventId"] = event.eventId;
        eventJson["sourceEntityId"] = event.sourceEntityId;
        eventJson["targetEntityId"] = event.targetEntityId;
        eventJson["sourceClientId"] = event.sourceClientId;
        eventJson["eventType"] = event.eventType;
        eventJson["category"] = static_cast<int>(event.category);
        eventJson["timestamp"] = event.timestamp;

        // Serialize properties
        nlohmann::json propsJson = nlohmann::json::object();
        for (const auto& prop : event.properties) {
            std::visit([&propsJson, &prop](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::monostate>) {
                    propsJson[prop.name] = nullptr;
                } else if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, int32_t> ||
                                     std::is_same_v<T, int64_t> || std::is_same_v<T, uint32_t> ||
                                     std::is_same_v<T, uint64_t> || std::is_same_v<T, float> ||
                                     std::is_same_v<T, double> || std::is_same_v<T, std::string>) {
                    propsJson[prop.name] = arg;
                } else if constexpr (std::is_same_v<T, glm::vec2>) {
                    propsJson[prop.name] = {arg.x, arg.y};
                } else if constexpr (std::is_same_v<T, glm::vec3>) {
                    propsJson[prop.name] = {arg.x, arg.y, arg.z};
                } else if constexpr (std::is_same_v<T, glm::vec4>) {
                    propsJson[prop.name] = {arg.x, arg.y, arg.z, arg.w};
                } else if constexpr (std::is_same_v<T, glm::quat>) {
                    propsJson[prop.name] = {arg.x, arg.y, arg.z, arg.w};
                } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                    propsJson[prop.name] = nlohmann::json::array();
                }
            }, prop.value);
        }
        eventJson["properties"] = propsJson;

        // Push to Firebase asynchronously
        m_firebaseClient->Set(path, eventJson, [](const FirebaseResult& result) {
            if (!result.success) {
                (void)result.errorMessage; // Logging would go here
            }
        });

        processed++;
    }
}

// ============================================================================
// Default Event Type Registration
// ============================================================================

void ReplicationSystem::RegisterDefaultEventTypes() {
    auto& registry = EventTypeRegistry::Instance();

    // Input events - high frequency, unreliable, no persistence
    registry.RegisterType({
        Events::INPUT_MOVE,
        ReplicationCategory::Input,
        ReplicationMode::ToHost,
        PersistenceMode::None,
        ReliabilityMode::Unreliable,
        EventPriority::High,
        0.0f, 60,  // Max 60 per second
        true, false, true
    });

    registry.RegisterType({
        Events::INPUT_LOOK,
        ReplicationCategory::Input,
        ReplicationMode::ToHost,
        PersistenceMode::None,
        ReliabilityMode::Unreliable,
        EventPriority::Normal,
        0.0f, 30,
        true, false, true
    });

    registry.RegisterType({
        Events::INPUT_ACTION,
        ReplicationCategory::Input,
        ReplicationMode::ToHost,
        PersistenceMode::None,
        ReliabilityMode::Reliable,
        EventPriority::High,
        0.0f, 20,
        false, false, true
    });

    // Entity events - reliable
    registry.RegisterType({
        Events::ENTITY_SPAWN,
        ReplicationCategory::EntitySpawn,
        ReplicationMode::ToClients,
        PersistenceMode::None,
        ReliabilityMode::ReliableOrdered,
        EventPriority::High,
        0.0f, 0,
        false, true, false  // Host only
    });

    registry.RegisterType({
        Events::ENTITY_DESTROY,
        ReplicationCategory::EntitySpawn,
        ReplicationMode::ToClients,
        PersistenceMode::None,
        ReliabilityMode::ReliableOrdered,
        EventPriority::High,
        0.0f, 0,
        false, true, false
    });

    registry.RegisterType({
        Events::ENTITY_MOVE,
        ReplicationCategory::EntityMovement,
        ReplicationMode::ToAll,
        PersistenceMode::None,
        ReliabilityMode::Unreliable,
        EventPriority::Normal,
        0.0f, 30,
        true, false, true
    });

    registry.RegisterType({
        Events::ENTITY_STATE,
        ReplicationCategory::EntityState,
        ReplicationMode::ToAll,
        PersistenceMode::None,
        ReliabilityMode::Reliable,
        EventPriority::Normal,
        0.0f, 0,
        true, false, true
    });

    // Combat events
    registry.RegisterType({
        Events::COMBAT_ATTACK,
        ReplicationCategory::Combat,
        ReplicationMode::ToHost,
        PersistenceMode::None,
        ReliabilityMode::Reliable,
        EventPriority::High,
        0.1f, 10,  // Rate limited
        true, false, true
    });

    registry.RegisterType({
        Events::COMBAT_DAMAGE,
        ReplicationCategory::Combat,
        ReplicationMode::ToClients,
        PersistenceMode::None,
        ReliabilityMode::Reliable,
        EventPriority::High,
        0.0f, 0,
        false, true, false  // Host only
    });

    // Ability events
    registry.RegisterType({
        Events::ABILITY_USE,
        ReplicationCategory::Abilities,
        ReplicationMode::ToHost,
        PersistenceMode::None,
        ReliabilityMode::Reliable,
        EventPriority::High,
        0.0f, 0,
        true, false, true
    });

    // Building events
    registry.RegisterType({
        Events::BUILDING_PLACE,
        ReplicationCategory::Building,
        ReplicationMode::ToHost,
        PersistenceMode::None,
        ReliabilityMode::Reliable,
        EventPriority::Normal,
        0.5f, 2,
        true, false, true
    });

    registry.RegisterType({
        Events::BUILDING_COMPLETE,
        ReplicationCategory::Building,
        ReplicationMode::ToClients,
        PersistenceMode::None,
        ReliabilityMode::ReliableOrdered,
        EventPriority::Normal,
        0.0f, 0,
        false, true, false
    });

    // Terrain events - PERSIST TO FIREBASE
    registry.RegisterType({
        Events::TERRAIN_MODIFY,
        ReplicationCategory::Terrain,
        ReplicationMode::ToClients,
        PersistenceMode::Firebase,  // <-- Key: persist terrain changes
        ReliabilityMode::ReliableOrdered,
        EventPriority::Normal,
        0.1f, 10,
        false, true, false  // Host only can modify
    });

    registry.RegisterType({
        Events::TERRAIN_SCULPT,
        ReplicationCategory::Terrain,
        ReplicationMode::ToClients,
        PersistenceMode::Firebase,
        ReliabilityMode::Reliable,
        EventPriority::Normal,
        0.05f, 20,
        false, true, false
    });

    registry.RegisterType({
        Events::TERRAIN_TUNNEL,
        ReplicationCategory::Terrain,
        ReplicationMode::ToClients,
        PersistenceMode::Firebase,
        ReliabilityMode::ReliableOrdered,
        EventPriority::Normal,
        0.5f, 2,
        false, true, false
    });

    registry.RegisterType({
        Events::TERRAIN_CAVE,
        ReplicationCategory::Terrain,
        ReplicationMode::ToClients,
        PersistenceMode::Firebase,
        ReliabilityMode::ReliableOrdered,
        EventPriority::Normal,
        1.0f, 1,
        false, true, false
    });

    // Progression events - reliable, no persistence (fetch from host)
    registry.RegisterType({
        Events::PROGRESSION_XP,
        ReplicationCategory::Progression,
        ReplicationMode::ToOwner,
        PersistenceMode::None,
        ReliabilityMode::Reliable,
        EventPriority::Normal,
        0.0f, 0,
        false, true, false
    });

    registry.RegisterType({
        Events::PROGRESSION_LEVEL,
        ReplicationCategory::Progression,
        ReplicationMode::ToAll,
        PersistenceMode::None,
        ReliabilityMode::Reliable,
        EventPriority::High,
        0.0f, 0,
        false, true, false
    });

    // Chat events
    registry.RegisterType({
        Events::CHAT_MESSAGE,
        ReplicationCategory::Chat,
        ReplicationMode::ToAll,
        PersistenceMode::None,
        ReliabilityMode::ReliableOrdered,
        EventPriority::Normal,
        0.1f, 10,
        false, false, true
    });

    // Game state events
    registry.RegisterType({
        Events::GAME_START,
        ReplicationCategory::GameState,
        ReplicationMode::ToClients,
        PersistenceMode::None,
        ReliabilityMode::ReliableOrdered,
        EventPriority::Critical,
        0.0f, 0,
        false, true, false
    });
}

} // namespace Nova
