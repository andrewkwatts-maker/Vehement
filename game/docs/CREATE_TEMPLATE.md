# Creating Custom World Templates

## Quick Start

The fastest way to create a custom world template is to start from an existing template:

```bash
# Copy a template as starting point
cp game/assets/procgen/templates/default_world.json my_world.json

# Edit in your favorite editor
code my_world.json

# Test the template
./nova_editor --load-template my_world.json
```

## Step-by-Step Guide

### Step 1: Basic Information

Start with metadata:

```json
{
  "name": "My Custom World",
  "description": "A unique world with custom features",
  "version": "1.0.0",
  "seed": 123456,
  "author": "Your Name",
  "tags": ["custom", "fantasy", "unique"]
}
```

**Tips:**
- Choose a descriptive, memorable name
- Write a clear description (1-2 sentences)
- Use a random seed (or specific seed for reproducibility)
- Add relevant tags for searchability

### Step 2: World Size and Terrain

Define world dimensions and basic terrain parameters:

```json
{
  "worldSize": {
    "width": 8000,
    "height": 8000
  },
  "maxHeight": 200,
  "terrainScale": 1.0,
  "terrainAmplitude": 80.0,
  "erosionStrength": 0.3,
  "erosionIterations": 100
}
```

**Guidelines:**
- **Small worlds**: 2000-5000 (faster generation, good for testing)
- **Medium worlds**: 5000-10000 (balanced size)
- **Large worlds**: 10000-20000 (epic scale, slower generation)
- **maxHeight**: 128-255 (lower for flat worlds, higher for mountains)
- **Erosion**: 0.2-0.4 strength, 50-150 iterations for realism

### Step 3: Procedural Generation Graph

Create the terrain generation pipeline:

```json
{
  "procGenGraph": {
    "nodes": [
      {
        "id": "base_terrain",
        "type": "PerlinNoise",
        "position": [100, 100],
        "parameters": {
          "frequency": 0.008,
          "octaves": 6,
          "persistence": 0.55,
          "lacunarity": 2.2
        }
      }
    ],
    "connections": []
  }
}
```

**Common Patterns:**

#### Simple Smooth Terrain
```json
{
  "id": "simple",
  "type": "SimplexNoise",
  "parameters": {
    "frequency": 0.01,
    "octaves": 4,
    "persistence": 0.5,
    "lacunarity": 2.0
  }
}
```

#### Mountain Ranges
```json
[
  {
    "id": "mountains",
    "type": "PerlinNoise",
    "parameters": {
      "frequency": 0.005,
      "octaves": 6,
      "persistence": 0.6,
      "lacunarity": 2.5
    }
  },
  {
    "id": "ridges",
    "type": "Ridge",
    "parameters": {
      "sharpness": 2.0,
      "offset": 0.5
    }
  }
]
```

#### Islands
```json
[
  {
    "id": "island_mask",
    "type": "WorleyNoise",
    "parameters": {
      "frequency": 0.002,
      "distanceType": 0
    }
  },
  {
    "id": "terrain",
    "type": "SimplexNoise",
    "parameters": {
      "frequency": 0.02,
      "octaves": 5
    }
  },
  {
    "id": "multiply",
    "type": "Blend",
    "parameters": {
      "blend": 1.0,
      "blendMode": "multiply"
    }
  }
]
```

### Step 4: Define Biomes

Create distinct biome types:

```json
{
  "biomes": [
    {
      "id": 0,
      "name": "Grasslands",
      "description": "Rolling hills and meadows",
      "color": [0.4, 0.7, 0.3],
      "minTemperature": 5.0,
      "maxTemperature": 25.0,
      "minPrecipitation": 500.0,
      "maxPrecipitation": 1200.0,
      "minElevation": 50.0,
      "maxElevation": 120.0,
      "treeTypes": ["oak", "birch"],
      "plantTypes": ["grass", "flowers"],
      "vegetationDensity": 0.6,
      "oreTypes": ["iron", "copper"],
      "oreDensities": [0.2, 0.25]
    }
  ]
}
```

**Biome Design Tips:**
- **Temperature ranges**: Use realistic ranges for your world type
  - Tropical: 20-35°C
  - Temperate: -5-25°C
  - Arctic: -40-5°C
- **Precipitation**: Correlate with biome type
  - Desert: 0-250 mm/year
  - Grassland: 400-1000 mm/year
  - Rainforest: 1500-3000 mm/year
