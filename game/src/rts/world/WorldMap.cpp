#include "WorldMap.hpp"
#include "../../network/FirebaseManager.hpp"
#include <algorithm>
#include <queue>
#include <chrono>

namespace Vehement {

// ============================================================================
// WorldFaction Implementation
// ============================================================================

nlohmann::json WorldFaction::ToJson() const {
    return {
        {"factionId", factionId},
        {"name", name},
        {"description", description},
        {"color", {color.r, color.g, color.b}},
        {"iconPath", iconPath},
        {"leaderPlayerId", leaderPlayerId},
        {"memberCount", memberCount},
        {"controlledRegions", controlledRegions},
        {"totalInfluence", totalInfluence},
        {"allies", allies},
        {"enemies", enemies},
        {"foundedTimestamp", foundedTimestamp}
    };
}

WorldFaction WorldFaction::FromJson(const nlohmann::json& j) {
    WorldFaction f;
    f.factionId = j.value("factionId", 0);
    f.name = j.value("name", "");
    f.description = j.value("description", "");

    if (j.contains("color") && j["color"].is_array() && j["color"].size() >= 3) {
        f.color = glm::vec3(j["color"][0], j["color"][1], j["color"][2]);
    }

    f.iconPath = j.value("iconPath", "");
    f.leaderPlayerId = j.value("leaderPlayerId", "");
    f.memberCount = j.value("memberCount", 0);
    f.controlledRegions = j.value("controlledRegions", 0);
    f.totalInfluence = j.value("totalInfluence", 0.0f);
    f.foundedTimestamp = j.value("foundedTimestamp", 0);

    auto loadStrArray = [&j](const std::string& key) {
        std::vector<std::string> result;
        if (j.contains(key) && j[key].is_array()) {
            for (const auto& item : j[key]) {
                result.push_back(item.get<std::string>());
            }
        }
        return result;
    };

    f.allies = loadStrArray("allies");
    f.enemies = loadStrArray("enemies");

    return f;
}

// ============================================================================
// WorldMapEvent Implementation
// ============================================================================

nlohmann::json WorldMapEvent::ToJson() const {
    nlohmann::json modifiersJson;
    for (const auto& [k, v] : regionModifiers) modifiersJson[k] = v;

    return {
        {"eventId", eventId},
        {"name", name},
        {"description", description},
        {"eventType", eventType},
        {"affectedRegions", affectedRegions},
        {"startTimestamp", startTimestamp},
        {"endTimestamp", endTimestamp},
        {"intensity", intensity},
        {"active", active},
        {"regionModifiers", modifiersJson},
        {"spawnedEntities", spawnedEntities}
    };
}

WorldMapEvent WorldMapEvent::FromJson(const nlohmann::json& j) {
    WorldMapEvent e;
    e.eventId = j.value("eventId", "");
    e.name = j.value("name", "");
    e.description = j.value("description", "");
    e.eventType = j.value("eventType", "");
    e.startTimestamp = j.value("startTimestamp", 0);
    e.endTimestamp = j.value("endTimestamp", 0);
    e.intensity = j.value("intensity", 1.0f);
    e.active = j.value("active", true);

    auto loadStrArray = [&j](const std::string& key) {
        std::vector<std::string> result;
        if (j.contains(key) && j[key].is_array()) {
            for (const auto& item : j[key]) result.push_back(item.get<std::string>());
        }
        return result;
    };

    e.affectedRegions = loadStrArray("affectedRegions");
    e.spawnedEntities = loadStrArray("spawnedEntities");

    if (j.contains("regionModifiers") && j["regionModifiers"].is_object()) {
        for (auto& [k, v] : j["regionModifiers"].items()) {
            e.regionModifiers[k] = v.get<float>();
        }
    }

    return e;
}

// ============================================================================
// PlayerWorldPosition Implementation
// ============================================================================

nlohmann::json PlayerWorldPosition::ToJson() const {
    return {
        {"playerId", playerId},
        {"currentRegionId", currentRegionId},
        {"gpsPosition", {{"lat", gpsPosition.latitude}, {"lon", gpsPosition.longitude}}},
        {"localPosition", {localPosition.x, localPosition.y, localPosition.z}},
        {"travelingToRegion", travelingToRegion},
        {"travelProgress", travelProgress},
        {"lastUpdated", lastUpdated},
        {"online", online},
        {"visible", visible}
    };
}

PlayerWorldPosition PlayerWorldPosition::FromJson(const nlohmann::json& j) {
    PlayerWorldPosition p;
    p.playerId = j.value("playerId", "");
    p.currentRegionId = j.value("currentRegionId", "");

    if (j.contains("gpsPosition")) {
        p.gpsPosition.latitude = j["gpsPosition"].value("lat", 0.0);
        p.gpsPosition.longitude = j["gpsPosition"].value("lon", 0.0);
    }

    if (j.contains("localPosition") && j["localPosition"].is_array() && j["localPosition"].size() >= 3) {
        p.localPosition = glm::vec3(j["localPosition"][0], j["localPosition"][1], j["localPosition"][2]);
    }

    p.travelingToRegion = j.value("travelingToRegion", "");
    p.travelProgress = j.value("travelProgress", 0.0f);
    p.lastUpdated = j.value("lastUpdated", 0);
    p.online = j.value("online", false);
    p.visible = j.value("visible", true);

    return p;
}

// ============================================================================
// WorldStatistics Implementation
// ============================================================================

nlohmann::json WorldStatistics::ToJson() const {
    nlohmann::json regionsPerFactionJson, playersPerRegionJson;
    for (const auto& [k, v] : regionsPerFaction) regionsPerFactionJson[std::to_string(k)] = v;
    for (const auto& [k, v] : playersPerRegion) playersPerRegionJson[k] = v;

    return {
        {"totalRegions", totalRegions},
        {"discoveredRegions", discoveredRegions},
        {"totalPlayers", totalPlayers},
        {"onlinePlayers", onlinePlayers},
        {"totalFactions", totalFactions},
        {"activeEvents", activeEvents},
        {"totalPortals", totalPortals},
        {"worldAge", worldAge},
        {"regionsPerFaction", regionsPerFactionJson},
        {"playersPerRegion", playersPerRegionJson}
    };
}

WorldStatistics WorldStatistics::FromJson(const nlohmann::json& j) {
    WorldStatistics s;
    s.totalRegions = j.value("totalRegions", 0);
    s.discoveredRegions = j.value("discoveredRegions", 0);
    s.totalPlayers = j.value("totalPlayers", 0);
    s.onlinePlayers = j.value("onlinePlayers", 0);
    s.totalFactions = j.value("totalFactions", 0);
    s.activeEvents = j.value("activeEvents", 0);
    s.totalPortals = j.value("totalPortals", 0);
    s.worldAge = j.value("worldAge", 0);

    if (j.contains("regionsPerFaction") && j["regionsPerFaction"].is_object()) {
        for (auto& [k, v] : j["regionsPerFaction"].items()) {
            s.regionsPerFaction[std::stoi(k)] = v.get<int>();
        }
    }
    if (j.contains("playersPerRegion") && j["playersPerRegion"].is_object()) {
        for (auto& [k, v] : j["playersPerRegion"].items()) {
            s.playersPerRegion[k] = v.get<int>();
        }
    }

    return s;
}

// ============================================================================
// WorldMap Implementation
// ============================================================================

WorldMap& WorldMap::Instance() {
    static WorldMap instance;
    return instance;
}

bool WorldMap::Initialize(const WorldMapConfig& config) {
    if (m_initialized) return true;

    m_config = config;
    m_currentZoom = config.defaultZoom;
    m_viewCenter = Geo::GeoCoordinate(0.0, 0.0);
    m_initialized = true;
    m_graphDirty = true;

    return true;
}

void WorldMap::Shutdown() {
    StopListening();

    m_factions.clear();
    m_playerPositions.clear();
    m_activeEvents.clear();
    m_playerDiscoveries.clear();
    m_regionConnections.clear();
    m_initialized = false;
}

void WorldMap::Update(float deltaTime) {
    if (!m_initialized) return;

    m_positionUpdateTimer += deltaTime;
    m_mapRefreshTimer += deltaTime;
    m_statsRefreshTimer += deltaTime;

    if (m_positionUpdateTimer >= m_config.positionUpdateInterval) {
        UpdatePlayerPositions(m_positionUpdateTimer);
        m_positionUpdateTimer = 0.0f;
    }

    if (m_mapRefreshTimer >= m_config.mapRefreshInterval) {
        if (m_graphDirty) {
            BuildRegionGraph();
            m_graphDirty = false;
        }
        m_mapRefreshTimer = 0.0f;
    }

    if (m_statsRefreshTimer >= 60.0f) {
        RefreshStatistics();
        m_statsRefreshTimer = 0.0f;
    }

    UpdateWorldEvents(deltaTime);
}

int WorldMap::GetRegionCount() const {
    return static_cast<int>(RegionManager::Instance().GetAllRegions().size());
}

std::vector<std::string> WorldMap::GetAllRegionIds() const {
    std::vector<std::string> ids;
    auto regions = RegionManager::Instance().GetAllRegions();
    ids.reserve(regions.size());
    for (const auto* region : regions) {
        ids.push_back(region->id);
    }
    return ids;
}

std::vector<std::string> WorldMap::GetContinentRegions(const std::string& continent) const {
    std::vector<std::string> ids;
    auto regions = RegionManager::Instance().GetRegionsByContinent(continent);
    for (const auto* region : regions) {
        ids.push_back(region->id);
    }
    return ids;
}

std::vector<std::string> WorldMap::GetFactionRegions(int factionId) const {
    std::vector<std::string> ids;
    auto regions = RegionManager::Instance().GetAllRegions();
    for (const auto* region : regions) {
        if (region->controllingFaction == factionId) {
            ids.push_back(region->id);
        }
    }
    return ids;
}

std::vector<std::string> WorldMap::GetNeighboringRegions(const std::string& regionId) const {
    std::lock_guard<std::mutex> lock(m_graphMutex);
    auto it = m_regionConnections.find(regionId);
    if (it != m_regionConnections.end()) {
        return it->second;
    }
    return {};
}

std::vector<std::string> WorldMap::GetConnectedRegions(const std::string& regionId) const {
    auto portals = PortalManager::Instance().GetPortalsInRegion(regionId);
    std::vector<std::string> connected;
    for (const auto* portal : portals) {
        if (!portal->destinationRegionId.empty()) {
            connected.push_back(portal->destinationRegionId);
        }
    }
    return connected;
}

double WorldMap::GetRegionDistance(const std::string& regionA, const std::string& regionB) const {
    auto& rm = RegionManager::Instance();
    const auto* a = rm.GetRegion(regionA);
    const auto* b = rm.GetRegion(regionB);
    if (!a || !b) return -1.0;

    return a->centerPoint.DistanceTo(b->centerPoint) / 1000.0;  // Convert to km
}

void WorldMap::DiscoverRegion(const std::string& regionId, const std::string& playerId) {
    std::lock_guard<std::mutex> lock(m_discoveriesMutex);
    m_playerDiscoveries[playerId].insert(regionId);

    RegionManager::Instance().DiscoverRegion(regionId, playerId);
}

std::vector<std::string> WorldMap::GetDiscoveredRegions(const std::string& playerId) const {
    std::lock_guard<std::mutex> lock(m_discoveriesMutex);
    auto it = m_playerDiscoveries.find(playerId);
    if (it != m_playerDiscoveries.end()) {
        return std::vector<std::string>(it->second.begin(), it->second.end());
    }
    return {};
}

float WorldMap::GetDiscoveryPercentage(const std::string& playerId) const {
    int total = GetRegionCount();
    if (total == 0) return 0.0f;

    auto discovered = GetDiscoveredRegions(playerId);
    return static_cast<float>(discovered.size()) / total * 100.0f;
}

void WorldMap::CheckAutoDiscovery(const std::string& playerId, const Geo::GeoCoordinate& position) {
    auto& rm = RegionManager::Instance();
    auto regions = rm.FindRegionsInRadius(position, m_config.autoDiscoveryRadius / 1000.0);

    for (const auto* region : regions) {
        std::lock_guard<std::mutex> lock(m_discoveriesMutex);
        if (m_playerDiscoveries[playerId].find(region->id) == m_playerDiscoveries[playerId].end()) {
            m_playerDiscoveries[playerId].insert(region->id);
            rm.DiscoverRegion(region->id, playerId);
        }
    }
}

bool WorldMap::RegisterFaction(const WorldFaction& faction) {
    std::lock_guard<std::mutex> lock(m_factionsMutex);
    if (m_factions.find(faction.factionId) != m_factions.end()) {
        return false;
    }
    m_factions[faction.factionId] = faction;
    return true;
}

const WorldFaction* WorldMap::GetFaction(int factionId) const {
    std::lock_guard<std::mutex> lock(m_factionsMutex);
    auto it = m_factions.find(factionId);
    return it != m_factions.end() ? &it->second : nullptr;
}

std::vector<WorldFaction> WorldMap::GetAllFactions() const {
    std::lock_guard<std::mutex> lock(m_factionsMutex);
    std::vector<WorldFaction> result;
    result.reserve(m_factions.size());
    for (const auto& [id, faction] : m_factions) {
        result.push_back(faction);
    }
    return result;
}

void WorldMap::SetRegionFaction(const std::string& regionId, int factionId) {
    RegionManager::Instance().SetRegionControl(regionId, factionId, "", 100.0f);

    std::lock_guard<std::mutex> lock(m_factionsMutex);
    // Update faction statistics
    for (auto& [id, faction] : m_factions) {
        faction.controlledRegions = static_cast<int>(GetFactionRegions(id).size());
    }
}

float WorldMap::GetFactionInfluence(int factionId) const {
    int factionRegions = static_cast<int>(GetFactionRegions(factionId).size());
    int totalRegions = GetRegionCount();
    if (totalRegions == 0) return 0.0f;
    return static_cast<float>(factionRegions) / totalRegions * 100.0f;
}

int WorldMap::GetDominantFaction() const {
    std::lock_guard<std::mutex> lock(m_factionsMutex);
    int dominantId = -1;
    int maxRegions = 0;

    for (const auto& [id, faction] : m_factions) {
        if (faction.controlledRegions > maxRegions) {
            maxRegions = faction.controlledRegions;
            dominantId = id;
        }
    }

    return dominantId;
}

TravelPath WorldMap::FindShortestPath(const std::string& from, const std::string& to) const {
    return PortalManager::Instance().FindPath(from, to, 100, {});
}

TravelPath WorldMap::FindSafePath(const std::string& from, const std::string& to,
                                   const std::vector<std::string>& avoidRegions) const {
    // Simple implementation - find path and check if it avoids regions
    auto path = FindShortestPath(from, to);

    for (const auto& regionId : path.regionIds) {
        if (std::find(avoidRegions.begin(), avoidRegions.end(), regionId) != avoidRegions.end()) {
            path.valid = false;
            path.invalidReason = "Path passes through avoided region: " + regionId;
            break;
        }
    }

    return path;
}

std::vector<std::string> WorldMap::GetReachableRegions(const std::string& fromRegion, int maxHops) const {
    std::unordered_set<std::string> visited;
    std::queue<std::pair<std::string, int>> queue;
    queue.push({fromRegion, 0});
    visited.insert(fromRegion);

    while (!queue.empty()) {
        auto [current, hops] = queue.front();
        queue.pop();

        if (maxHops >= 0 && hops >= maxHops) continue;

        auto connected = GetConnectedRegions(current);
        for (const auto& region : connected) {
            if (visited.find(region) == visited.end()) {
                visited.insert(region);
                queue.push({region, hops + 1});
            }
        }
    }

    return std::vector<std::string>(visited.begin(), visited.end());
}

bool WorldMap::HasDirectPortal(const std::string& regionA, const std::string& regionB) const {
    auto portals = PortalManager::Instance().GetPortalsInRegion(regionA);
    for (const auto* portal : portals) {
        if (portal->destinationRegionId == regionB) {
            return true;
        }
    }
    return false;
}

void WorldMap::UpdatePlayerPosition(const PlayerWorldPosition& position) {
    std::lock_guard<std::mutex> lock(m_positionsMutex);
    m_playerPositions[position.playerId] = position;

    // Notify callbacks
    std::lock_guard<std::mutex> cbLock(m_callbackMutex);
    for (const auto& cb : m_playerMovedCallbacks) {
        cb(position);
    }
}

const PlayerWorldPosition* WorldMap::GetPlayerPosition(const std::string& playerId) const {
    std::lock_guard<std::mutex> lock(m_positionsMutex);
    auto it = m_playerPositions.find(playerId);
    return it != m_playerPositions.end() ? &it->second : nullptr;
}

std::vector<PlayerWorldPosition> WorldMap::GetPlayersInRegion(const std::string& regionId) const {
    std::lock_guard<std::mutex> lock(m_positionsMutex);
    std::vector<PlayerWorldPosition> result;
    for (const auto& [id, pos] : m_playerPositions) {
        if (pos.currentRegionId == regionId && pos.visible) {
            result.push_back(pos);
        }
    }
    return result;
}

std::vector<PlayerWorldPosition> WorldMap::GetNearbyPlayers(
    const Geo::GeoCoordinate& center, double radiusKm) const {
    std::lock_guard<std::mutex> lock(m_positionsMutex);
    std::vector<PlayerWorldPosition> result;
    double radiusMeters = radiusKm * 1000.0;

    for (const auto& [id, pos] : m_playerPositions) {
        if (!pos.visible) continue;
        double dist = center.DistanceTo(pos.gpsPosition);
        if (dist <= radiusMeters) {
            result.push_back(pos);
        }
    }
    return result;
}

int WorldMap::GetOnlinePlayerCount() const {
    std::lock_guard<std::mutex> lock(m_positionsMutex);
    int count = 0;
    for (const auto& [id, pos] : m_playerPositions) {
        if (pos.online) count++;
    }
    return count;
}

void WorldMap::StartEvent(const WorldMapEvent& event) {
    std::lock_guard<std::mutex> lock(m_eventsMutex);
    m_activeEvents[event.eventId] = event;

    std::lock_guard<std::mutex> cbLock(m_callbackMutex);
    for (const auto& cb : m_eventStartedCallbacks) {
        cb(event);
    }
}

void WorldMap::EndEvent(const std::string& eventId) {
    std::lock_guard<std::mutex> lock(m_eventsMutex);
    auto it = m_activeEvents.find(eventId);
    if (it != m_activeEvents.end()) {
        it->second.active = false;
        m_activeEvents.erase(it);
    }
}

std::vector<WorldMapEvent> WorldMap::GetActiveEvents() const {
    std::lock_guard<std::mutex> lock(m_eventsMutex);
    std::vector<WorldMapEvent> result;
    for (const auto& [id, event] : m_activeEvents) {
        if (event.active) {
            result.push_back(event);
        }
    }
    return result;
}

std::vector<WorldMapEvent> WorldMap::GetRegionEvents(const std::string& regionId) const {
    std::lock_guard<std::mutex> lock(m_eventsMutex);
    std::vector<WorldMapEvent> result;
    for (const auto& [id, event] : m_activeEvents) {
        if (!event.active) continue;
        if (std::find(event.affectedRegions.begin(), event.affectedRegions.end(), regionId) !=
            event.affectedRegions.end()) {
            result.push_back(event);
        }
    }
    return result;
}

WorldStatistics WorldMap::GetStatistics() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_cachedStats;
}

