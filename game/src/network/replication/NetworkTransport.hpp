#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <chrono>

namespace Network {
namespace Replication {

// Connection state
enum class TransportState {
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
    Error
};

// Peer information
struct PeerInfo {
    uint64_t peerId;
    std::string address;
    uint16_t port;
    TransportState state;
    std::chrono::steady_clock::time_point connectedAt;
    std::chrono::steady_clock::time_point lastReceived;
    int roundTripTime;  // ms
    int packetLoss;     // percentage
    int jitter;         // ms
};

// Connection quality metrics
struct ConnectionQuality {
    int latency = 0;           // ms
    int packetLoss = 0;        // percentage
    int jitter = 0;            // ms
    float bandwidth = 0.0f;    // bytes/s
    int outOfOrderPackets = 0;
    int duplicatePackets = 0;

    enum class Rating {
        Excellent,  // <50ms, <1% loss
        Good,       // <100ms, <3% loss
        Fair,       // <150ms, <5% loss
        Poor,       // <200ms, <10% loss
        Bad         // >200ms or >10% loss
    } rating = Rating::Good;
};

// Packet for transmission
struct NetworkPacket {
    uint64_t sequenceNumber;
    uint64_t ackNumber;
    uint32_t ackBitfield;      // Selective ACKs
    std::string channel;
    std::vector<uint8_t> data;
    std::chrono::steady_clock::time_point timestamp;
    bool isReliable;
    bool isOrdered;
    int retransmitCount = 0;
};

// Channel configuration
struct TransportChannel {
    std::string name;
    bool reliable = false;
    bool ordered = false;
    int priority = 0;
    int maxRetransmits = 5;
    int retransmitDelay = 100;  // ms
};

// ICE candidate for WebRTC
struct ICECandidate {
    std::string candidate;
    std::string sdpMid;
    int sdpMLineIndex;
};

// Session description for WebRTC
struct SessionDescription {
    enum class Type { Offer, Answer, Pranswer };
    Type type;
    std::string sdp;
};

// Callbacks
using ConnectionCallback = std::function<void(uint64_t peerId, TransportState state)>;
using DataCallback = std::function<void(uint64_t peerId, const std::vector<uint8_t>& data)>;
using ErrorCallback = std::function<void(uint64_t peerId, const std::string& error)>;
using QualityCallback = std::function<void(uint64_t peerId, const ConnectionQuality& quality)>;

/**
 * NetworkTransport - Transport layer for networking
 *
 * Features:
 * - WebRTC for peer-to-peer
 * - Firebase Realtime as relay fallback
 * - UDP-like unreliable channel
 * - TCP-like reliable channel
 * - Connection quality monitoring
 */
class NetworkTransport {
public:
    static std::shared_ptr<NetworkTransport> create();

    NetworkTransport();
    virtual ~NetworkTransport();

    // Initialization
    virtual bool initialize();
    virtual void shutdown();
    virtual void update(float deltaTime);

    // Connection management
    virtual void connect(const std::string& address, uint16_t port);
    virtual void connectToPeer(uint64_t peerId, const std::string& signalingData);
    virtual void disconnect(uint64_t peerId = 0);
    virtual void disconnectAll();
    virtual bool isConnected(uint64_t peerId = 0) const;
    virtual TransportState getState(uint64_t peerId = 0) const;

    // Peer management
    virtual std::vector<uint64_t> getConnectedPeers() const;
    virtual PeerInfo getPeerInfo(uint64_t peerId) const;
    virtual void setPeerId(uint64_t localPeerId) { m_localPeerId = localPeerId; }
    virtual uint64_t getLocalPeerId() const { return m_localPeerId; }

    // Data transmission
    virtual void send(const std::vector<uint8_t>& data, const std::string& channel = "default");
    virtual void sendTo(uint64_t peerId, const std::vector<uint8_t>& data, const std::string& channel = "default");
    virtual void broadcast(const std::vector<uint8_t>& data, const std::string& channel = "default");
    virtual bool receive(std::vector<uint8_t>& data);
    virtual bool receiveFrom(uint64_t peerId, std::vector<uint8_t>& data);

    // Channels
    virtual void createChannel(const TransportChannel& channel);
    virtual void setDefaultChannel(const std::string& channelName);
    virtual const TransportChannel* getChannel(const std::string& name) const;

    // Connection quality
    virtual ConnectionQuality getConnectionQuality(uint64_t peerId = 0) const;
    virtual int getLatency(uint64_t peerId = 0) const;
    virtual int getPacketLoss(uint64_t peerId = 0) const;
    virtual void setMaxLatency(int ms) { m_maxLatency = ms; }
    virtual void setTargetBandwidth(float bytesPerSecond) { m_targetBandwidth = bytesPerSecond; }

