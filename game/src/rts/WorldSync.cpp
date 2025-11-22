#include "WorldSync.hpp"
#include "../network/FirebaseManager.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>

// Logging macros
#if __has_include("../../engine/core/Logger.hpp")
#include "../../engine/core/Logger.hpp"
#define SYNC_LOG_INFO(...) NOVA_LOG_INFO(__VA_ARGS__)
#define SYNC_LOG_WARN(...) NOVA_LOG_WARN(__VA_ARGS__)
#define SYNC_LOG_ERROR(...) NOVA_LOG_ERROR(__VA_ARGS__)
#else
#include <iostream>
#define SYNC_LOG_INFO(...) std::cout << "[WorldSync] " << __VA_ARGS__ << std::endl
#define SYNC_LOG_WARN(...) std::cerr << "[WorldSync WARN] " << __VA_ARGS__ << std::endl
#define SYNC_LOG_ERROR(...) std::cerr << "[WorldSync ERROR] " << __VA_ARGS__ << std::endl
#endif

namespace Vehement {

// ============================================================================
// NearbyPlayer Implementation
// ============================================================================

nlohmann::json NearbyPlayer::ToJson() const {
    return {
        {"playerId", playerId},
        {"displayName", displayName},
        {"position", {position.x, position.y}},
        {"distance", distance},
        {"isOnline", isOnline},
        {"lastSeen", lastSeen},
        {"level", level},
        {"territorySize", territorySize}
    };
}

NearbyPlayer NearbyPlayer::FromJson(const nlohmann::json& j) {
    NearbyPlayer np;
    np.playerId = j.value("playerId", "");
    np.displayName = j.value("displayName", "Unknown");
    if (j.contains("position") && j["position"].is_array()) {
        np.position.x = j["position"][0].get<float>();
        np.position.y = j["position"][1].get<float>();
    }
    np.distance = j.value("distance", 0.0f);
    np.isOnline = j.value("isOnline", false);
    np.lastSeen = j.value("lastSeen", 0LL);
    np.level = j.value("level", 1);
    np.territorySize = j.value("territorySize", 0);
    return np;
}

// ============================================================================
// ResourceUpdate Implementation
// ============================================================================

nlohmann::json ResourceUpdate::ToJson() const {
    return {
        {"playerId", playerId},
        {"resources", resources.ToJson()},
        {"timestamp", timestamp}
    };
}

ResourceUpdate ResourceUpdate::FromJson(const nlohmann::json& j) {
    ResourceUpdate ru;
    ru.playerId = j.value("playerId", "");
    if (j.contains("resources")) {
        ru.resources = ResourceStock::FromJson(j["resources"]);
    }
    ru.timestamp = j.value("timestamp", 0LL);
    return ru;
}

// ============================================================================
// BuildingChangeEvent Implementation
// ============================================================================

nlohmann::json BuildingChangeEvent::ToJson() const {
    return {
        {"type", static_cast<int>(type)},
        {"playerId", playerId},
        {"building", building.ToJson()},
        {"timestamp", timestamp}
    };
}

BuildingChangeEvent BuildingChangeEvent::FromJson(const nlohmann::json& j) {
    BuildingChangeEvent bce;
    bce.type = static_cast<BuildingChangeEvent::Type>(j.value("type", 0));
    bce.playerId = j.value("playerId", "");
    if (j.contains("building")) {
        bce.building = Building::FromJson(j["building"]);
    }
    bce.timestamp = j.value("timestamp", 0LL);
    return bce;
}

// ============================================================================
// WorkerAssignmentEvent Implementation
// ============================================================================

nlohmann::json WorkerAssignmentEvent::ToJson() const {
    return {
        {"playerId", playerId},
        {"workerId", workerId},
        {"job", static_cast<int>(job)},
        {"buildingId", buildingId},
        {"timestamp", timestamp}
    };
}

WorkerAssignmentEvent WorkerAssignmentEvent::FromJson(const nlohmann::json& j) {
    WorkerAssignmentEvent wae;
    wae.playerId = j.value("playerId", "");
    wae.workerId = j.value("workerId", -1);
    wae.job = static_cast<WorkerJob>(j.value("job", 0));
    wae.buildingId = j.value("buildingId", -1);
    wae.timestamp = j.value("timestamp", 0LL);
    return wae;
}

// ============================================================================
// WorldSync Implementation
// ============================================================================

WorldSync& WorldSync::Instance() {
    static WorldSync instance;
    return instance;
}

bool WorldSync::Initialize(const WorldSyncConfig& config) {
    if (m_initialized) {
        SYNC_LOG_WARN("WorldSync already initialized");
        return true;
    }

    m_config = config;
    m_initialized = true;

    SYNC_LOG_INFO("WorldSync initialized");
    return true;
}

void WorldSync::Shutdown() {
    if (!m_initialized) {
        return;
    }

    if (m_syncing) {
        StopSync();
    }

    m_initialized = false;
    SYNC_LOG_INFO("WorldSync shutdown complete");
}

void WorldSync::StartSync(const std::string& playerId, const std::string& region) {
    if (!m_initialized) {
        SYNC_LOG_ERROR("Cannot start sync: not initialized");
        return;
    }

    if (m_syncing) {
        StopSync();
    }

    m_playerId = playerId;
    m_region = region;

    // Setup Firebase listeners
    SetupListeners();

    // Mark as online
    SyncHeroOnlineStatus(true);

    m_syncing = true;
    SYNC_LOG_INFO("Started world sync for player " + playerId + " in region " + region);
}

void WorldSync::StopSync() {
    if (!m_syncing) {
        return;
    }

    // Mark as offline
    SyncHeroOnlineStatus(false);

    // Remove listeners
    RemoveListeners();

    // Flush pending updates
    FlushResourceSync();

    m_syncing = false;
    SYNC_LOG_INFO("Stopped world sync");
}

void WorldSync::Update(float deltaTime) {
    if (!m_syncing) {
        return;
    }

    // Process pending updates
    ProcessPendingUpdates();

    // Position sync timer
    m_positionSyncTimer += deltaTime;
    if (m_positionSyncTimer >= 1.0f / m_config.heroPositionSyncRate) {
        m_positionSyncTimer = 0.0f;
        // Position is synced on-demand via SyncHeroPosition
    }

    // Resource sync timer
    m_resourceSyncTimer += deltaTime;
    if (m_resourceSyncTimer >= 1.0f / m_config.resourceSyncRate) {
        m_resourceSyncTimer = 0.0f;
        if (m_resourceUpdatePending) {
            FlushResourceSync();
        }
    }

    // Nearby player check timer
    m_nearbyPlayerTimer += deltaTime;
    if (m_nearbyPlayerTimer >= 1.0f / m_config.nearbyPlayerSyncRate) {
        m_nearbyPlayerTimer = 0.0f;
        UpdateNearbyPlayers();
    }

    // Stats timer
    m_statsTimer += deltaTime;
    if (m_statsTimer >= 1.0f) {
        m_stats.positionUpdatesPerSecond = m_positionUpdates;
        m_stats.resourceSyncsPerSecond = m_resourceSyncs;
        m_stats.buildingEventsPerSecond = m_buildingEventCount;
        m_stats.nearbyPlayersCount = static_cast<int>(m_nearbyPlayers.size());
        m_stats.pendingUpdates = static_cast<int>(m_pendingUpdates.size());

        m_positionUpdates = 0;
        m_resourceSyncs = 0;
        m_buildingEventCount = 0;
        m_statsTimer = 0.0f;

        UpdateLatencyEstimate();
    }
}

// ==================== Hero Position Sync ====================

void WorldSync::SyncHeroPosition(const glm::vec2& pos, bool forceUpdate) {
    if (!m_syncing) return;

    if (!forceUpdate && !ShouldSendPositionUpdate(pos)) {
        return;
    }

    m_lastSyncedPosition = pos;

    auto now = std::chrono::system_clock::now();
    int64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    nlohmann::json posData = {
        {"x", pos.x},
        {"y", pos.y},
        {"rotation", m_lastSyncedRotation},
        {"timestamp", timestamp},
        {"online", true}
    };

    FirebaseManager::Instance().SetValue(GetHeroPositionPath(), posData);
    m_positionUpdates++;
}

void WorldSync::SyncHeroRotation(float rotation) {
    m_lastSyncedRotation = rotation;
}

void WorldSync::SyncHeroOnlineStatus(bool online) {
    if (!m_initialized || m_playerId.empty()) return;

    nlohmann::json statusData = {
        {"online", online},
        {"lastSeen", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };

    FirebaseManager::Instance().UpdateValue(GetHeroPositionPath(), statusData);
}

// ==================== Building Sync ====================

void WorldSync::SyncBuildingPlaced(const Building& building) {
    if (!m_syncing) return;

    BuildingChangeEvent event;
    event.type = BuildingChangeEvent::Type::Placed;
    event.playerId = m_playerId;
    event.building = building;
    event.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    FirebaseManager::Instance().PushValue(GetBuildingsPath(), event.ToJson());
    m_buildingEventCount++;

    SYNC_LOG_INFO("Synced building placed: " + std::string(BuildingTypeToString(building.type)));
}

void WorldSync::SyncBuildingDestroyed(int buildingId) {
    if (!m_syncing) return;

    BuildingChangeEvent event;
    event.type = BuildingChangeEvent::Type::Destroyed;
    event.playerId = m_playerId;
    event.building.id = buildingId;
    event.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    FirebaseManager::Instance().PushValue(GetBuildingsPath(), event.ToJson());
    m_buildingEventCount++;
}

void WorldSync::SyncBuildingUpgraded(int buildingId, int newLevel) {
    if (!m_syncing) return;

    BuildingChangeEvent event;
    event.type = BuildingChangeEvent::Type::Upgraded;
    event.playerId = m_playerId;
    event.building.id = buildingId;
    event.building.level = newLevel;
    event.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    FirebaseManager::Instance().PushValue(GetBuildingsPath(), event.ToJson());
    m_buildingEventCount++;
}

void WorldSync::SyncBuildingDamaged(int buildingId, int newHealth) {
    if (!m_syncing) return;

    BuildingChangeEvent event;
    event.type = BuildingChangeEvent::Type::Damaged;
    event.playerId = m_playerId;
    event.building.id = buildingId;
    event.building.health = newHealth;
    event.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // Building damage updates are batched
    {
        std::lock_guard<std::mutex> lock(m_buildingMutex);
        m_buildingEvents.push_back(event);

        // Limit queue size
        while (m_buildingEvents.size() > 100) {
            m_buildingEvents.pop_front();
        }
    }
}

void WorldSync::OnBuildingEvent(BuildingEventCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_buildingCallbacks.push_back(std::move(callback));
}

// ==================== Resource Sync ====================

void WorldSync::SyncResources(const ResourceStock& resources) {
    if (!m_syncing) return;

    m_pendingResourceUpdate = resources;
    m_resourceUpdatePending = true;
}

void WorldSync::FlushResourceSync() {
    if (!m_resourceUpdatePending || !m_syncing) return;

    ResourceUpdate update;
    update.playerId = m_playerId;
    update.resources = m_pendingResourceUpdate;
    update.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    FirebaseManager::Instance().SetValue(GetResourcesPath(), update.ToJson());

    m_resourceUpdatePending = false;
    m_resourceSyncs++;
}

// ==================== Worker Sync ====================

void WorldSync::SyncWorkerAssignment(int workerId, WorkerJob job, int buildingId) {
    if (!m_syncing) return;

    WorkerAssignmentEvent event;
    event.playerId = m_playerId;
    event.workerId = workerId;
    event.job = job;
    event.buildingId = buildingId;
    event.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    FirebaseManager::Instance().PushValue(GetWorkersPath(), event.ToJson());
}

// ==================== Nearby Players ====================

void WorldSync::ListenForNearbyPlayers() {
    if (!m_syncing) return;

    m_playersListenerId = FirebaseManager::Instance().ListenToPath(
        GetRegionPlayersPath(),
        [this](const nlohmann::json& data) {
            if (data.is_null() || data.empty()) return;

            std::lock_guard<std::mutex> lock(m_nearbyMutex);
            m_nearbyPlayers.clear();

            for (auto& [key, playerData] : data.items()) {
                if (key == m_playerId) continue;  // Skip self

                NearbyPlayer np = NearbyPlayer::FromJson(playerData);
                np.playerId = key;

                // Calculate distance
                glm::vec2 diff = np.position - m_lastSyncedPosition;
                np.distance = std::sqrt(diff.x * diff.x + diff.y * diff.y);

                // Only track if within radius
                if (np.distance <= m_config.nearbyPlayerRadius) {
                    m_nearbyPlayers[key] = np;
                }
            }

            // Notify callbacks
            std::vector<NearbyPlayer> playerList;
            for (const auto& [id, player] : m_nearbyPlayers) {
                playerList.push_back(player);
            }

            std::lock_guard<std::mutex> callbackLock(m_callbackMutex);
            for (auto& callback : m_nearbyCallbacks) {
                if (callback) {
                    callback(playerList);
                }
            }
        }
    );
}

void WorldSync::StopListeningForNearbyPlayers() {
    if (!m_playersListenerId.empty()) {
        FirebaseManager::Instance().StopListeningById(m_playersListenerId);
        m_playersListenerId.clear();
    }
}

std::vector<NearbyPlayer> WorldSync::GetNearbyPlayers() const {
    std::lock_guard<std::mutex> lock(m_nearbyMutex);

    std::vector<NearbyPlayer> result;
    for (const auto& [id, player] : m_nearbyPlayers) {
        result.push_back(player);
    }

    // Sort by distance
    std::sort(result.begin(), result.end(),
        [](const NearbyPlayer& a, const NearbyPlayer& b) {
            return a.distance < b.distance;
        });

    return result;
}

void WorldSync::OnNearbyPlayersChanged(NearbyPlayerCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_nearbyCallbacks.push_back(std::move(callback));
}

// ==================== World Events ====================

void WorldSync::ListenForWorldEvents() {
    if (!m_syncing) return;

    m_eventsListenerId = FirebaseManager::Instance().ListenToPath(
        GetWorldEventsPath(),
        [this](const nlohmann::json& data) {
            HandleWorldEvent(data);
        }
    );
}

void WorldSync::StopListeningForWorldEvents() {
    if (!m_eventsListenerId.empty()) {
        FirebaseManager::Instance().StopListeningById(m_eventsListenerId);
        m_eventsListenerId.clear();
    }
}

void WorldSync::BroadcastWorldEvent(const WorldEvent& event) {
    if (!m_syncing) return;

    FirebaseManager::Instance().PushValue(GetWorldEventsPath(), event.ToJson());
}

void WorldSync::OnWorldEvent(WorldEventCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_worldEventCallbacks.push_back(std::move(callback));
}

// ==================== Territory Sync ====================

void WorldSync::SyncTerritoryClaim(const std::vector<glm::ivec2>& tiles) {
    if (!m_syncing) return;

    nlohmann::json tilesJson = nlohmann::json::array();
    for (const auto& tile : tiles) {
        tilesJson.push_back({tile.x, tile.y});
    }

    nlohmann::json data = {
        {"playerId", m_playerId},
        {"tiles", tilesJson},
        {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };

    FirebaseManager::Instance().SetValue(GetTerritoryPath(), data);
}

void WorldSync::SyncTerritoryStrength(float strength) {
    if (!m_syncing) return;

    nlohmann::json data = {
        {"strength", strength}
    };

    FirebaseManager::Instance().UpdateValue(GetTerritoryPath(), data);
}

// ==================== Statistics ====================

WorldSync::SyncStats WorldSync::GetStats() const {
    return m_stats;
}

// ==================== Private Methods ====================

std::string WorldSync::GetHeroPositionPath() const {
    return "rts/regions/" + m_region + "/players/" + m_playerId + "/position";
}

std::string WorldSync::GetBuildingsPath() const {
    return "rts/regions/" + m_region + "/buildings/" + m_playerId;
}

std::string WorldSync::GetResourcesPath() const {
    return "rts/regions/" + m_region + "/players/" + m_playerId + "/resources";
}

std::string WorldSync::GetWorkersPath() const {
    return "rts/regions/" + m_region + "/workers/" + m_playerId;
}

std::string WorldSync::GetRegionPlayersPath() const {
    return "rts/regions/" + m_region + "/players";
}

std::string WorldSync::GetWorldEventsPath() const {
    return "rts/regions/" + m_region + "/events";
}

std::string WorldSync::GetTerritoryPath() const {
    return "rts/regions/" + m_region + "/territory/" + m_playerId;
}

void WorldSync::SetupListeners() {
    ListenForNearbyPlayers();
    ListenForWorldEvents();

    // Listen for building events
    m_buildingsListenerId = FirebaseManager::Instance().ListenToPath(
        "rts/regions/" + m_region + "/buildings",
        [this](const nlohmann::json& data) {
            HandleBuildingEvent(data);
        }
    );
}

void WorldSync::RemoveListeners() {
    StopListeningForNearbyPlayers();
    StopListeningForWorldEvents();

    if (!m_buildingsListenerId.empty()) {
        FirebaseManager::Instance().StopListeningById(m_buildingsListenerId);
        m_buildingsListenerId.clear();
    }
}

void WorldSync::ProcessPendingUpdates() {
    std::lock_guard<std::mutex> lock(m_pendingMutex);

    while (!m_pendingUpdates.empty()) {
        auto& update = m_pendingUpdates.front();

        // Process update based on type
        switch (update.type) {
            case PendingUpdate::Type::Position:
                // Position updates are handled directly
                break;

            case PendingUpdate::Type::Building:
                FirebaseManager::Instance().PushValue(GetBuildingsPath(), update.data);
                break;

            case PendingUpdate::Type::Resource:
                FirebaseManager::Instance().SetValue(GetResourcesPath(), update.data);
                break;

            case PendingUpdate::Type::Worker:
                FirebaseManager::Instance().PushValue(GetWorkersPath(), update.data);
                break;

            case PendingUpdate::Type::Territory:
                FirebaseManager::Instance().SetValue(GetTerritoryPath(), update.data);
                break;

            case PendingUpdate::Type::Event:
                FirebaseManager::Instance().PushValue(GetWorldEventsPath(), update.data);
                break;
        }

        m_pendingUpdates.pop_front();
    }
}

void WorldSync::UpdateNearbyPlayers() {
    // This is called periodically to refresh nearby player distances
    std::lock_guard<std::mutex> lock(m_nearbyMutex);

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // Remove stale players (not seen in 5 minutes)
    for (auto it = m_nearbyPlayers.begin(); it != m_nearbyPlayers.end();) {
        if (!it->second.isOnline && (now - it->second.lastSeen) > 300) {
            it = m_nearbyPlayers.erase(it);
        } else {
            ++it;
        }
    }
}

void WorldSync::HandlePlayerUpdate(const std::string& playerId, const nlohmann::json& data) {
    if (playerId == m_playerId) return;

    std::lock_guard<std::mutex> lock(m_nearbyMutex);

    NearbyPlayer np = NearbyPlayer::FromJson(data);
    np.playerId = playerId;

    // Calculate distance
    glm::vec2 diff = np.position - m_lastSyncedPosition;
    np.distance = std::sqrt(diff.x * diff.x + diff.y * diff.y);

    if (np.distance <= m_config.nearbyPlayerRadius) {
        m_nearbyPlayers[playerId] = np;
    } else {
        m_nearbyPlayers.erase(playerId);
    }
}

void WorldSync::HandleBuildingEvent(const nlohmann::json& data) {
    if (data.is_null() || data.empty()) return;

    // Parse and notify callbacks
    for (auto& [key, eventData] : data.items()) {
        BuildingChangeEvent event = BuildingChangeEvent::FromJson(eventData);

        // Skip our own events
        if (event.playerId == m_playerId) continue;

        std::lock_guard<std::mutex> lock(m_callbackMutex);
        for (auto& callback : m_buildingCallbacks) {
            if (callback) {
                callback(event);
            }
        }
    }
}

void WorldSync::HandleWorldEvent(const nlohmann::json& data) {
    if (data.is_null() || data.empty()) return;

    WorldEvent event = WorldEvent::FromJson(data);

    std::lock_guard<std::mutex> lock(m_callbackMutex);
    for (auto& callback : m_worldEventCallbacks) {
        if (callback) {
            callback(event);
        }
    }
}

void WorldSync::UpdateLatencyEstimate() {
    // Simple latency estimation based on Firebase response time
    // In production, this would use actual ping/pong measurements
    m_stats.averageLatency = m_latency;
}

bool WorldSync::ShouldSendPositionUpdate(const glm::vec2& pos) const {
    glm::vec2 diff = pos - m_lastSyncedPosition;
    float distSq = diff.x * diff.x + diff.y * diff.y;
    float threshold = m_config.positionUpdateThreshold;
    return distSq >= threshold * threshold;
}

} // namespace Vehement
