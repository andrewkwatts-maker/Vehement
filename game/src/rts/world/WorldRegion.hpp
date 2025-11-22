#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <optional>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include "../../geodata/GeoTypes.hpp"

namespace Vehement {

// Forward declarations
class FirebaseManager;

/**
 * @brief GPS bounds for a world region
 */
struct GPSBounds {
    Geo::GeoCoordinate southwest;  // Min lat/lon corner
    Geo::GeoCoordinate northeast;  // Max lat/lon corner

    GPSBounds() = default;
    GPSBounds(double minLat, double minLon, double maxLat, double maxLon)
        : southwest(minLat, minLon), northeast(maxLat, maxLon) {}
    GPSBounds(const Geo::GeoCoordinate& sw, const Geo::GeoCoordinate& ne)
        : southwest(sw), northeast(ne) {}

    /**
     * @brief Check if a coordinate is within bounds
     */
    [[nodiscard]] bool Contains(const Geo::GeoCoordinate& coord) const {
        return coord.latitude >= southwest.latitude &&
               coord.latitude <= northeast.latitude &&
               coord.longitude >= southwest.longitude &&
               coord.longitude <= northeast.longitude;
    }

    /**
     * @brief Get center point of bounds
     */
    [[nodiscard]] Geo::GeoCoordinate GetCenter() const {
        return Geo::GeoCoordinate(
            (southwest.latitude + northeast.latitude) / 2.0,
            (southwest.longitude + northeast.longitude) / 2.0
        );
    }

    /**
     * @brief Get approximate area in square kilometers
     */
    [[nodiscard]] double GetAreaKm2() const;

    /**
     * @brief Check if bounds intersect with another
     */
    [[nodiscard]] bool Intersects(const GPSBounds& other) const;

    [[nodiscard]] nlohmann::json ToJson() const;
    static GPSBounds FromJson(const nlohmann::json& j);
};

/**
 * @brief Biome types for regions
 */
enum class RegionBiome : uint8_t {
    Unknown = 0,
    Temperate,
    Desert,
    Arctic,
    Tropical,
    Mountain,
    Ocean,
    Forest,
    Swamp,
    Plains,
    Volcanic,
    Mystical,      // Special magical regions
    Corrupted,     // Dark/dangerous regions
    Ancient,       // Old civilization ruins
    Dimensional    // Extra-dimensional spaces
};

const char* RegionBiomeToString(RegionBiome biome);
RegionBiome RegionBiomeFromString(const std::string& str);

/**
 * @brief Resource node within a region
 */
struct ResourceNode {
    std::string id;
    std::string resourceType;  // gold, wood, stone, mana, etc.
    Geo::GeoCoordinate location;
    float baseYield = 100.0f;
    float currentYield = 100.0f;
    float regenerationRate = 1.0f;  // Per hour
    float maxYield = 1000.0f;
    bool depleted = false;
    int64_t lastHarvestTime = 0;
    std::string controllingPlayerId;

    [[nodiscard]] nlohmann::json ToJson() const;
    static ResourceNode FromJson(const nlohmann::json& j);
};

/**
 * @brief Portal connection to another region
 */
struct PortalConnection {
    std::string portalId;
    std::string targetRegionId;
    std::string targetPortalId;
    Geo::GeoCoordinate location;
    bool bidirectional = true;
    bool active = true;
    int minLevel = 1;
    std::vector<std::string> requiredQuests;
    std::unordered_map<std::string, int> resourceCost;
    float travelTimeSeconds = 30.0f;

    [[nodiscard]] nlohmann::json ToJson() const;
    static PortalConnection FromJson(const nlohmann::json& j);
};

/**
 * @brief Regional quest available in a region
 */
struct RegionalQuest {
    std::string questId;
    std::string name;
    std::string description;
    int minLevel = 1;
    int maxLevel = 100;
    bool repeatable = false;
    int64_t cooldownHours = 24;
    std::vector<std::string> prerequisites;
    std::unordered_map<std::string, int> rewards;

    [[nodiscard]] nlohmann::json ToJson() const;
    static RegionalQuest FromJson(const nlohmann::json& j);
};

/**
 * @brief Weather condition for regions
 */
struct RegionWeather {
    std::string type;  // clear, rain, snow, storm, fog, sandstorm
    float intensity = 0.5f;  // 0-1
    float temperature = 20.0f;  // Celsius
    float windSpeed = 0.0f;  // m/s
    float windDirection = 0.0f;  // degrees
    float visibility = 1.0f;  // 0-1 multiplier
    int64_t startTime = 0;
    int64_t duration = 3600;  // seconds

    [[nodiscard]] nlohmann::json ToJson() const;
    static RegionWeather FromJson(const nlohmann::json& j);
};

/**
 * @brief World region representing a real-world GPS-mapped area
 */
struct WorldRegion {
    // Identity
    std::string id;
    std::string name;
    std::string description;
    std::string continent;
    std::string country;

