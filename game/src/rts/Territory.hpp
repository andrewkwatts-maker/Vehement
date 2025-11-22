#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <mutex>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

namespace Vehement {

// Forward declarations
class FirebaseManager;
struct Building;
enum class BuildingType : uint8_t;

/**
 * @brief Territory control status
 */
enum class TerritoryStatus {
    Unclaimed,          // No owner
    Owned,              // Fully controlled by one player
    Contested,          // Multiple players claiming
    Protected           // Safe zone, cannot be claimed
};

/**
 * @brief Single tile's territory data
 */
struct TerritoryTile {
    glm::ivec2 position{0, 0};
    std::string ownerId;            // Player who controls this tile
    float controlStrength = 0.0f;   // 0-100
    TerritoryStatus status = TerritoryStatus::Unclaimed;

    // Contest info (if multiple claimants)
    std::vector<std::string> contestingPlayers;
    std::vector<float> contestStrengths;

    // Timestamps
    int64_t claimedTimestamp = 0;
    int64_t lastUpdated = 0;

    [[nodiscard]] bool IsOwned() const { return status == TerritoryStatus::Owned; }
    [[nodiscard]] bool IsContested() const { return status == TerritoryStatus::Contested; }
    [[nodiscard]] bool IsUnclaimed() const { return status == TerritoryStatus::Unclaimed; }

    [[nodiscard]] nlohmann::json ToJson() const;
    static TerritoryTile FromJson(const nlohmann::json& j);
};

/**
 * @brief Player's complete territory
 */
struct Territory {
    std::string ownerId;
    std::vector<glm::ivec2> tiles;  // All tiles in territory
    float totalControlStrength = 0.0f;
    int totalTiles = 0;

    // Core territory (fully controlled)
    std::vector<glm::ivec2> coreTiles;

    // Border tiles (edge of territory)
    std::vector<glm::ivec2> borderTiles;

    // Contested tiles
    std::vector<glm::ivec2> contestedTiles;

    // Statistics
    int buildingsInTerritory = 0;
    int resourceNodesInTerritory = 0;

    [[nodiscard]] nlohmann::json ToJson() const;
    static Territory FromJson(const nlohmann::json& j);

    /**
     * @brief Check if a position is in this territory
     */
    [[nodiscard]] bool Contains(const glm::ivec2& pos) const;

    /**
     * @brief Get control strength at a position
     */
    [[nodiscard]] float GetStrengthAt(const glm::ivec2& pos) const;
};

/**
 * @brief Contest event when territories overlap
 */
struct TerritoryContest {
    glm::ivec2 position;
    std::string defenderId;
    std::string attackerId;
    float defenderStrength;
    float attackerStrength;
    int64_t startTimestamp;
    int64_t resolveTimestamp;
    bool resolved = false;
    std::string winnerId;

    [[nodiscard]] nlohmann::json ToJson() const;
    static TerritoryContest FromJson(const nlohmann::json& j);
};

/**
 * @brief Configuration for territory system
 */
struct TerritoryConfig {
    // Expansion settings
    float baseExpansionRadius = 5.0f;       // Base radius from buildings
    float beaconExpansionBonus = 10.0f;     // Additional radius from beacon
    float commandCenterRadius = 8.0f;       // Command center territory radius

    // Control strength settings
    float baseControlPerBuilding = 20.0f;   // Base strength per building
    float controlDecayPerTile = 2.0f;       // Strength loss per tile from source
    float minControlStrength = 10.0f;       // Min strength to claim
    float contestThreshold = 0.7f;          // Ratio to contest (0.7 = 70% of defender)

    // Time settings
    float contestDurationHours = 4.0f;      // Time to resolve contest
    float controlGrowthPerHour = 5.0f;      // Strength growth in owned territory
    float controlDecayPerHour = 2.0f;       // Strength decay without buildings

    // Bonuses
    float ownTerritoryDefenseBonus = 1.5f;  // Combat bonus in own territory
    float ownTerritoryProductionBonus = 1.2f; // Production bonus
    float contestedPenalty = 0.5f;          // Penalty in contested zones
};

/**
 * @brief Territory management system
 *
 * Features:
 * - Territory expands from buildings
 * - Control strength based on building proximity
 * - Overlapping territories create contested zones
 * - Players can see into their territory
 * - Resources in territory belong to player
 * - Defense bonuses in own territory
 * - Time-based contest resolution
 */
class TerritoryManager {
public:
    /**
     * @brief Callback types
     */
    using TerritoryChangedCallback = std::function<void(const Territory& territory)>;
    using ContestCallback = std::function<void(const TerritoryContest& contest)>;

