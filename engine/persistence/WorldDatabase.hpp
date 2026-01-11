#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <set>
#include <mutex>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <sqlite3.h>

namespace Nova {

// Forward declarations
struct ChunkData;
struct Entity;
struct Player;
struct Building;
struct InventorySlot;
struct WorldMetadata;
class EntitySerializer;

/**
 * @brief Compressed chunk terrain data
 */
struct ChunkData {
    int chunkX = 0;
    int chunkY = 0;
    int chunkZ = 0;
    std::vector<uint8_t> terrainData;      // Compressed voxel data
    std::vector<uint8_t> biomeData;        // Biome IDs
    std::vector<uint8_t> lightingData;     // Light values
    bool isGenerated = false;
    bool isPopulated = false;
    bool isDirty = false;
    uint64_t modifiedAt = 0;
    int loadPriority = 0;
    std::string compressionType = "zlib";
    size_t uncompressedSize = 0;
    std::string checksum;
};

/**
 * @brief Entity structure for database storage
 */
struct Entity {
    int entityId = -1;
    int worldId = -1;
    std::string entityType;                // player, npc, building, item, projectile
    std::string entitySubtype;             // zombie, arrow, chest, etc.
    std::string uuid;                      // Unique identifier

    // Spatial data
    glm::ivec3 chunkPos = glm::ivec3(0);
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 velocity = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);

    // Component data (serialized)
    std::vector<uint8_t> data;

    // State
    bool isActive = true;
    bool isStatic = false;
    int ownerPlayerId = -1;
    float health = 100.0f;
    float maxHealth = 100.0f;
    uint32_t flags = 0;

    uint64_t createdAt = 0;
    uint64_t modifiedAt = 0;
};

/**
 * @brief Player data structure
 */
struct Player {
    int playerId = -1;
    int entityId = -1;
    std::string username;
    std::string displayName;
    std::string passwordHash;
    std::string email;

    // Stats
    int level = 1;
    int experience = 0;
    float health = 100.0f;
    float maxHealth = 100.0f;
    float mana = 100.0f;
    float maxMana = 100.0f;
    float stamina = 100.0f;
    float maxStamina = 100.0f;
    float hunger = 100.0f;
    float thirst = 100.0f;

    // Serialized data
    std::vector<uint8_t> stats;            // JSON stats
    std::vector<uint8_t> skills;           // Skill tree
    std::vector<uint8_t> achievements;     // Achievement data

    // Progression
    int deaths = 0;
    int kills = 0;
    std::string faction;
    int guildId = -1;

    // Currency
    int currencyGold = 0;
    int currencySilver = 0;
    int currencyPremium = 0;

    // Metadata
    std::string gameMode = "survival";
    bool isOnline = false;
    bool isBanned = false;
    std::string banReason;
    uint64_t createdAt = 0;
    uint64_t lastLogin = 0;
    uint64_t lastLogout = 0;
    uint64_t playTimeSeconds = 0;
};

/**
 * @brief Inventory slot
 */
struct InventorySlot {
    int slotIndex = 0;
    std::string itemId;
    int quantity = 1;
    float durability = 100.0f;
    float maxDurability = 100.0f;
    std::vector<uint8_t> itemData;         // Enchantments, properties, etc.
    bool isEquipped = false;
    bool isLocked = false;
    uint64_t acquiredAt = 0;
};

/**
 * @brief Equipment slot
 */
struct EquipmentSlot {
    std::string slotName;                  // head, chest, legs, weapon, shield, etc.
    std::string itemId;
    float durability = 100.0f;
    float maxDurability = 100.0f;
    std::vector<uint8_t> itemData;
    uint64_t equippedAt = 0;
};

/**
 * @brief Building structure
 */
struct Building {
    int buildingId = -1;
    int entityId = -1;
    int ownerPlayerId = -1;
    std::string buildingType;
    std::string buildingName;
    float health = 100.0f;
    float maxHealth = 100.0f;
    std::string faction;
    float constructionProgress = 100.0f;
    bool isConstructing = false;
    uint64_t constructionStarted = 0;
    uint64_t constructionCompleted = 0;
    std::vector<uint8_t> storageData;      // Building inventory
    std::vector<uint8_t> productionQueue;  // Unit production
    int upgradeLevel = 1;
};

