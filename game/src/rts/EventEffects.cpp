#include "EventEffects.hpp"
#include "EventScheduler.hpp"
#include <chrono>
#include <algorithm>
#include <cmath>

// Logging macros
#if __has_include("../../../engine/core/Logger.hpp")
#include "../../../engine/core/Logger.hpp"
#define EFFECTS_LOG_INFO(...) NOVA_LOG_INFO(__VA_ARGS__)
#define EFFECTS_LOG_WARN(...) NOVA_LOG_WARN(__VA_ARGS__)
#define EFFECTS_LOG_ERROR(...) NOVA_LOG_ERROR(__VA_ARGS__)
#else
#include <iostream>
#define EFFECTS_LOG_INFO(...) std::cout << "[EventEffects] " << __VA_ARGS__ << std::endl
#define EFFECTS_LOG_WARN(...) std::cerr << "[EventEffects WARN] " << __VA_ARGS__ << std::endl
#define EFFECTS_LOG_ERROR(...) std::cerr << "[EventEffects ERROR] " << __VA_ARGS__ << std::endl
#endif

namespace Vehement {

EventEffects::EventEffects()
    : m_rng(std::random_device{}()) {
    // Initialize default combined environment
    m_combinedEnvironment.visionMultiplier = 1.0f;
    m_combinedEnvironment.movementMultiplier = 1.0f;
    m_combinedEnvironment.damageMultiplier = 1.0f;
    m_combinedEnvironment.productionMultiplier = 1.0f;
    m_combinedEnvironment.disablePvP = false;
    m_combinedEnvironment.darknessLevel = 0.0f;
}

EventEffects::~EventEffects() {
    if (m_initialized) {
        Shutdown();
    }
}

bool EventEffects::Initialize(EntityManager* entityManager, World* world) {
    if (m_initialized) {
        EFFECTS_LOG_WARN("EventEffects already initialized");
        return true;
    }

    m_entityManager = entityManager;
    m_world = world;
    m_initialized = true;

    EFFECTS_LOG_INFO("EventEffects initialized");
    return true;
}

void EventEffects::Shutdown() {
    if (!m_initialized) return;

    EFFECTS_LOG_INFO("Shutting down EventEffects");

    RemoveAllEffects();

    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        m_spawnCallbacks.clear();
        m_lootCallbacks.clear();
        m_environmentCallbacks.clear();
    }

    m_initialized = false;
}

void EventEffects::Update(float deltaTime) {
    if (!m_initialized) return;

    std::vector<std::string> expiredEffects;
    int64_t currentTime = GetCurrentTimeMs();

    {
        std::lock_guard<std::mutex> lock(m_effectMutex);

        for (auto& [effectId, effect] : m_activeEffects) {
            if (!effect.isActive) continue;

            effect.elapsedTime += deltaTime;

            // Check if expired
            if (effect.IsExpired(currentTime)) {
                expiredEffects.push_back(effectId);
                continue;
            }

            // Process spawning for applicable effects
            ProcessSpawning(effect, deltaTime);
        }
    }

    // Clean up expired effects
    for (const auto& effectId : expiredEffects) {
        RemoveEventEffects(effectId);
    }
}

void EventEffects::CleanupExpiredEffects() {
    int64_t currentTime = GetCurrentTimeMs();
    std::vector<std::string> toRemove;

    {
        std::lock_guard<std::mutex> lock(m_effectMutex);
        for (const auto& [effectId, effect] : m_activeEffects) {
            if (effect.IsExpired(currentTime)) {
                toRemove.push_back(effectId);
            }
        }
    }

    for (const auto& effectId : toRemove) {
        RemoveEventEffects(effectId);
    }
}

void EventEffects::ApplyEvent(const WorldEvent& event) {
    EFFECTS_LOG_INFO("Applying effects for event: " + event.name);

    // Route to specific effect handler based on event type
    switch (event.type) {
        // Threats
        case EventType::ZombieHorde:    ApplyZombieHorde(event); break;
        case EventType::BossZombie:     ApplyBossZombie(event); break;
        case EventType::Plague:         ApplyPlague(event); break;
        case EventType::Infestation:    ApplyInfestation(event); break;
        case EventType::NightTerror:    ApplyNightTerror(event); break;

        // Opportunities
        case EventType::SupplyDrop:     ApplySupplyDrop(event); break;
        case EventType::RefugeeCamp:    ApplyRefugeeCamp(event); break;
        case EventType::TreasureCache:  ApplyTreasureCache(event); break;
        case EventType::AbandonedBase:  ApplyAbandonedBase(event); break;
        case EventType::WeaponCache:    ApplyWeaponCache(event); break;

        // Environmental
        case EventType::Storm:          ApplyStorm(event); break;
        case EventType::Earthquake:     ApplyEarthquake(event); break;
        case EventType::Drought:        ApplyDrought(event); break;
        case EventType::Bountiful:      ApplyBountiful(event); break;
        case EventType::Fog:            ApplyFog(event); break;
        case EventType::HeatWave:       ApplyHeatWave(event); break;

        // Social
        case EventType::TradeCaravan:   ApplyTradeCaravan(event); break;
        case EventType::MilitaryAid:    ApplyMilitaryAid(event); break;
        case EventType::Bandits:        ApplyBandits(event); break;
        case EventType::Deserters:      ApplyDeserters(event); break;
        case EventType::Merchant:       ApplyMerchant(event); break;

        // Global
        case EventType::BloodMoon:      ApplyBloodMoon(event); break;
        case EventType::Eclipse:        ApplyEclipse(event); break;
        case EventType::GoldenAge:      ApplyGoldenAge(event); break;
        case EventType::Apocalypse:     ApplyApocalypse(event); break;
        case EventType::Ceasefire:      ApplyCeasefire(event); break;
        case EventType::DoubleXP:       ApplyDoubleXP(event); break;

        default:
            EFFECTS_LOG_WARN("Unknown event type, no effects applied");
            break;
    }

    m_environmentDirty = true;
}

