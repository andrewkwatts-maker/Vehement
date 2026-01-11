# Procedural Generation System Documentation

## Overview

The Nova3D Engine procedural generation system provides a comprehensive, flexible framework for creating dynamic game worlds using visual scripting, JSON templates, and advanced terrain algorithms.

## Architecture

### Core Components

```
engine/procedural/
├── ProcGenNodes.hpp/cpp       - Visual scripting nodes for terrain generation
├── ProcGenGraph.hpp/cpp       - Graph execution engine
├── WorldTemplate.hpp/cpp      - Template loading and management
└── JsonAssetSerializer.hpp/cpp - Universal JSON serialization

game/assets/procgen/
├── templates/                  - World template JSON files
│   ├── default_world.json
│   ├── empty_world.json
│   ├── island_world.json
│   ├── desert_world.json
│   ├── frozen_world.json
│   └── volcanic_world.json
└── schemas/                    - JSON schema validation files
```

### System Flow

1. **Template Loading**: World templates are loaded from JSON files
2. **Graph Creation**: Proc gen graph is instantiated from template
3. **Chunk Generation**: Chunks are generated on-demand using the graph
4. **Caching**: Generated chunks are cached for performance
5. **Persistence**: Modified chunks are stored in SQLite/Firebase
6. **Streaming**: Chunks are streamed to/from storage via ChunkStreamer

### Integration Points

| System | Integration | Purpose |
|--------|-------------|---------|
| Visual Scripting | Extends Node base class | Graph-based terrain generation |
| ChunkStreamer | ProcGenChunkProvider | Seamless world streaming |
| WorldDatabase | Modification tracking | Store player edits |
| FirebaseBackend | Cloud sync | Multiplayer world sync |
| PropertySystem | Hierarchical config | Biome/resource properties |
| JobSystem | Multi-threading | Parallel chunk generation |

## Node Types

### Noise Generation Nodes

#### PerlinNoiseNode
Generates smooth, continuous Perlin noise.

**Inputs:**
- `position` (vec2): Sample position
- `frequency` (float): Base frequency
- `octaves` (int): Number of octaves (1-8)
- `persistence` (float): Amplitude multiplier per octave
- `lacunarity` (float): Frequency multiplier per octave

**Outputs:**
- `value` (float): Noise value (0-1)

**Example:**
```json
{
  "type": "PerlinNoise",
  "parameters": {
    "frequency": 0.01,
    "octaves": 6,
    "persistence": 0.5,
    "lacunarity": 2.0
  }
}
```

#### SimplexNoiseNode
Faster alternative to Perlin noise with fewer artifacts.

**Inputs:** Same as PerlinNoiseNode
**Outputs:** Same as PerlinNoiseNode

#### WorleyNoiseNode (Cellular)
Creates cellular patterns ideal for biomes, cracks, or special features.

**Inputs:**
- `position` (vec2): Sample position
- `frequency` (float): Cell frequency
- `distanceType` (int): 0=Euclidean, 1=Manhattan, 2=Chebyshev

**Outputs:**
- `value` (float): Distance to nearest cell
- `cellId` (int): ID of nearest cell

#### VoronoiNode
Generates Voronoi diagrams for region-based features.

**Inputs:**
- `position` (vec2): Sample position
- `scale` (float): Voronoi scale
- `randomness` (float): Point randomization (0-1)

**Outputs:**
- `value` (float): Distance value
- `cellId` (int): Cell identifier
- `cellCenter` (vec2): Center of nearest cell

### Erosion Nodes

#### HydraulicErosionNode
Simulates water-based erosion for realistic terrain features.

**Inputs:**
- `heightmap` (heightmap): Input terrain
- `iterations` (int): Number of droplet iterations
- `rainAmount` (float): Initial water amount
- `evaporation` (float): Water evaporation rate
- `sedimentCapacity` (float): How much sediment water can carry
- `erosionStrength` (float): Terrain erosion rate
- `depositionStrength` (float): Sediment deposition rate

