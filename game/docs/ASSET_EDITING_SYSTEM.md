# Asset Editing System - Comprehensive Guide

## Overview

The Asset Editing System provides a complete workflow for creating, editing, validating, and hot-reloading game assets (units, buildings, tiles, items, spells) directly within the Level Editor. This system integrates seamlessly with the RTSApplication's editor mode.

## Architecture

### Core Components

#### 1. **AssetBrowserEnhanced** (`H:/Github/Old3DEngine/game/src/editor/AssetBrowserEnhanced.hpp`)

Enhanced asset browser with advanced filtering and preview capabilities.

**Features:**
- Category-based filtering (Units, Buildings, Tiles, Models, Textures, Scripts, Configs, Locations, Spells, Items)
- Visual thumbnails with automatic generation for placeholder assets
- Search functionality with tag filtering
- Asset creation wizard with templates
- Inline preview panel
- Context menu actions (duplicate, delete, rename)
- Drag-drop support for asset placement

**Key Functions:**
```cpp
// Filtering
void SetCategoryFilter(AssetCategory category);
void SetSearchFilter(const std::string& filter);
void SetTagFilter(const std::vector<std::string>& tags);

// Asset operations
void CreateNewAsset(AssetCategory category, const std::string& templateId = "");
void DuplicateAsset(const std::string& path);
void DeleteAsset(const std::string& path);

// Selection
void SelectAsset(const std::string& path);
const std::string& GetSelectedAsset() const;
```

**Usage:**
```cpp
auto assetBrowser = std::make_unique<AssetBrowserEnhanced>(editor);
assetBrowser->SetCategoryFilter(AssetCategory::Units);
assetBrowser->OnAssetDoubleClicked = [](const std::string& path) {
    // Open config editor for this asset
};
```

#### 2. **ConfigEditor** (`H:/Github/Old3DEngine/game/src/editor/ConfigEditor.hpp`)

Existing configuration editor with JSON editing and property inspector.

**Features:**
- Tree view of all configs by type (Units, Buildings, Tiles)
- Dual editing modes: Property Inspector and Raw JSON Editor
- 3D model preview with wireframe rendering
- Collision shape visualization
- Python script hook browser
- Real-time validation
- Hot-reload on save

**Key Functions:**
```cpp
// Config management
void SelectConfig(const std::string& configId);
void CreateNewConfig(const std::string& type);
void SaveConfig(const std::string& configId);
void ReloadConfig(const std::string& configId);
void ValidateConfig(const std::string& configId);

// Callbacks
std::function<void(const std::string&)> OnConfigSelected;
std::function<void(const std::string&)> OnConfigModified;
```

**Tabs:**
1. **Properties** - Type-safe property editing with drag controls
2. **JSON** - Raw JSON editing with syntax highlighting and formatting
3. **Preview** - 3D model preview with rotation and zoom
4. **Collision** - Collision shape visualization and editing
5. **Scripts** - Python event hook configuration

#### 3. **ValidationPanel** (`H:/Github/Old3DEngine/game/src/editor/ValidationPanel.hpp`)

Comprehensive validation error and warning display.

**Features:**
- Real-time validation as configs are edited
- Severity-based filtering (Errors, Warnings, Info)
- Grouped by config and field
- Click to navigate to error location
- Batch validation of all configs
- Validation statistics

**Key Functions:**
```cpp
// Validation operations
void ValidateConfig(const std::string& configId);
void ValidateAllConfigs();
void ClearValidation();

// Results
size_t GetErrorCount() const;
size_t GetWarningCount() const;
bool HasErrors() const;

// Filtering
void SetShowErrors(bool show);
void SetShowWarnings(bool show);
void SetConfigFilter(const std::string& configId);
```

**Usage:**
```cpp
auto validationPanel = std::make_unique<ValidationPanel>(editor);
validationPanel->ValidateAllConfigs();
validationPanel->OnErrorClicked = [](const std::string& configId, int lineNumber) {
    // Navigate to config and highlight error
};
```

#### 4. **HotReloadManager** (`H:/Github/Old3DEngine/game/src/editor/HotReloadManager.hpp`)

Automatic file watching and hot-reload system.

**Features:**
- Directory watching with configurable poll interval
- Automatic reload when files are modified externally
- Debouncing to avoid excessive reloads
- Change history tracking
- Manual reload triggers
- File change notifications

