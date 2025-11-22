# Tutorial: Creating a Custom Ability

This tutorial shows how to create a custom spell or ability with visual effects, damage calculation, and cooldown management.

## Overview

We will create a "Chain Lightning" ability that:
- Damages an initial target
- Chains to nearby enemies
- Has diminishing damage per chain
- Includes visual and audio effects

## Prerequisites

- Completed [First Entity](first_entity.md) tutorial
- Understanding of the event system
- Basic Python knowledge (for scripting)

## Step 1: Define the Ability Configuration

Create `game/assets/configs/abilities/chain_lightning.json`:

```json
{
    "id": "chain_lightning",
    "name": "Chain Lightning",
    "description": "Launches a bolt of lightning that chains between enemies",
    "icon": "icons/abilities/chain_lightning.png",
    "type": "active",
    "activation": {
        "cost": 30,
        "cooldown": 8.0,
        "castTime": 0.3
    },
    "targeting": {
        "type": "target",
        "range": 25.0,
        "requiresEnemy": true
    },
    "effects": [
        {
            "type": "damage",
            "amount": 80,
            "damageType": "lightning"
        },
        {
            "type": "chain",
            "maxChains": 4,
            "chainRange": 8.0,
            "damageReduction": 0.25
        },
        {
            "type": "debuff",
            "id": "shocked",
            "duration": 2.0,
            "chance": 0.3
        }
    ],
    "visual": {
        "castParticle": "particles/lightning_charge.json",
        "projectileParticle": "particles/lightning_bolt.json",
        "impactParticle": "particles/lightning_impact.json"
    },
    "audio": {
        "cast": "sounds/abilities/lightning_cast.wav",
        "impact": "sounds/abilities/lightning_impact.wav",
        "chain": "sounds/abilities/lightning_chain.wav"
    },
    "upgrades": [
        {
            "level": 2,
            "modifiers": {
                "effects[0].amount": 100,
                "effects[1].maxChains": 5
            }
        },
        {
            "level": 3,
            "modifiers": {
                "effects[0].amount": 120,
                "effects[1].maxChains": 6,
                "effects[2].chance": 0.5
            }
        }
    ]
}
```

## Step 2: Create the Ability Class

Create `game/src/abilities/ChainLightning.hpp`:

```cpp
#pragma once

#include "Ability.hpp"
#include <vector>
#include <glm/glm.hpp>

namespace Vehement {

class ChainLightning : public Ability {
public:
    ChainLightning();

    // Ability interface
    bool CanCast(const AbilityContext& context) const override;
    void Cast(const AbilityContext& context) override;
    void OnCastComplete(const AbilityContext& context) override;

private:
    struct ChainTarget {
        uint64_t entityId;
        glm::vec3 position;
        float damage;
    };

    std::vector<ChainTarget> FindChainTargets(
        uint64_t initialTarget,
        const glm::vec3& startPos,
        int maxChains,
        float chainRange,
        float damageReduction);

    void ApplyDamage(uint64_t targetId, float damage);
    void ApplyDebuff(uint64_t targetId);
    void SpawnChainEffect(const glm::vec3& from, const glm::vec3& to);
    void PlayChainSound(const glm::vec3& position);

    // Config values
    float m_baseDamage = 80.0f;
    int m_maxChains = 4;
    float m_chainRange = 8.0f;
    float m_damageReduction = 0.25f;
    float m_debuffChance = 0.3f;
    float m_debuffDuration = 2.0f;
};

} // namespace Vehement
```

Create `game/src/abilities/ChainLightning.cpp`:

