#include "WorldDatabase.hpp"
#include "EntitySerializer.hpp"
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <random>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace Nova {

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

static uint64_t GetTimestamp() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

static std::string GenerateUUID() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    ss << std::setw(16) << dis(gen);
    ss << std::setw(16) << dis(gen);
    return ss.str();
}

static float GetTimeMs() {
    static auto start = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<float, std::milli>(now - start).count();
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

WorldDatabase::WorldDatabase() = default;

WorldDatabase::~WorldDatabase() {
    Shutdown();
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool WorldDatabase::Initialize(const std::string& dbPath) {
    std::lock_guard<std::mutex> lock(m_dbMutex);

    if (m_db) {
        LogError("Database already initialized");
        return false;
    }

    m_dbPath = dbPath;

    // Open database
    int result = sqlite3_open(dbPath.c_str(), &m_db);
    if (result != SQLITE_OK) {
        LogError("Failed to open database: " + std::string(sqlite3_errmsg(m_db)));
        m_db = nullptr;
        return false;
    }

    // Enable foreign keys
    ExecuteSQL("PRAGMA foreign_keys = ON;");

    // Performance optimizations
    ExecuteSQL("PRAGMA journal_mode = WAL;");        // Write-Ahead Logging
    ExecuteSQL("PRAGMA synchronous = NORMAL;");      // Faster writes
    ExecuteSQL("PRAGMA cache_size = -64000;");       // 64MB cache
    ExecuteSQL("PRAGMA temp_store = MEMORY;");       // Temp tables in memory
    ExecuteSQL("PRAGMA mmap_size = 268435456;");     // 256MB memory-mapped I/O

    // Load schema if tables don't exist
    const char* checkTableSQL = "SELECT name FROM sqlite_master WHERE type='table' AND name='WorldMeta';";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, checkTableSQL, -1, &stmt, nullptr) == SQLITE_OK) {
        bool tablesExist = (sqlite3_step(stmt) == SQLITE_ROW);
        sqlite3_finalize(stmt);

        if (!tablesExist) {
            // Load and execute schema
            std::ifstream schemaFile("assets/sql/world_schema.sql");
            if (!schemaFile) {
                LogError("Failed to load schema file");
                sqlite3_close(m_db);
                m_db = nullptr;
                return false;
            }

            std::stringstream buffer;
            buffer << schemaFile.rdbuf();
            std::string schema = buffer.str();

            char* errMsg = nullptr;
            result = sqlite3_exec(m_db, schema.c_str(), nullptr, nullptr, &errMsg);
            if (result != SQLITE_OK) {
                LogError("Failed to execute schema: " + std::string(errMsg ? errMsg : "Unknown error"));
                sqlite3_free(errMsg);
                sqlite3_close(m_db);
                m_db = nullptr;
                return false;
            }
        }
    }

    // Prepare statements
    if (!PrepareStatements()) {
        LogError("Failed to prepare statements");
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }

    return true;
}

void WorldDatabase::Shutdown() {
    std::lock_guard<std::mutex> lock(m_dbMutex);

    if (!m_db) return;

    // Finalize prepared statements
    FinalizeStatements();

    // Close database
    sqlite3_close(m_db);
    m_db = nullptr;
    m_currentWorldId = -1;
}

bool WorldDatabase::PrepareStatements() {
    // Save chunk
    const char* sqlSaveChunk = R"(
        INSERT OR REPLACE INTO Chunks (world_id, chunk_x, chunk_y, chunk_z, data, biome_data, lighting_data,
                                      is_generated, is_populated, is_dirty, modified_at, compression_type,
                                      uncompressed_size, checksum)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    if (sqlite3_prepare_v2(m_db, sqlSaveChunk, -1, &m_stmtSaveChunk, nullptr) != SQLITE_OK) {
        LogError("Failed to prepare save chunk statement");
        return false;
    }

    // Load chunk
    const char* sqlLoadChunk = R"(
        SELECT chunk_x, chunk_y, chunk_z, data, biome_data, lighting_data, is_generated, is_populated,
               is_dirty, modified_at, compression_type, uncompressed_size, checksum
        FROM Chunks WHERE world_id = ? AND chunk_x = ? AND chunk_y = ? AND chunk_z = ?
    )";
    if (sqlite3_prepare_v2(m_db, sqlLoadChunk, -1, &m_stmtLoadChunk, nullptr) != SQLITE_OK) {
        LogError("Failed to prepare load chunk statement");
        return false;
    }

    // Check chunk generated
    const char* sqlIsChunkGenerated = "SELECT is_generated FROM Chunks WHERE world_id = ? AND chunk_x = ? AND chunk_y = ? AND chunk_z = ?";
    if (sqlite3_prepare_v2(m_db, sqlIsChunkGenerated, -1, &m_stmtIsChunkGenerated, nullptr) != SQLITE_OK) {
        LogError("Failed to prepare is chunk generated statement");
        return false;
    }

    // Save entity
    const char* sqlSaveEntity = R"(
        INSERT OR REPLACE INTO Entities (entity_id, world_id, entity_type, entity_subtype, entity_uuid,
                                        chunk_x, chunk_y, chunk_z, position_x, position_y, position_z,
                                        rotation_x, rotation_y, rotation_z, rotation_w,
                                        velocity_x, velocity_y, velocity_z, scale_x, scale_y, scale_z,
                                        data, is_active, is_static, owner_player_id, health, max_health,
                                        flags, modified_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    if (sqlite3_prepare_v2(m_db, sqlSaveEntity, -1, &m_stmtSaveEntity, nullptr) != SQLITE_OK) {
        LogError("Failed to prepare save entity statement");
        return false;
    }

    // Load entity
    const char* sqlLoadEntity = R"(
        SELECT entity_id, world_id, entity_type, entity_subtype, entity_uuid, chunk_x, chunk_y, chunk_z,
               position_x, position_y, position_z, rotation_x, rotation_y, rotation_z, rotation_w,
               velocity_x, velocity_y, velocity_z, scale_x, scale_y, scale_z, data, is_active, is_static,
               owner_player_id, health, max_health, flags, created_at, modified_at
        FROM Entities WHERE entity_id = ?
    )";
    if (sqlite3_prepare_v2(m_db, sqlLoadEntity, -1, &m_stmtLoadEntity, nullptr) != SQLITE_OK) {
        LogError("Failed to prepare load entity statement");
        return false;
    }

    // Load entity by UUID
    const char* sqlLoadEntityByUUID = R"(
        SELECT entity_id, world_id, entity_type, entity_subtype, entity_uuid, chunk_x, chunk_y, chunk_z,
               position_x, position_y, position_z, rotation_x, rotation_y, rotation_z, rotation_w,
               velocity_x, velocity_y, velocity_z, scale_x, scale_y, scale_z, data, is_active, is_static,
               owner_player_id, health, max_health, flags, created_at, modified_at
        FROM Entities WHERE entity_uuid = ?
    )";
    if (sqlite3_prepare_v2(m_db, sqlLoadEntityByUUID, -1, &m_stmtLoadEntityByUUID, nullptr) != SQLITE_OK) {
        LogError("Failed to prepare load entity by UUID statement");
        return false;
    }

    // Delete entity
    const char* sqlDeleteEntity = "DELETE FROM Entities WHERE entity_id = ?";
    if (sqlite3_prepare_v2(m_db, sqlDeleteEntity, -1, &m_stmtDeleteEntity, nullptr) != SQLITE_OK) {
        LogError("Failed to prepare delete entity statement");
        return false;
    }

    // Save player
    const char* sqlSavePlayer = R"(
        INSERT OR REPLACE INTO Players (player_id, entity_id, username, display_name, password_hash, email,
                                       level, experience, health, max_health, mana, max_mana, stamina, max_stamina,
                                       hunger, thirst, stats, skills, achievements, deaths, kills, faction, guild_id,
                                       currency_gold, currency_silver, currency_premium, game_mode, is_online,
                                       is_banned, ban_reason, last_login, last_logout, play_time_seconds)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    if (sqlite3_prepare_v2(m_db, sqlSavePlayer, -1, &m_stmtSavePlayer, nullptr) != SQLITE_OK) {
        LogError("Failed to prepare save player statement");
        return false;
    }

    // Load player
    const char* sqlLoadPlayer = R"(
        SELECT player_id, entity_id, username, display_name, password_hash, email, level, experience,
               health, max_health, mana, max_mana, stamina, max_stamina, hunger, thirst, stats, skills,
               achievements, deaths, kills, faction, guild_id, currency_gold, currency_silver, currency_premium,
               game_mode, is_online, is_banned, ban_reason, created_at, last_login, last_logout, play_time_seconds
        FROM Players WHERE username = ?
    )";
    if (sqlite3_prepare_v2(m_db, sqlLoadPlayer, -1, &m_stmtLoadPlayer, nullptr) != SQLITE_OK) {
        LogError("Failed to prepare load player statement");
        return false;
    }

    // Save inventory slot
    const char* sqlSaveInventorySlot = R"(
        INSERT OR REPLACE INTO Inventory (player_id, slot_index, item_id, quantity, durability, max_durability,
                                         item_data, is_equipped, is_locked, acquired_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    if (sqlite3_prepare_v2(m_db, sqlSaveInventorySlot, -1, &m_stmtSaveInventorySlot, nullptr) != SQLITE_OK) {
        LogError("Failed to prepare save inventory slot statement");
        return false;
    }

    // Load inventory
    const char* sqlLoadInventory = R"(
        SELECT slot_index, item_id, quantity, durability, max_durability, item_data, is_equipped, is_locked, acquired_at
        FROM Inventory WHERE player_id = ? ORDER BY slot_index
    )";
    if (sqlite3_prepare_v2(m_db, sqlLoadInventory, -1, &m_stmtLoadInventory, nullptr) != SQLITE_OK) {
        LogError("Failed to prepare load inventory statement");
        return false;
    }

    // Save equipment
    const char* sqlSaveEquipment = R"(
        INSERT OR REPLACE INTO Equipment (player_id, slot_name, item_id, durability, max_durability, item_data, equipped_at)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )";
    if (sqlite3_prepare_v2(m_db, sqlSaveEquipment, -1, &m_stmtSaveEquipment, nullptr) != SQLITE_OK) {
        LogError("Failed to prepare save equipment statement");
        return false;
    }

    // Load equipment
    const char* sqlLoadEquipment = R"(
        SELECT slot_name, item_id, durability, max_durability, item_data, equipped_at
        FROM Equipment WHERE player_id = ?
    )";
    if (sqlite3_prepare_v2(m_db, sqlLoadEquipment, -1, &m_stmtLoadEquipment, nullptr) != SQLITE_OK) {
        LogError("Failed to prepare load equipment statement");
        return false;
    }

    return true;
}