void EventEffects::RemoveEventEffects(const std::string& eventId) {
    std::lock_guard<std::mutex> lock(m_effectMutex);

    auto it = m_activeEffects.find(eventId);
    if (it != m_activeEffects.end()) {
        EFFECTS_LOG_INFO("Removing effects for event: " + eventId);

        // Remove entity modifiers
        RemoveEntityModifiers(it->second);

        // Remove environment effects
        RemoveEnvironmentEffect(it->second);

        m_activeEffects.erase(it);
        m_environmentDirty = true;
    }
}

void EventEffects::RemoveAllEffects() {
    std::lock_guard<std::mutex> lock(m_effectMutex);

    for (auto& [effectId, effect] : m_activeEffects) {
        RemoveEntityModifiers(effect);
        RemoveEnvironmentEffect(effect);
    }

    m_activeEffects.clear();
    m_environmentDirty = true;
    UpdateCombinedEnvironment();
}

// =========================================================================
// Threat Events
// =========================================================================

void EventEffects::ApplyZombieHorde(const WorldEvent& event) {
    AppliedEffect effect = CreateBaseEffect(event);

    // Configure zombie spawning
    SpawnConfig spawn;
    spawn.entityType = "zombie_walker";
    spawn.baseCount = 20;
    spawn.countPerPlayer = 10.0f;
    spawn.spawnInterval = 15.0f;
    spawn.initialDelay = 5.0f;
    spawn.radiusMin = event.radius * 0.8f;
    spawn.radiusMax = event.radius * 1.2f;
    spawn.announceSpawn = true;

    SetupSpawnConfig(effect, spawn);

    // Add zombie stat modifiers
    EntityStatModifier zombieMod;
    zombieMod.entityTag = "zombie";
    zombieMod.healthMultiplier = 1.0f + (0.1f * event.intensity);
    zombieMod.damageMultiplier = 1.0f + (0.05f * event.intensity);
    zombieMod.speedMultiplier = 1.1f;
    effect.entityModifiers.push_back(zombieMod);

    {
        std::lock_guard<std::mutex> lock(m_effectMutex);
        m_activeEffects[event.id] = effect;
    }

    ApplyEntityModifiers(effect);
}

void EventEffects::ApplyBossZombie(const WorldEvent& event) {
    AppliedEffect effect = CreateBaseEffect(event);

    // Spawn single powerful boss
    SpawnConfig spawn;
    spawn.entityType = "zombie_boss";
    spawn.baseCount = 1;
    spawn.countPerPlayer = 0.0f;
    spawn.spawnInterval = 0.0f; // One-time spawn
    spawn.initialDelay = 3.0f;
    spawn.radiusMin = 0.0f;
    spawn.radiusMax = event.radius * 0.3f;
    spawn.announceSpawn = true;

    SetupSpawnConfig(effect, spawn);

    // Boss stats
    EntityStatModifier bossMod;
    bossMod.entityTag = "zombie_boss";
    bossMod.healthMultiplier = 5.0f * event.intensity;
    bossMod.damageMultiplier = 3.0f * event.intensity;
    bossMod.speedMultiplier = 0.7f;
    bossMod.armorMultiplier = 2.0f;
    bossMod.bonusHealth = 1000 * event.difficultyTier;
    effect.entityModifiers.push_back(bossMod);

    // Boss drops valuable loot
    effect.lootConfig.guaranteedResources[ResourceType::RareComponents] = 20 * event.difficultyTier;
    effect.lootConfig.guaranteedResources[ResourceType::Metal] = 100 * event.difficultyTier;
    effect.lootConfig.experienceReward = 1000 * event.difficultyTier;
    effect.lootConfig.itemDrops.emplace_back("weapon_legendary", 0.1f);
    effect.lootConfig.itemDrops.emplace_back("armor_heavy", 0.3f);

    {
        std::lock_guard<std::mutex> lock(m_effectMutex);
        m_activeEffects[event.id] = effect;
    }

    ApplyEntityModifiers(effect);
}

void EventEffects::ApplyPlague(const WorldEvent& event) {
    AppliedEffect effect = CreateBaseEffect(event);

    // Debuff workers/civilians
    EntityStatModifier workerMod;
    workerMod.entityTag = "worker";
    workerMod.healthMultiplier = 0.8f;
    workerMod.speedMultiplier = 0.7f;
    effect.entityModifiers.push_back(workerMod);

    // Reduce production
    EnvironmentConfig env;
    env.productionMultiplier = 0.5f;
    env.visionMultiplier = 1.0f;
    env.movementMultiplier = 0.9f;
    env.damageMultiplier = 1.0f;
    SetupEnvironmentConfig(effect, env);

    {
        std::lock_guard<std::mutex> lock(m_effectMutex);
        m_activeEffects[event.id] = effect;
    }

    ApplyEntityModifiers(effect);
    ApplyEnvironmentEffect(effect);
}

void EventEffects::ApplyInfestation(const WorldEvent& event) {
    AppliedEffect effect = CreateBaseEffect(event);

    // Spawn zombies inside buildings
    SpawnConfig spawn;
    spawn.entityType = "zombie_crawler";
    spawn.baseCount = 5;
    spawn.countPerPlayer = 3.0f;
    spawn.spawnInterval = 30.0f;
    spawn.initialDelay = 0.0f;
    spawn.radiusMin = 0.0f;
    spawn.radiusMax = event.radius * 0.5f;
    spawn.announceSpawn = false;

    SetupSpawnConfig(effect, spawn);

    {
        std::lock_guard<std::mutex> lock(m_effectMutex);
        m_activeEffects[event.id] = effect;
    }
}

