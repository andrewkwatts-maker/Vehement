# Spherical World System Architecture

## Overview

The Spherical World System enables the Nova3D editor to create and edit planet-scale environments with procedural terrain generation, visual node-based PCG (Procedural Content Generation), and persistent world editing. This architecture supports both fully procedural worlds and custom-edited regions, with changes stored as delta overrides in either Firebase or SQLite.

## Table of Contents

1. [Coordinate System](#coordinate-system)
2. [World Data Structure](#world-data-structure)
3. [PCG Visual Node Graph System](#pcg-visual-node-graph-system)
4. [Persistence Layer](#persistence-layer)
5. [Editor Integration](#editor-integration)
6. [File Format](#file-format)
7. [Rendering Pipeline](#rendering-pipeline)
8. [Performance Considerations](#performance-considerations)

---

## Coordinate System

### Spherical to Cartesian Conversion

The system uses a right-handed coordinate system where:
- **Y-axis** points "up" through the north pole
- **X-axis** points to the right (equator at 0° longitude)
- **Z-axis** points forward (equator at 90° longitude)

#### Latitude/Longitude to XYZ

```cpp
// Input:
// - lat: Latitude in degrees [-90, 90] (negative = south, positive = north)
// - lon: Longitude in degrees [-180, 180] (negative = west, positive = east)
// - radius: Distance from planet center
// - altitude: Height above surface (optional)

glm::vec3 SphericalToCartesian(float lat, float lon, float radius, float altitude = 0.0f) {
    float r = radius + altitude;
    float latRad = glm::radians(lat);
    float lonRad = glm::radians(lon);

    float x = r * cos(latRad) * cos(lonRad);
    float y = r * sin(latRad);
    float z = r * cos(latRad) * sin(lonRad);

    return glm::vec3(x, y, z);
}
```

#### XYZ to Latitude/Longitude

```cpp
// Output:
// - lat: Latitude in degrees
// - lon: Longitude in degrees
// - altitude: Height above nominal radius

struct SphericalCoords {
    float latitude;
    float longitude;
    float altitude;
};

SphericalCoords CartesianToSpherical(const glm::vec3& position, float nominalRadius) {
    SphericalCoords result;

    float distance = glm::length(position);
    result.altitude = distance - nominalRadius;

    // Latitude: angle from equatorial plane
    result.latitude = glm::degrees(asin(position.y / distance));

    // Longitude: angle in XZ plane
    result.longitude = glm::degrees(atan2(position.z, position.x));

    return result;
}
```

#### Surface Normal Calculation

```cpp
// Normal at any point on the sphere points radially outward
glm::vec3 GetSurfaceNormal(const glm::vec3& position) {
    return glm::normalize(position);
}

// Tangent space for texture mapping
struct TangentSpace {
    glm::vec3 normal;
    glm::vec3 tangent;   // East direction
    glm::vec3 bitangent; // North direction
};

TangentSpace GetTangentSpace(float lat, float lon) {
    float latRad = glm::radians(lat);
    float lonRad = glm::radians(lon);

    TangentSpace ts;

    // Normal points radially outward
    ts.normal = glm::vec3(
        cos(latRad) * cos(lonRad),
        sin(latRad),
        cos(latRad) * sin(lonRad)
    );

    // Tangent points east
    ts.tangent = glm::vec3(-sin(lonRad), 0.0f, cos(lonRad));

    // Bitangent points north
    ts.bitangent = glm::cross(ts.normal, ts.tangent);

    return ts;
}
```

### Coordinate Precision

For large worlds, use double precision for coordinates:

```cpp
// World-space uses doubles for precision
struct SphericalCoordsD {
    double latitude;
    double longitude;
    double altitude;
};

// Convert to local float coordinates for rendering
glm::vec3 ToLocalSpace(const SphericalCoordsD& world, const SphericalCoordsD& origin);
```

---

## World Data Structure

### World Definition

```cpp
/**
 * @brief Defines a complete spherical world
 */
struct SphericalWorld {
    // Identity
    std::string worldId;              // Unique identifier
    std::string name;                 // Display name
    std::string description;

    // Physical properties
    float radius = 6371000.0f;        // Default: Earth radius in meters
    float gravity = 9.81f;            // Surface gravity (m/s²)
    float atmosphereHeight = 100000.0f; // Atmosphere thickness (m)

    // Procedural generation
    PCGGraphPtr proceduralGraph;      // Visual node graph for PCG
    uint32_t seed = 0;                // Random seed for generation

    // Biomes and climate
    std::vector<BiomeDefinition> biomes;
    ClimateSettings climateSettings;

    // LOD configuration
    LODSettings lodSettings;

    // Rendering
    RenderSettings renderSettings;

    // Persistence
    PersistenceMode persistenceMode;  // Firebase or SQLite
    std::string persistenceId;        // Database identifier

    // Metadata
    uint64_t createdTime;
    uint64_t modifiedTime;
    std::string author;
};
```

### Biome Definition

```cpp
/**
 * @brief Defines a climate/terrain biome
 */
struct BiomeDefinition {
    std::string id;
    std::string name;

    // Climate conditions
    float minTemperature = -50.0f;    // Celsius
    float maxTemperature = 50.0f;
    float minPrecipitation = 0.0f;    // mm/year
    float maxPrecipitation = 5000.0f;
    float minAltitude = -11000.0f;    // Meters (ocean trenches)
    float maxAltitude = 9000.0f;      // Meters (mountains)

    // Terrain parameters
    TerrainParams terrainParams;

    // Visual appearance
    std::vector<SurfaceLayer> surfaceLayers; // Ground textures, vegetation
    glm::vec3 fogColor;
    float fogDensity;

    // Asset placement rules
    std::vector<AssetPlacementRule> assetRules;
};

struct TerrainParams {
    float baseHeight = 0.0f;
    float heightScale = 1000.0f;
    int noiseOctaves = 6;
    float noisePersistence = 0.5f;
    float noiseLacunarity = 2.0f;
    float noiseScale = 1.0f;

    // Erosion
    bool enableErosion = true;
    float erosionStrength = 0.5f;
    int erosionIterations = 10;
};
```

### Climate Settings

```cpp
/**
 * @brief Global climate configuration
 */
struct ClimateSettings {
    // Temperature model
    float equatorTemperature = 30.0f;  // °C
    float poleTemperature = -40.0f;    // °C
    float temperatureLapseRate = 6.5f; // °C per 1000m altitude

    // Precipitation model
    float oceanEvaporation = 1000.0f;  // mm/year
    float windStrength = 1.0f;
    glm::vec2 dominantWindDirection = glm::vec2(1.0f, 0.0f);

    // Seasons
    bool enableSeasons = false;
    float axialTilt = 23.5f;          // Degrees
    float currentSeason = 0.0f;       // [0,1] = [winter,winter]
};
```

### LOD Settings

```cpp
/**
 * @brief Level-of-detail configuration for spherical rendering
 */
struct LODSettings {
    // Chunk subdivision
    int maxLODLevels = 20;
    float lodSwitchDistance = 2.0f;    // Multiplier for LOD transitions

    // Adaptive subdivision
    bool enableAdaptiveLOD = true;
    float subdivisionThreshold = 1.0f; // Screen-space error threshold

    // Chunk sizes
    int baseChunkResolution = 32;      // Vertices per chunk edge at LOD 0
    float baseChunkSize = 100000.0f;   // Meters per chunk at LOD 0

    // Frustum culling
    bool enableFrustumCulling = true;
    bool enableOcclusionCulling = true;

    // Caching
    int maxCachedChunks = 1000;
    int maxGeneratingChunksPerFrame = 4;
};
```

### World Chunk

```cpp
/**
 * @brief Individual terrain chunk on the sphere
 *
 * Chunks are organized in a quadtree structure for each cube face
 * (using cube-to-sphere mapping for better distribution)
 */
struct WorldChunk {
    // Position in world
    ChunkAddress address;              // Hierarchical position
    SphericalBounds bounds;            // Lat/lon bounds

    // Procedural state
    bool isGenerated = false;
    bool hasOverrides = false;

    // Terrain data
    HeightmapData heightmap;           // Vertex heights
    NormalMap normalMap;               // Pre-computed normals
    BiomeMap biomeMap;                 // Biome IDs per vertex

    // Overrides (stored separately, sparse)
    std::vector<VoxelOverride> overrides;
    std::vector<AssetPlacement> placedAssets;

    // LOD
    int lodLevel = 0;
    std::array<WorldChunk*, 4> children = {nullptr}; // Quadtree subdivision

    // Rendering
    MeshPtr mesh;                      // GPU geometry
    MaterialPtr material;
    bool needsRebuild = false;
};

struct ChunkAddress {
    int face;        // 0-5 for cube faces
    int level;       // LOD level
    uint64_t index;  // Morton code for position in quadtree
};

struct SphericalBounds {
    float minLat, maxLat;
    float minLon, maxLon;
    float minAlt, maxAlt;

    bool Contains(const SphericalCoords& point) const;
    bool Intersects(const Frustum& frustum) const;
};
```

### Override Storage

```cpp
/**
 * @brief Represents a manual edit to procedural terrain
 */
struct VoxelOverride {
    glm::ivec3 localPosition;  // Position within chunk
    float heightDelta;         // Change from procedural height
    uint8_t biomeOverride;     // 0 = no override
    uint8_t flags;             // Custom flags
};

/**
 * @brief Asset placed in the world
 */
struct AssetPlacement {
    std::string assetId;
    SphericalCoords position;
    glm::quat rotation;
    glm::vec3 scale;
    std::unordered_map<std::string, std::any> properties;
};
```

---

## PCG Visual Node Graph System

The procedural generation system uses a visual node graph similar to the existing `VisualScriptingCore` and `ShaderNodes` systems.

### PCG Node Categories

```cpp
enum class PCGNodeCategory {
    Input,          // Lat/lon, altitude, seed inputs
    Noise,          // Perlin, Simplex, Worley, etc.
    Math,           // Arithmetic operations
    Terrain,        // Terrain-specific operations
    Climate,        // Temperature, precipitation
    Biome,          // Biome selection and blending
    Output,         // Final height, biome outputs
    Data,           // Real-world data integration
    Custom          // User-defined
};
```

### Base PCG Node

```cpp
/**
 * @brief Base class for procedural generation nodes
 */
class PCGNode : public VisualScript::Node {
public:
    PCGNode(const std::string& typeId, const std::string& displayName);

    // Execute for a specific world position
    virtual void EvaluateAt(const SphericalCoords& position,
                           PCGContext& context) = 0;

    // Can this node be evaluated in parallel?
    virtual bool IsThreadSafe() const { return true; }

    // Preview generation (for editor)
    virtual Texture2DPtr GeneratePreview(int resolution = 256);

protected:
    PCGNodeCategory GetPCGCategory() const;
};
```

### Standard PCG Nodes

#### Input Nodes

```cpp
/**
 * @brief Provides latitude as input
 */
class LatitudeInputNode : public PCGNode {
    // Output: latitude in degrees [-90, 90]
};

/**
 * @brief Provides longitude as input
 */
class LongitudeInputNode : public PCGNode {
    // Output: longitude in degrees [-180, 180]
};

/**
 * @brief Provides altitude input
 */
class AltitudeInputNode : public PCGNode {
    // Output: altitude in meters
};

/**
 * @brief World seed
 */
class SeedInputNode : public PCGNode {
    // Output: world random seed
};
```

#### Noise Nodes

```cpp
/**
 * @brief 3D Perlin noise sampled on sphere
 */
class SphericalPerlinNode : public PCGNode {
public:
    // Parameters
    float scale = 1.0f;
    int octaves = 4;
    float persistence = 0.5f;
    float lacunarity = 2.0f;

    void EvaluateAt(const SphericalCoords& position, PCGContext& context) override;
};

/**
 * @brief Simplex noise (faster alternative)
 */
class SphericalSimplexNode : public PCGNode {
    // Similar to Perlin but optimized
};

/**
 * @brief Worley/Cellular noise for cell-like patterns
 */
class SphericalWorleyNode : public PCGNode {
    // Good for craters, tile patterns
};

/**
 * @brief Ridged multifractal for mountain ranges
 */
class RidgedNoiseNode : public PCGNode {
    // Excellent for mountain ranges
};

/**
 * @brief Domain warping for realistic terrain
 */
class DomainWarpNode : public PCGNode {
    // Distorts noise coordinates for organic shapes
};
```

#### Terrain Nodes

```cpp
/**
 * @brief Generates continental distribution
 */
class ContinentNode : public PCGNode {
public:
    float oceanLevel = 0.0f;
    float continentScale = 5000000.0f; // ~5000km
    float shelfWidth = 100000.0f;      // Continental shelf

    // Output: height in meters
};

/**
 * @brief Simulates tectonic plates
 */
class TectonicPlateNode : public PCGNode {
public:
    int plateCount = 12;
    float mountainHeight = 5000.0f;
    float trenchDepth = -8000.0f;
};

/**
 * @brief Erosion simulation
 */
class ErosionNode : public PCGNode {
public:
    int iterations = 100;
    float erosionRate = 0.01f;
    float sedimentCapacity = 4.0f;
    float evaporationRate = 0.01f;
};

/**
 * @brief Terrace generator (for stepped terrain)
 */
class TerraceNode : public PCGNode {
public:
    float terraceHeight = 100.0f;
    float smoothness = 0.1f;
};
```

#### Climate Nodes

```cpp
/**
 * @brief Calculates temperature based on latitude and altitude
 */
class TemperatureNode : public PCGNode {
public:
    float equatorTemp = 30.0f;
    float poleTemp = -40.0f;
    float lapseRate = 6.5f; // °C per 1000m
};

/**
 * @brief Calculates precipitation
 */
class PrecipitationNode : public PCGNode {
public:
    // Based on temperature, altitude, wind patterns
    float baseRainfall = 1000.0f;
    glm::vec2 windDirection = glm::vec2(1.0f, 0.0f);
};

/**
 * @brief Humidity calculation
 */
class HumidityNode : public PCGNode {
    // Based on proximity to water, temperature, altitude
};
```

#### Biome Nodes

```cpp
/**
 * @brief Selects biome based on climate
 */
class BiomeSelectorNode : public PCGNode {
public:
    struct BiomeThreshold {
        std::string biomeId;
        float minTemp, maxTemp;
        float minPrecip, maxPrecip;
        float minAlt, maxAlt;
        float priority;
    };

    std::vector<BiomeThreshold> biomes;
};

/**
 * @brief Blends between biomes smoothly
 */
class BiomeBlendNode : public PCGNode {
public:
    float blendDistance = 1000.0f; // Meters
};
```

#### Data Integration Nodes

```cpp
/**
 * @brief Loads real-world elevation data (SRTM, etc.)
 */
class RealWorldElevationNode : public PCGNode {
public:
    std::string datasetPath;
    float scale = 1.0f;
    bool interpolate = true;
};

/**
 * @brief Loads real-world climate data
 */
class RealWorldClimateNode : public PCGNode {
public:
    std::string temperatureDataset;
    std::string precipitationDataset;
};

/**
 * @brief OpenStreetMap data integration
 */
class OSMDataNode : public PCGNode {
public:
    std::string osmFilePath;
    std::vector<std::string> featureFilters; // roads, buildings, etc.
};
```

### PCG Context

```cpp
/**
 * @brief Execution context for PCG evaluation
 */
class PCGContext {
public:
    // World reference
    const SphericalWorld* world;

    // Current evaluation position
    SphericalCoords position;

    // Random number generator (seeded per position)
    std::mt19937 rng;

    // Intermediate values
    std::unordered_map<std::string, float> floatValues;
    std::unordered_map<std::string, glm::vec3> vectorValues;
    std::unordered_map<std::string, std::string> stringValues;

    // Cache for expensive operations
    std::unordered_map<std::string, std::any> cache;

    // Get value from port
    template<typename T>
    T GetValue(const std::string& portName, const T& defaultValue = T{});

    // Set output value
    template<typename T>
    void SetValue(const std::string& portName, const T& value);
};
```

### PCG Graph Execution

```cpp
/**
 * @brief PCG graph executor
 */
class PCGGraphExecutor {
public:
    PCGGraphExecutor(const VisualScript::Graph* graph);

    // Evaluate graph at a specific position
    float EvaluateHeight(const SphericalCoords& position);
    std::string EvaluateBiome(const SphericalCoords& position);

    // Batch evaluation for chunk generation
    void EvaluateChunk(WorldChunk* chunk,
                      HeightmapData& outHeightmap,
                      BiomeMap& outBiomeMap);

    // Async evaluation
    std::future<HeightmapData> EvaluateChunkAsync(WorldChunk* chunk);

private:
    const VisualScript::Graph* m_graph;
    std::vector<PCGNode*> m_sortedNodes; // Topologically sorted
};
```

---

## Persistence Layer

The system supports two persistence backends: **Firebase** (cloud-based, multiplayer) and **SQLite** (local, single-player).

### Persistence Mode

```cpp
enum class PersistenceMode {
    None,           // No persistence (fully procedural)
    SQLite,         // Local SQLite database
    Firebase        // Cloud Firebase Realtime Database
};
```

### Firebase Schema

For cloud-based persistence with real-time sync:

```
worlds/
  {worldId}/
    metadata/
      name: "Earth"
      radius: 6371000
      seed: 42
      created: timestamp
      modified: timestamp
      author: "userId"

    pcg/
      graph: {serialized JSON graph}
      version: 1

    overrides/
      chunks/
        {chunkId}/
          address: "face:0,level:5,index:1234"
          bounds: {minLat, maxLat, minLon, maxLon}
          voxels/
            {voxelId}: {localPos, heightDelta, biomeOverride}
          assets/
            {assetId}: {assetId, position, rotation, scale, properties}
          modified: timestamp
          author: "userId"

      regions/
        {regionId}/
          bounds: {minLat, maxLat, minLon, maxLon}
          description: "Custom city area"
          chunks: [chunkId1, chunkId2, ...]
```

### SQLite Schema

For local persistence:

```sql
-- World metadata
CREATE TABLE worlds (
    world_id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    radius REAL NOT NULL,
    gravity REAL,
    seed INTEGER,
    pcg_graph TEXT,  -- JSON serialized graph
    climate_settings TEXT,  -- JSON
    lod_settings TEXT,  -- JSON
    created_time INTEGER,
    modified_time INTEGER,
    author TEXT
);

-- Chunk overrides
CREATE TABLE chunk_overrides (
    chunk_id TEXT PRIMARY KEY,
    world_id TEXT NOT NULL,
    face INTEGER NOT NULL,
    level INTEGER NOT NULL,
    index INTEGER NOT NULL,
    min_lat REAL,
    max_lat REAL,
    min_lon REAL,
    max_lon REAL,
    modified_time INTEGER,
    FOREIGN KEY (world_id) REFERENCES worlds(world_id)
);

CREATE INDEX idx_chunk_world ON chunk_overrides(world_id);
CREATE INDEX idx_chunk_address ON chunk_overrides(face, level, index);

-- Voxel overrides (sparse storage)
CREATE TABLE voxel_overrides (
    override_id INTEGER PRIMARY KEY AUTOINCREMENT,
    chunk_id TEXT NOT NULL,
    local_x INTEGER NOT NULL,
    local_y INTEGER NOT NULL,
    local_z INTEGER NOT NULL,
    height_delta REAL,
    biome_override INTEGER,
    flags INTEGER,
    FOREIGN KEY (chunk_id) REFERENCES chunk_overrides(chunk_id)
);

CREATE INDEX idx_voxel_chunk ON voxel_overrides(chunk_id);

-- Asset placements
CREATE TABLE asset_placements (
    placement_id TEXT PRIMARY KEY,
    chunk_id TEXT NOT NULL,
    asset_id TEXT NOT NULL,
    lat REAL NOT NULL,
    lon REAL NOT NULL,
    alt REAL NOT NULL,
    rotation_x REAL,
    rotation_y REAL,
    rotation_z REAL,
    rotation_w REAL,
    scale_x REAL,
    scale_y REAL,
    scale_z REAL,
    properties TEXT,  -- JSON for custom properties
    FOREIGN KEY (chunk_id) REFERENCES chunk_overrides(chunk_id)
);

CREATE INDEX idx_asset_chunk ON asset_placements(chunk_id);

-- Regions (named collections of chunks)
CREATE TABLE regions (
    region_id TEXT PRIMARY KEY,
    world_id TEXT NOT NULL,
    name TEXT NOT NULL,
    description TEXT,
    min_lat REAL,
    max_lat REAL,
    min_lon REAL,
    max_lon REAL,
    FOREIGN KEY (world_id) REFERENCES worlds(world_id)
);

CREATE TABLE region_chunks (
    region_id TEXT NOT NULL,
    chunk_id TEXT NOT NULL,
    PRIMARY KEY (region_id, chunk_id),
    FOREIGN KEY (region_id) REFERENCES regions(region_id),
    FOREIGN KEY (chunk_id) REFERENCES chunk_overrides(chunk_id)
);
```

### Persistence Manager

```cpp
/**
 * @brief Unified interface for world persistence
 */
class WorldPersistence {
public:
    static WorldPersistence& Instance();

    void Initialize(PersistenceMode mode, const std::string& connectionString);
    void Shutdown();

    // World operations
    void SaveWorld(const SphericalWorld& world);
    std::shared_ptr<SphericalWorld> LoadWorld(const std::string& worldId);
    void DeleteWorld(const std::string& worldId);
    std::vector<WorldMetadata> ListWorlds();

    // Chunk operations
    void SaveChunkOverrides(const std::string& worldId, const WorldChunk& chunk);
    void LoadChunkOverrides(const std::string& worldId, WorldChunk& chunk);
    bool HasChunkOverrides(const std::string& worldId, const ChunkAddress& address);

    // Batch operations for efficiency
    void SaveChunkBatch(const std::string& worldId,
                       const std::vector<WorldChunk*>& chunks);
    void LoadChunkBatch(const std::string& worldId,
                       const std::vector<ChunkAddress>& addresses,
                       std::vector<WorldChunk*>& outChunks);

    // Asset operations
    void SaveAssetPlacement(const std::string& worldId,
                          const AssetPlacement& placement);
    void DeleteAssetPlacement(const std::string& worldId,
                            const std::string& placementId);
    std::vector<AssetPlacement> LoadAssetsInRegion(const std::string& worldId,
                                                   const SphericalBounds& bounds);

    // Region operations
    void SaveRegion(const std::string& worldId, const Region& region);
    std::vector<Region> LoadRegions(const std::string& worldId);

    // Real-time sync (Firebase only)
    void SubscribeToChunkChanges(const std::string& worldId,
                                const ChunkAddress& address,
                                std::function<void(const WorldChunk&)> callback);
    void UnsubscribeFromChunkChanges(uint64_t subscriptionId);

private:
    PersistenceMode m_mode;
    std::unique_ptr<IPersistenceBackend> m_backend;
};

/**
 * @brief Backend interface
 */
class IPersistenceBackend {
public:
    virtual ~IPersistenceBackend() = default;

    virtual void SaveWorld(const SphericalWorld& world) = 0;
    virtual std::shared_ptr<SphericalWorld> LoadWorld(const std::string& worldId) = 0;
    // ... etc
};

class SQLitePersistence : public IPersistenceBackend {
    // Implementation using SQLite
};

class FirebasePersistence : public IPersistenceBackend {
    // Implementation using Firebase (extends existing FirebaseClient)
};
```

### Delta Compression

Only changes from procedural generation are stored:

```cpp
/**
 * @brief Computes delta between procedural and edited terrain
 */
class DeltaCompressor {
public:
    // Compute what needs to be saved
    static std::vector<VoxelOverride> ComputeDeltas(
        const HeightmapData& procedural,
        const HeightmapData& edited,
        float threshold = 0.01f  // Minimum change to record
    );

    // Apply deltas to procedural terrain
    static void ApplyDeltas(
        HeightmapData& procedural,
        const std::vector<VoxelOverride>& deltas
    );
};
```

---

## Editor Integration

### World Editor Panel

```cpp
/**
 * @brief Main editor panel for spherical worlds
 */
class SphericalWorldEditorPanel : public EditorPanel {
public:
    SphericalWorldEditorPanel();

protected:
    void OnRender() override;
    void OnRenderToolbar() override;
    void OnRenderMenuBar() override;

private:
    // UI sections
    void RenderWorldSettings();
    void RenderPCGGraphEditor();
    void RenderBiomeList();
    void RenderClimateSettings();
    void RenderTerrainPainting();
    void RenderAssetPalette();
    void RenderPreviewWindow();

    // Tools
    enum class EditMode {
        View,           // Camera navigation only
        Sculpt,         // Terrain height editing
        Paint,          // Biome painting
        Smooth,         // Smoothing tool
        Flatten,        // Flatten to altitude
        PlaceAssets,    // Asset placement
        SelectRegion    // Region selection
    };

    EditMode m_editMode = EditMode::View;

    // State
    std::shared_ptr<SphericalWorld> m_currentWorld;
    PCGGraphExecutor m_graphExecutor;

    // Preview rendering
    std::unique_ptr<SphericalWorldRenderer> m_renderer;
    Texture2DPtr m_previewTexture;
};
```

### PCG Graph Editor

```cpp
/**
 * @brief Visual node editor for PCG graphs
 */
class PCGGraphEditorPanel : public EditorPanel {
public:
    PCGGraphEditorPanel();

    void SetGraph(VisualScript::GraphPtr graph);

protected:
    void OnRender() override;

private:
    // Node palette
    void RenderNodePalette();

    // Graph canvas
    void RenderGraphCanvas();

    // Property inspector
    void RenderNodeProperties();

    // Preview generation
    void RenderPreviewPanel();
    void GeneratePreview();

    VisualScript::GraphPtr m_graph;
    PCGNode* m_selectedNode = nullptr;

    // Preview state
    Texture2DPtr m_heightPreview;
    Texture2DPtr m_biomePreview;
    bool m_needsPreviewUpdate = true;
};
```

### Content Browser Integration

When a `.nmap` file is clicked in the content browser:

```cpp
/**
 * @brief Handler for .nmap files in content browser
 */
class NMapAssetHandler : public IAssetHandler {
public:
    bool CanHandle(const std::string& extension) override {
        return extension == ".nmap";
    }

    void OnDoubleClick(const std::string& path) override {
        // Open in PCG graph editor
        auto graph = LoadNMapFile(path);

        auto editor = std::make_shared<PCGGraphEditorPanel>();
        editor->SetGraph(graph);

        PanelRegistry::Instance().Register("pcg_editor_" + path, editor);
        editor->Show();
    }

    Texture2DPtr GetThumbnail(const std::string& path) override {
        // Generate thumbnail preview
        auto graph = LoadNMapFile(path);
        return GenerateGraphThumbnail(graph, 256, 256);
    }
};
```

### Terrain Editing Tools

```cpp
/**
 * @brief Terrain sculpting tools
 */
class TerrainSculptTool {
public:
    struct BrushSettings {
        float radius = 100.0f;      // Meters
        float strength = 1.0f;      // [0,1]
        float hardness = 0.5f;      // Edge falloff
        bool smoothMode = false;
        bool additive = true;       // Add or subtract
    };

    void BeginStroke(const SphericalCoords& position);
    void UpdateStroke(const SphericalCoords& position);
    void EndStroke();

    // Apply brush to terrain
    void ApplyBrush(WorldChunk* chunk, const SphericalCoords& position);

private:
    BrushSettings m_settings;
    std::vector<VoxelOverride> m_strokeOverrides;
};

/**
 * @brief Biome painting tool
 */
class BiomePaintTool {
public:
    void SetBiome(const std::string& biomeId);
    void Paint(WorldChunk* chunk, const SphericalCoords& position, float radius);
};
```

### Asset Placement

```cpp
/**
 * @brief Asset placement tool
 */
class AssetPlacementTool {
public:
    void SetAsset(const std::string& assetId);

    // Placement modes
    enum class PlacementMode {
        Single,         // One at a time
        Scatter,        // Random scatter in radius
        Grid,           // Regular grid pattern
        Path            // Along a path
    };

    void SetPlacementMode(PlacementMode mode);

    // Place asset at position
    AssetPlacement PlaceAsset(const SphericalCoords& position,
                             const glm::quat& rotation = glm::quat{},
                             const glm::vec3& scale = glm::vec3(1.0f));

    // Procedural placement
    void ScatterInRadius(const SphericalCoords& center,
                        float radius,
                        int count,
                        std::function<bool(const SphericalCoords&)> filter = nullptr);
};
```

---

## File Format

### .nmap File Format

World map files use JSON format with `.nmap` extension:

```json
{
  "version": "1.0",
  "type": "spherical_world",

  "metadata": {
    "name": "Earth-like Planet",
    "description": "A procedurally generated Earth-like world",
    "author": "User123",
    "created": "2025-12-02T10:00:00Z",
    "modified": "2025-12-02T15:30:00Z"
  },

  "world": {
    "radius": 6371000.0,
    "gravity": 9.81,
    "atmosphereHeight": 100000.0,
    "seed": 42
  },

  "pcg": {
    "graph": {
      "nodes": [
        {
          "id": "node_1",
          "type": "LatitudeInput",
          "position": [100, 100]
        },
        {
          "id": "node_2",
          "type": "SphericalPerlin",
          "position": [300, 100],
          "parameters": {
            "scale": 5000000.0,
            "octaves": 6,
            "persistence": 0.5,
            "lacunarity": 2.0
          }
        },
        {
          "id": "node_3",
          "type": "HeightOutput",
          "position": [500, 100]
        }
      ],
      "connections": [
        {
          "source": "node_1",
          "sourcePort": "output",
          "target": "node_2",
          "targetPort": "latitude"
        },
        {
          "source": "node_2",
          "sourcePort": "output",
          "target": "node_3",
          "targetPort": "height"
        }
      ]
    }
  },

  "biomes": [
    {
      "id": "ocean",
      "name": "Ocean",
      "minTemperature": -2.0,
      "maxTemperature": 35.0,
      "minAltitude": -11000.0,
      "maxAltitude": 0.0,
      "terrainParams": {
        "baseHeight": -1000.0,
        "heightScale": 500.0,
        "noiseOctaves": 4
      },
      "surfaceLayers": [
        {
          "material": "water",
          "depth": 1000.0
        }
      ]
    },
    {
      "id": "grassland",
      "name": "Grassland",
      "minTemperature": 0.0,
      "maxTemperature": 30.0,
      "minPrecipitation": 300.0,
      "maxPrecipitation": 1500.0,
      "minAltitude": 0.0,
      "maxAltitude": 2000.0,
      "assetRules": [
        {
          "assetId": "tree_oak",
          "density": 0.1,
          "minScale": 0.8,
          "maxScale": 1.2
        }
      ]
    }
  ],

  "climate": {
    "equatorTemperature": 30.0,
    "poleTemperature": -40.0,
    "temperatureLapseRate": 6.5,
    "oceanEvaporation": 1000.0,
    "enableSeasons": false
  },

  "lod": {
    "maxLODLevels": 20,
    "lodSwitchDistance": 2.0,
    "baseChunkResolution": 32,
    "baseChunkSize": 100000.0
  },

  "persistence": {
    "mode": "sqlite",
    "connectionString": "worlds/earth.db",
    "autoSave": true,
    "autoSaveInterval": 300.0
  }
}
```

### File Operations

```cpp
/**
 * @brief .nmap file I/O
 */
class NMapFile {
public:
    // Load world from .nmap file
    static std::shared_ptr<SphericalWorld> Load(const std::string& path);

    // Save world to .nmap file
    static void Save(const std::string& path, const SphericalWorld& world);

    // Load just the PCG graph
    static VisualScript::GraphPtr LoadGraph(const std::string& path);

    // Save just the PCG graph
    static void SaveGraph(const std::string& path,
                         const VisualScript::Graph& graph);

    // Validation
    static bool Validate(const std::string& path,
                        std::vector<std::string>& errors);
};
```

---

## Rendering Pipeline

### Spherical LOD Renderer

```cpp
/**
 * @brief Renders spherical worlds with adaptive LOD
 */
class SphericalWorldRenderer {
public:
    void Initialize(const SphericalWorld& world);
    void Shutdown();

    void Update(float deltaTime, const Camera& camera);
    void Render(const Camera& camera);

    // LOD management
    void UpdateLOD(const Camera& camera);
    void GenerateVisibleChunks();

    // Culling
    void FrustumCull(const Camera& camera);
    void OcclusionCull(const Camera& camera);

private:
    const SphericalWorld* m_world;

    // Chunk management
    std::unordered_map<ChunkAddress, std::unique_ptr<WorldChunk>> m_chunks;
    std::vector<WorldChunk*> m_visibleChunks;

    // Generation queue
    std::queue<ChunkAddress> m_generationQueue;
    std::vector<std::future<HeightmapData>> m_pendingGeneration;

    // Rendering
    ShaderPtr m_terrainShader;
    std::vector<DrawCall> m_drawCalls;
};
```

### Cube-to-Sphere Mapping

For better vertex distribution, use cube-to-sphere mapping:

```cpp
/**
 * @brief Maps cube face coordinates to sphere
 *
 * Divides sphere into 6 cube faces for more uniform subdivision
 */
glm::vec3 CubeToSphere(int face, float u, float v, float radius) {
    // u, v in [-1, 1]
    glm::vec3 point;

    switch(face) {
        case 0: point = glm::vec3(1, u, v); break;  // +X
        case 1: point = glm::vec3(-1, u, v); break; // -X
        case 2: point = glm::vec3(u, 1, v); break;  // +Y
        case 3: point = glm::vec3(u, -1, v); break; // -Y
        case 4: point = glm::vec3(u, v, 1); break;  // +Z
        case 5: point = glm::vec3(u, v, -1); break; // -Z
    }

    return glm::normalize(point) * radius;
}
```

### Atmosphere Rendering

```cpp
/**
 * @brief Atmospheric scattering shader
 */
struct AtmosphereParams {
    glm::vec3 sunDirection;
    glm::vec3 rayleighScattering;  // Sky color (blue)
    glm::vec3 mieScattering;       // Haze color (white)
    float atmosphereHeight;
    float planetRadius;
};

// In fragment shader:
vec3 ComputeAtmosphere(vec3 rayOrigin, vec3 rayDir, AtmosphereParams params) {
    // Raymarch through atmosphere
    // Sample scattering at each step
    // Return final color
}
```

---

## Performance Considerations

### Chunk Generation

- **Async Generation**: Generate chunks on worker threads
- **Incremental Updates**: Generate at most N chunks per frame
- **Caching**: Keep generated chunks in memory/disk cache
- **Priority Queue**: Prioritize chunks by distance and visibility

### Memory Management

```cpp
/**
 * @brief Chunk cache with LRU eviction
 */
class ChunkCache {
public:
    ChunkCache(size_t maxChunks);

    WorldChunk* Get(const ChunkAddress& address);
    void Put(const ChunkAddress& address, std::unique_ptr<WorldChunk> chunk);

    void EvictLRU();
    void Clear();

private:
    size_t m_maxChunks;
    std::unordered_map<ChunkAddress, std::unique_ptr<WorldChunk>> m_chunks;
    std::list<ChunkAddress> m_lruList;
};
```

### Streaming

```cpp
/**
 * @brief Streams world data from disk/network
 */
class WorldStreamer {
public:
    void SetViewPosition(const SphericalCoords& position);
    void Update(float deltaTime);

    // Returns chunks ready for rendering
    std::vector<WorldChunk*> GetLoadedChunks(const SphericalBounds& bounds);

private:
    void LoadChunksAround(const SphericalCoords& position, float radius);
    void UnloadDistantChunks(float maxDistance);

    std::unordered_map<ChunkAddress, ChunkLoadState> m_chunkStates;
};
```

### GPU Optimization

- **Instancing**: Instance vegetation and rocks
- **GPU Tessellation**: Use tessellation shaders for LOD
- **Clipmaps**: Ring-based terrain clipmaps for seamless LOD
- **Compressed Textures**: Use BCN/ASTC compression

### Network Optimization (Firebase)

- **Batching**: Batch multiple edits before syncing
- **Delta Compression**: Only send changes, not full chunks
- **Spatial Queries**: Firebase queries by lat/lon bounds
- **Throttling**: Limit sync frequency to avoid quota

```cpp
// Example Firebase query for chunks in view
void LoadChunksInView(const SphericalBounds& bounds,
                     std::function<void(const WorldChunk&)> callback) {
    firebase->Query("overrides/chunks")
        .OrderBy("min_lat")
        .StartAt(bounds.minLat)
        .EndAt(bounds.maxLat)
        .OnValue([=](const json& data) {
            // Filter by longitude
            for (auto& chunk : data) {
                if (chunk["min_lon"] >= bounds.minLon &&
                    chunk["max_lon"] <= bounds.maxLon) {
                    callback(chunk);
                }
            }
        });
}
```

---

## Integration Checklist

### Implementation Phases

**Phase 1: Core Data Structures**
- [ ] SphericalWorld, WorldChunk, ChunkAddress
- [ ] Coordinate conversion functions
- [ ] Cube-to-sphere mapping
- [ ] Basic chunk generation

**Phase 2: PCG System**
- [ ] PCG node base classes
- [ ] Standard noise nodes
- [ ] Terrain/climate nodes
- [ ] PCG graph executor
- [ ] Preview generation

**Phase 3: Editor UI**
- [ ] SphericalWorldEditorPanel
- [ ] PCGGraphEditorPanel
- [ ] Content browser integration (.nmap files)
- [ ] Terrain editing tools
- [ ] Asset placement tools

**Phase 4: Persistence**
- [ ] SQLite backend
- [ ] Firebase backend
- [ ] Delta compression
- [ ] Chunk streaming
- [ ] Auto-save system

**Phase 5: Rendering**
- [ ] LOD system
- [ ] Frustum culling
- [ ] Chunk mesh generation
- [ ] Atmosphere rendering
- [ ] Water rendering

**Phase 6: Optimization**
- [ ] Async chunk generation
- [ ] Chunk caching
- [ ] GPU tessellation
- [ ] Network throttling
- [ ] Profiling and tuning

---

## Example Workflow

### Creating a New World

1. **Create World**: `File > New World`
2. **Configure Settings**: Set radius, gravity, seed
3. **Design PCG Graph**: Open PCG editor, add nodes
4. **Preview**: Generate preview to test parameters
5. **Save**: Save as `.nmap` file

### Editing Terrain

1. **Load World**: Open `.nmap` in content browser
2. **Navigate**: Fly camera to area of interest
3. **Select Tool**: Choose sculpt/paint/smooth
4. **Edit**: Click and drag to modify terrain
5. **Save**: Changes auto-saved as deltas to SQLite/Firebase

### Placing Assets

1. **Open Asset Palette**: `Tools > Asset Placement`
2. **Select Asset**: Choose tree, rock, building, etc.
3. **Place**: Click on terrain to place
4. **Adjust**: Rotate, scale, configure properties
5. **Procedural Scatter**: Use scatter tool for forests, rocks

### Real-World Data Integration

1. **Import Elevation**: Add `RealWorldElevationNode` to PCG graph
2. **Load Dataset**: Point to SRTM/ASTER data
3. **Blend**: Mix with procedural noise for detail
4. **Import Climate**: Add temperature/precipitation data
5. **Generate Biomes**: Automatic biome assignment

---

## Future Enhancements

- **Erosion Simulation**: Hydraulic and thermal erosion
- **Weather System**: Dynamic weather based on climate
- **Ocean Simulation**: Wave simulation, tides
- **Cave Generation**: Underground cave systems
- **Multiplayer Editing**: Real-time collaborative editing
- **Version Control**: Git-like versioning for worlds
- **Cloud Rendering**: Server-side chunk rendering for streaming
- **VR Support**: VR editing and exploration

---

## References

### Mathematical References
- **Spherical Coordinates**: Standard geographic coordinate system
- **Cube Mapping**: [Cube-to-sphere projection](https://mathproofs.blogspot.com/2005/07/mapping-cube-to-sphere.html)
- **Perlin Noise**: Ken Perlin's improved noise (2002)

### Integration with Existing Systems
- **VisualScriptingCore**: Base for PCG node graph
- **ShaderNodes**: Similar visual node paradigm
- **FirebasePersistence**: Extends existing Firebase integration
- **EditorPanel**: Follows existing editor panel pattern
- **NoiseGenerator**: Extends existing noise functions

### Existing Engine Files
- `engine/scripting/visual/VisualScriptingCore.hpp` - Visual scripting foundation
- `engine/materials/ShaderNodes.hpp` - Node graph reference
- `engine/networking/FirebasePersistence.hpp` - Firebase persistence pattern
- `engine/ui/EditorPanel.hpp` - Editor panel base class
- `engine/terrain/NoiseGenerator.hpp` - Noise generation utilities
