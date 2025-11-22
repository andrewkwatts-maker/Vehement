#pragma once

/**
 * @file Faction.hpp
 * @brief AI factions for PvE gameplay
 *
 * Expands beyond zombies with diverse enemy types:
 * - Zombies: Mindless horde (original)
 * - Bandits: Human raiders
 * - Wild Creatures: Wildlife threats
 * - Ancient Guardians: Protect ruins
 * - Rival Kingdom: AI-controlled civilizations
 * - Natural Disasters: Environmental threats
 */

#include "Culture.hpp"
#include "Resource.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <cstdint>

namespace Vehement {
namespace RTS {

// Forward declarations
class Building;
struct Voxel;

// ============================================================================
// Faction Types
// ============================================================================

/**
 * @brief Types of AI factions in the game
 */
enum class FactionType : uint8_t {
    // Hostile factions
    Zombies,            ///< Mindless undead horde
    Bandits,            ///< Human raiders and thieves
    WildCreatures,      ///< Dangerous wildlife
    AncientGuardians,   ///< Protectors of ruins/artifacts
    RivalKingdom,       ///< AI-controlled civilization
    CultOfDarkness,     ///< Dark magic users
    MutantSwarm,        ///< Mutated creatures

    // Environmental threats
    NaturalDisasters,   ///< Earthquakes, floods, etc.

    // Neutral/Friendly
    Merchants,          ///< Traveling traders
    Refugees,           ///< Survivors seeking shelter
    WildAnimals,        ///< Non-aggressive wildlife

    COUNT
};

/**
 * @brief Get faction name
 */
inline const char* FactionTypeToString(FactionType type) {
    switch (type) {
        case FactionType::Zombies:          return "Zombies";
        case FactionType::Bandits:          return "Bandits";
        case FactionType::WildCreatures:    return "Wild Creatures";
        case FactionType::AncientGuardians: return "Ancient Guardians";
        case FactionType::RivalKingdom:     return "Rival Kingdom";
        case FactionType::CultOfDarkness:   return "Cult of Darkness";
        case FactionType::MutantSwarm:      return "Mutant Swarm";
        case FactionType::NaturalDisasters: return "Natural Disasters";
        case FactionType::Merchants:        return "Merchants";
        case FactionType::Refugees:         return "Refugees";
        case FactionType::WildAnimals:      return "Wild Animals";
        default:                            return "Unknown";
    }
}

/**
 * @brief Get faction description
 */
inline const char* GetFactionDescription(FactionType type) {
    switch (type) {
        case FactionType::Zombies:
            return "Mindless undead driven by hunger. Endless waves attack at night.";
        case FactionType::Bandits:
            return "Human raiders who attack settlements for resources. Can be negotiated with.";
        case FactionType::WildCreatures:
            return "Dangerous predators roaming the wilderness. Defend their territory.";
        case FactionType::AncientGuardians:
            return "Mysterious beings protecting ancient ruins and artifacts.";
        case FactionType::RivalKingdom:
            return "Competing civilization with their own base, army, and ambitions.";
        case FactionType::CultOfDarkness:
            return "Fanatical cultists performing dark rituals and summoning horrors.";
        case FactionType::MutantSwarm:
            return "Twisted creatures from contaminated zones. Highly aggressive.";
        case FactionType::NaturalDisasters:
            return "Earthquakes, storms, and floods that can devastate your settlement.";
        case FactionType::Merchants:
            return "Traveling traders offering goods and services.";
        case FactionType::Refugees:
            return "Survivors seeking shelter. May join your settlement.";
        case FactionType::WildAnimals:
            return "Common wildlife that flees from threats.";
        default:
            return "Unknown faction";
    }
}

// ============================================================================
// Faction Hostility
// ============================================================================

/**
 * @brief Hostility level toward player
 */
enum class Hostility : uint8_t {
    Friendly,       ///< Will help player
    Neutral,        ///< Ignores player
    Suspicious,     ///< Watches player, may attack if provoked
    Hostile,        ///< Attacks on sight
    Berserk         ///< Attacks everything
};

/**
 * @brief Get default hostility for faction type
 */
inline Hostility GetDefaultHostility(FactionType type) {
    switch (type) {
        case FactionType::Zombies:          return Hostility::Berserk;
        case FactionType::Bandits:          return Hostility::Hostile;
        case FactionType::WildCreatures:    return Hostility::Hostile;
        case FactionType::AncientGuardians: return Hostility::Neutral;
        case FactionType::RivalKingdom:     return Hostility::Suspicious;
        case FactionType::CultOfDarkness:   return Hostility::Hostile;
        case FactionType::MutantSwarm:      return Hostility::Berserk;
        case FactionType::NaturalDisasters: return Hostility::Berserk;
        case FactionType::Merchants:        return Hostility::Friendly;
        case FactionType::Refugees:         return Hostility::Friendly;
        case FactionType::WildAnimals:      return Hostility::Neutral;
        default:                            return Hostility::Neutral;
    }
}

// ============================================================================
// Unit Types per Faction
// ============================================================================

/**
 * @brief Unit types that factions can spawn
 */
enum class FactionUnitType : uint8_t {
    // Zombie units
    ZombieWalker,       ///< Basic slow zombie
    ZombieRunner,       ///< Fast zombie
    ZombieBrute,        ///< Large tanky zombie
    ZombieSpitter,      ///< Ranged acid attack
    ZombieScreamer,     ///< Alerts other zombies