void EventEffects::ApplyNightTerror(const WorldEvent& event) {
    AppliedEffect effect = CreateBaseEffect(event);

    // Massive zombie buff
    EntityStatModifier zombieMod;
    zombieMod.entityTag = "zombie";
    zombieMod.healthMultiplier = 2.0f;
    zombieMod.damageMultiplier = 2.5f;
    zombieMod.speedMultiplier = 1.5f;
    zombieMod.detectionMultiplier = 2.0f;
    effect.entityModifiers.push_back(zombieMod);

    // Extreme darkness
    EnvironmentConfig env;
    env.visionMultiplier = 0.3f;
    env.darknessLevel = 0.9f;
    env.weatherEffect = "darkness_fog";
    env.ambientSound = "night_terror_ambient";
    SetupEnvironmentConfig(effect, env);

    // Spawn extra dangerous zombies
    SpawnConfig spawn;
    spawn.entityType = "zombie_nightmare";
    spawn.baseCount = 10;
    spawn.countPerPlayer = 5.0f;
    spawn.spawnInterval = 20.0f;
    spawn.initialDelay = 10.0f;
    spawn.radiusMin = event.radius * 0.5f;
    spawn.radiusMax = event.radius * 1.5f;

    SetupSpawnConfig(effect, spawn);

    {
        std::lock_guard<std::mutex> lock(m_effectMutex);
        m_activeEffects[event.id] = effect;
    }

    ApplyEntityModifiers(effect);
    ApplyEnvironmentEffect(effect);
}

// =========================================================================
// Opportunity Events
// =========================================================================

void EventEffects::ApplySupplyDrop(const WorldEvent& event) {
    AppliedEffect effect = CreateBaseEffect(event);

    // Configure loot
    effect.lootConfig.guaranteedResources[ResourceType::Food] = 50 * event.difficultyTier;
    effect.lootConfig.guaranteedResources[ResourceType::Water] = 30 * event.difficultyTier;
    effect.lootConfig.guaranteedResources[ResourceType::Ammunition] = 20 * event.difficultyTier;
    effect.lootConfig.randomResources[ResourceType::Medicine] = {15, 0.5f};
    effect.lootConfig.randomResources[ResourceType::Fuel] = {25, 0.3f};
    effect.lootConfig.experienceReward = 50;
    effect.lootConfig.qualityMultiplier = event.intensity;

    {
        std::lock_guard<std::mutex> lock(m_effectMutex);
        m_activeEffects[event.id] = effect;
    }

    // Spawn loot immediately
    SpawnLoot(effect);
}

void EventEffects::ApplyRefugeeCamp(const WorldEvent& event) {
    AppliedEffect effect = CreateBaseEffect(event);

    // Spawn recruitable NPCs
    SpawnConfig spawn;
    spawn.entityType = "npc_refugee";
    spawn.baseCount = 5;
    spawn.countPerPlayer = 2.0f;
    spawn.spawnInterval = 0.0f; // One-time spawn
    spawn.initialDelay = 0.0f;
    spawn.radiusMin = 0.0f;
    spawn.radiusMax = event.radius * 0.3f;

    SetupSpawnConfig(effect, spawn);

    effect.lootConfig.experienceReward = 100 * event.difficultyTier;

    {
        std::lock_guard<std::mutex> lock(m_effectMutex);
        m_activeEffects[event.id] = effect;
    }
}

void EventEffects::ApplyTreasureCache(const WorldEvent& event) {
    AppliedEffect effect = CreateBaseEffect(event);

    // Valuable resources
    effect.lootConfig.guaranteedResources[ResourceType::Metal] = 40 * event.difficultyTier;
    effect.lootConfig.guaranteedResources[ResourceType::Electronics] = 20 * event.difficultyTier;
    effect.lootConfig.guaranteedResources[ResourceType::RareComponents] = 10 * event.difficultyTier;
    effect.lootConfig.itemDrops.emplace_back("blueprint_rare", 0.2f);
    effect.lootConfig.itemDrops.emplace_back("tool_advanced", 0.3f);
    effect.lootConfig.experienceReward = 200;
    effect.lootConfig.qualityMultiplier = event.intensity * 1.5f;

    {
        std::lock_guard<std::mutex> lock(m_effectMutex);
        m_activeEffects[event.id] = effect;
    }

    SpawnLoot(effect);
}

void EventEffects::ApplyAbandonedBase(const WorldEvent& event) {
    AppliedEffect effect = CreateBaseEffect(event);

    // Spawn claimable structure
    SpawnConfig spawn;
    spawn.entityType = "structure_abandoned_base";
    spawn.baseCount = 1;
    spawn.countPerPlayer = 0.0f;
    spawn.spawnInterval = 0.0f;
    spawn.initialDelay = 0.0f;
    spawn.radiusMin = 0.0f;
    spawn.radiusMax = 0.0f;

    SetupSpawnConfig(effect, spawn);

    // Resources inside the base
    effect.lootConfig.guaranteedResources[ResourceType::Wood] = 100 * event.difficultyTier;
    effect.lootConfig.guaranteedResources[ResourceType::Stone] = 80 * event.difficultyTier;
    effect.lootConfig.guaranteedResources[ResourceType::Metal] = 50 * event.difficultyTier;

    {
        std::lock_guard<std::mutex> lock(m_effectMutex);
        m_activeEffects[event.id] = effect;
    }
}