```cpp
#include "ChainLightning.hpp"
#include <systems/lifecycle/LifecycleManager.hpp>
#include <engine/reflection/EventBus.hpp>
#include <engine/spatial/SpatialIndex.hpp>
#include <engine/math/Random.hpp>
#include <engine/core/Logger.hpp>
#include <unordered_set>

namespace Vehement {

ChainLightning::ChainLightning() {
    m_id = "chain_lightning";
    m_type = AbilityType::Active;
}

bool ChainLightning::CanCast(const AbilityContext& context) const {
    // Base checks (cooldown, mana, etc.)
    if (!Ability::CanCast(context)) {
        return false;
    }

    // Require valid target
    if (!context.target.IsValid()) {
        return false;
    }

    // Check range
    float distance = glm::length(context.targetPosition - context.casterPosition);
    if (distance > m_range) {
        return false;
    }

    // Check line of sight (optional)
    // if (!HasLineOfSight(context.casterPosition, context.targetPosition)) {
    //     return false;
    // }

    return true;
}

void ChainLightning::Cast(const AbilityContext& context) {
    Logger::Info("Casting Chain Lightning");

    // Start cast animation/particles
    Nova::Reflect::BusEvent castEvt("OnAbilityCast", "ChainLightning");
    castEvt.SetData("casterId", context.caster.ToUint64());
    castEvt.SetData("abilityId", m_id);
    castEvt.SetData("position", context.casterPosition);
    Nova::Reflect::EventBus::Instance().Publish(castEvt);

    // Spawn cast particle
    SpawnParticle(m_config["visual"]["castParticle"], context.casterPosition);

    // Play cast sound
    PlaySound(m_config["audio"]["cast"], context.casterPosition);
}

void ChainLightning::OnCastComplete(const AbilityContext& context) {
    // Find chain targets
    auto targets = FindChainTargets(
        context.target.ToUint64(),
        context.targetPosition,
        m_maxChains,
        m_chainRange,
        m_damageReduction
    );

    // Apply damage to initial target
    float initialDamage = m_baseDamage * context.damageMultiplier;
    ApplyDamage(context.target.ToUint64(), initialDamage);

    // Spawn initial bolt
    SpawnChainEffect(context.casterPosition, context.targetPosition);
    SpawnParticle(m_config["visual"]["impactParticle"], context.targetPosition);
    PlaySound(m_config["audio"]["impact"], context.targetPosition);

    // Apply debuff to initial target
    if (Nova::Random::Float() < m_debuffChance) {
        ApplyDebuff(context.target.ToUint64());
    }

    // Process chain targets
    glm::vec3 lastPos = context.targetPosition;
    for (const auto& chainTarget : targets) {
        // Apply chain damage
        ApplyDamage(chainTarget.entityId, chainTarget.damage);

        // Spawn chain effect
        SpawnChainEffect(lastPos, chainTarget.position);
        SpawnParticle(m_config["visual"]["impactParticle"], chainTarget.position);
        PlayChainSound(chainTarget.position);

        // Apply debuff
        if (Nova::Random::Float() < m_debuffChance) {
            ApplyDebuff(chainTarget.entityId);
        }

        lastPos = chainTarget.position;
    }

    // Publish completion event
    Nova::Reflect::BusEvent completeEvt("OnAbilityComplete", "ChainLightning");
    completeEvt.SetData("casterId", context.caster.ToUint64());
    completeEvt.SetData("abilityId", m_id);
    completeEvt.SetData("targetsHit", static_cast<int>(targets.size() + 1));
    Nova::Reflect::EventBus::Instance().Publish(completeEvt);

    // Start cooldown
    StartCooldown();
}

std::vector<ChainLightning::ChainTarget> ChainLightning::FindChainTargets(
    uint64_t initialTarget,
    const glm::vec3& startPos,
    int maxChains,
    float chainRange,
    float damageReduction)
{
    std::vector<ChainTarget> targets;
    std::unordered_set<uint64_t> hitTargets;
    hitTargets.insert(initialTarget);

    glm::vec3 currentPos = startPos;
    float currentDamage = m_baseDamage;

    for (int i = 0; i < maxChains; i++) {
        // Reduce damage for chain
        currentDamage *= (1.0f - damageReduction);

        // Query nearby enemies
        auto& spatial = GetSpatialIndex();
        auto nearby = spatial.QuerySphere(currentPos, chainRange);

        // Find closest unhit enemy
        uint64_t nextTarget = 0;
        float minDist = chainRange;
        glm::vec3 nextPos;

        for (uint64_t id : nearby) {
            // Skip already hit targets
            if (hitTargets.count(id)) continue;

            // Check if enemy
            if (!IsEnemy(id)) continue;

            // Get position
            glm::vec3 pos = GetEntityPosition(id);
            float dist = glm::length(pos - currentPos);

            if (dist < minDist) {
                minDist = dist;
                nextTarget = id;
                nextPos = pos;
            }
        }

        // No more targets
        if (nextTarget == 0) break;

        // Add chain target
        targets.push_back({nextTarget, nextPos, currentDamage});
        hitTargets.insert(nextTarget);
        currentPos = nextPos;
    }

    return targets;
}

void ChainLightning::ApplyDamage(uint64_t targetId, float damage) {
    auto& manager = Lifecycle::GetGlobalLifecycleManager();
    LifecycleHandle handle = LifecycleHandle::FromUint64(targetId);

    if (!manager.IsAlive(handle)) return;

    Lifecycle::GameEvent evt(Lifecycle::EventType::Damaged, GetOwnerHandle());
    evt.SetData("damage", damage);
    evt.SetData("type", std::string("lightning"));
    evt.SetData("source", GetOwnerHandle().ToUint64());
    manager.SendEvent(handle, evt);
}

void ChainLightning::ApplyDebuff(uint64_t targetId) {
    Nova::Reflect::BusEvent evt("OnApplyDebuff", "ChainLightning");
    evt.SetData("targetId", targetId);
    evt.SetData("debuffId", std::string("shocked"));
    evt.SetData("duration", m_debuffDuration);
    Nova::Reflect::EventBus::Instance().Publish(evt);
}

void ChainLightning::SpawnChainEffect(const glm::vec3& from, const glm::vec3& to) {
    Nova::Reflect::BusEvent evt("OnSpawnLineEffect");
    evt.SetData("effectId", std::string("lightning_arc"));
    evt.SetData("start", from);
    evt.SetData("end", to);
    evt.SetData("duration", 0.2f);
    Nova::Reflect::EventBus::Instance().Publish(evt);
}

void ChainLightning::PlayChainSound(const glm::vec3& position) {
    PlaySound(m_config["audio"]["chain"], position);
}

} // namespace Vehement
```

