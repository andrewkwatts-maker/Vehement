#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include <chrono>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

namespace Vehement {

// Forward declarations
class FirebaseManager;
class OfflineSimulation;
class WorldSync;
class Territory;
class WorldClock;

/**
 * @brief Resource types in the RTS system
 */
enum class ResourceType : uint8_t {
    Food = 0,       // Consumed by workers/population
    Wood,           // Construction material
    Stone,          // Advanced construction
    Metal,          // Weapons and upgrades
    Fuel,           // Energy for defenses
    Medicine,       // Healing and population growth
    Ammunition,     // Defense and attacks
    Count           // Number of resource types
};

/**
 * @brief Convert resource type to string
 */
[[nodiscard]] const char* ResourceTypeToString(ResourceType type);
[[nodiscard]] ResourceType StringToResourceType(const std::string& str);

/**
 * @brief Resource stock tracking
 */
struct ResourceStock {
    std::array<int, static_cast<size_t>(ResourceType::Count)> amounts{};
    std::array<int, static_cast<size_t>(ResourceType::Count)> capacity{};
    std::array<float, static_cast<size_t>(ResourceType::Count)> productionRate{}; // Per hour
    std::array<float, static_cast<size_t>(ResourceType::Count)> consumptionRate{}; // Per hour

    ResourceStock();

    [[nodiscard]] int Get(ResourceType type) const;
    void Set(ResourceType type, int amount);
    void Add(ResourceType type, int amount);
    bool Consume(ResourceType type, int amount);
    [[nodiscard]] bool CanAfford(ResourceType type, int amount) const;
    [[nodiscard]] int GetCapacity(ResourceType type) const;
    void SetCapacity(ResourceType type, int cap);
    [[nodiscard]] float GetProductionRate(ResourceType type) const;
    void SetProductionRate(ResourceType type, float rate);
    [[nodiscard]] float GetConsumptionRate(ResourceType type) const;
    void SetConsumptionRate(ResourceType type, float rate);

    [[nodiscard]] nlohmann::json ToJson() const;
    static ResourceStock FromJson(const nlohmann::json& j);
};

/**
 * @brief Building types in the base
 */
enum class BuildingType : uint8_t {
    // Production
    Farm = 0,           // Produces food
    Sawmill,            // Produces wood
    Quarry,             // Produces stone
    Mine,               // Produces metal
    Refinery,           // Produces fuel
    Hospital,           // Produces medicine
    Armory,             // Produces ammunition

    // Storage
    Warehouse,          // Increases resource capacity
    Silo,               // Food storage

    // Defense
    Wall,               // Basic defense
    Tower,              // Ranged defense
    Gate,               // Entry point
    Bunker,             // Strong defense

    // Population
    House,              // Worker housing
    Barracks,           // Military units

    // Special
    CommandCenter,      // Main building
    Workshop,           // Upgrades
    Laboratory,         // Research
    TradingPost,        // Trade with others
    Beacon,             // Territory expansion

    Count
};

/**
 * @brief Get building name string
 */
[[nodiscard]] const char* BuildingTypeToString(BuildingType type);
[[nodiscard]] BuildingType StringToBuildingType(const std::string& str);

/**
 * @brief Building instance in the world
 */
struct Building {
    int id = -1;
    BuildingType type = BuildingType::Farm;
    glm::ivec2 position{0, 0};
    glm::ivec2 size{1, 1};          // Size in tiles
    int health = 100;
    int maxHealth = 100;
    int level = 1;
    float constructionProgress = 1.0f;  // 0-1, 1 = complete
    bool isActive = true;
    int64_t createdTimestamp = 0;
    int assignedWorkers = 0;

    // Production info
    ResourceType producesResource = ResourceType::Food;
    float productionPerHour = 0.0f;

    [[nodiscard]] bool IsConstructed() const { return constructionProgress >= 1.0f; }
    [[nodiscard]] bool IsDestroyed() const { return health <= 0; }
    [[nodiscard]] glm::ivec2 GetCenter() const { return position + size / 2; }