    // Bandit units
    BanditScout,        ///< Fast, light armor
    BanditRaider,       ///< Standard fighter
    BanditArcher,       ///< Ranged attacker
    BanditBoss,         ///< Leader with buffs

    // Wild creature units
    Wolf,               ///< Fast pack hunter
    Bear,               ///< Strong and tanky
    GiantSpider,        ///< Webbing attacks
    Wyvern,             ///< Flying attacker

    // Guardian units
    StoneGolem,         ///< Heavy defense
    SpectralKnight,     ///< Fast melee
    AncientMage,        ///< Ranged magic

    // Rival kingdom units
    Peasant,            ///< Worker unit
    Militia,            ///< Basic soldier
    Knight,             ///< Heavy cavalry
    Siege,              ///< Building destroyer

    // Cult units
    Cultist,            ///< Basic summoner
    DarkPriest,         ///< Healer/buffer
    DemonSpawn,         ///< Summoned creature

    // Mutant units
    MutantDog,          ///< Fast attacker
    Abomination,        ///< Multi-target melee
    ToxicBlob,          ///< AoE poison

    COUNT
};

/**
 * @brief Statistics for a faction unit
 */
struct FactionUnitStats {
    FactionUnitType type;
    std::string name;
    std::string description;

    int health = 100;
    int armor = 0;
    float moveSpeed = 3.0f;
    float attackDamage = 10.0f;
    float attackRange = 1.0f;
    float attackSpeed = 1.0f;
    float visionRange = 10.0f;

    bool canSwim = false;
    bool canFly = false;
    bool canClimb = false;
    bool canBurrow = false;

    // Resources dropped on death
    ResourceCost loot;

    // Experience granted
    int experienceValue = 10;
};

/**
 * @brief Get unit stats for type
 */
FactionUnitStats GetFactionUnitStats(FactionUnitType type);

// ============================================================================
// Faction Behavior
// ============================================================================

/**
 * @brief AI behavior patterns
 */
enum class FactionBehavior : uint8_t {
    Passive,        ///< Does nothing unless attacked
    Patrol,         ///< Moves around area
    Hunt,           ///< Actively seeks targets
    Defend,         ///< Protects specific location
    Raid,           ///< Attacks then retreats
    Siege,          ///< Concentrated assault
    Swarm,          ///< Overwhelming numbers
    Ambush,         ///< Waits to surprise
    Trade           ///< Seeks trade
};

/**
 * @brief Wave/attack pattern
 */
struct AttackWave {
    std::string name;
    std::vector<std::pair<FactionUnitType, int>> units; ///< Unit type and count
    float spawnDelay = 0.0f;        ///< Delay before wave starts
    float spawnInterval = 1.0f;     ///< Time between unit spawns
    glm::ivec3 spawnDirection;      ///< Direction to spawn from
    bool nightOnly = false;         ///< Only spawns at night
};

// ============================================================================
// Faction Data
// ============================================================================

/**
 * @brief Complete faction definition
 */
struct FactionData {
    FactionType type;
    std::string name;
    std::string description;

    // Visuals
    std::string bannerTexture;
    std::string unitTexturePath;
    uint32_t primaryColor;      ///< RGBA
    uint32_t secondaryColor;

    // Behavior
    Hostility defaultHostility;
    FactionBehavior defaultBehavior;
    bool canBeAllied = false;
    bool canBeBribed = false;
    bool respawns = true;

    // Available units
    std::vector<FactionUnitType> availableUnits;

    // Spawn configuration
    float baseSpawnRate = 0.1f;     ///< Units per second base rate
    float difficultyScaling = 1.0f; ///< How much difficulty affects strength
    int minGroupSize = 1;
    int maxGroupSize = 5;

    // Attack patterns
    std::vector<AttackWave> attackWaves;

    // Territory
    float territoryRadius = 50.0f;  ///< How far from spawn they roam
    bool definesTerritory = false;  ///< Has specific territory