void WorldDatabase::FinalizeStatements() {
    if (m_stmtSaveChunk) sqlite3_finalize(m_stmtSaveChunk);
    if (m_stmtLoadChunk) sqlite3_finalize(m_stmtLoadChunk);
    if (m_stmtIsChunkGenerated) sqlite3_finalize(m_stmtIsChunkGenerated);
    if (m_stmtSaveEntity) sqlite3_finalize(m_stmtSaveEntity);
    if (m_stmtLoadEntity) sqlite3_finalize(m_stmtLoadEntity);
    if (m_stmtLoadEntityByUUID) sqlite3_finalize(m_stmtLoadEntityByUUID);
    if (m_stmtDeleteEntity) sqlite3_finalize(m_stmtDeleteEntity);
    if (m_stmtSavePlayer) sqlite3_finalize(m_stmtSavePlayer);
    if (m_stmtLoadPlayer) sqlite3_finalize(m_stmtLoadPlayer);
    if (m_stmtSaveInventorySlot) sqlite3_finalize(m_stmtSaveInventorySlot);
    if (m_stmtLoadInventory) sqlite3_finalize(m_stmtLoadInventory);
    if (m_stmtSaveEquipment) sqlite3_finalize(m_stmtSaveEquipment);
    if (m_stmtLoadEquipment) sqlite3_finalize(m_stmtLoadEquipment);

    m_stmtSaveChunk = nullptr;
    m_stmtLoadChunk = nullptr;
    m_stmtIsChunkGenerated = nullptr;
    m_stmtSaveEntity = nullptr;
    m_stmtLoadEntity = nullptr;
    m_stmtLoadEntityByUUID = nullptr;
    m_stmtDeleteEntity = nullptr;
    m_stmtSavePlayer = nullptr;
    m_stmtLoadPlayer = nullptr;
    m_stmtSaveInventorySlot = nullptr;
    m_stmtLoadInventory = nullptr;
    m_stmtSaveEquipment = nullptr;
    m_stmtLoadEquipment = nullptr;
}

// ============================================================================
// WORLD OPERATIONS
// ============================================================================

int WorldDatabase::CreateWorld(const std::string& name, int seed) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db) return -1;

    float startTime = GetTimeMs();

    const char* sql = R"(
        INSERT INTO WorldMeta (world_name, seed, created_at, last_saved, last_accessed)
        VALUES (?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LogError("Failed to prepare create world statement");
        return -1;
    }

    uint64_t now = GetTimestamp();
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, seed);
    sqlite3_bind_int64(stmt, 3, now);
    sqlite3_bind_int64(stmt, 4, now);
    sqlite3_bind_int64(stmt, 5, now);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
        LogError("Failed to create world: " + std::string(sqlite3_errmsg(m_db)));
        return -1;
    }

    int worldId = static_cast<int>(sqlite3_last_insert_rowid(m_db));
    CheckSlowQuery("CreateWorld", GetTimeMs() - startTime);
    return worldId;
}

bool WorldDatabase::LoadWorld(int worldId) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db) return false;

    float startTime = GetTimeMs();

    const char* sql = "SELECT world_id FROM WorldMeta WHERE world_id = ? AND is_active = 1";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LogError("Failed to prepare load world statement");
        return false;
    }

    sqlite3_bind_int(stmt, 1, worldId);
    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_ROW) {
        LogError("World not found: " + std::to_string(worldId));
        return false;
    }

    m_currentWorldId = worldId;

    // Update last accessed time
    ExecuteSQL("UPDATE WorldMeta SET last_accessed = " + std::to_string(GetTimestamp()) +
               " WHERE world_id = " + std::to_string(worldId));

    CheckSlowQuery("LoadWorld", GetTimeMs() - startTime);
    return true;
}

bool WorldDatabase::LoadWorldByName(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db) return false;

    const char* sql = "SELECT world_id FROM WorldMeta WHERE world_name = ? AND is_active = 1";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    int result = sqlite3_step(stmt);

    if (result == SQLITE_ROW) {
        int worldId = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        m_dbMutex.unlock();
        return LoadWorld(worldId);
    }

    sqlite3_finalize(stmt);
    return false;
}

void WorldDatabase::SaveWorld() {
    if (!m_db || m_currentWorldId < 0) return;

    std::lock_guard<std::mutex> lock(m_dbMutex);
    ExecuteSQL("UPDATE WorldMeta SET last_saved = " + std::to_string(GetTimestamp()) +
               " WHERE world_id = " + std::to_string(m_currentWorldId));
}

void WorldDatabase::UnloadWorld() {
    SaveWorld();
    m_currentWorldId = -1;
}

bool WorldDatabase::DeleteWorld(int worldId) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db) return false;

    // Soft delete by setting is_active to 0
    const char* sql = "UPDATE WorldMeta SET is_active = 0 WHERE world_id = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, worldId);
    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}

WorldMetadata WorldDatabase::GetWorldMetadata(int worldId) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    WorldMetadata metadata;
    if (!m_db) return metadata;

    const char* sql = R"(
        SELECT world_id, world_name, world_description, seed, created_at, last_saved, last_accessed,
               schema_version, world_size_x, world_size_y, world_size_z, spawn_x, spawn_y, spawn_z,
               game_time, real_play_time, difficulty, game_mode, custom_data, is_active
        FROM WorldMeta WHERE world_id = ?
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return metadata;
    }

    sqlite3_bind_int(stmt, 1, worldId);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        metadata.worldId = sqlite3_column_int(stmt, 0);
        metadata.worldName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (sqlite3_column_text(stmt, 2)) {
            metadata.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        }
        metadata.seed = sqlite3_column_int(stmt, 3);
        metadata.createdAt = sqlite3_column_int64(stmt, 4);
        metadata.lastSaved = sqlite3_column_int64(stmt, 5);
        metadata.lastAccessed = sqlite3_column_int64(stmt, 6);
        metadata.schemaVersion = sqlite3_column_int(stmt, 7);
        metadata.worldSize.x = sqlite3_column_int(stmt, 8);
        metadata.worldSize.y = sqlite3_column_int(stmt, 9);
        metadata.worldSize.z = sqlite3_column_int(stmt, 10);
        metadata.spawnPoint.x = static_cast<float>(sqlite3_column_double(stmt, 11));
        metadata.spawnPoint.y = static_cast<float>(sqlite3_column_double(stmt, 12));
        metadata.spawnPoint.z = static_cast<float>(sqlite3_column_double(stmt, 13));
        metadata.gameTime = static_cast<float>(sqlite3_column_double(stmt, 14));
        metadata.realPlayTime = sqlite3_column_int64(stmt, 15);
        metadata.difficulty = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 16));
        metadata.gameMode = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 17));
        if (sqlite3_column_text(stmt, 18)) {
            metadata.customData = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 18));
        }
        metadata.isActive = sqlite3_column_int(stmt, 19) != 0;
    }

    sqlite3_finalize(stmt);
    return metadata;
}