    [[nodiscard]] nlohmann::json ToJson() const;
    static Building FromJson(const nlohmann::json& j);
};

/**
 * @brief Worker job types
 */
enum class WorkerJob : uint8_t {
    Idle = 0,
    Gathering,      // Collecting resources
    Building,       // Constructing buildings
    Repairing,      // Fixing damaged buildings
    Defending,      // Manning defenses
    Scouting,       // Exploring territory
    Trading,        // At trading post
    Count
};

/**
 * @brief Worker unit
 */
struct Worker {
    int id = -1;
    std::string name;
    int health = 100;
    int maxHealth = 100;
    WorkerJob job = WorkerJob::Idle;
    int assignedBuildingId = -1;
    glm::ivec2 position{0, 0};
    float efficiency = 1.0f;        // Work speed multiplier
    float morale = 100.0f;          // Affects efficiency
    int64_t hiredTimestamp = 0;

    [[nodiscard]] bool IsAlive() const { return health > 0; }
    [[nodiscard]] bool IsIdle() const { return job == WorkerJob::Idle; }

    [[nodiscard]] nlohmann::json ToJson() const;
    static Worker FromJson(const nlohmann::json& j);
};

/**
 * @brief Hero character data (the player's main character)
 */
struct HeroData {
    std::string playerId;
    std::string name;
    int level = 1;
    int experience = 0;
    int health = 100;
    int maxHealth = 100;
    glm::vec2 position{0.0f, 0.0f};
    float rotation = 0.0f;

    // Combat stats
    int zombiesKilled = 0;
    int deaths = 0;
    float survivalTime = 0.0f;      // Total hours survived

    // Equipment
    std::vector<int> inventory;
    int equippedWeapon = -1;

    // Status
    bool isOnline = false;
    int64_t lastOnlineTimestamp = 0;

    [[nodiscard]] nlohmann::json ToJson() const;
    static HeroData FromJson(const nlohmann::json& j);
};

/**
 * @brief Map tile change record
 */
struct TileChange {
    glm::ivec2 position{0, 0};
    int previousTileType = 0;
    int newTileType = 0;
    std::string changedBy;          // Player ID who made the change
    int64_t timestamp = 0;

    [[nodiscard]] nlohmann::json ToJson() const;
    static TileChange FromJson(const nlohmann::json& j);
};

/**
 * @brief Resource node in the world (harvestable)
 */
struct ResourceNode {
    int id = -1;
    ResourceType type = ResourceType::Wood;
    glm::ivec2 position{0, 0};
    int remaining = 100;            // Resources left to harvest
    int maxAmount = 100;
    float regenerationRate = 0.0f;  // Per hour, 0 = doesn't regenerate
    bool depleted = false;
    int64_t lastHarvestTimestamp = 0;

    [[nodiscard]] bool IsDepleted() const { return depleted || remaining <= 0; }

    [[nodiscard]] nlohmann::json ToJson() const;
    static ResourceNode FromJson(const nlohmann::json& j);
};

/**
 * @brief World event that occurred (for history/replay)
 */
struct WorldEvent {
    enum class Type {
        ZombieAttack,
        ResourceDepleted,
        ResourceDiscovered,
        BuildingDestroyed,
        WorkerDied,
        TradeCompleted,
        TerritoryContested,
        TerritoryLost,
        TerritoryGained,
        PlayerRaided,
        SeasonChanged,
        WorldBossSpawned,
        Custom
    };

    Type type = Type::Custom;
    std::string description;
    int64_t timestamp = 0;
    std::string affectedPlayerId;
    nlohmann::json data;

    [[nodiscard]] nlohmann::json ToJson() const;
    static WorldEvent FromJson(const nlohmann::json& j);
};

/**
 * @brief Complete world state for persistence
 */
struct WorldState {
    std::string playerId;
    std::string baseRegion;         // Geographic region

    // Buildings and workers
    std::vector<Building> buildings;
    std::vector<Worker> workers;

    // Resources
    ResourceStock resources;

    // Hero
    HeroData hero;

    // Map modifications
    std::vector<TileChange> mapChanges;
    std::vector<ResourceNode> resourceNodes;

