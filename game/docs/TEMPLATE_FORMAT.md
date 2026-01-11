# World Template JSON Format Specification

## Overview

World templates are JSON files that define complete procedural worlds including terrain generation, biomes, resources, structures, and climate. This document specifies the format in detail.

## Root Structure

```json
{
  "name": string,                  // Required: Template display name
  "description": string,           // Required: Template description
  "version": string,               // Required: Semantic version (e.g., "1.0.0")
  "seed": integer,                 // Required: Random seed for generation
  "author": string,                // Optional: Template creator
  "createdDate": string,           // Optional: ISO 8601 date
  "modifiedDate": string,          // Optional: Last modification date
  "tags": [string],                // Optional: Searchable tags

  "worldSize": {...},              // Required: World dimensions
  "maxHeight": integer,            // Required: Maximum terrain height (0-255)

  "terrainScale": float,           // Optional: World scale multiplier (default: 1.0)
  "terrainAmplitude": float,       // Optional: Height amplitude (default: 100.0)
  "erosionStrength": float,        // Optional: Erosion strength (default: 0.0)
  "erosionIterations": integer,    // Optional: Erosion iterations (default: 0)

  "procGenGraph": {...},           // Required: Visual script graph definition
  "biomes": [...],                 // Required: Biome definitions (min 1)
  "resources": {...},              // Optional: Resource distributions
  "structures": {...},             // Optional: Structure placements
  "climate": {...},                // Required: Climate configuration

  "modificationLog": [...]         // Optional: Tracked modifications
}
```

## worldSize

```json
{
  "width": integer,    // World width in chunks (> 0)
  "height": integer    // World height in chunks (> 0)
}
```

**Example:**
```json
{
  "width": 10000,
  "height": 10000
}
```

## procGenGraph

Visual scripting graph for terrain generation.

```json
{
  "nodes": [
    {
      "id": string,              // Unique node identifier
      "type": string,            // Node type (e.g., "PerlinNoise")
      "position": [x, y],        // Editor position [float, float]
      "parameters": {...}        // Node-specific parameters
    }
  ],
  "connections": [
    {
      "from": "nodeId.portName",  // Source node.port
      "to": "nodeId.portName"     // Target node.port
    }
  ]
}
```

**Node Types:**
- Noise: `PerlinNoise`, `SimplexNoise`, `WorleyNoise`, `Voronoi`
- Erosion: `HydraulicErosion`, `ThermalErosion`
- Shaping: `Terrace`, `Ridge`, `Slope`
- Resources: `ResourcePlacement`, `VegetationPlacement`, `WaterPlacement`
- Structures: `RuinsPlacement`, `AncientStructures`, `BuildingPlacement`
- Climate: `Biome`, `Climate`
- Utility: `Blend`, `Remap`, `Curve`, `Clamp`

**Example:**
```json
{
  "nodes": [
    {
      "id": "base_noise",
      "type": "PerlinNoise",
      "position": [100, 100],
      "parameters": {
        "frequency": 0.01,
        "octaves": 6,
        "persistence": 0.5,
        "lacunarity": 2.0
      }
    },
    {
      "id": "remap",
      "type": "Remap",
      "position": [300, 100],
      "parameters": {
        "inputMin": 0.0,
        "inputMax": 1.0,
        "outputMin": 0.0,
        "outputMax": 255.0
      }
    }
  ],
  "connections": [
    {
      "from": "base_noise.value",
      "to": "remap.input"
    }
  ]
}
```

## biomes

Array of biome definitions. At least one biome is required.

```json
{
  "id": integer,                    // Unique biome ID (0-255)
  "name": string,                   // Biome name
  "description": string,            // Biome description
  "color": [r, g, b],              // Display color [float, float, float] (0-1)

  // Climate constraints
  "minTemperature": float,          // Minimum temperature (Celsius)
  "maxTemperature": float,          // Maximum temperature (Celsius)
  "minPrecipitation": float,        // Minimum rainfall (mm/year)
  "maxPrecipitation": float,        // Maximum rainfall (mm/year)
  "minElevation": float,            // Minimum elevation (0-maxHeight)
  "maxElevation": float,            // Maximum elevation (0-maxHeight)

  // Vegetation
  "treeTypes": [string],            // Tree types that spawn
  "plantTypes": [string],           // Plant types that spawn
  "vegetationDensity": float,       // Vegetation density (0-1)

  // Resources
  "oreTypes": [string],             // Ore types present
  "oreDensities": [float]           // Corresponding densities (0-1)
}
```

**Example:**
```json
{
  "id": 2,
  "name": "Forest",
  "description": "Dense woodland",
  "color": [0.2, 0.5, 0.2],
  "minTemperature": -5.0,
  "maxTemperature": 20.0,
  "minPrecipitation": 800.0,
  "maxPrecipitation": 2000.0,
  "minElevation": 50.0,
  "maxElevation": 150.0,
  "treeTypes": ["oak", "pine", "spruce"],
  "plantTypes": ["ferns", "mushrooms"],
  "vegetationDensity": 0.9,
  "oreTypes": ["iron", "gold"],
  "oreDensities": [0.15, 0.05]
}
```

## resources

Optional resource distributions.

```json
{
  "ores": [ResourceDefinition],       // Ore/mineral deposits
  "vegetation": [ResourceDefinition], // Trees, plants, grass
  "water": [ResourceDefinition]       // Lakes, rivers, ponds
}
```

