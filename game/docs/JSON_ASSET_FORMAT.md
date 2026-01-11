# JSON Asset Format Specification

Comprehensive specification for JSON asset formats in the Nova3D Engine.

## Table of Contents

1. [Overview](#overview)
2. [General Structure](#general-structure)
3. [Asset Types](#asset-types)
4. [Format Guidelines](#format-guidelines)
5. [Examples](#examples)

## Overview

Nova3D uses JSON as the primary format for asset serialization. This provides:

- **Human-readable** format for easy editing
- **Version control friendly** (git diff works well)
- **AI-friendly** structure for LLM-assisted content creation
- **Hot-reloadable** assets during development
- **Schema validation** with clear error messages

## General Structure

All Nova3D JSON assets follow this base structure:

```json
{
  "type": "asset_type",
  "version": "1.0",
  "name": "Asset Name",
  "description": "Human-readable description",
  "author": "Creator Name",
  "tags": ["tag1", "tag2"],
  "dependencies": ["uuid1", "uuid2"],

  // Asset-specific data follows
}
```

### Metadata Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `type` | string | Yes | Asset type identifier |
| `version` | string | Yes | Asset format version (major.minor) |
| `name` | string | Yes | Display name |
| `description` | string | No | Longer description |
| `author` | string | No | Creator attribution |
| `tags` | string[] | No | Searchable tags |
| `dependencies` | string[] | No | UUIDs of required assets |
| `uuid` | string | Auto | Unique identifier (auto-generated) |

### Comments

JSON doesn't officially support comments, but Nova3D strips `//` and `/* */` style comments:

```json
{
  // This is a single-line comment
  "name": "My Asset",

  /* This is a
     multi-line comment */
  "value": 42
}
```

**Note:** Comments are stripped during parsing and not saved back.

## Asset Types

### 1. Material Assets

**Type:** `material`

```json
{
  "type": "material",
  "version": "1.0",
  "name": "Material Name",

  // Basic PBR properties
  "albedo": [1.0, 1.0, 1.0],          // RGB color [0-1]
  "metallic": 0.0,                     // 0 = dielectric, 1 = metal
  "roughness": 0.5,                    // 0 = smooth, 1 = rough
  "ior": 1.5,                          // Index of refraction
  "abbeNumber": 55.0,                  // Dispersion coefficient
  "enableDispersion": false,           // Enable chromatic aberration

  // Advanced properties
  "transmission": 0.0,                 // 0 = opaque, 1 = transparent
  "subsurfaceScattering": 0.0,         // SSS strength
  "emissive": [0.0, 0.0, 0.0],        // Emissive color
  "emissiveIntensity": 0.0,           // Emissive strength

  // Textures (paths relative to asset)
  "textures": {
    "albedo": "textures/albedo.png",
    "normal": "textures/normal.png",
    "roughness": "textures/roughness.png",
    "metallic": "textures/metallic.png",
    "ao": "textures/ao.png",
    "emissive": null                   // null = not used
  },

  // UV mapping
  "tiling": [1.0, 1.0],
  "offset": [0.0, 0.0],

  // Optional advanced features
  "blackbody": {
    "enabled": false,
    "temperature": 6500.0,             // Kelvin
    "intensity": 1.0
  },

  "scattering": {
    "enabled": false,
    "scatterColor": [0.5, 0.5, 0.5],
    "scatterDistance": 1.0,
    "absorptionColor": [1.0, 1.0, 1.0],
    "absorptionDistance": 10.0
  },

  "materialFunction": {
    "type": "flicker",                 // "pulse", "flicker", "noise"
    "frequency": 1.0,
    "amplitude": 0.5,
    "property": "emissiveIntensity"    // Which property to animate
  }
}
```

**IOR Values:**
- Air: 1.0
- Water: 1.333
- Glass: 1.5-1.9
- Diamond: 2.42
- Metals: 2.5-3.5

### 2. Light Assets

**Type:** `light`

```json
{
  "type": "light",
  "version": "1.0",
  "name": "Light Name",

  "lightType": "point",                // "directional", "point", "spot", "area"

  // Color and intensity
  "color": [1.0, 1.0, 1.0],           // RGB [0-1]
  "intensity": 1.0,                    // Lumens or arbitrary units
  "temperature": 6500.0,               // Color temperature in Kelvin
  "useTemperature": false,             // Use temperature instead of color

  // Position and direction
  "position": [0.0, 0.0, 0.0],        // World position
  "direction": [0.0, -1.0, 0.0],      // Direction vector (normalized)
  "radius": 10.0,                      // Influence radius

  // Spot light specific
  "spotAngle": 45.0,                   // Cone angle in degrees
  "spotBlend": 0.3,                    // Edge softness [0-1]

  // Shadows
  "castsShadows": true,
  "shadows": {
    "resolution": 1024,                // Shadow map resolution
    "bias": 0.001,                     // Shadow bias
    "normalBias": 0.01,                // Normal-based bias
    "filterMode": "PCF",               // "PCF", "PCSS", "VSM"
    "softness": 1.0,                   // Soft shadow radius
    "cascadeCount": 4,                 // Directional light only
    "cascadeDistances": [10, 30, 100, 300]
  },

  // IES profile (photometric data)
  "iesProfile": {
    "enabled": false,
    "profilePath": "ies/profile.ies",
    "intensity": 1.0
  },

  // Material function (for flickering, etc.)
  "materialFunction": {
    "type": "flicker",
    "enabled": true,
    "frequency": 5.0,
    "amplitude": 0.3,
    "affectsIntensity": true,
    "affectsRadius": false,
    "affectsColor": false
  }
}
```

**Light Types:**
- **directional**: Sun/moon, infinite distance
- **point**: Omni-directional, sphere
- **spot**: Cone-shaped beam
- **area**: Rectangular/disc surface light
- **emissive_mesh**: Mesh-based area light

### 3. Model Assets

**Type:** `model`

```json
{
  "type": "model",
  "version": "1.0",
  "name": "Model Name",

  "mesh": "meshes/model.obj",          // Path to mesh file

  // Material assignments
  "materials": [
    {
      "slot": 0,                       // Material slot index
      "material": "materials/mat1.json",
      "submeshName": "body"            // Optional submesh name
    }
  ],

  // Transform
  "transform": {
    "position": [0.0, 0.0, 0.0],
    "rotation": [0.0, 0.0, 0.0],       // Euler angles (degrees)
    "scale": [1.0, 1.0, 1.0]
  },

  // Skeleton (for animated models)
  "skeleton": {
    "enabled": true,
    "boneCount": 54,
    "rootBone": "Root"
  },

  // Animations
  "animations": [
    "animations/idle.json",
    "animations/walk.json"
  ],

  // Physics collision
  "collision": {
    "enabled": true,
    "type": "mesh",                    // "box", "sphere", "capsule", "mesh"
    "isStatic": false,
    "mass": 1.0,
    "friction": 0.5,
    "restitution": 0.0,

    // Shape-specific parameters
    "box": { "size": [1, 1, 1] },
    "sphere": { "radius": 0.5 },
    "capsule": { "height": 2.0, "radius": 0.5 }
  },

  // SDF representation (for ray marching)
  "sdf": {
    "enabled": false,
    "primitives": [
      {
        "type": "sphere",
        "position": [0, 0, 0],
        "radius": 1.0,
        "operation": "union"           // "union", "subtract", "intersect"
      }
    ]
  },

  // LOD levels
  "lod": {
    "levels": [
      { "distance": 0.0, "mesh": "meshes/lod0.obj" },
      { "distance": 50.0, "mesh": "meshes/lod1.obj" },
      { "distance": 100.0, "mesh": "meshes/lod2.obj" }
    ]
  }
}
```

### 4. Animation Assets

**Type:** `animation`

```json
{
  "type": "animation",
  "version": "1.0",
  "name": "Animation Name",

  "clipPath": "animations/clip.fbx",   // Path to animation file
  "duration": 1.0,                     // Duration in seconds
  "looping": true,
  "speed": 1.0,                        // Playback speed multiplier

  // Animation events
  "events": [
    {
      "time": 0.5,                     // Time in seconds
      "name": "footstep",
      "parameters": {
        "foot": "left"
      }
    }
  ],

  // Blend tree (for complex animations)
  "blendTree": {
    "type": "1D",                      // "1D", "2D", "direct"
    "parameter": "speed",
    "blendPoints": [
      { "value": 0.0, "animation": "idle" },
      { "value": 0.5, "animation": "walk" },
      { "value": 1.0, "animation": "run" }
    ]
  }
}
```

### 5. Visual Script Assets

**Type:** `visual_script`

```json
{
  "type": "visual_script",
  "version": "1.0",
  "name": "Script Name",

  "nodes": [
    {
      "id": "node_1",
      "type": "Action",
      "position": [0, 0],              // Editor position
      "parameters": {
        "action": "MoveToTarget",
        "speed": 5.0
      },
      "inputs": ["input_1"],
      "outputs": ["output_1"]
    }
  ],

  "connections": [
    {
      "from": "node_1.output_1",
      "to": "node_2.input_1"
    }
  ],

  "variables": {
    "speed": {
      "type": "float",
      "default": 1.0,
      "exposed": true                  // Visible in inspector
    }
  }
}
```

### 6. Particle System Assets

**Type:** `particles`

```json
{
  "type": "particles",
  "version": "1.0",
  "name": "Particle System Name",

  "emitter": {
    "shape": "cone",                   // "point", "box", "sphere", "cone"
    "coneAngle": 30.0,
    "radius": 1.0
  },

  "emission": {
    "rate": 100.0,                     // Particles per second
    "burst": false,
    "maxParticles": 1000,
    "lifetime": 1.0,
    "lifetimeVariation": 0.2
  },

  "velocity": {
    "initial": [0, 5, 0],
    "variation": 1.0
  },

  "size": {
    "start": 0.5,
    "end": 0.0,
    "curve": "linear"                  // "linear", "ease-in", "ease-out"
  },

  "color": {
    "start": [1, 1, 1, 1],
    "end": [1, 1, 1, 0],
    "gradient": "smooth"
  },

  "material": {
    "texture": "textures/particle.png",
    "blendMode": "additive",           // "alpha", "additive", "multiply"
    "sortMode": "distance"             // "none", "distance", "age"
  }
}
```

## Format Guidelines

### Naming Conventions

**Files:**
- Use snake_case: `my_material.json`
- Be descriptive: `iron_metal_rusty.json` not `mat01.json`
- Include type suffix for clarity: `sword_mesh.json`

**Fields:**
- Use camelCase for field names: `emissiveIntensity`
- Avoid abbreviations: `intensity` not `int`
- Be explicit: `castsShadows` not `shadows`

### Value Ranges

**Colors:**
```json
// RGB format, each component [0-1]
"color": [1.0, 0.5, 0.0]

// RGBA format when alpha is needed
"color": [1.0, 0.5, 0.0, 1.0]
```

**Vectors:**
```json
// 2D: [x, y]
"tiling": [2.0, 2.0]

// 3D: [x, y, z]
"position": [0.0, 1.0, 0.0]

// 4D: [x, y, z, w]
"quaternion": [0.0, 0.0, 0.0, 1.0]
```

**Paths:**
```json
// Relative to asset directory
"path": "textures/albedo.png"

// Absolute paths (not recommended)
"path": "C:/Projects/game/assets/texture.png"

// Cross-platform (use forward slashes)
"path": "models/characters/hero.fbx"
```

### AI-Friendly Format

For LLM-assisted content creation:

1. **Flat Structure**: Avoid deep nesting where possible
2. **Clear Names**: `emissiveIntensity` not `ei` or `emit_int`
3. **Inline Docs**: Use comments to explain non-obvious fields
4. **Default Values**: Include all fields, even if default
5. **Type Hints**: Use consistent types (don't mix strings/numbers)

**Good Example:**
```json
{
  "lightType": "point",              // Clear and explicit
  "intensity": 10.0,                 // Includes decimal point
  "castsShadows": true,              // Boolean, not 1/0
  "position": [0.0, 5.0, 0.0]       // Consistent formatting
}
```

**Bad Example:**
```json
{
  "lt": "pt",                        // Abbreviated
  "i": 10,                           // Inconsistent type
  "shad": 1,                         // Boolean as integer
  "pos": [0, 5, 0]                  // Inconsistent formatting
}
```

## Validation

### Schema Validation

All assets are validated against JSON schemas on load:

```cpp
JsonAssetSerializer serializer;
auto asset = serializer.LoadFromFile("material.json");

if (!asset) {
    // Parse error
}

auto result = serializer.Validate(*asset);
if (!result.isValid) {
    for (const auto& error : result.errors) {
        std::cout << "Error: " << error << std::endl;
    }
}
```

### Common Validation Errors

**Missing Required Field:**
```
Error: Missing required field 'type'
```

**Invalid Type:**
```
Error: Field 'intensity' expects number, got string
```

**Out of Range:**
```
Error: Field 'roughness' value 1.5 out of range [0.0, 1.0]
```

**Invalid Reference:**
```
Error: Texture 'invalid.png' not found
```

## Versioning and Migration

### Version Format

```
major.minor.patch
1.0.0
```

- **Major**: Breaking changes, requires migration
- **Minor**: New features, backwards compatible
- **Patch**: Bug fixes, fully compatible

### Migration Example

When format changes, migrations are applied automatically:

```cpp
// Old format (v1.0)
{
  "type": "material",
  "version": "1.0",
  "color": [1, 1, 1]
}

// New format (v1.1) - added albedo field
{
  "type": "material",
  "version": "1.1",
  "albedo": [1, 1, 1],  // Renamed from 'color'
  "color": null         // Deprecated
}
```

Migration is automatic if `autoMigration` is enabled in AssetDatabase.

## See Also

- [Editor World Creation](EDITOR_WORLD_CREATION.md)
- [Template Project Guide](TEMPLATE_PROJECT_GUIDE.md)
- [Asset Pipeline](ASSET_PIPELINE.md)
