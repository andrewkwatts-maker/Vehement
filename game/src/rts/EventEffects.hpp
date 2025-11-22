#pragma once

#include "WorldEvent.hpp"
#include <memory>
#include <vector>
#include <map>
#include <set>
#include <functional>
#include <mutex>

namespace Vehement {

// Forward declarations
class EntityManager;
class World;
class EventScheduler;

/**
 * @brief Spawn configuration for zombie/NPC spawning
 */
struct SpawnConfig {
    std::string entityType;         ///< Entity type to spawn
    int32_t baseCount;              ///< Base number to spawn
    float countPerPlayer;           ///< Additional per player
    float spawnInterval;            ///< Time between spawn waves
    float initialDelay;             ///< Delay before first spawn
    float radiusMin;                ///< Minimum spawn distance from center
    float radiusMax;                ///< Maximum spawn distance from center
    glm::vec2 targetOffset;         ///< Offset from event center for AI target
    bool announceSpawn;             ///< Announce each spawn wave
};

/**
 * @brief Configuration for environmental effects
 */
struct EnvironmentConfig {
    float visionMultiplier;         ///< Multiplier for vision range
    float movementMultiplier;       ///< Multiplier for movement speed
    float damageMultiplier;         ///< Multiplier for damage taken
    float productionMultiplier;     ///< Multiplier for resource production
    bool disablePvP;                ///< If true, PvP is disabled
    float darknessLevel;            ///< 0.0 = normal, 1.0 = complete darkness
    std::string weatherEffect;      ///< Weather particle effect name
    std::string ambientSound;       ///< Ambient sound to play
};

/**
 * @brief Stats modifier for entities during events
 */
struct EntityStatModifier {
    std::string entityTag;          ///< Tag to match entities (empty = all)
    float healthMultiplier;
    float damageMultiplier;
    float speedMultiplier;
    float armorMultiplier;
    float detectionMultiplier;
    int32_t bonusHealth;
    int32_t bonusDamage;
};

/**
 * @brief Loot drop configuration
 */
struct LootConfig {
    std::map<ResourceType, int32_t> guaranteedResources;
    std::map<ResourceType, std::pair<int32_t, float>> randomResources; // amount, chance
    std::vector<std::pair<std::string, float>> itemDrops; // itemId, chance
    float qualityMultiplier;        ///< Loot quality scaling
    int32_t experienceReward;
};

/**
 * @brief Active effect instance being applied
 */
struct AppliedEffect {
    std::string effectId;
    std::string eventId;
    EventType eventType;

    // Duration tracking
    int64_t startTime;
    int64_t endTime;
    float elapsedTime;

    // Effect data
    EnvironmentConfig environment;
    std::vector<EntityStatModifier> entityModifiers;
    SpawnConfig spawnConfig;
    LootConfig lootConfig;

    // State
    bool isActive;
    int32_t spawnWaveCount;
    float timeSinceLastSpawn;
    std::set<std::string> affectedEntityIds;

    [[nodiscard]] bool IsExpired(int64_t currentTimeMs) const {
        return currentTimeMs >= endTime;
    }

    [[nodiscard]] float GetProgress(int64_t currentTimeMs) const {
        if (currentTimeMs <= startTime) return 0.0f;
        if (currentTimeMs >= endTime) return 1.0f;
        return static_cast<float>(currentTimeMs - startTime) /
               static_cast<float>(endTime - startTime);
    }
};

/**
 * @brief Manages gameplay effects of world events
 *
 * Responsible for:
 * - Applying event modifiers to entities
 * - Spawning zombies/NPCs for events
 * - Managing environmental effects
 * - Tracking active effects
 * - Cleaning up expired effects
 */
class EventEffects {
public:
    /**
     * @brief Construct EventEffects system
     */
    EventEffects();

    /**
     * @brief Destructor
     */
    ~EventEffects();

