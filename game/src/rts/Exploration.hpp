#pragma once

#include "SessionFogOfWar.hpp"
#include "VisionSource.hpp"

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <unordered_map>
#include <random>

namespace Vehement2 {
namespace RTS {

// Forward declarations
class SessionFogOfWar;

/**
 * @brief Types of discoveries that can be made while exploring
 */
enum class DiscoveryType : uint8_t {
    None = 0,
    ResourceNode,       // Wood, stone, gold, etc.
    Survivor,           // NPC that can be recruited
    LootCache,          // Hidden treasure/equipment
    EnemyBase,          // Enemy encampment or base
    PointOfInterest,    // Landmark or special location
    AncientRuin,        // Exploration objective
    HiddenPath,         // Secret passage
    DangerZone,         // Area with increased enemy activity
    SafeZone,           // Defensible position
    WaterSource,        // Essential resource
    Artifact            // Rare collectible
};

/**
 * @brief Rarity levels for discoveries
 */
enum class DiscoveryRarity : uint8_t {
    Common = 0,         // Found frequently
    Uncommon = 1,       // Occasional
    Rare = 2,           // Hard to find
    Epic = 3,           // Very rare
    Legendary = 4       // Extremely rare
};

/**
 * @brief Discovery event data
 */
struct Discovery {
    uint32_t id = 0;                            // Unique discovery ID
    DiscoveryType type = DiscoveryType::None;
    DiscoveryRarity rarity = DiscoveryRarity::Common;
    glm::ivec2 tile{0};                         // Tile location
    glm::vec2 worldPosition{0.0f};              // World position
    std::string name;                           // Discovery name
    std::string description;                    // Discovery description

    // Resource node specific
    int resourceAmount = 0;                     // Amount of resources available
    std::string resourceType;                   // Type of resource

    // Survivor specific
    int survivorCount = 1;                      // Number of survivors
    bool hostile = false;                       // Are they initially hostile

    // Loot specific
    int lootTier = 1;                           // Quality of loot (1-5)
    std::vector<std::string> lootItems;         // Items in the cache

    // State
    bool discovered = false;                    // Has been found
    bool claimed = false;                       // Has been interacted with
    float respawnTime = -1.0f;                  // Time until respawn (-1 = never)
    float currentRespawnTimer = 0.0f;

    /**
     * @brief Get XP reward for this discovery
     */
    float GetXPReward() const {
        float baseXP = 10.0f;
        float rarityMultiplier = 1.0f + static_cast<float>(rarity) * 0.5f;

        switch (type) {
            case DiscoveryType::ResourceNode: baseXP = 5.0f; break;
            case DiscoveryType::Survivor: baseXP = 25.0f; break;
            case DiscoveryType::LootCache: baseXP = 15.0f; break;
            case DiscoveryType::EnemyBase: baseXP = 50.0f; break;
            case DiscoveryType::PointOfInterest: baseXP = 20.0f; break;
            case DiscoveryType::AncientRuin: baseXP = 75.0f; break;
            case DiscoveryType::Artifact: baseXP = 100.0f; break;
            default: break;
        }

        return baseXP * rarityMultiplier;
    }
};

/**
 * @brief Scout mission data
 */
struct ScoutMission {
    uint32_t missionId = 0;
    uint32_t scoutUnitId = 0;                   // Worker assigned to scout
    glm::vec2 destination{0.0f};                // Target location
    glm::vec2 startPosition{0.0f};              // Starting position
    std::vector<glm::vec2> waypoints;           // Path to follow

    enum class Status : uint8_t {
        Pending,        // Not yet started
        InProgress,     // Scout is moving
        Completed,      // Reached destination
        Aborted,        // Mission cancelled
        Failed          // Scout was killed/captured
    };
    Status status = Status::Pending;

    float progress = 0.0f;                      // 0-1 completion
    float timeStarted = 0.0f;                   // Game time when started
    float estimatedDuration = 0.0f;             // Expected completion time

    // Results
    std::vector<Discovery> discoveries;         // What was found
    float explorationXPEarned = 0.0f;
};

/**
 * @brief Callbacks for exploration events
 */
using DiscoveryCallback = std::function<void(const Discovery&)>;
using ExplorationMilestoneCallback = std::function<void(float percent)>;
using ScoutCompleteCallback = std::function<void(const ScoutMission&)>;

/**
 * @brief Configuration for exploration mechanics
 */
struct ExplorationConfig {
    // XP settings
    float xpPerTileExplored = 1.0f;
    float xpBonusForFirstDiscovery = 10.0f;
    float xpMultiplierForRarity = 1.5f;

