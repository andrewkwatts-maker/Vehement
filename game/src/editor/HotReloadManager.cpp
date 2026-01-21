#include "HotReloadManager.hpp"
#include "Editor.hpp"
#include "../config/ConfigRegistry.hpp"
#include "engine/assets/AssetDatabase.hpp"
#include <imgui.h>
#include <filesystem>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

namespace Vehement {

HotReloadManager::HotReloadManager(Editor* editor)
    : m_editor(editor) {
    // Setup default watch directories
    AddWatchDirectory("game/assets/configs", true);
    AddWatchDirectory("game/assets/scripts", true);
}

HotReloadManager::~HotReloadManager() = default;

void HotReloadManager::Update(float deltaTime) {
    if (!m_enabled) return;

    m_pollTimer += deltaTime;
    if (m_pollTimer >= m_pollInterval) {
        m_pollTimer = 0.0f;
        ScanForChanges();
    }

    // Process debounced changes
    auto now = std::chrono::system_clock::now();
    std::vector<std::string> toProcess;

    for (const auto& [path, timestamp] : m_pendingChanges) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - timestamp).count();
        if (elapsed >= m_debounceDelay * 1000.0f) {
            toProcess.push_back(path);
        }
    }

    for (const auto& path : toProcess) {
        FileChange change;
        change.path = path;
        change.changeType = ChangeType::Modified;
        change.timestamp = now;

        ProcessChange(change);
        m_pendingChanges.erase(path);
    }
}

