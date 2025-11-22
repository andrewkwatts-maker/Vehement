#include "NetworkTransport.hpp"
#include "../firebase/FirebaseRealtime.hpp"
#include <random>
#include <algorithm>
#include <cstring>

namespace Network {
namespace Replication {

std::shared_ptr<NetworkTransport> NetworkTransport::create() {
    return std::make_shared<NetworkTransport>();
}

NetworkTransport::NetworkTransport() {
    m_lastQualityUpdate = std::chrono::steady_clock::now();
}

NetworkTransport::~NetworkTransport() {
    shutdown();
}

bool NetworkTransport::initialize() {
    if (m_initialized) return true;

    // Create default channels
    TransportChannel reliable;
    reliable.name = "reliable";
    reliable.reliable = true;
    reliable.ordered = true;
    reliable.priority = 0;
    createChannel(reliable);

    TransportChannel unreliable;
    unreliable.name = "unreliable";
    unreliable.reliable = false;
    unreliable.ordered = false;
    unreliable.priority = 1;
    createChannel(unreliable);

    m_initialized = true;
    return true;
}

void NetworkTransport::shutdown() {
    if (!m_initialized) return;

    disconnectAll();

    m_initialized = false;
}

void NetworkTransport::update(float deltaTime) {
    if (!m_initialized) return;

    // Process outgoing packets
    processOutgoingPackets();

    // Process incoming packets
    processIncomingPackets();

    // Retransmit unacked reliable packets
    for (auto& [peerId, info] : m_peers) {
        retransmitPackets(peerId);
    }

    // Update connection quality periodically
    auto now = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration<float>(now - m_lastQualityUpdate).count();
    if (elapsed >= QUALITY_UPDATE_INTERVAL) {
        m_lastQualityUpdate = now;

        for (auto& [peerId, info] : m_peers) {
            updateConnectionQuality(peerId);
        }
    }
}

void NetworkTransport::connect(const std::string& address, uint16_t port) {
    // This would implement actual socket connection
    // For now, create a peer entry

    uint64_t peerId = std::hash<std::string>{}(address + ":" + std::to_string(port));

    PeerInfo info;
    info.peerId = peerId;
    info.address = address;
    info.port = port;
    info.state = TransportState::Connecting;
    info.connectedAt = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(m_peerMutex);
        m_peers[peerId] = info;
    }

    // Notify callbacks
    for (auto& callback : m_connectionCallbacks) {
        callback(peerId, TransportState::Connecting);
    }

    // Simulate connection success
    {
        std::lock_guard<std::mutex> lock(m_peerMutex);
        m_peers[peerId].state = TransportState::Connected;
    }

    for (auto& callback : m_connectionCallbacks) {
        callback(peerId, TransportState::Connected);
    }
}

void NetworkTransport::connectToPeer(uint64_t peerId, const std::string& signalingData) {
    // WebRTC peer connection via signaling
    PeerInfo info;
    info.peerId = peerId;
    info.state = TransportState::Connecting;
    info.connectedAt = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(m_peerMutex);
        m_peers[peerId] = info;
    }

    // In real implementation, this would:
    // 1. Parse signaling data (SDP offer/answer)
    // 2. Set up WebRTC peer connection
    // 3. Handle ICE candidates
}

void NetworkTransport::disconnect(uint64_t peerId) {
    std::lock_guard<std::mutex> lock(m_peerMutex);

    auto it = m_peers.find(peerId);
    if (it == m_peers.end()) return;

    it->second.state = TransportState::Disconnected;

    for (auto& callback : m_connectionCallbacks) {
        callback(peerId, TransportState::Disconnected);
    }

    m_peers.erase(it);
    m_outgoingQueues.erase(peerId);
    m_incomingQueues.erase(peerId);
    m_unackedPackets.erase(peerId);
    m_connectionQuality.erase(peerId);
}

void NetworkTransport::disconnectAll() {
    std::vector<uint64_t> peerIds;
    {
        std::lock_guard<std::mutex> lock(m_peerMutex);
        for (const auto& [peerId, info] : m_peers) {
            peerIds.push_back(peerId);
        }
    }

    for (uint64_t peerId : peerIds) {
        disconnect(peerId);
    }
}

bool NetworkTransport::isConnected(uint64_t peerId) const {
    std::lock_guard<std::mutex> lock(m_peerMutex);

    if (peerId == 0) {
        return !m_peers.empty();
    }

    auto it = m_peers.find(peerId);
    return it != m_peers.end() && it->second.state == TransportState::Connected;
}