    /**
     * @brief Get singleton instance
     */
    [[nodiscard]] static TerritoryManager& Instance();

    // Non-copyable
    TerritoryManager(const TerritoryManager&) = delete;
    TerritoryManager& operator=(const TerritoryManager&) = delete;

    /**
     * @brief Initialize territory system
     * @param config Territory configuration
     * @return true if successful
     */
    [[nodiscard]] bool Initialize(const TerritoryConfig& config = {});

    /**
     * @brief Shutdown territory system
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    /**
     * @brief Update territory system (call from game loop)
     * @param deltaTime Time since last frame
     */
    void Update(float deltaTime);

    // ==================== Territory Queries ====================

    /**
     * @brief Get territory at a position
     * @param pos Tile position
     * @return Territory tile data
     */
    [[nodiscard]] TerritoryTile GetTileAt(const glm::ivec2& pos) const;

    /**
     * @brief Get player's territory
     * @param playerId Player to get territory for
     * @return Player's territory data
     */
    [[nodiscard]] Territory GetPlayerTerritory(const std::string& playerId) const;

    /**
     * @brief Get local player's territory
     */
    [[nodiscard]] Territory GetLocalTerritory() const;

    /**
     * @brief Check if position is in player's territory
     * @param pos Position to check
     * @param playerId Player to check for
     */
    [[nodiscard]] bool IsInTerritory(const glm::ivec2& pos, const std::string& playerId) const;

    /**
     * @brief Check if position is in local player's territory
     */
    [[nodiscard]] bool IsInOwnTerritory(const glm::ivec2& pos) const;

    /**
     * @brief Check if position is contested
     */
    [[nodiscard]] bool IsContested(const glm::ivec2& pos) const;

    /**
     * @brief Get territory status at position
     */
    [[nodiscard]] TerritoryStatus GetStatusAt(const glm::ivec2& pos) const;

    /**
     * @brief Get all players with territory in a region
     */
    [[nodiscard]] std::vector<std::string> GetPlayersInRegion(
        const glm::ivec2& min, const glm::ivec2& max) const;

    // ==================== Territory Modification ====================

    /**
     * @brief Recalculate territory from buildings
     * @param playerId Player to recalculate for
     * @param buildings Player's buildings
     */
    void RecalculateTerritory(const std::string& playerId,
                              const std::vector<Building>& buildings);

    /**
     * @brief Claim a tile for a player
     * @param pos Position to claim
     * @param playerId Player claiming
     * @param strength Claim strength
     * @return true if claimed or contest started
     */
    bool ClaimTile(const glm::ivec2& pos, const std::string& playerId, float strength);

    /**
     * @brief Release a tile
     * @param pos Position to release
     * @param playerId Player releasing
     */
    void ReleaseTile(const glm::ivec2& pos, const std::string& playerId);

    /**
     * @brief Release all territory for a player
     */
    void ReleaseAllTerritory(const std::string& playerId);

    // ==================== Contest Management ====================

    /**
     * @brief Get active contests for a player
     */
    [[nodiscard]] std::vector<TerritoryContest> GetActiveContests(const std::string& playerId) const;

    /**
     * @brief Get contest at a position
     */
    [[nodiscard]] const TerritoryContest* GetContestAt(const glm::ivec2& pos) const;

    /**
     * @brief Resolve a contest
     * @param pos Contest position
     * @param winnerId Winner's player ID
     */
    void ResolveContest(const glm::ivec2& pos, const std::string& winnerId);

    // ==================== Bonuses ====================

    /**
     * @brief Get defense bonus at position for player
     * @param pos Position
     * @param playerId Player
     * @return Defense multiplier (1.0 = no bonus)
     */
    [[nodiscard]] float GetDefenseBonus(const glm::ivec2& pos, const std::string& playerId) const;

