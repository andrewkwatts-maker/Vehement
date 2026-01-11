# World Persistence System

## Overview

The World Persistence system provides a comprehensive solution for saving and synchronizing world edits in the 3D engine. It supports both online (Firebase) and offline (SQLite) storage modes, with automatic conflict resolution and seamless synchronization.

## Architecture

### Core Components

1. **WorldEdit** - Represents a single edit/modification to the world
   - Supports both geographic (lat/long/alt) and Cartesian (XYZ) coordinates
   - Flexible edit data stored as JSON
   - Automatic chunk ID calculation for spatial indexing
   - Types: TerrainHeight, PlacedObject, RemovedObject, PaintTexture, Sculpt, Custom

2. **WorldPersistenceManager** - Central persistence manager
   - Three storage modes: Online (Firebase), Offline (SQLite), Hybrid (both)
   - Automatic synchronization with configurable intervals
   - Conflict detection and resolution
   - Integration with procedural generation
   - Thread-safe operations with mutex protection

3. **WorldPersistenceUI** - ImGui-based UI panel
   - Mode switching (Online/Offline/Hybrid)
   - Manual sync controls (Upload/Download/Bidirectional)
   - Real-time statistics display
   - Conflict resolution interface
   - Configuration editor

## Storage Backends

### SQLite (Offline Storage)

**Database Schema:**
```sql
CREATE TABLE world_edits (
    id TEXT PRIMARY KEY,
    world_id TEXT NOT NULL,
    chunk_x INTEGER NOT NULL,
    chunk_y INTEGER NOT NULL,
    chunk_z INTEGER NOT NULL,
    use_geo INTEGER NOT NULL,
    latitude REAL,
    longitude REAL,
    altitude REAL,
    pos_x REAL,
    pos_y REAL,
    pos_z REAL,
    edit_type INTEGER NOT NULL,
    edit_data TEXT,
    timestamp INTEGER NOT NULL,
    user_id TEXT,
    version INTEGER NOT NULL,
    synced INTEGER NOT NULL DEFAULT 0
);
```

**Indexes:**
- `idx_world_chunk` - Fast chunk-based queries
- `idx_timestamp` - Temporal queries
- `idx_synced` - Pending sync tracking

**Features:**
- Local-first architecture
- Fast spatial queries using chunk indexing
- Persistent storage for offline editing
- Automatic sync state tracking

### Firebase (Online Storage)

**Path Structure:**
```
/worlds/{world_id}/edits/{chunk_id}/{edit_id}
```

**Features:**
- Real-time synchronization
- Collaborative editing support
- Cloud backup
- Cross-device accessibility
- Automatic timestamp management

## Usage

### Initialization

```cpp
// Basic initialization (Offline only)
WorldPersistenceManager manager;
WorldPersistenceManager::Config config;
config.mode = WorldPersistenceManager::StorageMode::Offline;
config.sqlitePath = "world_edits.db";
config.worldId = "my_world";
manager.Initialize(config);

// With Firebase (Hybrid mode)
auto firebaseClient = std::make_shared<FirebaseClient>();
firebaseClient->Initialize(firebaseConfig);

WorldPersistenceManager::Config config;
config.mode = WorldPersistenceManager::StorageMode::Hybrid;
config.autoSync = true;
config.syncInterval = 30.0f;  // Sync every 30 seconds

manager.InitializeWithFirebase(firebaseClient, config);
```

### Saving Edits

```cpp
// Create an edit
WorldEdit edit;
edit.worldId = "my_world";
edit.position = glm::vec3(100.0f, 50.0f, 200.0f);
edit.type = WorldEdit::Type::TerrainHeight;

// Add edit-specific data
nlohmann::json editData;
editData["height"] = 10.0f;
editData["radius"] = 5.0f;
editData["falloff"] = 0.5f;
edit.editData = editData.dump();

// Save the edit
manager.SaveEdit(edit);
```

### Loading Edits

