# Tutorial: Creating Your First Entity

This tutorial walks you through creating your first game entity in Nova3D Engine, from configuration to spawning in the game world.

## Overview

In this tutorial, you will:
1. Define an entity configuration in JSON
2. Create the entity class in C++
3. Register the entity type
4. Spawn and interact with the entity

## Prerequisites

- Nova3D Engine built and running
- Basic understanding of C++ and JSON
- Editor access for testing

## Step 1: Define the Configuration

First, create a JSON configuration file for your entity.

Create `game/assets/configs/units/my_goblin.json`:

```json
{
    "id": "my_goblin",
    "type": "infantry",
    "name": "Goblin Scout",
    "description": "A sneaky goblin that scouts the area",
    "model": "models/enemies/goblin.fbx",
    "icon": "icons/enemies/goblin.png",
    "stats": {
        "health": 50,
        "armor": 5,
        "damage": 8,
        "attackSpeed": 1.2,
        "moveSpeed": 4.5,
        "range": 1.5,
        "cost": 25
    },
    "abilities": [
        "melee_attack",
        "stealth"
    ],
    "tags": [
        "goblin",
        "melee",
        "scout",
        "hostile"
    ],
    "ai": {
        "behavior": "aggressive",
        "detectionRange": 15.0,
        "fleeHealthPercent": 0.2
    },
    "loot": {
        "gold": {
            "min": 5,
            "max": 15
        },
        "drops": [
            {
                "item": "goblin_ear",
                "chance": 0.5
            }
        ]
    }
}
```

## Step 2: Create the Entity Class

Create `game/src/entities/Goblin.hpp`:

```cpp
#pragma once

#include <systems/lifecycle/ILifecycle.hpp>
#include <engine/reflection/TypeRegistry.hpp>
#include <glm/glm.hpp>
#include <string>

namespace Vehement {

/**
 * @brief A goblin enemy entity
 */
class Goblin : public Lifecycle::LifecycleBase {
public:
    Goblin();
    ~Goblin() override = default;

    // Lifecycle methods
    void OnCreate(const nlohmann::json& config) override;
    void OnTick(float deltaTime) override;
    void OnDestroy() override;
    bool OnEvent(Lifecycle::GameEvent& event) override;

    // Combat
    void TakeDamage(float damage, const std::string& damageType = "physical");
    void Attack(LifecycleHandle target);

    // State
    [[nodiscard]] float GetHealth() const { return m_health; }
    [[nodiscard]] float GetMaxHealth() const { return m_maxHealth; }
    [[nodiscard]] bool IsAlive() const { return m_health > 0; }
    [[nodiscard]] bool IsStealthed() const { return m_stealthed; }

    // Position
    [[nodiscard]] const glm::vec3& GetPosition() const { return m_position; }
    void SetPosition(const glm::vec3& pos) { m_position = pos; }

private:
    void UpdateAI(float deltaTime);
    void FindTarget();
    void MoveTowardsTarget(float deltaTime);
    void EnterStealth();
    void ExitStealth();
    void Die();
    void DropLoot();

    // Stats (loaded from config)
    float m_health = 50.0f;
    float m_maxHealth = 50.0f;
    float m_armor = 5.0f;
    float m_damage = 8.0f;
    float m_attackSpeed = 1.2f;
    float m_moveSpeed = 4.5f;
    float m_attackRange = 1.5f;

    // AI
    float m_detectionRange = 15.0f;
    float m_fleeHealthPercent = 0.2f;

    // State
    glm::vec3 m_position{0.0f};
    glm::vec3 m_targetPosition{0.0f};
    LifecycleHandle m_currentTarget;
    float m_attackCooldown = 0.0f;
    bool m_stealthed = false;
    float m_stealthTimer = 0.0f;

    // Configuration
    std::string m_configId;
    nlohmann::json m_lootConfig;
};

// Reflection registration
REFLECT_TYPE(Goblin)
REFLECT_TYPE_BEGIN(Goblin)
    REFLECT_PROPERTY(m_health, REFLECT_ATTR_EDITABLE REFLECT_ATTR_RANGE(0, 100))
    REFLECT_PROPERTY(m_maxHealth, REFLECT_ATTR_EDITABLE)
    REFLECT_PROPERTY(m_damage, REFLECT_ATTR_EDITABLE)
    REFLECT_PROPERTY(m_moveSpeed, REFLECT_ATTR_EDITABLE)
    REFLECT_PROPERTY(m_position, REFLECT_ATTR_EDITABLE)
REFLECT_TYPE_END()

} // namespace Vehement
```