- **Elevation**: Set appropriate ranges
  - Ocean: 0-50
  - Plains: 50-100
  - Hills: 100-150
  - Mountains: 150-255

### Step 5: Place Resources

Add ores, vegetation, and water:

```json
{
  "resources": {
    "ores": [
      {
        "resourceType": "iron_ore",
        "density": 0.2,
        "minHeight": 0.0,
        "maxHeight": 150.0,
        "minSlope": 0.0,
        "maxSlope": 45.0,
        "clusterSize": 8.0,
        "allowedBiomes": [0, 1, 2]
      }
    ],
    "vegetation": [
      {
        "resourceType": "oak_tree",
        "density": 0.3,
        "minHeight": 50.0,
        "maxHeight": 120.0,
        "minSlope": 0.0,
        "maxSlope": 30.0,
        "clusterSize": 10.0,
        "allowedBiomes": [0, 1]
      }
    ],
    "water": [
      {
        "resourceType": "lake",
        "density": 0.05,
        "minHeight": 50.0,
        "maxHeight": 90.0,
        "minSlope": 0.0,
        "maxSlope": 5.0,
        "clusterSize": 20.0,
        "allowedBiomes": [0]
      }
    ]
  }
}
```

**Resource Balancing:**
- **Common resources** (iron, copper): density 0.15-0.3
- **Uncommon resources** (gold, silver): density 0.05-0.15
- **Rare resources** (diamond, mithril): density 0.01-0.05
- **Very rare** (adamantite, unobtainium): density 0.001-0.01

### Step 6: Add Structures

Place ruins, temples, and buildings:

```json
{
  "structures": {
    "ruins": [
      {
        "structureType": "ancient_ruins",
        "density": 0.01,
        "minDistance": 500.0,
        "minSize": 10.0,
        "maxSize": 30.0,
        "maxSlope": 20.0,
        "allowedBiomes": [0, 1],
        "variants": ["temple", "tower", "fort"]
      }
    ],
    "ancients": [
      {
        "structureType": "dungeon",
        "density": 0.005,
        "minDistance": 1000.0,
        "minSize": 20.0,
        "maxSize": 60.0,
        "maxSlope": 25.0,
        "allowedBiomes": [1, 2],
        "variants": ["cave", "crypt", "vault"]
      }
    ],
    "buildings": [
      {
        "structureType": "village",
        "density": 0.003,
        "minDistance": 1500.0,
        "minSize": 50.0,
        "maxSize": 150.0,
        "maxSlope": 10.0,
        "allowedBiomes": [0],
        "variants": ["human", "elf"]
      }
    ]
  }
}
```

**Structure Guidelines:**
- **Ruins**: High density (0.01-0.03), small size, low min distance
- **Ancients**: Low density (0.001-0.005), large size, high min distance
- **Buildings**: Very low density (0.001-0.003), variable size, specific biomes

### Step 7: Configure Climate

Set up temperature and precipitation patterns:

```json
{
  "climate": {
    "equatorTemperature": 28.0,
    "poleTemperature": -15.0,
    "temperatureVariation": 8.0,
    "elevationTemperatureGradient": -6.5,
    "baseRainfall": 900.0,
    "oceanRainfallBonus": 400.0,
    "mountainRainShadow": 0.6,
    "windPattern": "westerlies"
  }
}
```

**Climate Presets:**

| Type | Equator Temp | Pole Temp | Base Rainfall | Wind Pattern |
|------|--------------|-----------|---------------|--------------|
| Earth-like | 28 | -20 | 1000 | westerlies |
| Hot | 40 | -5 | 600 | trade |
| Cold | 10 | -45 | 400 | polar |
| Tropical | 35 | 25 | 2000 | monsoon |
| Desert | 40 | 20 | 150 | trade |
| Alien | 50 | -60 | 1500 | chaotic |

## Testing Your Template

### Validation

Before using your template, validate it:

```bash
# Using the engine's validation tool
./nova_tools validate-template my_world.json

# Or programmatically
auto templ = WorldTemplate::LoadFromFile("my_world.json");
std::vector<std::string> errors;
if (!templ->Validate(errors)) {
    for (const auto& err : errors) {
        std::cerr << "Error: " << err << std::endl;
    }
}
```

### Preview Generation

Generate test chunks to preview your world:

```cpp
auto graph = templ->CreateProcGenGraph();

// Generate center chunk
auto result = graph->GenerateChunk(glm::ivec2(0, 0));

// Visualize heightmap
VisualizeHeightmap(result.heightmap);

// Check generation time
std::cout << "Generation time: " << result.generationTime << "ms" << std::endl;
```

