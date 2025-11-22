# Configuration Reference

Nova3D Engine uses JSON configuration files for defining game content. This reference documents all configuration formats, schema descriptions, and validation rules.

## Overview

Configuration files are located in:
- `config/` - Engine configuration
- `game/assets/configs/` - Game content definitions
- `game/assets/schemas/` - JSON validation schemas

## Engine Configuration

### engine.json

Main engine settings.

```json
{
    "window": {
        "title": "Nova3D Game",
        "width": 1920,
        "height": 1080,
        "fullscreen": false,
        "vsync": true,
        "resizable": true
    },
    "graphics": {
        "msaa": 4,
        "shadowMapSize": 2048,
        "maxDrawCalls": 10000,
        "enableHDR": true
    },
    "audio": {
        "masterVolume": 1.0,
        "musicVolume": 0.8,
        "sfxVolume": 1.0,
        "maxChannels": 32
    },
    "scripting": {
        "enabled": true,
        "hotReload": true,
        "scriptPaths": ["scripts/", "game/scripts/"]
    },
    "debug": {
        "showFPS": true,
        "enableProfiling": false,
        "logLevel": "info"
    }
}
```

### graphics_config.json

Detailed graphics settings.

```json
{
    "renderer": {
        "backend": "opengl",
        "targetFPS": 60,
        "maxBatchSize": 1000,
        "frustumCulling": true,
        "occlusionCulling": false
    },
    "shadows": {
        "enabled": true,
        "type": "cascaded",
        "cascadeCount": 4,
        "mapSize": 2048,
        "bias": 0.005,
        "distance": 100.0
    },
    "postProcessing": {
        "bloom": {
            "enabled": true,
            "threshold": 1.0,
            "intensity": 0.5
        },
        "ssao": {
            "enabled": false,
            "radius": 0.5,
            "samples": 16
        },
        "fxaa": {
            "enabled": true
        }
    },
    "quality": {
        "textureQuality": "high",
        "modelQuality": "high",
        "effectQuality": "high",
        "lodBias": 0.0
    }
}
```

## Game Content Schemas

### Unit Configuration

**Schema**: `unit.schema.json`

```json
{
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "required": ["id", "type", "name", "stats"],
    "properties": {
        "id": {
            "type": "string",
            "description": "Unique identifier"
        },
        "type": {
            "type": "string",
            "enum": ["infantry", "vehicle", "building", "hero"]
        },
        "name": {
            "type": "string",
            "description": "Display name"
        },
        "description": {
            "type": "string"
        },
        "model": {
            "type": "string",
            "description": "Path to 3D model"
        },
        "icon": {
            "type": "string",
            "description": "Path to icon image"
        },
        "stats": {
            "type": "object",
            "properties": {
                "health": { "type": "number", "minimum": 1 },
                "armor": { "type": "number", "minimum": 0 },
                "damage": { "type": "number", "minimum": 0 },
                "attackSpeed": { "type": "number", "minimum": 0.1 },
                "moveSpeed": { "type": "number", "minimum": 0 },
                "range": { "type": "number", "minimum": 0 },
                "cost": { "type": "integer", "minimum": 0 }
            },
            "required": ["health"]
        },
        "abilities": {
            "type": "array",
            "items": { "type": "string" }
        },
        "tags": {
            "type": "array",
            "items": { "type": "string" }
        }
    }
}
```

**Example Unit**:

```json
{
    "id": "zombie_basic",
    "type": "infantry",
    "name": "Zombie",
    "description": "Basic undead enemy",
    "model": "models/enemies/zombie.fbx",
    "icon": "icons/enemies/zombie.png",
    "stats": {
        "health": 100,
        "armor": 0,
        "damage": 15,
        "attackSpeed": 1.0,
        "moveSpeed": 3.0,
        "range": 1.5
    },
    "abilities": ["melee_attack", "groan"],
    "tags": ["undead", "melee", "hostile"]
}
```

### Spell Configuration

**Schema**: `spell.schema.json`