**Key Functions:**
```cpp
// Directory watching
void AddWatchDirectory(const std::string& path, bool recursive = true);
void RemoveWatchDirectory(const std::string& path);

// Control
void EnableHotReload(bool enable);
void SetPollInterval(float seconds);
void SetDebounceDelay(float seconds);

// Manual triggers
void ReloadFile(const std::string& path);
void ReloadAllConfigs();
void ReloadAllAssets();

// Callbacks
std::function<void(const std::string&, ChangeType)> OnFileChanged;
std::function<void(const std::string&)> OnConfigReloaded;
```

**Usage:**
```cpp
auto hotReloadMgr = std::make_unique<HotReloadManager>(editor);
hotReloadMgr->SetPollInterval(1.0f);  // Check every second
hotReloadMgr->SetDebounceDelay(0.5f); // Wait 0.5s before reloading
hotReloadMgr->OnConfigReloaded = [](const std::string& path) {
    spdlog::info("Config reloaded: {}", path);
};
```

### Supporting Systems

#### 5. **ConfigRegistry** (`H:/Github/Old3DEngine/game/src/config/ConfigRegistry.hpp`)

Centralized registry for all entity configurations.

**Features:**
- Hot-reload support built-in
- Type-safe config retrieval
- Query by ID, type, or tags
- Inheritance resolution
- Change notifications
- Schema validation

**Key Functions:**
```cpp
// Loading
size_t LoadDirectory(const std::string& rootPath);
bool LoadFile(const std::string& filePath);
bool ReloadConfig(const std::string& configId);
size_t ReloadAll();

// Retrieval
std::shared_ptr<EntityConfig> Get(const std::string& id) const;
std::vector<std::shared_ptr<UnitConfig>> GetAllUnits() const;
std::vector<std::shared_ptr<BuildingConfig>> GetAllBuildings() const;

// Hot-reload
void EnableHotReload(const std::string& rootPath, int pollIntervalMs = 1000);
size_t CheckForChanges();

// Callbacks
int Subscribe(ConfigChangeCallback callback);
```

#### 6. **PlaceholderGenerator** (`H:/Github/Old3DEngine/game/src/assets/PlaceholderGenerator.hpp`)

Procedural asset generation for previews and placeholders.

**Features:**
- Generate 3D models (buildings, units, trees, resources)
- Generate textures (noise, checker, brick, wood, metal, water)
- Normal map generation
- UI icon generation

**Key Functions:**
```cpp
// Complete generation
static void GenerateAllPlaceholders(const std::string& basePath = "game/assets");

// Model generation
static void GenerateBuildingModel(const std::string& path, BuildingType type);
static void GenerateUnitModel(const std::string& path, UnitType type);
static void GenerateHexTileModel(const std::string& path, TileType type);

// Texture generation
static void GenerateNoiseTexture(const std::string& path, const glm::vec3& baseColor, int size = 256);
static void GenerateBrickTexture(const std::string& path, const glm::vec3& brickColor, const glm::vec3& mortarColor);
```

## Workflow

### 1. Creating a New Asset

**Using Asset Browser:**
```cpp
// User clicks "New Asset" button in AssetBrowserEnhanced
// 1. Select asset category (Unit, Building, Tile, etc.)
// 2. Choose from available templates
// 3. Enter asset name
// 4. Asset is created from template and saved to disk
// 5. ConfigRegistry automatically loads the new config
// 6. Asset is selected in browser for immediate editing
```

**Using Config Editor:**
```cpp
// User clicks "New" in ConfigEditor
// 1. Select config type from popup (Unit, Building, Tile)
// 2. Config is created with default values
// 3. Config is added to registry
// 4. User can immediately edit properties
```

### 2. Editing Assets

**Property Inspector Mode:**
```
1. Select asset in AssetBrowser or ConfigEditor tree
2. Edit properties in typed fields (numbers, text, dropdowns)
3. Changes are applied immediately to in-memory config
4. Click "Save" to write to JSON file
5. Hot-reload automatically updates running game (if enabled)
```

**JSON Editor Mode:**
```
1. Select asset
2. Switch to "JSON" tab
3. Edit raw JSON with syntax highlighting
4. Click "Format" to auto-format JSON
5. Click "Apply" to parse and update config
6. Click "Save" to write to file
```

### 3. Validation

**Automatic Validation:**
```cpp
// Validation happens automatically when:
// - Config is loaded from file
// - Config is modified in property inspector
// - JSON is edited and applied
// - User clicks "Validate" button
```

