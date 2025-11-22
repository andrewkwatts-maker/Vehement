#include "NetworkDebugPanel.hpp"
#include "../../network/replication/ReplicationManager.hpp"
#include "../../network/replication/NetworkTransport.hpp"
#include <sstream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <numeric>

namespace Editor {
namespace Network {

NetworkDebugPanel& NetworkDebugPanel::getInstance() {
    static NetworkDebugPanel instance;
    return instance;
}

NetworkDebugPanel::NetworkDebugPanel() {}

NetworkDebugPanel::~NetworkDebugPanel() {}

void NetworkDebugPanel::update(float deltaTime) {
    if (!m_visible) return;

    // Trim histories
    trimLatencyHistory();
    trimBandwidthHistory();
    trimPacketHistory();
}

void NetworkDebugPanel::render() {
    if (!m_visible) return;

    // This would integrate with your rendering system
    // For now, data is available via renderHTML() or renderImGui()
}

void NetworkDebugPanel::renderImGui() {
    if (!m_visible) return;

    // ImGui rendering would go here
    // Example structure:
    /*
    if (ImGui::Begin("Network Debug", &m_visible)) {
        if (ImGui::BeginTabBar("NetworkTabs")) {
            if (ImGui::BeginTabItem("Overview")) {
                renderOverview();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Latency")) {
                renderLatencyGraph();
                ImGui::EndTabItem();
            }
            // etc...
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
    */
}

std::string NetworkDebugPanel::renderHTML() {
    std::ostringstream html;

    html << "<!DOCTYPE html>\n<html>\n<head>\n";
    html << "<title>Network Debug Panel</title>\n";
    html << "<style>\n";
    html << "body { font-family: monospace; background: #1a1a2e; color: #eef; padding: 20px; }\n";
    html << ".panel { background: #16213e; border-radius: 8px; padding: 15px; margin: 10px 0; }\n";
    html << ".stat { display: inline-block; margin-right: 30px; }\n";
    html << ".stat-value { font-size: 24px; color: #4ecca3; }\n";
    html << ".stat-label { font-size: 12px; color: #888; }\n";
    html << "table { width: 100%; border-collapse: collapse; }\n";
    html << "th, td { padding: 8px; text-align: left; border-bottom: 1px solid #333; }\n";
    html << "th { background: #0f3460; }\n";
    html << ".graph { height: 200px; background: #0f3460; border-radius: 4px; position: relative; }\n";
    html << ".good { color: #4ecca3; }\n";
    html << ".warn { color: #f9ed69; }\n";
    html << ".bad { color: #f38181; }\n";
    html << "</style>\n</head>\n<body>\n";

    html << "<h1>Network Debug Panel</h1>\n";

    // Overview stats
    html << "<div class='panel'>\n";
    html << "<h2>Overview</h2>\n";
    html << "<div class='stat'><div class='stat-value'>" << std::fixed << std::setprecision(1) << getCurrentLatency() << "ms</div><div class='stat-label'>Latency</div></div>\n";
    html << "<div class='stat'><div class='stat-value'>" << std::fixed << std::setprecision(1) << getCurrentBandwidthSent() / 1024.0f << "KB/s</div><div class='stat-label'>Upload</div></div>\n";
    html << "<div class='stat'><div class='stat-value'>" << std::fixed << std::setprecision(1) << getCurrentBandwidthReceived() / 1024.0f << "KB/s</div><div class='stat-label'>Download</div></div>\n";
    html << "<div class='stat'><div class='stat-value'>" << getReplicatedEntityCount() << "</div><div class='stat-label'>Entities</div></div>\n";
    html << "<div class='stat'><div class='stat-value'>" << getTotalDirtyProperties() << "</div><div class='stat-label'>Dirty Props</div></div>\n";
    html << "</div>\n";

    // Entity table
    html << "<div class='panel'>\n";
    html << "<h2>Replicated Entities</h2>\n";
    html << "<table>\n";
    html << "<tr><th>ID</th><th>Type</th><th>Owner</th><th>Authority</th><th>Dirty</th><th>Bytes Sent</th></tr>\n";

    for (const auto& [id, status] : m_entityStatus) {
        html << "<tr>";
        html << "<td>" << id << "</td>";
        html << "<td>" << status.entityType << "</td>";
        html << "<td>" << status.ownerId << "</td>";
        html << "<td>" << (status.hasAuthority ? "Yes" : "No") << "</td>";
        html << "<td>" << status.dirtyPropertyCount << "</td>";
        html << "<td>" << status.bytesSent << "</td>";
        html << "</tr>\n";
    }

    html << "</table>\n</div>\n";

    // Packet history
    html << "<div class='panel'>\n";
    html << "<h2>Recent Packets</h2>\n";
    html << "<table>\n";
    html << "<tr><th>Seq</th><th>Channel</th><th>Size</th><th>Dir</th><th>Preview</th></tr>\n";

    int packetCount = 0;
    for (auto it = m_packetHistory.rbegin(); it != m_packetHistory.rend() && packetCount < 20; ++it, ++packetCount) {
        const auto& packet = *it;
        html << "<tr>";
        html << "<td>" << packet.sequenceNumber << "</td>";
        html << "<td>" << packet.channel << "</td>";
        html << "<td>" << packet.size << "</td>";
        html << "<td>" << (packet.isOutgoing ? "OUT" : "IN") << "</td>";
        html << "<td>" << packet.preview << "</td>";
        html << "</tr>\n";
    }

    html << "</table>\n</div>\n";

    // Simulation controls
    html << "<div class='panel'>\n";
    html << "<h2>Network Simulation</h2>\n";
    html << "<p>Enabled: " << (m_simulationSettings.enabled ? "Yes" : "No") << "</p>\n";
    html << "<p>Latency: " << m_simulationSettings.minLatencyMs << "-" << m_simulationSettings.maxLatencyMs << "ms</p>\n";
    html << "<p>Packet Loss: " << m_simulationSettings.packetLossPercent << "%</p>\n";
    html << "<p>Jitter: " << m_simulationSettings.jitterMs << "ms</p>\n";
    html << "</div>\n";

    html << "</body>\n</html>";

    return html.str();
}

void NetworkDebugPanel::recordLatency(uint64_t peerId, float latencyMs) {
    LatencySample sample;
    sample.latency = latencyMs;
    sample.timestamp = std::chrono::steady_clock::now();

    m_latencyHistory.push_back(sample);
    m_peerLatency[peerId].push_back(sample);
}

void NetworkDebugPanel::recordBandwidth(float sentBytesPerSec, float receivedBytesPerSec) {
    BandwidthSample sample;
    sample.sent = sentBytesPerSec;
    sample.received = receivedBytesPerSec;
    sample.timestamp = std::chrono::steady_clock::now();

    m_bandwidthHistory.push_back(sample);
}

void NetworkDebugPanel::recordPacket(const PacketInfo& packet) {
    m_packetHistory.push_back(packet);
    trimPacketHistory();
}

void NetworkDebugPanel::updateEntityStatus(const EntityReplicationStatus& status) {
    m_entityStatus[status.networkId] = status;
}

void NetworkDebugPanel::clearEntityStatus(uint64_t networkId) {
    m_entityStatus.erase(networkId);
}

void NetworkDebugPanel::clearPacketHistory() {
    m_packetHistory.clear();
}

void NetworkDebugPanel::setSimulationSettings(const NetworkSimulationSettings& settings) {
    m_simulationSettings = settings;
}

void NetworkDebugPanel::applySimulation() {
    auto transport = ::Network::Replication::NetworkTransport::create();
    if (!transport) return;

    if (m_simulationSettings.enabled) {
        transport->simulateLatency(m_simulationSettings.minLatencyMs,
                                   m_simulationSettings.maxLatencyMs);
        transport->simulatePacketLoss(m_simulationSettings.packetLossPercent);
        transport->simulateJitter(m_simulationSettings.jitterMs);
    } else {
        transport->clearSimulation();
    }
}

void NetworkDebugPanel::clearSimulation() {
    m_simulationSettings.enabled = false;
    applySimulation();
}

float NetworkDebugPanel::getCurrentLatency() const {
    if (m_latencyHistory.empty()) return 0.0f;
    return m_latencyHistory.back().latency;
}

float NetworkDebugPanel::getAverageLatency() const {
    if (m_latencyHistory.empty()) return 0.0f;

    float sum = 0.0f;
    for (const auto& sample : m_latencyHistory) {
        sum += sample.latency;
    }
    return sum / m_latencyHistory.size();
}

float NetworkDebugPanel::getCurrentBandwidthSent() const {
    if (m_bandwidthHistory.empty()) return 0.0f;
    return m_bandwidthHistory.back().sent;
}

float NetworkDebugPanel::getCurrentBandwidthReceived() const {
    if (m_bandwidthHistory.empty()) return 0.0f;
    return m_bandwidthHistory.back().received;
}

int NetworkDebugPanel::getReplicatedEntityCount() const {
    return static_cast<int>(m_entityStatus.size());
}

int NetworkDebugPanel::getTotalDirtyProperties() const {
    int total = 0;
    for (const auto& [id, status] : m_entityStatus) {
        total += status.dirtyPropertyCount;
    }
    return total;
}

std::string NetworkDebugPanel::exportStats() const {
    std::ostringstream ss;

    ss << "Network Debug Stats Export\n";
    ss << "==========================\n\n";

    ss << "Current Latency: " << getCurrentLatency() << "ms\n";
    ss << "Average Latency: " << getAverageLatency() << "ms\n";
    ss << "Bandwidth Sent: " << getCurrentBandwidthSent() << " B/s\n";
    ss << "Bandwidth Received: " << getCurrentBandwidthReceived() << " B/s\n";
    ss << "Replicated Entities: " << getReplicatedEntityCount() << "\n";
    ss << "Total Dirty Properties: " << getTotalDirtyProperties() << "\n\n";

    ss << "Entity Details:\n";
    for (const auto& [id, status] : m_entityStatus) {
        ss << "  Entity " << id << " (" << status.entityType << "):\n";
        ss << "    Owner: " << status.ownerId << "\n";
        ss << "    Authority: " << (status.hasAuthority ? "Yes" : "No") << "\n";
        ss << "    Dirty Properties: " << status.dirtyPropertyCount << "\n";
        ss << "    Bytes Sent: " << status.bytesSent << "\n";
    }

    return ss.str();
}

void NetworkDebugPanel::exportToFile(const std::string& filename) const {
    std::ofstream file(filename);
    if (file.is_open()) {
        file << exportStats();
        file.close();
    }
}

// Private methods

void NetworkDebugPanel::renderLatencyGraph() {
    // Would render latency graph using your graphics API or ImGui
}

void NetworkDebugPanel::renderBandwidthGraph() {
    // Would render bandwidth graph
}

void NetworkDebugPanel::renderEntityList() {
    // Would render entity list
}

void NetworkDebugPanel::renderPacketInspector() {
    // Would render packet inspector with hex view
}

void NetworkDebugPanel::renderSimulationControls() {
    // Would render simulation sliders/controls
}

void NetworkDebugPanel::trimLatencyHistory() {
    auto cutoff = std::chrono::steady_clock::now() -
                  std::chrono::seconds(static_cast<int>(m_graphTimeRange));

    while (!m_latencyHistory.empty() && m_latencyHistory.front().timestamp < cutoff) {
        m_latencyHistory.pop_front();
    }

    for (auto& [peerId, history] : m_peerLatency) {
        while (!history.empty() && history.front().timestamp < cutoff) {
            history.pop_front();
        }
    }
}

void NetworkDebugPanel::trimBandwidthHistory() {
    auto cutoff = std::chrono::steady_clock::now() -
                  std::chrono::seconds(static_cast<int>(m_graphTimeRange));

    while (!m_bandwidthHistory.empty() && m_bandwidthHistory.front().timestamp < cutoff) {
        m_bandwidthHistory.pop_front();
    }
}

void NetworkDebugPanel::trimPacketHistory() {
    while (static_cast<int>(m_packetHistory.size()) > m_maxPacketHistory) {
        m_packetHistory.erase(m_packetHistory.begin());
    }
}

} // namespace Network
} // namespace Editor