## Step 3: Create Python Script Alternative

For rapid iteration, abilities can also be implemented in Python.

Create `scripts/abilities/chain_lightning.py`:

```python
"""Chain Lightning ability implementation."""

from nova import entities, events, math, debug
from nova import audio, particles

# Configuration
BASE_DAMAGE = 80
MAX_CHAINS = 4
CHAIN_RANGE = 8.0
DAMAGE_REDUCTION = 0.25
DEBUFF_CHANCE = 0.3
DEBUFF_DURATION = 2.0

def can_cast(caster, target, context):
    """Check if ability can be cast."""
    # Check mana
    mana = entities.get_component(caster, "Mana")
    if not mana or mana.current < 30:
        return False, "Not enough mana"

    # Check cooldown
    if is_on_cooldown(caster, "chain_lightning"):
        return False, "On cooldown"

    # Check target
    if not target or not entities.exists(target):
        return False, "Invalid target"

    # Check range
    caster_pos = entities.get_position(caster)
    target_pos = entities.get_position(target)
    if math.distance(caster_pos, target_pos) > 25.0:
        return False, "Out of range"

    # Check if enemy
    if not is_enemy(caster, target):
        return False, "Must target enemy"

    return True, None

def cast(caster, target, context):
    """Cast the chain lightning ability."""
    caster_pos = entities.get_position(caster)
    target_pos = entities.get_position(target)

    # Consume mana
    consume_mana(caster, 30)

    # Start cooldown
    start_cooldown(caster, "chain_lightning", 8.0)

    # Play cast effects
    particles.spawn("lightning_charge", caster_pos)
    audio.play_at("sounds/abilities/lightning_cast.wav", caster_pos)

    # Find chain targets
    chain_targets = find_chain_targets(target, target_pos)

    # Apply damage to initial target
    damage = BASE_DAMAGE * context.get("damage_multiplier", 1.0)
    apply_damage(target, damage, "lightning", caster)

    # Spawn initial bolt
    spawn_lightning_bolt(caster_pos, target_pos)
    particles.spawn("lightning_impact", target_pos)
    audio.play_at("sounds/abilities/lightning_impact.wav", target_pos)

    # Apply debuff chance
    if math.random() < DEBUFF_CHANCE:
        apply_debuff(target, "shocked", DEBUFF_DURATION)

    # Process chains
    last_pos = target_pos
    for chain_target in chain_targets:
        apply_damage(chain_target["id"], chain_target["damage"], "lightning", caster)
        spawn_lightning_bolt(last_pos, chain_target["pos"])
        particles.spawn("lightning_impact", chain_target["pos"])
        audio.play_at("sounds/abilities/lightning_chain.wav", chain_target["pos"])

        if math.random() < DEBUFF_CHANCE:
            apply_debuff(chain_target["id"], "shocked", DEBUFF_DURATION)

        last_pos = chain_target["pos"]

    # Publish event
    events.publish("OnAbilityComplete", {
        "caster": caster,
        "ability": "chain_lightning",
        "targets_hit": len(chain_targets) + 1
    })

def find_chain_targets(initial_target, start_pos):
    """Find nearby enemies to chain to."""
    targets = []
    hit_ids = {initial_target}
    current_pos = start_pos
    current_damage = BASE_DAMAGE

    for _ in range(MAX_CHAINS):
        current_damage *= (1.0 - DAMAGE_REDUCTION)

        # Find nearby enemies
        nearby = entities.query_sphere(current_pos, CHAIN_RANGE)

        # Find closest unhit enemy
        best_target = None
        best_dist = CHAIN_RANGE

        for entity_id in nearby:
            if entity_id in hit_ids:
                continue
            if not is_enemy_entity(entity_id):
                continue

            pos = entities.get_position(entity_id)
            dist = math.distance(current_pos, pos)

            if dist < best_dist:
                best_dist = dist
                best_target = {
                    "id": entity_id,
                    "pos": pos,
                    "damage": current_damage
                }

        if not best_target:
            break

        targets.append(best_target)
        hit_ids.add(best_target["id"])
        current_pos = best_target["pos"]

    return targets

def spawn_lightning_bolt(start, end):
    """Spawn lightning bolt effect between two points."""
    events.publish("OnSpawnLineEffect", {
        "effect": "lightning_arc",
        "start": start,
        "end": end,
        "duration": 0.2
    })

def apply_damage(target, amount, damage_type, source):
    """Apply damage to target."""
    events.publish("OnDamage", {
        "target": target,
        "damage": amount,
        "type": damage_type,
        "source": source
    })

def apply_debuff(target, debuff_id, duration):
    """Apply debuff to target."""
    events.publish("OnApplyDebuff", {
        "target": target,
        "debuff": debuff_id,
        "duration": duration
    })

# Helper functions (would be in a utility module)
def is_enemy(caster, target):
    return entities.get_faction(caster) != entities.get_faction(target)

def is_enemy_entity(entity_id):
    return entities.has_tag(entity_id, "hostile")

def consume_mana(entity, amount):
    mana = entities.get_component(entity, "Mana")
    if mana:
        mana.current -= amount

def is_on_cooldown(entity, ability_id):
    cooldowns = entities.get_component(entity, "Cooldowns")
    return cooldowns and ability_id in cooldowns.active

def start_cooldown(entity, ability_id, duration):
    events.publish("OnStartCooldown", {
        "entity": entity,
        "ability": ability_id,
        "duration": duration
    })
```

