# Template Project Guide

Complete guide to the Nova3D default project template and how to create your own projects.

## Overview

The **default_project** serves as both a template and a comprehensive example of Nova3D Engine features. It demonstrates:

- Complete procedurally generated RTS game world
- All asset types with realistic configurations
- Best practices for project structure
- Integration with procedural generation system

**Location:** `game/assets/projects/default_project/`

## Project Structure

```
default_project/
├── project.json              # Project manifest and settings
├── world.json                # World configuration
├── player_start.json         # Player spawn configuration
├── game_rules.json           # Game rules and settings
│
├── assets/
│   ├── materials/           # PBR materials
│   │   ├── metal_iron.json
│   │   ├── glass_clear.json
│   │   ├── crystal_glowing.json
│   │   ├── water_ocean.json
│   │   └── wood_oak.json
│   │
│   ├── lights/              # Light configurations
│   │   ├── sun.json
│   │   ├── torch.json
│   │   ├── campfire.json
│   │   └── street_lamp.json
│   │
│   ├── models/              # 3D models
│   │   ├── buildings/
│   │   │   └── house_medieval.json
│   │   ├── units/
│   │   │   └── warrior.json
│   │   └── resources/
│   │
│   ├── animations/          # Animation clips
│   │   ├── character_idle.json
│   │   ├── character_walk.json
│   │   └── character_attack.json
│   │
│   ├── scripts/             # Visual scripts
│   │   ├── unit_ai.json
│   │   ├── building_construction.json
│   │   └── resource_gathering.json
│   │
│   ├── shaders/             # Custom shaders
│   │   ├── custom_water.json
│   │   └── custom_fire.json
│   │
│   ├── audio/               # Audio assets
│   │   ├── ambient_forest.json
│   │   └── sfx_sword.json
│   │
│   └── vfx/                 # Particle systems
│       ├── fire.json
│       └── smoke.json
│
└── scenes/                   # Scene definitions
    └── main_menu.scene
```

## Project Configuration

### project.json

Main project manifest:

```json
{
  "name": "Default Project",
  "description": "Example showcasing all engine features",
  "version": "1.0",

  "worldConfig": "world.json",
  "playerStartConfig": "player_start.json",
  "gameRulesConfig": "game_rules.json",

  "assetDirectories": [
    "assets/materials",
    "assets/lights",
    ...
  ],

  "renderSettings": {
    "defaultResolution": [1920, 1080],
    "antialiasing": "MSAA4x",
    "shadowQuality": "high"
  }
}
```

**Key Fields:**

| Field | Purpose |
|-------|---------|
| `worldConfig` | Path to world configuration |
| `assetDirectories` | Directories to scan for assets |
| `startupScenes` | Scenes to load on startup |
| `renderSettings` | Default rendering configuration |

### world.json

World generation and persistence settings:

```json
{
  "name": "Default World",
  "template": "default_world",
  "seed": 98765,
  "size": [5000, 5000],

  "procGenOverrides": {
    "erosionIterations": 150,
    "resourceDensity": 1.2,
    "structureDensity": 0.8
  },

  "chunkStreaming": {
    "chunkSize": 64,
    "viewDistance": 8,
    "loadRadius": 10
  }
}
```

**Chunk Streaming:**
- `chunkSize`: Size of terrain chunks (meters)
- `viewDistance`: Chunks to keep loaded around player
- `loadRadius`: Distance to start loading chunks
- `unloadRadius`: Distance to unload chunks

### game_rules.json

Game-specific rules and configuration:

```json
{
  "name": "Fantasy RTS",

  "startingResources": {
    "gold": 500,
    "wood": 300,
    "food": 200
  },

  "dayNightCycle": {
    "enabled": true,
    "dayLengthMinutes": 20
  },

  "combat": {
    "friendlyFire": false,
    "unitCap": 200
  },

  "victory": {
    "conditions": [...]
  }
}
```

## Asset Examples

### Materials

#### Basic PBR Material (Iron)

```json
{
  "type": "material",
  "name": "Iron Metal",

  "albedo": [0.56, 0.57, 0.58],
  "metallic": 1.0,
  "roughness": 0.4,

  "textures": {
    "albedo": "assets/textures/materials/iron_albedo.png",
    "normal": "assets/textures/materials/iron_normal.png",
    "roughness": "assets/textures/materials/iron_roughness.png"
  }
}
```

**Use Cases:**
- Weapons
- Armor
- Tools
- Building materials

#### Transparent Material (Glass)