    // GPS mapping
    GPSBounds bounds;
    Geo::GeoCoordinate centerPoint;
    std::vector<Geo::GeoCoordinate> polygonBoundary;  // Detailed boundary

    // Gameplay
    RegionBiome biome = RegionBiome::Unknown;
    int controllingFaction = -1;  // -1 = neutral
    std::string controllingPlayerId;
    float controlStrength = 0.0f;
    int dangerLevel = 1;  // 1-10
    int recommendedLevel = 1;
    bool discovered = false;
    bool accessible = true;
    bool pvpEnabled = true;

    // Content
    std::vector<PortalConnection> portals;
    std::vector<ResourceNode> resources;
    std::vector<RegionalQuest> quests;
    std::vector<std::string> npcSpawnIds;
    std::vector<std::string> bossSpawnIds;

    // Environment
    RegionWeather currentWeather;
    float timeZoneOffset = 0.0f;  // Hours from UTC
    bool usesRealTime = true;
    float gameTimeMultiplier = 1.0f;

    // Modifiers
    float resourceMultiplier = 1.0f;
    float experienceMultiplier = 1.0f;
    float combatDifficultyMultiplier = 1.0f;
    float movementSpeedMultiplier = 1.0f;

    // Metadata
    int64_t createdTimestamp = 0;
    int64_t lastUpdated = 0;
    int playerCount = 0;
    int totalVisits = 0;

    // Special flags
    bool isStartingRegion = false;
    bool isSafeZone = false;
    bool isContested = false;
    bool isEventRegion = false;
    bool isHidden = false;  // Must be discovered

    /**
     * @brief Check if a GPS coordinate is within this region
     */
    [[nodiscard]] bool ContainsCoordinate(const Geo::GeoCoordinate& coord) const;

    /**
     * @brief Get distance from coordinate to region center
     */
    [[nodiscard]] double GetDistanceFromCenter(const Geo::GeoCoordinate& coord) const;

    /**
     * @brief Find nearest portal to a coordinate
     */
    [[nodiscard]] const PortalConnection* FindNearestPortal(const Geo::GeoCoordinate& coord) const;

    /**
     * @brief Check if player meets requirements to enter
     */
    [[nodiscard]] bool CanPlayerEnter(int playerLevel, const std::vector<std::string>& completedQuests) const;

    /**
     * @brief Get effective danger level considering modifiers
     */
    [[nodiscard]] int GetEffectiveDangerLevel() const;

    [[nodiscard]] nlohmann::json ToJson() const;
    static WorldRegion FromJson(const nlohmann::json& j);
};

/**
 * @brief Region discovery record for a player
 */
struct RegionDiscovery {
    std::string regionId;
    std::string playerId;
    int64_t discoveredTimestamp = 0;
    int64_t lastVisitTimestamp = 0;
    int visitCount = 0;
    bool fullyExplored = false;
    float explorationPercent = 0.0f;
    std::vector<std::string> discoveredPortals;
    std::vector<std::string> discoveredResources;
    std::vector<std::string> completedQuests;

    [[nodiscard]] nlohmann::json ToJson() const;
    static RegionDiscovery FromJson(const nlohmann::json& j);
};

/**
 * @brief Configuration for region system
 */
struct RegionConfig {
    // Discovery
    float autoDiscoverRadius = 100.0f;  // meters
    bool requirePhysicalPresence = true;

    // Control
    float captureBaseTime = 3600.0f;  // 1 hour
    float captureSpeedMultiplier = 1.0f;
    float controlDecayPerHour = 5.0f;

    // Resources
    float resourceRespawnHours = 24.0f;
    float harvestCooldownSeconds = 60.0f;

    // Danger
    float dangerScalePerLevel = 0.15f;
    float eliteSpawnChance = 0.05f;
    float bossSpawnChance = 0.01f;

    // Weather
    float weatherChangeProbability = 0.1f;  // Per hour
    float extremeWeatherProbability = 0.05f;
};

/**
 * @brief Manager for world regions
 */
class RegionManager {
public:
    using RegionChangedCallback = std::function<void(const WorldRegion& region)>;
    using DiscoveryCallback = std::function<void(const RegionDiscovery& discovery)>;
    using WeatherCallback = std::function<void(const std::string& regionId, const RegionWeather& weather)>;

    [[nodiscard]] static RegionManager& Instance();

    RegionManager(const RegionManager&) = delete;
    RegionManager& operator=(const RegionManager&) = delete;

    /**
     * @brief Initialize region manager
     */
    [[nodiscard]] bool Initialize(const RegionConfig& config = {});
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    /**
     * @brief Update region system
     */
    void Update(float deltaTime);

    // ==================== Region Queries ====================

    /**
     * @brief Get region by ID
     */
    [[nodiscard]] const WorldRegion* GetRegion(const std::string& regionId) const;

    /**
     * @brief Get all regions
     */
    [[nodiscard]] std::vector<const WorldRegion*> GetAllRegions() const;