```json
{
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "required": ["id", "name", "type", "cost"],
    "properties": {
        "id": {
            "type": "string"
        },
        "name": {
            "type": "string"
        },
        "description": {
            "type": "string"
        },
        "type": {
            "type": "string",
            "enum": ["damage", "heal", "buff", "debuff", "summon", "utility"]
        },
        "targeting": {
            "type": "string",
            "enum": ["self", "target", "point", "area", "cone", "line"]
        },
        "cost": {
            "type": "object",
            "properties": {
                "mana": { "type": "integer", "minimum": 0 },
                "health": { "type": "integer", "minimum": 0 },
                "resource": { "type": "integer", "minimum": 0 }
            }
        },
        "cooldown": {
            "type": "number",
            "minimum": 0
        },
        "castTime": {
            "type": "number",
            "minimum": 0
        },
        "range": {
            "type": "number",
            "minimum": 0
        },
        "effects": {
            "type": "array",
            "items": {
                "$ref": "#/definitions/effect"
            }
        },
        "projectile": {
            "$ref": "#/definitions/projectile"
        },
        "sound": {
            "type": "object",
            "properties": {
                "cast": { "type": "string" },
                "impact": { "type": "string" }
            }
        },
        "particles": {
            "type": "object",
            "properties": {
                "cast": { "type": "string" },
                "travel": { "type": "string" },
                "impact": { "type": "string" }
            }
        }
    }
}
```

**Example Spell**:

```json
{
    "id": "fireball",
    "name": "Fireball",
    "description": "Hurls a ball of fire that explodes on impact",
    "type": "damage",
    "targeting": "point",
    "cost": {
        "mana": 25
    },
    "cooldown": 2.0,
    "castTime": 0.5,
    "range": 30.0,
    "effects": [
        {
            "type": "damage",
            "amount": 50,
            "damageType": "fire",
            "radius": 3.0
        },
        {
            "type": "dot",
            "amount": 10,
            "duration": 3.0,
            "tickRate": 1.0,
            "damageType": "fire"
        }
    ],
    "projectile": {
        "speed": 20.0,
        "model": "effects/fireball_projectile.fbx",
        "trail": "particles/fire_trail.json"
    },
    "sound": {
        "cast": "sounds/fireball_cast.wav",
        "impact": "sounds/explosion.wav"
    },
    "particles": {
        "cast": "particles/fire_cast.json",
        "travel": "particles/fire_ball.json",
        "impact": "particles/fire_explosion.json"
    }
}
```

### Ability Configuration

**Schema**: `ability.schema.json`

```json
{
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "required": ["id", "name", "type"],
    "properties": {
        "id": { "type": "string" },
        "name": { "type": "string" },
        "description": { "type": "string" },
        "icon": { "type": "string" },
        "type": {
            "type": "string",
            "enum": ["passive", "active", "toggle", "channeled"]
        },
        "activation": {
            "type": "object",
            "properties": {
                "cost": { "type": "integer" },
                "cooldown": { "type": "number" },
                "charges": { "type": "integer" },
                "chargeRegenTime": { "type": "number" }
            }
        },
        "conditions": {
            "type": "array",
            "items": {
                "type": "object",
                "properties": {
                    "type": { "type": "string" },
                    "value": {}
                }
            }
        },
        "effects": {
            "type": "array",
            "items": { "$ref": "effect.schema.json" }
        },
        "upgrades": {
            "type": "array",
            "items": {
                "type": "object",
                "properties": {
                    "level": { "type": "integer" },
                    "modifiers": { "type": "object" }
                }
            }
        }
    }
}
```

### Building Configuration

**Schema**: `building.schema.json`

```json
{
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "required": ["id", "name", "size"],
    "properties": {
        "id": { "type": "string" },
        "name": { "type": "string" },
        "description": { "type": "string" },
        "model": { "type": "string" },
        "icon": { "type": "string" },
        "size": {
            "type": "object",
            "properties": {
                "width": { "type": "integer", "minimum": 1 },
                "height": { "type": "integer", "minimum": 1 }
            },
            "required": ["width", "height"]
        },
        "stats": {
            "type": "object",
            "properties": {
                "health": { "type": "number" },
                "armor": { "type": "number" }
            }
        },
        "cost": {
            "type": "object",
            "properties": {
                "gold": { "type": "integer" },
                "wood": { "type": "integer" },
                "stone": { "type": "integer" },
                "buildTime": { "type": "number" }
            }
        },
        "production": {
            "type": "object",
            "properties": {
                "units": {
                    "type": "array",
                    "items": { "type": "string" }
                },
                "resources": {
                    "type": "object"
                }
            }
        },
        "requirements": {
            "type": "array",
            "items": { "type": "string" }
        },
        "upgrades": {
            "type": "array",
            "items": { "type": "string" }
        }
    }
}
```

