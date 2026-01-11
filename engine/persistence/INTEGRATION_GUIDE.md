# Persistence System Integration Guide

## Overview

The persistence system provides dual-backend storage with SQLite (local) and Firebase (cloud) support, enabling:
- Automatic save/load of editor assets
- Offline editing with cloud sync
- Change tracking for undo/redo
- Multi-user collaboration
- Asset versioning

## Quick Start

### 1. Initialize Persistence Manager

```cpp
#include "persistence/PersistenceManager.hpp"
#include <fstream>

// Load configuration
std::ifstream configFile("assets/config/persistence.json");
nlohmann::json config = nlohmann::json::parse(configFile);

// Configure persistence
PersistenceManager::Config pmConfig;
pmConfig.enableSQLite = config["backends"]["sqlite"]["enabled"];
pmConfig.enableFirebase = config["backends"]["firebase"]["enabled"];
pmConfig.sqliteConfig = config["backends"]["sqlite"];
pmConfig.firebaseConfig = config["backends"]["firebase"];
pmConfig.autoSync = config["sync"]["auto_sync"];
pmConfig.syncInterval = config["sync"]["sync_interval"];

// Initialize
auto& persistence = PersistenceManager::Instance();
persistence.Initialize(pmConfig);
```

### 2. Save Assets

```cpp
// Save a scene asset
nlohmann::json sceneData;
sceneData["name"] = "MyScene";
sceneData["objects"] = /* ... */;

// Auto-saves to SQLite immediately, queues for Firebase
persistence.SaveAsset("scene_001", sceneData);
```

### 3. Load Assets

```cpp
// Load from cache -> SQLite -> Firebase (in order)
auto sceneData = persistence.LoadAsset("scene_001");

if (!sceneData.is_null()) {
    // Use loaded data
}
```

### 4. List Assets

```cpp
AssetFilter filter;
filter.type = "scene";
filter.modifiedAfter = yesterday;

auto assets = persistence.ListAssets(filter);
for (const auto& id : assets) {
    spdlog::info("Asset: {}", id);
}
```

### 5. Undo/Redo

```cpp
// Undo last change
if (persistence.CanUndo("scene_001")) {
    persistence.UndoChange("scene_001");
}

// Redo
if (persistence.CanRedo("scene_001")) {
    persistence.RedoChange("scene_001");
}
```

### 6. Sync with Cloud

```cpp
// Manual sync
persistence.ForceSync([](bool success, const std::string& error) {
    if (success) {
        spdlog::info("Sync completed");
    } else {
        spdlog::error("Sync failed: {}", error);
    }
});

// Check sync status
auto status = persistence.GetSyncStatus();
spdlog::info("Online: {}, Pending: {}", status.online, status.pending Changes);
```

### 7. Handle Conflicts

```cpp
// Setup callback
persistence.OnConflictDetected = [](const std::string& assetId) {
    spdlog::warn("Conflict detected for asset: {}", assetId);

    // Show UI dialog for manual resolution
    ShowConflictDialog(assetId);
};

// Manual resolution
void ShowConflictDialog(const std::string& assetId) {
    auto conflictData = persistence.GetConflictData(assetId);
    auto localData = conflictData["local"];
    auto remoteData = conflictData["remote"];

    // User chooses version
    bool useLocal = /* UI choice */;
    persistence.ResolveConflict(assetId, useLocal);
}
```

### 8. Multi-User Coordination

```cpp
// Lock asset for editing
if (persistence.LockAsset("scene_001", 300.0f)) {  // 5 min lock
    // Edit asset
    EditAsset("scene_001");

    // Unlock when done
    persistence.UnlockAsset("scene_001");
} else {
    auto owner = persistence.GetAssetLockOwner("scene_001");
    spdlog::warn("Asset locked by: {}", owner);
}
```

### 9. Editor Integration Example

```cpp
class StandaloneEditor {
private:
    PersistenceManager& m_persistence;

public:
    bool Initialize() {
        // Load config
        std::ifstream configFile("assets/config/persistence.json");
        nlohmann::json config = nlohmann::json::parse(configFile);

        // Init persistence
        PersistenceManager::Config pmConfig;
        pmConfig.sqliteConfig = config["backends"]["sqlite"];
        pmConfig.firebaseConfig = config["backends"]["firebase"];

        m_persistence = PersistenceManager::Instance();
        m_persistence.Initialize(pmConfig);

        // Setup callbacks
        m_persistence.OnAssetChanged = [this](const std::string& id, const nlohmann::json& data) {
            OnAssetChanged(id, data);
        };

        m_persistence.OnSyncCompleted = [](bool success, const std::string& error) {
            if (success) {
                ShowNotification("Cloud sync completed");
            } else {
                ShowNotification("Cloud sync failed: " + error);
            }
        };

        return true;
    }

    void Update(float deltaTime) {
        m_persistence.Update(deltaTime);
    }

    void SaveScene(const std::string& sceneId) {
        nlohmann::json sceneData = SerializeScene();

        AssetMetadata metadata;
        metadata.type = "scene";
        metadata.userId = "editor_user";

        m_persistence.SaveAsset(sceneId, sceneData, &metadata);
    }

    void LoadScene(const std::string& sceneId) {
        auto sceneData = m_persistence.LoadAsset(sceneId);
        if (!sceneData.is_null()) {
            DeserializeScene(sceneData);
        }
    }

    void ShowSyncUI() {
        auto status = m_persistence.GetSyncStatus();

        ImGui::Text("Status: %s", status.online ? "Online" : "Offline");
        ImGui::Text("Pending: %zu", status.pendingChanges);
        ImGui::Text("Synced: %zu", status.syncedChanges);

        if (ImGui::Button("Force Sync")) {
            m_persistence.ForceSync();
        }

        // Show conflicts
        auto conflicts = m_persistence.GetConflictedAssets();
        if (!conflicts.empty()) {
            ImGui::TextColored(ImVec4(1,0,0,1), "Conflicts: %zu", conflicts.size());
            for (const auto& id : conflicts) {
                if (ImGui::Button(("Resolve: " + id).c_str())) {
                    ShowConflictDialog(id);
                }
            }
        }
    }
};
```

