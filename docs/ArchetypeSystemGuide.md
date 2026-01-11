# Archetype System Guide

## Overview

The Archetype System provides a flexible, composition-based approach to defining game entities (units, buildings, spells) using **mixable behavior components**. Unlike traditional inheritance hierarchies, this system allows you to freely combine behaviors from any archetype category.

## Core Concepts

### 1. Behavior Components

Behaviors are modular pieces that define how entities act. Each behavior is self-contained and can be added to any archetype.

**Available Behavior Types:**

#### Spell Behaviors
- `TargetingBehavior` - How spell targets (single, AoE, cone, aura, etc.)
- `EffectBehavior` - What spell does (damage, heal, buff, debuff)
- `ProjectileBehavior` - How spell travels (linear, homing, bouncing)
- `ResourceBehavior` - Costs and cooldowns

#### Unit Behaviors
- `MovementBehavior` - How unit moves (ground, flying, phasing)
- `CombatBehavior` - Attack type and damage
- `WorkerBehavior` - Resource gathering and building
- `SpellcasterBehavior` - Mana pool and spells
- `DualAttackBehavior` - Both ranged and melee attacks

#### Building Behaviors
- `ResourceGenerationBehavior` - Resource production
- `SpawnerBehavior` - Unit production queue
- `HousingBehavior` - Population capacity
- `DefenseBehavior` - Armor and attack capability
- `BuildingAuraBehavior` - Area buffs

#### Cross-Category Behaviors
- `TransformationBehavior` - Change forms (unit ↔ building)
- `MobileBuildingBehavior` - Buildings that can move
- `DualAttackBehavior` - Mixed combat styles

### 2. Multiple Inheritance

Entities can inherit from **multiple parent archetypes**, combining their behaviors:

```json
{
  "id": "temple_of_light",
  "parentArchetypes": [
    "spawner_building",
    "resource_building"
  ]
}
```

This temple inherits:
- Unit spawning from `spawner_building`
- Resource generation from `resource_building`

### 3. Behavior Composition

Add any behavior to any archetype, regardless of its "primary" type:

```json
{
  "id": "ranger",
  "type": "unit",
  "behaviors": [
    {
      "type": "MovementBehavior",
      "config": { "speed": 5.5 }
    },
    {
      "type": "DualAttackBehavior",
      "config": {
        "rangedDamage": 15.0,
        "meleeDamage": 18.0
      }
    }
  ]
}
```

## Mix-and-Match Examples

### Building with Aura

**Temple of Light** combines:
- `SpawnerBehavior` (trains priests)
- `ResourceGenerationBehavior` (generates mana)
- `BuildingAuraBehavior` (buffs nearby allies)
- `DefenseBehavior` (garrison and armor)

```json
{
  "id": "temple_of_light",
  "type": "building",
  "parentArchetypes": ["spawner_building", "resource_building"],
  "behaviors": [
    {
      "type": "BuildingAuraBehavior",
      "config": {
        "auraName": "Divine Protection",
        "radius": 15.0,
        "statModifiers": {
          "armor": 3.0,
          "magicResist": 25.0
        }
      }
    }
  ]
}
```

### Ranged Unit with Melee Fallback

**Ranger** combines:
- `MovementBehavior` (fast ground movement)
- `DualAttackBehavior` (bow + sword)
- `CombatBehavior` (base attack stats)

```json
{
  "id": "ranger",
  "type": "unit",
  "behaviors": [
    {
      "type": "DualAttackBehavior",
      "config": {
        "attackMode": "AutoSwitch",
        "rangedDamage": 15.0,
        "rangedRange": 9.0,
        "meleeDamage": 18.0,
        "meleeRange": 1.5,
        "switchRange": 3.0
      }
    }
  ]
}
```

**How it works:**
- At range >3m: Uses bow (15 damage, 9m range)
- At range ≤3m: Draws sword (18 damage, 1.5m range)
- Auto-switches based on enemy distance

### Unit ↔ Building Transformation

**Treant** can root/uproot:

**Mobile Form (Unit):**
```json
{
  "id": "treant_mobile",
  "type": "unit",
  "behaviors": [
    {
      "type": "MovementBehavior",
      "config": { "speed": 3.0 }
    },
    {
      "type": "CombatBehavior",
      "config": { "damage": 40.0 }
    },
    {
      "type": "TransformationBehavior",
      "config": {
        "transformationType": "UnitToBuilding",
        "targetFormId": "treant_rooted",
        "transformTime": 3.0,
        "abilityName": "Root"
      }
    }
  ]
}
```

**Rooted Form (Building):**
```json
{
  "id": "treant_rooted",
  "type": "building",
  "behaviors": [
    {
      "type": "DefenseBehavior",
      "config": {
        "armor": 15.0,
        "healthRegen": 5.0
      }
    },
    {
      "type": "BuildingAuraBehavior",
      "config": {
        "auraName": "Nature's Blessing",
        "radius": 12.0,
        "statModifiers": {
          "healthRegen": 2.0
        }
      }
    },
    {
      "type": "TransformationBehavior",
      "config": {
        "transformationType": "BuildingToUnit",
        "targetFormId": "treant_mobile",
        "abilityName": "Uproot"
      }
    }
  ]
}
```