    // Time tracking
    int64_t lastUpdateTimestamp = 0;
    int64_t lastLoginTimestamp = 0;
    int64_t createdTimestamp = 0;
    float totalPlayTime = 0.0f;     // Hours

    // Territory
    std::vector<glm::ivec2> ownedTiles;
    float territoryStrength = 0.0f;

    // Stats
    int totalZombiesKilled = 0;
    int totalBuildingsBuilt = 0;
    int totalWorkersHired = 0;
    int attacksSurvived = 0;

    [[nodiscard]] nlohmann::json ToJson() const;
    static WorldState FromJson(const nlohmann::json& j);

    // Helpers
    [[nodiscard]] Building* GetBuilding(int id);
    [[nodiscard]] const Building* GetBuilding(int id) const;
    [[nodiscard]] Worker* GetWorker(int id);
    [[nodiscard]] const Worker* GetWorker(int id) const;
    [[nodiscard]] int GetTotalPopulation() const;
    [[nodiscard]] int GetPopulationCapacity() const;
    [[nodiscard]] int GetIdleWorkers() const;
};

/**
 * @brief Persistent world manager
 *
 * Handles saving and loading world state to/from Firebase,
 * simulating offline time, and synchronizing with server.
 */
class PersistentWorld {
public:
    /**
     * @brief Callback types
     */
    using StateLoadedCallback = std::function<void(bool success, const WorldState& state)>;
    using StateSavedCallback = std::function<void(bool success)>;
    using OfflineReportCallback = std::function<void(const struct OfflineReport& report)>;

    /**
     * @brief Get singleton instance
     */
    [[nodiscard]] static PersistentWorld& Instance();

    // Non-copyable
    PersistentWorld(const PersistentWorld&) = delete;
    PersistentWorld& operator=(const PersistentWorld&) = delete;

    /**
     * @brief Initialize the persistent world system
     * @return true if successful
     */
    [[nodiscard]] bool Initialize();

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    /**
     * @brief Update (call from game loop)
     * @param deltaTime Time since last frame
     */
    void Update(float deltaTime);

    // ==================== State Management ====================

    /**
     * @brief Save current world state to Firebase
     * @param callback Optional callback for completion
     */
    void SaveState(StateSavedCallback callback = nullptr);

    /**
     * @brief Load world state from Firebase
     * @param playerId Player to load state for
     * @param callback Callback with loaded state
     */
    void LoadState(const std::string& playerId, StateLoadedCallback callback);

    /**
     * @brief Get current world state
     */
    [[nodiscard]] WorldState& GetState() { return m_state; }
    [[nodiscard]] const WorldState& GetState() const { return m_state; }

    /**
     * @brief Check if state has been loaded
     */
    [[nodiscard]] bool IsStateLoaded() const noexcept { return m_stateLoaded; }

    /**
     * @brief Mark state as dirty (needs save)
     */
    void MarkDirty() { m_dirty = true; }

    /**
     * @brief Check if state needs saving
     */
    [[nodiscard]] bool IsDirty() const noexcept { return m_dirty; }

    // ==================== Offline Simulation ====================

    /**
     * @brief Simulate what happened while player was offline
     * @param secondsElapsed Seconds since last login
     * @param callback Callback with offline report
     */
    void SimulateOfflineTime(int64_t secondsElapsed, OfflineReportCallback callback = nullptr);

    /**
     * @brief Get time elapsed since last login
     */
    [[nodiscard]] int64_t GetSecondsOffline() const;

    // ==================== Server Sync ====================

    /**
     * @brief Sync local state with server
     */
    void SyncWithServer();

    /**
     * @brief Force immediate sync
     */
    void ForceSyncNow();

    /**
     * @brief Set auto-save interval
     * @param seconds Seconds between auto-saves (0 to disable)
     */
    void SetAutoSaveInterval(float seconds) { m_autoSaveInterval = seconds; }

    // ==================== Building Management ====================

    /**
     * @brief Place a new building
     * @param type Building type
     * @param position Tile position
     * @return Building ID or -1 on failure
     */
    int PlaceBuilding(BuildingType type, const glm::ivec2& position);

