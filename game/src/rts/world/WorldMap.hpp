#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <mutex>
#include <memory>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include "WorldRegion.hpp"
#include "PortalGate.hpp"

namespace Vehement {

/**
 * @brief Global faction controlling regions
 */
struct WorldFaction {
    int factionId = 0;
    std::string name;
    std::string description;
    glm::vec3 color{1.0f, 0.0f, 0.0f};
    std::string iconPath;
    std::string leaderPlayerId;
    int memberCount = 0;
    int controlledRegions = 0;
    float totalInfluence = 0.0f;
    std::vector<std::string> allies;
    std::vector<std::string> enemies;
    int64_t foundedTimestamp = 0;

    [[nodiscard]] nlohmann::json ToJson() const;
    static WorldFaction FromJson(const nlohmann::json& j);
};

/**
 * @brief World event affecting multiple regions
 */
struct WorldMapEvent {
    std::string eventId;
    std::string name;
    std::string description;
    std::string eventType;  // invasion, festival, disaster, war
    std::vector<std::string> affectedRegions;
    int64_t startTimestamp = 0;
    int64_t endTimestamp = 0;
    float intensity = 1.0f;
    bool active = true;
    std::unordered_map<std::string, float> regionModifiers;
    std::vector<std::string> spawnedEntities;

    [[nodiscard]] nlohmann::json ToJson() const;
    static WorldMapEvent FromJson(const nlohmann::json& j);
};

/**
 * @brief Player's global position on world map
 */
struct PlayerWorldPosition {
    std::string playerId;
    std::string currentRegionId;
    Geo::GeoCoordinate gpsPosition;
    glm::vec3 localPosition{0.0f};
    std::string travelingToRegion;
    float travelProgress = 0.0f;
    int64_t lastUpdated = 0;
    bool online = false;
    bool visible = true;

    [[nodiscard]] nlohmann::json ToJson() const;
    static PlayerWorldPosition FromJson(const nlohmann::json& j);
};

/**
 * @brief Statistics for the entire world
 */
struct WorldStatistics {
    int totalRegions = 0;
    int discoveredRegions = 0;
    int totalPlayers = 0;
    int onlinePlayers = 0;
    int totalFactions = 0;
    int activeEvents = 0;
    int totalPortals = 0;
    int64_t worldAge = 0;
    std::unordered_map<int, int> regionsPerFaction;
    std::unordered_map<std::string, int> playersPerRegion;

    [[nodiscard]] nlohmann::json ToJson() const;
    static WorldStatistics FromJson(const nlohmann::json& j);
};

/**
 * @brief Configuration for world map system
 */
struct WorldMapConfig {
    // Map display
    float defaultZoom = 1.0f;
    float minZoom = 0.1f;
    float maxZoom = 10.0f;
    bool showUndiscoveredRegions = false;
    bool showOtherPlayers = true;
    bool showFactionColors = true;
    bool showPortalConnections = true;

    // Discovery
    float autoDiscoveryRadius = 500.0f;  // meters
    bool requirePhysicalVisit = true;

    // Updates
    float positionUpdateInterval = 5.0f;  // seconds
    float mapRefreshInterval = 60.0f;     // seconds

    // Pathfinding
    int maxPathLength = 20;
    float maxPathSearchTime = 5.0f;  // seconds
};

/**
 * @brief Global world map manager
 *
 * Manages all world regions, faction territories, portal network,
 * and player positions globally.
 */
class WorldMap {
public:
    using RegionSelectedCallback = std::function<void(const std::string& regionId)>;
    using PortalSelectedCallback = std::function<void(const std::string& portalId)>;
    using PlayerMovedCallback = std::function<void(const PlayerWorldPosition& position)>;
    using EventStartedCallback = std::function<void(const WorldMapEvent& event)>;
    using FactionChangedCallback = std::function<void(const WorldFaction& faction)>;

    [[nodiscard]] static WorldMap& Instance();

    WorldMap(const WorldMap&) = delete;
    WorldMap& operator=(const WorldMap&) = delete;

    /**
     * @brief Initialize world map
     */
    [[nodiscard]] bool Initialize(const WorldMapConfig& config = {});
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    /**
     * @brief Update world map
     */
    void Update(float deltaTime);

    // ==================== Region Queries ====================

    /**
     * @brief Get total region count
     */
    [[nodiscard]] int GetRegionCount() const;