Create `game/src/entities/Goblin.cpp`:

```cpp
#include "Goblin.hpp"
#include <systems/lifecycle/LifecycleManager.hpp>
#include <engine/reflection/EventBus.hpp>
#include <engine/math/Random.hpp>
#include <engine/core/Logger.hpp>

namespace Vehement {

Goblin::Goblin() {
    // Set tick rate for AI updates
    SetTickRate(TickRate::Normal);
}

void Goblin::OnCreate(const nlohmann::json& config) {
    Logger::Info("Creating Goblin from config");

    // Load stats from config
    if (config.contains("stats")) {
        const auto& stats = config["stats"];
        m_health = stats.value("health", 50.0f);
        m_maxHealth = m_health;
        m_armor = stats.value("armor", 5.0f);
        m_damage = stats.value("damage", 8.0f);
        m_attackSpeed = stats.value("attackSpeed", 1.2f);
        m_moveSpeed = stats.value("moveSpeed", 4.5f);
        m_attackRange = stats.value("range", 1.5f);
    }

    // Load AI settings
    if (config.contains("ai")) {
        const auto& ai = config["ai"];
        m_detectionRange = ai.value("detectionRange", 15.0f);
        m_fleeHealthPercent = ai.value("fleeHealthPercent", 0.2f);
    }

    // Store loot config for later
    if (config.contains("loot")) {
        m_lootConfig = config["loot"];
    }

    // Store config ID
    m_configId = config.value("id", "unknown");

    // Load position if specified
    if (config.contains("position")) {
        const auto& pos = config["position"];
        m_position = glm::vec3(pos[0], pos[1], pos[2]);
    }

    // Publish spawn event
    Nova::Reflect::BusEvent evt("OnEntitySpawned", "Goblin", GetHandle().ToUint64());
    evt.SetData("configId", m_configId);
    evt.SetData("position", m_position);
    Nova::Reflect::EventBus::Instance().Publish(evt);
}

void Goblin::OnTick(float deltaTime) {
    if (!IsAlive()) return;

    // Update cooldowns
    m_attackCooldown = std::max(0.0f, m_attackCooldown - deltaTime);

    // Update stealth
    if (m_stealthed) {
        m_stealthTimer -= deltaTime;
        if (m_stealthTimer <= 0) {
            ExitStealth();
        }
    }

    // AI update
    UpdateAI(deltaTime);
}

void Goblin::OnDestroy() {
    Logger::Info("Goblin destroyed");

    // Publish death event
    Nova::Reflect::BusEvent evt("OnEntityDestroyed", "Goblin", GetHandle().ToUint64());
    Nova::Reflect::EventBus::Instance().Publish(evt);
}

bool Goblin::OnEvent(Lifecycle::GameEvent& event) {
    switch (event.type) {
        case Lifecycle::EventType::Damaged: {
            float damage = event.GetData<float>("damage").value_or(0.0f);
            std::string type = event.GetData<std::string>("type").value_or("physical");
            TakeDamage(damage, type);
            return true;
        }
        case Lifecycle::EventType::Healed: {
            float amount = event.GetData<float>("amount").value_or(0.0f);
            m_health = std::min(m_maxHealth, m_health + amount);
            return true;
        }
        default:
            return false;
    }
}

void Goblin::TakeDamage(float damage, const std::string& damageType) {
    // Exit stealth when hit
    if (m_stealthed) {
        ExitStealth();
    }

    // Apply armor reduction
    float actualDamage = damage - m_armor;
    if (damageType == "magic") {
        actualDamage = damage; // Magic ignores armor
    }
    actualDamage = std::max(1.0f, actualDamage);

    m_health -= actualDamage;

    // Publish damage event
    Nova::Reflect::BusEvent evt("OnDamage", "Goblin", GetHandle().ToUint64());
    evt.SetData("damage", actualDamage);
    evt.SetData("health", m_health);
    evt.SetData("healthPercent", m_health / m_maxHealth);
    Nova::Reflect::EventBus::Instance().Publish(evt);

    // Check death
    if (m_health <= 0) {
        Die();
    }
    // Check flee
    else if (m_health / m_maxHealth < m_fleeHealthPercent) {
        EnterStealth();
    }
}

void Goblin::Attack(LifecycleHandle target) {
    if (m_attackCooldown > 0 || !IsAlive()) return;

    // Exit stealth to attack
    if (m_stealthed) {
        ExitStealth();
    }

    // Send damage event to target
    auto& manager = Lifecycle::GetGlobalLifecycleManager();
    Lifecycle::GameEvent dmgEvent(Lifecycle::EventType::Damaged, GetHandle());
    dmgEvent.SetData("damage", m_damage);
    dmgEvent.SetData("type", std::string("physical"));
    dmgEvent.SetData("attacker", GetHandle().ToUint64());
    manager.SendEvent(target, dmgEvent);

    // Set cooldown
    m_attackCooldown = 1.0f / m_attackSpeed;

    // Publish attack event
    Nova::Reflect::BusEvent evt("OnAttack", "Goblin", GetHandle().ToUint64());
    evt.SetData("target", target.ToUint64());
    evt.SetData("damage", m_damage);
    Nova::Reflect::EventBus::Instance().Publish(evt);
}

void Goblin::UpdateAI(float deltaTime) {
    // Find target if none
    if (!m_currentTarget.IsValid()) {
        FindTarget();
    }

    auto& manager = Lifecycle::GetGlobalLifecycleManager();

    // Validate current target
    if (m_currentTarget.IsValid() && !manager.IsAlive(m_currentTarget)) {
        m_currentTarget = {};
    }

    // No target, idle
    if (!m_currentTarget.IsValid()) {
        return;
    }

    // Get target position
    if (auto* target = manager.Get(m_currentTarget)) {
        // Simple position access (would use component in real code)
        m_targetPosition = glm::vec3(0); // Placeholder
    }

    // Check attack range
    float distance = glm::length(m_targetPosition - m_position);
    if (distance <= m_attackRange) {
        Attack(m_currentTarget);
    } else {
        MoveTowardsTarget(deltaTime);
    }
}

void Goblin::FindTarget() {
    auto& manager = Lifecycle::GetGlobalLifecycleManager();

    // Find nearest player
    auto players = manager.GetAll([](const Lifecycle::ILifecycle* obj) {
        // In real code, check for Player type
        return true;
    });

    if (players.empty()) return;

    // Find closest
    float minDist = m_detectionRange;
    for (auto* player : players) {
        glm::vec3 playerPos = glm::vec3(0); // Would get from player
        float dist = glm::length(playerPos - m_position);
        if (dist < minDist) {
            minDist = dist;
            // m_currentTarget = player->GetHandle();
        }
    }
}

void Goblin::MoveTowardsTarget(float deltaTime) {
    glm::vec3 direction = glm::normalize(m_targetPosition - m_position);
    m_position += direction * m_moveSpeed * deltaTime;
}

void Goblin::EnterStealth() {
    if (m_stealthed) return;

    m_stealthed = true;
    m_stealthTimer = 5.0f; // 5 seconds of stealth

    Nova::Reflect::BusEvent evt("OnStealthEnter", "Goblin", GetHandle().ToUint64());
    Nova::Reflect::EventBus::Instance().Publish(evt);
}

void Goblin::ExitStealth() {
    if (!m_stealthed) return;

    m_stealthed = false;
    m_stealthTimer = 0.0f;

    Nova::Reflect::BusEvent evt("OnStealthExit", "Goblin", GetHandle().ToUint64());
    Nova::Reflect::EventBus::Instance().Publish(evt);
}

void Goblin::Die() {
    SetLifecycleState(Lifecycle::LifecycleState::Dying);

    // Drop loot
    DropLoot();

    // Publish death event
    Nova::Reflect::BusEvent evt("OnDeath", "Goblin", GetHandle().ToUint64());
    evt.SetData("position", m_position);
    Nova::Reflect::EventBus::Instance().Publish(evt);

    // Schedule destruction
    auto& manager = Lifecycle::GetGlobalLifecycleManager();
    manager.Destroy(GetHandle());
}

void Goblin::DropLoot() {
    if (m_lootConfig.empty()) return;

    // Drop gold
    if (m_lootConfig.contains("gold")) {
        int minGold = m_lootConfig["gold"]["min"];
        int maxGold = m_lootConfig["gold"]["max"];
        int gold = Nova::Random::Range(minGold, maxGold);

        Nova::Reflect::BusEvent evt("OnLootDrop", "Goblin");
        evt.SetData("type", std::string("gold"));
        evt.SetData("amount", gold);
        evt.SetData("position", m_position);
        Nova::Reflect::EventBus::Instance().Publish(evt);
    }

    // Drop items
    if (m_lootConfig.contains("drops")) {
        for (const auto& drop : m_lootConfig["drops"]) {
            float chance = drop["chance"];
            if (Nova::Random::Float() < chance) {
                Nova::Reflect::BusEvent evt("OnLootDrop", "Goblin");
                evt.SetData("type", std::string("item"));
                evt.SetData("item", drop["item"].get<std::string>());
                evt.SetData("position", m_position);
                Nova::Reflect::EventBus::Instance().Publish(evt);
            }
        }
    }
}

} // namespace Vehement
```