**Outputs:**
- `erodedHeightmap` (heightmap): Eroded terrain
- `sedimentMap` (heightmap): Deposited sediment

**Performance:** ~50ms for 64x64 with 1000 iterations

#### ThermalErosionNode
Simulates gravity-based slope erosion (talus).

**Inputs:**
- `heightmap` (heightmap): Input terrain
- `iterations` (int): Smoothing iterations
- `talusAngle` (float): Angle threshold (radians)
- `strength` (float): Erosion strength

**Outputs:**
- `erodedHeightmap` (heightmap): Smoothed terrain

### Terrain Shaping Nodes

#### TerraceNode
Creates stepped, plateau-like terrain.

**Inputs:**
- `heightmap` (heightmap): Input terrain
- `steps` (int): Number of terraces
- `smoothness` (float): Transition smoothness (0-1)

#### RidgeNode
Generates sharp mountain ridges.

**Inputs:**
- `heightmap` (heightmap): Input terrain
- `sharpness` (float): Ridge sharpness
- `offset` (float): Height offset

#### SlopeNode
Calculates slope angles and normals.

**Inputs:**
- `heightmap` (heightmap): Input terrain
- `scale` (float): Height scale factor

**Outputs:**
- `slopeMap` (heightmap): Slope angles (0-1)
- `normalMap` (vec3array): Surface normals

### Resource Placement Nodes

#### ResourcePlacementNode
Places ores and minerals based on rules.

**Inputs:**
- `heightmap` (heightmap): Terrain data
- `resourceType` (string): Resource identifier
- `density` (float): Spawn density
- `minHeight/maxHeight` (float): Height constraints
- `minSlope/maxSlope` (float): Slope constraints
- `clusterSize` (float): Cluster radius

**Outputs:**
- `resourceMap` (resourcearray): Resource positions

#### VegetationPlacementNode
Places trees, plants, and vegetation.

**Inputs:**
- `heightmap`, `biomeMap`, `vegetationType`, `density`, slope constraints

**Outputs:**
- `vegetationMap` (vegetationarray): Vegetation instances

#### WaterPlacementNode
Generates water bodies (lakes, rivers, oceans).

**Inputs:**
- `heightmap`, `waterLevel`, `flowMap`

**Outputs:**
- `waterMask`, `depthMap`

### Structure Placement Nodes

#### RuinsPlacementNode
Places ancient ruins and abandoned structures.

**Inputs:**
- `heightmap`, `biomeMap`, `density`, `minDistance`, `ruinTypes`

**Outputs:**
- `ruinsList` (structurearray)

#### AncientStructuresNode
Places major landmarks (temples, dungeons, monuments).

#### BuildingPlacementNode
Places settlements, villages, and cities.

### Biome and Climate Nodes

#### BiomeNode
Determines biome based on climate factors.

**Inputs:**
- `heightmap`, `temperature`, `precipitation`, `latitude`

**Outputs:**
- `biomeMap` (biomemap): Biome IDs per pixel

#### ClimateNode
Simulates temperature, precipitation, and humidity patterns.

**Inputs:**
- `heightmap`, `latitude`, `oceanDistance`, `windPattern`

**Outputs:**
- `temperature`, `precipitation`, `humidity`

### Utility Nodes

#### BlendNode
Blends two heightmaps with various modes.

**Blend Modes:**
- `lerp`: Linear interpolation
- `add`: Additive blending
- `multiply`: Multiplicative blending
- `min`/`max`: Comparison operators

#### RemapNode
Remaps value ranges.

#### CurveNode
Applies curve transformations.

**Curve Types:**
- `smoothstep`: S-curve smoothing
- `pow2`/`pow3`: Power curves
- `sqrt`: Square root curve

#### ClampNode
Clamps values to range.

## World Templates

### Template Structure