/**
 * @brief World metadata
 */
struct WorldMetadata {
    int worldId = -1;
    std::string worldName;
    std::string description;
    int seed = 0;
    uint64_t createdAt = 0;
    uint64_t lastSaved = 0;
    uint64_t lastAccessed = 0;
    int schemaVersion = 1;
    glm::ivec3 worldSize = glm::ivec3(1000, 256, 1000);
    glm::vec3 spawnPoint = glm::vec3(0.0f, 100.0f, 0.0f);
    float gameTime = 0.0f;
    uint64_t realPlayTime = 0;
    std::string difficulty = "normal";
    std::string gameMode = "survival";
    std::string customData;                // JSON
    bool isActive = true;
};

/**
 * @brief Query result for entity radius queries
 */
struct EntityQueryResult {
    std::vector<Entity> entities;
    size_t totalCount = 0;
    float queryTime = 0.0f;                // Milliseconds
};

/**
 * @brief Database statistics
 */
struct DatabaseStats {
    size_t totalChunks = 0;
    size_t generatedChunks = 0;
    size_t dirtyChunks = 0;
    size_t totalEntities = 0;
    size_t activeEntities = 0;
    size_t totalPlayers = 0;
    size_t onlinePlayers = 0;
    size_t databaseSizeBytes = 0;
    float avgQueryTime = 0.0f;
};

/**
 * @brief Main world database for SQLite-based persistence
 *
 * Features:
 * - Complete world state storage
 * - Chunk-based terrain with spatial indexing
 * - Entity storage with R-tree queries
 * - Player profiles and progression
 * - Transaction batching for performance
 * - Prepared statements for fast queries
 * - Backup and restore functionality
 * - Schema versioning and migration
 */
class WorldDatabase {
public:
    WorldDatabase();
    ~WorldDatabase();

    // Prevent copying
    WorldDatabase(const WorldDatabase&) = delete;
    WorldDatabase& operator=(const WorldDatabase&) = delete;

    // =========================================================================
    // INITIALIZATION
    // =========================================================================

    /**
     * @brief Initialize database connection
     * @param dbPath Path to SQLite database file
     * @return True if initialization succeeded
     */
    bool Initialize(const std::string& dbPath);

    /**
     * @brief Shutdown database and flush changes
     */
    void Shutdown();

    /**
     * @brief Check if database is initialized
     */
    bool IsInitialized() const { return m_db != nullptr; }

    /**
     * @brief Get database file path
     */
    const std::string& GetDatabasePath() const { return m_dbPath; }

    // =========================================================================
    // WORLD OPERATIONS
    // =========================================================================

    /**
     * @brief Create a new world
     * @param name World name
     * @param seed World generation seed
     * @return World ID, or -1 on failure
     */
    int CreateWorld(const std::string& name, int seed);

    /**
     * @brief Load world by ID
     * @param worldId World ID
     * @return True if load succeeded
     */
    bool LoadWorld(int worldId);

    /**
     * @brief Load world by name
     * @param name World name
     * @return True if load succeeded
     */
    bool LoadWorldByName(const std::string& name);

    /**
     * @brief Save current world state
     */
    void SaveWorld();

    /**
     * @brief Unload current world
     */
    void UnloadWorld();

    /**
     * @brief Delete world permanently
     * @param worldId World ID
     * @return True if deletion succeeded
     */
    bool DeleteWorld(int worldId);

    /**
     * @brief Get current world ID
     */
    int GetCurrentWorldId() const { return m_currentWorldId; }

    /**
     * @brief Get world metadata
     */
    WorldMetadata GetWorldMetadata(int worldId);

    /**
     * @brief Update world metadata
     */
    bool UpdateWorldMetadata(const WorldMetadata& metadata);

    /**
     * @brief List all worlds
     */
    std::vector<WorldMetadata> ListWorlds();

