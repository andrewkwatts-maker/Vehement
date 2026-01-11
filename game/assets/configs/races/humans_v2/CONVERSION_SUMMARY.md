# Human Race Archetype Conversion Summary

This document summarizes the conversion of all human race entities from the legacy hardcoded system to the new composition-based archetype system.

## Conversion Statistics

- **Total Entities Converted**: 23
  - **Units**: 15
  - **Buildings**: 6
  - **Heroes**: 2

## Base Archetypes Used

The conversion leveraged the following base archetypes:

### Unit Archetypes
1. **worker** - Used for Peasant
2. **infantry** - Used for Militia, Footman, Champion, Spellbreaker
3. **archer** - Used for Archer, Crossbowman
4. **cavalry** - Used for Knight, Paladin, Dragon Knight, Scout, Griffon Rider

### Building Archetypes
1. **main_hall** - Used for Town Hall
2. **farm** - Used for Farm
3. **barracks** - Used for Barracks
4. **tower** - Used for Guard Tower

### Hero Archetypes
1. **warrior_hero** - Used for Lord Commander
2. **mage_hero** - Used for Archmage

### Custom Categories (No Base Archetype)
Some units required custom implementations due to their unique nature:
- **Caster Units** (Priest, Mage) - Support/magic category
- **Siege Units** (Trebuchet) - Siege category
- **Custom Buildings** (Blacksmith, Church) - Upgrade/support buildings

## Converted Units

### 1. Worker Units
**Peasant** (`human_peasant`)
- Inherits from: `worker` archetype
- Key Features:
  - Resource gathering (gold: 10/s, wood: 12/s, stone: 8/s, food: 10/s)
  - Building construction (speed multiplier: 1.0)
  - Repair capability (rate: 5.0)
  - "Call to Arms" ability - transforms to militia when needed
- Composition: Worker + weak combat capabilities

**Militia** (`human_militia`)
- Inherits from: `infantry` archetype
- Key Features:
  - Temporary combat form of peasant
  - "Return to Work" ability - transforms back to peasant
  - Light armor, basic melee combat
- Composition: Basic infantry with transformation mechanics

### 2. Infantry Units

**Footman** (`human_footman`)
- Inherits from: `infantry` archetype
- Key Overrides:
  - Health: 420 (vs 100 base)
  - Armor: 5 (medium armor)
  - Magic Resist: 10
- Unique Behaviors:
  - **Shield Wall**: Toggle ability providing 40% damage reduction, 50% ranged resistance, stacks with nearby footmen
  - **Defend**: Toggle ability for 30% damage reduction
  - **Cleave Attack**: Passive 15% cleave damage in 1.5 radius
- Composition: Infantry + defensive abilities + area damage

**Champion** (`human_champion`)
- Inherits from: `infantry` archetype
- Key Overrides:
  - Health: 650
  - Armor: 8 (heavy armor)
  - Damage: 22 (high tier)
- Unique Behaviors:
  - **Whirlwind**: Active AoE ability dealing 1.5x damage in 3.0 radius
  - **Battle Hardened**: Passive giving 20% damage reduction when below 50% health
- Composition: Elite infantry + survivability + AoE damage

**Spellbreaker** (`human_spellbreaker`)
- Inherits from: `infantry` archetype
- Key Overrides:
  - Magic Resist: 50 (anti-magic specialist)
  - Attack Type: Magic
- Unique Behaviors:
  - **Spell Steal**: Steal buffs from enemies
  - **Feedback**: Damage equal to target's mana and drain it
  - **Magic Immunity**: Passive 75% magic immunity
  - Bonus vs casters (2.0x) and summoned units (1.5x)
- Composition: Infantry + anti-magic abilities + spell manipulation

### 3. Ranged Units

**Archer** (`human_archer`)
- Inherits from: `archer` archetype
- Key Overrides:
  - Health: 160
  - Range: 14.0
  - Damage: 12
- Unique Behaviors:
  - **Flaming Arrows**: Toggle ability adding fire DoT (5 damage/3s)
  - Projectile system with gravity and trail effects
- Composition: Archer + elemental damage enhancement