## Best Practices

1. **Always initialize before use**
   - Load configuration from JSON
   - Set up callbacks before first save/load

2. **Handle offline mode gracefully**
   - Check `IsOnline()` before requiring cloud features
   - Queue changes will auto-sync when back online

3. **Use appropriate asset IDs**
   - Use unique, descriptive IDs
   - Consider hierarchical naming: "scenes/level1/main"

4. **Implement conflict resolution UI**
   - Always provide manual resolution option
   - Show diffs when possible

5. **Lock assets for exclusive editing**
   - Lock before editing in multi-user scenarios
   - Set appropriate lock duration
   - Always unlock when done

6. **Regular sync for collaboration**
   - Keep sync interval reasonable (30s default)
   - Show sync status in UI
   - Provide manual sync button

7. **Test offline scenarios**
   - Disable Firebase temporarily
   - Verify offline queue works
   - Test sync after reconnection

## Configuration

Edit `assets/config/persistence.json`:

```json
{
  "backends": {
    "sqlite": {
      "enabled": true,
      "database_path": "editor_data.db"
    },
    "firebase": {
      "enabled": true,
      "project_id": "your-project",
      "api_key": "your-api-key"
    }
  },
  "sync": {
    "auto_sync": true,
    "sync_interval": 30.0
  }
}
```

## Database Schema

### SQLite Tables

**assets**
- id (TEXT PRIMARY KEY)
- type (TEXT)
- data (TEXT - JSON)
- version (INTEGER)
- created_at (INTEGER)
- modified_at (INTEGER)
- checksum (TEXT)
- user_id (TEXT)

**changes**
- id (INTEGER PRIMARY KEY)
- asset_id (TEXT)
- change_type (TEXT: create/update/delete)
- old_data (TEXT - JSON)
- new_data (TEXT - JSON)
- timestamp (INTEGER)
- synced (INTEGER - 0/1)
- user_id (TEXT)

**asset_versions**
- id (INTEGER PRIMARY KEY)
- asset_id (TEXT)
- version (INTEGER)
- data (TEXT - JSON)
- created_at (INTEGER)

### Firebase Structure

```
/assets/
  /{assetId}/
    /data - Asset JSON
    /metadata - AssetMetadata
    /versions/
      /{version} - Versioned data

/changes/
  /{changeId} - ChangeEntry

/locks/
  /{assetId}/
    /userId
    /expiresAt
```

## Troubleshooting

### SQLite not found
- Install SQLite3 development package
- Or place sqlite3.lib in `deps/sqlite/`

### Firebase connection fails
- Check API key and project ID
- Verify network connectivity
- Check Firebase console for project status

### Sync conflicts
- Review conflict resolution strategy
- Implement manual resolution UI
- Consider last-write-wins for non-critical data

### Performance issues
- Reduce sync interval
- Increase sync batch size
- Enable memory cache
- Vacuum SQLite database periodically

## Testing

```cpp
// Unit test example
TEST(PersistenceTest, SaveAndLoad) {
    auto& pm = PersistenceManager::Instance();

    nlohmann::json data;
    data["value"] = 42;

    ASSERT_TRUE(pm.SaveAsset("test_001", data));

    auto loaded = pm.LoadAsset("test_001");
    ASSERT_FALSE(loaded.is_null());
    ASSERT_EQ(loaded["value"], 42);
}

TEST(PersistenceTest, UndoRedo) {
    auto& pm = PersistenceManager::Instance();

    nlohmann::json v1; v1["version"] = 1;
    nlohmann::json v2; v2["version"] = 2;

    pm.SaveAsset("test_undo", v1);
    pm.SaveAsset("test_undo", v2);

    ASSERT_TRUE(pm.CanUndo("test_undo"));
    pm.UndoChange("test_undo");

    auto loaded = pm.LoadAsset("test_undo");
    ASSERT_EQ(loaded["version"], 1);
}
```

## API Reference

See header files for complete API:
- `IPersistenceBackend.hpp` - Backend interface
- `SQLiteBackend.hpp` - SQLite implementation
- `FirebaseBackend.hpp` - Firebase implementation
- `PersistenceManager.hpp` - Main manager class