    // =========================================================================
    // CHUNK OPERATIONS
    // =========================================================================

    /**
     * @brief Save chunk data
     * @param chunkX Chunk X coordinate
     * @param chunkY Chunk Y coordinate
     * @param chunkZ Chunk Z coordinate
     * @param data Chunk data
     * @return True if save succeeded
     */
    bool SaveChunk(int chunkX, int chunkY, int chunkZ, const ChunkData& data);

    /**
     * @brief Load chunk data
     * @param chunkX Chunk X coordinate
     * @param chunkY Chunk Y coordinate
     * @param chunkZ Chunk Z coordinate
     * @return Chunk data, or empty chunk if not found
     */
    ChunkData LoadChunk(int chunkX, int chunkY, int chunkZ);

    /**
     * @brief Check if chunk exists and is generated
     */
    bool IsChunkGenerated(int chunkX, int chunkY, int chunkZ);

    /**
     * @brief Delete chunk data
     */
    bool DeleteChunk(int chunkX, int chunkY, int chunkZ);

    /**
     * @brief Get chunks within radius
     * @param center Center position
     * @param radius Search radius
     * @return List of chunk positions
     */
    std::vector<glm::ivec3> GetChunksInRadius(glm::vec3 center, float radius);

    /**
     * @brief Get all dirty chunks (need saving)
     */
    std::vector<glm::ivec3> GetDirtyChunks();

    /**
     * @brief Mark chunk as dirty
     */
    bool MarkChunkDirty(int chunkX, int chunkY, int chunkZ, bool dirty = true);

    // =========================================================================
    // ENTITY OPERATIONS
    // =========================================================================

    /**
     * @brief Save entity
     * @param entity Entity data
     * @return Entity ID, or -1 on failure
     */
    int SaveEntity(const Entity& entity);

    /**
     * @brief Load entity by ID
     */
    Entity LoadEntity(int entityId);

    /**
     * @brief Load entity by UUID
     */
    Entity LoadEntityByUUID(const std::string& uuid);

    /**
     * @brief Delete entity
     */
    bool DeleteEntity(int entityId);

    /**
     * @brief Load entities in chunk
     */
    std::vector<Entity> LoadEntitiesInChunk(int chunkX, int chunkY, int chunkZ);

    /**
     * @brief Query entities within radius (uses R-tree)
     * @param center Center position
     * @param radius Search radius
     * @return Query result with entities
     */
    EntityQueryResult QueryEntitiesInRadius(glm::vec3 center, float radius);

    /**
     * @brief Query entities by type
     */
    std::vector<Entity> QueryEntitiesByType(const std::string& entityType);

    /**
     * @brief Count entities in world
     */
    size_t CountEntities(bool activeOnly = true);

    // =========================================================================
    // PLAYER OPERATIONS
    // =========================================================================

    /**
     * @brief Create new player
     * @param username Player username
     * @return Player ID, or -1 on failure
     */
    int CreatePlayer(const std::string& username);

    /**
     * @brief Load player by username
     */
    Player LoadPlayer(const std::string& username);

    /**
     * @brief Load player by ID
     */
    Player LoadPlayerById(int playerId);

    /**
     * @brief Save player data
     */
    bool SavePlayer(const Player& player);

    /**
     * @brief Delete player
     */
    bool DeletePlayer(int playerId);

    /**
     * @brief Check if player exists
     */
    bool PlayerExists(const std::string& username);

    /**
     * @brief Get all players
     */
    std::vector<Player> GetAllPlayers();

    /**
     * @brief Get online players
     */
    std::vector<Player> GetOnlinePlayers();

    // =========================================================================
    // INVENTORY OPERATIONS
    // =========================================================================

    /**
     * @brief Save player inventory
     */
    bool SaveInventory(int playerId, const std::vector<InventorySlot>& inventory);

    /**
     * @brief Load player inventory
     */
    std::vector<InventorySlot> LoadInventory(int playerId);

    /**
     * @brief Clear inventory
     */
    bool ClearInventory(int playerId);

    // =========================================================================
    // EQUIPMENT OPERATIONS
    // =========================================================================