**Validation Results:**
```cpp
// Errors displayed in ValidationPanel:
// - Missing required fields
// - Invalid data types
// - Out-of-range values
// - Circular dependencies in inheritance
// - Invalid file paths (models, textures, scripts)
```

**Click-to-Fix:**
```cpp
// User clicks error in ValidationPanel
// -> Navigates to ConfigEditor with config selected
// -> Highlights problematic field (if applicable)
```

### 4. Hot-Reloading

**Automatic Hot-Reload:**
```
1. User modifies config in external editor (VSCode, Notepad++)
2. File system change detected by HotReloadManager
3. Change is debounced (default 0.5s delay)
4. ConfigRegistry.ReloadConfig() called
5. Config is re-parsed from JSON
6. Validation runs automatically
7. Change notification sent to subscribers
8. In-game entities update with new config values
```

**Manual Hot-Reload:**
```cpp
// User clicks "Reload" in ConfigEditor
// -> Specific config is reloaded from disk

// User clicks "Reload All Configs" in HotReloadManager
// -> All configs are reloaded from disk
```

### 5. Preview

**3D Model Preview:**
```
- Wireframe preview in ConfigEditor "Preview" tab
- Rotation and zoom controls
- Collision shape overlay
- Coordinate axes display
```

**Texture Preview:**
```
- Full texture display in AssetBrowserEnhanced
- Checkerboard background for transparency
- Zoom and pan controls
```

**Config Preview:**
```
- Structured JSON display
- Key stats highlighted (health, damage, speed)
- Tag display
- Referenced assets (model, texture paths)
```

## Integration with RTSApplication

### Editor Mode Setup

```cpp
// In RTSApplication.cpp - StartEditor()
void RTSApplication::StartEditor() {
    spdlog::info("Starting Level Editor");
    m_currentMode = GameMode::Editor;

    // Initialize editor if not already created
    if (!m_editor) {
        m_editor = std::make_unique<Vehement::Editor>();

        // Initialize with engine
        EditorConfig config;
        config.projectPath = "game/project.json";
        m_editor->Initialize(Nova::Engine::Instance(), config);

        // Create panels
        auto assetBrowser = std::make_unique<AssetBrowserEnhanced>(m_editor.get());
        auto validationPanel = std::make_unique<ValidationPanel>(m_editor.get());
        auto hotReloadMgr = std::make_unique<HotReloadManager>(m_editor.get());

        // Setup callbacks
        assetBrowser->OnAssetDoubleClicked = [this](const std::string& path) {
            // Open in config editor
            if (auto* configEditor = m_editor->GetConfigEditor()) {
                // Extract config ID from path
                std::string configId = ExtractConfigId(path);
                configEditor->SelectConfig(configId);
            }
        };

        hotReloadMgr->OnConfigReloaded = [this](const std::string& path) {
            // Update in-game entities with new config
            RefreshEntitiesFromConfig(path);
        };
    }
}
```

### Update Loop Integration

```cpp
// In RTSApplication.cpp - Update()
void RTSApplication::Update(float deltaTime) {
    // ... existing update code ...

    if (m_currentMode == GameMode::Editor && m_editor) {
        m_editor->Update(deltaTime);

        // Update hot-reload manager
        if (auto* hotReloadMgr = m_editor->GetHotReloadManager()) {
            hotReloadMgr->Update(deltaTime);
        }
    }
}
```

### Render Integration

```cpp
// In RTSApplication.cpp - OnImGui()
void RTSApplication::OnImGui() {
    // ... existing UI code ...

    if (m_currentMode == GameMode::Editor && m_editor) {
        m_editor->Render();
    }
}
```

## Configuration Files

### Directory Structure

```
game/assets/
├── configs/
│   ├── units/
│   │   ├── warrior.json
│   │   ├── archer.json
│   │   └── mage.json
│   ├── buildings/
│   │   ├── barracks.json
│   │   ├── town_hall.json
│   │   └── tower.json
│   └── tiles/
│       ├── grass.json
│       ├── water.json
│       └── stone.json
├── models/
│   ├── warrior.obj
│   ├── barracks.obj
│   └── grass_tile.obj
├── textures/
│   ├── warrior_diffuse.png
│   └── stone_wall.png
└── scripts/
    ├── units/
    │   └── warrior_ai.py
    └── buildings/
        └── barracks_production.py
```

### Unit Config Example