**Crossbowman** (`human_crossbowman`)
- Inherits from: `archer` archetype
- Key Overrides:
  - Health: 220
  - Armor: 2 (medium armor)
  - Damage: 20 (higher damage, slower attack)
  - Attack Speed: 2.0 (slower than archer)
- Unique Behaviors:
  - **Piercing Bolt**: Passive ignoring 50% of enemy armor
  - **Pavise**: Toggle shield providing 75% ranged damage reduction when stationary
  - Bonus vs heavy armor (1.5x)
- Composition: Heavy archer + armor penetration + defensive stance

### 4. Cavalry Units

**Scout** (`human_scout`)
- Inherits from: `cavalry` archetype
- Key Overrides:
  - Health: 180 (light cavalry)
  - Move Speed: 8.0 (very fast)
  - Vision Range: 16.0 (extended sight)
- Unique Behaviors:
  - **Flare**: Reveal area ability (12.0 radius, 10s duration)
  - **Sprint**: 50% movement speed boost for 5 seconds
  - Enhanced vision system with night vision (12.0)
- Composition: Light cavalry + reconnaissance + mobility

**Knight** (`human_knight`)
- Inherits from: `cavalry` archetype
- Key Overrides:
  - Health: 800 (heavy cavalry)
  - Armor: 6
  - Damage: 28
- Unique Behaviors:
  - **Charge**: 250% damage + 1s stun (8s cooldown)
  - **Trample**: Passive damage to units during charge
  - **Splash Attack**: 25% splash damage in 2.0 radius
- Composition: Heavy cavalry + charge mechanics + AoE damage

**Paladin** (`human_paladin`)
- Inherits from: `cavalry` archetype
- Key Overrides:
  - Health: 1200 (very tanky)
  - Mana: 200 (hybrid warrior-caster)
  - Attack Type: Holy
  - Bonus vs undead (2.5x) and demons (2.0x)
- Unique Behaviors:
  - **Holy Light**: Heal 200 or damage undead 200
  - **Divine Shield**: 10s invulnerability (60s cooldown)
  - **Devotion Aura**: +3 armor to nearby allies
- Composition: Cavalry + holy magic + support aura + anti-undead

**Dragon Knight** (`human_dragon_knight`)
- Inherits from: `cavalry` archetype
- Key Overrides:
  - Health: 1500 (ultimate unit)
  - Movement Type: Air
  - Damage: 40
  - Tier: 4 (late game)
- Unique Behaviors:
  - **Dragon Breath**: Cone fire damage (80 + 10 DoT for 4s)
  - **Aerial Superiority**: 50% bonus damage vs air units
  - Flying mechanics with fire attacks
- Composition: Flying cavalry + breath weapon + anti-air

### 5. Caster Units

**Priest** (`human_priest`)
- Custom implementation (support category)
- Key Stats:
  - Health: 200
  - Mana: 250
  - Mana Regen: 1.5
  - Magic Resist: 20
- Unique Behaviors:
  - **Heal**: Restore 75 health (auto-cast enabled)
  - **Dispel Magic**: Remove buffs/debuffs in area
  - **Inner Fire**: +10% damage + 3 armor buff
  - **Devotion Aura**: +50% health regen to nearby allies
  - Bonus vs undead (2.0x) and demons (1.5x)
- Composition: Healer + support magic + anti-undead + aura

**Mage** (`human_mage`)
- Custom implementation (caster category)
- Key Stats:
  - Health: 280
  - Mana: 400
  - Mana Regen: 2.5
  - Spell Power: 100
- Unique Behaviors:
  - **Fireball**: 120 damage + 60 splash
  - **Blizzard**: 40 DPS AoE for 6s + 30% slow
  - **Polymorph**: Transform enemy to sheep for 15s
  - **Arcane Brilliance**: +100% mana regen aura
- Composition: Pure caster + crowd control + area damage + support

### 6. Special Units

**Griffon Rider** (`human_griffon_rider`)
- Inherits from: `cavalry` archetype (adapted for air)
- Key Overrides:
  - Movement Type: Air
  - Flight Height: 8.0
  - Can attack both air and ground
