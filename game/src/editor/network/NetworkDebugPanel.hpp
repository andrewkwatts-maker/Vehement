#pragma once

#include <string>
#include <vector>
#include <deque>
#include <memory>
#include <chrono>
#include <functional>

namespace Editor {
namespace Network {

// Latency sample for graphing
struct LatencySample {
    float latency;
    std::chrono::steady_clock::time_point timestamp;
};

// Bandwidth sample
struct BandwidthSample {
    float sent;
    float received;
    std::chrono::steady_clock::time_point timestamp;
};

// Packet info for inspector
struct PacketInfo {
    uint64_t sequenceNumber;
    std::string channel;
    int size;
    bool isReliable;
    bool isOutgoing;
    std::chrono::steady_clock::time_point timestamp;
    std::string preview;  // First N bytes as hex
    std::vector<uint8_t> data;
};

// Entity replication status
struct EntityReplicationStatus {
    uint64_t networkId;
    std::string entityType;
    uint64_t ownerId;
    bool hasAuthority;
    int dirtyPropertyCount;
    int bytesSent;
    int bytesReceived;
    float lastReplicationTime;
    std::vector<std::string> dirtyProperties;
};

// Simulation settings
struct NetworkSimulationSettings {
    bool enabled = false;
    int minLatencyMs = 0;
    int maxLatencyMs = 0;
    float packetLossPercent = 0.0f;
    int jitterMs = 0;
    bool simulateDisconnect = false;
    float disconnectDuration = 0.0f;
};

/**
 * NetworkDebugPanel - Network debugging UI
 *
 * Features:
 * - Latency graph
 * - Bandwidth usage
 * - Entity replication status
 * - Packet inspector
 * - Simulate lag/packet loss
 */
class NetworkDebugPanel {
public:
    static NetworkDebugPanel& getInstance();

    // Panel state
    void show(bool visible = true) { m_visible = visible; }
    void hide() { m_visible = false; }
    void toggle() { m_visible = !m_visible; }
    bool isVisible() const { return m_visible; }

    // Update and render
    void update(float deltaTime);
    void render();
    void renderImGui();  // If using ImGui
    std::string renderHTML();  // For HTML UI

    // Data collection
    void recordLatency(uint64_t peerId, float latencyMs);
    void recordBandwidth(float sentBytesPerSec, float receivedBytesPerSec);
    void recordPacket(const PacketInfo& packet);
    void updateEntityStatus(const EntityReplicationStatus& status);
    void clearEntityStatus(uint64_t networkId);

    // Graph settings
    void setGraphHistorySize(int samples) { m_graphHistorySize = samples; }
    void setGraphTimeRange(float seconds) { m_graphTimeRange = seconds; }

    // Packet inspector
    void setMaxPacketHistory(int count) { m_maxPacketHistory = count; }
    void clearPacketHistory();
    void setPacketFilter(const std::string& filter) { m_packetFilter = filter; }
    const std::vector<PacketInfo>& getPacketHistory() const { return m_packetHistory; }

    // Network simulation
    void setSimulationSettings(const NetworkSimulationSettings& settings);
    const NetworkSimulationSettings& getSimulationSettings() const { return m_simulationSettings; }
    void applySimulation();
    void clearSimulation();

    // Stats
    float getCurrentLatency() const;
    float getAverageLatency() const;
    float getCurrentBandwidthSent() const;
    float getCurrentBandwidthReceived() const;
    int getReplicatedEntityCount() const;
    int getTotalDirtyProperties() const;

    // Export
    std::string exportStats() const;
    void exportToFile(const std::string& filename) const;

private:
    NetworkDebugPanel();
    ~NetworkDebugPanel();
    NetworkDebugPanel(const NetworkDebugPanel&) = delete;
    NetworkDebugPanel& operator=(const NetworkDebugPanel&) = delete;

    // Rendering helpers
    void renderLatencyGraph();
    void renderBandwidthGraph();
    void renderEntityList();
    void renderPacketInspector();
    void renderSimulationControls();

    // Data management
    void trimLatencyHistory();
    void trimBandwidthHistory();
    void trimPacketHistory();

private:
    bool m_visible = false;

    // Latency data
    std::deque<LatencySample> m_latencyHistory;
    std::unordered_map<uint64_t, std::deque<LatencySample>> m_peerLatency;

    // Bandwidth data
    std::deque<BandwidthSample> m_bandwidthHistory;

    // Entity status
    std::unordered_map<uint64_t, EntityReplicationStatus> m_entityStatus;

    // Packet history
    std::vector<PacketInfo> m_packetHistory;
    int m_maxPacketHistory = 1000;
    std::string m_packetFilter;

    // Simulation
    NetworkSimulationSettings m_simulationSettings;

    // Graph settings
    int m_graphHistorySize = 120;
    float m_graphTimeRange = 60.0f;

    // Selected items
    uint64_t m_selectedEntity = 0;
    int m_selectedPacket = -1;

    // Tabs
    enum class Tab { Overview, Latency, Bandwidth, Entities, Packets, Simulation };
    Tab m_currentTab = Tab::Overview;
};

} // namespace Network
} // namespace Editor