bool WorldDatabase::UpdateWorldMetadata(const WorldMetadata& metadata) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db) return false;

    const char* sql = R"(
        UPDATE WorldMeta SET world_description = ?, spawn_x = ?, spawn_y = ?, spawn_z = ?,
                            game_time = ?, real_play_time = ?, difficulty = ?, game_mode = ?,
                            custom_data = ?, last_saved = ?
        WHERE world_id = ?
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, metadata.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 2, metadata.spawnPoint.x);
    sqlite3_bind_double(stmt, 3, metadata.spawnPoint.y);
    sqlite3_bind_double(stmt, 4, metadata.spawnPoint.z);
    sqlite3_bind_double(stmt, 5, metadata.gameTime);
    sqlite3_bind_int64(stmt, 6, metadata.realPlayTime);
    sqlite3_bind_text(stmt, 7, metadata.difficulty.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, metadata.gameMode.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, metadata.customData.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 10, GetTimestamp());
    sqlite3_bind_int(stmt, 11, metadata.worldId);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}

std::vector<WorldMetadata> WorldDatabase::ListWorlds() {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    std::vector<WorldMetadata> worlds;
    if (!m_db) return worlds;

    const char* sql = "SELECT world_id FROM WorldMeta WHERE is_active = 1 ORDER BY last_accessed DESC";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return worlds;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int worldId = sqlite3_column_int(stmt, 0);
        m_dbMutex.unlock();
        worlds.push_back(GetWorldMetadata(worldId));
        m_dbMutex.lock();
    }

    sqlite3_finalize(stmt);
    return worlds;
}

// ============================================================================
// CHUNK OPERATIONS
// ============================================================================

bool WorldDatabase::SaveChunk(int chunkX, int chunkY, int chunkZ, const ChunkData& data) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db || m_currentWorldId < 0 || !m_stmtSaveChunk) return false;

    float startTime = GetTimeMs();

    sqlite3_reset(m_stmtSaveChunk);
    sqlite3_clear_bindings(m_stmtSaveChunk);

    sqlite3_bind_int(m_stmtSaveChunk, 1, m_currentWorldId);
    sqlite3_bind_int(m_stmtSaveChunk, 2, chunkX);
    sqlite3_bind_int(m_stmtSaveChunk, 3, chunkY);
    sqlite3_bind_int(m_stmtSaveChunk, 4, chunkZ);
    sqlite3_bind_blob(m_stmtSaveChunk, 5, data.terrainData.data(), data.terrainData.size(), SQLITE_TRANSIENT);
    sqlite3_bind_blob(m_stmtSaveChunk, 6, data.biomeData.data(), data.biomeData.size(), SQLITE_TRANSIENT);
    sqlite3_bind_blob(m_stmtSaveChunk, 7, data.lightingData.data(), data.lightingData.size(), SQLITE_TRANSIENT);
    sqlite3_bind_int(m_stmtSaveChunk, 8, data.isGenerated ? 1 : 0);
    sqlite3_bind_int(m_stmtSaveChunk, 9, data.isPopulated ? 1 : 0);
    sqlite3_bind_int(m_stmtSaveChunk, 10, data.isDirty ? 1 : 0);
    sqlite3_bind_int64(m_stmtSaveChunk, 11, GetTimestamp());
    sqlite3_bind_text(m_stmtSaveChunk, 12, data.compressionType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(m_stmtSaveChunk, 13, static_cast<int>(data.uncompressedSize));
    sqlite3_bind_text(m_stmtSaveChunk, 14, data.checksum.c_str(), -1, SQLITE_TRANSIENT);

    int result = sqlite3_step(m_stmtSaveChunk);
    CheckSlowQuery("SaveChunk", GetTimeMs() - startTime);

    return result == SQLITE_DONE;
}

ChunkData WorldDatabase::LoadChunk(int chunkX, int chunkY, int chunkZ) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    ChunkData chunk;
    if (!m_db || m_currentWorldId < 0 || !m_stmtLoadChunk) return chunk;

    float startTime = GetTimeMs();

    sqlite3_reset(m_stmtLoadChunk);
    sqlite3_clear_bindings(m_stmtLoadChunk);

    sqlite3_bind_int(m_stmtLoadChunk, 1, m_currentWorldId);
    sqlite3_bind_int(m_stmtLoadChunk, 2, chunkX);
    sqlite3_bind_int(m_stmtLoadChunk, 3, chunkY);
    sqlite3_bind_int(m_stmtLoadChunk, 4, chunkZ);

    if (sqlite3_step(m_stmtLoadChunk) == SQLITE_ROW) {
        chunk.chunkX = sqlite3_column_int(m_stmtLoadChunk, 0);
        chunk.chunkY = sqlite3_column_int(m_stmtLoadChunk, 1);
        chunk.chunkZ = sqlite3_column_int(m_stmtLoadChunk, 2);

        // Terrain data
        const void* terrainBlob = sqlite3_column_blob(m_stmtLoadChunk, 3);
        int terrainSize = sqlite3_column_bytes(m_stmtLoadChunk, 3);
        if (terrainBlob && terrainSize > 0) {
            chunk.terrainData.resize(terrainSize);
            std::memcpy(chunk.terrainData.data(), terrainBlob, terrainSize);
        }

        // Biome data
        const void* biomeBlob = sqlite3_column_blob(m_stmtLoadChunk, 4);
        int biomeSize = sqlite3_column_bytes(m_stmtLoadChunk, 4);
        if (biomeBlob && biomeSize > 0) {
            chunk.biomeData.resize(biomeSize);
            std::memcpy(chunk.biomeData.data(), biomeBlob, biomeSize);
        }

        // Lighting data
        const void* lightBlob = sqlite3_column_blob(m_stmtLoadChunk, 5);
        int lightSize = sqlite3_column_bytes(m_stmtLoadChunk, 5);
        if (lightBlob && lightSize > 0) {
            chunk.lightingData.resize(lightSize);
            std::memcpy(chunk.lightingData.data(), lightBlob, lightSize);
        }

        chunk.isGenerated = sqlite3_column_int(m_stmtLoadChunk, 6) != 0;
        chunk.isPopulated = sqlite3_column_int(m_stmtLoadChunk, 7) != 0;
        chunk.isDirty = sqlite3_column_int(m_stmtLoadChunk, 8) != 0;
        chunk.modifiedAt = sqlite3_column_int64(m_stmtLoadChunk, 9);
        chunk.compressionType = reinterpret_cast<const char*>(sqlite3_column_text(m_stmtLoadChunk, 10));
        chunk.uncompressedSize = sqlite3_column_int(m_stmtLoadChunk, 11);
        if (sqlite3_column_text(m_stmtLoadChunk, 12)) {
            chunk.checksum = reinterpret_cast<const char*>(sqlite3_column_text(m_stmtLoadChunk, 12));
        }
    }

    CheckSlowQuery("LoadChunk", GetTimeMs() - startTime);
    return chunk;
}

bool WorldDatabase::IsChunkGenerated(int chunkX, int chunkY, int chunkZ) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db || m_currentWorldId < 0 || !m_stmtIsChunkGenerated) return false;

    sqlite3_reset(m_stmtIsChunkGenerated);
    sqlite3_clear_bindings(m_stmtIsChunkGenerated);

    sqlite3_bind_int(m_stmtIsChunkGenerated, 1, m_currentWorldId);
    sqlite3_bind_int(m_stmtIsChunkGenerated, 2, chunkX);
    sqlite3_bind_int(m_stmtIsChunkGenerated, 3, chunkY);
    sqlite3_bind_int(m_stmtIsChunkGenerated, 4, chunkZ);

    bool isGenerated = false;
    if (sqlite3_step(m_stmtIsChunkGenerated) == SQLITE_ROW) {
        isGenerated = sqlite3_column_int(m_stmtIsChunkGenerated, 0) != 0;
    }

    return isGenerated;
}

bool WorldDatabase::DeleteChunk(int chunkX, int chunkY, int chunkZ) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db || m_currentWorldId < 0) return false;

    const char* sql = "DELETE FROM Chunks WHERE world_id = ? AND chunk_x = ? AND chunk_y = ? AND chunk_z = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, m_currentWorldId);
    sqlite3_bind_int(stmt, 2, chunkX);
    sqlite3_bind_int(stmt, 3, chunkY);
    sqlite3_bind_int(stmt, 4, chunkZ);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}