TransportState NetworkTransport::getState(uint64_t peerId) const {
    std::lock_guard<std::mutex> lock(m_peerMutex);

    if (peerId == 0 && !m_peers.empty()) {
        return m_peers.begin()->second.state;
    }

    auto it = m_peers.find(peerId);
    if (it == m_peers.end()) return TransportState::Disconnected;
    return it->second.state;
}

std::vector<uint64_t> NetworkTransport::getConnectedPeers() const {
    std::lock_guard<std::mutex> lock(m_peerMutex);

    std::vector<uint64_t> result;
    for (const auto& [peerId, info] : m_peers) {
        if (info.state == TransportState::Connected) {
            result.push_back(peerId);
        }
    }
    return result;
}

PeerInfo NetworkTransport::getPeerInfo(uint64_t peerId) const {
    std::lock_guard<std::mutex> lock(m_peerMutex);

    auto it = m_peers.find(peerId);
    if (it == m_peers.end()) return PeerInfo();
    return it->second;
}

void NetworkTransport::send(const std::vector<uint8_t>& data, const std::string& channel) {
    // Send to all connected peers
    broadcast(data, channel);
}

void NetworkTransport::sendTo(uint64_t peerId, const std::vector<uint8_t>& data,
                               const std::string& channel) {
    if (!isConnected(peerId)) return;

    const TransportChannel* ch = getChannel(channel);
    if (!ch) ch = getChannel(m_defaultChannel);
    if (!ch) return;

    NetworkPacket packet;
    packet.sequenceNumber = m_nextSequenceNumber[peerId]++;
    packet.channel = channel;
    packet.data = data;
    packet.timestamp = std::chrono::steady_clock::now();
    packet.isReliable = ch->reliable;
    packet.isOrdered = ch->ordered;

    // Apply simulation if enabled
    if (m_simulateEnabled && shouldDropPacket()) {
        m_stats.packetsLost++;
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_outgoingQueues[peerId].push(packet);
    }

    if (packet.isReliable) {
        queueReliablePacket(peerId, packet);
    }

    m_stats.packetsSent++;
    m_stats.bytesSent += data.size();
}

void NetworkTransport::broadcast(const std::vector<uint8_t>& data, const std::string& channel) {
    std::vector<uint64_t> peers = getConnectedPeers();
    for (uint64_t peerId : peers) {
        sendTo(peerId, data, channel);
    }
}

bool NetworkTransport::receive(std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(m_queueMutex);

    if (m_receivedData.empty()) return false;

    data = m_receivedData.front().second;
    m_receivedData.pop();
    return true;
}

bool NetworkTransport::receiveFrom(uint64_t peerId, std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(m_queueMutex);

    auto it = m_incomingQueues.find(peerId);
    if (it == m_incomingQueues.end() || it->second.empty()) return false;

    data = it->second.front().data;
    it->second.pop();

    m_stats.packetsReceived++;
    m_stats.bytesReceived += data.size();

    return true;
}

void NetworkTransport::createChannel(const TransportChannel& channel) {
    m_channels[channel.name] = channel;
}

void NetworkTransport::setDefaultChannel(const std::string& channelName) {
    if (m_channels.find(channelName) != m_channels.end()) {
        m_defaultChannel = channelName;
    }
}

const TransportChannel* NetworkTransport::getChannel(const std::string& name) const {
    auto it = m_channels.find(name);
    if (it == m_channels.end()) return nullptr;
    return &it->second;
}

ConnectionQuality NetworkTransport::getConnectionQuality(uint64_t peerId) const {
    auto it = m_connectionQuality.find(peerId);
    if (it != m_connectionQuality.end()) return it->second;

    // Return average quality if no specific peer
    if (peerId == 0 && !m_connectionQuality.empty()) {
        return m_connectionQuality.begin()->second;
    }

    return ConnectionQuality();
}

int NetworkTransport::getLatency(uint64_t peerId) const {
    return getConnectionQuality(peerId).latency;
}

int NetworkTransport::getPacketLoss(uint64_t peerId) const {
    return getConnectionQuality(peerId).packetLoss;
}

void NetworkTransport::simulateLatency(int minMs, int maxMs) {
    m_simulateEnabled = true;
    m_simulateMinLatency = minMs;
    m_simulateMaxLatency = maxMs;
}

void NetworkTransport::simulatePacketLoss(float percentage) {
    m_simulateEnabled = true;
    m_simulatePacketLoss = percentage;
}

void NetworkTransport::simulateJitter(int ms) {
    m_simulateEnabled = true;
    m_simulateJitter = ms;
}