## Step 3: Register the Entity Type

In your game initialization code:

```cpp
#include <systems/lifecycle/LifecycleManager.hpp>
#include "entities/Goblin.hpp"

void RegisterEntityTypes() {
    auto& manager = Lifecycle::GetGlobalLifecycleManager();

    // Register Goblin type
    manager.RegisterType<Vehement::Goblin>("Goblin");

    // Enable pooling for frequently spawned enemies
    manager.EnablePooling<Vehement::Goblin>(50);
}
```

## Step 4: Spawn the Entity

### From Code

```cpp
#include <systems/lifecycle/LifecycleManager.hpp>

void SpawnGoblin(const glm::vec3& position) {
    auto& manager = Lifecycle::GetGlobalLifecycleManager();

    // Load config
    auto config = ConfigLoader::Load<json>("configs/units/my_goblin.json");
    config["position"] = {position.x, position.y, position.z};

    // Create entity
    auto handle = manager.Create<Vehement::Goblin>(config);

    Logger::Info("Spawned goblin with handle: {}", handle.ToUint64());
}
```

### From Config File

```cpp
auto handle = manager.CreateFromFile("configs/units/my_goblin.json");
```

### From Editor

1. Open the Asset Browser
2. Navigate to `configs/units/`
3. Drag `my_goblin.json` into the World View
4. Drop at desired location

