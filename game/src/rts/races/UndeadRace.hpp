#pragma once

/**
 * @file UndeadRace.hpp
 * @brief Undead Race (The Scourge) implementation for the RTS game
 *
 * Features:
 * - Corpse Economy: Raise free units from battlefield corpses
 * - Blight System: Buildings require blight terrain, which spreads from structures
 * - No Food Upkeep: Undead units don't consume food
 * - Blight Regeneration: Units heal while on blighted ground
 * - Necromancy: Chance to raise killed enemies as undead
 */

#include "../Culture.hpp"
#include "../Faction.hpp"
#include "../TechTree.hpp"
#include "../Ability.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <cstdint>

namespace Vehement {
namespace RTS {

// Forward declarations
class Entity;
class Building;
class Unit;
class Hero;

// ============================================================================
// Undead Race Constants
// ============================================================================

namespace UndeadConstants {
    // Blight system
    constexpr float BLIGHT_SPREAD_RATE = 0.5f;           // Tiles per second
    constexpr float BLIGHT_BASE_RADIUS = 8.0f;          // Base blight radius
    constexpr float BLIGHT_REGEN_RATE = 2.0f;           // HP/sec on blight
    constexpr float BLIGHT_MANA_REGEN = 1.0f;           // Mana/sec on blight

    // Corpse system
    constexpr float CORPSE_DECAY_TIME = 120.0f;         // Seconds until corpse disappears
    constexpr float CORPSE_GATHER_RADIUS = 15.0f;       // Range to collect corpses
    constexpr int MAX_STORED_CORPSES = 8;               // Per meat wagon
    constexpr float BASE_RAISE_CHANCE = 0.15f;          // Chance to auto-raise killed enemies

    // Damage modifiers
    constexpr float HOLY_DAMAGE_MULTIPLIER = 1.5f;      // Extra damage from holy
    constexpr float FIRE_DAMAGE_MULTIPLIER = 1.25f;     // Extra damage from fire
    constexpr float ICE_DAMAGE_MULTIPLIER = 0.75f;      // Reduced damage from ice
    constexpr float POISON_DAMAGE_MULTIPLIER = 0.0f;    // Immune to poison

    // Movement
    constexpr float BASE_SPEED_MULTIPLIER = 0.85f;      // Slower than living

    // Population
    constexpr int BASE_POPULATION_CAP = 12;
    constexpr int ZIGGURAT_POPULATION = 10;
    constexpr int MAX_POPULATION = 200;
}

// ============================================================================
// Corpse System
// ============================================================================

/**
 * @brief Represents a corpse on the battlefield
 */
struct Corpse {
    uint32_t id = 0;
    glm::vec3 position{0.0f};
    std::string sourceUnitType;
    int sourceUnitHealth = 0;
    float decayTimer = UndeadConstants::CORPSE_DECAY_TIME;
    bool isReserved = false;        // Being used by a raise ability
    uint32_t reservedBy = 0;        // Entity ID that reserved it

    [[nodiscard]] bool IsDecayed() const { return decayTimer <= 0.0f; }
};

/**
 * @brief Manages corpses on the battlefield
 */
class CorpseManager {
public:
    static CorpseManager& Instance();

    // Non-copyable
    CorpseManager(const CorpseManager&) = delete;
    CorpseManager& operator=(const CorpseManager&) = delete;

    /**
     * @brief Update all corpses (decay timers)
     */
    void Update(float deltaTime);

    /**
     * @brief Create a corpse at a position
     * @return Corpse ID
     */
    uint32_t CreateCorpse(const glm::vec3& position, const std::string& unitType, int health);

    /**
     * @brief Remove a corpse (consumed or decayed)
     */
    void RemoveCorpse(uint32_t corpseId);

    /**
     * @brief Find corpses within radius of a position
     */
    std::vector<Corpse*> FindCorpsesInRadius(const glm::vec3& position, float radius);

    /**
     * @brief Get nearest unreserved corpse
     */
    Corpse* GetNearestCorpse(const glm::vec3& position, float maxRange);

    /**
     * @brief Reserve a corpse for raising
     */
    bool ReserveCorpse(uint32_t corpseId, uint32_t entityId);

    /**
     * @brief Release a reserved corpse
     */
    void ReleaseCorpse(uint32_t corpseId);

    /**
     * @brief Consume a corpse (for raising or cannibalize)
     */
    bool ConsumeCorpse(uint32_t corpseId);

    /**
     * @brief Get corpse by ID
     */
    Corpse* GetCorpse(uint32_t corpseId);

    /**
     * @brief Get all corpses
     */
    const std::unordered_map<uint32_t, Corpse>& GetAllCorpses() const { return m_corpses; }

    /**
     * @brief Get corpse count
     */
    [[nodiscard]] size_t GetCorpseCount() const { return m_corpses.size(); }

private:
    CorpseManager() = default;

