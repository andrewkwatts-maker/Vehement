# Asset Pipeline Documentation

Complete guide to the Nova3D Engine asset import, management, and hot-reload pipeline.

## Table of Contents

1. [Overview](#overview)
2. [Asset Database](#asset-database)
3. [Import Pipeline](#import-pipeline)
4. [Hot-Reload System](#hot-reload-system)
5. [Dependency Management](#dependency-management)
6. [Asset Serialization](#asset-serialization)

## Overview

The Nova3D asset pipeline manages the entire lifecycle of game assets:

```
Source Files → Import → Validation → Database → Runtime → Hot-Reload
```

**Key Components:**

- **AssetDatabase**: Central registry of all assets
- **JsonAssetSerializer**: JSON parsing and serialization
- **Import Pipeline**: Automatic asset discovery and loading
- **Hot-Reload**: Live updates during development
- **Dependency Tracker**: Automatic dependency resolution

## Asset Database

The AssetDatabase is the central hub for all asset management.

### Initialization

```cpp
#include "engine/assets/AssetDatabase.hpp"

using namespace Nova;

// Initialize database with project root
AssetDatabase assetDB;
assetDB.Initialize("game/assets/projects/my_project");

// Or use singleton
AssetDatabaseManager::Instance().Initialize("project_root");
```

### Registering Assets

**Manual Registration:**

```cpp
auto material = std::make_shared<JsonAsset>();
material->metadata.type = AssetType::Material;
material->metadata.name = "My Material";
material->data = /* JSON data */;

assetDB.RegisterAsset(material);
```

**Automatic Import:**

```cpp
// Import single file
assetDB.ImportAsset("assets/materials/metal.json");

// Import entire directory
assetDB.ImportDirectory("assets/materials", true); // recursive
```

### Retrieving Assets

**By UUID:**

```cpp
auto asset = assetDB.GetAsset("uuid-string");
if (asset) {
    // Use asset
}
```

**By Path:**

```cpp
auto asset = assetDB.GetAssetByPath("assets/materials/metal.json");
```

**By Type:**

```cpp
auto materials = assetDB.GetAssetsByType(AssetType::Material);
for (const auto& mat : materials) {
    // Process each material
}
```

**By Tag:**

```cpp
auto glowingMats = assetDB.GetAssetsByTag("emissive");
```

**Search:**

```cpp
auto results = assetDB.SearchByName("metal");
```

### Statistics

```cpp
auto stats = assetDB.GetStatistics();
std::cout << "Total assets: " << stats.totalAssets << std::endl;
std::cout << "Loaded: " << stats.loadedAssets << std::endl;
std::cout << "Materials: " << stats.assetsByType[AssetType::Material] << std::endl;
```

## Import Pipeline

### Automatic Import

The import pipeline automatically discovers and loads assets:

```cpp
AssetImportSettings settings;
settings.generateThumbnail = true;
settings.validateOnImport = true;
settings.autoMigrate = true;
settings.trackDependencies = true;

assetDB.ImportDirectory("assets", true, settings);
```

### Import Process

**Step 1: Discovery**
- Scan directory for .json files
- Check file modification times
- Skip unchanged files

**Step 2: Parsing**
- Strip comments from JSON
- Parse JSON structure
- Extract metadata

**Step 3: Validation**
- Validate against schema
- Check required fields
- Verify dependencies exist

**Step 4: Migration**
- Check asset version
- Apply migrations if needed
- Update to latest format

**Step 5: Registration**
- Generate UUID if missing
- Build dependency graph
- Add to database

### Custom Import Settings

```cpp
AssetImportSettings customSettings;

// Skip validation for trusted assets
customSettings.validateOnImport = false;

// Don't generate thumbnails (faster)
customSettings.generateThumbnail = false;

// Import without tracking dependencies
customSettings.trackDependencies = false;

assetDB.ImportAsset("fast_asset.json", customSettings);
```

### Import Callbacks

```cpp
assetDB.SetProgressCallback(
    [](int current, int total, const std::string& name) {
        float progress = (float)current / total * 100.0f;
        std::cout << "Importing: " << name
                  << " (" << progress << "%)" << std::endl;
    }
);
```

## Hot-Reload System

The hot-reload system monitors assets for changes and reloads them automatically.

### Enabling Hot-Reload

```cpp
// Enable hot-reload
assetDB.SetHotReloadEnabled(true);

// Update every frame
void Update() {
    assetDB.Update(); // Checks for file changes
}
```

### Hot-Reload Callbacks

```cpp
assetDB.RegisterReloadCallback(
    [](const AssetReloadEvent& event) {
        std::cout << "Asset reloaded: " << event.uuid << std::endl;

        // Update runtime systems
        if (event.type == AssetType::Material) {
            materialSystem->UpdateMaterial(event.newAsset);
        }
        else if (event.type == AssetType::Shader) {
            shaderSystem->RecompileShader(event.newAsset);
        }
    }
);
```

### Hot-Reload Workflow

**Developer Workflow:**

1. Edit material JSON in external editor
2. Save file
3. Engine detects change (< 1 second)
4. Asset is reloaded
5. Callback updates runtime material
6. See changes immediately in viewport

**Example:**

```json
// Edit material.json
{
  "roughness": 0.5  // Change from 0.3 to 0.5
}
```

Engine automatically:
- Detects file change
- Reloads JSON
- Validates new data
- Updates GPU material
- Notifies callbacks

### Hot-Reload Performance

**File Watching:**
- Checks modification times every frame
- O(1) lookup via hash map
- Minimal overhead (~0.1ms for 1000 assets)

**Reload Time:**
- Small JSON: < 1ms
- Large JSON: < 10ms
- Shader recompile: 50-200ms
- Texture reload: 100-500ms

## Dependency Management

The asset database automatically tracks dependencies between assets.

### Dependency Graph

```
Material "metal_iron"
  ├─ Texture "iron_albedo.png"
  ├─ Texture "iron_normal.png"
  └─ Texture "iron_roughness.png"

Model "warrior"
  ├─ Material "armor_leather"
  ├─ Material "skin_human"
  └─ Animation "character_idle"
```

### Querying Dependencies

**Get Dependencies:**

```cpp
// Get all assets this asset depends on
std::vector<std::string> deps = assetDB.GetDependencies("material_uuid");

for (const auto& depUuid : deps) {
    auto depAsset = assetDB.GetAsset(depUuid);
    std::cout << "Depends on: " << depAsset->metadata.name << std::endl;
}
```

**Get Dependents:**

```cpp
// Get all assets that depend on this asset
std::vector<std::string> dependents = assetDB.GetDependents("texture_uuid");

// If texture changes, these materials need to be updated
for (const auto& matUuid : dependents) {
    auto mat = assetDB.GetAsset(matUuid);
    std::cout << "Used by: " << mat->metadata.name << std::endl;
}
```

### Dependency Resolution

**Automatic:**

```cpp
// Build dependency graph from all assets
assetDB.BuildDependencyGraph();

// Dependencies are extracted from JSON paths
// "textures": { "albedo": "texture.png" }
// Creates dependency: material → texture
```

**Manual:**

```cpp
// Manually add dependency
assetDB.AddDependency("material_uuid", "texture_uuid");
```

### Circular Dependencies

Circular dependencies are detected and reported:

```cpp
auto result = assetDB.ValidateAll();
for (const auto& error : result.errors) {
    if (error.find("circular") != std::string::npos) {
        std::cerr << "Circular dependency: " << error << std::endl;
    }
}
```

## Asset Serialization

### JsonAssetSerializer

The serializer handles all JSON parsing and generation.

### Loading Assets

**From File:**

```cpp
JsonAssetSerializer serializer;
auto asset = serializer.LoadFromFile("material.json");

if (!asset) {
    std::cerr << "Failed to load asset" << std::endl;
    return;
}
```

**From JSON:**

```cpp
nlohmann::json json = /* ... */;
auto asset = serializer.LoadFromJson(json);
```

### Saving Assets

**To File:**

```cpp
JsonAsset asset;
// Configure asset...

if (!serializer.SaveToFile(asset, "output.json")) {
    std::cerr << "Failed to save asset" << std::endl;
}
```

**To JSON:**

```cpp
nlohmann::json json = serializer.SaveToJson(asset);
std::cout << json.dump(2) << std::endl; // Pretty print
```

### Validation

**Validate Asset:**

```cpp
ValidationResult result = serializer.Validate(asset);

if (!result.isValid) {
    std::cerr << "Validation failed:" << std::endl;
    for (const auto& error : result.errors) {
        std::cerr << "  - " << error << std::endl;
    }
}

// Check warnings
for (const auto& warning : result.warnings) {
    std::cout << "Warning: " << warning << std::endl;
}
```

**Validate JSON:**

```cpp
nlohmann::json json = /* ... */;
auto result = serializer.ValidateJson(json, AssetType::Material);
```

### Schema Registration

**Register Custom Schema:**

```cpp
nlohmann::json schema = {
    {"type", "object"},
    {"required", {"name", "type", "version"}},
    {"properties", {
        {"name", {{"type", "string"}}},
        {"type", {{"type", "string"}}},
        {"version", {{"type", "string"}}}
    }}
};

serializer.RegisterSchema(AssetType::Material, schema);
```

### Type-Specific Serializers

**Material:**

```cpp
#include "engine/assets/JsonAssetSerializer.hpp"

using namespace Nova::MaterialSerializer;

// Serialize material
nlohmann::json json = Serialize(material);

// Deserialize material
auto material = Deserialize(json);

// Validate
auto result = Validate(json);
```

**Light:**

```cpp
using namespace Nova::LightSerializer;

LightData light;
light.type = "point";
light.intensity = 10.0f;
light.temperature = 3000.0f;

nlohmann::json json = Serialize(light);
```

**Model:**

```cpp
using namespace Nova::ModelSerializer;

ModelData model;
model.meshPath = "meshes/character.fbx";
model.materialPaths = {"materials/armor.json"};

nlohmann::json json = Serialize(model);
```

## Advanced Features

### Asset Versioning

**Check Version:**

```cpp
AssetVersion v1(1, 0, 0);
AssetVersion v2(1, 1, 0);

bool compatible = v1.IsCompatible(v2); // true (same major version)
```

**Migration:**

```cpp
class MaterialMigration_1_0_to_1_1 : public IAssetMigration {
public:
    AssetVersion GetFromVersion() const override {
        return AssetVersion(1, 0, 0);
    }

    AssetVersion GetToVersion() const override {
        return AssetVersion(1, 1, 0);
    }

    bool Migrate(nlohmann::json& data) override {
        // Rename 'color' to 'albedo'
        if (data.contains("color")) {
            data["albedo"] = data["color"];
            data.erase("color");
        }
        return true;
    }

    std::string GetDescription() const override {
        return "Rename 'color' field to 'albedo'";
    }
};

// Register migration
auto migration = std::make_shared<MaterialMigration_1_0_to_1_1>();
serializer.RegisterMigration(AssetType::Material, migration);
```

### Asset Thumbnails

**Generate Thumbnail:**

```cpp
assetDB.GenerateThumbnail("material_uuid", 256, 256);
```

**Get Thumbnail Path:**

```cpp
std::string thumbPath = assetDB.GetThumbnailPath("material_uuid");
if (assetDB.HasThumbnail("material_uuid")) {
    // Load and display thumbnail
}
```

### Asset Export

**Export Asset:**

```cpp
// Export to different location
assetDB.ExportAsset("material_uuid", "exported/material.json");
```

**Duplicate Asset:**

```cpp
auto duplicate = assetDB.DuplicateAsset(
    "source_uuid",
    "new_uuid",
    "New Material Name"
);
```

### Batch Operations

**Validate All Assets:**

```cpp
auto results = assetDB.ValidateAll();

for (const auto& result : results) {
    if (!result.isValid) {
        // Handle invalid asset
    }
}
```

**Reimport All:**

```cpp
assetDB.ClearCache();
assetDB.ImportDirectory("assets", true);
```

## Best Practices

### Performance

**Do:**
- Enable hot-reload only in development
- Use asset streaming for large projects
- Cache frequently accessed assets
- Profile asset loading

**Don't:**
- Load all assets at startup
- Validate in release builds
- Generate thumbnails for all assets
- Keep stale assets in database

### Organization

**Asset Naming:**
- Descriptive names: `warrior_leather_armor.json`
- Consistent prefixes: `mat_`, `tex_`, `mdl_`
- Version suffixes: `_v2.json` for iterations

**Directory Structure:**
```
assets/
  materials/
    characters/
    environment/
    effects/
  models/
    characters/
    buildings/
    props/
```

### Version Control

**Include in Git:**
- All .json asset files
- Project configuration
- Documentation

**Exclude from Git:**
- Generated thumbnails
- Asset database cache
- Compiled assets

**.gitignore:**
```
# Asset caches
cache/
*.db
*.thumbnail

# Compiled assets
*.compiled
```

### Workflow Integration

**Artist Workflow:**
1. Create asset in DCC tool (Blender, Maya)
2. Export to game format (FBX, OBJ)
3. Create JSON asset descriptor
4. Test in engine with hot-reload
5. Iterate rapidly

**Programmer Workflow:**
1. Define new asset type
2. Create schema
3. Register serializer
4. Implement hot-reload callback
5. Document format

## Troubleshooting

### Import Failures

**Common Causes:**
- Invalid JSON syntax
- Missing required fields
- Incorrect file paths
- Schema validation errors

**Debug:**
```cpp
assetDB.SetValidationEnabled(true);
auto asset = assetDB.ImportAsset("problem.json");
if (!asset) {
    // Check console for validation errors
}
```

### Hot-Reload Not Working

**Check:**
1. Hot-reload enabled: `assetDB.IsHotReloadEnabled()`
2. Update called every frame: `assetDB.Update()`
3. File modification time changed
4. Callback registered

**Debug:**
```cpp
assetDB.RegisterReloadCallback([](const AssetReloadEvent& e) {
    std::cout << "Reloaded: " << e.path << std::endl;
});
```

### Dependency Issues

**Circular Dependencies:**
```cpp
// Detect cycles
assetDB.BuildDependencyGraph();
auto result = assetDB.ValidateAll();
// Check for "circular dependency" errors
```

**Missing Dependencies:**
```cpp
auto deps = assetDB.GetDependencies("asset_uuid");
for (const auto& dep : deps) {
    if (!assetDB.HasAsset(dep)) {
        std::cerr << "Missing dependency: " << dep << std::endl;
    }
}
```

## API Reference

### AssetDatabase

| Method | Description |
|--------|-------------|
| `Initialize(path)` | Initialize database with project root |
| `RegisterAsset(asset)` | Register asset manually |
| `ImportAsset(path)` | Import single asset |
| `ImportDirectory(path)` | Import directory recursively |
| `GetAsset(uuid)` | Retrieve asset by UUID |
| `GetAssetByPath(path)` | Retrieve asset by file path |
| `SetHotReloadEnabled(bool)` | Enable/disable hot-reload |
| `Update()` | Check for file changes (call every frame) |

### JsonAssetSerializer

| Method | Description |
|--------|-------------|
| `LoadFromFile(path)` | Load asset from JSON file |
| `SaveToFile(asset, path)` | Save asset to JSON file |
| `Validate(asset)` | Validate asset structure |
| `RegisterSchema(type, schema)` | Register JSON schema |

## See Also

- [JSON Asset Format](JSON_ASSET_FORMAT.md)
- [Template Project Guide](TEMPLATE_PROJECT_GUIDE.md)
- [Editor World Creation](EDITOR_WORLD_CREATION.md)