std::vector<glm::ivec3> WorldDatabase::GetChunksInRadius(glm::vec3 center, float radius) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    std::vector<glm::ivec3> chunks;
    if (!m_db || m_currentWorldId < 0) return chunks;

    // Convert world position to chunk coordinates
    const int chunkSize = 16; // Assuming 16x16x16 chunks
    int centerX = static_cast<int>(std::floor(center.x / chunkSize));
    int centerY = static_cast<int>(std::floor(center.y / chunkSize));
    int centerZ = static_cast<int>(std::floor(center.z / chunkSize));
    int radiusChunks = static_cast<int>(std::ceil(radius / chunkSize));

    const char* sql = R"(
        SELECT chunk_x, chunk_y, chunk_z FROM Chunks
        WHERE world_id = ? AND chunk_x BETWEEN ? AND ? AND chunk_y BETWEEN ? AND ? AND chunk_z BETWEEN ? AND ?
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return chunks;
    }

    sqlite3_bind_int(stmt, 1, m_currentWorldId);
    sqlite3_bind_int(stmt, 2, centerX - radiusChunks);
    sqlite3_bind_int(stmt, 3, centerX + radiusChunks);
    sqlite3_bind_int(stmt, 4, centerY - radiusChunks);
    sqlite3_bind_int(stmt, 5, centerY + radiusChunks);
    sqlite3_bind_int(stmt, 6, centerZ - radiusChunks);
    sqlite3_bind_int(stmt, 7, centerZ + radiusChunks);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        glm::ivec3 chunk(
            sqlite3_column_int(stmt, 0),
            sqlite3_column_int(stmt, 1),
            sqlite3_column_int(stmt, 2)
        );
        chunks.push_back(chunk);
    }

    sqlite3_finalize(stmt);
    return chunks;
}

std::vector<glm::ivec3> WorldDatabase::GetDirtyChunks() {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    std::vector<glm::ivec3> chunks;
    if (!m_db || m_currentWorldId < 0) return chunks;

    const char* sql = "SELECT chunk_x, chunk_y, chunk_z FROM Chunks WHERE world_id = ? AND is_dirty = 1";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return chunks;
    }

    sqlite3_bind_int(stmt, 1, m_currentWorldId);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        chunks.push_back(glm::ivec3(
            sqlite3_column_int(stmt, 0),
            sqlite3_column_int(stmt, 1),
            sqlite3_column_int(stmt, 2)
        ));
    }

    sqlite3_finalize(stmt);
    return chunks;
}

bool WorldDatabase::MarkChunkDirty(int chunkX, int chunkY, int chunkZ, bool dirty) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db || m_currentWorldId < 0) return false;

    const char* sql = "UPDATE Chunks SET is_dirty = ? WHERE world_id = ? AND chunk_x = ? AND chunk_y = ? AND chunk_z = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, dirty ? 1 : 0);
    sqlite3_bind_int(stmt, 2, m_currentWorldId);
    sqlite3_bind_int(stmt, 3, chunkX);
    sqlite3_bind_int(stmt, 4, chunkY);
    sqlite3_bind_int(stmt, 5, chunkZ);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}

// ============================================================================
// ENTITY OPERATIONS
// ============================================================================

int WorldDatabase::SaveEntity(const Entity& entity) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db || m_currentWorldId < 0 || !m_stmtSaveEntity) return -1;

    float startTime = GetTimeMs();

    sqlite3_reset(m_stmtSaveEntity);
    sqlite3_clear_bindings(m_stmtSaveEntity);

    int entityId = entity.entityId;
    if (entityId < 0) {
        sqlite3_bind_null(m_stmtSaveEntity, 1);
    } else {
        sqlite3_bind_int(m_stmtSaveEntity, 1, entityId);
    }

    sqlite3_bind_int(m_stmtSaveEntity, 2, m_currentWorldId);
    sqlite3_bind_text(m_stmtSaveEntity, 3, entity.entityType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_stmtSaveEntity, 4, entity.entitySubtype.c_str(), -1, SQLITE_TRANSIENT);

    std::string uuid = entity.uuid.empty() ? GenerateUUID() : entity.uuid;
    sqlite3_bind_text(m_stmtSaveEntity, 5, uuid.c_str(), -1, SQLITE_TRANSIENT);

    sqlite3_bind_int(m_stmtSaveEntity, 6, entity.chunkPos.x);
    sqlite3_bind_int(m_stmtSaveEntity, 7, entity.chunkPos.y);
    sqlite3_bind_int(m_stmtSaveEntity, 8, entity.chunkPos.z);
    sqlite3_bind_double(m_stmtSaveEntity, 9, entity.position.x);
    sqlite3_bind_double(m_stmtSaveEntity, 10, entity.position.y);
    sqlite3_bind_double(m_stmtSaveEntity, 11, entity.position.z);
    sqlite3_bind_double(m_stmtSaveEntity, 12, entity.rotation.x);
    sqlite3_bind_double(m_stmtSaveEntity, 13, entity.rotation.y);
    sqlite3_bind_double(m_stmtSaveEntity, 14, entity.rotation.z);
    sqlite3_bind_double(m_stmtSaveEntity, 15, entity.rotation.w);
    sqlite3_bind_double(m_stmtSaveEntity, 16, entity.velocity.x);
    sqlite3_bind_double(m_stmtSaveEntity, 17, entity.velocity.y);
    sqlite3_bind_double(m_stmtSaveEntity, 18, entity.velocity.z);
    sqlite3_bind_double(m_stmtSaveEntity, 19, entity.scale.x);
    sqlite3_bind_double(m_stmtSaveEntity, 20, entity.scale.y);
    sqlite3_bind_double(m_stmtSaveEntity, 21, entity.scale.z);
    sqlite3_bind_blob(m_stmtSaveEntity, 22, entity.data.data(), entity.data.size(), SQLITE_TRANSIENT);
    sqlite3_bind_int(m_stmtSaveEntity, 23, entity.isActive ? 1 : 0);
    sqlite3_bind_int(m_stmtSaveEntity, 24, entity.isStatic ? 1 : 0);

    if (entity.ownerPlayerId >= 0) {
        sqlite3_bind_int(m_stmtSaveEntity, 25, entity.ownerPlayerId);
    } else {
        sqlite3_bind_null(m_stmtSaveEntity, 25);
    }

    sqlite3_bind_double(m_stmtSaveEntity, 26, entity.health);
    sqlite3_bind_double(m_stmtSaveEntity, 27, entity.maxHealth);
    sqlite3_bind_int(m_stmtSaveEntity, 28, entity.flags);
    sqlite3_bind_int64(m_stmtSaveEntity, 29, GetTimestamp());

    int result = sqlite3_step(m_stmtSaveEntity);
    if (result != SQLITE_DONE) {
        LogError("Failed to save entity: " + std::string(sqlite3_errmsg(m_db)));
        return -1;
    }

    if (entityId < 0) {
        entityId = static_cast<int>(sqlite3_last_insert_rowid(m_db));
    }

    CheckSlowQuery("SaveEntity", GetTimeMs() - startTime);
    return entityId;
}

Entity WorldDatabase::LoadEntity(int entityId) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    Entity entity;
    if (!m_db || !m_stmtLoadEntity) return entity;

    float startTime = GetTimeMs();

    sqlite3_reset(m_stmtLoadEntity);
    sqlite3_clear_bindings(m_stmtLoadEntity);
    sqlite3_bind_int(m_stmtLoadEntity, 1, entityId);

    if (sqlite3_step(m_stmtLoadEntity) == SQLITE_ROW) {
        entity.entityId = sqlite3_column_int(m_stmtLoadEntity, 0);
        entity.worldId = sqlite3_column_int(m_stmtLoadEntity, 1);
        entity.entityType = reinterpret_cast<const char*>(sqlite3_column_text(m_stmtLoadEntity, 2));
        entity.entitySubtype = reinterpret_cast<const char*>(sqlite3_column_text(m_stmtLoadEntity, 3));
        entity.uuid = reinterpret_cast<const char*>(sqlite3_column_text(m_stmtLoadEntity, 4));
        entity.chunkPos.x = sqlite3_column_int(m_stmtLoadEntity, 5);
        entity.chunkPos.y = sqlite3_column_int(m_stmtLoadEntity, 6);
        entity.chunkPos.z = sqlite3_column_int(m_stmtLoadEntity, 7);
        entity.position.x = static_cast<float>(sqlite3_column_double(m_stmtLoadEntity, 8));
        entity.position.y = static_cast<float>(sqlite3_column_double(m_stmtLoadEntity, 9));
        entity.position.z = static_cast<float>(sqlite3_column_double(m_stmtLoadEntity, 10));
        entity.rotation.x = static_cast<float>(sqlite3_column_double(m_stmtLoadEntity, 11));
        entity.rotation.y = static_cast<float>(sqlite3_column_double(m_stmtLoadEntity, 12));
        entity.rotation.z = static_cast<float>(sqlite3_column_double(m_stmtLoadEntity, 13));
        entity.rotation.w = static_cast<float>(sqlite3_column_double(m_stmtLoadEntity, 14));
        entity.velocity.x = static_cast<float>(sqlite3_column_double(m_stmtLoadEntity, 15));
        entity.velocity.y = static_cast<float>(sqlite3_column_double(m_stmtLoadEntity, 16));
        entity.velocity.z = static_cast<float>(sqlite3_column_double(m_stmtLoadEntity, 17));
        entity.scale.x = static_cast<float>(sqlite3_column_double(m_stmtLoadEntity, 18));
        entity.scale.y = static_cast<float>(sqlite3_column_double(m_stmtLoadEntity, 19));
        entity.scale.z = static_cast<float>(sqlite3_column_double(m_stmtLoadEntity, 20));

        const void* dataBlob = sqlite3_column_blob(m_stmtLoadEntity, 21);
        int dataSize = sqlite3_column_bytes(m_stmtLoadEntity, 21);
        if (dataBlob && dataSize > 0) {
            entity.data.resize(dataSize);
            std::memcpy(entity.data.data(), dataBlob, dataSize);
        }

        entity.isActive = sqlite3_column_int(m_stmtLoadEntity, 22) != 0;
        entity.isStatic = sqlite3_column_int(m_stmtLoadEntity, 23) != 0;

        if (sqlite3_column_type(m_stmtLoadEntity, 24) != SQLITE_NULL) {
            entity.ownerPlayerId = sqlite3_column_int(m_stmtLoadEntity, 24);
        }

        entity.health = static_cast<float>(sqlite3_column_double(m_stmtLoadEntity, 25));
        entity.maxHealth = static_cast<float>(sqlite3_column_double(m_stmtLoadEntity, 26));
        entity.flags = sqlite3_column_int(m_stmtLoadEntity, 27);
        entity.createdAt = sqlite3_column_int64(m_stmtLoadEntity, 28);
        entity.modifiedAt = sqlite3_column_int64(m_stmtLoadEntity, 29);
    }

    CheckSlowQuery("LoadEntity", GetTimeMs() - startTime);
    return entity;
}