**Gameplay:**
- Mobile: Strong melee fighter, 3.0 speed
- Root (3s channel): Becomes immobile building
- Rooted: Healing aura (12m radius, +2 HP/s to allies)
- Uproot (5s channel): Return to mobile form
- Health/mana preserved across transformations

### Mobile Building (Siege Tank)

**Siege Tank** deploys/packs:

**Mobile Mode:**
- Moves at 2.5 speed
- 8m attack range
- 35 damage
- Can attack while moving

**Deployed Mode (becomes building):**
- Immobile (0 speed)
- 25m attack range
- 150 damage
- 2.5m splash radius
- 3m minimum range (vulnerability)

```json
{
  "type": "MobileBuildingBehavior",
  "config": {
    "mobilityMode": "DeploySystem",
    "packTime": 4.0,
    "unpackTime": 3.0,
    "canAttackWhileMoving": true
  }
}
```

## Creating Hybrid Entities

### Step 1: Choose Parent Archetypes

Pick 1-3 base archetypes that provide most of your behaviors:

```json
{
  "parentArchetypes": [
    "melee_unit",
    "spellcaster_unit"
  ]
}
```

### Step 2: Add Cross-Category Behaviors

Add behaviors from other categories:

```json
{
  "behaviors": [
    {
      "type": "BuildingAuraBehavior",
      "config": { /* ... */ }
    }
  ]
}
```

### Step 3: Override Stats

Override inherited stats:

```json
{
  "stats": {
    "health": 1200.0,
    "armor": 10.0
  }
}
```

## Behavior Compatibility Rules

### Compatible Combinations

✅ **Allowed:**
- Building + Aura
- Unit + Dual Attack
- Unit + Transformation (to building)
- Building + Transformation (to unit)
- Building + Mobile behavior
- Ranged + Melee (via DualAttackBehavior)
- Any + ResourceGeneration
- Any + Spawner

### Conflicting Behaviors

⚠️ **Handle with care:**
- Multiple MovementBehaviors (use priority resolution)
- Multiple CombatBehaviors (use DualAttackBehavior instead)
- Flying + Burrowing (mutually exclusive)

The system will use the **last defined** behavior if conflicts occur, or you can use `DualAttackBehavior` to explicitly handle dual combat modes.

## Common Patterns

### 1. Support Building with Combat

```json
{
  "id": "church",
  "parentArchetypes": ["spawner_building"],
  "behaviors": [
    {
      "type": "BuildingAuraBehavior",
      "config": {
        "statModifiers": { "healthRegen": 1.5 }
      }
    },
    {
      "type": "DefenseBehavior",
      "config": {
        "hasAttack": true,
        "attackRange": 10.0
      }
    }
  ]
}
```

### 2. Worker with Combat Capability

```json
{
  "id": "militia",
  "parentArchetypes": ["worker_unit"],
  "behaviors": [
    {
      "type": "CombatBehavior",
      "config": {
        "damage": 8.0,
        "attackRange": 1.5
      }
    }
  ]
}
```

### 3. Flying Spellcaster

```json
{
  "id": "dragon",
  "parentArchetypes": ["flying_unit", "mystic_unit"],
  "behaviors": [
    {
      "type": "CombatBehavior",
      "config": { "attackType": "Magic" }
    },
    {
      "type": "SpellcasterBehavior",
      "config": {
        "spells": ["dragon_breath", "fear"]
      }
    }
  ]
}
```

## JSON Structure Reference

```json
{
  "$schema": "../archetype_schema.json",
  "id": "unique_id",
  "name": "Display Name",
  "description": "Entity description",
  "type": "unit|building|spell",

  "parentArchetypes": [
    "base_archetype_1",
    "base_archetype_2"
  ],

  "behaviors": [
    {
      "type": "BehaviorTypeName",
      "config": {
        "param1": value1,
        "param2": value2
      }
    }
  ],

  "stats": {
    "health": 1000.0,
    "armor": 5.0
  },

  "properties": {
    "modelPath": "models/...",
    "iconPath": "icons/..."
  },

  "costs": {
    "resource1": amount1
  }
}
```

## Benefits of This System

1. **Flexibility**: Mix any behaviors regardless of entity "type"
2. **Reusability**: Define behaviors once, use everywhere
3. **Maintainability**: Change behavior definition → affects all users
4. **Experimentation**: Easy to test new combinations
5. **No Hard-Coding**: Everything defined in JSON configs
6. **Clear Composition**: See exactly what an entity can do

## Advanced Examples

### Portal Building (Spawner + Teleporter)

```json
{
  "id": "nether_portal",
  "type": "building",
  "parentArchetypes": ["spawner_building"],
  "behaviors": [
    {
      "type": "TeleportBehavior",
      "config": {
        "linkedPortalId": "overworld_portal",
        "teleportCooldown": 5.0,
        "canTeleportUnits": true
      }
    }
  ]
}
```

### Shapeshifter (Multiple Forms)

```json
{
  "id": "druid_bear_form",
  "behaviors": [
    {
      "type": "TransformationBehavior",
      "config": {
        "targetFormId": "druid_elf_form",
        "transformTime": 1.0
      }
    },
    {
      "type": "CombatBehavior",
      "config": {
        "damage": 35.0,
        "armor": 15.0
      }
    }
  ]
}
```

This system gives you complete freedom to define unique, interesting entities by combining behaviors in creative ways!