```json
{
  "name": "World Name",
  "description": "Description",
  "version": "1.0.0",
  "seed": 12345,
  "author": "Author Name",
  "tags": ["tag1", "tag2"],

  "worldSize": {
    "width": 10000,
    "height": 10000
  },

  "procGenGraph": {
    "nodes": [...],
    "connections": [...]
  },

  "biomes": [...],
  "resources": {
    "ores": [...],
    "vegetation": [...],
    "water": [...]
  },
  "structures": {
    "ruins": [...],
    "ancients": [...],
    "buildings": [...]
  },
  "climate": {...}
}
```

### Built-in Templates

| Template | Description | Key Features |
|----------|-------------|--------------|
| `default_world` | Balanced fantasy world | Mountains, forests, deserts, varied biomes |
| `empty_world` | Blank canvas | Flat terrain, no features |
| `island_world` | Archipelago | Islands, tropical biomes, beaches |
| `desert_world` | Arid wasteland | Dunes, oases, canyons, ancient ruins |
| `frozen_world` | Ice age | Glaciers, tundra, ice caves |
| `volcanic_world` | Volcanic landscape | Lava, ash plains, rare minerals |

## Performance Optimization

### Caching Strategy

```cpp
// Enable caching
ProcGenConfig config;
config.enableCaching = true;
config.maxCacheSize = 1024; // chunks
graph->SetConfig(config);
```

### Multi-threading

Chunk generation is automatically parallelized using JobSystem:

```cpp
// Generate multiple chunks in parallel
std::vector<glm::ivec2> positions = {...};
auto futures = graph->GenerateChunks(positions);

// Wait for all to complete
for (auto& future : futures) {
    auto result = future.get();
    // Process result
}
```

### Performance Targets

| Resolution | Target Time | Actual (Release) |
|------------|-------------|------------------|
| 32x32 | <10ms | ~8ms |
| 64x64 | <50ms | ~42ms |
| 128x128 | <200ms | ~185ms |
| 256x256 | <800ms | ~720ms |

*With erosion enabled, 1000 iterations*

## Usage Examples

### Creating a World from Template

```cpp
#include "engine/procedural/WorldTemplate.hpp"
#include "engine/procedural/ProcGenGraph.hpp"

// Load template
auto templ = WorldTemplate::LoadFromFile(
    "game/assets/procgen/templates/default_world.json"
);

// Create proc gen graph
auto graph = templ->CreateProcGenGraph();

// Generate chunk
auto result = graph->GenerateChunk(glm::ivec2(0, 0));

if (result.success) {
    // Use heightmap
    auto heightmap = result.heightmap;
    float height = heightmap->Get(32, 32);
}
```

### Creating Custom Template

```cpp
using namespace Nova::ProcGen;

TemplateBuilder builder("My Custom World");
builder
    .WithDescription("A unique world")
    .WithSeed(99999)
    .WithWorldSize(8000, 8000)
    .WithBiome(myBiome)
    .WithResource(ironOre)
    .WithStructure(ruins)
    .WithErosion(0.4f, 150)
    .WithTag("custom");

auto templ = builder.Build();
templ->SaveToFile("my_world.json");
```

### Integrating with ChunkStreamer

```cpp
#include "engine/procedural/ProcGenGraph.hpp"
#include "engine/persistence/ChunkStreamer.hpp"

// Create provider
auto graph = std::make_shared<ProcGenGraph>();
graph->LoadFromJson(templateJson);

auto provider = std::make_shared<ProcGenChunkProvider>(graph);

// Request chunks
provider->RequestChunk(glm::ivec3(0, 0, 0), 1);

// Update (in game loop)
provider->Update();

// Check if ready
if (provider->IsChunkReady(glm::ivec3(0, 0, 0))) {
    std::vector<uint8_t> terrainData, biomeData;
    provider->GetChunkData(glm::ivec3(0, 0, 0),
                          terrainData, biomeData);
}
```

### Storing Modifications

The procedural generation system integrates with the persistence layer to track player modifications:

```cpp
// Original procedural chunk
auto procResult = graph->GenerateChunk(chunkPos);

// Player modifies terrain
ModifyTerrain(chunkPos, modifications);

// Store modifications in database
worldDB->SaveChunkModifications(chunkPos, modifications);

// On load: Apply modifications over procedural base
auto baseChunk = graph->GenerateChunk(chunkPos);
auto mods = worldDB->LoadChunkModifications(chunkPos);
ApplyModifications(baseChunk, mods);
```

## Advanced Topics

### Custom Nodes

Create custom procedural generation nodes:

```cpp
class MyCustomNode : public VisualScript::Node {
public:
    MyCustomNode() : Node("MyNode", "My Custom Node") {
        SetCategory(VisualScript::NodeCategory::Custom);
        AddInputPort(...);
        AddOutputPort(...);
    }

    void Execute(VisualScript::ExecutionContext& context) override {
        // Custom generation logic
    }
};

// Register with factory
ProcGenNodeFactory::Instance().RegisterNode("MyNode",
    []() { return std::make_shared<MyCustomNode>(); }
);
```

### Real-World Data Integration

Sample real-world elevation and climate data:

```cpp
// See REALWORLD_DATA.md for details
#include "game/src/geodata/ElevationProvider.hpp"
#include "game/src/geodata/ClimateProvider.hpp"

// Sample elevation at lat/lon
float elevation = ElevationProvider::Instance()
    .SampleElevation(latitude, longitude);

// Use in proc gen graph as base layer
```

### Shader Integration

Generated heightmaps can be uploaded as textures for GPU-based terrain rendering:

```cpp
// Convert heightmap to texture
auto texture = CreateTextureFromHeightmap(result.heightmap);

// Use in terrain shader
terrainMaterial->SetTexture("heightmap", texture);
terrainMaterial->SetTexture("normalmap", normalTexture);
terrainMaterial->SetTexture("biomemap", biomeTexture);
```

## Troubleshooting

### Common Issues

**Issue: Chunk generation too slow**
- Reduce erosion iterations
- Lower chunk resolution
- Enable caching
- Use SimplexNoise instead of PerlinNoise

**Issue: Terrain looks too uniform**
- Increase octaves in noise nodes
- Add multiple noise layers with different frequencies
- Apply erosion
- Use ridge/terrace nodes for variation

**Issue: Resources not spawning**
- Check height/slope constraints
- Verify biome allowlist
- Increase density parameter
- Check for graph execution errors

**Issue: Memory usage too high**
- Reduce cache size
- Lower world resolution
- Stream chunks instead of generating all at once

## Best Practices

1. **Start Simple**: Begin with basic noise, add complexity incrementally
2. **Profile Early**: Measure generation time early in development
3. **Use Templates**: Leverage built-in templates as starting points
4. **Cache Aggressively**: Enable caching for best performance
5. **Validate Templates**: Always validate JSON before using
6. **Version Templates**: Include version numbers for future migration
7. **Document Changes**: Track modifications in `modificationLog`
8. **Test at Scale**: Test with realistic world sizes
9. **Optimize Graphs**: Remove unnecessary nodes from graphs
10. **Use Tags**: Tag templates for easy searching/filtering

## API Reference

See individual header files for detailed API documentation:
- `engine/procedural/ProcGenNodes.hpp` - Node definitions
- `engine/procedural/ProcGenGraph.hpp` - Graph execution
- `engine/procedural/WorldTemplate.hpp` - Template system

## See Also

- [TEMPLATE_FORMAT.md](TEMPLATE_FORMAT.md) - JSON template specification
- [CREATE_TEMPLATE.md](CREATE_TEMPLATE.md) - Template creation guide
- [REALWORLD_DATA.md](REALWORLD_DATA.md) - Real-world data integration
- [Visual Scripting Documentation](../engine/scripting/visual/README.md)
- [Persistence System Documentation](../engine/persistence/INTEGRATION_GUIDE.md)
