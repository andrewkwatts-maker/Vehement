#pragma once

#include "IPersistenceBackend.hpp"
#include <sqlite3.h>
#include <mutex>
#include <queue>
#include <memory>

namespace Nova {

/**
 * @brief SQLite-based persistence backend
 *
 * Features:
 * - Local database storage
 * - ACID transactions
 * - Full-text search support
 * - Automatic schema migration
 * - Change tracking for sync
 * - Asset versioning
 */
class SQLiteBackend : public IPersistenceBackend {
public:
    struct Config {
        std::string databasePath = "editor_data.db";
        bool enableWAL = true;              // Write-Ahead Logging for better concurrency
        bool enableFTS = true;              // Full-Text Search
        int cacheSize = 10000;              // Page cache size (in pages)
        int busyTimeout = 5000;             // Timeout for locked database (ms)
        bool autoVacuum = true;             // Automatically reclaim space
        bool foreignKeys = true;            // Enable foreign key constraints
        int maxVersionsPerAsset = 10;       // Max versions to keep (0 = unlimited)
    };

    SQLiteBackend();
    ~SQLiteBackend() override;

    // IPersistenceBackend implementation
    bool Initialize(const nlohmann::json& config) override;
    void Shutdown() override;
    void Update(float deltaTime) override;

    bool SaveAsset(const std::string& id, const nlohmann::json& data,
                  const AssetMetadata* metadata = nullptr) override;
    nlohmann::json LoadAsset(const std::string& id) override;
    bool DeleteAsset(const std::string& id) override;
    bool AssetExists(const std::string& id) override;
    std::vector<std::string> ListAssets(const AssetFilter& filter = {}) override;
    AssetMetadata GetMetadata(const std::string& id) override;

    nlohmann::json GetAssetVersion(const std::string& id, int version) override;
    std::vector<int> GetAssetVersions(const std::string& id) override;
    bool RevertToVersion(const std::string& id, int version) override;

    std::vector<ChangeEntry> GetChangeHistory(const std::string& id, size_t limit = 0) override;
    std::vector<ChangeEntry> GetUnsyncedChanges() override;
    bool MarkChangesSynced(const std::vector<uint64_t>& changeIds) override;

    bool IsOnline() const override { return false; }  // SQLite is always "offline" (local)
    void Sync(std::function<void(bool, const std::string&)> callback = nullptr) override;
    SyncStatus GetSyncStatus() const override;

    bool BeginTransaction() override;
    bool CommitTransaction() override;
    bool RollbackTransaction() override;

    bool HasConflicts(const std::string& id) override { return false; }  // No remote conflicts
    nlohmann::json GetConflictData(const std::string& id) override;
    bool ResolveConflict(const std::string& id, bool useLocal) override { return true; }

    // SQLite-specific operations

    /**
     * @brief Execute raw SQL query
     * @param sql SQL query
     * @param params Query parameters
     * @return Result as JSON array
     */
    nlohmann::json ExecuteQuery(const std::string& sql,
                               const std::vector<std::string>& params = {});

    /**
     * @brief Execute SQL without expecting results
     * @param sql SQL statement
     * @param params Statement parameters
     * @return True if execution succeeded
     */
    bool ExecuteStatement(const std::string& sql,
                         const std::vector<std::string>& params = {});

    /**
     * @brief Vacuum database to reclaim space
     */
    void Vacuum();

    /**
     * @brief Get database size in bytes
     */
    uint64_t GetDatabaseSize() const;

    /**
     * @brief Export database to file
     */
    bool ExportToFile(const std::string& filepath);

    /**
     * @brief Import database from file
     */
    bool ImportFromFile(const std::string& filepath);

private:
    // Database initialization
    bool OpenDatabase();
    void CloseDatabase();
    bool CreateSchema();
    bool MigrateSchema(int currentVersion);

    // Helper functions
    std::string CalculateChecksum(const nlohmann::json& data) const;
    uint64_t GetCurrentTimestamp() const;
    std::string SerializeJson(const nlohmann::json& data) const;
    nlohmann::json DeserializeJson(const std::string& data) const;

    // Asset versioning
    bool SaveAssetVersion(const std::string& id, const nlohmann::json& data,
                         const AssetMetadata& metadata);
    void PruneOldVersions(const std::string& id);

    // Change tracking
    bool RecordChange(const std::string& assetId, ChangeEntry::Type type,
                     const nlohmann::json& oldData, const nlohmann::json& newData);

    // Statement preparation helpers
    sqlite3_stmt* PrepareStatement(const std::string& sql);
    void FinalizeStatement(sqlite3_stmt* stmt);
    bool BindParameters(sqlite3_stmt* stmt, const std::vector<std::string>& params);

    Config m_config;
    sqlite3* m_db = nullptr;
    bool m_initialized = false;
    int m_transactionDepth = 0;
    mutable std::mutex m_dbMutex;

    // Cached prepared statements (for performance)
    std::unordered_map<std::string, sqlite3_stmt*> m_preparedStatements;

    SyncStatus m_syncStatus;
};

} // namespace Nova