void EventEffects::ApplyWeaponCache(const WorldEvent& event) {
    AppliedEffect effect = CreateBaseEffect(event);

    // Military loot
    effect.lootConfig.guaranteedResources[ResourceType::Ammunition] = 100 * event.difficultyTier;
    effect.lootConfig.itemDrops.emplace_back("weapon_assault_rifle", 0.8f);
    effect.lootConfig.itemDrops.emplace_back("weapon_shotgun", 0.6f);
    effect.lootConfig.itemDrops.emplace_back("weapon_sniper", 0.3f);
    effect.lootConfig.itemDrops.emplace_back("armor_military", 0.4f);
    effect.lootConfig.itemDrops.emplace_back("grenade_frag", 0.7f);
    effect.lootConfig.experienceReward = 150;

    {
        std::lock_guard<std::mutex> lock(m_effectMutex);
        m_activeEffects[event.id] = effect;
    }

    SpawnLoot(effect);
}

// =========================================================================
// Environmental Events
// =========================================================================

void EventEffects::ApplyStorm(const WorldEvent& event) {
    AppliedEffect effect = CreateBaseEffect(event);

    EnvironmentConfig env;
    env.visionMultiplier = 0.5f;
    env.movementMultiplier = 0.7f;
    env.damageMultiplier = 1.0f;
    env.productionMultiplier = 0.8f;
    env.darknessLevel = 0.4f;
    env.weatherEffect = "storm_heavy";
    env.ambientSound = "storm_ambient";

    SetupEnvironmentConfig(effect, env);

    {
        std::lock_guard<std::mutex> lock(m_effectMutex);
        m_activeEffects[event.id] = effect;
    }

    ApplyEnvironmentEffect(effect);
}

void EventEffects::ApplyEarthquake(const WorldEvent& event) {
    AppliedEffect effect = CreateBaseEffect(event);

    // Damage buildings - this would be handled by custom logic
    effect.environment.damageMultiplier = 1.5f; // Structures take extra damage
    effect.environment.weatherEffect = "earthquake_dust";
    effect.environment.ambientSound = "earthquake_rumble";

    // Store building damage info in custom data
    // Actual damage application would be in game logic

    {
        std::lock_guard<std::mutex> lock(m_effectMutex);
        m_activeEffects[event.id] = effect;
    }

    ApplyEnvironmentEffect(effect);
}

void EventEffects::ApplyDrought(const WorldEvent& event) {
    AppliedEffect effect = CreateBaseEffect(event);

    EnvironmentConfig env;
    env.visionMultiplier = 1.1f; // Slightly better visibility
    env.movementMultiplier = 0.9f;
    env.productionMultiplier = 0.4f; // Severe production penalty
    env.weatherEffect = "drought_haze";

    SetupEnvironmentConfig(effect, env);

    {
        std::lock_guard<std::mutex> lock(m_effectMutex);
        m_activeEffects[event.id] = effect;
    }

    ApplyEnvironmentEffect(effect);
}

void EventEffects::ApplyBountiful(const WorldEvent& event) {
    AppliedEffect effect = CreateBaseEffect(event);

    EnvironmentConfig env;
    env.visionMultiplier = 1.0f;
    env.movementMultiplier = 1.0f;
    env.productionMultiplier = 2.0f; // Double production!
    env.weatherEffect = "bountiful_particles";

    SetupEnvironmentConfig(effect, env);

    {
        std::lock_guard<std::mutex> lock(m_effectMutex);
        m_activeEffects[event.id] = effect;
    }

    ApplyEnvironmentEffect(effect);
}

void EventEffects::ApplyFog(const WorldEvent& event) {
    AppliedEffect effect = CreateBaseEffect(event);

    EnvironmentConfig env;
    env.visionMultiplier = 0.3f;
    env.movementMultiplier = 0.9f;
    env.darknessLevel = 0.3f;
    env.weatherEffect = "fog_dense";

    SetupEnvironmentConfig(effect, env);

    // Zombies get detection bonus in fog
    EntityStatModifier zombieMod;
    zombieMod.entityTag = "zombie";
    zombieMod.detectionMultiplier = 1.5f;
    effect.entityModifiers.push_back(zombieMod);

    {
        std::lock_guard<std::mutex> lock(m_effectMutex);
        m_activeEffects[event.id] = effect;
    }

    ApplyEntityModifiers(effect);
    ApplyEnvironmentEffect(effect);
}

void EventEffects::ApplyHeatWave(const WorldEvent& event) {
    AppliedEffect effect = CreateBaseEffect(event);

    EnvironmentConfig env;
    env.visionMultiplier = 1.0f;
    env.movementMultiplier = 0.8f;
    env.productionMultiplier = 0.7f;
    env.weatherEffect = "heat_shimmer";

    SetupEnvironmentConfig(effect, env);

    // Workers affected more
    EntityStatModifier workerMod;
    workerMod.entityTag = "worker";
    workerMod.speedMultiplier = 0.6f;
    effect.entityModifiers.push_back(workerMod);

    {
        std::lock_guard<std::mutex> lock(m_effectMutex);
        m_activeEffects[event.id] = effect;
    }

    ApplyEntityModifiers(effect);
    ApplyEnvironmentEffect(effect);
}

// =========================================================================
// Social Events
// =========================================================================

void EventEffects::ApplyTradeCaravan(const WorldEvent& event) {
    AppliedEffect effect = CreateBaseEffect(event);

    // Spawn trader NPCs
    SpawnConfig spawn;
    spawn.entityType = "npc_trader";
    spawn.baseCount = 2;
    spawn.countPerPlayer = 0.5f;
    spawn.spawnInterval = 0.0f;
    spawn.initialDelay = 0.0f;
    spawn.radiusMin = 0.0f;
    spawn.radiusMax = event.radius * 0.2f;

    SetupSpawnConfig(effect, spawn);

    {
        std::lock_guard<std::mutex> lock(m_effectMutex);
        m_activeEffects[event.id] = effect;
    }
}