Entity WorldDatabase::LoadEntityByUUID(const std::string& uuid) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    Entity entity;
    if (!m_db || !m_stmtLoadEntityByUUID) return entity;

    sqlite3_reset(m_stmtLoadEntityByUUID);
    sqlite3_clear_bindings(m_stmtLoadEntityByUUID);
    sqlite3_bind_text(m_stmtLoadEntityByUUID, 1, uuid.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(m_stmtLoadEntityByUUID) == SQLITE_ROW) {
        // Same logic as LoadEntity...
        entity.entityId = sqlite3_column_int(m_stmtLoadEntityByUUID, 0);
        // ... (copy remaining fields similar to LoadEntity)
    }

    return entity;
}

bool WorldDatabase::DeleteEntity(int entityId) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db || !m_stmtDeleteEntity) return false;

    sqlite3_reset(m_stmtDeleteEntity);
    sqlite3_clear_bindings(m_stmtDeleteEntity);
    sqlite3_bind_int(m_stmtDeleteEntity, 1, entityId);

    int result = sqlite3_step(m_stmtDeleteEntity);
    return result == SQLITE_DONE;
}

std::vector<Entity> WorldDatabase::LoadEntitiesInChunk(int chunkX, int chunkY, int chunkZ) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    std::vector<Entity> entities;
    if (!m_db || m_currentWorldId < 0) return entities;

    const char* sql = "SELECT entity_id FROM Entities WHERE world_id = ? AND chunk_x = ? AND chunk_y = ? AND chunk_z = ? AND is_active = 1";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return entities;
    }

    sqlite3_bind_int(stmt, 1, m_currentWorldId);
    sqlite3_bind_int(stmt, 2, chunkX);
    sqlite3_bind_int(stmt, 3, chunkY);
    sqlite3_bind_int(stmt, 4, chunkZ);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int entityId = sqlite3_column_int(stmt, 0);
        m_dbMutex.unlock();
        entities.push_back(LoadEntity(entityId));
        m_dbMutex.lock();
    }

    sqlite3_finalize(stmt);
    return entities;
}

EntityQueryResult WorldDatabase::QueryEntitiesInRadius(glm::vec3 center, float radius) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    EntityQueryResult result;
    if (!m_db || m_currentWorldId < 0) return result;

    float startTime = GetTimeMs();

    // Use R-tree spatial index for fast queries
    const char* sql = R"(
        SELECT e.entity_id FROM EntitySpatialIndex si
        JOIN Entities e ON si.id = e.entity_id
        WHERE si.min_x <= ? AND si.max_x >= ? AND
              si.min_y <= ? AND si.max_y >= ? AND
              si.min_z <= ? AND si.max_z >= ? AND
              e.world_id = ? AND e.is_active = 1
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }

    sqlite3_bind_double(stmt, 1, center.x + radius);
    sqlite3_bind_double(stmt, 2, center.x - radius);
    sqlite3_bind_double(stmt, 3, center.y + radius);
    sqlite3_bind_double(stmt, 4, center.y - radius);
    sqlite3_bind_double(stmt, 5, center.z + radius);
    sqlite3_bind_double(stmt, 6, center.z - radius);
    sqlite3_bind_int(stmt, 7, m_currentWorldId);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int entityId = sqlite3_column_int(stmt, 0);
        m_dbMutex.unlock();
        Entity entity = LoadEntity(entityId);
        m_dbMutex.lock();

        // Additional distance check (R-tree gives bounding box hits)
        float distSq = glm::dot(entity.position - center, entity.position - center);
        if (distSq <= radius * radius) {
            result.entities.push_back(entity);
        }
    }

    sqlite3_finalize(stmt);
    result.totalCount = result.entities.size();
    result.queryTime = GetTimeMs() - startTime;

    CheckSlowQuery("QueryEntitiesInRadius", result.queryTime);
    return result;
}

std::vector<Entity> WorldDatabase::QueryEntitiesByType(const std::string& entityType) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    std::vector<Entity> entities;
    if (!m_db || m_currentWorldId < 0) return entities;

    const char* sql = "SELECT entity_id FROM Entities WHERE world_id = ? AND entity_type = ? AND is_active = 1";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return entities;
    }

    sqlite3_bind_int(stmt, 1, m_currentWorldId);
    sqlite3_bind_text(stmt, 2, entityType.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int entityId = sqlite3_column_int(stmt, 0);
        m_dbMutex.unlock();
        entities.push_back(LoadEntity(entityId));
        m_dbMutex.lock();
    }

    sqlite3_finalize(stmt);
    return entities;
}

size_t WorldDatabase::CountEntities(bool activeOnly) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db || m_currentWorldId < 0) return 0;

    std::string sql = "SELECT COUNT(*) FROM Entities WHERE world_id = " + std::to_string(m_currentWorldId);
    if (activeOnly) {
        sql += " AND is_active = 1";
    }

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    size_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int64(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return count;
}

// ============================================================================
// PLAYER OPERATIONS
// ============================================================================

int WorldDatabase::CreatePlayer(const std::string& username) {
    if (!m_db || m_currentWorldId < 0) return -1;

    // First create entity for the player
    Entity playerEntity;
    playerEntity.entityType = "player";
    playerEntity.uuid = GenerateUUID();
    playerEntity.position = glm::vec3(0.0f, 100.0f, 0.0f); // Default spawn

    int entityId = SaveEntity(playerEntity);
    if (entityId < 0) {
        return -1;
    }

    std::lock_guard<std::mutex> lock(m_dbMutex);

    const char* sql = R"(
        INSERT INTO Players (entity_id, username, created_at)
        VALUES (?, ?, ?)
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_int(stmt, 1, entityId);
    sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, GetTimestamp());

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
        return -1;
    }

    return static_cast<int>(sqlite3_last_insert_rowid(m_db));
}

