# Content Folder Structure

This document describes the organization of game content assets in the Nova3D/Vehement2 engine.

## Overview

All game configuration data is stored as JSON files in the `game/assets/configs/` directory. The content is organized by type, with each type having its own subfolder.

## Directory Layout

```
game/assets/configs/
├── units/              # Unit configurations
│   ├── factions/       # Organized by faction (optional)
│   │   ├── human/
│   │   ├── orc/
│   │   └── undead/
│   ├── soldier.json
│   ├── worker.json
│   └── zombie.json
│
├── buildings/          # Building configurations
│   ├── military/
│   ├── economic/
│   ├── barracks.json
│   ├── house.json
│   └── wall.json
│
├── spells/             # Spell configurations
│   ├── fire/           # Organized by school (optional)
│   ├── ice/
│   ├── holy/
│   ├── fireball.json
│   ├── frost_nova.json
│   └── heal_aura.json
│
├── effects/            # Status effects and buffs
│   ├── buffs/
│   ├── debuffs/
│   ├── poisoned.json
│   ├── burning.json
│   └── regeneration.json
│
├── tiles/              # Terrain tile types
│   ├── terrain/
│   ├── resources/
│   ├── grass.json
│   ├── water.json
│   └── forest.json
│
├── heroes/             # Hero character configs
│   ├── hero_warlord.json
│   ├── hero_engineer.json
│   └── hero_scout.json
│
├── abilities/          # Hero/unit abilities
│   ├── passive/
│   ├── active/
│   ├── ability_cleave.json
│   └── ability_inspire.json
│
├── techtrees/          # Technology tree definitions
│   ├── universal_tree.json
│   ├── fortress_tree.json
│   └── nomad_tree.json
│
├── projectiles/        # Projectile definitions
│   ├── bullets/
│   ├── arrows/
│   ├── spells/
│   └── bullet_rifle.json
│
├── resources/          # Resource type definitions
│   ├── gold.json
│   ├── wood.json
│   └── food.json
│
├── cultures/           # Cultural/faction definitions
│   ├── human.json
│   ├── orc.json
│   └── undead.json
│
├── quests/             # Quest definitions
│   └── main_storyline/
│
├── dialogs/            # Dialog/conversation trees
│   └── npcs/
│
└── scripts/            # Script configuration files
    └── hooks/
```

## File Naming Conventions

### General Rules
- Use lowercase letters only
- Use underscores `_` to separate words
- Keep names concise but descriptive
- Prefix with type indicator when helpful (e.g., `unit_soldier`, `spell_fireball`)

### Examples
```
Good:
  soldier.json
  fire_mage.json
  frost_nova.json
  tech_archery_upgrade.json

Bad:
  Soldier.json          # No uppercase
  FireMage.json         # Use underscores
  frost nova.json       # No spaces
  a1.json               # Not descriptive
```

## JSON File Structure

### Required Fields
Every config file should have these base fields:

```json
{
    "id": "unique_identifier",      // Required: Unique ID
    "type": "unit",                 // Required: Asset type
    "name": "Display Name",         // Required: Human-readable name
    "description": "...",           // Optional: Description text
    "tags": ["tag1", "tag2"]        // Optional: Classification tags
}
```

### Type-Specific Structures

#### Units (`units/`)
```json
{
    "id": "unit_soldier",
    "type": "unit",
    "name": "Soldier",
    "description": "Basic infantry unit",
    "tags": ["military", "infantry", "ranged"],

    "model": {
        "path": "models/units/soldier.obj",
        "scale": [1.0, 1.0, 1.0]
    },

    "combat": {
        "health": 100,
        "maxHealth": 100,
        "armor": 5,
        "attackDamage": 15,
        "attackSpeed": 1.5,
        "attackRange": 12.0
    },

    "movement": {
        "speed": 5.0,
        "turnRate": 360.0
    },

    "faction": "human",
    "tier": 1,
    "class": "infantry"
}
```

#### Buildings (`buildings/`)
```json
{
    "id": "building_barracks",
    "type": "building",
    "name": "Barracks",
    "description": "Trains military units",
    "tags": ["military", "production"],

    "footprint": {
        "width": 3,
        "height": 3
    },

    "stats": {
        "health": 1000,
        "armor": 20,
        "buildTime": 30.0
    },

    "costs": {
        "gold": 200,
        "wood": 150
    },

    "trainableUnits": ["unit_soldier", "unit_archer"],
    "requirements": {
        "buildings": ["building_town_center"],
        "techs": []
    }
}
```

