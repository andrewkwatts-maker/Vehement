# World Creation Guide

Complete guide for creating and configuring procedurally generated worlds using the Nova3D Editor.

## Table of Contents

1. [Overview](#overview)
2. [New World Dialog](#new-world-dialog)
3. [World Templates](#world-templates)
4. [World Configuration](#world-configuration)
5. [Advanced Settings](#advanced-settings)
6. [Best Practices](#best-practices)

## Overview

The Nova3D Engine provides a powerful procedural generation system for creating vast, detailed game worlds. The **New World Dialog** provides an intuitive interface for configuring world generation from templates.

### Key Features

- Template-based world generation
- Configurable world size and parameters
- Real-time preview (coming soon)
- Seed-based deterministic generation
- Advanced erosion and resource placement settings
- Hot-reloadable templates

## New World Dialog

### Opening the Dialog

**Editor Menu:** `World → New World` or `Ctrl+N`

The New World Dialog presents all options for creating a new world:

```
┌─ New World ────────────────────────────────┐
│ Template: [Dropdown ▼] [Preview thumbnail] │
│                                             │
│ Default Fantasy World                       │
│ A balanced fantasy world with varied       │
│ biomes, resources, and ancient structures  │
│                                             │
│ Seed: [12345____] [🎲 Random]             │
│ Size: [Medium ▼] (5000x5000m)            │
│                                             │
│ ▼ Advanced Settings                         │
│   Erosion: [===|===] 100 iterations        │
│   Resources: [==|====] 1.0x density        │
│   Structures: [==|====] 1.0x density       │
│   ☑ Use real-world data                    │
│                                             │
│ [Create Empty]  [Cancel]  [Create World]   │
└─────────────────────────────────────────────┘
```

### Template Selection

**Available Templates:**

- **Default World** - Balanced fantasy world with varied biomes
- **Empty World** - Flat, empty world for manual creation
- **Island World** - Ocean world with islands and archipelagos
- **Desert World** - Arid desert with oases and canyons
- **Frozen World** - Frozen tundra and ice sheets
- **Volcanic World** - Volcanic terrain with lava flows

Each template includes:
- Pre-configured biomes
- Resource distributions
- Structure placement rules
- Climate settings
- Erosion parameters

### Basic Settings

#### World Name
The display name for your world. Used in save files and world selection.

```
World Name: My Fantasy World
```

#### Seed
Seed value controls all random generation. Same seed = same world.

- **Random Button**: Generates a random seed
- **Manual Entry**: Enter specific seed for reproducible worlds
- **Range**: 1 to 999,999

**Use Cases:**
- Share seeds with other players for identical worlds
- Test specific terrain features
- Debug generation issues

#### World Size

**Presets:**
- **Small**: 2,500 x 2,500m (~1,526 chunks)
- **Medium**: 5,000 x 5,000m (~6,104 chunks)
- **Large**: 10,000 x 10,000m (~24,414 chunks)
- **Huge**: 20,000 x 20,000m (~97,656 chunks)
- **Custom**: Specify exact size

**Performance Considerations:**
- Larger worlds require more disk space
- Chunk streaming handles runtime memory
- Generation time scales with world size
- Recommended: Start with Medium

## Advanced Settings

### Erosion Iterations

Controls how many erosion simulation passes are applied to the terrain.

```
Range: 0 - 500
Default: 100
Recommended: 100-200
```

**Effects:**
- **0-50**: Minimal erosion, sharp terrain
- **50-150**: Balanced, natural-looking terrain
- **150-300**: Heavy erosion, smooth terrain
- **300+**: Very smooth, aged terrain (slow)

**Performance Impact:** High - Each iteration adds ~5-10% generation time

### Resource Density

Multiplier for resource (ore, vegetation) placement density.

```
Range: 0.1x - 3.0x
Default: 1.0x
Recommended: 0.8x - 1.5x
```

**Effects:**
- **< 1.0**: Scarce resources, survival challenge
- **1.0**: Balanced gameplay
- **> 1.0**: Abundant resources, easier gameplay

### Structure Density

Multiplier for structure (ruins, temples, dungeons) placement.

```
Range: 0.1x - 3.0x
Default: 1.0x
Recommended: 0.5x - 1.5x
```

**Note:** Structures have minimum distance constraints to prevent overlap.

### Use Real-World Data

**Experimental Feature** - Uses real-world elevation and climate data.

- Requires internet connection
- Limited to specific geographic regions
- Significantly slower generation
- May not work with all templates

### Terrain Roughness

Controls overall terrain noise amplitude.

```
Range: 0.1x - 2.0x
Default: 1.0x
```

- **< 1.0**: Smoother, gentler terrain
- **> 1.0**: Rougher, more dramatic terrain

### Water Level

Global water level adjustment in meters.

```
Range: -10.0 to +50.0 meters
Default: 0.0
```

- **Negative**: Lower ocean levels, more land
- **Positive**: Higher ocean levels, more water

## World Configuration Files

After creation, worlds are configured via JSON files:

### world.json

Main world configuration file:

```json
{
  "name": "My World",
  "template": "default_world",
  "seed": 12345,
  "size": [5000, 5000],
  "procGenOverrides": {
    "erosionIterations": 150,
    "resourceDensity": 1.2
  }
}
```

**Hot-Reloadable:** Changes take effect on chunk regeneration.

### Persistence Settings

```json
"persistence": {
  "backend": "sqlite",
  "path": "saves/my_world.db",
  "autoSave": true,
  "autoSaveIntervalSeconds": 300
}
```

**Backends:**
- **sqlite**: File-based database (recommended)
- **postgres**: Server database (multiplayer)
- **memory**: No persistence (testing)

## Creating Custom Templates

### Template Structure

Templates are JSON files in `game/assets/procgen/templates/`:

```json
{
  "name": "My Custom World",
  "description": "Custom world template",
  "version": "1.0.0",
  "seed": 12345,
  "worldSize": [5000, 5000],
  "biomes": [...],
  "ores": [...],
  "structures": [...]
}
```

### Biome Definition

```json
{
  "id": 1,
  "name": "Grassland",
  "minTemperature": 10.0,
  "maxTemperature": 25.0,
  "minPrecipitation": 500.0,
  "maxPrecipitation": 1500.0,
  "treeTypes": ["oak", "birch"],
  "vegetationDensity": 0.7
}
```

### Resource Definition

```json
{
  "resourceType": "iron_ore",
  "density": 0.1,
  "minHeight": 0.0,
  "maxHeight": 100.0,
  "clusterSize": 5.0,
  "allowedBiomes": [1, 2, 3]
}
```

## Best Practices

### Performance Optimization

1. **Start Small**: Test with Small world size first
2. **Moderate Erosion**: 100-150 iterations is sufficient
3. **Chunk Streaming**: Let engine load chunks on-demand
4. **LOD Settings**: Configure appropriate view distances

### Artistic Control

1. **Seed Exploration**: Try multiple seeds to find ideal terrain
2. **Manual Editing**: Procedural generation is a starting point
3. **Layer Biomes**: Combine multiple noise layers
4. **Test Iteratively**: Small changes, frequent testing

### Multiplayer Considerations

1. **Shared Seed**: All players must use same seed
2. **Template Sync**: Ensure templates are identical
3. **Chunk Persistence**: Use server-side database
4. **Generation Order**: Deterministic generation is crucial

### Common Pitfalls

**Too Many Erosion Iterations**
- Symptom: Very slow generation (>30s per chunk)
- Solution: Reduce to 100-150 iterations

**Extreme Resource Density**
- Symptom: Performance issues, cluttered world
- Solution: Keep density between 0.5x - 2.0x

**Incompatible Template Versions**
- Symptom: Generation errors, missing features
- Solution: Update templates to match engine version

## Workflow Examples

### Scenario 1: Quick Test World

```
1. Open New World Dialog
2. Select "Empty World" template
3. Set size to "Small"
4. Click "Create World"
5. Total time: < 1 minute
```

### Scenario 2: Production World

```
1. Test multiple seeds with Small size
2. Find ideal seed for terrain
3. Adjust erosion iterations (150)
4. Set resource density (1.2x)
5. Switch to Medium/Large size
6. Enable auto-save
7. Generate world (5-15 minutes)
```

### Scenario 3: Custom Template World

```
1. Create custom template JSON
2. Place in game/assets/procgen/templates/
3. Restart editor to refresh templates
4. Select custom template
5. Configure parameters
6. Test with Small size first
7. Iterate on template
8. Generate final Large world
```

## Troubleshooting

### World Generation Fails

**Check:**
- Template file is valid JSON
- All referenced assets exist
- Seed is within valid range
- Disk space available

### Slow Generation

**Solutions:**
- Reduce erosion iterations
- Decrease world size
- Close other applications
- Check CPU/disk usage

### Inconsistent Results

**Causes:**
- Template file changed
- Different engine version
- Corrupted cache
- Non-deterministic scripts

**Fix:** Clear cache, regenerate with same seed

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+N` | New World Dialog |
| `Ctrl+W` | Close World |
| `Ctrl+S` | Save World Config |
| `Ctrl+R` | Regenerate Current Chunk |
| `F5` | Reload Templates |

## API Reference

### C++ Integration

```cpp
// Create world from template
auto templateLib = std::make_shared<WorldTemplateLibrary>();
templateLib->Initialize();

auto templ = templateLib->GetTemplate("default_world");
auto procGenGraph = templ->CreateProcGenGraph();

// Generate chunk
auto result = procGenGraph->GenerateChunk(glm::ivec2(0, 0));
```

### Editor Callbacks

```cpp
// Register world creation callback
newWorldDialog->SetOnWorldCreatedCallback(
    [](const WorldCreationParams& params) {
        // Handle world creation
        CreateAndLoadWorld(params);
    }
);
```

## See Also

- [JSON Asset Format](JSON_ASSET_FORMAT.md)
- [Template Project Guide](TEMPLATE_PROJECT_GUIDE.md)
- [Asset Pipeline](ASSET_PIPELINE.md)
- [Procedural Generation API](../engine/docs/PROC_GEN_API.md)