    /**
     * @brief Get all region IDs
     */
    [[nodiscard]] std::vector<std::string> GetAllRegionIds() const;

    /**
     * @brief Get regions by continent
     */
    [[nodiscard]] std::vector<std::string> GetContinentRegions(const std::string& continent) const;

    /**
     * @brief Get regions controlled by faction
     */
    [[nodiscard]] std::vector<std::string> GetFactionRegions(int factionId) const;

    /**
     * @brief Get neighboring regions
     */
    [[nodiscard]] std::vector<std::string> GetNeighboringRegions(const std::string& regionId) const;

    /**
     * @brief Get connected regions via portals
     */
    [[nodiscard]] std::vector<std::string> GetConnectedRegions(const std::string& regionId) const;

    /**
     * @brief Calculate distance between regions (km)
     */
    [[nodiscard]] double GetRegionDistance(const std::string& regionA, const std::string& regionB) const;

    // ==================== Discovery ====================

    /**
     * @brief Discover region for player
     */
    void DiscoverRegion(const std::string& regionId, const std::string& playerId);

    /**
     * @brief Get discovered regions for player
     */
    [[nodiscard]] std::vector<std::string> GetDiscoveredRegions(const std::string& playerId) const;

    /**
     * @brief Get discovery percentage
     */
    [[nodiscard]] float GetDiscoveryPercentage(const std::string& playerId) const;

    /**
     * @brief Auto-discover based on GPS position
     */
    void CheckAutoDiscovery(const std::string& playerId, const Geo::GeoCoordinate& position);

    // ==================== Factions ====================

    /**
     * @brief Register a faction
     */
    bool RegisterFaction(const WorldFaction& faction);

    /**
     * @brief Get faction
     */
    [[nodiscard]] const WorldFaction* GetFaction(int factionId) const;

    /**
     * @brief Get all factions
     */
    [[nodiscard]] std::vector<WorldFaction> GetAllFactions() const;

    /**
     * @brief Set region faction control
     */
    void SetRegionFaction(const std::string& regionId, int factionId);

    /**
     * @brief Calculate faction influence
     */
    [[nodiscard]] float GetFactionInfluence(int factionId) const;

    /**
     * @brief Get dominant faction globally
     */
    [[nodiscard]] int GetDominantFaction() const;

    // ==================== Portal Network ====================

    /**
     * @brief Find shortest path between regions
     */
    [[nodiscard]] TravelPath FindShortestPath(const std::string& from, const std::string& to) const;

    /**
     * @brief Find path avoiding certain regions
     */
    [[nodiscard]] TravelPath FindSafePath(const std::string& from, const std::string& to,
                                           const std::vector<std::string>& avoidRegions) const;

    /**
     * @brief Get all reachable regions from a starting point
     */
    [[nodiscard]] std::vector<std::string> GetReachableRegions(const std::string& fromRegion,
                                                                int maxHops = -1) const;

    /**
     * @brief Check if direct portal exists
     */
    [[nodiscard]] bool HasDirectPortal(const std::string& regionA, const std::string& regionB) const;

    // ==================== Player Positions ====================

    /**
     * @brief Update player position
     */
    void UpdatePlayerPosition(const PlayerWorldPosition& position);

    /**
     * @brief Get player position
     */
    [[nodiscard]] const PlayerWorldPosition* GetPlayerPosition(const std::string& playerId) const;

    /**
     * @brief Get players in region
     */
    [[nodiscard]] std::vector<PlayerWorldPosition> GetPlayersInRegion(const std::string& regionId) const;

    /**
     * @brief Get nearby players
     */
    [[nodiscard]] std::vector<PlayerWorldPosition> GetNearbyPlayers(
        const Geo::GeoCoordinate& center, double radiusKm) const;

    /**
     * @brief Get online player count
     */
    [[nodiscard]] int GetOnlinePlayerCount() const;

    // ==================== World Events ====================

    /**
     * @brief Start a world event
     */
    void StartEvent(const WorldMapEvent& event);

    /**
     * @brief End a world event
     */
    void EndEvent(const std::string& eventId);

    /**
     * @brief Get active events
     */
    [[nodiscard]] std::vector<WorldMapEvent> GetActiveEvents() const;

    /**
     * @brief Get events affecting region
     */
    [[nodiscard]] std::vector<WorldMapEvent> GetRegionEvents(const std::string& regionId) const;

    // ==================== Statistics ====================