Player WorldDatabase::LoadPlayer(const std::string& username) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    Player player;
    if (!m_db || !m_stmtLoadPlayer) return player;

    float startTime = GetTimeMs();

    sqlite3_reset(m_stmtLoadPlayer);
    sqlite3_clear_bindings(m_stmtLoadPlayer);
    sqlite3_bind_text(m_stmtLoadPlayer, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(m_stmtLoadPlayer) == SQLITE_ROW) {
        player.playerId = sqlite3_column_int(m_stmtLoadPlayer, 0);
        player.entityId = sqlite3_column_int(m_stmtLoadPlayer, 1);
        player.username = reinterpret_cast<const char*>(sqlite3_column_text(m_stmtLoadPlayer, 2));

        if (sqlite3_column_text(m_stmtLoadPlayer, 3)) {
            player.displayName = reinterpret_cast<const char*>(sqlite3_column_text(m_stmtLoadPlayer, 3));
        }
        if (sqlite3_column_text(m_stmtLoadPlayer, 4)) {
            player.passwordHash = reinterpret_cast<const char*>(sqlite3_column_text(m_stmtLoadPlayer, 4));
        }
        if (sqlite3_column_text(m_stmtLoadPlayer, 5)) {
            player.email = reinterpret_cast<const char*>(sqlite3_column_text(m_stmtLoadPlayer, 5));
        }

        player.level = sqlite3_column_int(m_stmtLoadPlayer, 6);
        player.experience = sqlite3_column_int(m_stmtLoadPlayer, 7);
        player.health = static_cast<float>(sqlite3_column_double(m_stmtLoadPlayer, 8));
        player.maxHealth = static_cast<float>(sqlite3_column_double(m_stmtLoadPlayer, 9));
        player.mana = static_cast<float>(sqlite3_column_double(m_stmtLoadPlayer, 10));
        player.maxMana = static_cast<float>(sqlite3_column_double(m_stmtLoadPlayer, 11));
        player.stamina = static_cast<float>(sqlite3_column_double(m_stmtLoadPlayer, 12));
        player.maxStamina = static_cast<float>(sqlite3_column_double(m_stmtLoadPlayer, 13));
        player.hunger = static_cast<float>(sqlite3_column_double(m_stmtLoadPlayer, 14));
        player.thirst = static_cast<float>(sqlite3_column_double(m_stmtLoadPlayer, 15));

        // Load blob data
        const void* statsBlob = sqlite3_column_blob(m_stmtLoadPlayer, 16);
        int statsSize = sqlite3_column_bytes(m_stmtLoadPlayer, 16);
        if (statsBlob && statsSize > 0) {
            player.stats.resize(statsSize);
            std::memcpy(player.stats.data(), statsBlob, statsSize);
        }

        const void* skillsBlob = sqlite3_column_blob(m_stmtLoadPlayer, 17);
        int skillsSize = sqlite3_column_bytes(m_stmtLoadPlayer, 17);
        if (skillsBlob && skillsSize > 0) {
            player.skills.resize(skillsSize);
            std::memcpy(player.skills.data(), skillsBlob, skillsSize);
        }

        const void* achieveBlob = sqlite3_column_blob(m_stmtLoadPlayer, 18);
        int achieveSize = sqlite3_column_bytes(m_stmtLoadPlayer, 18);
        if (achieveBlob && achieveSize > 0) {
            player.achievements.resize(achieveSize);
            std::memcpy(player.achievements.data(), achieveBlob, achieveSize);
        }

        player.deaths = sqlite3_column_int(m_stmtLoadPlayer, 19);
        player.kills = sqlite3_column_int(m_stmtLoadPlayer, 20);

        if (sqlite3_column_text(m_stmtLoadPlayer, 21)) {
            player.faction = reinterpret_cast<const char*>(sqlite3_column_text(m_stmtLoadPlayer, 21));
        }

        player.guildId = sqlite3_column_int(m_stmtLoadPlayer, 22);
        player.currencyGold = sqlite3_column_int(m_stmtLoadPlayer, 23);
        player.currencySilver = sqlite3_column_int(m_stmtLoadPlayer, 24);
        player.currencyPremium = sqlite3_column_int(m_stmtLoadPlayer, 25);
        player.gameMode = reinterpret_cast<const char*>(sqlite3_column_text(m_stmtLoadPlayer, 26));
        player.isOnline = sqlite3_column_int(m_stmtLoadPlayer, 27) != 0;
        player.isBanned = sqlite3_column_int(m_stmtLoadPlayer, 28) != 0;

        if (sqlite3_column_text(m_stmtLoadPlayer, 29)) {
            player.banReason = reinterpret_cast<const char*>(sqlite3_column_text(m_stmtLoadPlayer, 29));
        }

        player.createdAt = sqlite3_column_int64(m_stmtLoadPlayer, 30);
        player.lastLogin = sqlite3_column_int64(m_stmtLoadPlayer, 31);
        player.lastLogout = sqlite3_column_int64(m_stmtLoadPlayer, 32);
        player.playTimeSeconds = sqlite3_column_int64(m_stmtLoadPlayer, 33);
    }

    CheckSlowQuery("LoadPlayer", GetTimeMs() - startTime);
    return player;
}

Player WorldDatabase::LoadPlayerById(int playerId) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    Player player;
    if (!m_db) return player;

    const char* sql = "SELECT username FROM Players WHERE player_id = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return player;
    }

    sqlite3_bind_int(stmt, 1, playerId);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        sqlite3_finalize(stmt);
        m_dbMutex.unlock();
        return LoadPlayer(username);
    }

    sqlite3_finalize(stmt);
    return player;
}

bool WorldDatabase::SavePlayer(const Player& player) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db || !m_stmtSavePlayer) return false;

    float startTime = GetTimeMs();

    sqlite3_reset(m_stmtSavePlayer);
    sqlite3_clear_bindings(m_stmtSavePlayer);

    if (player.playerId >= 0) {
        sqlite3_bind_int(m_stmtSavePlayer, 1, player.playerId);
    } else {
        sqlite3_bind_null(m_stmtSavePlayer, 1);
    }

    sqlite3_bind_int(m_stmtSavePlayer, 2, player.entityId);
    sqlite3_bind_text(m_stmtSavePlayer, 3, player.username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_stmtSavePlayer, 4, player.displayName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_stmtSavePlayer, 5, player.passwordHash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_stmtSavePlayer, 6, player.email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(m_stmtSavePlayer, 7, player.level);
    sqlite3_bind_int(m_stmtSavePlayer, 8, player.experience);
    sqlite3_bind_double(m_stmtSavePlayer, 9, player.health);
    sqlite3_bind_double(m_stmtSavePlayer, 10, player.maxHealth);
    sqlite3_bind_double(m_stmtSavePlayer, 11, player.mana);
    sqlite3_bind_double(m_stmtSavePlayer, 12, player.maxMana);
    sqlite3_bind_double(m_stmtSavePlayer, 13, player.stamina);
    sqlite3_bind_double(m_stmtSavePlayer, 14, player.maxStamina);
    sqlite3_bind_double(m_stmtSavePlayer, 15, player.hunger);
    sqlite3_bind_double(m_stmtSavePlayer, 16, player.thirst);
    sqlite3_bind_blob(m_stmtSavePlayer, 17, player.stats.data(), player.stats.size(), SQLITE_TRANSIENT);
    sqlite3_bind_blob(m_stmtSavePlayer, 18, player.skills.data(), player.skills.size(), SQLITE_TRANSIENT);
    sqlite3_bind_blob(m_stmtSavePlayer, 19, player.achievements.data(), player.achievements.size(), SQLITE_TRANSIENT);
    sqlite3_bind_int(m_stmtSavePlayer, 20, player.deaths);
    sqlite3_bind_int(m_stmtSavePlayer, 21, player.kills);
    sqlite3_bind_text(m_stmtSavePlayer, 22, player.faction.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(m_stmtSavePlayer, 23, player.guildId);
    sqlite3_bind_int(m_stmtSavePlayer, 24, player.currencyGold);
    sqlite3_bind_int(m_stmtSavePlayer, 25, player.currencySilver);
    sqlite3_bind_int(m_stmtSavePlayer, 26, player.currencyPremium);
    sqlite3_bind_text(m_stmtSavePlayer, 27, player.gameMode.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(m_stmtSavePlayer, 28, player.isOnline ? 1 : 0);
    sqlite3_bind_int(m_stmtSavePlayer, 29, player.isBanned ? 1 : 0);
    sqlite3_bind_text(m_stmtSavePlayer, 30, player.banReason.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(m_stmtSavePlayer, 31, player.lastLogin);
    sqlite3_bind_int64(m_stmtSavePlayer, 32, player.lastLogout);
    sqlite3_bind_int64(m_stmtSavePlayer, 33, player.playTimeSeconds);

    int result = sqlite3_step(m_stmtSavePlayer);
    CheckSlowQuery("SavePlayer", GetTimeMs() - startTime);

    return result == SQLITE_DONE;
}

bool WorldDatabase::DeletePlayer(int playerId) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db) return false;

    const char* sql = "DELETE FROM Players WHERE player_id = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, playerId);
    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}

bool WorldDatabase::PlayerExists(const std::string& username) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db) return false;

    const char* sql = "SELECT 1 FROM Players WHERE username = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);

    return exists;
}

std::vector<Player> WorldDatabase::GetAllPlayers() {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    std::vector<Player> players;
    if (!m_db) return players;

    const char* sql = "SELECT username FROM Players";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return players;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        m_dbMutex.unlock();
        players.push_back(LoadPlayer(username));
        m_dbMutex.lock();
    }

    sqlite3_finalize(stmt);
    return players;
}

std::vector<Player> WorldDatabase::GetOnlinePlayers() {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    std::vector<Player> players;
    if (!m_db) return players;

    const char* sql = "SELECT username FROM Players WHERE is_online = 1";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return players;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        m_dbMutex.unlock();
        players.push_back(LoadPlayer(username));
        m_dbMutex.lock();
    }

    sqlite3_finalize(stmt);
    return players;
}

// ============================================================================
// INVENTORY OPERATIONS
// ============================================================================

