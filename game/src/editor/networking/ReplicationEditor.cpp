#include "ReplicationEditor.hpp"
#include "../Editor.hpp"
#include <imgui.h>
#include <algorithm>
#include <chrono>

namespace Vehement {

ReplicationEditor::ReplicationEditor() = default;
ReplicationEditor::~ReplicationEditor() {
    if (m_initialized) {
        // Cleanup subscriptions would go here
    }
}

void ReplicationEditor::Initialize(Editor* editor) {
    m_editor = editor;

    // Subscribe to events for monitoring
    auto& replication = Nova::ReplicationSystem::Instance();

    replication.OnEventSent = [this](const Nova::NetworkEvent& event) {
        OnEventSent(event);
    };

    replication.OnEventReceived = [this](const Nova::NetworkEvent& event) {
        OnEventReceived(event);
    };

    m_initialized = true;
}

void ReplicationEditor::Render() {
    if (!m_initialized) return;

    ImGui::Begin("Replication Editor", nullptr, ImGuiWindowFlags_MenuBar);

    // Menu bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Event Types", nullptr, &m_showEventTypes);
            ImGui::MenuItem("Connections", nullptr, &m_showConnections);
            ImGui::MenuItem("Statistics", nullptr, &m_showStats);
            ImGui::MenuItem("Event Monitor", nullptr, &m_showEventMonitor);
            ImGui::MenuItem("Firebase", nullptr, &m_showFirebase);
            ImGui::MenuItem("Persistence", nullptr, &m_showPersistence);
            ImGui::MenuItem("Entity Ownership", nullptr, &m_showOwnership);
            ImGui::MenuItem("Overrides", nullptr, &m_showOverrides);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // Connection status header
    auto& replication = Nova::ReplicationSystem::Instance();
    bool connected = replication.IsConnected();
    bool isHost = replication.IsHost();

    ImGui::Text("Status: ");
    ImGui::SameLine();
    if (connected) {
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), isHost ? "Host" : "Connected");
    } else {
        ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "Disconnected");
    }

    ImGui::SameLine();
    ImGui::Text("| Client ID: %u", replication.GetLocalClientId());

    ImGui::Separator();

    // Render panels
    if (m_showEventTypes) {
        RenderEventTypePanel();
    }

    if (m_showConnections) {
        RenderConnectionPanel();
    }

    if (m_showStats) {
        RenderStatsPanel();
    }

    if (m_showEventMonitor) {
        RenderEventMonitor();
    }

    if (m_showFirebase) {
        RenderFirebasePanel();
    }

    if (m_showPersistence) {
        RenderPersistencePanel();
    }

    if (m_showOwnership) {
        RenderEntityOwnershipPanel();
    }

    if (m_showOverrides) {
        RenderOverridesPanel();
    }

    ImGui::End();
}

void ReplicationEditor::Update(float deltaTime) {
    // Cleanup old event log entries
    auto now = std::chrono::steady_clock::now();
    uint64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    // Remove entries older than 30 seconds
    m_eventLog.erase(
        std::remove_if(m_eventLog.begin(), m_eventLog.end(),
            [nowMs](const EventLogEntry& e) { return (nowMs - e.timestamp) > 30000; }),
        m_eventLog.end()
    );
}