    // Discovery spawn rates (per 100 tiles)
    float resourceNodeDensity = 5.0f;
    float survivorDensity = 1.0f;
    float lootCacheDensity = 2.0f;
    float enemyBaseDensity = 0.5f;
    float poiDensity = 3.0f;

    // Milestone percentages
    std::vector<float> milestones = { 10.0f, 25.0f, 50.0f, 75.0f, 90.0f, 100.0f };

    // Scout settings
    float scoutSpeed = 1.5f;                    // Tiles per second
    float scoutVisionBonus = 2.0f;              // Extra vision tiles
};

/**
 * @brief Worker class for scouting (minimal interface)
 */
class Worker {
public:
    uint32_t GetId() const { return m_id; }
    glm::vec2 GetPosition() const { return m_position; }
    void SetPosition(const glm::vec2& pos) { m_position = pos; }
    bool IsAvailable() const { return m_available; }
    void SetAvailable(bool available) { m_available = available; }
    float GetMoveSpeed() const { return m_moveSpeed; }

private:
    uint32_t m_id = 0;
    glm::vec2 m_position{0.0f};
    bool m_available = true;
    float m_moveSpeed = 5.0f;

    friend class Exploration;
};

/**
 * @brief Exploration mechanics system
 *
 * Handles:
 * - Tracking exploration progress across the map
 * - Discovery events when tiles are revealed
 * - Scout missions for autonomous exploration
 * - XP rewards for exploration
 * - Discovery spawning and management
 *
 * Works in conjunction with SessionFogOfWar to trigger events
 * when new areas are revealed.
 */
class Exploration {
public:
    Exploration();
    ~Exploration();