    std::unordered_map<uint32_t, Corpse> m_corpses;
    uint32_t m_nextCorpseId = 1;
};

// ============================================================================
// Blight System
// ============================================================================

/**
 * @brief Represents a blight tile
 */
struct BlightTile {
    glm::ivec2 position{0, 0};
    float intensity = 1.0f;         // 0-1, affects visuals
    uint32_t sourceBuilding = 0;    // Building that created this blight
    float spreadTimer = 0.0f;
};

/**
 * @brief Manages blight terrain
 */
class BlightManager {
public:
    static BlightManager& Instance();

    // Non-copyable
    BlightManager(const BlightManager&) = delete;
    BlightManager& operator=(const BlightManager&) = delete;

    /**
     * @brief Update blight spread
     */
    void Update(float deltaTime);

    /**
     * @brief Add blight source (from building)
     */
    void AddBlightSource(uint32_t buildingId, const glm::vec3& position, float radius);

    /**
     * @brief Remove blight source (building destroyed)
     */
    void RemoveBlightSource(uint32_t buildingId);

    /**
     * @brief Check if a position is on blight
     */
    [[nodiscard]] bool IsOnBlight(const glm::vec3& position) const;

    /**
     * @brief Check if a position is valid for undead building
     */
    [[nodiscard]] bool CanBuildHere(const glm::vec3& position, bool requiresBlight) const;

    /**
     * @brief Get blight intensity at position (0-1)
     */
    [[nodiscard]] float GetBlightIntensity(const glm::vec3& position) const;

    /**
     * @brief Get all blighted tiles
     */
    const std::unordered_set<uint64_t>& GetBlightedTiles() const { return m_blightedTiles; }

    /**
     * @brief Set blight spread rate multiplier
     */
    void SetSpreadRateMultiplier(float mult) { m_spreadRateMultiplier = mult; }

private:
    BlightManager() = default;

    void SpreadBlight(const glm::ivec2& from, float radius);
    uint64_t TileKey(int x, int y) const { return (static_cast<uint64_t>(x) << 32) | static_cast<uint32_t>(y); }

    struct BlightSource {
        uint32_t buildingId;
        glm::vec3 position;
        float radius;
    };

    std::vector<BlightSource> m_blightSources;
    std::unordered_set<uint64_t> m_blightedTiles;
    std::unordered_map<uint64_t, BlightTile> m_blightData;
    float m_spreadRateMultiplier = 1.0f;
};

// ============================================================================
// Undead Race Manager
// ============================================================================

/**
 * @brief Main manager for the Undead race mechanics
 */
class UndeadRace {
public:
    static UndeadRace& Instance();

    // Non-copyable
    UndeadRace(const UndeadRace&) = delete;
    UndeadRace& operator=(const UndeadRace&) = delete;

    /**
     * @brief Initialize the Undead race
     */
    bool Initialize();

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Update race mechanics
     */
    void Update(float deltaTime);

    // =========================================================================
    // Unit Management
    // =========================================================================

    /**
     * @brief Check if a unit is undead
     */
    [[nodiscard]] bool IsUndeadUnit(uint32_t entityId) const;

    /**
     * @brief Register an undead unit
     */
    void RegisterUndeadUnit(uint32_t entityId, const std::string& unitType);

    /**
     * @brief Unregister an undead unit
     */
    void UnregisterUndeadUnit(uint32_t entityId);

    /**
     * @brief Apply undead bonuses to a unit
     */
    void ApplyUndeadBonuses(uint32_t entityId);

    /**
     * @brief Get damage multiplier for damage type
     */
    [[nodiscard]] float GetDamageMultiplier(const std::string& damageType) const;

    // =========================================================================
    // Corpse Economy
    // =========================================================================

    /**
     * @brief Called when a unit dies - potentially creates corpse
     */
    void OnUnitDeath(uint32_t entityId, const glm::vec3& position, const std::string& unitType, int health);

    /**
     * @brief Try to raise a skeleton from a corpse
     */
    bool RaiseFromCorpse(uint32_t corpseId, uint32_t casterId, const std::string& summonType);

    /**
     * @brief Cannibalize a corpse for healing
     */
    bool CannibalizeCorpse(uint32_t corpseId, uint32_t consumerId, float& healAmount);

    /**
     * @brief Get stored corpse count for an entity (meat wagon)
     */
    [[nodiscard]] int GetStoredCorpses(uint32_t entityId) const;

    /**
     * @brief Store a corpse in an entity
     */
    bool StoreCorpse(uint32_t entityId, uint32_t corpseId);

    /**
     * @brief Drop a corpse from storage
     */
    bool DropCorpse(uint32_t entityId, const glm::vec3& position);

    // =========================================================================
    // Blight System
    // =========================================================================

    /**
     * @brief Apply blight regeneration to undead units
     */
    void ApplyBlightRegeneration(float deltaTime);

    /**
     * @brief Check if building can be placed (blight requirement)
     */
    [[nodiscard]] bool CanPlaceBuilding(const std::string& buildingType, const glm::vec3& position) const;

    /**
     * @brief Called when undead building is constructed
     */
    void OnBuildingConstructed(uint32_t buildingId, const std::string& buildingType, const glm::vec3& position);

    /**
     * @brief Called when undead building is destroyed
     */
    void OnBuildingDestroyed(uint32_t buildingId);

    // =========================================================================
    // Necromancy
    // =========================================================================

