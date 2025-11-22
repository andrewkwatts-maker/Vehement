#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include "WorldRegion.hpp"

namespace Vehement {

/**
 * @brief View mode for region display
 */
enum class RegionViewMode : uint8_t {
    Local,      // Player's immediate area
    Regional,   // Full region view
    Global,     // World map view
    Tactical    // Combat/tactical overlay
};

/**
 * @brief NPC spawn point in region
 */
struct NPCSpawnPoint {
    std::string id;
    std::string npcTypeId;
    Geo::GeoCoordinate location;
    glm::vec3 localPosition{0.0f};
    float spawnRadius = 10.0f;
    int maxSpawned = 5;
    int currentSpawned = 0;
    float respawnTimeSeconds = 300.0f;
    int minLevel = 1;
    int maxLevel = 100;
    bool active = true;
    std::vector<std::string> conditions;  // Spawn conditions

    [[nodiscard]] nlohmann::json ToJson() const;
    static NPCSpawnPoint FromJson(const nlohmann::json& j);
};

/**
 * @brief Time of day for region
 */
struct RegionTimeOfDay {
    float hour = 12.0f;      // 0-24
    float minute = 0.0f;     // 0-60
    float dayProgress = 0.5f; // 0-1
    bool isDaytime = true;
    float sunAngle = 45.0f;
    float ambientLight = 1.0f;
    glm::vec3 sunColor{1.0f, 0.95f, 0.9f};
    glm::vec3 ambientColor{0.4f, 0.45f, 0.5f};

    [[nodiscard]] nlohmann::json ToJson() const;
    static RegionTimeOfDay FromJson(const nlohmann::json& j);
};

/**
 * @brief Regional rule modifiers
 */
struct RegionRules {
    // Combat rules
    bool pvpAllowed = true;
    float damageMultiplier = 1.0f;
    float healingMultiplier = 1.0f;
    bool friendlyFireEnabled = false;
    bool deathPenalty = true;
    float deathPenaltyMultiplier = 1.0f;

    // Economy rules
    float resourceGatherMultiplier = 1.0f;
    float tradingTaxPercent = 0.0f;
    bool buildingAllowed = true;
    float buildingCostMultiplier = 1.0f;
    float buildingTimeMultiplier = 1.0f;

    // Movement rules
    float movementSpeedMultiplier = 1.0f;
    bool mountsAllowed = true;
    bool flyingAllowed = false;
    bool teleportAllowed = true;

    // Experience rules
    float experienceMultiplier = 1.0f;
    int levelCap = -1;  // -1 = no cap

    // Special rules
    bool isInstanced = false;
    int maxPlayersPerInstance = 100;
    bool allowGrouping = true;
    int maxGroupSize = 10;

    [[nodiscard]] nlohmann::json ToJson() const;
    static RegionRules FromJson(const nlohmann::json& j);
};

/**
 * @brief Regional achievement/milestone
 */
struct RegionalMilestone {
    std::string id;
    std::string name;
    std::string description;
    std::string requirement;  // condition string
    int targetValue = 0;
    int currentValue = 0;
    bool completed = false;
    std::unordered_map<std::string, int> rewards;

    [[nodiscard]] nlohmann::json ToJson() const;
    static RegionalMilestone FromJson(const nlohmann::json& j);
};

/**
 * @brief Configuration for region controller
 */
struct RegionControllerConfig {
    float npcUpdateInterval = 1.0f;
    float weatherUpdateInterval = 60.0f;
    float timeOfDaySpeed = 1.0f;  // Real seconds per game hour
    bool useRealWorldTime = true;
    float dayNightCycleDuration = 1800.0f;  // 30 min cycle
    int maxNPCsPerUpdate = 50;
    float spawnCheckRadius = 500.0f;
};

/**
 * @brief Controller for region-specific gameplay
 */
class RegionController {
public:
    using ViewModeChangedCallback = std::function<void(RegionViewMode mode)>;
    using TimeChangedCallback = std::function<void(const RegionTimeOfDay& time)>;
    using NPCSpawnedCallback = std::function<void(const std::string& npcId, const NPCSpawnPoint& spawn)>;
    using MilestoneCallback = std::function<void(const RegionalMilestone& milestone)>;

    [[nodiscard]] static RegionController& Instance();

    RegionController(const RegionController&) = delete;
    RegionController& operator=(const RegionController&) = delete;

    [[nodiscard]] bool Initialize(const RegionControllerConfig& config = {});
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    void Update(float deltaTime);

    // ==================== Region Management ====================

    /**
     * @brief Set active region for local player
     */
    void SetActiveRegion(const std::string& regionId);

    /**
     * @brief Get active region ID
     */
    [[nodiscard]] std::string GetActiveRegionId() const { return m_activeRegionId; }

    /**
     * @brief Get active region data
     */
    [[nodiscard]] const WorldRegion* GetActiveRegion() const;

    /**
     * @brief Enter a region
     */
    bool EnterRegion(const std::string& regionId, const std::string& playerId);

    /**
     * @brief Exit current region
     */
    void ExitRegion(const std::string& playerId);

    // ==================== View Mode ====================

    /**
     * @brief Set view mode
     */
    void SetViewMode(RegionViewMode mode);

    /**
     * @brief Get current view mode
     */
    [[nodiscard]] RegionViewMode GetViewMode() const { return m_viewMode; }