void ReplicationEditor::RenderEventTypePanel() {
    if (!ImGui::CollapsingHeader("Event Types", ImGuiTreeNodeFlags_DefaultOpen)) return;

    // Filter
    ImGui::InputText("Filter##EventTypes", &m_eventFilter[0], 256);

    // Category filter combo
    static int categoryFilter = -1;
    const char* categories[] = {
        "All", "Input", "EntityState", "EntitySpawn", "EntityMovement",
        "Combat", "Abilities", "Building", "Terrain", "Progression",
        "Inventory", "UI", "Chat", "GameState", "Custom"
    };
    ImGui::Combo("Category", &categoryFilter, categories, IM_ARRAYSIZE(categories));

    ImGui::Separator();

    // Table of event types
    if (ImGui::BeginTable("EventTypesTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0, 200))) {
        ImGui::TableSetupColumn("Event Type");
        ImGui::TableSetupColumn("Category");
        ImGui::TableSetupColumn("Replication");
        ImGui::TableSetupColumn("Persistence");
        ImGui::TableSetupColumn("Actions");
        ImGui::TableHeadersRow();

        auto eventTypes = GetEventTypes();

        for (const auto& et : eventTypes) {
            // Filter
            if (!m_eventFilter.empty()) {
                if (et.name.find(m_eventFilter) == std::string::npos) continue;
            }

            if (categoryFilter > 0) {
                if (static_cast<int>(et.category) != (categoryFilter - 1)) continue;
            }

            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            bool selected = (m_selectedEventType == et.name);
            if (ImGui::Selectable(et.name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                m_selectedEventType = et.name;
            }

            ImGui::TableNextColumn();
            ImGui::Text("%s", GetCategoryName(et.category));

            ImGui::TableNextColumn();
            ImGui::Text("%s", GetReplicationModeName(et.replicationMode));

            ImGui::TableNextColumn();
            if (et.persistenceMode == Nova::PersistenceMode::Firebase) {
                ImGui::TextColored(ImVec4(0.2f, 0.6f, 1.0f, 1.0f), "%s", GetPersistenceModeName(et.persistenceMode));
            } else {
                ImGui::Text("%s", GetPersistenceModeName(et.persistenceMode));
            }

            ImGui::TableNextColumn();
            if (et.hasOverride) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Override");
            } else {
                ImGui::TextDisabled("Default");
            }
        }

        ImGui::EndTable();
    }

    // Selected event type details
    if (!m_selectedEventType.empty()) {
        ImGui::Separator();
        ImGui::Text("Selected: %s", m_selectedEventType.c_str());

        // Edit replication mode
        static int repMode = 0;
        const char* repModes[] = {"None", "ToHost", "ToClients", "ToAll", "ToOwner", "ToServer", "Multicast"};
        if (ImGui::Combo("Replication Mode##Edit", &repMode, repModes, IM_ARRAYSIZE(repModes))) {
            Nova::ReplicationSystem::Instance().SetEventTypeOverride(
                m_selectedEventType,
                static_cast<Nova::ReplicationMode>(repMode),
                Nova::PersistenceMode::None
            );
        }

        // Edit persistence mode
        static int persMode = 0;
        const char* persModes[] = {"None", "Firebase", "LocalFile", "Both"};
        if (ImGui::Combo("Persistence Mode##Edit", &persMode, persModes, IM_ARRAYSIZE(persModes))) {
            Nova::ReplicationSystem::Instance().SetEventTypeOverride(
                m_selectedEventType,
                Nova::ReplicationMode::ToAll,
                static_cast<Nova::PersistenceMode>(persMode)
            );
        }

        if (ImGui::Button("Clear Override")) {
            Nova::ReplicationSystem::Instance().ClearEventTypeOverride(m_selectedEventType);
        }
    }
}

void ReplicationEditor::RenderConnectionPanel() {
    if (!ImGui::CollapsingHeader("Connections")) return;

    auto& replication = Nova::ReplicationSystem::Instance();
    auto connections = replication.GetAllConnections();

    if (connections.empty()) {
        ImGui::TextDisabled("No connections");
        return;
    }

    if (ImGui::BeginTable("ConnectionsTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Client ID");
        ImGui::TableSetupColumn("Address");
        ImGui::TableSetupColumn("Latency");
        ImGui::TableSetupColumn("Owned Entities");
        ImGui::TableSetupColumn("Status");
        ImGui::TableHeadersRow();

        for (const auto& conn : connections) {
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            bool selected = (m_selectedClientId == conn.clientId);
            std::string label = std::to_string(conn.clientId);
            if (conn.isLocal) label += " (Local)";
            if (conn.isHost) label += " (Host)";

            if (ImGui::Selectable(label.c_str(), selected)) {
                m_selectedClientId = conn.clientId;
            }

            ImGui::TableNextColumn();
            ImGui::Text("%s:%u", conn.address.c_str(), conn.port);

            ImGui::TableNextColumn();
            ImGui::Text("%.1f ms", conn.latency * 1000.0f);

            ImGui::TableNextColumn();
            ImGui::Text("%zu", conn.ownedEntities.size());

            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Connected");
        }

        ImGui::EndTable();
    }

    // Host controls
    if (replication.IsHost()) {
        ImGui::Separator();
        ImGui::Text("Host Controls");

        if (m_selectedClientId != 0 && m_selectedClientId != replication.GetLocalClientId()) {
            if (ImGui::Button("Kick Client")) {
                // Would implement kick functionality
            }
        }
    } else {
        if (ImGui::Button("Disconnect")) {
            replication.Disconnect();
        }
    }
}

void ReplicationEditor::RenderStatsPanel() {
    if (!ImGui::CollapsingHeader("Statistics")) return;

    auto& replication = Nova::ReplicationSystem::Instance();
    const auto& stats = replication.GetStats();

    ImGui::Columns(2);

    ImGui::Text("Events Sent:"); ImGui::NextColumn();
    ImGui::Text("%llu", stats.eventsSent); ImGui::NextColumn();

    ImGui::Text("Events Received:"); ImGui::NextColumn();
    ImGui::Text("%llu", stats.eventsReceived); ImGui::NextColumn();

    ImGui::Text("Events Dropped:"); ImGui::NextColumn();
    ImGui::Text("%llu", stats.eventsDropped); ImGui::NextColumn();

    ImGui::Text("Events Persisted:"); ImGui::NextColumn();
    ImGui::Text("%llu", stats.eventsPersisted); ImGui::NextColumn();

    ImGui::Text("Bytes Out:"); ImGui::NextColumn();
    ImGui::Text("%.2f KB", stats.bytesOut / 1024.0f); ImGui::NextColumn();

    ImGui::Text("Bytes In:"); ImGui::NextColumn();
    ImGui::Text("%.2f KB", stats.bytesIn / 1024.0f); ImGui::NextColumn();

    ImGui::Text("Avg Latency:"); ImGui::NextColumn();
    ImGui::Text("%.1f ms", stats.avgLatency * 1000.0f); ImGui::NextColumn();

    ImGui::Columns(1);

    if (ImGui::Button("Reset Stats")) {
        replication.ResetStats();
    }

    // Top event types by count
    if (!stats.eventCountByType.empty()) {
        ImGui::Separator();
        ImGui::Text("Top Event Types:");

        std::vector<std::pair<std::string, uint64_t>> sorted(
            stats.eventCountByType.begin(), stats.eventCountByType.end());
        std::sort(sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

        int shown = 0;
        for (const auto& [type, count] : sorted) {
            if (shown >= 10) break;
            ImGui::Text("  %s: %llu", type.c_str(), count);
            shown++;
        }
    }
}

void ReplicationEditor::RenderEventMonitor() {
    if (!ImGui::CollapsingHeader("Event Monitor")) return;

    ImGui::Checkbox("Auto Scroll", &m_autoScroll);
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        m_eventLog.clear();
    }

    ImGui::Separator();

    // Event log
    ImGui::BeginChild("EventLog", ImVec2(0, 200), true, ImGuiWindowFlags_HorizontalScrollbar);

    for (const auto& entry : m_eventLog) {
        ImVec4 color = entry.sent ?
            ImVec4(0.2f, 0.8f, 0.2f, 1.0f) :
            ImVec4(0.2f, 0.6f, 1.0f, 1.0f);

        ImGui::TextColored(color, "[%s] %s (%s)",
            entry.sent ? "OUT" : "IN",
            entry.type.c_str(),
            GetReplicationModeName(entry.mode));
    }

    if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
}

void ReplicationEditor::RenderFirebasePanel() {
    if (!ImGui::CollapsingHeader("Firebase")) return;

    auto& persistence = Nova::FirebasePersistence::Instance();

    // Status
    ImGui::Text("Pending Modifications: %zu", persistence.GetPendingModificationCount());
    ImGui::Text("Pending Chunks: %zu", persistence.GetPendingChunkCount());

    ImGui::Separator();

    // Config
    auto& config = persistence.GetConfig();

    ImGui::Text("Sync Settings:");
    ImGui::SliderFloat("Min Sync Interval", &config.minSyncInterval, 5.0f, 120.0f, "%.0f sec");
    ImGui::SliderFloat("Max Sync Interval", &config.maxSyncInterval, 60.0f, 600.0f, "%.0f sec");
    ImGui::SliderFloat("Idle Sync Delay", &config.idleSyncDelay, 10.0f, 120.0f, "%.0f sec");

    ImGui::Separator();

    ImGui::Text("Batching:");
    ImGui::SliderInt("Max Mods Per Batch", &config.maxModificationsPerBatch, 10, 500);
    ImGui::SliderInt("Max Chunks Per Sync", &config.maxChunksPerSync, 1, 50);
    ImGui::Checkbox("Merge Overlapping", &config.mergeOverlappingMods);

    ImGui::Separator();

    ImGui::Text("Bandwidth Limits:");
    ImGui::SliderInt("Max KB/min", &config.maxBytesPerMinute, 10000, 200000);
    ImGui::SliderInt("Max Ops/min", &config.maxOperationsPerMinute, 10, 100);

    ImGui::Separator();

    // Actions
    if (ImGui::Button("Force Sync Now")) {
        persistence.ForceSync();
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear Pending")) {
        // Would clear pending modifications
    }

    // Stats
    const auto& stats = persistence.GetStats();
    ImGui::Separator();
    ImGui::Text("Firebase Stats:");
    ImGui::Text("  Total Synced: %llu modifications, %llu chunks",
        stats.totalModificationsSynced, stats.totalChunksSynced);
    ImGui::Text("  Total Bytes Sent: %.2f KB", stats.totalBytesSent / 1024.0f);
    ImGui::Text("  Merged: %llu, Failed: %llu", stats.mergedModifications, stats.failedSyncs);
    ImGui::Text("  Avg Sync Time: %.2f ms", stats.avgSyncTime * 1000.0f);
}

void ReplicationEditor::RenderPersistencePanel() {
    if (!ImGui::CollapsingHeader("Persistence Settings")) return;

    ImGui::Text("Default persistence rules:");
    ImGui::BulletText("Terrain changes -> Firebase (batched)");
    ImGui::BulletText("Entity state -> Replicated only (fetch from host)");
    ImGui::BulletText("Input/Movement -> Replicated only (temporary)");
    ImGui::BulletText("Combat/Abilities -> Replicated only");
    ImGui::BulletText("Buildings -> Replicated + Firebase on completion");

    ImGui::Separator();

    ImGui::Text("Quick Toggle:");

    static bool persistTerrain = true;
    static bool persistBuildings = true;
    static bool persistProgression = false;

    if (ImGui::Checkbox("Persist Terrain to Firebase", &persistTerrain)) {
        Nova::FirebasePersistence::Instance().SetPersistenceOverride(
            Nova::Events::TERRAIN_MODIFY, persistTerrain);
    }

    if (ImGui::Checkbox("Persist Buildings to Firebase", &persistBuildings)) {
        Nova::FirebasePersistence::Instance().SetPersistenceOverride(
            Nova::Events::BUILDING_COMPLETE, persistBuildings);
    }

    if (ImGui::Checkbox("Persist Progression to Firebase", &persistProgression)) {
        Nova::FirebasePersistence::Instance().SetPersistenceOverride(
            Nova::Events::PROGRESSION_LEVEL, persistProgression);
    }
}

void ReplicationEditor::RenderEntityOwnershipPanel() {
    if (!ImGui::CollapsingHeader("Entity Ownership")) return;

    ImGui::TextDisabled("Entity ownership tracking would be shown here");
    // Would show a list of entities and their owners
}

void ReplicationEditor::RenderOverridesPanel() {
    if (!ImGui::CollapsingHeader("Active Overrides")) return;

    ImGui::TextDisabled("Custom overrides for event types");

    // Would list all active overrides with ability to remove them
}

// ============================================================================
// Helper Methods
// ============================================================================

std::vector<ReplicationEditor::EventTypeDisplay> ReplicationEditor::GetEventTypes() const {
    std::vector<EventTypeDisplay> types;

    // Add known event types
    auto addType = [&types](const char* name, Nova::ReplicationCategory cat,
                            Nova::ReplicationMode rep, Nova::PersistenceMode pers) {
        types.push_back({name, cat, rep, pers, Nova::ReliabilityMode::Reliable, false});
    };

    // Input events
    addType(Nova::Events::INPUT_MOVE, Nova::ReplicationCategory::Input,
            Nova::ReplicationMode::ToHost, Nova::PersistenceMode::None);
    addType(Nova::Events::INPUT_ACTION, Nova::ReplicationCategory::Input,
            Nova::ReplicationMode::ToHost, Nova::PersistenceMode::None);

    // Entity events
    addType(Nova::Events::ENTITY_SPAWN, Nova::ReplicationCategory::EntitySpawn,
            Nova::ReplicationMode::ToClients, Nova::PersistenceMode::None);
    addType(Nova::Events::ENTITY_DESTROY, Nova::ReplicationCategory::EntitySpawn,
            Nova::ReplicationMode::ToClients, Nova::PersistenceMode::None);
    addType(Nova::Events::ENTITY_MOVE, Nova::ReplicationCategory::EntityMovement,
            Nova::ReplicationMode::ToAll, Nova::PersistenceMode::None);
    addType(Nova::Events::ENTITY_STATE, Nova::ReplicationCategory::EntityState,
            Nova::ReplicationMode::ToAll, Nova::PersistenceMode::None);

    // Combat events
    addType(Nova::Events::COMBAT_ATTACK, Nova::ReplicationCategory::Combat,
            Nova::ReplicationMode::ToHost, Nova::PersistenceMode::None);
    addType(Nova::Events::COMBAT_DAMAGE, Nova::ReplicationCategory::Combat,
            Nova::ReplicationMode::ToClients, Nova::PersistenceMode::None);

    // Ability events
    addType(Nova::Events::ABILITY_USE, Nova::ReplicationCategory::Abilities,
            Nova::ReplicationMode::ToHost, Nova::PersistenceMode::None);

    // Building events
    addType(Nova::Events::BUILDING_PLACE, Nova::ReplicationCategory::Building,
            Nova::ReplicationMode::ToHost, Nova::PersistenceMode::None);
    addType(Nova::Events::BUILDING_COMPLETE, Nova::ReplicationCategory::Building,
            Nova::ReplicationMode::ToClients, Nova::PersistenceMode::None);

    // Terrain events - FIREBASE PERSISTENCE
    addType(Nova::Events::TERRAIN_MODIFY, Nova::ReplicationCategory::Terrain,
            Nova::ReplicationMode::ToClients, Nova::PersistenceMode::Firebase);
    addType(Nova::Events::TERRAIN_SCULPT, Nova::ReplicationCategory::Terrain,
            Nova::ReplicationMode::ToClients, Nova::PersistenceMode::Firebase);
    addType(Nova::Events::TERRAIN_TUNNEL, Nova::ReplicationCategory::Terrain,
            Nova::ReplicationMode::ToClients, Nova::PersistenceMode::Firebase);

    // Progression events
    addType(Nova::Events::PROGRESSION_XP, Nova::ReplicationCategory::Progression,
            Nova::ReplicationMode::ToOwner, Nova::PersistenceMode::None);
    addType(Nova::Events::PROGRESSION_LEVEL, Nova::ReplicationCategory::Progression,
            Nova::ReplicationMode::ToAll, Nova::PersistenceMode::None);

    // Chat events
    addType(Nova::Events::CHAT_MESSAGE, Nova::ReplicationCategory::Chat,
            Nova::ReplicationMode::ToAll, Nova::PersistenceMode::None);

    return types;
}

const char* ReplicationEditor::GetCategoryName(Nova::ReplicationCategory cat) const {
    switch (cat) {
        case Nova::ReplicationCategory::Input: return "Input";
        case Nova::ReplicationCategory::EntityState: return "EntityState";
        case Nova::ReplicationCategory::EntitySpawn: return "EntitySpawn";
        case Nova::ReplicationCategory::EntityMovement: return "EntityMovement";
        case Nova::ReplicationCategory::Combat: return "Combat";
        case Nova::ReplicationCategory::Abilities: return "Abilities";
        case Nova::ReplicationCategory::Building: return "Building";
        case Nova::ReplicationCategory::Terrain: return "Terrain";
        case Nova::ReplicationCategory::Progression: return "Progression";
        case Nova::ReplicationCategory::Inventory: return "Inventory";
        case Nova::ReplicationCategory::UI: return "UI";
        case Nova::ReplicationCategory::Chat: return "Chat";
        case Nova::ReplicationCategory::GameState: return "GameState";
        case Nova::ReplicationCategory::Custom: return "Custom";
        default: return "Unknown";
    }
}

const char* ReplicationEditor::GetReplicationModeName(Nova::ReplicationMode mode) const {
    switch (mode) {
        case Nova::ReplicationMode::None: return "None";
        case Nova::ReplicationMode::ToHost: return "ToHost";
        case Nova::ReplicationMode::ToClients: return "ToClients";
        case Nova::ReplicationMode::ToAll: return "ToAll";
        case Nova::ReplicationMode::ToOwner: return "ToOwner";
        case Nova::ReplicationMode::ToServer: return "ToServer";
        case Nova::ReplicationMode::Multicast: return "Multicast";
        default: return "Unknown";
    }
}

const char* ReplicationEditor::GetPersistenceModeName(Nova::PersistenceMode mode) const {
    switch (mode) {
        case Nova::PersistenceMode::None: return "None";
        case Nova::PersistenceMode::Firebase: return "Firebase";
        case Nova::PersistenceMode::LocalFile: return "LocalFile";
        case Nova::PersistenceMode::Both: return "Both";
        default: return "Unknown";
    }
}

const char* ReplicationEditor::GetReliabilityModeName(Nova::ReliabilityMode mode) const {
    switch (mode) {
        case Nova::ReliabilityMode::Unreliable: return "Unreliable";
        case Nova::ReliabilityMode::Reliable: return "Reliable";
        case Nova::ReliabilityMode::ReliableOrdered: return "ReliableOrdered";
        default: return "Unknown";
    }
}

void ReplicationEditor::OnEventSent(const Nova::NetworkEvent& event) {
    EventLogEntry entry;
    entry.id = event.eventId;
    entry.type = event.eventType;
    entry.mode = event.replicationMode;
    entry.sent = true;
    entry.timestamp = event.timestamp;

    m_eventLog.push_back(entry);

    if (m_eventLog.size() > MAX_EVENT_LOG) {
        m_eventLog.erase(m_eventLog.begin());
    }
}

void ReplicationEditor::OnEventReceived(const Nova::NetworkEvent& event) {
    EventLogEntry entry;
    entry.id = event.eventId;
    entry.type = event.eventType;
    entry.mode = event.replicationMode;
    entry.sent = false;
    entry.timestamp = event.timestamp;

    m_eventLog.push_back(entry);

    if (m_eventLog.size() > MAX_EVENT_LOG) {
        m_eventLog.erase(m_eventLog.begin());
    }
}

} // namespace Vehement