## Step 5: Interact with the Entity

### Listen for Events

```cpp
auto& bus = Nova::Reflect::EventBus::Instance();

// Listen for goblin deaths
bus.Subscribe("OnDeath", [](Nova::Reflect::BusEvent& evt) {
    if (evt.sourceType == "Goblin") {
        auto pos = evt.GetData<glm::vec3>("position");
        // Spawn death effect
        SpawnEffect("goblin_death", *pos);
    }
});

// Listen for loot drops
bus.Subscribe("OnLootDrop", [](Nova::Reflect::BusEvent& evt) {
    auto type = evt.GetData<std::string>("type");
    auto pos = evt.GetData<glm::vec3>("position");

    if (*type == "gold") {
        int amount = evt.GetDataOr<int>("amount", 0);
        SpawnGoldPickup(*pos, amount);
    } else if (*type == "item") {
        auto itemId = evt.GetData<std::string>("item");
        SpawnItemPickup(*pos, *itemId);
    }
});
```

### Access Entity Directly

```cpp
auto& manager = Lifecycle::GetGlobalLifecycleManager();

// Get all goblins
auto goblins = manager.GetAllOfType<Vehement::Goblin>();
for (auto* goblin : goblins) {
    if (goblin->GetHealth() < 25) {
        // Low health goblin
    }
}

// Get specific goblin
if (auto* goblin = manager.GetAs<Vehement::Goblin>(goblinHandle)) {
    goblin->TakeDamage(20, "fire");
}
```

## Summary

You have learned how to:
- Define entity configurations in JSON
- Create entity classes with proper lifecycle methods
- Register entity types with the lifecycle manager
- Spawn entities from code and the editor
- Subscribe to entity events
- Access and modify entities at runtime

## Next Steps

- [Creating an Ability](custom_ability.md) - Add custom abilities
- [Creating AI Behavior](ai_behavior.md) - Implement AI with Python
- [Animation System](../ANIMATION_GUIDE.md) - Add animations