    /**
     * @brief Toggle between local and regional view
     */
    void ToggleViewMode();

    // ==================== Rules ====================

    /**
     * @brief Get region rules
     */
    [[nodiscard]] RegionRules GetRegionRules(const std::string& regionId) const;

    /**
     * @brief Set region rules
     */
    void SetRegionRules(const std::string& regionId, const RegionRules& rules);

    /**
     * @brief Check if action is allowed
     */
    [[nodiscard]] bool IsActionAllowed(const std::string& action) const;

    /**
     * @brief Get damage multiplier
     */
    [[nodiscard]] float GetDamageMultiplier() const;

    /**
     * @brief Get experience multiplier
     */
    [[nodiscard]] float GetExperienceMultiplier() const;

    // ==================== Time of Day ====================

    /**
     * @brief Get current time of day
     */
    [[nodiscard]] RegionTimeOfDay GetTimeOfDay() const;

    /**
     * @brief Set time of day (for testing/admin)
     */
    void SetTimeOfDay(float hour, float minute);

    /**
     * @brief Check if currently daytime
     */
    [[nodiscard]] bool IsDaytime() const;

    /**
     * @brief Get sun position
     */
    [[nodiscard]] glm::vec3 GetSunDirection() const;

    // ==================== Weather ====================

    /**
     * @brief Get current weather
     */
    [[nodiscard]] RegionWeather GetCurrentWeather() const;

    /**
     * @brief Set weather (for testing/admin)
     */
    void SetWeather(const RegionWeather& weather);

    /**
     * @brief Get weather effect on gameplay
     */
    [[nodiscard]] float GetWeatherVisibilityMultiplier() const;
    [[nodiscard]] float GetWeatherMovementMultiplier() const;

    // ==================== NPC Spawning ====================

    /**
     * @brief Register spawn point
     */
    void RegisterSpawnPoint(const NPCSpawnPoint& spawn);

    /**
     * @brief Get spawn points in region
     */
    [[nodiscard]] std::vector<NPCSpawnPoint> GetSpawnPoints(const std::string& regionId) const;

    /**
     * @brief Force spawn at point
     */
    void ForceSpawn(const std::string& spawnPointId);

    /**
     * @brief Clear all spawned NPCs in region
     */
    void ClearSpawnedNPCs(const std::string& regionId);

    /**
     * @brief Notify NPC death (for respawn tracking)
     */
    void OnNPCDeath(const std::string& spawnPointId);

    // ==================== Resources ====================

    /**
     * @brief Get available resources in view
     */
    [[nodiscard]] std::vector<ResourceNode> GetVisibleResources() const;

    /**
     * @brief Get resource availability multiplier
     */
    [[nodiscard]] float GetResourceAvailability(const std::string& resourceType) const;

    // ==================== Milestones ====================

    /**
     * @brief Get regional milestones
     */
    [[nodiscard]] std::vector<RegionalMilestone> GetMilestones(const std::string& regionId) const;

    /**
     * @brief Update milestone progress
     */
    void UpdateMilestoneProgress(const std::string& milestoneId, int progress);

    /**
     * @brief Check milestone completion
     */
    [[nodiscard]] bool IsMilestoneComplete(const std::string& milestoneId) const;

    // ==================== Callbacks ====================

    void OnViewModeChanged(ViewModeChangedCallback callback);
    void OnTimeChanged(TimeChangedCallback callback);
    void OnNPCSpawned(NPCSpawnedCallback callback);
    void OnMilestoneCompleted(MilestoneCallback callback);

    // ==================== Configuration ====================

    void SetLocalPlayerId(const std::string& playerId) { m_localPlayerId = playerId; }
    [[nodiscard]] const RegionControllerConfig& GetConfig() const { return m_config; }

private:
    RegionController() = default;
    ~RegionController() = default;

    void UpdateTimeOfDay(float deltaTime);
    void UpdateWeather(float deltaTime);
    void UpdateNPCSpawning(float deltaTime);
    void CheckMilestones();
    RegionTimeOfDay CalculateRealWorldTime(float timezoneOffset) const;

    bool m_initialized = false;
    RegionControllerConfig m_config;
    std::string m_localPlayerId;
    std::string m_activeRegionId;
    RegionViewMode m_viewMode = RegionViewMode::Local;

    // Time of day
    RegionTimeOfDay m_currentTime;
    float m_timeAccumulator = 0.0f;

    // Rules per region
    std::unordered_map<std::string, RegionRules> m_regionRules;
    mutable std::mutex m_rulesMutex;

    // Spawn points
    std::unordered_map<std::string, std::vector<NPCSpawnPoint>> m_spawnPoints;
    std::unordered_map<std::string, float> m_respawnTimers;
    mutable std::mutex m_spawnMutex;

    // Milestones
    std::unordered_map<std::string, std::vector<RegionalMilestone>> m_milestones;
    mutable std::mutex m_milestoneMutex;

    // Timers
    float m_npcUpdateTimer = 0.0f;
    float m_weatherUpdateTimer = 0.0f;

    // Callbacks
    std::vector<ViewModeChangedCallback> m_viewModeCallbacks;
    std::vector<TimeChangedCallback> m_timeCallbacks;
    std::vector<NPCSpawnedCallback> m_spawnCallbacks;
    std::vector<MilestoneCallback> m_milestoneCallbacks;
    std::mutex m_callbackMutex;
};

} // namespace Vehement