    /**
     * @brief Get world statistics
     */
    [[nodiscard]] WorldStatistics GetStatistics() const;

    /**
     * @brief Get leaderboard by faction control
     */
    [[nodiscard]] std::vector<std::pair<int, int>> GetFactionLeaderboard() const;

    /**
     * @brief Get player exploration leaderboard
     */
    [[nodiscard]] std::vector<std::pair<std::string, float>> GetExplorationLeaderboard(int limit = 10) const;

    // ==================== Map View ====================

    /**
     * @brief Get map view bounds
     */
    [[nodiscard]] GPSBounds GetViewBounds() const;

    /**
     * @brief Set map view center
     */
    void SetViewCenter(const Geo::GeoCoordinate& center);

    /**
     * @brief Set map zoom level
     */
    void SetZoom(float zoom);

    /**
     * @brief Get current zoom level
     */
    [[nodiscard]] float GetZoom() const { return m_currentZoom; }

    /**
     * @brief Pan map by offset
     */
    void PanMap(double latOffset, double lonOffset);

    /**
     * @brief Zoom to fit region
     */
    void ZoomToRegion(const std::string& regionId);

    /**
     * @brief Zoom to fit all discovered regions
     */
    void ZoomToDiscovered(const std::string& playerId);

    // ==================== Selection ====================

    /**
     * @brief Select a region
     */
    void SelectRegion(const std::string& regionId);

    /**
     * @brief Get selected region
     */
    [[nodiscard]] std::string GetSelectedRegion() const { return m_selectedRegionId; }

    /**
     * @brief Clear selection
     */
    void ClearSelection();

    // ==================== Synchronization ====================

    void SyncToServer();
    void LoadFromServer();
    void ListenForChanges();
    void StopListening();

    // ==================== Callbacks ====================

    void OnRegionSelected(RegionSelectedCallback callback);
    void OnPortalSelected(PortalSelectedCallback callback);
    void OnPlayerMoved(PlayerMovedCallback callback);
    void OnEventStarted(EventStartedCallback callback);
    void OnFactionChanged(FactionChangedCallback callback);

    // ==================== Configuration ====================

    void SetLocalPlayerId(const std::string& playerId) { m_localPlayerId = playerId; }
    [[nodiscard]] const WorldMapConfig& GetConfig() const { return m_config; }
    void SetConfig(const WorldMapConfig& config) { m_config = config; }

private:
    WorldMap() = default;
    ~WorldMap() = default;

    void UpdatePlayerPositions(float deltaTime);
    void UpdateWorldEvents(float deltaTime);
    void RefreshStatistics();
    void BuildRegionGraph();

    bool m_initialized = false;
    WorldMapConfig m_config;
    std::string m_localPlayerId;

    // View state
    Geo::GeoCoordinate m_viewCenter;
    float m_currentZoom = 1.0f;
    std::string m_selectedRegionId;

    // Factions
    std::unordered_map<int, WorldFaction> m_factions;
    mutable std::mutex m_factionsMutex;

    // Player positions
    std::unordered_map<std::string, PlayerWorldPosition> m_playerPositions;
    mutable std::mutex m_positionsMutex;

    // World events
    std::unordered_map<std::string, WorldMapEvent> m_activeEvents;
    std::mutex m_eventsMutex;

    // Discovery tracking
    std::unordered_map<std::string, std::unordered_set<std::string>> m_playerDiscoveries;
    mutable std::mutex m_discoveriesMutex;

    // Region connectivity graph
    std::unordered_map<std::string, std::vector<std::string>> m_regionConnections;
    mutable std::mutex m_graphMutex;
    bool m_graphDirty = true;

    // Statistics
    WorldStatistics m_cachedStats;
    mutable std::mutex m_statsMutex;
    float m_statsRefreshTimer = 0.0f;

    // Update timers
    float m_positionUpdateTimer = 0.0f;
    float m_mapRefreshTimer = 0.0f;

    // Callbacks
    std::vector<RegionSelectedCallback> m_regionSelectedCallbacks;
    std::vector<PortalSelectedCallback> m_portalSelectedCallbacks;
    std::vector<PlayerMovedCallback> m_playerMovedCallbacks;
    std::vector<EventStartedCallback> m_eventStartedCallbacks;
    std::vector<FactionChangedCallback> m_factionChangedCallbacks;
    std::mutex m_callbackMutex;
};

} // namespace Vehement