void NetworkTransport::clearSimulation() {
    m_simulateEnabled = false;
    m_simulateMinLatency = 0;
    m_simulateMaxLatency = 0;
    m_simulatePacketLoss = 0.0f;
    m_simulateJitter = 0;
}

void NetworkTransport::createOffer(std::function<void(const SessionDescription&)> callback) {
    // WebRTC: Create SDP offer
    SessionDescription offer;
    offer.type = SessionDescription::Type::Offer;
    offer.sdp = "v=0\r\n...";  // Would be actual SDP

    if (callback) {
        callback(offer);
    }
}

void NetworkTransport::createAnswer(const SessionDescription& offer,
                                     std::function<void(const SessionDescription&)> callback) {
    // WebRTC: Create SDP answer from offer
    SessionDescription answer;
    answer.type = SessionDescription::Type::Answer;
    answer.sdp = "v=0\r\n...";  // Would be actual SDP

    if (callback) {
        callback(answer);
    }
}

void NetworkTransport::setRemoteDescription(const SessionDescription& desc) {
    // WebRTC: Set remote description
}

void NetworkTransport::addIceCandidate(const ICECandidate& candidate) {
    // WebRTC: Add ICE candidate
}

void NetworkTransport::onIceCandidate(std::function<void(const ICECandidate&)> callback) {
    m_iceCandidateCallback = callback;
}

void NetworkTransport::enableFirebaseRelay(bool enabled) {
    m_useFirebaseRelay = enabled;
}

void NetworkTransport::setFirebaseRelayPath(const std::string& path) {
    m_firebaseRelayPath = path;
}

void NetworkTransport::onConnection(ConnectionCallback callback) {
    m_connectionCallbacks.push_back(callback);
}

void NetworkTransport::onData(DataCallback callback) {
    m_dataCallbacks.push_back(callback);
}

void NetworkTransport::onError(ErrorCallback callback) {
    m_errorCallbacks.push_back(callback);
}

void NetworkTransport::onQualityChange(QualityCallback callback) {
    m_qualityCallbacks.push_back(callback);
}

void NetworkTransport::resetStats() {
    m_stats = TransportStats();
}

// Protected methods

void NetworkTransport::processOutgoingPackets() {
    std::lock_guard<std::mutex> lock(m_queueMutex);

    for (auto& [peerId, queue] : m_outgoingQueues) {
        while (!queue.empty()) {
            NetworkPacket& packet = queue.front();

            // Apply simulated latency
            if (m_simulateEnabled) {
                int latency = getSimulatedLatency();
                auto sendTime = packet.timestamp + std::chrono::milliseconds(latency);
                if (std::chrono::steady_clock::now() < sendTime) {
                    break;  // Not time to send yet
                }
            }

            // Actually send the packet
            // In real implementation, this would use sockets/WebRTC
            // For Firebase relay:
            if (m_useFirebaseRelay) {
                // Send via Firebase Realtime Database
                // Firebase::FirebaseRealtime::getInstance().set(...);
            }

            queue.pop();
        }
    }
}

void NetworkTransport::processIncomingPackets() {
    // In real implementation, this would receive from sockets/WebRTC
    // For Firebase relay:
    if (m_useFirebaseRelay) {
        // Receive via Firebase Realtime Database
        // Firebase::FirebaseRealtime::getInstance().get(...);
    }

    // Process any received data
    std::lock_guard<std::mutex> lock(m_queueMutex);

    for (auto& [peerId, queue] : m_incomingQueues) {
        while (!queue.empty()) {
            NetworkPacket& packet = queue.front();

            // Handle ordered delivery
            const TransportChannel* channel = getChannel(packet.channel);
            if (channel && channel->ordered) {
                if (packet.sequenceNumber != m_expectedSequenceNumber[peerId]) {
                    break;  // Out of order, wait
                }
                m_expectedSequenceNumber[peerId]++;
            }

            // Acknowledge reliable packets
            if (packet.isReliable) {
                acknowledgePacket(peerId, packet.sequenceNumber);
            }

            // Deliver to application
            m_receivedData.push({peerId, packet.data});

            // Notify callbacks
            for (auto& callback : m_dataCallbacks) {
                callback(peerId, packet.data);
            }

            // Update peer info
            {
                auto it = m_peers.find(peerId);
                if (it != m_peers.end()) {
                    it->second.lastReceived = std::chrono::steady_clock::now();
                }
            }

            queue.pop();
        }
    }
}