- Unique Behaviors:
  - **Dive Attack**: 150% damage + 1.5s stun (requires flight)
  - **Talon Strike**: Multi-target attack (up to 3 enemies)
  - **Screech**: -25% enemy attack damage debuff
  - Flight mechanics (can land, ignore terrain/walls)
- Composition: Flying cavalry + dive attacks + debuff

**Trebuchet** (`human_trebuchet`)
- Custom implementation (siege category)
- Key Stats:
  - Health: 800
  - Armor: 8 (fortified)
  - Damage: 200 (massive)
  - Range: 40.0 (extreme range)
  - Min Range: 15.0
  - Move Speed: 1.5 (very slow)
- Unique Behaviors:
  - **Diseased Corpse**: Poison cloud AoE
  - **Explosive Barrel**: 300 damage fire AoE
  - **Scatter Shot**: 5 projectiles covering large area
  - **Ground Attack**: Attack without vision requirement
  - Setup/packup mechanics (5s/4s)
  - Bonus vs buildings (3.0x)
  - Vulnerabilities: Fire (1.75x), Melee (1.5x), Cavalry (2.0x)
- Composition: Siege weapon + setup mechanics + special ammunition + vulnerabilities

## Converted Buildings

### 1. Main Buildings

**Town Hall** (`human_town_hall`)
- Inherits from: `main_hall` archetype
- Key Features:
  - Health: 1500
  - Garrison: 8 units
  - Attack capability: 10 damage, 12 range
  - Produces peasants
  - Age advancement research
- Unique Behaviors:
  - **Town Bell**: Transform all peasants to militia for 60s (180s cooldown)
  - **Town Center Aura**: +10% gather rate for peasants in 20.0 radius
- Composition: Resource drop-off + production + age advancement + defensive + aura

### 2. Economic Buildings

**Farm** (`human_farm`)
- Inherits from: `farm` archetype
- Key Features:
  - Provides: 6 population
  - Resource Generation: 0.1 food/second
  - Low cost (80 gold, 60 wood)
- Composition: Population + passive resource generation

**Blacksmith** (`human_blacksmith`)
- Custom implementation (upgrade category)
- Key Features:
  - Researches all weapon and armor upgrades
  - Melee damage/armor tiers 1-2
  - Ranged damage/armor tiers 1-2
- Composition: Technology research building

### 3. Military Buildings

**Barracks** (`human_barracks`)
- Inherits from: `barracks` archetype
- Key Features:
  - Health: 1200
  - Produces: Militia, Footman, Champion
  - Garrison: 6 units (heals at 2.0/s)
  - Researches infantry upgrades
- Composition: Military production + garrison healing + tech research

**Church** (`human_church`)
- Custom implementation (military/support)
- Key Features:
  - Produces: Priest
  - Researches: Devotion Aura, healing upgrades
- Unique Behaviors:
  - **Holy Ground Aura**: +5 magic resist to allies in 15.0 radius
- Composition: Caster production + tech research + support aura

### 4. Defensive Buildings

**Guard Tower** (`human_guard_tower`)
- Inherits from: `tower` archetype
- Key Features:
  - Health: 600
  - Attack: 20 damage, 12 range, pierce type
  - Garrison: 4 units (each adds +5 attack)
  - Can target air and ground
  - Upgrades to Cannon Tower
- Composition: Defensive structure + garrison bonus + upgrade path

## Converted Heroes

### Lord Commander (`human_lord_commander`)
- Inherits from: `warrior_hero` archetype
- Primary Attribute: Strength
- Key Overrides:
  - Health: 800 (+ 80/level)
  - Mana: 200
  - Armor: 6
  - Strength: 28 (+ 3.2/level)
- Hero Abilities:
  - **Shield Wall** (Q): 75% damage reduction + 30% reflect at max level
  - **Rallying Cry** (W): +30% damage + 25% speed to allies (18 radius)
  - **Blade Storm** (E): 170 DPS in 4.5 radius, magic immune
  - **Avatar of War** (R - Ultimate): Massive stat bonuses, half to nearby allies
- Passive:
  - **Commanding Presence**: +2 armor to nearby allies, +10% damage when focusing enemies attacking hero