### Effect Configuration

**Schema**: `effect.schema.json`

```json
{
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "required": ["type"],
    "properties": {
        "type": {
            "type": "string",
            "enum": [
                "damage", "heal", "dot", "hot",
                "buff", "debuff", "stun", "slow",
                "knockback", "teleport", "summon",
                "aura", "shield", "lifesteal"
            ]
        },
        "amount": { "type": "number" },
        "duration": { "type": "number" },
        "tickRate": { "type": "number" },
        "radius": { "type": "number" },
        "damageType": {
            "type": "string",
            "enum": ["physical", "fire", "ice", "lightning", "poison", "holy", "shadow"]
        },
        "statModifiers": {
            "type": "object",
            "additionalProperties": {
                "type": "object",
                "properties": {
                    "flat": { "type": "number" },
                    "percent": { "type": "number" }
                }
            }
        },
        "conditions": {
            "type": "array",
            "items": {
                "type": "object",
                "properties": {
                    "type": { "type": "string" },
                    "operator": { "type": "string" },
                    "value": {}
                }
            }
        },
        "visual": {
            "type": "object",
            "properties": {
                "particle": { "type": "string" },
                "sound": { "type": "string" },
                "animation": { "type": "string" }
            }
        }
    }
}
```

### Event Binding Configuration

**Schema**: `event_binding.schema.json`

```json
{
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "required": ["id", "condition"],
    "properties": {
        "id": { "type": "string" },
        "name": { "type": "string" },
        "description": { "type": "string" },
        "category": { "type": "string" },
        "enabled": { "type": "boolean", "default": true },
        "condition": {
            "type": "object",
            "required": ["eventName"],
            "properties": {
                "eventName": { "type": "string" },
                "sourceType": { "type": "string" },
                "propertyConditions": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "properties": {
                            "property": { "type": "string" },
                            "operator": {
                                "type": "string",
                                "enum": ["equals", "not_equals", "less_than", "greater_than", "contains"]
                            },
                            "value": {}
                        }
                    }
                }
            }
        },
        "callbackType": {
            "type": "string",
            "enum": ["Python", "Native", "Event", "Command"]
        },
        "pythonScript": { "type": "string" },
        "pythonFile": { "type": "string" },
        "pythonModule": { "type": "string" },
        "pythonFunction": { "type": "string" },
        "emitEventType": { "type": "string" },
        "emitEventData": { "type": "object" },
        "command": { "type": "string" },
        "commandArgs": {
            "type": "array",
            "items": { "type": "string" }
        },
        "priority": { "type": "integer", "default": 0 },
        "async": { "type": "boolean", "default": false },
        "delay": { "type": "number", "default": 0 },
        "cooldown": { "type": "number", "default": 0 },
        "maxExecutions": { "type": "integer", "default": -1 },
        "oneShot": { "type": "boolean", "default": false }
    }
}
```

**Example Event Binding**:

```json
{
    "id": "on_player_low_health",
    "name": "Low Health Warning",
    "category": "UI",
    "enabled": true,
    "condition": {
        "eventName": "OnHealthChanged",
        "sourceType": "Player",
        "propertyConditions": [
            {
                "property": "healthPercent",
                "operator": "less_than",
                "value": 0.25
            }
        ]
    },
    "callbackType": "Python",
    "pythonModule": "ui_effects",
    "pythonFunction": "show_low_health_warning",
    "cooldown": 5.0
}
```

### Particle Configuration

**Schema**: `particle.schema.json`