```json
{
  "id": "warrior",
  "name": "Warrior",
  "type": "unit",
  "tags": ["melee", "infantry", "tier1"],
  "model": "models/warrior.obj",
  "scale": [1.0, 1.0, 1.0],

  "movement": {
    "speed": 5.0,
    "turnRate": 360.0,
    "canFly": false,
    "canSwim": false
  },

  "combat": {
    "health": 100,
    "maxHealth": 100,
    "armor": 5,
    "attackDamage": 15,
    "attackSpeed": 1.0,
    "attackRange": 1.5
  },

  "ai": {
    "behaviorTree": "scripts/ai/melee_basic.py",
    "aggroRange": 10.0,
    "leashRange": 30.0
  },

  "abilities": [
    {
      "id": "battle_cry",
      "name": "Battle Cry",
      "cooldown": 30.0,
      "script": "scripts/abilities/battle_cry.py"
    }
  ],

  "scripts": {
    "on_spawn": "scripts/units/warrior_on_spawn.py",
    "on_death": "scripts/units/warrior_on_death.py",
    "on_attack": "scripts/units/warrior_on_attack.py"
  }
}
```

### Building Config Example

```json
{
  "id": "barracks",
  "name": "Barracks",
  "type": "building",
  "tags": ["production", "military", "tier1"],
  "model": "models/barracks.obj",

  "footprint": {
    "gridType": "rect",
    "size": [2, 2]
  },

  "construction": {
    "buildTime": 30.0,
    "cost": {
      "wood": 100,
      "stone": 50
    },
    "stages": [
      {
        "name": "foundation",
        "model": "models/barracks_stage1.obj",
        "progressStart": 0,
        "progressEnd": 33
      },
      {
        "name": "framing",
        "model": "models/barracks_stage2.obj",
        "progressStart": 33,
        "progressEnd": 66
      },
      {
        "name": "complete",
        "model": "models/barracks.obj",
        "progressStart": 66,
        "progressEnd": 100
      }
    ]
  },

  "stats": {
    "maxHealth": 500,
    "armor": 10,
    "visionRange": 15.0
  },

  "production": [
    {
      "type": "unit",
      "outputId": "warrior",
      "productionTime": 20.0,
      "cost": {
        "gold": 75,
        "food": 1
      },
      "maxQueue": 5
    }
  ],

  "scripts": {
    "on_construct_complete": "scripts/buildings/barracks_complete.py",
    "on_produce": "scripts/buildings/barracks_produce.py"
  }
}
```

## Asset Templates

The Asset Creation Wizard provides templates for quick asset creation:

### Unit Templates

1. **Basic Unit** - Simple melee unit with basic stats
2. **Ranged Unit** - Unit with projectile attack
3. **Hero Unit** - Powerful hero with abilities
4. **Worker Unit** - Resource gathering unit

### Building Templates

1. **Basic Building** - Simple production building
2. **Defense Tower** - Defensive structure with attack
3. **Resource Generator** - Building that produces resources
4. **Research Facility** - Building that unlocks technologies

### Tile Templates

1. **Terrain Tile** - Basic walkable terrain
2. **Special Tile** - Tile with special properties (speed boost, damage, etc.)
3. **Resource Node** - Harvestable resource tile

## Validation Rules

### Unit Validation

- **Required fields:** id, name, type, health, speed
- **Numeric ranges:** health > 0, speed > 0, armor >= 0
- **File existence:** model path must exist
- **Script validation:** script paths must exist and be valid Python files
- **Ability validation:** cooldown >= 0, manaCost >= 0

### Building Validation

- **Required fields:** id, name, type, footprint, buildTime
- **Footprint:** size must be > 0
- **Production:** outputId must reference valid unit/item config
- **Construction stages:** progressStart < progressEnd, covers 0-100%

### Tile Validation

- **Required fields:** id, name, type, walkable
- **Movement cost:** must be > 0 if walkable
- **Texture paths:** must exist

## Hot-Reload Implementation

### File Change Detection

```cpp
// HotReloadManager watches directories recursively
// 1. Poll file system every N seconds (configurable)
// 2. Compare file timestamps with cached values
// 3. Detect created, modified, deleted files
// 4. Apply debounce delay to avoid rapid reloads
// 5. Process change based on file type
```

### Config Reload Process

