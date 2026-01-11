#pragma once

#include <string>
#include <vector>
#include <map>

namespace Nova {

/**
 * @brief Server configuration settings
 *
 * Stores all server settings including game rules, performance tuning,
 * and administrative settings.
 */
class ServerConfig {
public:
    ServerConfig() = default;
    ~ServerConfig() = default;

    // =========================================================================
    // SERVER IDENTITY
    // =========================================================================

    std::string serverName = "Nova Server";
    std::string serverDescription = "A Nova Engine Server";
    std::string serverVersion = "1.0.0";
    int maxPlayers = 32;
    bool passwordProtected = false;
    std::string passwordHash;

    // =========================================================================
    // GAME RULES
    // =========================================================================

    bool pvpEnabled = true;
    bool pveEnabled = true;
    bool friendlyFire = false;
    std::string difficulty = "normal"; // easy, normal, hard, nightmare
    bool hardcoreMode = false; // Permadeath
    std::string gameMode = "survival"; // survival, creative, adventure

    // =========================================================================
    // WORLD SETTINGS
    // =========================================================================

    int maxBuildHeight = 256;
    float spawnProtectionRadius = 50.0f;
    float dayNightCycleSpeed = 1.0f;
    bool weatherEnabled = true;
    bool naturalRegeneration = true;

    // =========================================================================
    // MOB SETTINGS
    // =========================================================================

    bool mobSpawning = true;
    bool mobGriefing = true;
    float mobSpawnRate = 1.0f;

    // =========================================================================
    // DAMAGE SETTINGS
    // =========================================================================

    bool fallDamage = true;
    bool fireDamage = true;
    bool drowningDamage = true;
    float damageMultiplier = 1.0f;

    // =========================================================================
    // SAVE/BACKUP SETTINGS
    // =========================================================================

    bool autoSaveEnabled = true;
    int autoSaveInterval = 300; // Seconds
    bool backupEnabled = true;
    int backupInterval = 3600; // Seconds
    int backupRetentionCount = 10;

    // =========================================================================
    // PERFORMANCE SETTINGS
    // =========================================================================

    int maxChunksPerTick = 10;
    int maxEntitiesPerChunk = 100;
    int maxPlayersPerChunk = 10;
    int tickRate = 20; // Ticks per second
    int chunkLoadThreads = 2;

    // =========================================================================
    // NETWORK SETTINGS
    // =========================================================================

    int serverPort = 25565;
    std::string serverIP = "0.0.0.0";
    int maxConnectionsPerIP = 3;
    int networkThreads = 4;
    int maxPacketSize = 1024 * 1024; // 1 MB

    // =========================================================================
    // ACCESS CONTROL
    // =========================================================================

    bool whitelistEnabled = false;
    std::vector<std::string> whitelist;
    std::vector<std::string> blacklist;
    std::map<std::string, int> admins; // username -> permission level

    // =========================================================================
    // CUSTOM SETTINGS
    // =========================================================================

    std::map<std::string, std::string> customSettings;

    // =========================================================================
    // METHODS
    // =========================================================================

    /**
     * @brief Load config from database
     */
    bool LoadFromDatabase(class WorldDatabase* db);

    /**
     * @brief Save config to database
     */
    bool SaveToDatabase(class WorldDatabase* db);

    /**
     * @brief Load config from JSON file
     */
    bool LoadFromFile(const std::string& path);

    /**
     * @brief Save config to JSON file
     */
    bool SaveToFile(const std::string& path);

    /**
     * @brief Reset to defaults
     */
    void ResetToDefaults();

    /**
     * @brief Validate configuration
     */
    bool Validate(std::string& errorMessage);

    /**
     * @brief Get custom setting
     */
    std::string GetCustomSetting(const std::string& key, const std::string& defaultValue = "") const;

    /**
     * @brief Set custom setting
     */
    void SetCustomSetting(const std::string& key, const std::string& value);

    /**
     * @brief Check if user is admin
     */
    bool IsAdmin(const std::string& username) const;

    /**
     * @brief Get admin permission level
     */
    int GetAdminLevel(const std::string& username) const;

    /**
     * @brief Add admin
     */
    void AddAdmin(const std::string& username, int permissionLevel = 1);

    /**
     * @brief Remove admin
     */
    void RemoveAdmin(const std::string& username);

    /**
     * @brief Check if user is whitelisted
     */
    bool IsWhitelisted(const std::string& username) const;

    /**
     * @brief Check if user is blacklisted
     */
    bool IsBlacklisted(const std::string& username) const;

    /**
     * @brief Add to whitelist
     */
    void AddToWhitelist(const std::string& username);

    /**
     * @brief Remove from whitelist
     */
    void RemoveFromWhitelist(const std::string& username);

    /**
     * @brief Add to blacklist
     */
    void AddToBlacklist(const std::string& username);

    /**
     * @brief Remove from blacklist
     */
    void RemoveFromBlacklist(const std::string& username);
};

} // namespace Nova