## Step 4: Register and Use the Ability

### C++ Registration

```cpp
void RegisterAbilities() {
    auto& registry = AbilityRegistry::Instance();

    // Register chain lightning
    registry.Register<Vehement::ChainLightning>("chain_lightning");
}
```

### Using from Code

```cpp
// Give ability to player
player.AddAbility("chain_lightning");

// Cast ability
AbilityContext context;
context.caster = playerHandle;
context.target = enemyHandle;
context.casterPosition = player.GetPosition();
context.targetPosition = enemy.GetPosition();
context.damageMultiplier = 1.0f;

if (auto* ability = player.GetAbility("chain_lightning")) {
    if (ability->CanCast(context)) {
        ability->Cast(context);
    }
}
```

### Using from Python

```python
from scripts.abilities import chain_lightning

# In player input handler
def on_ability_key(ability_num, player, target):
    if ability_num == 2:  # Q key
        can_cast, error = chain_lightning.can_cast(player, target, {})
        if can_cast:
            chain_lightning.cast(player, target, {"damage_multiplier": 1.0})
        else:
            show_error_message(error)
```

## Step 5: Add Visual Effects

Create particle configuration `game/assets/particles/lightning_arc.json`:

```json
{
    "id": "lightning_arc",
    "type": "line",
    "emitter": {
        "type": "line",
        "rate": 50,
        "duration": 0.2
    },
    "particle": {
        "texture": "textures/particles/spark.png",
        "lifetime": {"min": 0.1, "max": 0.2},
        "size": {"start": 0.3, "end": 0.1},
        "color": {
            "start": [0.5, 0.7, 1.0, 1.0],
            "end": [0.2, 0.4, 1.0, 0.0]
        },
        "velocity": {
            "min": [-1, -1, -1],
            "max": [1, 1, 1]
        }
    },
    "rendering": {
        "blendMode": "additive"
    }
}
```

## Summary

You have learned how to:
- Define ability configurations in JSON
- Implement abilities in C++ with proper patterns
- Create Python scripts for rapid ability development
- Handle targeting and chain effects
- Integrate visual and audio effects
- Register and use abilities in gameplay

## Next Steps

- [Creating AI Behavior](ai_behavior.md) - Make enemies use abilities
- [Animation System](../ANIMATION_GUIDE.md) - Add cast animations
- [Config Reference](../CONFIG_REFERENCE.md) - Full ability schema