void EventEffects::ApplyMilitaryAid(const WorldEvent& event) {
    AppliedEffect effect = CreateBaseEffect(event);

    // Spawn allied soldiers
    SpawnConfig spawn;
    spawn.entityType = "npc_soldier_ally";
    spawn.baseCount = 5;
    spawn.countPerPlayer = 2.0f;
    spawn.spawnInterval = 0.0f;
    spawn.initialDelay = 5.0f;
    spawn.radiusMin = event.radius * 0.3f;
    spawn.radiusMax = event.radius * 0.7f;
    spawn.announceSpawn = true;

    SetupSpawnConfig(effect, spawn);

    // Buff allied NPCs
    EntityStatModifier allyMod;
    allyMod.entityTag = "ally";
    allyMod.healthMultiplier = 1.5f;
    allyMod.damageMultiplier = 1.3f;
    effect.entityModifiers.push_back(allyMod);

    {
        std::lock_guard<std::mutex> lock(m_effectMutex);
        m_activeEffects[event.id] = effect;
    }

    ApplyEntityModifiers(effect);
}

void EventEffects::ApplyBandits(const WorldEvent& event) {
    AppliedEffect effect = CreateBaseEffect(event);

    // Spawn hostile bandits
    SpawnConfig spawn;
    spawn.entityType = "npc_bandit";
    spawn.baseCount = 8;
    spawn.countPerPlayer = 3.0f;
    spawn.spawnInterval = 30.0f;
    spawn.initialDelay = 3.0f;
    spawn.radiusMin = event.radius * 0.6f;
    spawn.radiusMax = event.radius;
    spawn.announceSpawn = true;

    SetupSpawnConfig(effect, spawn);

    // Loot from defeating bandits
    effect.lootConfig.guaranteedResources[ResourceType::Ammunition] = 30;
    effect.lootConfig.randomResources[ResourceType::Food] = {20, 0.5f};
    effect.lootConfig.itemDrops.emplace_back("weapon_pistol", 0.3f);
    effect.lootConfig.experienceReward = 75;

    {
        std::lock_guard<std::mutex> lock(m_effectMutex);
        m_activeEffects[event.id] = effect;
    }
}

void EventEffects::ApplyDeserters(const WorldEvent& event) {
    AppliedEffect effect = CreateBaseEffect(event);

    // Spawn recruitable soldiers
    SpawnConfig spawn;
    spawn.entityType = "npc_deserter";
    spawn.baseCount = 3;
    spawn.countPerPlayer = 1.0f;
    spawn.spawnInterval = 0.0f;
    spawn.initialDelay = 0.0f;
    spawn.radiusMin = 0.0f;
    spawn.radiusMax = event.radius * 0.4f;

    SetupSpawnConfig(effect, spawn);

    effect.lootConfig.experienceReward = 50;

    {
        std::lock_guard<std::mutex> lock(m_effectMutex);
        m_activeEffects[event.id] = effect;
    }
}

void EventEffects::ApplyMerchant(const WorldEvent& event) {
    AppliedEffect effect = CreateBaseEffect(event);

    // Spawn rare item merchant
    SpawnConfig spawn;
    spawn.entityType = "npc_merchant_rare";
    spawn.baseCount = 1;
    spawn.countPerPlayer = 0.0f;
    spawn.spawnInterval = 0.0f;
    spawn.initialDelay = 0.0f;
    spawn.radiusMin = 0.0f;
    spawn.radiusMax = 0.0f;

    SetupSpawnConfig(effect, spawn);

    {
        std::lock_guard<std::mutex> lock(m_effectMutex);
        m_activeEffects[event.id] = effect;
    }
}

// =========================================================================
// Global Events
// =========================================================================

void EventEffects::ApplyBloodMoon(const WorldEvent& event) {
    AppliedEffect effect = CreateBaseEffect(event);

    // Major zombie buffs
    EntityStatModifier zombieMod;
    zombieMod.entityTag = "zombie";
    zombieMod.healthMultiplier = 1.5f;
    zombieMod.damageMultiplier = 2.0f;
    zombieMod.speedMultiplier = 1.3f;
    zombieMod.detectionMultiplier = 1.5f;
    effect.entityModifiers.push_back(zombieMod);

    // Eerie atmosphere
    EnvironmentConfig env;
    env.visionMultiplier = 0.7f;
    env.darknessLevel = 0.6f;
    env.weatherEffect = "blood_moon_sky";
    env.ambientSound = "blood_moon_ambient";

    SetupEnvironmentConfig(effect, env);

    // Spawn additional zombies
    SpawnConfig spawn;
    spawn.entityType = "zombie_blood";
    spawn.baseCount = 15;
    spawn.countPerPlayer = 5.0f;
    spawn.spawnInterval = 30.0f;
    spawn.initialDelay = 10.0f;
    spawn.radiusMin = 200.0f;
    spawn.radiusMax = 500.0f;

    SetupSpawnConfig(effect, spawn);

    {
        std::lock_guard<std::mutex> lock(m_effectMutex);
        m_activeEffects[event.id] = effect;
    }

    ApplyEntityModifiers(effect);
    ApplyEnvironmentEffect(effect);
}