std::vector<std::pair<int, int>> WorldMap::GetFactionLeaderboard() const {
    std::lock_guard<std::mutex> lock(m_factionsMutex);
    std::vector<std::pair<int, int>> leaderboard;
    for (const auto& [id, faction] : m_factions) {
        leaderboard.emplace_back(id, faction.controlledRegions);
    }
    std::sort(leaderboard.begin(), leaderboard.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    return leaderboard;
}

std::vector<std::pair<std::string, float>> WorldMap::GetExplorationLeaderboard(int limit) const {
    std::lock_guard<std::mutex> lock(m_discoveriesMutex);
    std::vector<std::pair<std::string, float>> leaderboard;
    int totalRegions = GetRegionCount();

    for (const auto& [playerId, discovered] : m_playerDiscoveries) {
        float percent = totalRegions > 0 ?
                       static_cast<float>(discovered.size()) / totalRegions * 100.0f : 0.0f;
        leaderboard.emplace_back(playerId, percent);
    }

    std::sort(leaderboard.begin(), leaderboard.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    if (limit > 0 && static_cast<int>(leaderboard.size()) > limit) {
        leaderboard.resize(limit);
    }

    return leaderboard;
}

GPSBounds WorldMap::GetViewBounds() const {
    // Calculate bounds based on view center and zoom
    double latRange = 90.0 / m_currentZoom;
    double lonRange = 180.0 / m_currentZoom;

    return GPSBounds(
        m_viewCenter.latitude - latRange,
        m_viewCenter.longitude - lonRange,
        m_viewCenter.latitude + latRange,
        m_viewCenter.longitude + lonRange
    );
}

void WorldMap::SetViewCenter(const Geo::GeoCoordinate& center) {
    m_viewCenter = center;
}

void WorldMap::SetZoom(float zoom) {
    m_currentZoom = std::clamp(zoom, m_config.minZoom, m_config.maxZoom);
}

void WorldMap::PanMap(double latOffset, double lonOffset) {
    m_viewCenter.latitude = std::clamp(m_viewCenter.latitude + latOffset, -90.0, 90.0);
    m_viewCenter.longitude = std::clamp(m_viewCenter.longitude + lonOffset, -180.0, 180.0);
}

void WorldMap::ZoomToRegion(const std::string& regionId) {
    const auto* region = RegionManager::Instance().GetRegion(regionId);
    if (!region) return;

    m_viewCenter = region->centerPoint;

    double latRange = region->bounds.northeast.latitude - region->bounds.southwest.latitude;
    double lonRange = region->bounds.northeast.longitude - region->bounds.southwest.longitude;
    double maxRange = std::max(latRange, lonRange);

    m_currentZoom = std::clamp(static_cast<float>(10.0 / maxRange), m_config.minZoom, m_config.maxZoom);
}

void WorldMap::ZoomToDiscovered(const std::string& playerId) {
    auto discovered = GetDiscoveredRegions(playerId);
    if (discovered.empty()) return;

    double minLat = 90.0, maxLat = -90.0;
    double minLon = 180.0, maxLon = -180.0;

    for (const auto& regionId : discovered) {
        const auto* region = RegionManager::Instance().GetRegion(regionId);
        if (!region) continue;

        minLat = std::min(minLat, region->bounds.southwest.latitude);
        maxLat = std::max(maxLat, region->bounds.northeast.latitude);
        minLon = std::min(minLon, region->bounds.southwest.longitude);
        maxLon = std::max(maxLon, region->bounds.northeast.longitude);
    }

    m_viewCenter.latitude = (minLat + maxLat) / 2.0;
    m_viewCenter.longitude = (minLon + maxLon) / 2.0;

    double latRange = maxLat - minLat;
    double lonRange = maxLon - minLon;
    double maxRange = std::max(latRange, lonRange);

    m_currentZoom = std::clamp(static_cast<float>(90.0 / maxRange), m_config.minZoom, m_config.maxZoom);
}

void WorldMap::SelectRegion(const std::string& regionId) {
    m_selectedRegionId = regionId;

    std::lock_guard<std::mutex> lock(m_callbackMutex);
    for (const auto& cb : m_regionSelectedCallbacks) {
        cb(regionId);
    }
}

void WorldMap::ClearSelection() {
    m_selectedRegionId.clear();
}

void WorldMap::SyncToServer() {
    auto& firebase = FirebaseManager::Instance();
    if (!firebase.IsInitialized()) return;

    std::lock_guard<std::mutex> fLock(m_factionsMutex);
    for (const auto& [id, faction] : m_factions) {
        firebase.SetValue("world/factions/" + std::to_string(id), faction.ToJson());
    }
}

void WorldMap::LoadFromServer() {
    auto& firebase = FirebaseManager::Instance();
    if (!firebase.IsInitialized()) return;

    firebase.GetValue("world/factions", [this](const nlohmann::json& data) {
        if (!data.is_object()) return;

        std::lock_guard<std::mutex> lock(m_factionsMutex);
        for (auto& [key, val] : data.items()) {
            m_factions[std::stoi(key)] = WorldFaction::FromJson(val);
        }
    });
}

void WorldMap::ListenForChanges() {
    auto& firebase = FirebaseManager::Instance();
    if (!firebase.IsInitialized()) return;

    firebase.ListenToPath("world/factions", [this](const nlohmann::json& data) {
        if (!data.is_object()) return;

        std::lock_guard<std::mutex> lock(m_factionsMutex);
        for (auto& [key, val] : data.items()) {
            m_factions[std::stoi(key)] = WorldFaction::FromJson(val);
        }
    });

    firebase.ListenToPath("world/players", [this](const nlohmann::json& data) {
        if (!data.is_object()) return;

        std::lock_guard<std::mutex> lock(m_positionsMutex);
        for (auto& [key, val] : data.items()) {
            m_playerPositions[key] = PlayerWorldPosition::FromJson(val);
        }
    });
}

void WorldMap::StopListening() {
    auto& firebase = FirebaseManager::Instance();
    if (firebase.IsInitialized()) {
        firebase.StopListening("world/factions");
        firebase.StopListening("world/players");
    }
}

void WorldMap::OnRegionSelected(RegionSelectedCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_regionSelectedCallbacks.push_back(std::move(callback));
}

void WorldMap::OnPortalSelected(PortalSelectedCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_portalSelectedCallbacks.push_back(std::move(callback));
}

void WorldMap::OnPlayerMoved(PlayerMovedCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_playerMovedCallbacks.push_back(std::move(callback));
}

void WorldMap::OnEventStarted(EventStartedCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_eventStartedCallbacks.push_back(std::move(callback));
}

void WorldMap::OnFactionChanged(FactionChangedCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_factionChangedCallbacks.push_back(std::move(callback));
}

void WorldMap::UpdatePlayerPositions(float deltaTime) {
    // Sync local player position to server
    if (!m_localPlayerId.empty()) {
        std::lock_guard<std::mutex> lock(m_positionsMutex);
        auto it = m_playerPositions.find(m_localPlayerId);
        if (it != m_playerPositions.end()) {
            it->second.lastUpdated = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            auto& firebase = FirebaseManager::Instance();
            if (firebase.IsInitialized()) {
                firebase.SetValue("world/players/" + m_localPlayerId, it->second.ToJson());
            }
        }
    }
}

void WorldMap::UpdateWorldEvents(float deltaTime) {
    std::lock_guard<std::mutex> lock(m_eventsMutex);
    int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::vector<std::string> expiredEvents;
    for (auto& [id, event] : m_activeEvents) {
        if (event.endTimestamp > 0 && now >= event.endTimestamp) {
            event.active = false;
            expiredEvents.push_back(id);
        }
    }

    for (const auto& id : expiredEvents) {
        m_activeEvents.erase(id);
    }
}

void WorldMap::RefreshStatistics() {
    std::lock_guard<std::mutex> lock(m_statsMutex);

    m_cachedStats.totalRegions = GetRegionCount();
    m_cachedStats.totalPlayers = static_cast<int>(m_playerPositions.size());
    m_cachedStats.onlinePlayers = GetOnlinePlayerCount();
    m_cachedStats.totalFactions = static_cast<int>(m_factions.size());
    m_cachedStats.activeEvents = static_cast<int>(m_activeEvents.size());
    m_cachedStats.totalPortals = static_cast<int>(PortalManager::Instance().GetAllPortals().size());

    m_cachedStats.regionsPerFaction.clear();
    for (const auto& [id, faction] : m_factions) {
        m_cachedStats.regionsPerFaction[id] = static_cast<int>(GetFactionRegions(id).size());
    }

    m_cachedStats.playersPerRegion.clear();
    for (const auto& [id, pos] : m_playerPositions) {
        m_cachedStats.playersPerRegion[pos.currentRegionId]++;
    }
}

void WorldMap::BuildRegionGraph() {
    std::lock_guard<std::mutex> lock(m_graphMutex);
    m_regionConnections.clear();

    auto regions = RegionManager::Instance().GetAllRegions();
    auto portals = PortalManager::Instance().GetAllPortals();

    for (const auto* portal : portals) {
        if (!portal->destinationRegionId.empty()) {
            m_regionConnections[portal->regionId].push_back(portal->destinationRegionId);
            if (portal->bidirectional) {
                m_regionConnections[portal->destinationRegionId].push_back(portal->regionId);
            }
        }
    }
}

} // namespace Vehement