    // Non-copyable
    EventEffects(const EventEffects&) = delete;
    EventEffects& operator=(const EventEffects&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the effects system
     * @param entityManager Reference to entity manager for spawning
     * @param world Reference to world for terrain effects
     * @return true if initialization succeeded
     */
    [[nodiscard]] bool Initialize(EntityManager* entityManager, World* world);

    /**
     * @brief Shutdown the effects system
     */
    void Shutdown();

    /**
     * @brief Set the event scheduler reference
     */
    void SetEventScheduler(EventScheduler* scheduler) { m_scheduler = scheduler; }

    // =========================================================================
    // Update
    // =========================================================================

    /**
     * @brief Update all active effects
     * @param deltaTime Time since last update in seconds
     */
    void Update(float deltaTime);

    /**
     * @brief Clean up expired effects
     */
    void CleanupExpiredEffects();

    // =========================================================================
    // Effect Application
    // =========================================================================

    /**
     * @brief Apply effects for a world event
     * @param event The event to apply effects for
     */
    void ApplyEvent(const WorldEvent& event);

    /**
     * @brief Remove effects for an event
     * @param eventId ID of the event to remove effects for
     */
    void RemoveEventEffects(const std::string& eventId);

    /**
     * @brief Remove all active effects
     */
    void RemoveAllEffects();

    // =========================================================================
    // Specific Event Effects
    // =========================================================================

    /**
     * @brief Apply Zombie Horde effect - spawn many zombies attacking bases
     */
    void ApplyZombieHorde(const WorldEvent& event);

    /**
     * @brief Apply Boss Zombie effect - spawn powerful boss enemy
     */
    void ApplyBossZombie(const WorldEvent& event);

    /**
     * @brief Apply Plague effect - disease debuff on workers
     */
    void ApplyPlague(const WorldEvent& event);

    /**
     * @brief Apply Infestation effect - zombies spawn inside buildings
     */
    void ApplyInfestation(const WorldEvent& event);

    /**
     * @brief Apply Night Terror effect - enhanced zombie abilities at night
     */
    void ApplyNightTerror(const WorldEvent& event);

    /**
     * @brief Apply Supply Drop effect - spawn loot containers
     */
    void ApplySupplyDrop(const WorldEvent& event);

    /**
     * @brief Apply Refugee Camp effect - spawn recruitable NPCs
     */
    void ApplyRefugeeCamp(const WorldEvent& event);

    /**
     * @brief Apply Treasure Cache effect - spawn valuable loot
     */
    void ApplyTreasureCache(const WorldEvent& event);

    /**
     * @brief Apply Abandoned Base effect - create claimable structure
     */
    void ApplyAbandonedBase(const WorldEvent& event);

    /**
     * @brief Apply Weapon Cache effect - spawn military equipment
     */
    void ApplyWeaponCache(const WorldEvent& event);

    /**
     * @brief Apply Storm effect - reduce vision and slow movement
     */
    void ApplyStorm(const WorldEvent& event);

    /**
     * @brief Apply Earthquake effect - damage buildings
     */
    void ApplyEarthquake(const WorldEvent& event);

    /**
     * @brief Apply Drought effect - reduce farm output
     */
    void ApplyDrought(const WorldEvent& event);

    /**
     * @brief Apply Bountiful effect - increase all production
     */
    void ApplyBountiful(const WorldEvent& event);

    /**
     * @brief Apply Fog effect - severely reduce visibility
     */
    void ApplyFog(const WorldEvent& event);

    /**
     * @brief Apply Heat Wave effect - reduce stamina and speed
     */
    void ApplyHeatWave(const WorldEvent& event);

    /**
     * @brief Apply Trade Caravan effect - spawn NPC traders
     */
    void ApplyTradeCaravan(const WorldEvent& event);

    /**
     * @brief Apply Military Aid effect - spawn allied NPC soldiers
     */
    void ApplyMilitaryAid(const WorldEvent& event);

    /**
     * @brief Apply Bandits effect - spawn hostile NPCs
     */
    void ApplyBandits(const WorldEvent& event);

    /**
     * @brief Apply Deserters effect - spawn recruitable enemy soldiers
     */
    void ApplyDeserters(const WorldEvent& event);

    /**
     * @brief Apply Merchant effect - spawn rare item trader
     */
    void ApplyMerchant(const WorldEvent& event);

    /**
     * @brief Apply Blood Moon effect - buff all zombie stats
     */
    void ApplyBloodMoon(const WorldEvent& event);

    /**
     * @brief Apply Eclipse effect - extended darkness
     */
    void ApplyEclipse(const WorldEvent& event);

    /**
     * @brief Apply Golden Age effect - bonus to all players
     */
    void ApplyGoldenAge(const WorldEvent& event);

    /**
     * @brief Apply Apocalypse effect - massive multi-wave assault
     */
    void ApplyApocalypse(const WorldEvent& event);

    /**
     * @brief Apply Ceasefire effect - disable PvP
     */
    void ApplyCeasefire(const WorldEvent& event);

    /**
     * @brief Apply Double XP effect - double experience gains
     */
    void ApplyDoubleXP(const WorldEvent& event);

    // =========================================================================
    // Effect Queries
    // =========================================================================

    /**
     * @brief Get all currently active effects
     */
    [[nodiscard]] std::vector<AppliedEffect> GetActiveEffects() const;

    /**
     * @brief Get effects affecting a specific position
     */
    [[nodiscard]] std::vector<AppliedEffect> GetEffectsAtPosition(const glm::vec2& pos) const;

    /**
     * @brief Get effects affecting a specific entity
     */
    [[nodiscard]] std::vector<AppliedEffect> GetEffectsForEntity(const std::string& entityId) const;

    /**
     * @brief Check if any effect of a type is active
     */
    [[nodiscard]] bool IsEffectTypeActive(EventType type) const;

    /**
     * @brief Get the combined environment config from all active effects
     */
    [[nodiscard]] EnvironmentConfig GetCombinedEnvironmentConfig() const;

    /**
     * @brief Get stat modifiers for an entity from all active effects
     */
    [[nodiscard]] EntityStatModifier GetCombinedEntityModifiers(const std::string& entityId,
                                                                 const std::string& entityTag) const;

    // =========================================================================
    // Modifier Calculation
    // =========================================================================

    /**
     * @brief Calculate vision modifier at position
     */
    [[nodiscard]] float GetVisionModifier(const glm::vec2& pos) const;

    /**
     * @brief Calculate movement speed modifier at position
     */
    [[nodiscard]] float GetMovementModifier(const glm::vec2& pos) const;

    /**
     * @brief Calculate production modifier (global + local)
     */
    [[nodiscard]] float GetProductionModifier(const glm::vec2& pos) const;

    /**
     * @brief Calculate damage modifier at position
     */
    [[nodiscard]] float GetDamageModifier(const glm::vec2& pos) const;

    /**
     * @brief Check if PvP is currently disabled
     */
    [[nodiscard]] bool IsPvPDisabled() const;

    /**
     * @brief Get current darkness level (0.0 - 1.0)
     */
    [[nodiscard]] float GetDarknessLevel() const;

    /**
     * @brief Get current experience multiplier
     */
    [[nodiscard]] float GetExperienceMultiplier() const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Register callback for when entities are spawned
     */
    using EntitySpawnCallback = std::function<void(const std::string& entityId,
                                                   const std::string& eventId)>;
    void OnEntitySpawned(EntitySpawnCallback callback);

    /**
     * @brief Register callback for when loot is dropped
     */
    using LootDropCallback = std::function<void(const glm::vec2& position,
                                                const LootConfig& loot,
                                                const std::string& eventId)>;
    void OnLootDropped(LootDropCallback callback);

    /**
     * @brief Register callback for environmental changes
     */
    using EnvironmentChangeCallback = std::function<void(const EnvironmentConfig& config)>;
    void OnEnvironmentChanged(EnvironmentChangeCallback callback);

private:
    // Effect creation helpers
    AppliedEffect CreateBaseEffect(const WorldEvent& event);
    void SetupSpawnConfig(AppliedEffect& effect, const SpawnConfig& config);
    void SetupEnvironmentConfig(AppliedEffect& effect, const EnvironmentConfig& config);

    // Spawn helpers
    void ProcessSpawning(AppliedEffect& effect, float deltaTime);
    void SpawnEntities(AppliedEffect& effect, int32_t count);
    glm::vec2 GetSpawnPosition(const AppliedEffect& effect);
    void SpawnLoot(const AppliedEffect& effect);

    // Entity modification helpers
    void ApplyEntityModifiers(const AppliedEffect& effect);
    void RemoveEntityModifiers(const AppliedEffect& effect);

    // Environment helpers
    void ApplyEnvironmentEffect(const AppliedEffect& effect);
    void RemoveEnvironmentEffect(const AppliedEffect& effect);
    void UpdateCombinedEnvironment();

    // Utility
    [[nodiscard]] int64_t GetCurrentTimeMs() const;
    [[nodiscard]] bool IsPositionInEffect(const glm::vec2& pos, const AppliedEffect& effect) const;

    // State
    bool m_initialized = false;
    EntityManager* m_entityManager = nullptr;
    World* m_world = nullptr;
    EventScheduler* m_scheduler = nullptr;

    // Active effects
    std::map<std::string, AppliedEffect> m_activeEffects;
    mutable std::mutex m_effectMutex;

    // Combined state (cached)
    EnvironmentConfig m_combinedEnvironment;
    float m_experienceMultiplier = 1.0f;
    bool m_pvpDisabled = false;
    mutable bool m_environmentDirty = true;

    // Callbacks
    std::vector<EntitySpawnCallback> m_spawnCallbacks;
    std::vector<LootDropCallback> m_lootCallbacks;
    std::vector<EnvironmentChangeCallback> m_environmentCallbacks;
    std::mutex m_callbackMutex;

    // Random generation
    mutable std::mt19937 m_rng;
};

} // namespace Vehement
