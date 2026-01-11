#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <chrono>

namespace Vehement {

class Editor;

/**
 * @brief Hot-reload manager for asset and config files
 *
 * Features:
 * - Watch directories for file changes
 * - Automatic reload when files are modified externally
 * - Manual reload triggers
 * - Change notifications
 * - Debouncing to avoid excessive reloads
 */
class HotReloadManager {
public:
    enum class ChangeType {
        Created,
        Modified,
        Deleted
    };

    struct FileChange {
        std::string path;
        ChangeType changeType;
        std::chrono::system_clock::time_point timestamp;
    };

    explicit HotReloadManager(Editor* editor);
    ~HotReloadManager();

    void Update(float deltaTime);
    void Render();  // Optional UI panel for hot-reload status

    // Directory watching
    void AddWatchDirectory(const std::string& path, bool recursive = true);
    void RemoveWatchDirectory(const std::string& path);
    void ClearWatchDirectories();

    // Hot-reload control
    void EnableHotReload(bool enable) { m_enabled = enable; }
    bool IsHotReloadEnabled() const { return m_enabled; }

    void SetPollInterval(float seconds) { m_pollInterval = seconds; }
    float GetPollInterval() const { return m_pollInterval; }

    void SetDebounceDelay(float seconds) { m_debounceDelay = seconds; }
    float GetDebounceDelay() const { return m_debounceDelay; }

    // Manual reload triggers
    void ReloadFile(const std::string& path);
    void ReloadAllConfigs();
    void ReloadAllAssets();

    // Change history
    [[nodiscard]] const std::vector<FileChange>& GetRecentChanges() const { return m_recentChanges; }
    void ClearChangeHistory();

    // Callbacks
    std::function<void(const std::string&, ChangeType)> OnFileChanged;
    std::function<void(const std::string&)> OnConfigReloaded;
    std::function<void(const std::string&)> OnAssetReloaded;

private:
    struct WatchEntry {
        std::string path;
        bool recursive;
        int64_t lastModified;
        std::unordered_map<std::string, int64_t> fileTimestamps;
    };

    void ScanForChanges();
    void ProcessChange(const FileChange& change);
    bool ShouldProcessChange(const FileChange& change);
    int64_t GetFileTimestamp(const std::string& path);

    void ReloadConfigFile(const std::string& path);
    void ReloadAssetFile(const std::string& path);

    Editor* m_editor = nullptr;

    // Watch state
    std::vector<WatchEntry> m_watchDirs;
    bool m_enabled = true;
    float m_pollInterval = 1.0f;  // Check every second
    float m_pollTimer = 0.0f;

    // Debouncing
    float m_debounceDelay = 0.5f;  // Wait 0.5s before reloading
    std::unordered_map<std::string, std::chrono::system_clock::time_point> m_pendingChanges;

    // Change history
    std::vector<FileChange> m_recentChanges;
    size_t m_maxChangeHistory = 50;

    // Stats
    size_t m_totalReloads = 0;
    size_t m_failedReloads = 0;
};

} // namespace Vehement