- Composition: Tank hero + aura support + damage + crowd presence

### Archmage (`human_archmage`)
- Inherits from: `mage_hero` archetype
- Primary Attribute: Intelligence
- Key Overrides:
  - Health: 450 (+ 40/level)
  - Mana: 500 (+ 60/level)
  - Intelligence: 32 (+ 4.0/level)
  - Magic Resist: 40
- Hero Abilities:
  - **Fireball** (Q): 375 damage + 50 DoT at max level
  - **Blizzard** (W): 110 damage/wave, 9 radius, 60% slow
  - **Polymorph** (E): 25s disable (5s on heroes)
  - **Mass Teleport** (R - Ultimate): Teleport 24 units to friendly building
- Passive:
  - **Arcane Mastery**: +15% spell damage, -10% cooldowns
- Aura:
  - **Brilliance Aura**: +1.5 mana regen to nearby allies
- Composition: Nuker hero + crowd control + support aura + strategic mobility

## Key Archetype Patterns Demonstrated

### 1. Inheritance Patterns

**Simple Inheritance**:
```json
{
  "parentArchetype": "worker",
  "id": "human_peasant",
  // Override only specific stats
  "baseStats": {
    "health": 40,
    "moveSpeed": 4.5
  }
}
```

**Inheritance with Additions**:
```json
{
  "parentArchetype": "infantry",
  "id": "human_footman",
  // Add race-specific abilities
  "abilities": [
    {"id": "shield_wall", ...},
    {"id": "defend", ...}
  ]
}
```

### 2. Composition Patterns

**Multi-Role Unit** (Paladin):
- Base: Cavalry archetype
- Added: Mana system (200 mana, 1.0 regen)
- Added: Healing abilities
- Added: Support aura
- Added: Anti-undead bonuses
- Result: Cavalry + Healer + Support + Anti-Undead

**Specialist Unit** (Spellbreaker):
- Base: Infantry archetype
- Added: High magic resist (50)
- Added: Anti-magic abilities (Spell Steal, Feedback)
- Added: Magic immunity passive (75%)
- Result: Infantry + Anti-Caster Specialist

**Transformation Unit** (Militia):
- Base: Infantry archetype
- Added: Transformation mechanics
- Added: Bidirectional transform (Peasant ↔ Militia)
- Result: Infantry + Dynamic Role Switching

### 3. Behavior Composition Examples

**Scout** = Light Cavalry + Vision Extension + Reveal Ability + Sprint
```json
{
  "parentArchetype": "cavalry",
  "baseStats": {"sightRange": 16.0, "moveSpeed": 8.0},
  "vision": {"nightVision": 12.0, "detectsInvisible": false},
  "abilities": [
    {"id": "flare", "revealRadius": 12.0},
    {"id": "sprint", "speedBonus": 0.50}
  ]
}
```

**Priest** = Support Caster + Healer + Buffer + Anti-Undead + Aura
```json
{
  "category": "support",
  "bonusVs": [{"category": "undead", "multiplier": 2.0}],
  "abilities": [
    {"id": "heal", "autoCast": true},
    {"id": "inner_fire", "effects": [...]},
    {"id": "devotion_aura", "type": "aura"}
  ]
}
```

**Trebuchet** = Siege Weapon + Setup Mechanics + Special Ammo + Vulnerabilities
```json
{
  "category": "siege",
  "setup": {"required": true, "setupTime": 5.0},
  "abilities": [
    {"id": "diseased_corpse"},
    {"id": "explosive_barrel"},
    {"id": "scatter_shot"}
  ],
  "vulnerabilities": {"fire": 1.75, "cavalry": 2.0}
}
```

## New Behaviors Required for Humans

Based on the conversion, the following new behavior components/systems are needed:

### 1. Transformation System
- **Bidirectional Transforms**: Peasant ↔ Militia
- **Temporary Transforms**: Call to Arms ability
- **State Preservation**: Maintain health/position during transform