void EventEffects::ApplyEclipse(const WorldEvent& event) {
    AppliedEffect effect = CreateBaseEffect(event);

    // Complete darkness
    EnvironmentConfig env;
    env.visionMultiplier = 0.4f;
    env.movementMultiplier = 0.9f;
    env.darknessLevel = 0.8f;
    env.weatherEffect = "eclipse_darkness";
    env.ambientSound = "eclipse_ambient";

    SetupEnvironmentConfig(effect, env);

    // Zombies active as if night
    EntityStatModifier zombieMod;
    zombieMod.entityTag = "zombie";
    zombieMod.speedMultiplier = 1.2f;
    zombieMod.detectionMultiplier = 1.3f;
    effect.entityModifiers.push_back(zombieMod);

    {
        std::lock_guard<std::mutex> lock(m_effectMutex);
        m_activeEffects[event.id] = effect;
    }

    ApplyEntityModifiers(effect);
    ApplyEnvironmentEffect(effect);
}

void EventEffects::ApplyGoldenAge(const WorldEvent& event) {
    AppliedEffect effect = CreateBaseEffect(event);

    // All bonuses
    EnvironmentConfig env;
    env.visionMultiplier = 1.2f;
    env.movementMultiplier = 1.1f;
    env.productionMultiplier = 1.5f;
    env.weatherEffect = "golden_particles";

    SetupEnvironmentConfig(effect, env);

    // Player unit buffs
    EntityStatModifier playerMod;
    playerMod.entityTag = "player_unit";
    playerMod.healthMultiplier = 1.2f;
    playerMod.damageMultiplier = 1.1f;
    effect.entityModifiers.push_back(playerMod);

    {
        std::lock_guard<std::mutex> lock(m_effectMutex);
        m_activeEffects[event.id] = effect;
    }

    ApplyEntityModifiers(effect);
    ApplyEnvironmentEffect(effect);
}

void EventEffects::ApplyApocalypse(const WorldEvent& event) {
    AppliedEffect effect = CreateBaseEffect(event);

    // Massive zombie invasion
    SpawnConfig spawn;
    spawn.entityType = "zombie_apocalypse";
    spawn.baseCount = 50;
    spawn.countPerPlayer = 15.0f;
    spawn.spawnInterval = 20.0f;
    spawn.initialDelay = 15.0f;
    spawn.radiusMin = 300.0f;
    spawn.radiusMax = 800.0f;
    spawn.announceSpawn = true;

    SetupSpawnConfig(effect, spawn);

    // Extreme zombie buffs
    EntityStatModifier zombieMod;
    zombieMod.entityTag = "zombie";
    zombieMod.healthMultiplier = 2.0f;
    zombieMod.damageMultiplier = 2.5f;
    zombieMod.speedMultiplier = 1.4f;
    effect.entityModifiers.push_back(zombieMod);

    // Apocalyptic atmosphere
    EnvironmentConfig env;
    env.visionMultiplier = 0.6f;
    env.darknessLevel = 0.5f;
    env.weatherEffect = "apocalypse_storm";
    env.ambientSound = "apocalypse_ambient";

    SetupEnvironmentConfig(effect, env);

    // Great rewards for surviving
    effect.lootConfig.guaranteedResources[ResourceType::RareComponents] = 50;
    effect.lootConfig.guaranteedResources[ResourceType::Electronics] = 100;
    effect.lootConfig.itemDrops.emplace_back("weapon_legendary", 0.3f);
    effect.lootConfig.experienceReward = 2000;

    {
        std::lock_guard<std::mutex> lock(m_effectMutex);
        m_activeEffects[event.id] = effect;
    }

    ApplyEntityModifiers(effect);
    ApplyEnvironmentEffect(effect);
}

void EventEffects::ApplyCeasefire(const WorldEvent& event) {
    AppliedEffect effect = CreateBaseEffect(event);

    // Disable PvP
    EnvironmentConfig env;
    env.disablePvP = true;
    env.productionMultiplier = 1.2f; // Bonus for cooperation

    SetupEnvironmentConfig(effect, env);

    m_pvpDisabled = true;

    {
        std::lock_guard<std::mutex> lock(m_effectMutex);
        m_activeEffects[event.id] = effect;
    }

    ApplyEnvironmentEffect(effect);
}

void EventEffects::ApplyDoubleXP(const WorldEvent& event) {
    AppliedEffect effect = CreateBaseEffect(event);

    m_experienceMultiplier = 2.0f;

    EnvironmentConfig env;
    env.weatherEffect = "xp_sparkles";

    SetupEnvironmentConfig(effect, env);

    {
        std::lock_guard<std::mutex> lock(m_effectMutex);
        m_activeEffects[event.id] = effect;
    }

    ApplyEnvironmentEffect(effect);
}

// =========================================================================
// Queries
// =========================================================================

std::vector<AppliedEffect> EventEffects::GetActiveEffects() const {
    std::lock_guard<std::mutex> lock(m_effectMutex);
    std::vector<AppliedEffect> effects;
    effects.reserve(m_activeEffects.size());
    for (const auto& [id, effect] : m_activeEffects) {
        effects.push_back(effect);
    }
    return effects;
}

std::vector<AppliedEffect> EventEffects::GetEffectsAtPosition(const glm::vec2& pos) const {
    std::lock_guard<std::mutex> lock(m_effectMutex);
    std::vector<AppliedEffect> effects;
    for (const auto& [id, effect] : m_activeEffects) {
        if (IsPositionInEffect(pos, effect)) {
            effects.push_back(effect);
        }
    }
    return effects;
}

std::vector<AppliedEffect> EventEffects::GetEffectsForEntity(const std::string& entityId) const {
    std::lock_guard<std::mutex> lock(m_effectMutex);
    std::vector<AppliedEffect> effects;
    for (const auto& [id, effect] : m_activeEffects) {
        if (effect.affectedEntityIds.count(entityId) > 0) {
            effects.push_back(effect);
        }
    }
    return effects;
}