    // Non-copyable
    Exploration(const Exploration&) = delete;
    Exploration& operator=(const Exploration&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize exploration system
     * @param fogOfWar Reference to fog of war system
     * @param mapWidth Map width in tiles
     * @param mapHeight Map height in tiles
     * @param tileSize Size of each tile
     * @return true if initialization succeeded
     */
    bool Initialize(SessionFogOfWar* fogOfWar, int mapWidth, int mapHeight, float tileSize);

    /**
     * @brief Shutdown exploration system
     */
    void Shutdown();

    /**
     * @brief Update exploration system
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    // =========================================================================
    // Exploration Progress
    // =========================================================================

    /**
     * @brief Get current exploration percentage (0-100)
     */
    float GetExplorationPercent() const;

    /**
     * @brief Get number of tiles explored
     */
    int GetTilesExplored() const;

    /**
     * @brief Get total exploration XP earned
     */
    float GetTotalExplorationXP() const { return m_totalXP; }

    /**
     * @brief Get current exploration level
     */
    int GetExplorationLevel() const;

    /**
     * @brief Check if a milestone has been reached
     */
    bool HasReachedMilestone(float percent) const;

    // =========================================================================
    // Discovery System
    // =========================================================================

    /**
     * @brief Called when a tile is revealed (called by SessionFogOfWar)
     * @param tile Tile coordinates
     */
    void OnTileRevealed(const glm::ivec2& tile);

    /**
     * @brief Manually place a discovery at a location
     * @param discovery The discovery to place
     */
    void PlaceDiscovery(const Discovery& discovery);

    /**
     * @brief Generate random discoveries for the map
     * @param seed Random seed for generation
     */
    void GenerateDiscoveries(uint32_t seed = 0);

    /**
     * @brief Get all discoveries
     */
    const std::vector<Discovery>& GetAllDiscoveries() const { return m_discoveries; }

    /**
     * @brief Get discoveries of a specific type
     */
    std::vector<const Discovery*> GetDiscoveriesOfType(DiscoveryType type) const;

    /**
     * @brief Get undiscovered items in range
     */
    std::vector<const Discovery*> GetUndiscoveredInRange(const glm::vec2& center, float radius) const;

    /**
     * @brief Get discovered but unclaimed items
     */
    std::vector<const Discovery*> GetUnclaimedDiscoveries() const;

    /**
     * @brief Claim a discovery (interact with it)
     * @param discoveryId Discovery ID to claim
     * @return true if successfully claimed
     */
    bool ClaimDiscovery(uint32_t discoveryId);

    // =========================================================================
    // Scout Missions
    // =========================================================================

    /**
     * @brief Send a scout to explore an area
     * @param scout Worker to send as scout
     * @param destination Target location
     * @return Mission ID, or 0 if failed
     */
    uint32_t SendScout(Worker* scout, const glm::vec2& destination);

    /**
     * @brief Cancel a scout mission
     * @param missionId Mission to cancel
     */
    void CancelScoutMission(uint32_t missionId);

    /**
     * @brief Get all active scout missions
     */
    const std::vector<ScoutMission>& GetActiveScoutMissions() const { return m_activeMissions; }

    /**
     * @brief Get completed scout missions
     */
    const std::vector<ScoutMission>& GetCompletedMissions() const { return m_completedMissions; }

    // =========================================================================
    // XP and Rewards
    // =========================================================================

    /**
     * @brief Grant exploration XP
     * @param amount Amount of XP to grant
     */
    void GrantExplorationXP(float amount);

    /**
     * @brief Get XP required for next level
     */
    float GetXPForNextLevel() const;

    /**
     * @brief Get current XP progress toward next level (0-1)
     */
    float GetLevelProgress() const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Set callback for when a discovery is found
     */
    void SetDiscoveryCallback(DiscoveryCallback callback) {
        m_onDiscovery = std::move(callback);
    }

    /**
     * @brief Set callback for exploration milestones
     */
    void SetMilestoneCallback(ExplorationMilestoneCallback callback) {
        m_onMilestone = std::move(callback);
    }

    /**
     * @brief Set callback for scout mission completion
     */
    void SetScoutCompleteCallback(ScoutCompleteCallback callback) {
        m_onScoutComplete = std::move(callback);
    }

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set configuration
     */
    void SetConfig(const ExplorationConfig& config) { m_config = config; }

    /**
     * @brief Get configuration
     */
    const ExplorationConfig& GetConfig() const { return m_config; }

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get number of discoveries made
     */
    int GetDiscoveryCount() const;

    /**
     * @brief Get discoveries by type count
     */
    int GetDiscoveryCount(DiscoveryType type) const;

    /**
     * @brief Get total resources found
     */
    int GetTotalResourcesFound() const;

    /**
     * @brief Get total survivors found
     */
    int GetTotalSurvivorsFound() const;

private:
    // Internal methods
    void UpdateScoutMissions(float deltaTime);
    void CheckForDiscoveries(const glm::ivec2& tile);
    void ProcessMilestones();
    Discovery GenerateRandomDiscovery(const glm::ivec2& tile);
    std::vector<glm::vec2> CalculateScoutPath(const glm::vec2& from, const glm::vec2& to);

    float CalculateXPForLevel(int level) const;

    // References
    SessionFogOfWar* m_fogOfWar = nullptr;

    // Map data
    int m_mapWidth = 0;
    int m_mapHeight = 0;
    float m_tileSize = 1.0f;

    // Configuration
    ExplorationConfig m_config;

    // State
    bool m_initialized = false;

    // Exploration tracking
    float m_totalXP = 0.0f;
    int m_explorationLevel = 1;
    float m_lastExplorationPercent = 0.0f;
    std::vector<float> m_reachedMilestones;

    // Discovery data
    std::vector<Discovery> m_discoveries;
    std::unordered_map<uint64_t, uint32_t> m_tileToDiscovery;  // tile hash -> discovery index
    uint32_t m_nextDiscoveryId = 1;

    // Scout missions
    std::vector<ScoutMission> m_activeMissions;
    std::vector<ScoutMission> m_completedMissions;
    uint32_t m_nextMissionId = 1;

    // Statistics
    int m_totalResourcesFound = 0;
    int m_totalSurvivorsFound = 0;

    // Random generation
    std::mt19937 m_rng;

    // Callbacks
    DiscoveryCallback m_onDiscovery;
    ExplorationMilestoneCallback m_onMilestone;
    ScoutCompleteCallback m_onScoutComplete;
};

} // namespace RTS
} // namespace Vehement2