    // Special abilities
    std::vector<std::string> specialAbilities;
};

// ============================================================================
// Faction Instance
// ============================================================================

/**
 * @brief Active faction in the game world
 */
class Faction {
public:
    using FactionId = uint32_t;
    static constexpr FactionId INVALID_ID = 0;

    Faction();
    explicit Faction(FactionType type);
    ~Faction();

    // Non-copyable
    Faction(const Faction&) = delete;
    Faction& operator=(const Faction&) = delete;

    /**
     * @brief Initialize faction
     */
    bool Initialize(const FactionData& data);

    /**
     * @brief Update faction AI
     */
    void Update(float deltaTime);

    // =========================================================================
    // Properties
    // =========================================================================

    [[nodiscard]] FactionId GetId() const { return m_id; }
    [[nodiscard]] FactionType GetType() const { return m_type; }
    [[nodiscard]] const std::string& GetName() const { return m_name; }
    [[nodiscard]] Hostility GetHostility() const { return m_hostility; }
    [[nodiscard]] FactionBehavior GetBehavior() const { return m_behavior; }

    void SetHostility(Hostility h) { m_hostility = h; }
    void SetBehavior(FactionBehavior b) { m_behavior = b; }

    // =========================================================================
    // Territory
    // =========================================================================

    /**
     * @brief Set faction home/spawn location
     */
    void SetHomePosition(const glm::vec3& pos) { m_homePosition = pos; }

    /**
     * @brief Get home position
     */
    [[nodiscard]] const glm::vec3& GetHomePosition() const { return m_homePosition; }

    /**
     * @brief Set territory bounds
     */
    void SetTerritory(const glm::vec3& min, const glm::vec3& max);

    /**
     * @brief Check if position is in territory
     */
    [[nodiscard]] bool IsInTerritory(const glm::vec3& pos) const;

    // =========================================================================
    // Unit Management
    // =========================================================================

    /**
     * @brief Get current unit count
     */
    [[nodiscard]] int GetUnitCount() const { return m_currentUnits; }

    /**
     * @brief Get maximum units
     */
    [[nodiscard]] int GetMaxUnits() const { return m_maxUnits; }

    /**
     * @brief Set max units
     */
    void SetMaxUnits(int max) { m_maxUnits = max; }

    /**
     * @brief Spawn a unit
     */
    void SpawnUnit(FactionUnitType type, const glm::vec3& position);

    /**
     * @brief Spawn a wave of units
     */
    void SpawnWave(const AttackWave& wave, const glm::vec3& targetPosition);

    /**
     * @brief Report unit death
     */
    void OnUnitDeath(FactionUnitType type);

    // =========================================================================
    // Relations
    // =========================================================================

    /**
     * @brief Get relationship with player (-100 to +100)
     */
    [[nodiscard]] int GetPlayerRelationship() const { return m_playerRelationship; }

    /**
     * @brief Modify relationship
     */
    void ModifyRelationship(int delta);

    /**
     * @brief Check if can negotiate
     */
    [[nodiscard]] bool CanNegotiate() const;

    /**
     * @brief Attempt bribe
     */
    bool AttemptBribe(const ResourceCost& offer);

    /**
     * @brief Form alliance
     */
    bool FormAlliance();

    // =========================================================================
    // Combat
    // =========================================================================

    /**
     * @brief Get target position for attacks
     */
    [[nodiscard]] glm::vec3 GetAttackTarget() const { return m_attackTarget; }

    /**
     * @brief Set attack target
     */
    void SetAttackTarget(const glm::vec3& target) { m_attackTarget = target; }

    /**
     * @brief Queue an attack
     */
    void QueueAttack(const AttackWave& wave, float delay = 0.0f);

    /**
     * @brief Cancel pending attacks
     */
    void CancelAttacks();