void NetworkTransport::processAcks(uint64_t peerId, uint64_t ackNumber, uint32_t ackBitfield) {
    auto it = m_unackedPackets.find(peerId);
    if (it == m_unackedPackets.end()) return;

    auto& unacked = it->second;

    // Remove acked packet
    unacked.erase(ackNumber);

    // Process selective acks
    for (int i = 0; i < 32; i++) {
        if (ackBitfield & (1 << i)) {
            unacked.erase(ackNumber - i - 1);
        }
    }
}

void NetworkTransport::retransmitPackets(uint64_t peerId) {
    auto it = m_unackedPackets.find(peerId);
    if (it == m_unackedPackets.end()) return;

    auto now = std::chrono::steady_clock::now();

    for (auto& [seq, packet] : it->second) {
        const TransportChannel* channel = getChannel(packet.channel);
        int retransmitDelay = channel ? channel->retransmitDelay : 100;
        int maxRetransmits = channel ? channel->maxRetransmits : 5;

        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - packet.timestamp).count();

        if (age > retransmitDelay * (packet.retransmitCount + 1)) {
            if (packet.retransmitCount < maxRetransmits) {
                packet.retransmitCount++;
                packet.timestamp = now;

                // Re-queue for sending
                {
                    std::lock_guard<std::mutex> lock(m_queueMutex);
                    m_outgoingQueues[peerId].push(packet);
                }

                m_stats.packetsRetransmitted++;
            } else {
                // Max retransmits exceeded - consider connection lost
                m_stats.packetsLost++;
            }
        }
    }
}

void NetworkTransport::queueReliablePacket(uint64_t peerId, const NetworkPacket& packet) {
    m_unackedPackets[peerId][packet.sequenceNumber] = packet;
}

void NetworkTransport::acknowledgePacket(uint64_t peerId, uint64_t sequenceNumber) {
    // Send ACK packet
    NetworkPacket ack;
    ack.sequenceNumber = m_nextSequenceNumber[peerId]++;
    ack.ackNumber = sequenceNumber;
    ack.ackBitfield = 0;  // Could include selective acks
    ack.isReliable = false;
    ack.timestamp = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_outgoingQueues[peerId].push(ack);
    }
}

void NetworkTransport::updateConnectionQuality(uint64_t peerId) {
    ConnectionQuality& quality = m_connectionQuality[peerId];

    // Calculate latency from RTT
    auto it = m_peers.find(peerId);
    if (it != m_peers.end()) {
        quality.latency = it->second.roundTripTime;
        quality.jitter = it->second.jitter;
    }

    // Calculate packet loss
    if (m_stats.packetsSent > 0) {
        quality.packetLoss = static_cast<int>(
            (m_stats.packetsLost * 100) / m_stats.packetsSent);
    }

    // Determine rating
    if (quality.latency < 50 && quality.packetLoss < 1) {
        quality.rating = ConnectionQuality::Rating::Excellent;
    } else if (quality.latency < 100 && quality.packetLoss < 3) {
        quality.rating = ConnectionQuality::Rating::Good;
    } else if (quality.latency < 150 && quality.packetLoss < 5) {
        quality.rating = ConnectionQuality::Rating::Fair;
    } else if (quality.latency < 200 && quality.packetLoss < 10) {
        quality.rating = ConnectionQuality::Rating::Poor;
    } else {
        quality.rating = ConnectionQuality::Rating::Bad;
    }

    // Notify callbacks
    for (auto& callback : m_qualityCallbacks) {
        callback(peerId, quality);
    }
}

void NetworkTransport::calculateRTT(uint64_t peerId, const NetworkPacket& packet) {
    auto now = std::chrono::steady_clock::now();
    int rtt = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
        now - packet.timestamp).count());

    auto it = m_peers.find(peerId);
    if (it != m_peers.end()) {
        // Exponential moving average
        it->second.roundTripTime = (it->second.roundTripTime * 7 + rtt) / 8;
    }
}

bool NetworkTransport::shouldDropPacket() const {
    if (m_simulatePacketLoss <= 0.0f) return false;

    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dis(0.0f, 100.0f);

    return dis(gen) < m_simulatePacketLoss;
}

int NetworkTransport::getSimulatedLatency() const {
    if (m_simulateMinLatency >= m_simulateMaxLatency) {
        return m_simulateMinLatency;
    }

    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(m_simulateMinLatency, m_simulateMaxLatency);

    int latency = dis(gen);

    // Add jitter
    if (m_simulateJitter > 0) {
        std::uniform_int_distribution<int> jitterDis(-m_simulateJitter, m_simulateJitter);
        latency += jitterDis(gen);
    }

    return std::max(0, latency);
}

} // namespace Replication
} // namespace Network