    /**
     * @brief Get regions by continent
     */
    [[nodiscard]] std::vector<const WorldRegion*> GetRegionsByContinent(const std::string& continent) const;

    /**
     * @brief Get regions by biome
     */
    [[nodiscard]] std::vector<const WorldRegion*> GetRegionsByBiome(RegionBiome biome) const;

    /**
     * @brief Find region containing GPS coordinate
     */
    [[nodiscard]] const WorldRegion* FindRegionAtCoordinate(const Geo::GeoCoordinate& coord) const;

    /**
     * @brief Find nearest region to coordinate
     */
    [[nodiscard]] const WorldRegion* FindNearestRegion(const Geo::GeoCoordinate& coord) const;

    /**
     * @brief Get regions within radius of coordinate
     */
    [[nodiscard]] std::vector<const WorldRegion*> FindRegionsInRadius(
        const Geo::GeoCoordinate& center, double radiusKm) const;

    /**
     * @brief Get discovered regions for player
     */
    [[nodiscard]] std::vector<const WorldRegion*> GetDiscoveredRegions(const std::string& playerId) const;

    /**
     * @brief Get accessible regions for player
     */
    [[nodiscard]] std::vector<const WorldRegion*> GetAccessibleRegions(
        const std::string& playerId, int playerLevel) const;

    // ==================== Region Modification ====================

    /**
     * @brief Register a new region
     */
    bool RegisterRegion(const WorldRegion& region);

    /**
     * @brief Update existing region
     */
    bool UpdateRegion(const WorldRegion& region);

    /**
     * @brief Remove a region
     */
    bool RemoveRegion(const std::string& regionId);

    /**
     * @brief Load regions from config directory
     */
    bool LoadRegionsFromConfig(const std::string& configPath);

    /**
     * @brief Set region control
     */
    void SetRegionControl(const std::string& regionId, int factionId,
                          const std::string& playerId, float strength);

    /**
     * @brief Update region weather
     */
    void SetRegionWeather(const std::string& regionId, const RegionWeather& weather);

    // ==================== Discovery ====================

    /**
     * @brief Mark region as discovered by player
     */
    void DiscoverRegion(const std::string& regionId, const std::string& playerId);

    /**
     * @brief Get discovery record
     */
    [[nodiscard]] const RegionDiscovery* GetDiscovery(
        const std::string& regionId, const std::string& playerId) const;

    /**
     * @brief Update discovery progress
     */
    void UpdateDiscovery(const RegionDiscovery& discovery);

    /**
     * @brief Check if region is discovered
     */
    [[nodiscard]] bool IsRegionDiscovered(const std::string& regionId, const std::string& playerId) const;

    // ==================== Resources ====================

    /**
     * @brief Harvest resource from node
     */
    bool HarvestResource(const std::string& regionId, const std::string& resourceId,
                         const std::string& playerId, float amount);

    /**
     * @brief Get available resources in region
     */
    [[nodiscard]] std::vector<ResourceNode> GetAvailableResources(const std::string& regionId) const;

    // ==================== Synchronization ====================

    void SyncToServer();
    void LoadFromServer();
    void ListenForChanges();
    void StopListening();

    // ==================== Callbacks ====================

    void OnRegionChanged(RegionChangedCallback callback);
    void OnRegionDiscovered(DiscoveryCallback callback);
    void OnWeatherChanged(WeatherCallback callback);

    // ==================== Configuration ====================

    void SetLocalPlayerId(const std::string& playerId) { m_localPlayerId = playerId; }
    [[nodiscard]] const RegionConfig& GetConfig() const { return m_config; }
    void SetConfig(const RegionConfig& config) { m_config = config; }

private:
    RegionManager() = default;
    ~RegionManager() = default;

    void UpdateWeather(float deltaTime);
    void UpdateResourceRegeneration(float deltaTime);
    void UpdateControlDecay(float deltaTime);
    void GenerateRandomWeather(WorldRegion& region);

    bool m_initialized = false;
    RegionConfig m_config;
    std::string m_localPlayerId;

    std::unordered_map<std::string, WorldRegion> m_regions;
    mutable std::mutex m_regionsMutex;

    std::unordered_map<std::string, std::unordered_map<std::string, RegionDiscovery>> m_discoveries;
    mutable std::mutex m_discoveriesMutex;

    std::vector<RegionChangedCallback> m_regionCallbacks;
    std::vector<DiscoveryCallback> m_discoveryCallbacks;
    std::vector<WeatherCallback> m_weatherCallbacks;
    std::mutex m_callbackMutex;

    float m_weatherUpdateTimer = 0.0f;
    float m_resourceUpdateTimer = 0.0f;
    float m_controlUpdateTimer = 0.0f;

    static constexpr float WEATHER_UPDATE_INTERVAL = 300.0f;  // 5 minutes
    static constexpr float RESOURCE_UPDATE_INTERVAL = 60.0f;  // 1 minute
    static constexpr float CONTROL_UPDATE_INTERVAL = 60.0f;   // 1 minute
};

} // namespace Vehement