```cpp
// Load edits in a specific chunk
glm::ivec3 chunkId(5, 0, 10);
auto edits = manager.LoadEditsInChunk(chunkId);

// Query edits in a region
auto edits = manager.LoadEditsInRegion(
    glm::ivec3(0, 0, 0),  // Min chunk
    glm::ivec3(10, 5, 10) // Max chunk
);

// Advanced query
WorldEditQuery query;
query.worldId = "my_world";
query.chunkMin = glm::ivec3(0, 0, 0);
query.chunkMax = glm::ivec3(100, 10, 100);
query.sinceTimestamp = lastSyncTime;
query.typeFilter = {WorldEdit::Type::TerrainHeight, WorldEdit::Type::Sculpt};
query.maxResults = 1000;

auto edits = manager.QueryEdits(query);
```

### Synchronization

```cpp
// Manual sync
manager.SyncToFirebase();      // Upload local edits
manager.SyncFromFirebase();    // Download remote edits
manager.SyncBidirectional();   // Full two-way sync

// Automatic sync (configured during initialization)
// Just call Update() each frame
manager.Update(deltaTime);

// Check sync status
bool hasPending = manager.HasPendingUploads();
int pendingCount = manager.GetPendingUploadCount();
```

### Procedural Generation Integration

```cpp
// Set procedural generator
manager.SetProceduralGenerator([](const glm::ivec3& chunkId, std::vector<float>& terrainData) {
    // Generate base terrain using noise, etc.
    GenerateProceduralTerrain(chunkId, terrainData);
});

// Apply edits on top of procedural terrain
std::vector<float> terrainData;
glm::ivec3 chunkId(5, 0, 10);
manager.ApplyEditsToProceduralTerrain(chunkId, terrainData);
```

### Conflict Resolution

```cpp
// Set default resolution strategy
manager.SetConflictResolutionStrategy(ConflictResolution::MergeChanges);

// Register custom conflict callback
manager.RegisterConflictCallback([](const EditConflict& conflict) {
    // Custom resolution logic
    if (conflict.localEdit.timestamp > conflict.remoteEdit.timestamp) {
        return ConflictResolution::KeepLocal;
    }
    return ConflictResolution::KeepRemote;
});

// Handle conflicts manually
auto conflicts = manager.GetPendingConflicts();
for (const auto& conflict : conflicts) {
    // User decides resolution
    manager.ResolveConflict(conflict.localEdit.id, ConflictResolution::KeepLocal);
}
```

## UI Integration

### Adding to StandaloneEditor

```cpp
// In StandaloneEditor.hpp
#include "WorldPersistence.hpp"

class StandaloneEditor {
private:
    std::unique_ptr<WorldPersistenceManager> m_persistenceManager;
    std::unique_ptr<WorldPersistenceUI> m_persistenceUI;
};

// In StandaloneEditor.cpp
bool StandaloneEditor::Initialize() {
    // ... other initialization ...

    // Initialize persistence
    m_persistenceManager = std::make_unique<WorldPersistenceManager>();
    WorldPersistenceManager::Config config;
    config.mode = WorldPersistenceManager::StorageMode::Hybrid;
    config.sqlitePath = "world_edits.db";
    config.worldId = "editor_world";
    config.autoSync = true;
    m_persistenceManager->Initialize(config);

    // Initialize UI
    m_persistenceUI = std::make_unique<WorldPersistenceUI>();
    m_persistenceUI->Initialize(m_persistenceManager.get());

    return true;
}

void StandaloneEditor::Update(float deltaTime) {
    // ... other updates ...

    // Update persistence system
    if (m_persistenceManager) {
        m_persistenceManager->Update(deltaTime);
    }
}

void StandaloneEditor::RenderUI() {
    // ... other UI ...

    // Render persistence UI
    if (ImGui::Begin("World Persistence")) {
        if (m_persistenceUI) {
            m_persistenceUI->Render();
        }
        ImGui::End();
    }
}
```

## Configuration Options

### Storage Modes

- **Online**: Uses Firebase only, requires internet connection
- **Offline**: Uses SQLite only, works without internet
- **Hybrid**: Uses both, syncs between them automatically