    /**
     * @brief Get production bonus at position for player
     * @param pos Position
     * @param playerId Player
     * @return Production multiplier (1.0 = no bonus)
     */
    [[nodiscard]] float GetProductionBonus(const glm::ivec2& pos, const std::string& playerId) const;

    /**
     * @brief Check if player has vision at position
     * @param pos Position to check
     * @param playerId Player to check for
     */
    [[nodiscard]] bool HasVision(const glm::ivec2& pos, const std::string& playerId) const;

    // ==================== Synchronization ====================

    /**
     * @brief Sync territory to server
     */
    void SyncToServer();

    /**
     * @brief Load territory from server
     * @param playerId Player to load territory for
     */
    void LoadFromServer(const std::string& playerId);

    /**
     * @brief Listen for territory changes
     */
    void ListenForChanges();

    /**
     * @brief Stop listening for changes
     */
    void StopListening();

    // ==================== Callbacks ====================

    /**
     * @brief Register callback for territory changes
     */
    void OnTerritoryChanged(TerritoryChangedCallback callback);

    /**
     * @brief Register callback for contest events
     */
    void OnContest(ContestCallback callback);

    // ==================== Configuration ====================

    /**
     * @brief Set local player ID
     */
    void SetLocalPlayerId(const std::string& playerId) { m_localPlayerId = playerId; }

    /**
     * @brief Get configuration
     */
    [[nodiscard]] const TerritoryConfig& GetConfig() const { return m_config; }

    /**
     * @brief Update configuration
     */
    void SetConfig(const TerritoryConfig& config) { m_config = config; }

private:
    TerritoryManager() = default;
    ~TerritoryManager() = default;

    // Helper for coordinate hashing
    struct IVec2Hash {
        std::size_t operator()(const glm::ivec2& v) const {
            return std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 16);
        }
    };

    // Internal methods
    void UpdateControlStrength(float deltaTime);
    void UpdateContests(float deltaTime);
    void StartContest(const glm::ivec2& pos, const std::string& attackerId,
                     const std::string& defenderId, float attackStrength);
    float CalculateStrengthFromBuildings(const glm::ivec2& pos,
                                         const std::vector<Building>& buildings) const;
    float GetBuildingTerritoryRadius(BuildingType type) const;
    float GetBuildingTerritoryStrength(BuildingType type) const;

    // Firebase paths
    [[nodiscard]] std::string GetTerritoryPath(const std::string& playerId) const;
    [[nodiscard]] std::string GetContestsPath() const;

    bool m_initialized = false;
    TerritoryConfig m_config;
    std::string m_localPlayerId;

    // Territory data
    std::unordered_map<glm::ivec2, TerritoryTile, IVec2Hash> m_tiles;
    mutable std::mutex m_tilesMutex;

    // Player territories
    std::unordered_map<std::string, Territory> m_playerTerritories;
    mutable std::mutex m_territoriesMutex;

    // Active contests
    std::unordered_map<glm::ivec2, TerritoryContest, IVec2Hash> m_contests;
    std::mutex m_contestsMutex;

    // Listeners
    std::string m_territoryListenerId;
    std::string m_contestsListenerId;

    // Callbacks
    std::vector<TerritoryChangedCallback> m_territoryCallbacks;
    std::vector<ContestCallback> m_contestCallbacks;
    std::mutex m_callbackMutex;

    // Update timers
    float m_strengthUpdateTimer = 0.0f;
    float m_contestUpdateTimer = 0.0f;
    static constexpr float STRENGTH_UPDATE_INTERVAL = 1.0f;
    static constexpr float CONTEST_UPDATE_INTERVAL = 5.0f;
};

/**
 * @brief Helper for territory visualization
 */
class TerritoryVisualizer {
public:
    /**
     * @brief Get color for territory status
     */
    [[nodiscard]] static glm::vec4 GetStatusColor(TerritoryStatus status);

    /**
     * @brief Get color for player territory
     * @param playerId Player ID (used to generate consistent color)
     * @param isOwn Is this the local player's territory
     */
    [[nodiscard]] static glm::vec4 GetPlayerColor(const std::string& playerId, bool isOwn);

    /**
     * @brief Get border color based on control strength
     * @param strength Control strength (0-100)
     */
    [[nodiscard]] static glm::vec4 GetStrengthColor(float strength);
};

} // namespace Vehement