```json
{
  "type": "material",
  "name": "Clear Glass",

  "albedo": [1.0, 1.0, 1.0],
  "metallic": 0.0,
  "roughness": 0.05,
  "ior": 1.5,
  "transmission": 0.95,
  "enableDispersion": true,
  "abbeNumber": 55.0
}
```

**Features:**
- Physically accurate refraction
- Chromatic aberration (dispersion)
- IOR-based light bending

#### Emissive Material (Glowing Crystal)

```json
{
  "type": "material",
  "name": "Glowing Crystal",

  "albedo": [0.5, 0.7, 1.0],
  "emissive": [0.3, 0.5, 1.0],
  "emissiveIntensity": 5.0,

  "blackbody": {
    "enabled": true,
    "temperature": 8000.0
  },

  "materialFunction": {
    "type": "pulse",
    "frequency": 1.5,
    "amplitude": 0.3
  }
}
```

**Features:**
- Self-illumination
- Blackbody radiation (physically accurate color)
- Animated pulsing

#### Water Material

```json
{
  "type": "material",
  "name": "Ocean Water",

  "ior": 1.333,
  "transmission": 0.8,
  "subsurfaceScattering": 0.5,

  "scattering": {
    "scatterColor": [0.1, 0.4, 0.6],
    "absorptionColor": [0.3, 0.5, 0.7]
  },

  "animation": {
    "normalMapScroll": [0.02, 0.03],
    "waveAmplitude": 0.5
  }
}
```

**Features:**
- Subsurface scattering
- Animated normal maps
- Realistic absorption/scattering

### Lights

#### Directional Sun Light

```json
{
  "type": "light",
  "lightType": "directional",

  "temperature": 5778.0,
  "intensity": 5.0,
  "direction": [0.3, -0.8, 0.5],

  "shadows": {
    "resolution": 4096,
    "cascadeCount": 4
  },

  "dayNightCycle": {
    "enabled": true,
    "animateDirection": true,
    "animateColor": true
  }
}
```

**Best Practices:**
- Use high shadow resolution (2048-4096)
- Enable cascaded shadow maps
- Animate for day/night cycle

#### Flickering Torch

```json
{
  "type": "light",
  "lightType": "point",

  "temperature": 1800.0,
  "radius": 8.0,

  "materialFunction": {
    "type": "flicker",
    "frequency": 15.0,
    "amplitude": 0.3,
    "affectsIntensity": true
  }
}
```

**Features:**
- Procedural flickering
- Warm color temperature
- Audio integration support

#### IES Profile Light

```json
{
  "type": "light",
  "lightType": "spot",

  "iesProfile": {
    "enabled": true,
    "profilePath": "assets/ies/street_lamp.ies"
  }
}
```

**Use Cases:**
- Realistic architectural lighting
- Street lamps
- Indoor fixtures

### Models

#### Building Model

```json
{
  "type": "model",
  "name": "Medieval House",

  "mesh": "assets/meshes/buildings/house_medieval.obj",

  "materials": [
    { "slot": 0, "material": "assets/materials/wood_oak.json" },
    { "slot": 1, "material": "assets/materials/stone_cobble.json" }
  ],

  "collision": {
    "enabled": true,
    "type": "mesh",
    "isStatic": true
  },

  "lod": {
    "levels": [
      { "distance": 0.0, "mesh": "lod0.obj" },
      { "distance": 50.0, "mesh": "lod1.obj" }
    ]
  }
}
```

**Features:**
- Multi-material support
- Static collision
- LOD levels for performance

#### Character Model

```json
{
  "type": "model",
  "name": "Warrior Unit",

  "mesh": "assets/meshes/units/warrior.fbx",
  "skeleton": { "enabled": true },

  "animations": [
    "assets/animations/character_idle.json",
    "assets/animations/character_walk.json",
    "assets/animations/character_attack.json"
  ],

  "collision": {
    "type": "capsule",
    "capsule": { "height": 1.8, "radius": 0.4 },
    "mass": 75.0
  },

  "ai": {
    "script": "assets/scripts/unit_ai.json"
  }
}
```

**Features:**
- Skeletal animation
- Dynamic collision
- AI behavior integration

### Visual Scripts

#### Unit AI Behavior Tree

```json
{
  "type": "visual_script",
  "name": "Unit AI Behavior",

  "nodes": [
    {
      "id": "root",
      "type": "Selector",
      "children": ["flee_check", "attack_check", "patrol"]
    },
    {
      "id": "flee_check",
      "type": "Sequence",
      "children": ["health_low", "flee_action"]
    }
  ]
}
```