### Sync Settings

- `autoSync`: Enable/disable automatic synchronization
- `syncInterval`: Time in seconds between automatic syncs (default: 30.0)
- `maxEditsPerSync`: Maximum edits to sync in one batch (default: 100)

### Conflict Resolution Strategies

- `KeepLocal`: Always keep the local version
- `KeepRemote`: Always use the remote version
- `KeepBoth`: Save both as separate edits
- `MergeChanges`: Attempt to merge (uses newer timestamp)
- `AskUser`: Prompt user to resolve each conflict

## Statistics

The system tracks various statistics accessible via `GetStats()`:

- `totalEditsLocal`: Total edits in local database
- `totalEditsRemote`: Total edits in Firebase
- `editsUploaded`: Number of edits uploaded to Firebase
- `editsDownloaded`: Number of edits downloaded from Firebase
- `conflictsDetected`: Number of conflicts detected
- `conflictsResolved`: Number of conflicts resolved
- `lastSyncDuration`: Duration of last sync operation (seconds)
- `lastSyncTime`: Timestamp of last sync

## Best Practices

### 1. Chunk Size Selection

Choose appropriate chunk sizes for your world:
- Smaller chunks (8-16 units): More granular loading, better for sparse edits
- Larger chunks (32-64 units): Less overhead, better for dense editing

### 2. Sync Frequency

Balance between data freshness and network usage:
- Frequent syncs (10-30s): Real-time collaboration, more network traffic
- Infrequent syncs (60-300s): Reduced network usage, delayed updates

### 3. Conflict Prevention

Minimize conflicts by:
- Using coarse-grained locking for shared areas
- Implementing user presence indicators
- Encouraging users to work in different regions

### 4. Performance Optimization

- Use spatial queries (chunk-based) instead of full world scans
- Batch multiple edits before saving
- Configure maxEditsPerSync based on network conditions
- Enable delta compression for large terrain edits

### 5. Offline Support

For reliable offline editing:
- Set mode to Hybrid or Offline
- Enable local backups: `config.saveLocalBackup = true`
- Handle sync errors gracefully
- Display sync status to users

## Error Handling

The system provides callbacks for error handling:

```cpp
manager.OnSyncComplete = [](bool success, const std::string& message) {
    if (success) {
        spdlog::info("Sync completed: {}", message);
    } else {
        spdlog::error("Sync failed: {}", message);
    }
};

manager.OnConflictDetected = [](const EditConflict& conflict) {
    spdlog::warn("Conflict detected for edit: {}", conflict.localEdit.id);
    // Display notification to user
};

manager.OnSyncProgress = [](int progress, int total) {
    float percentage = (float)progress / (float)total * 100.0f;
    spdlog::info("Sync progress: {:.1f}%", percentage);
};
```

## Future Enhancements

Potential improvements for future development:

1. **Compression**: Implement data compression for large edits
2. **Versioning**: Add edit history and rollback support
3. **Replication**: Real-time multiplayer synchronization
4. **Partitioning**: Automatic world partitioning for very large worlds
5. **Analytics**: Track editing patterns and performance metrics
6. **Migration**: Tools for migrating between storage backends
7. **Encryption**: Encrypt sensitive edit data
8. **Delta Updates**: Only sync changed portions of edits

## Requirements

### SQLite

**Windows:**
- Download from: https://www.sqlite.org/download.html
- Place `sqlite3.lib` and `sqlite3.dll` in `deps/sqlite/` directory
- Or install via vcpkg: `vcpkg install sqlite3`

**Linux:**
```bash
sudo apt-get install libsqlite3-dev  # Ubuntu/Debian
sudo dnf install sqlite-devel        # Fedora
```

**macOS:**
```bash
brew install sqlite3
```

### Firebase (Optional)

Firebase support is included in the engine's networking layer. Configure Firebase credentials in your app configuration.

## License

This persistence system is part of the Nova3D Engine and follows the same license terms.