```json
{
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "required": ["id", "emitter"],
    "properties": {
        "id": { "type": "string" },
        "name": { "type": "string" },
        "emitter": {
            "type": "object",
            "properties": {
                "type": {
                    "type": "string",
                    "enum": ["point", "sphere", "box", "cone", "mesh"]
                },
                "rate": { "type": "number" },
                "burstCount": { "type": "integer" },
                "duration": { "type": "number" },
                "loop": { "type": "boolean" }
            }
        },
        "particle": {
            "type": "object",
            "properties": {
                "texture": { "type": "string" },
                "lifetime": {
                    "type": "object",
                    "properties": {
                        "min": { "type": "number" },
                        "max": { "type": "number" }
                    }
                },
                "size": {
                    "type": "object",
                    "properties": {
                        "start": { "type": "number" },
                        "end": { "type": "number" }
                    }
                },
                "color": {
                    "type": "object",
                    "properties": {
                        "start": { "type": "array" },
                        "end": { "type": "array" }
                    }
                },
                "velocity": {
                    "type": "object",
                    "properties": {
                        "min": { "type": "array" },
                        "max": { "type": "array" }
                    }
                },
                "gravity": { "type": "number" },
                "drag": { "type": "number" }
            }
        },
        "rendering": {
            "type": "object",
            "properties": {
                "blendMode": {
                    "type": "string",
                    "enum": ["additive", "alpha", "multiply"]
                },
                "sortMode": {
                    "type": "string",
                    "enum": ["none", "distance", "age"]
                },
                "facing": {
                    "type": "string",
                    "enum": ["camera", "velocity", "world"]
                }
            }
        }
    }
}
```

### Tile Configuration

**Schema**: `tile.schema.json`

```json
{
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "required": ["id", "name", "texture"],
    "properties": {
        "id": { "type": "string" },
        "name": { "type": "string" },
        "texture": { "type": "string" },
        "normalMap": { "type": "string" },
        "movementCost": { "type": "number", "minimum": 0 },
        "passable": { "type": "boolean", "default": true },
        "buildable": { "type": "boolean", "default": true },
        "transparent": { "type": "boolean", "default": false },
        "animated": {
            "type": "object",
            "properties": {
                "frames": { "type": "integer" },
                "frameTime": { "type": "number" }
            }
        },
        "properties": {
            "type": "object",
            "additionalProperties": true
        },
        "transitions": {
            "type": "array",
            "items": {
                "type": "object",
                "properties": {
                    "to": { "type": "string" },
                    "texture": { "type": "string" }
                }
            }
        }
    }
}
```

## Validation Rules

### Required Fields

Each config type has required fields that must be present:

| Config Type | Required Fields |
|-------------|-----------------|
| Unit | id, type, name, stats |
| Spell | id, name, type, cost |
| Building | id, name, size |
| Effect | type |
| Event Binding | id, condition |
| Particle | id, emitter |

### Value Constraints

Common constraints applied during validation:

```json
{
    "health": { "minimum": 1 },
    "armor": { "minimum": 0 },
    "damage": { "minimum": 0 },
    "cost": { "minimum": 0 },
    "cooldown": { "minimum": 0 },
    "duration": { "minimum": 0 },
    "range": { "minimum": 0 },
    "speed": { "minimum": 0 }
}
```

### Reference Validation

The editor validates references to other configs:

- Unit abilities reference valid ability IDs
- Building requirements reference valid building IDs
- Spell effects reference valid effect definitions
- Particle textures reference existing files

### Custom Validators

Register custom validation functions:

```cpp
ConfigValidator::Instance().RegisterValidator("unit",
    [](const json& config) -> ValidationResult {
        ValidationResult result;

        // Custom validation logic
        if (config["stats"]["health"] < config["stats"]["armor"]) {
            result.AddWarning("Armor exceeds health");
        }

        return result;
    });
```

## Working with Configs

### Loading Configs

```cpp
#include <engine/config/Config.hpp>

// Load single config
auto unitConfig = ConfigLoader::Load<UnitConfig>("configs/units/zombie.json");

// Load all configs of type
auto allUnits = ConfigLoader::LoadAll<UnitConfig>("configs/units/");

// Watch for changes
ConfigLoader::Watch("configs/units/", [](const std::string& path) {
    ReloadUnit(path);
});
```

### Creating Configs in Code

```cpp
json unitConfig = {
    {"id", "custom_unit"},
    {"type", "infantry"},
    {"name", "Custom Unit"},
    {"stats", {
        {"health", 100},
        {"damage", 20}
    }}
};

// Validate
auto result = ConfigValidator::Validate("unit", unitConfig);
if (result.isValid) {
    ConfigLoader::Save("configs/units/custom_unit.json", unitConfig);
}
```

### Config Inheritance

```json
{
    "id": "zombie_boss",
    "extends": "zombie_basic",
    "name": "Zombie Boss",
    "stats": {
        "health": 500,
        "damage": 30
    },
    "abilities": ["melee_attack", "groan", "summon_zombies"]
}
```

The `extends` field merges with the base config.