    /**
     * @brief Remove a building
     * @param buildingId Building to remove
     * @return true if removed
     */
    bool RemoveBuilding(int buildingId);

    /**
     * @brief Upgrade a building
     * @param buildingId Building to upgrade
     * @return true if upgrade started
     */
    bool UpgradeBuilding(int buildingId);

    /**
     * @brief Repair a building
     * @param buildingId Building to repair
     * @param amount Health to restore
     * @return Actual health restored
     */
    int RepairBuilding(int buildingId, int amount);

    // ==================== Worker Management ====================

    /**
     * @brief Hire a new worker
     * @return Worker ID or -1 if can't hire
     */
    int HireWorker();

    /**
     * @brief Fire a worker
     * @param workerId Worker to fire
     * @return true if fired
     */
    bool FireWorker(int workerId);

    /**
     * @brief Assign worker to a job
     * @param workerId Worker to assign
     * @param job Job type
     * @param buildingId Building to work at (-1 for no building)
     * @return true if assigned
     */
    bool AssignWorker(int workerId, WorkerJob job, int buildingId = -1);

    /**
     * @brief Auto-assign idle workers to optimal jobs
     */
    void AutoAssignWorkers();

    // ==================== Resource Management ====================

    /**
     * @brief Add resources
     * @param type Resource type
     * @param amount Amount to add
     * @return Actual amount added (limited by capacity)
     */
    int AddResources(ResourceType type, int amount);

    /**
     * @brief Spend resources
     * @param type Resource type
     * @param amount Amount to spend
     * @return true if had enough resources
     */
    bool SpendResources(ResourceType type, int amount);

    /**
     * @brief Check if can afford resources
     */
    [[nodiscard]] bool CanAfford(ResourceType type, int amount) const;

    /**
     * @brief Calculate total production rate
     */
    [[nodiscard]] float CalculateProductionRate(ResourceType type) const;

    /**
     * @brief Calculate total consumption rate
     */
    [[nodiscard]] float CalculateConsumptionRate(ResourceType type) const;

    // ==================== Map Changes ====================

    /**
     * @brief Record a tile change
     * @param position Tile position
     * @param newTileType New tile type
     */
    void RecordTileChange(const glm::ivec2& position, int newTileType);

    /**
     * @brief Get map changes since timestamp
     */
    [[nodiscard]] std::vector<TileChange> GetMapChangesSince(int64_t timestamp) const;

    // ==================== Timestamps ====================

    /**
     * @brief Get current server timestamp
     */
    [[nodiscard]] int64_t GetCurrentTimestamp() const;

    /**
     * @brief Get last save timestamp
     */
    [[nodiscard]] int64_t GetLastSaveTimestamp() const { return m_lastSaveTimestamp; }

private:
    PersistentWorld() = default;
    ~PersistentWorld() = default;

    // Firebase paths
    [[nodiscard]] std::string GetPlayerStatePath(const std::string& playerId) const;
    [[nodiscard]] std::string GetWorldEventsPath() const;

    // Internal helpers
    void RecalculateProduction();
    void ProcessAutoSave(float deltaTime);
    int GenerateBuildingId();
    int GenerateWorkerId();

    // Building cost lookup
    [[nodiscard]] bool CanAffordBuilding(BuildingType type) const;
    void PayBuildingCost(BuildingType type);
    [[nodiscard]] glm::ivec2 GetBuildingSize(BuildingType type) const;
    [[nodiscard]] int GetBuildingMaxHealth(BuildingType type) const;
    [[nodiscard]] float GetBuildingProductionRate(BuildingType type) const;
    [[nodiscard]] ResourceType GetBuildingResourceType(BuildingType type) const;

    bool m_initialized = false;
    bool m_stateLoaded = false;
    bool m_dirty = false;

    WorldState m_state;
    mutable std::mutex m_stateMutex;

    // Auto-save
    float m_autoSaveInterval = 60.0f;   // Save every 60 seconds
    float m_autoSaveTimer = 0.0f;
    int64_t m_lastSaveTimestamp = 0;

    // ID generation
    int m_nextBuildingId = 1;
    int m_nextWorkerId = 1;
};

} // namespace Vehement