```cpp
// When config file changes:
1. ConfigRegistry::ReloadConfig(configId) called
2. JSON is re-parsed from disk
3. Config object is updated with new values
4. Validation runs automatically
5. Change notification sent to subscribers
6. In-game entities lookup new config values
7. Entities update their runtime state (health, speed, etc.)
```

### In-Game Entity Updates

```cpp
// Entities reference configs, not copy values
// When config reloads, entities automatically see new values

class Unit {
    std::shared_ptr<UnitConfig> m_config;  // Reference, not copy

    void Update(float dt) {
        // Always reads latest values from config
        float speed = m_config->GetMoveSpeed();
        float health = m_config->GetMaxHealth();
    }
};

// OR entities can subscribe to config changes:
auto subscriptionId = ConfigRegistry::Instance().Subscribe([](const ConfigChangeEvent& event) {
    if (event.changeType == ConfigChangeEvent::Type::Modified) {
        // Refresh entity using this config
        RefreshEntity(event.configId);
    }
});
```

## Performance Considerations

### Thumbnail Caching

- Thumbnails are generated once and cached in memory
- OpenGL textures are shared across multiple instances
- Placeholder thumbnails use simple procedural generation

### Hot-Reload Optimization

- Debounce delay prevents excessive reloads during rapid edits
- Only modified files are reloaded, not entire directory
- File timestamp comparison is O(1) per file
- Recursive directory iteration is throttled by poll interval

### Validation Performance

- Validation is cached until config is modified
- Batch validation uses parallel processing (future enhancement)
- Schema validation is pre-compiled for speed

## Keyboard Shortcuts

```
Ctrl+N       - New Asset
Ctrl+S       - Save Config
Ctrl+R       - Reload Config
Ctrl+Shift+R - Reload All Configs
Ctrl+V       - Validate Selected Config
Ctrl+Shift+V - Validate All Configs
F5           - Refresh Asset Browser
Delete       - Delete Selected Asset
Ctrl+D       - Duplicate Selected Asset
```

## Troubleshooting

### Assets Not Showing in Browser

1. Check that asset directory exists and is readable
2. Verify JSON files have valid syntax
3. Refresh asset browser (F5)
4. Check console for error messages

### Hot-Reload Not Working

1. Verify HotReloadManager is initialized and enabled
2. Check that watch directories are configured correctly
3. Increase poll interval if changes are being missed
4. Check file permissions

### Validation Errors Not Clearing

1. Ensure validation runs after fixing config
2. Clear validation panel and re-validate
3. Check that config file was actually saved to disk

### Preview Not Updating

1. Verify model file path in config is correct
2. Check that PlaceholderGenerator has generated preview assets
3. Reload config to refresh preview

## Future Enhancements

1. **Visual Scripting** - Node-based behavior tree editor
2. **Diff Viewer** - Show changes between config versions
3. **Batch Operations** - Multi-select and batch edit assets
4. **Import/Export** - Import assets from external sources
5. **Asset Bundles** - Package related assets together
6. **Version Control** - Git integration for asset tracking
7. **Collaboration** - Multi-user editing with conflict resolution
8. **Performance Profiler** - Analyze asset impact on game performance

## Summary

The Asset Editing System provides a comprehensive, production-ready solution for managing game assets in the RTS game. It integrates seamlessly with the existing ConfigRegistry and PlaceholderGenerator systems, while adding powerful new features for asset creation, validation, and hot-reloading.

Key benefits:
- **Fast iteration** - Create and edit assets without restarting the game
- **Error prevention** - Real-time validation catches mistakes early
- **Visual feedback** - 3D previews show exactly how assets will look in-game
- **Professional workflow** - Asset browser, property inspector, and JSON editor provide multiple ways to work
- **Extensible** - Easy to add new asset types and validation rules

All source files are located in:
- `H:/Github/Old3DEngine/game/src/editor/AssetBrowserEnhanced.{hpp,cpp}`
- `H:/Github/Old3DEngine/game/src/editor/ValidationPanel.{hpp,cpp}`
- `H:/Github/Old3DEngine/game/src/editor/HotReloadManager.{hpp,cpp}`
- `H:/Github/Old3DEngine/game/src/editor/ConfigEditor.{hpp,cpp}` (existing, enhanced)
- `H:/Github/Old3DEngine/game/src/config/ConfigRegistry.{hpp,cpp}` (existing, used)
- `H:/Github/Old3DEngine/game/src/assets/PlaceholderGenerator.{hpp,cpp}` (existing, used)