    /**
     * @brief Save player equipment
     */
    bool SaveEquipment(int playerId, const std::map<std::string, EquipmentSlot>& equipment);

    /**
     * @brief Load player equipment
     */
    std::map<std::string, EquipmentSlot> LoadEquipment(int playerId);

    // =========================================================================
    // BUILDING OPERATIONS
    // =========================================================================

    /**
     * @brief Save building
     */
    int SaveBuilding(const Building& building);

    /**
     * @brief Load building by ID
     */
    Building LoadBuilding(int buildingId);

    /**
     * @brief Get buildings owned by player
     */
    std::vector<Building> GetPlayerBuildings(int playerId);

    // =========================================================================
    // TRANSACTION SUPPORT
    // =========================================================================

    /**
     * @brief Begin transaction
     */
    bool BeginTransaction();

    /**
     * @brief Commit transaction
     */
    bool Commit();

    /**
     * @brief Rollback transaction
     */
    bool Rollback();

    /**
     * @brief Begin batch operation (for multiple saves)
     */
    void BeginBatch();

    /**
     * @brief End batch operation and commit
     */
    void EndBatch();

    /**
     * @brief Check if in transaction
     */
    bool IsInTransaction() const { return m_inTransaction; }

    // =========================================================================
    // MAINTENANCE
    // =========================================================================

    /**
     * @brief Vacuum database (reclaim space)
     */
    bool VacuumDatabase();

    /**
     * @brief Analyze database for query optimization
     */
    bool AnalyzeDatabase();

    /**
     * @brief Check database integrity
     */
    bool CheckIntegrity();

    /**
     * @brief Create backup
     */
    bool CreateBackup(const std::string& backupPath);

    /**
     * @brief Restore from backup
     */
    bool RestoreFromBackup(const std::string& backupPath);

    /**
     * @brief Get database statistics
     */
    DatabaseStats GetStatistics();

    /**
     * @brief Get database file size
     */
    size_t GetDatabaseSize();

    // =========================================================================
    // CALLBACKS
    // =========================================================================

    std::function<void(const std::string& error)> OnError;
    std::function<void(const std::string& operation, float timeMs)> OnSlowQuery;

private:
    // Prepared statements
    bool PrepareStatements();
    void FinalizeStatements();

    // Helper functions
    bool ExecuteSQL(const std::string& sql);
    int64_t GetLastInsertRowId();
    uint64_t GetCurrentTimestamp();
    std::string GenerateUUID();

    // Error handling
    void LogError(const std::string& message);
    void CheckSlowQuery(const std::string& operation, float timeMs);

    sqlite3* m_db = nullptr;
    std::string m_dbPath;
    int m_currentWorldId = -1;
    bool m_inTransaction = false;
    int m_batchDepth = 0;
    mutable std::mutex m_dbMutex;

    // Prepared statements for performance
    sqlite3_stmt* m_stmtSaveChunk = nullptr;
    sqlite3_stmt* m_stmtLoadChunk = nullptr;
    sqlite3_stmt* m_stmtIsChunkGenerated = nullptr;
    sqlite3_stmt* m_stmtSaveEntity = nullptr;
    sqlite3_stmt* m_stmtLoadEntity = nullptr;
    sqlite3_stmt* m_stmtLoadEntityByUUID = nullptr;
    sqlite3_stmt* m_stmtDeleteEntity = nullptr;
    sqlite3_stmt* m_stmtSavePlayer = nullptr;
    sqlite3_stmt* m_stmtLoadPlayer = nullptr;
    sqlite3_stmt* m_stmtSaveInventorySlot = nullptr;
    sqlite3_stmt* m_stmtLoadInventory = nullptr;
    sqlite3_stmt* m_stmtSaveEquipment = nullptr;
    sqlite3_stmt* m_stmtLoadEquipment = nullptr;

    // Performance tracking
    float m_totalQueryTime = 0.0f;
    size_t m_totalQueries = 0;
    float m_slowQueryThreshold = 10.0f;  // Milliseconds
};

} // namespace Nova