    /**
     * @brief Check if currently attacking
     */
    [[nodiscard]] bool IsAttacking() const { return m_isAttacking; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    using SpawnCallback = std::function<void(FactionUnitType type, const glm::vec3& pos)>;
    using AttackCallback = std::function<void(const AttackWave& wave, const glm::vec3& target)>;
    using RelationCallback = std::function<void(int oldRelation, int newRelation)>;

    void SetOnSpawn(SpawnCallback cb) { m_onSpawn = std::move(cb); }
    void SetOnAttack(AttackCallback cb) { m_onAttack = std::move(cb); }
    void SetOnRelationChange(RelationCallback cb) { m_onRelationChange = std::move(cb); }

private:
    void UpdateSpawning(float deltaTime);
    void UpdateBehavior(float deltaTime);
    void ProcessAttackQueue(float deltaTime);

    static FactionId s_nextId;
    FactionId m_id = INVALID_ID;

    FactionType m_type = FactionType::Zombies;
    std::string m_name;
    Hostility m_hostility = Hostility::Hostile;
    FactionBehavior m_behavior = FactionBehavior::Hunt;

    // Territory
    glm::vec3 m_homePosition{0, 0, 0};
    glm::vec3 m_territoryMin{0, 0, 0};
    glm::vec3 m_territoryMax{100, 100, 100};
    float m_territoryRadius = 50.0f;

    // Units
    int m_currentUnits = 0;
    int m_maxUnits = 50;
    float m_spawnTimer = 0.0f;
    float m_spawnRate = 0.1f;
    std::vector<FactionUnitType> m_availableUnits;

    // Relations
    int m_playerRelationship = 0;
    bool m_canBeAllied = false;
    bool m_canBeBribed = false;

    // Combat
    bool m_isAttacking = false;
    glm::vec3 m_attackTarget{0, 0, 0};

    struct QueuedAttack {
        AttackWave wave;
        float delay;
    };
    std::vector<QueuedAttack> m_attackQueue;

    // Callbacks
    SpawnCallback m_onSpawn;
    AttackCallback m_onAttack;
    RelationCallback m_onRelationChange;
};

// ============================================================================
// Faction Manager
// ============================================================================

/**
 * @brief Manages all factions in the game
 */
class FactionManager {
public:
    /**
     * @brief Get singleton instance
     */
    static FactionManager& Instance();

    // Non-copyable
    FactionManager(const FactionManager&) = delete;
    FactionManager& operator=(const FactionManager&) = delete;

    /**
     * @brief Initialize faction system
     */
    bool Initialize();

    /**
     * @brief Shutdown
     */
    void Shutdown();

    /**
     * @brief Update all factions
     */
    void Update(float deltaTime);

    // =========================================================================
    // Faction Access
    // =========================================================================

    /**
     * @brief Get faction by ID
     */
    Faction* GetFaction(Faction::FactionId id);

    /**
     * @brief Get factions by type
     */
    std::vector<Faction*> GetFactionsByType(FactionType type);

    /**
     * @brief Get all active factions
     */
    const std::vector<std::unique_ptr<Faction>>& GetAllFactions() const { return m_factions; }

    /**
     * @brief Get faction data template
     */
    const FactionData* GetFactionData(FactionType type) const;

    // =========================================================================
    // Faction Creation
    // =========================================================================

    /**
     * @brief Create a new faction instance
     */
    Faction* CreateFaction(FactionType type, const glm::vec3& homePosition);

    /**
     * @brief Remove a faction
     */
    void RemoveFaction(Faction::FactionId id);

    // =========================================================================
    // Global Events
    // =========================================================================

    /**
     * @brief Trigger global attack event
     */
    void TriggerGlobalAttack(FactionType attackerType, const glm::vec3& target);

    /**
     * @brief Trigger natural disaster
     */
    void TriggerDisaster(const std::string& disasterType, const glm::vec3& epicenter,
                         float radius, float intensity);

    /**
     * @brief Set global difficulty multiplier
     */
    void SetDifficultyMultiplier(float mult) { m_difficultyMultiplier = mult; }

    /**
     * @brief Get difficulty multiplier
     */
    [[nodiscard]] float GetDifficultyMultiplier() const { return m_difficultyMultiplier; }

    // =========================================================================
    // Day/Night Cycle Integration
    // =========================================================================

    /**
     * @brief Set current time of day (0-24)
     */
    void SetTimeOfDay(float hour);

    /**
     * @brief Check if it's night
     */
    [[nodiscard]] bool IsNight() const { return m_isNight; }

private:
    FactionManager() = default;
    ~FactionManager() = default;

    void InitializeFactionData();
    void CreateDefaultFactionData(FactionType type);

    std::vector<std::unique_ptr<Faction>> m_factions;
    std::map<FactionType, FactionData> m_factionTemplates;

    float m_difficultyMultiplier = 1.0f;
    float m_timeOfDay = 12.0f;
    bool m_isNight = false;
    bool m_initialized = false;
};

// ============================================================================
// Natural Disaster Types
// ============================================================================

/**
 * @brief Types of natural disasters
 */
enum class DisasterType : uint8_t {
    Earthquake,     ///< Damages buildings
    Flood,          ///< Blocks movement, damages low areas
    Wildfire,       ///< Spreads, burns resources
    Storm,          ///< Reduces visibility, lightning
    Tornado,        ///< Destroys buildings in path
    Blizzard,       ///< Slows units, freezes water
    Drought,        ///< Reduces food production
    Plague,         ///< Damages population
    COUNT
};

/**
 * @brief Disaster event data
 */
struct DisasterEvent {
    DisasterType type;
    glm::vec3 epicenter;
    float radius;
    float intensity;
    float duration;
    float elapsed;
    bool isActive;
};

} // namespace RTS
} // namespace Vehement