**Behavior Tree Nodes:**
- **Selector**: Try children until one succeeds
- **Sequence**: Execute children in order
- **Condition**: Boolean check
- **Action**: Perform action

### Particle Systems

#### Fire Effect

```json
{
  "type": "particles",
  "name": "Fire Effect",

  "emitter": {
    "shape": "cone",
    "coneAngle": 30.0
  },

  "emission": {
    "rate": 50.0,
    "lifetime": 1.5
  },

  "color": {
    "start": [1.0, 0.8, 0.2, 1.0],
    "end": [0.3, 0.1, 0.05, 0.0]
  },

  "material": {
    "blendMode": "additive",
    "texture": "assets/textures/particles/fire.png"
  }
}
```

## Creating Your Own Project

### Step 1: Copy Template

```bash
cd game/assets/projects/
cp -r default_project my_project
cd my_project
```

### Step 2: Configure project.json

```json
{
  "name": "My Project",
  "description": "My awesome game",
  "version": "0.1.0",
  ...
}
```

### Step 3: Configure world.json

Choose a template or create custom:

```json
{
  "name": "My World",
  "template": "island_world",
  "seed": 12345,
  "size": [10000, 10000]
}
```

### Step 4: Add Custom Assets

1. Create materials for your game's aesthetic
2. Add models for buildings/units/resources
3. Define behavior scripts
4. Configure lighting

### Step 5: Load in Editor

```
File → Open Project → my_project/project.json
```

## Best Practices

### Organization

**Do:**
- Group assets by type (materials/, models/, etc.)
- Use descriptive names (warrior_leather_armor.json)
- Keep related assets together
- Document complex assets with comments

**Don't:**
- Mix asset types in same directory
- Use generic names (mat01.json, model.json)
- Store source files with game assets
- Forget to version control JSON files

### Performance

**Materials:**
- Reuse materials where possible
- Use texture atlases for small objects
- Limit emissive materials
- Profile transparent materials

**Models:**
- Always use LOD for distant objects
- Keep poly counts reasonable (< 50k for characters)
- Use instancing for repeated objects
- Optimize collision meshes

**Lights:**
- Limit shadow-casting lights (< 4 visible)
- Use lower shadow resolutions for small lights
- Prefer baked lighting for static scenes
- Profile light overdraw

### Content Pipeline

**Workflow:**
1. Create high-poly model in DCC tool
2. Retopologize for game
3. Bake normal/AO maps
4. Export as FBX/OBJ
5. Create JSON asset
6. Test in engine
7. Iterate

**Tools:**
- **Modeling**: Blender, Maya, 3ds Max
- **Texturing**: Substance Painter, Quixel
- **Audio**: Audacity, REAPER
- **Scripts**: Visual Studio Code

## Integration with Procedural Generation

### Placing Assets in Proc Gen World

Models referenced in templates are automatically placed:

```json
// In world template
"structures": [
  {
    "structureType": "ruins",
    "density": 0.01,
    "variants": [
      "assets/models/buildings/ruins_temple.json",
      "assets/models/buildings/ruins_tower.json"
    ]
  }
]
```

### Resource Placement

```json
"ores": [
  {
    "resourceType": "iron_ore",
    "density": 0.1,
    "model": "assets/models/resources/ore_iron.json"
  }
]
```

### Vegetation

```json
"vegetation": [
  {
    "vegetationType": "tree_oak",
    "density": 0.5,
    "model": "assets/models/vegetation/tree_oak.json",
    "minSlope": 0.0,
    "maxSlope": 30.0
  }
]
```

## Troubleshooting

### Assets Not Loading

**Check:**
1. File path is correct and relative
2. Asset type matches file extension
3. JSON is valid (use JSON validator)
4. Dependencies exist
5. Asset is in registered directory

### Performance Issues

**Profile:**
1. Draw calls (use built-in profiler)
2. Light count
3. Shadow map resolution
4. Material complexity

**Optimize:**
- Reduce light count
- Lower shadow quality
- Use LOD
- Batch materials

### Visual Issues

**Materials Not Correct:**
- Check texture paths
- Verify PBR values in range
- Test with simple values first
- Compare with working example

**Lighting Problems:**
- Check light intensity
- Verify shadow settings
- Test with different times of day
- Disable advanced features

## See Also

- [JSON Asset Format](JSON_ASSET_FORMAT.md)
- [Editor World Creation](EDITOR_WORLD_CREATION.md)
- [Asset Pipeline](ASSET_PIPELINE.md)