    // Simulation (for debugging)
    virtual void simulateLatency(int minMs, int maxMs);
    virtual void simulatePacketLoss(float percentage);
    virtual void simulateJitter(int ms);
    virtual void clearSimulation();

    // WebRTC signaling
    virtual void createOffer(std::function<void(const SessionDescription&)> callback);
    virtual void createAnswer(const SessionDescription& offer,
                              std::function<void(const SessionDescription&)> callback);
    virtual void setRemoteDescription(const SessionDescription& desc);
    virtual void addIceCandidate(const ICECandidate& candidate);
    virtual void onIceCandidate(std::function<void(const ICECandidate&)> callback);

    // Firebase relay fallback
    virtual void enableFirebaseRelay(bool enabled);
    virtual bool isUsingFirebaseRelay() const { return m_useFirebaseRelay; }
    virtual void setFirebaseRelayPath(const std::string& path);

    // Callbacks
    virtual void onConnection(ConnectionCallback callback);
    virtual void onData(DataCallback callback);
    virtual void onError(ErrorCallback callback);
    virtual void onQualityChange(QualityCallback callback);

    // Stats
    struct TransportStats {
        uint64_t bytesSent = 0;
        uint64_t bytesReceived = 0;
        uint64_t packetsSent = 0;
        uint64_t packetsReceived = 0;
        uint64_t packetsLost = 0;
        uint64_t packetsRetransmitted = 0;
        float averageLatency = 0.0f;
        float averageBandwidth = 0.0f;
    };
    virtual const TransportStats& getStats() const { return m_stats; }
    virtual void resetStats();

protected:
    // Internal packet handling
    virtual void processOutgoingPackets();
    virtual void processIncomingPackets();
    virtual void processAcks(uint64_t peerId, uint64_t ackNumber, uint32_t ackBitfield);
    virtual void retransmitPackets(uint64_t peerId);

    // Reliability
    virtual void queueReliablePacket(uint64_t peerId, const NetworkPacket& packet);
    virtual void acknowledgePacket(uint64_t peerId, uint64_t sequenceNumber);

    // Quality monitoring
    virtual void updateConnectionQuality(uint64_t peerId);
    virtual void calculateRTT(uint64_t peerId, const NetworkPacket& packet);

    // Simulation
    virtual bool shouldDropPacket() const;
    virtual int getSimulatedLatency() const;

protected:
    bool m_initialized = false;
    uint64_t m_localPeerId = 0;

    // Peers
    std::unordered_map<uint64_t, PeerInfo> m_peers;
    mutable std::mutex m_peerMutex;

    // Channels
    std::unordered_map<std::string, TransportChannel> m_channels;
    std::string m_defaultChannel = "default";

    // Packet queues
    std::unordered_map<uint64_t, std::queue<NetworkPacket>> m_outgoingQueues;
    std::unordered_map<uint64_t, std::queue<NetworkPacket>> m_incomingQueues;
    std::queue<std::pair<uint64_t, std::vector<uint8_t>>> m_receivedData;
    mutable std::mutex m_queueMutex;

    // Reliable packet tracking
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, NetworkPacket>> m_unackedPackets;
    std::unordered_map<uint64_t, uint64_t> m_nextSequenceNumber;
    std::unordered_map<uint64_t, uint64_t> m_expectedSequenceNumber;

    // Connection quality
    std::unordered_map<uint64_t, ConnectionQuality> m_connectionQuality;
    int m_maxLatency = 500;
    float m_targetBandwidth = 100000.0f;

    // Simulation
    bool m_simulateEnabled = false;
    int m_simulateMinLatency = 0;
    int m_simulateMaxLatency = 0;
    float m_simulatePacketLoss = 0.0f;
    int m_simulateJitter = 0;

    // Firebase relay
    bool m_useFirebaseRelay = false;
    std::string m_firebaseRelayPath;

    // WebRTC signaling
    std::function<void(const ICECandidate&)> m_iceCandidateCallback;

    // Callbacks
    std::vector<ConnectionCallback> m_connectionCallbacks;
    std::vector<DataCallback> m_dataCallbacks;
    std::vector<ErrorCallback> m_errorCallbacks;
    std::vector<QualityCallback> m_qualityCallbacks;

    // Stats
    TransportStats m_stats;

    // Timing
    std::chrono::steady_clock::time_point m_lastQualityUpdate;
    static constexpr float QUALITY_UPDATE_INTERVAL = 1.0f;
};

} // namespace Replication
} // namespace Network