bool EventEffects::IsEffectTypeActive(EventType type) const {
    std::lock_guard<std::mutex> lock(m_effectMutex);
    for (const auto& [id, effect] : m_activeEffects) {
        if (effect.eventType == type && effect.isActive) {
            return true;
        }
    }
    return false;
}

EnvironmentConfig EventEffects::GetCombinedEnvironmentConfig() const {
    if (m_environmentDirty) {
        const_cast<EventEffects*>(this)->UpdateCombinedEnvironment();
    }
    return m_combinedEnvironment;
}

EntityStatModifier EventEffects::GetCombinedEntityModifiers(const std::string& entityId,
                                                            const std::string& entityTag) const {
    EntityStatModifier combined;
    combined.healthMultiplier = 1.0f;
    combined.damageMultiplier = 1.0f;
    combined.speedMultiplier = 1.0f;
    combined.armorMultiplier = 1.0f;
    combined.detectionMultiplier = 1.0f;
    combined.bonusHealth = 0;
    combined.bonusDamage = 0;

    std::lock_guard<std::mutex> lock(m_effectMutex);
    for (const auto& [id, effect] : m_activeEffects) {
        if (!effect.isActive) continue;

        for (const auto& mod : effect.entityModifiers) {
            if (mod.entityTag.empty() || mod.entityTag == entityTag) {
                combined.healthMultiplier *= mod.healthMultiplier;
                combined.damageMultiplier *= mod.damageMultiplier;
                combined.speedMultiplier *= mod.speedMultiplier;
                combined.armorMultiplier *= mod.armorMultiplier;
                combined.detectionMultiplier *= mod.detectionMultiplier;
                combined.bonusHealth += mod.bonusHealth;
                combined.bonusDamage += mod.bonusDamage;
            }
        }
    }

    return combined;
}

float EventEffects::GetVisionModifier(const glm::vec2& pos) const {
    float modifier = 1.0f;
    std::lock_guard<std::mutex> lock(m_effectMutex);

    for (const auto& [id, effect] : m_activeEffects) {
        if (!effect.isActive) continue;
        // Apply global effects and local effects if in range
        // For simplicity, applying all active effects
        modifier *= effect.environment.visionMultiplier;
    }

    return modifier;
}

float EventEffects::GetMovementModifier(const glm::vec2& pos) const {
    float modifier = 1.0f;
    std::lock_guard<std::mutex> lock(m_effectMutex);

    for (const auto& [id, effect] : m_activeEffects) {
        if (!effect.isActive) continue;
        modifier *= effect.environment.movementMultiplier;
    }

    return modifier;
}

float EventEffects::GetProductionModifier(const glm::vec2& pos) const {
    float modifier = 1.0f;
    std::lock_guard<std::mutex> lock(m_effectMutex);

    for (const auto& [id, effect] : m_activeEffects) {
        if (!effect.isActive) continue;
        modifier *= effect.environment.productionMultiplier;
    }

    return modifier;
}

float EventEffects::GetDamageModifier(const glm::vec2& pos) const {
    float modifier = 1.0f;
    std::lock_guard<std::mutex> lock(m_effectMutex);

    for (const auto& [id, effect] : m_activeEffects) {
        if (!effect.isActive) continue;
        modifier *= effect.environment.damageMultiplier;
    }

    return modifier;
}

bool EventEffects::IsPvPDisabled() const {
    return m_pvpDisabled;
}

float EventEffects::GetDarknessLevel() const {
    float maxDarkness = 0.0f;
    std::lock_guard<std::mutex> lock(m_effectMutex);

    for (const auto& [id, effect] : m_activeEffects) {
        if (!effect.isActive) continue;
        maxDarkness = std::max(maxDarkness, effect.environment.darknessLevel);
    }

    return maxDarkness;
}

float EventEffects::GetExperienceMultiplier() const {
    return m_experienceMultiplier;
}

void EventEffects::OnEntitySpawned(EntitySpawnCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_spawnCallbacks.push_back(std::move(callback));
}

void EventEffects::OnLootDropped(LootDropCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_lootCallbacks.push_back(std::move(callback));
}

void EventEffects::OnEnvironmentChanged(EnvironmentChangeCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_environmentCallbacks.push_back(std::move(callback));
}

// =========================================================================
// Private Helpers
// =========================================================================

AppliedEffect EventEffects::CreateBaseEffect(const WorldEvent& event) {
    AppliedEffect effect;
    effect.effectId = event.id + "_effect";
    effect.eventId = event.id;
    effect.eventType = event.type;
    effect.startTime = event.startTime;
    effect.endTime = event.endTime;
    effect.elapsedTime = 0.0f;
    effect.isActive = true;
    effect.spawnWaveCount = 0;
    effect.timeSinceLastSpawn = 0.0f;

    // Default environment config
    effect.environment.visionMultiplier = 1.0f;
    effect.environment.movementMultiplier = 1.0f;
    effect.environment.damageMultiplier = 1.0f;
    effect.environment.productionMultiplier = 1.0f;
    effect.environment.disablePvP = false;
    effect.environment.darknessLevel = 0.0f;

    return effect;
}

void EventEffects::SetupSpawnConfig(AppliedEffect& effect, const SpawnConfig& config) {
    effect.spawnConfig = config;
}

void EventEffects::SetupEnvironmentConfig(AppliedEffect& effect, const EnvironmentConfig& config) {
    effect.environment = config;
}