### 2. Aura System
- **Passive Auras**: Devotion Aura, Brilliance Aura, Commanding Presence
- **Building Auras**: Town Center gathering bonus, Holy Ground magic resist
- **Conditional Auras**: Avatar of War (different bonuses for self vs allies)
- **Stacking Mechanics**: Shield Wall ability stacks with nearby footmen

### 3. Stance/Toggle System
- **Toggle Abilities**: Shield Wall, Defend, Flaming Arrows
- **Movement Penalties**: Reduced speed while defending
- **Resource Costs**: Mana per shot (Flaming Arrows)
- **State Persistence**: Toggle states survive save/load

### 4. Special Attack Mechanics
- **Cleave/Splash**: Footman cleave, Knight splash
- **Armor Penetration**: Crossbowman piercing bolts (50% penetration)
- **Multi-Target**: Talon Strike (3 targets)
- **Reflect Damage**: Shield Wall reflects melee damage

### 5. Setup/Deploy System
- **Setup Time**: Trebuchet 5s setup before attack
- **Packup Time**: 4s to become mobile
- **State Restrictions**: Cannot attack while moving
- **Visual States**: Different models for deployed/packed

### 6. Vision/Reveal System
- **Extended Vision**: Scout 16.0 range
- **Night Vision**: 12.0 range at night
- **Active Reveal**: Flare ability
- **Temporary Vision**: Reveal duration tracking

### 7. Charge Mechanics
- **Minimum Distance**: Must be 5.0+ units away
- **Damage Multiplier**: 250% damage on charge
- **CC Effect**: Stun on impact
- **Speed Boost**: Movement bonus during charge
- **Trample**: Damage all units passed through

### 8. Hero-Specific Systems
- **Level System**: 1-25 progression
- **Attribute System**: Strength, Agility, Intelligence
- **Stat Scaling**: Per-level growth formulas
- **Ability Levels**: 1-4 levels per ability
- **Ultimate Abilities**: Unlock at level 6
- **Talent Trees**: Unlock at levels 10, 15, 20, 25
- **Respawn System**: Time = base + (level × modifier)

### 9. Channeled Abilities
- **Blizzard**: 8 waves over 8 seconds
- **Mass Teleport**: 3s channel time
- **Interrupt Detection**: Can be interrupted
- **Mana Drain**: Continuous mana cost

### 10. Vulnerability System
- **Type-Specific**: Trebuchet vulnerable to fire (1.75x), cavalry (2.0x)
- **Siege Weaknesses**: High vulnerability to fast melee units
- **Bonus/Penalty Tracking**: Dynamic damage calculations

## Backward Compatibility

All new archetype-based entities maintain backward compatibility with existing SDF models:

- **Model Paths**: Preserved original model references
- **Animation Sets**: All animation names maintained
- **Texture Paths**: Diffuse, normal, specular maps preserved
- **Sound Files**: All sound effect paths unchanged
- **Visual Scale**: Original scale values maintained

## Implementation Notes

### Stat Inheritance Rules
1. If `parentArchetype` is specified, inherit all baseStats from parent
2. Override individual stats by specifying them in the entity's baseStats
3. Composition adds new fields without removing inherited ones

### Ability System
- Abilities can be inherited from archetypes
- Race-specific abilities are added to the abilities array
- Passive abilities use `"type": "passive"`
- Toggle abilities use `"type": "toggle"`
- Active abilities use `"type": "active"`
- Aura abilities use `"type": "aura"`

### Cost Inheritance
- Costs are NOT inherited by default
- Each unit must specify its own cost structure
- Allows for race-specific pricing strategies

### Building Production Queues
- Specify what can be produced/researched
- Include age and building requirements
- Support for both units and technologies

## Summary

The conversion successfully demonstrates:

1. **Clear Inheritance**: 15 of 23 entities use base archetypes
2. **Composition Over Hardcoding**: All complex behaviors use component composition
3. **Stat Overrides**: Only specify what differs from parent
4. **Race Identity**: Human-specific abilities and behaviors preserved
5. **Scalability**: Easy to add new units by extending archetypes
6. **Maintainability**: Centralized archetype changes affect all derived units

The human race conversion serves as a reference implementation for converting other races (Orcs, Elves, Undead, Dwarves, Demons, Mechanoids) to the archetype system.
