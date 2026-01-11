#pragma once

#include "ServerConfig.hpp"
#include "../engine/persistence/WorldDatabase.hpp"
#include "../engine/persistence/PlayerDatabase.hpp"
#include "../engine/persistence/ChunkStreamer.hpp"
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <functional>

namespace Nova {

/**
 * @brief Server statistics
 */
struct ServerStats {
    uint64_t uptime = 0;          // Seconds
    size_t totalPlayers = 0;
    size_t onlinePlayers = 0;
    size_t loadedChunks = 0;
    size_t activeEntities = 0;
    float avgTickTime = 0.0f;     // Milliseconds
    float avgFrameTime = 0.0f;    // Milliseconds
    size_t memoryUsage = 0;       // Bytes
    size_t databaseSize = 0;      // Bytes
    size_t networkBytesSent = 0;
    size_t networkBytesReceived = 0;
};

/**
 * @brief Server status
 */
enum class ServerStatus {
    Stopped,
    Starting,
    Running,
    Stopping,
    Error
};

/**
 * @brief Server manager - Main server coordinator
 *
 * Manages all server subsystems including database, player management,
 * chunk streaming, game logic, and networking.
 *
 * Features:
 * - World persistence
 * - Player management
 * - Chunk streaming
 * - Auto-save and backup
 * - Performance monitoring
 * - Admin commands
 */
class ServerManager {
public:
    ServerManager();
    ~ServerManager();

    // Prevent copying
    ServerManager(const ServerManager&) = delete;
    ServerManager& operator=(const ServerManager&) = delete;

    // =========================================================================
    // INITIALIZATION
    // =========================================================================

    /**
     * @brief Initialize server
     * @param configPath Path to config file
     * @param worldPath Path to world database
     * @return True if initialization succeeded
     */
    bool Initialize(const std::string& configPath, const std::string& worldPath);

    /**
     * @brief Shutdown server gracefully
     */
    void Shutdown();

    /**
     * @brief Check if server is initialized
     */
    bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // SERVER CONTROL
    // =========================================================================

    /**
     * @brief Start server
     * @return True if start succeeded
     */
    bool Start();

    /**
     * @brief Stop server
     */
    void Stop();

    /**
     * @brief Restart server
     */
    bool Restart();

    /**
     * @brief Get server status
     */
    ServerStatus GetStatus() const { return m_status; }

    /**
     * @brief Check if server is running
     */
    bool IsRunning() const { return m_status == ServerStatus::Running; }

    // =========================================================================
    // UPDATE
    // =========================================================================

    /**
     * @brief Update server (call from main loop)
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    /**
     * @brief Get tick rate (updates per second)
     */
    int GetTickRate() const;

    /**
     * @brief Set tick rate
     */
    void SetTickRate(int ticksPerSecond);

    // =========================================================================
    // PLAYER MANAGEMENT
    // =========================================================================

    /**
     * @brief Player joins server
     * @param username Username
     * @param password Password
     * @return Player ID, or -1 on failure
     */
    int PlayerJoin(const std::string& username, const std::string& password);

    /**
     * @brief Player leaves server
     * @param playerId Player ID
     */
    void PlayerLeave(int playerId);

    /**
     * @brief Get online player count
     */
    size_t GetOnlinePlayerCount() const;

    /**
     * @brief Get online players
     */
    std::vector<Player> GetOnlinePlayers() const;

    /**
     * @brief Kick player
     */
    bool KickPlayer(int playerId, const std::string& reason);

    /**
     * @brief Ban player
     */
    bool BanPlayer(const std::string& username, const std::string& reason);

    /**
     * @brief Unban player
     */
    bool UnbanPlayer(const std::string& username);

    // =========================================================================
    // WORLD MANAGEMENT
    // =========================================================================

    /**
     * @brief Create new world
     * @param worldName World name
     * @param seed World seed
     * @return True if creation succeeded
     */
    bool CreateWorld(const std::string& worldName, int seed);

    /**
     * @brief Load world
     * @param worldName World name
     * @return True if load succeeded
     */
    bool LoadWorld(const std::string& worldName);

    /**
     * @brief Save world
     */
    void SaveWorld();

    /**
     * @brief Get loaded world name
     */
    std::string GetWorldName() const { return m_worldName; }

    // =========================================================================
    // SAVE/BACKUP
    // =========================================================================

    /**
     * @brief Trigger immediate save
     */
    void TriggerSave();

    /**
     * @brief Create backup
     */
    bool CreateBackup();

    /**
     * @brief Restore from backup
     */
    bool RestoreBackup(const std::string& backupPath);

    // =========================================================================
    // CONFIGURATION
    // =========================================================================

    /**
     * @brief Get server config
     */
    ServerConfig& GetConfig() { return m_config; }

    /**
     * @brief Get server config (const)
     */
    const ServerConfig& GetConfig() const { return m_config; }

    /**
     * @brief Reload configuration
     */
    bool ReloadConfig();

    /**
     * @brief Save configuration
     */
    bool SaveConfig();

    // =========================================================================
    // STATISTICS
    // =========================================================================

    /**
     * @brief Get server statistics
     */
    ServerStats GetStatistics() const;

    /**
     * @brief Get database statistics
     */
    DatabaseStats GetDatabaseStats() const;

    /**
     * @brief Get chunk streaming statistics
     */
    ChunkStreamStats GetChunkStreamStats() const;

    // =========================================================================
    // SUBSYSTEM ACCESS
    // =========================================================================

    /**
     * @brief Get world database
     */
    WorldDatabase* GetWorldDatabase() { return m_worldDatabase.get(); }

    /**
     * @brief Get player database
     */
    PlayerDatabase* GetPlayerDatabase() { return m_playerDatabase.get(); }

    /**
     * @brief Get chunk streamer
     */
    ChunkStreamer* GetChunkStreamer() { return m_chunkStreamer.get(); }

    // =========================================================================
    // CALLBACKS
    // =========================================================================

    std::function<void(int playerId, const std::string& username)> OnPlayerJoin;
    std::function<void(int playerId, const std::string& username)> OnPlayerLeave;
    std::function<void(const std::string& message)> OnServerMessage;
    std::function<void(const std::string& error)> OnError;

private:
    // Server loop (if running in separate thread)
    void ServerLoop();

    // Auto-save logic
    void ProcessAutoSave(float deltaTime);

    // Backup logic
    void ProcessBackup(float deltaTime);

    // Performance monitoring
    void UpdatePerformanceStats(float deltaTime);

    // Helper functions
    void LogMessage(const std::string& message);
    void LogError(const std::string& error);
    std::string GenerateBackupPath();

    ServerConfig m_config;
    ServerStatus m_status = ServerStatus::Stopped;
    bool m_initialized = false;

    // Core subsystems
    std::unique_ptr<WorldDatabase> m_worldDatabase;
    std::unique_ptr<PlayerDatabase> m_playerDatabase;
    std::unique_ptr<ChunkStreamer> m_chunkStreamer;

    // Server state
    std::string m_worldName;
    std::string m_configPath;
    std::string m_worldPath;

    // Server thread (optional)
    std::unique_ptr<std::thread> m_serverThread;
    std::atomic<bool> m_running{false};

    // Timing
    uint64_t m_startTime = 0;
    float m_autoSaveTimer = 0.0f;
    float m_backupTimer = 0.0f;
    int m_tickRate = 20;
    float m_tickInterval = 0.05f; // 1.0 / tickRate

    // Statistics
    mutable std::mutex m_statsMutex;
    ServerStats m_stats;
    float m_totalTickTime = 0.0f;
    size_t m_totalTicks = 0;
};

} // namespace Nova