### Iteration Workflow

1. **Generate samples**: Create 5-10 random chunks
2. **Evaluate**: Check for desired features
3. **Adjust parameters**: Tweak noise, erosion, resources
4. **Regenerate**: Test with same or different seeds
5. **Repeat**: Until satisfied with results

## Advanced Techniques

### Layered Terrain

Combine multiple noise layers for complexity:

```json
{
  "nodes": [
    {"id": "base", "type": "PerlinNoise", "parameters": {"frequency": 0.005, "octaves": 4}},
    {"id": "detail", "type": "SimplexNoise", "parameters": {"frequency": 0.05, "octaves": 3}},
    {"id": "micro", "type": "SimplexNoise", "parameters": {"frequency": 0.2, "octaves": 2}},

    {"id": "blend1", "type": "Blend", "parameters": {"blend": 0.3, "blendMode": "add"}},
    {"id": "blend2", "type": "Blend", "parameters": {"blend": 0.1, "blendMode": "add"}}
  ],
  "connections": [
    {"from": "base.value", "to": "blend1.inputA"},
    {"from": "detail.value", "to": "blend1.inputB"},
    {"from": "blend1.result", "to": "blend2.inputA"},
    {"from": "micro.value", "to": "blend2.inputB"}
  ]
}
```

### Erosion Pipeline

Create realistic terrain with multiple erosion passes:

```json
{
  "nodes": [
    {"id": "terrain", "type": "PerlinNoise", ...},
    {"id": "hydraulic1", "type": "HydraulicErosion", "parameters": {"iterations": 500, "strength": 0.4}},
    {"id": "thermal", "type": "ThermalErosion", "parameters": {"iterations": 100, "strength": 0.3}},
    {"id": "hydraulic2", "type": "HydraulicErosion", "parameters": {"iterations": 300, "strength": 0.2}}
  ]
}
```

### Biome Transitions

Use blend modes for smooth biome boundaries:

```json
{
  "nodes": [
    {"id": "biome_noise", "type": "SimplexNoise", "parameters": {"frequency": 0.01}},
    {"id": "biome_calc", "type": "Biome", ...},
    {"id": "smooth", "type": "Blur", "parameters": {"radius": 3}}
  ]
}
```

## Programmatic Template Creation

Use the TemplateBuilder API for dynamic generation:

```cpp
using namespace Nova::ProcGen;

auto templ = TemplateBuilder("Procedural World")
    .WithDescription("Generated world")
    .WithSeed(std::rand())
    .WithWorldSize(8000, 8000)
    .WithErosion(0.35f, 120)
    .WithTag("procedural")
    .WithBiome(CreateGrasslandBiome())
    .WithBiome(CreateForestBiome())
    .WithResource(CreateIronOre())
    .WithStructure(CreateRuins())
    .Build();

templ->SaveToFile("generated_world.json");
```

## Common Pitfalls

1. **Too much erosion**: Starts at 0.2, increase gradually
2. **Biome overlap**: Ensure temperature/precipitation ranges don't conflict
3. **Resource spam**: Start with low densities, increase if too sparse
4. **Structure collisions**: Set adequate minDistance
5. **Performance**: Test generation time early, optimize if >100ms per chunk
6. **Unrealistic climate**: Research real-world climate patterns
7. **Missing connections**: Validate all graph connections
8. **Wrong slope units**: Use degrees (0-90), not radians

## Examples by World Type

### Fantasy RPG
- Multiple biomes (6-8)
- Varied resources (common to legendary)
- Many structure types
- Realistic climate
- Medium erosion

### Survival Game
- Harsh biomes (desert, tundra, jungle)
- Scarce resources
- Few structures
- Extreme climate
- Heavy erosion

### Creative Sandbox
- Flat or gentle terrain
- Abundant resources
- No or minimal structures
- Mild climate
- No erosion

### Sci-Fi Alien
- Unusual biomes
- Exotic resources
- Ancient alien structures
- Extreme/unusual climate
- Unique terrain features

## See Also

- [TEMPLATE_FORMAT.md](TEMPLATE_FORMAT.md) - Complete format specification
- [PROCGEN_SYSTEM.md](PROCGEN_SYSTEM.md) - System documentation
- [REALWORLD_DATA.md](REALWORLD_DATA.md) - Using real-world data
- Example templates: `game/assets/procgen/templates/`