### ResourceDefinition

```json
{
  "resourceType": string,       // Resource identifier
  "density": float,             // Spawn density (0-1)
  "minHeight": float,           // Min elevation constraint
  "maxHeight": float,           // Max elevation constraint
  "minSlope": float,            // Min slope constraint (degrees)
  "maxSlope": float,            // Max slope constraint (degrees)
  "clusterSize": float,         // Cluster radius
  "allowedBiomes": [integer]    // Biome IDs where this spawns
}
```

**Example:**
```json
{
  "resourceType": "iron_ore",
  "density": 0.2,
  "minHeight": 0.0,
  "maxHeight": 150.0,
  "minSlope": 0.0,
  "maxSlope": 45.0,
  "clusterSize": 8.0,
  "allowedBiomes": [1, 2, 3]
}
```

## structures

Optional structure placements.

```json
{
  "ruins": [StructureDefinition],      // Small ancient ruins
  "ancients": [StructureDefinition],   // Major ancient structures
  "buildings": [StructureDefinition]   // Settlements and buildings
}
```

### StructureDefinition

```json
{
  "structureType": string,      // Structure identifier
  "density": float,             // Spawn density (0-1)
  "minDistance": float,         // Min distance between instances
  "minSize": float,             // Minimum structure size
  "maxSize": float,             // Maximum structure size
  "maxSlope": float,            // Max slope for placement (degrees)
  "allowedBiomes": [integer],   // Biome IDs where this spawns
  "variants": [string]          // Structure variants
}
```

**Example:**
```json
{
  "structureType": "ancient_temple",
  "density": 0.002,
  "minDistance": 2000.0,
  "minSize": 40.0,
  "maxSize": 100.0,
  "maxSlope": 15.0,
  "allowedBiomes": [1, 2, 3],
  "variants": ["pyramid", "ziggurat", "monument"]
}
```

## climate

Climate simulation configuration.

```json
{
  "equatorTemperature": float,             // Temperature at equator (Celsius)
  "poleTemperature": float,                // Temperature at poles (Celsius)
  "temperatureVariation": float,           // Seasonal variation (Celsius)
  "elevationTemperatureGradient": float,   // Temp change per 1000m elevation

  "baseRainfall": float,                   // Base precipitation (mm/year)
  "oceanRainfallBonus": float,             // Additional rainfall near ocean
  "mountainRainShadow": float,             // Rain shadow effect (0-1)

  "windPattern": string                    // Wind pattern: "westerlies", "trade", "monsoon", "polar", "chaotic", "none"
}
```

**Example:**
```json
{
  "equatorTemperature": 30.0,
  "poleTemperature": -20.0,
  "temperatureVariation": 5.0,
  "elevationTemperatureGradient": -6.5,
  "baseRainfall": 1000.0,
  "oceanRainfallBonus": 500.0,
  "mountainRainShadow": 0.5,
  "windPattern": "westerlies"
}
```

## modificationLog

Optional array tracking manual modifications to procedurally generated content.

```json
[
  {
    "timestamp": string,          // ISO 8601 timestamp
    "chunkPos": [x, y, z],       // Chunk coordinates
    "modificationType": string,   // "terrain", "structure", "resource"
    "description": string,        // Human-readable description
    "data": {...}                // Modification-specific data
  }
]
```

**Example:**
```json
[
  {
    "timestamp": "2025-01-15T14:32:00Z",
    "chunkPos": [10, 5, 0],
    "modificationType": "terrain",
    "description": "Player flattened area for base",
    "data": {
      "affectedVoxels": 1250,
      "operation": "flatten"
    }
  }
]
```

## Validation Rules

### Required Fields
- `name`, `description`, `version`, `seed`
- `worldSize` (width, height > 0)
- `maxHeight` (1-255)
- `procGenGraph` (valid nodes and connections)
- `biomes` (at least 1 biome)
- `climate`

### Constraints
- Biome IDs must be unique (0-255)
- Temperature: -100 to 100 Celsius
- Precipitation: 0 to 10000 mm/year
- Elevation: 0 to maxHeight
- Density: 0 to 1
- Slope: 0 to 90 degrees

### Graph Validation
- All node IDs must be unique
- All connections must reference existing nodes and ports
- No cycles in execution flow
- At least one output node

## Schema Versioning

Templates include a version field for future migration:

```json
{
  "version": "1.0.0"  // Semantic versioning
}
```

When loading older templates:
1. Check version compatibility
2. Apply migrations if needed
3. Update to current version
4. Save migrated template

## AI-Friendly Guidelines

For LLM-based template generation:

1. **Use flat structure**: Minimize nesting where possible
2. **Explicit types**: Always specify types in comments
3. **Validation friendly**: Use clear constraints
4. **Self-documenting**: Include descriptions for all entities
5. **Consistent naming**: Use camelCase for properties
6. **Array clarity**: Always use arrays even for single items
7. **Optional vs Required**: Mark clearly in documentation

## Complete Example

See `game/assets/procgen/templates/default_world.json` for a complete, commented example.

## See Also

- [PROCGEN_SYSTEM.md](PROCGEN_SYSTEM.md) - System overview
- [CREATE_TEMPLATE.md](CREATE_TEMPLATE.md) - Template creation guide
- JSON Schema: `game/assets/procgen/schemas/world_template.schema.json`