    /**
     * @brief Set the auto-raise chance for killed enemies
     */
    void SetRaiseChance(float chance) { m_raiseChance = chance; }

    /**
     * @brief Get current raise chance
     */
    [[nodiscard]] float GetRaiseChance() const { return m_raiseChance; }

    /**
     * @brief Try to auto-raise a killed enemy
     */
    bool TryAutoRaise(const glm::vec3& position, const std::string& unitType);

    // =========================================================================
    // Population
    // =========================================================================

    /**
     * @brief Get current population cap
     */
    [[nodiscard]] int GetPopulationCap() const;

    /**
     * @brief Get current population used
     */
    [[nodiscard]] int GetPopulationUsed() const { return m_populationUsed; }

    /**
     * @brief Add to population used
     */
    void AddPopulation(int amount) { m_populationUsed += amount; }

    /**
     * @brief Remove from population used
     */
    void RemovePopulation(int amount) { m_populationUsed = std::max(0, m_populationUsed - amount); }

    // =========================================================================
    // Callbacks
    // =========================================================================

    using CorpseCreatedCallback = std::function<void(uint32_t corpseId, const glm::vec3& position)>;
    using UnitRaisedCallback = std::function<void(uint32_t unitId, const glm::vec3& position)>;
    using BlightSpreadCallback = std::function<void(const glm::ivec2& tile)>;

    void SetOnCorpseCreated(CorpseCreatedCallback cb) { m_onCorpseCreated = std::move(cb); }
    void SetOnUnitRaised(UnitRaisedCallback cb) { m_onUnitRaised = std::move(cb); }
    void SetOnBlightSpread(BlightSpreadCallback cb) { m_onBlightSpread = std::move(cb); }

    // =========================================================================
    // Configuration Loading
    // =========================================================================

    /**
     * @brief Load race configuration from JSON
     */
    bool LoadConfiguration(const std::string& configPath);

    /**
     * @brief Get unit configuration
     */
    [[nodiscard]] const nlohmann::json* GetUnitConfig(const std::string& unitType) const;

    /**
     * @brief Get building configuration
     */
    [[nodiscard]] const nlohmann::json* GetBuildingConfig(const std::string& buildingType) const;

    /**
     * @brief Get hero configuration
     */
    [[nodiscard]] const nlohmann::json* GetHeroConfig(const std::string& heroType) const;

private:
    UndeadRace() = default;

    // Configuration data
    nlohmann::json m_raceConfig;
    std::unordered_map<std::string, nlohmann::json> m_unitConfigs;
    std::unordered_map<std::string, nlohmann::json> m_buildingConfigs;
    std::unordered_map<std::string, nlohmann::json> m_heroConfigs;

    // Unit tracking
    std::unordered_set<uint32_t> m_undeadUnits;
    std::unordered_map<uint32_t, std::string> m_unitTypes;

    // Corpse storage (for meat wagons)
    std::unordered_map<uint32_t, std::vector<uint32_t>> m_storedCorpses;

    // Building tracking (for blight)
    std::unordered_map<uint32_t, std::string> m_buildings;
    int m_zigguratCount = 0;

    // Population
    int m_populationUsed = 0;

    // Necromancy
    float m_raiseChance = UndeadConstants::BASE_RAISE_CHANCE;

    // State
    bool m_initialized = false;

    // Callbacks
    CorpseCreatedCallback m_onCorpseCreated;
    UnitRaisedCallback m_onUnitRaised;
    BlightSpreadCallback m_onBlightSpread;
};

// ============================================================================
// Undead Ability Implementations
// ============================================================================

/**
 * @brief Raise Dead ability - raises skeletons from corpses
 */
class RaiseDeadAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Cannibalize ability - consume corpse for health
 */
class CannibalizeAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
    void Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) override;
    void OnEnd(const AbilityCastContext& context, const AbilityData& data) override;

private:
    uint32_t m_targetCorpse = 0;
    float m_channelTime = 0.0f;
};

/**
 * @brief Death Coil ability - damages enemies or heals undead
 */
class DeathCoilAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Frost Nova ability - AoE frost damage and slow
 */
class FrostNovaAbility : public AbilityBehavior {
public:
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Unholy Aura ability - passive movement and regen aura
 */
class UnholyAuraAbility : public AbilityBehavior {
public:
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
    void Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) override;
};

/**
 * @brief Dark Ritual ability - sacrifice undead for mana
 */
class DarkRitualAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

/**
 * @brief Animate Dead ability - raises multiple corpses as invulnerable undead
 */
class AnimateDeadAbility : public AbilityBehavior {
public:
    [[nodiscard]] bool CanCast(const AbilityCastContext& context, const AbilityData& data) const override;
    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override;
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Register all undead abilities with the ability manager
 */
void RegisterUndeadAbilities();

/**
 * @brief Check if a unit type leaves a corpse
 */
[[nodiscard]] bool UnitLeavesCorpse(const std::string& unitType);

/**
 * @brief Get the skeleton type to raise based on corpse source
 */
[[nodiscard]] std::string GetRaisedUnitType(const std::string& corpseSource);

} // namespace RTS
} // namespace Vehement