#### Spells (`spells/`)
```json
{
    "id": "spell_fireball",
    "type": "spell",
    "name": "Fireball",
    "description": "Launches a ball of fire",
    "tags": ["fire", "damage", "projectile"],

    "school": "fire",
    "targetType": "point",

    "damage": {
        "amount": 50,
        "type": "fire",
        "radius": 3.0
    },

    "costs": {
        "mana": 25,
        "cooldown": 8.0,
        "castTime": 0.5
    },

    "range": 15.0,

    "effects": ["effect_burning"]
}
```

#### Effects (`effects/`)
```json
{
    "id": "effect_poisoned",
    "type": "effect",
    "name": "Poisoned",
    "description": "Taking damage over time",
    "tags": ["debuff", "damage", "nature"],

    "duration": 10.0,
    "interval": 1.0,
    "stackable": false,
    "maxStacks": 1,

    "damagePerTick": 5,
    "damageType": "nature",

    "visual": {
        "particleEffect": "effects/poison_aura.fx",
        "tintColor": [0.2, 0.8, 0.2, 1.0]
    }
}
```

#### Tiles (`tiles/`)
```json
{
    "id": "tile_grass",
    "type": "tile",
    "name": "Grass",
    "description": "Open grassland terrain",
    "tags": ["terrain", "walkable", "temperate"],

    "terrainType": "ground",
    "biome": "temperate",

    "movement": {
        "cost": 1.0,
        "allowsBuilding": true
    },

    "combat": {
        "defenseBonus": 0.0,
        "coverBonus": 0.0
    },

    "texture": "textures/tiles/grass_diffuse.png",
    "variations": 4
}
```

## Tags System

Tags are used for classification and filtering. Use consistent tag names:

### Common Tags
- `military` - Combat-related
- `economic` - Resource-related
- `magic` - Magical/spell-related
- `melee` - Close combat
- `ranged` - Ranged combat
- `support` - Support/utility

### Tier Tags
- `tier1`, `tier2`, `tier3` - Technology/power tiers

### Faction Tags
- `human`, `orc`, `undead`, `elf` - Faction affiliations

### School Tags (Spells)
- `fire`, `ice`, `holy`, `shadow`, `nature`, `arcane`

## Dependencies and References

When referencing other assets, use their full ID:

```json
{
    "trainableUnits": ["unit_soldier", "unit_archer"],
    "appliedEffects": ["effect_burning", "effect_slowed"],
    "projectile": "projectile_fireball"
}
```

The Content Database tracks these dependencies automatically.

## Validation

All config files are validated against JSON schemas. The schemas are located in:
```
game/assets/schemas/
├── unit_schema.json
├── building_schema.json
├── spell_schema.json
└── ...
```

Run validation using the editor or command line:
```bash
./tools/validate_configs.py game/assets/configs/
```

## Best Practices

1. **Keep IDs Unique**: Use prefixes like `unit_`, `spell_`, `building_` to avoid collisions

2. **Use Tags Liberally**: Tags make filtering and searching much easier

3. **Document Complex Configs**: Use JSON comments (`// comment`) for complex logic

4. **Keep Related Assets Together**: Use subfolders to group related assets

5. **Version Control**: Commit config changes with descriptive messages

6. **Test After Changes**: Run validation and test in-game after making changes

7. **Use Templates**: Start from templates in the Create Asset dialog

## Editor Integration

The Content Browser in the editor provides:
- Visual browsing of all content
- Search and filtering by type, tags, properties
- Thumbnail previews
- Drag-and-drop organization
- Quick editing of common properties
- Validation status indicators
- Dependency tracking

Access via: `Edit > Content Browser` or press `Ctrl+Shift+C`

## Adding New Content Types

To add a new content type:

1. Create the folder in `game/assets/configs/`
2. Add the type to `AssetType` enum in `ContentDatabase.hpp`
3. Create a JSON schema in `game/assets/schemas/`
4. Add template(s) in `game/assets/templates/`
5. (Optional) Create a specialized browser in `game/src/editor/content/browsers/`

## Migration Guide

When upgrading from older versions:

1. Run the migration tool: `./tools/migrate_configs.py`
2. Review and fix any validation errors
3. Test all affected game systems
4. Commit the migrated files