bool WorldDatabase::SaveInventory(int playerId, const std::vector<InventorySlot>& inventory) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db || !m_stmtSaveInventorySlot) return false;

    float startTime = GetTimeMs();

    // Clear existing inventory
    const char* clearSQL = "DELETE FROM Inventory WHERE player_id = ?";
    sqlite3_stmt* clearStmt;
    if (sqlite3_prepare_v2(m_db, clearSQL, -1, &clearStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(clearStmt, 1, playerId);
        sqlite3_step(clearStmt);
        sqlite3_finalize(clearStmt);
    }

    // Insert new inventory
    for (const auto& slot : inventory) {
        sqlite3_reset(m_stmtSaveInventorySlot);
        sqlite3_clear_bindings(m_stmtSaveInventorySlot);

        sqlite3_bind_int(m_stmtSaveInventorySlot, 1, playerId);
        sqlite3_bind_int(m_stmtSaveInventorySlot, 2, slot.slotIndex);
        sqlite3_bind_text(m_stmtSaveInventorySlot, 3, slot.itemId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(m_stmtSaveInventorySlot, 4, slot.quantity);
        sqlite3_bind_double(m_stmtSaveInventorySlot, 5, slot.durability);
        sqlite3_bind_double(m_stmtSaveInventorySlot, 6, slot.maxDurability);
        sqlite3_bind_blob(m_stmtSaveInventorySlot, 7, slot.itemData.data(), slot.itemData.size(), SQLITE_TRANSIENT);
        sqlite3_bind_int(m_stmtSaveInventorySlot, 8, slot.isEquipped ? 1 : 0);
        sqlite3_bind_int(m_stmtSaveInventorySlot, 9, slot.isLocked ? 1 : 0);
        sqlite3_bind_int64(m_stmtSaveInventorySlot, 10, slot.acquiredAt);

        if (sqlite3_step(m_stmtSaveInventorySlot) != SQLITE_DONE) {
            LogError("Failed to save inventory slot");
            return false;
        }
    }

    CheckSlowQuery("SaveInventory", GetTimeMs() - startTime);
    return true;
}

std::vector<InventorySlot> WorldDatabase::LoadInventory(int playerId) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    std::vector<InventorySlot> inventory;
    if (!m_db || !m_stmtLoadInventory) return inventory;

    float startTime = GetTimeMs();

    sqlite3_reset(m_stmtLoadInventory);
    sqlite3_clear_bindings(m_stmtLoadInventory);
    sqlite3_bind_int(m_stmtLoadInventory, 1, playerId);

    while (sqlite3_step(m_stmtLoadInventory) == SQLITE_ROW) {
        InventorySlot slot;
        slot.slotIndex = sqlite3_column_int(m_stmtLoadInventory, 0);
        slot.itemId = reinterpret_cast<const char*>(sqlite3_column_text(m_stmtLoadInventory, 1));
        slot.quantity = sqlite3_column_int(m_stmtLoadInventory, 2);
        slot.durability = static_cast<float>(sqlite3_column_double(m_stmtLoadInventory, 3));
        slot.maxDurability = static_cast<float>(sqlite3_column_double(m_stmtLoadInventory, 4));

        const void* dataBlob = sqlite3_column_blob(m_stmtLoadInventory, 5);
        int dataSize = sqlite3_column_bytes(m_stmtLoadInventory, 5);
        if (dataBlob && dataSize > 0) {
            slot.itemData.resize(dataSize);
            std::memcpy(slot.itemData.data(), dataBlob, dataSize);
        }

        slot.isEquipped = sqlite3_column_int(m_stmtLoadInventory, 6) != 0;
        slot.isLocked = sqlite3_column_int(m_stmtLoadInventory, 7) != 0;
        slot.acquiredAt = sqlite3_column_int64(m_stmtLoadInventory, 8);

        inventory.push_back(slot);
    }

    CheckSlowQuery("LoadInventory", GetTimeMs() - startTime);
    return inventory;
}

bool WorldDatabase::ClearInventory(int playerId) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db) return false;

    const char* sql = "DELETE FROM Inventory WHERE player_id = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, playerId);
    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}

// ============================================================================
// EQUIPMENT OPERATIONS
// ============================================================================

bool WorldDatabase::SaveEquipment(int playerId, const std::map<std::string, EquipmentSlot>& equipment) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db || !m_stmtSaveEquipment) return false;

    // Clear existing equipment
    const char* clearSQL = "DELETE FROM Equipment WHERE player_id = ?";
    sqlite3_stmt* clearStmt;
    if (sqlite3_prepare_v2(m_db, clearSQL, -1, &clearStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(clearStmt, 1, playerId);
        sqlite3_step(clearStmt);
        sqlite3_finalize(clearStmt);
    }

    // Insert new equipment
    for (const auto& [slotName, slot] : equipment) {
        sqlite3_reset(m_stmtSaveEquipment);
        sqlite3_clear_bindings(m_stmtSaveEquipment);

        sqlite3_bind_int(m_stmtSaveEquipment, 1, playerId);
        sqlite3_bind_text(m_stmtSaveEquipment, 2, slotName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(m_stmtSaveEquipment, 3, slot.itemId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(m_stmtSaveEquipment, 4, slot.durability);
        sqlite3_bind_double(m_stmtSaveEquipment, 5, slot.maxDurability);
        sqlite3_bind_blob(m_stmtSaveEquipment, 6, slot.itemData.data(), slot.itemData.size(), SQLITE_TRANSIENT);
        sqlite3_bind_int64(m_stmtSaveEquipment, 7, slot.equippedAt);

        if (sqlite3_step(m_stmtSaveEquipment) != SQLITE_DONE) {
            return false;
        }
    }

    return true;
}

std::map<std::string, EquipmentSlot> WorldDatabase::LoadEquipment(int playerId) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    std::map<std::string, EquipmentSlot> equipment;
    if (!m_db || !m_stmtLoadEquipment) return equipment;

    sqlite3_reset(m_stmtLoadEquipment);
    sqlite3_clear_bindings(m_stmtLoadEquipment);
    sqlite3_bind_int(m_stmtLoadEquipment, 1, playerId);

    while (sqlite3_step(m_stmtLoadEquipment) == SQLITE_ROW) {
        EquipmentSlot slot;
        slot.slotName = reinterpret_cast<const char*>(sqlite3_column_text(m_stmtLoadEquipment, 0));
        slot.itemId = reinterpret_cast<const char*>(sqlite3_column_text(m_stmtLoadEquipment, 1));
        slot.durability = static_cast<float>(sqlite3_column_double(m_stmtLoadEquipment, 2));
        slot.maxDurability = static_cast<float>(sqlite3_column_double(m_stmtLoadEquipment, 3));

        const void* dataBlob = sqlite3_column_blob(m_stmtLoadEquipment, 4);
        int dataSize = sqlite3_column_bytes(m_stmtLoadEquipment, 4);
        if (dataBlob && dataSize > 0) {
            slot.itemData.resize(dataSize);
            std::memcpy(slot.itemData.data(), dataBlob, dataSize);
        }

        slot.equippedAt = sqlite3_column_int64(m_stmtLoadEquipment, 5);
        equipment[slot.slotName] = slot;
    }

    return equipment;
}

// ============================================================================
// BUILDING OPERATIONS
// ============================================================================

int WorldDatabase::SaveBuilding(const Building& building) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db) return -1;

    const char* sql = R"(
        INSERT OR REPLACE INTO Buildings (building_id, entity_id, owner_player_id, building_type, building_name,
                                         health, max_health, faction, construction_progress, is_constructing,
                                         construction_started, construction_completed, storage_data,
                                         production_queue, upgrade_level)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return -1;
    }

    if (building.buildingId >= 0) {
        sqlite3_bind_int(stmt, 1, building.buildingId);
    } else {
        sqlite3_bind_null(stmt, 1);
    }

    sqlite3_bind_int(stmt, 2, building.entityId);
    if (building.ownerPlayerId >= 0) {
        sqlite3_bind_int(stmt, 3, building.ownerPlayerId);
    } else {
        sqlite3_bind_null(stmt, 3);
    }
    sqlite3_bind_text(stmt, 4, building.buildingType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, building.buildingName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 6, building.health);
    sqlite3_bind_double(stmt, 7, building.maxHealth);
    sqlite3_bind_text(stmt, 8, building.faction.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 9, building.constructionProgress);
    sqlite3_bind_int(stmt, 10, building.isConstructing ? 1 : 0);
    sqlite3_bind_int64(stmt, 11, building.constructionStarted);
    sqlite3_bind_int64(stmt, 12, building.constructionCompleted);
    sqlite3_bind_blob(stmt, 13, building.storageData.data(), building.storageData.size(), SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 14, building.productionQueue.data(), building.productionQueue.size(), SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 15, building.upgradeLevel);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
        return -1;
    }

    if (building.buildingId < 0) {
        return static_cast<int>(sqlite3_last_insert_rowid(m_db));
    }
    return building.buildingId;
}

