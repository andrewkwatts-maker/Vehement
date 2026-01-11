# Human Race - Archetype-Based Configuration (v2)

This directory contains the archetype-based configuration for the Human race of Valorheim.

## Directory Structure

```
humans_v2/
├── units/           # 15 unit configurations
├── buildings/       # 6 building configurations
├── heroes/          # 2 hero configurations
├── CONVERSION_SUMMARY.md  # Detailed conversion documentation
└── README.md        # This file
```

## Quick Reference

### Base Archetypes Used

| Archetype | Used By |
|-----------|---------|
| `worker` | Peasant |
| `infantry` | Militia, Footman, Champion, Spellbreaker |
| `archer` | Archer, Crossbowman |
| `cavalry` | Scout, Knight, Paladin, Dragon Knight, Griffon Rider |
| `main_hall` | Town Hall |
| `farm` | Farm |
| `barracks` | Barracks |
| `tower` | Guard Tower |
| `warrior_hero` | Lord Commander |
| `mage_hero` | Archmage |

### Unit Categories

**Workers (2 units)**
- Peasant - Resource gathering, building, repairs
- Militia - Temporary combat form of peasants

**Infantry (4 units)**
- Militia - Basic melee (tier 1)
- Footman - Defensive infantry (tier 2)
- Champion - Elite infantry (tier 3)
- Spellbreaker - Anti-magic specialist (tier 2)

**Ranged (2 units)**
- Archer - Longbow user (tier 1)
- Crossbowman - Heavy ranged (tier 2)

**Cavalry (4 units)**
- Scout - Fast reconnaissance (tier 1)
- Knight - Heavy cavalry (tier 2)
- Paladin - Holy warrior (tier 3)
- Dragon Knight - Flying cavalry (tier 4)

**Casters (2 units)**
- Priest - Healer/support (tier 2)
- Mage - Area damage caster (tier 3)

**Special (1 unit)**
- Griffon Rider - Flying melee (tier 3)
- Trebuchet - Siege weapon (tier 4)

**Heroes (2 units)**
- Lord Commander - Tank/support hero
- Archmage - Nuker/disabler hero

### Buildings

**Economic**
- Town Hall - Main building, worker production
- Farm - Food production, population

**Military**
- Barracks - Infantry production
- Church - Priest production

**Support**
- Blacksmith - Weapon/armor upgrades
- Guard Tower - Defense

## Key Features

### Composition Patterns

**Multi-Role Units**
- Paladin = Cavalry + Healer + Support + Anti-Undead
- Priest = Healer + Buffer + Anti-Undead + Aura
- Scout = Light Cavalry + Vision + Mobility

**Specialist Units**
- Spellbreaker = Infantry + Anti-Magic (50 magic resist, spell steal, feedback)
- Crossbowman = Archer + Armor Penetration (50% pen) + Defense (pavise shield)
- Trebuchet = Siege + Special Ammo (poison, explosives, scatter)

**Transformation Units**
- Peasant ↔ Militia (bidirectional transformation)
- Call to Arms ability (temporary 60s transform)

### Interesting Compositions

1. **"Priest" Unit** = Mystic Unit + Heal Abilities + Anti-Undead + Support Aura
   - Heal: 75 HP restore
   - Dispel Magic: Remove buffs/debuffs
   - Inner Fire: +10% damage, +3 armor buff
   - Devotion Aura: +50% health regen

2. **"Paladin" Unit** = Heavy Cavalry + Holy Magic + Tank + Support
   - Holy Light: Heal allies 200 or damage undead 200
   - Divine Shield: 10s invulnerability
   - Devotion Aura: +3 armor to nearby units
   - Massive anti-undead damage (2.5x vs undead, 2.0x vs demons)

3. **"Spellbreaker" Unit** = Infantry + Anti-Magic Specialist
   - Spell Steal: Steal enemy buffs
   - Feedback: Burn mana for damage
   - Magic Immunity: 75% passive resistance
   - 2.0x damage vs casters

4. **"Trebuchet" Unit** = Siege + Special Ammunition + Setup System
   - Diseased Corpse: Poison cloud (50 DPS for 8s)
   - Explosive Barrel: 300 fire damage AoE
   - Scatter Shot: 5 projectiles wide area
   - Must setup (5s) before attacking

5. **"Dragon Knight" Unit** = Flying Cavalry + Breath Weapon + Anti-Air
   - Dragon Breath: 80 damage cone + 10 DoT
   - Aerial Superiority: +50% vs air
   - Extreme stats (1500 HP, 40 damage)

## New Behaviors Implemented

The conversion introduced these behavior components:

1. **Transformation System** - Peasant ↔ Militia
2. **Aura System** - Passive bonuses to nearby units
3. **Toggle Abilities** - Shield Wall, Defend, Flaming Arrows
4. **Charge Mechanics** - Knight charge with stun
5. **Setup/Deploy** - Trebuchet setup time
6. **Vision System** - Scout extended sight and reveal
7. **Hero Systems** - Levels, attributes, talents
8. **Channeled Abilities** - Blizzard, Mass Teleport
9. **Vulnerability System** - Trebuchet weaknesses
10. **Reflect Damage** - Shield Wall reflects melee

## Usage Example

### Creating a New Human Unit

```json
{
  "$schema": "../../../../schemas/unit_archetype.schema.json",
  "parentArchetype": "infantry",
  "id": "human_new_unit",
  "name": "New Unit",
  "race": "humans",

  "baseStats": {
    "health": 300,
    "damage": 15
  },

  "cost": {
    "gold": 150,
    "food": 2
  },

  "abilities": [
    {
      "id": "custom_ability",
      "name": "Custom Ability",
      "type": "active"
    }
  ],

  "visuals": {
    "model": "models/races/humans/units/new_unit.obj"
  }
}
```

### Inheriting and Overriding

The archetype system supports:
- **Full Inheritance**: Use all parent stats
- **Partial Override**: Change only specific values
- **Addition**: Add new abilities/behaviors
- **Composition**: Combine multiple behavior systems

## Integration Notes

- All entities are backward compatible with existing SDF models
- Model paths, animations, and textures preserved from original
- Sound files reference original audio assets
- Can be loaded alongside original human configs for testing

## See Also

- `CONVERSION_SUMMARY.md` - Detailed conversion documentation
- `/archetypes/` - Base archetype definitions
- `/schemas/` - JSON schemas for validation

---

Generated for Old3DEngine - Human Race Archetype System v2.0