void HotReloadManager::Render() {
    if (!ImGui::Begin("Hot Reload")) {
        ImGui::End();
        return;
    }

    ImGui::Text("Hot-Reload Manager");
    ImGui::Separator();

    // Control panel
    if (ImGui::Checkbox("Enable Hot-Reload", &m_enabled)) {
        if (m_enabled) {
            spdlog::info("Hot-reload enabled");
        } else {
            spdlog::info("Hot-reload disabled");
        }
    }

    ImGui::SliderFloat("Poll Interval (s)", &m_pollInterval, 0.1f, 5.0f);
    ImGui::SliderFloat("Debounce Delay (s)", &m_debounceDelay, 0.0f, 2.0f);

    ImGui::Separator();

    // Manual reload buttons
    if (ImGui::Button("Reload All Configs")) {
        ReloadAllConfigs();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload All Assets")) {
        ReloadAllAssets();
    }

    ImGui::Separator();

    // Stats
    ImGui::Text("Total Reloads: %zu", m_totalReloads);
    ImGui::Text("Failed Reloads: %zu", m_failedReloads);
    ImGui::Text("Pending Changes: %zu", m_pendingChanges.size());

    ImGui::Separator();

    // Watch directories
    ImGui::Text("Watched Directories:");
    for (const auto& watch : m_watchDirs) {
        ImGui::BulletText("%s (%s)", watch.path.c_str(), watch.recursive ? "recursive" : "non-recursive");
    }

    ImGui::Separator();

    // Recent changes
    ImGui::Text("Recent Changes:");
    if (ImGui::Button("Clear History")) {
        ClearChangeHistory();
    }

    if (ImGui::BeginTable("RecentChanges", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                         ImVec2(0, 200))) {
        ImGui::TableSetupColumn("File");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Time");
        ImGui::TableHeadersRow();

        for (auto it = m_recentChanges.rbegin(); it != m_recentChanges.rend(); ++it) {
            const auto& change = *it;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", fs::path(change.path).filename().string().c_str());

            ImGui::TableNextColumn();
            const char* typeText = "";
            ImVec4 typeColor = ImVec4(1, 1, 1, 1);
            switch (change.changeType) {
                case ChangeType::Created:
                    typeText = "Created";
                    typeColor = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
                    break;
                case ChangeType::Modified:
                    typeText = "Modified";
                    typeColor = ImVec4(0.3f, 0.8f, 1.0f, 1.0f);
                    break;
                case ChangeType::Deleted:
                    typeText = "Deleted";
                    typeColor = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                    break;
            }
            ImGui::TextColored(typeColor, "%s", typeText);

            ImGui::TableNextColumn();
            // Format timestamp
            auto time_t = std::chrono::system_clock::to_time_t(change.timestamp);
            char timeStr[32];
            std::strftime(timeStr, sizeof(timeStr), "%H:%M:%S", std::localtime(&time_t));
            ImGui::Text("%s", timeStr);
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

void HotReloadManager::AddWatchDirectory(const std::string& path, bool recursive) {
    // Check if already watching
    for (const auto& watch : m_watchDirs) {
        if (watch.path == path) {
            spdlog::warn("Already watching directory: {}", path);
            return;
        }
    }

    WatchEntry entry;
    entry.path = path;
    entry.recursive = recursive;
    entry.lastModified = 0;

    // Initialize file timestamps
    if (fs::exists(path)) {
        try {
            if (recursive) {
                for (const auto& dirEntry : fs::recursive_directory_iterator(path)) {
                    if (dirEntry.is_regular_file()) {
                        std::string filePath = dirEntry.path().string();
                        entry.fileTimestamps[filePath] = GetFileTimestamp(filePath);
                    }
                }
            } else {
                for (const auto& dirEntry : fs::directory_iterator(path)) {
                    if (dirEntry.is_regular_file()) {
                        std::string filePath = dirEntry.path().string();
                        entry.fileTimestamps[filePath] = GetFileTimestamp(filePath);
                    }
                }
            }
        } catch (const fs::filesystem_error& e) {
            spdlog::error("Failed to scan directory {}: {}", path, e.what());
            return;
        }
    }

    m_watchDirs.push_back(entry);
    spdlog::info("Now watching directory: {}", path);
}

void HotReloadManager::RemoveWatchDirectory(const std::string& path) {
    auto it = std::find_if(m_watchDirs.begin(), m_watchDirs.end(),
                          [&path](const WatchEntry& entry) { return entry.path == path; });

    if (it != m_watchDirs.end()) {
        m_watchDirs.erase(it);
        spdlog::info("Stopped watching directory: {}", path);
    }
}

void HotReloadManager::ClearWatchDirectories() {
    m_watchDirs.clear();
    spdlog::info("Cleared all watch directories");
}

void HotReloadManager::ReloadFile(const std::string& path) {
    std::string ext = fs::path(path).extension().string();

    if (ext == ".json") {
        ReloadConfigFile(path);
    } else {
        ReloadAssetFile(path);
    }
}

void HotReloadManager::ReloadAllConfigs() {
    spdlog::info("Reloading all configs...");

    auto& registry = Config::ConfigRegistry::Instance();
    size_t reloaded = registry.ReloadAll();

    spdlog::info("Reloaded {} configs", reloaded);
    m_totalReloads += reloaded;
}

void HotReloadManager::ReloadAllAssets() {
    spdlog::info("Reloading all assets...");

    auto& assetDb = Nova::AssetDatabaseManager::Instance().GetDatabase();
    size_t reloadedCount = 0;

    // Get all asset UUIDs and reimport them
    auto assetUuids = assetDb.GetAllAssetUUIDs();
    for (const auto& uuid : assetUuids) {
        if (assetDb.ReimportAsset(uuid)) {
            reloadedCount++;
        }
    }

    m_totalReloads += reloadedCount;
    spdlog::info("Reloaded {} assets", reloadedCount);
}

void HotReloadManager::ClearChangeHistory() {
    m_recentChanges.clear();
}

void HotReloadManager::ScanForChanges() {
    for (auto& watch : m_watchDirs) {
        if (!fs::exists(watch.path)) continue;

        std::unordered_map<std::string, int64_t> currentFiles;

        try {
            if (watch.recursive) {
                for (const auto& entry : fs::recursive_directory_iterator(watch.path)) {
                    if (entry.is_regular_file()) {
                        std::string filePath = entry.path().string();
                        currentFiles[filePath] = GetFileTimestamp(filePath);
                    }
                }
            } else {
                for (const auto& entry : fs::directory_iterator(watch.path)) {
                    if (entry.is_regular_file()) {
                        std::string filePath = entry.path().string();
                        currentFiles[filePath] = GetFileTimestamp(filePath);
                    }
                }
            }
        } catch (const fs::filesystem_error& e) {
            spdlog::error("Error scanning directory {}: {}", watch.path, e.what());
            continue;
        }

        // Detect modified files
        for (const auto& [path, timestamp] : currentFiles) {
            auto it = watch.fileTimestamps.find(path);
            if (it != watch.fileTimestamps.end()) {
                if (it->second != timestamp) {
                    // File modified
                    m_pendingChanges[path] = std::chrono::system_clock::now();
                }
            } else {
                // New file created
                FileChange change;
                change.path = path;
                change.changeType = ChangeType::Created;
                change.timestamp = std::chrono::system_clock::now();
                ProcessChange(change);
            }
        }

        // Detect deleted files
        for (const auto& [path, timestamp] : watch.fileTimestamps) {
            if (currentFiles.find(path) == currentFiles.end()) {
                // File deleted
                FileChange change;
                change.path = path;
                change.changeType = ChangeType::Deleted;
                change.timestamp = std::chrono::system_clock::now();
                ProcessChange(change);
            }
        }

        // Update watch entry
        watch.fileTimestamps = currentFiles;
    }
}

void HotReloadManager::ProcessChange(const FileChange& change) {
    if (!ShouldProcessChange(change)) {
        return;
    }

    // Add to history
    m_recentChanges.push_back(change);
    if (m_recentChanges.size() > m_maxChangeHistory) {
        m_recentChanges.erase(m_recentChanges.begin());
    }

    // Process based on change type
    switch (change.changeType) {
        case ChangeType::Created:
            spdlog::info("File created: {}", change.path);
            ReloadFile(change.path);
            break;

        case ChangeType::Modified:
            spdlog::info("File modified: {}", change.path);
            ReloadFile(change.path);
            break;

        case ChangeType::Deleted:
            spdlog::info("File deleted: {}", change.path);
            HandleAssetDeletion(change.path);
            break;
    }

    // Notify callback
    if (OnFileChanged) {
        OnFileChanged(change.path, change.changeType);
    }
}

bool HotReloadManager::ShouldProcessChange(const FileChange& change) {
    // Filter out temporary files, backups, etc.
    std::string filename = fs::path(change.path).filename().string();

    // Ignore temp files
    if (filename.front() == '~' || filename.front() == '.') {
        return false;
    }

    // Ignore backup files
    if (filename.ends_with(".bak") || filename.ends_with(".tmp")) {
        return false;
    }

    return true;
}

int64_t HotReloadManager::GetFileTimestamp(const std::string& path) {
    try {
        auto ftime = fs::last_write_time(path);
        return ftime.time_since_epoch().count();
    } catch (...) {
        return 0;
    }
}

void HotReloadManager::ReloadConfigFile(const std::string& path) {
    spdlog::info("Reloading config file: {}", path);

    auto& registry = Config::ConfigRegistry::Instance();
    bool success = registry.LoadFile(path);

    if (success) {
        m_totalReloads++;
        if (OnConfigReloaded) {
            OnConfigReloaded(path);
        }
        spdlog::info("Successfully reloaded config: {}", path);
    } else {
        m_failedReloads++;
        spdlog::error("Failed to reload config: {}", path);
    }
}

void HotReloadManager::ReloadAssetFile(const std::string& path) {
    spdlog::info("Reloading asset file: {}", path);

    std::string ext = fs::path(path).extension().string();
    auto& assetDb = Nova::AssetDatabaseManager::Instance().GetDatabase();

    // Check if asset is already registered by path
    if (assetDb.HasPath(path)) {
        auto asset = assetDb.GetAssetByPath(path);
        if (asset) {
            // Reimport the existing asset
            if (assetDb.ReimportAsset(asset->GetUuid())) {
                m_totalReloads++;
                spdlog::info("Successfully reloaded asset: {}", path);
            } else {
                m_failedReloads++;
                spdlog::error("Failed to reload asset: {}", path);
            }
        }
    } else {
        // New asset file - import it
        // Determine asset type based on extension for import settings
        Nova::AssetImportSettings settings;
        settings.generateThumbnail = true;
        settings.validateOnImport = true;
        settings.autoMigrate = true;
        settings.trackDependencies = true;

        if (assetDb.ImportAsset(path, settings)) {
            m_totalReloads++;
            spdlog::info("Successfully imported new asset: {}", path);
        } else {
            m_failedReloads++;
            spdlog::error("Failed to import asset: {}", path);
        }
    }

    if (OnAssetReloaded) {
        OnAssetReloaded(path);
    }
}

void HotReloadManager::HandleAssetDeletion(const std::string& path) {
    std::string ext = fs::path(path).extension().string();

    if (ext == ".json") {
        // Handle config deletion
        auto& registry = Config::ConfigRegistry::Instance();

        // Find config ID by path and unregister it
        auto allIds = registry.GetAllIds();
        for (const auto& id : allIds) {
            auto config = registry.Get(id);
            if (config && config->GetSourcePath() == path) {
                registry.Unregister(id);
                spdlog::info("Unregistered deleted config: {} ({})", id, path);
                break;
            }
        }
    } else {
        // Handle asset deletion
        auto& assetDb = Nova::AssetDatabaseManager::Instance().GetDatabase();

        if (assetDb.HasPath(path)) {
            auto asset = assetDb.GetAssetByPath(path);
            if (asset) {
                std::string uuid = asset->GetUuid();
                assetDb.UnregisterAsset(uuid);
                spdlog::info("Unregistered deleted asset: {} ({})", uuid, path);
            }
        }
    }
}

} // namespace Vehement