Building WorldDatabase::LoadBuilding(int buildingId) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    Building building;
    if (!m_db) return building;

    const char* sql = R"(
        SELECT building_id, entity_id, owner_player_id, building_type, building_name, health, max_health,
               faction, construction_progress, is_constructing, construction_started, construction_completed,
               storage_data, production_queue, upgrade_level
        FROM Buildings WHERE building_id = ?
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return building;
    }

    sqlite3_bind_int(stmt, 1, buildingId);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        building.buildingId = sqlite3_column_int(stmt, 0);
        building.entityId = sqlite3_column_int(stmt, 1);
        if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
            building.ownerPlayerId = sqlite3_column_int(stmt, 2);
        }
        building.buildingType = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        if (sqlite3_column_text(stmt, 4)) {
            building.buildingName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        }
        building.health = static_cast<float>(sqlite3_column_double(stmt, 5));
        building.maxHealth = static_cast<float>(sqlite3_column_double(stmt, 6));
        if (sqlite3_column_text(stmt, 7)) {
            building.faction = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        }
        building.constructionProgress = static_cast<float>(sqlite3_column_double(stmt, 8));
        building.isConstructing = sqlite3_column_int(stmt, 9) != 0;
        building.constructionStarted = sqlite3_column_int64(stmt, 10);
        building.constructionCompleted = sqlite3_column_int64(stmt, 11);

        const void* storageBlob = sqlite3_column_blob(stmt, 12);
        int storageSize = sqlite3_column_bytes(stmt, 12);
        if (storageBlob && storageSize > 0) {
            building.storageData.resize(storageSize);
            std::memcpy(building.storageData.data(), storageBlob, storageSize);
        }

        const void* queueBlob = sqlite3_column_blob(stmt, 13);
        int queueSize = sqlite3_column_bytes(stmt, 13);
        if (queueBlob && queueSize > 0) {
            building.productionQueue.resize(queueSize);
            std::memcpy(building.productionQueue.data(), queueBlob, queueSize);
        }

        building.upgradeLevel = sqlite3_column_int(stmt, 14);
    }

    sqlite3_finalize(stmt);
    return building;
}

std::vector<Building> WorldDatabase::GetPlayerBuildings(int playerId) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    std::vector<Building> buildings;
    if (!m_db) return buildings;

    const char* sql = "SELECT building_id FROM Buildings WHERE owner_player_id = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return buildings;
    }

    sqlite3_bind_int(stmt, 1, playerId);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int buildingId = sqlite3_column_int(stmt, 0);
        m_dbMutex.unlock();
        buildings.push_back(LoadBuilding(buildingId));
        m_dbMutex.lock();
    }

    sqlite3_finalize(stmt);
    return buildings;
}

// ============================================================================
// TRANSACTION SUPPORT
// ============================================================================

bool WorldDatabase::BeginTransaction() {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db || m_inTransaction) return false;

    bool success = ExecuteSQL("BEGIN TRANSACTION");
    if (success) {
        m_inTransaction = true;
    }
    return success;
}

bool WorldDatabase::Commit() {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db || !m_inTransaction) return false;

    bool success = ExecuteSQL("COMMIT");
    if (success) {
        m_inTransaction = false;
    }
    return success;
}

bool WorldDatabase::Rollback() {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db || !m_inTransaction) return false;

    bool success = ExecuteSQL("ROLLBACK");
    if (success) {
        m_inTransaction = false;
    }
    return success;
}

void WorldDatabase::BeginBatch() {
    if (m_batchDepth == 0) {
        BeginTransaction();
    }
    m_batchDepth++;
}

void WorldDatabase::EndBatch() {
    if (m_batchDepth > 0) {
        m_batchDepth--;
        if (m_batchDepth == 0) {
            Commit();
        }
    }
}

// ============================================================================
// MAINTENANCE
// ============================================================================

bool WorldDatabase::VacuumDatabase() {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db) return false;
    return ExecuteSQL("VACUUM");
}

bool WorldDatabase::AnalyzeDatabase() {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db) return false;
    return ExecuteSQL("ANALYZE");
}

bool WorldDatabase::CheckIntegrity() {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db) return false;

    const char* sql = "PRAGMA integrity_check";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    bool ok = true;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        ok = (std::string(result) == "ok");
    }

    sqlite3_finalize(stmt);
    return ok;
}

bool WorldDatabase::CreateBackup(const std::string& backupPath) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db) return false;

    sqlite3* backupDb;
    int result = sqlite3_open(backupPath.c_str(), &backupDb);
    if (result != SQLITE_OK) {
        return false;
    }

    sqlite3_backup* backup = sqlite3_backup_init(backupDb, "main", m_db, "main");
    if (backup) {
        sqlite3_backup_step(backup, -1);
        sqlite3_backup_finish(backup);
    }

    result = sqlite3_errcode(backupDb);
    sqlite3_close(backupDb);

    return result == SQLITE_OK;
}

bool WorldDatabase::RestoreFromBackup(const std::string& backupPath) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db) return false;

    sqlite3* backupDb;
    int result = sqlite3_open(backupPath.c_str(), &backupDb);
    if (result != SQLITE_OK) {
        return false;
    }

    sqlite3_backup* backup = sqlite3_backup_init(m_db, "main", backupDb, "main");
    if (backup) {
        sqlite3_backup_step(backup, -1);
        sqlite3_backup_finish(backup);
    }

    result = sqlite3_errcode(m_db);
    sqlite3_close(backupDb);

    return result == SQLITE_OK;
}

DatabaseStats WorldDatabase::GetStatistics() {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    DatabaseStats stats;
    if (!m_db || m_currentWorldId < 0) return stats;

    // Get chunk counts
    const char* chunkSQL = "SELECT COUNT(*), SUM(CASE WHEN is_generated = 1 THEN 1 ELSE 0 END), SUM(CASE WHEN is_dirty = 1 THEN 1 ELSE 0 END) FROM Chunks WHERE world_id = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, chunkSQL, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, m_currentWorldId);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats.totalChunks = sqlite3_column_int64(stmt, 0);
            stats.generatedChunks = sqlite3_column_int64(stmt, 1);
            stats.dirtyChunks = sqlite3_column_int64(stmt, 2);
        }
        sqlite3_finalize(stmt);
    }

    // Get entity counts
    const char* entitySQL = "SELECT COUNT(*), SUM(CASE WHEN is_active = 1 THEN 1 ELSE 0 END) FROM Entities WHERE world_id = ?";
    if (sqlite3_prepare_v2(m_db, entitySQL, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, m_currentWorldId);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats.totalEntities = sqlite3_column_int64(stmt, 0);
            stats.activeEntities = sqlite3_column_int64(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }

    // Get player counts
    const char* playerSQL = "SELECT COUNT(*), SUM(CASE WHEN is_online = 1 THEN 1 ELSE 0 END) FROM Players";
    if (sqlite3_prepare_v2(m_db, playerSQL, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats.totalPlayers = sqlite3_column_int64(stmt, 0);
            stats.onlinePlayers = sqlite3_column_int64(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }

    stats.databaseSizeBytes = GetDatabaseSize();
    if (m_totalQueries > 0) {
        stats.avgQueryTime = m_totalQueryTime / static_cast<float>(m_totalQueries);
    }

    return stats;
}

size_t WorldDatabase::GetDatabaseSize() {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db) return 0;

    size_t pageCount = 0;
    size_t pageSize = 0;

    const char* pageSizeSQL = "PRAGMA page_size";
    const char* pageCountSQL = "PRAGMA page_count";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, pageSizeSQL, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            pageSize = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (sqlite3_prepare_v2(m_db, pageCountSQL, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            pageCount = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    return pageCount * pageSize;
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

bool WorldDatabase::ExecuteSQL(const std::string& sql) {
    if (!m_db) return false;

    char* errMsg = nullptr;
    int result = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &errMsg);

    if (result != SQLITE_OK) {
        if (errMsg) {
            LogError("SQL error: " + std::string(errMsg));
            sqlite3_free(errMsg);
        }
        return false;
    }

    return true;
}

int64_t WorldDatabase::GetLastInsertRowId() {
    return m_db ? sqlite3_last_insert_rowid(m_db) : -1;
}

uint64_t WorldDatabase::GetCurrentTimestamp() {
    return GetTimestamp();
}

std::string WorldDatabase::GenerateUUID() {
    return ::Nova::GenerateUUID();
}

void WorldDatabase::LogError(const std::string& message) {
    if (OnError) {
        OnError(message);
    }
}

void WorldDatabase::CheckSlowQuery(const std::string& operation, float timeMs) {
    m_totalQueryTime += timeMs;
    m_totalQueries++;

    if (timeMs > m_slowQueryThreshold && OnSlowQuery) {
        OnSlowQuery(operation, timeMs);
    }
}

} // namespace Nova