void EventEffects::ProcessSpawning(AppliedEffect& effect, float deltaTime) {
    if (effect.spawnConfig.entityType.empty()) return;

    effect.timeSinceLastSpawn += deltaTime;

    // Check initial delay
    if (effect.elapsedTime < effect.spawnConfig.initialDelay) {
        return;
    }

    // Check if one-time spawn
    if (effect.spawnConfig.spawnInterval <= 0.0f) {
        if (effect.spawnWaveCount == 0) {
            int32_t count = effect.spawnConfig.baseCount;
            // Would add player scaling here
            SpawnEntities(effect, count);
            effect.spawnWaveCount++;
        }
        return;
    }

    // Check spawn interval
    if (effect.timeSinceLastSpawn >= effect.spawnConfig.spawnInterval) {
        effect.timeSinceLastSpawn = 0.0f;
        int32_t count = effect.spawnConfig.baseCount;
        SpawnEntities(effect, count);
        effect.spawnWaveCount++;
    }
}

void EventEffects::SpawnEntities(AppliedEffect& effect, int32_t count) {
    // This would interface with EntityManager to actually spawn entities
    // For now, just invoke callbacks

    EFFECTS_LOG_INFO("Spawning " + std::to_string(count) + " " +
                     effect.spawnConfig.entityType + " for event " + effect.eventId);

    std::lock_guard<std::mutex> lock(m_callbackMutex);
    for (const auto& callback : m_spawnCallbacks) {
        if (callback) {
            for (int32_t i = 0; i < count; ++i) {
                std::string entityId = effect.eventId + "_spawn_" +
                                       std::to_string(effect.spawnWaveCount) + "_" +
                                       std::to_string(i);
                effect.affectedEntityIds.insert(entityId);
                callback(entityId, effect.eventId);
            }
        }
    }
}

glm::vec2 EventEffects::GetSpawnPosition(const AppliedEffect& effect) {
    // Generate random position within spawn radius
    std::uniform_real_distribution<float> radiusDist(
        effect.spawnConfig.radiusMin, effect.spawnConfig.radiusMax);
    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159f);

    float radius = radiusDist(m_rng);
    float angle = angleDist(m_rng);

    // Would need to get event location - for now return offset from origin
    return {radius * std::cos(angle), radius * std::sin(angle)};
}

void EventEffects::SpawnLoot(const AppliedEffect& effect) {
    EFFECTS_LOG_INFO("Spawning loot for event " + effect.eventId);

    std::lock_guard<std::mutex> lock(m_callbackMutex);
    for (const auto& callback : m_lootCallbacks) {
        if (callback) {
            glm::vec2 pos = GetSpawnPosition(effect);
            callback(pos, effect.lootConfig, effect.eventId);
        }
    }
}

void EventEffects::ApplyEntityModifiers(const AppliedEffect& effect) {
    // This would interface with EntityManager to apply modifiers
    EFFECTS_LOG_INFO("Applying entity modifiers for event " + effect.eventId);
}

void EventEffects::RemoveEntityModifiers(const AppliedEffect& effect) {
    // This would interface with EntityManager to remove modifiers
    EFFECTS_LOG_INFO("Removing entity modifiers for event " + effect.eventId);
}

void EventEffects::ApplyEnvironmentEffect(const AppliedEffect& effect) {
    m_environmentDirty = true;
    UpdateCombinedEnvironment();

    std::lock_guard<std::mutex> lock(m_callbackMutex);
    for (const auto& callback : m_environmentCallbacks) {
        if (callback) {
            callback(m_combinedEnvironment);
        }
    }
}

void EventEffects::RemoveEnvironmentEffect(const AppliedEffect& effect) {
    // Check if this was affecting PvP or XP
    if (effect.environment.disablePvP) {
        // Check if any other effect disables PvP
        bool stillDisabled = false;
        for (const auto& [id, e] : m_activeEffects) {
            if (id != effect.effectId && e.environment.disablePvP) {
                stillDisabled = true;
                break;
            }
        }
        m_pvpDisabled = stillDisabled;
    }

    // Reset XP multiplier if this was a double XP event
    if (effect.eventType == EventType::DoubleXP) {
        m_experienceMultiplier = 1.0f;
    }

    m_environmentDirty = true;
}

void EventEffects::UpdateCombinedEnvironment() {
    m_combinedEnvironment.visionMultiplier = 1.0f;
    m_combinedEnvironment.movementMultiplier = 1.0f;
    m_combinedEnvironment.damageMultiplier = 1.0f;
    m_combinedEnvironment.productionMultiplier = 1.0f;
    m_combinedEnvironment.darknessLevel = 0.0f;
    m_combinedEnvironment.disablePvP = false;

    for (const auto& [id, effect] : m_activeEffects) {
        if (!effect.isActive) continue;

        m_combinedEnvironment.visionMultiplier *= effect.environment.visionMultiplier;
        m_combinedEnvironment.movementMultiplier *= effect.environment.movementMultiplier;
        m_combinedEnvironment.damageMultiplier *= effect.environment.damageMultiplier;
        m_combinedEnvironment.productionMultiplier *= effect.environment.productionMultiplier;
        m_combinedEnvironment.darknessLevel = std::max(
            m_combinedEnvironment.darknessLevel, effect.environment.darknessLevel);

        if (effect.environment.disablePvP) {
            m_combinedEnvironment.disablePvP = true;
        }

        // Use latest weather effect
        if (!effect.environment.weatherEffect.empty()) {
            m_combinedEnvironment.weatherEffect = effect.environment.weatherEffect;
        }
        if (!effect.environment.ambientSound.empty()) {
            m_combinedEnvironment.ambientSound = effect.environment.ambientSound;
        }
    }

    m_environmentDirty = false;
}

int64_t EventEffects::GetCurrentTimeMs() const {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

bool EventEffects::IsPositionInEffect(const glm::vec2& pos, const AppliedEffect& effect) const {
    // For global effects, always true
    // For local effects, would check distance to event center
    // Would need event location stored in effect
    return true; // Simplified for now
}

} // namespace Vehement
