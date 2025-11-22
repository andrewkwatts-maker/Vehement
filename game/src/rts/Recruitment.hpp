#pragma once

#include "Worker.hpp"
#include "Population.hpp"
#include "../entities/NPC.hpp"
#include <vector>
#include <memory>
#include <functional>
#include <string>
#include <unordered_set>

namespace Vehement {

// Forward declarations
class EntityManager;
class Player;

/**
 * @brief Types of potential recruits with different characteristics
 */
enum class RecruitType : uint8_t {
    Regular,        ///< Standard survivor, balanced stats
    Skilled,        ///< Has high skill in one area
    Refugee,        ///< Comes to base seeking shelter, low stats but grateful
    Mercenary,      ///< Good combat skills, but lower loyalty
    Specialist,     ///< Expert in specific job, very high skill
    Leader          ///< Boosts morale of nearby workers
};

/**
 * @brief Convert recruit type to display string
 */
inline const char* RecruitTypeToString(RecruitType type) {
    switch (type) {
        case RecruitType::Regular:    return "Survivor";
        case RecruitType::Skilled:    return "Skilled Survivor";
        case RecruitType::Refugee:    return "Refugee";
        case RecruitType::Mercenary:  return "Mercenary";
        case RecruitType::Specialist: return "Specialist";
        case RecruitType::Leader:     return "Natural Leader";
        default:                      return "Unknown";
    }
}

/**
 * @brief Information about a potential recruit (discovered survivor)
 */
struct RecruitInfo {
    Entity::EntityId npcId = Entity::INVALID_ID;  ///< ID of NPC entity
    RecruitType type = RecruitType::Regular;
    std::string name;
    WorkerJob specialization = WorkerJob::None;   ///< Best job for this recruit
    float recruitDifficulty = 1.0f;               ///< How hard to recruit (1.0 = normal)
    bool discovered = false;                       ///< Has player found this NPC?
    bool canRecruit = false;                       ///< Is in range and conditions met?
    float interactionProgress = 0.0f;             ///< Progress toward recruitment (0-1)

    // Preview stats (shown before recruiting)
    float healthPreview = 100.0f;
    float bestSkillLevel = 10.0f;
    std::string bestSkillName;
    std::string personalityHint;  ///< Brief description of personality
};

/**
 * @brief Configuration for recruitment spawning
 */
struct RecruitmentConfig {
    // Spawn rates
    float baseSpawnChance = 0.01f;       ///< Base chance per second to spawn survivor
    float refugeeChance = 0.3f;          ///< Chance that spawn is a refugee
    float specialistChance = 0.1f;       ///< Chance that spawn is a specialist
    float mercenaryChance = 0.1f;        ///< Chance that spawn is a mercenary
    float leaderChance = 0.05f;          ///< Chance that spawn is a leader

    // Spawn limits
    int maxUnrecruitedSurvivors = 10;    ///< Max survivors waiting to be recruited
    float minSpawnDistance = 30.0f;      ///< Min distance from player to spawn
    float maxSpawnDistance = 100.0f;     ///< Max distance from player to spawn

    // Recruitment requirements
    float interactionRange = 3.0f;       ///< Distance to interact with NPC
    float interactionTime = 2.0f;        ///< Time to recruit (seconds)
    bool requireHousing = true;          ///< Must have housing to recruit

    // Refugee waves
    float refugeeWaveChance = 0.005f;    ///< Chance per second for refugee wave
    int refugeeWaveMin = 2;              ///< Min refugees in wave
    int refugeeWaveMax = 5;              ///< Max refugees in wave
};

/**
 * @brief System for finding and recruiting survivors
 *
 * Handles:
 * - NPC spawning as potential recruits
 * - Discovery (player approaching NPCs)
 * - Recruitment interaction
 * - Different recruit types with varying stats
 * - Refugee waves that come to the player's base
 */
class Recruitment {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    Recruitment();
    explicit Recruitment(const RecruitmentConfig& config);
    ~Recruitment();

    // Non-copyable
    Recruitment(const Recruitment&) = delete;
    Recruitment& operator=(const Recruitment&) = delete;

    // =========================================================================
    // Configuration
    // =========================================================================

    /** @brief Get configuration */
    [[nodiscard]] const RecruitmentConfig& GetConfig() const noexcept { return m_config; }

    /** @brief Set configuration */
    void SetConfig(const RecruitmentConfig& config) { m_config = config; }

    /** @brief Set base position (player's settlement center) */
    void SetBasePosition(const glm::vec3& position) { m_basePosition = position; }

    // =========================================================================
    // Core Update
    // =========================================================================

    /**
     * @brief Update recruitment system
     * @param deltaTime Time since last update
     * @param entityManager Entity manager for NPC access
     * @param population Population manager
     * @param player Player entity for position/interaction
     */
    void Update(float deltaTime, EntityManager& entityManager, Population& population,
                Player* player);

    // =========================================================================
    // Discovery
    // =========================================================================

    /**
     * @brief Manually discover an NPC as a potential recruit
     * @param position World position where survivor was found
     * @param entityManager Entity manager to create NPC
     * @return Entity ID of discovered survivor, or INVALID_ID on failure
     */
    Entity::EntityId DiscoverSurvivor(const glm::vec3& position, EntityManager& entityManager);

    /**
     * @brief Check if an NPC has been discovered
     * @param npcId Entity ID of NPC
     */
    [[nodiscard]] bool IsDiscovered(Entity::EntityId npcId) const;

    /**
     * @brief Get all discovered potential recruits
     */
    [[nodiscard]] std::vector<RecruitInfo> GetDiscoveredRecruits() const;

    /**
     * @brief Get recruitin info for a specific NPC
     * @param npcId Entity ID of NPC
     * @return Recruit info or nullptr if not a potential recruit
     */
    [[nodiscard]] const RecruitInfo* GetRecruitInfo(Entity::EntityId npcId) const;

    // =========================================================================
    // Recruitment
    // =========================================================================

    /**
     * @brief Attempt to recruit an NPC
     * @param npcId Entity ID of NPC to recruit
     * @param entityManager Entity manager for NPC access
     * @param population Population manager to add worker
     * @return true if recruitment successful
     */
    bool RecruitWorker(Entity::EntityId npcId, EntityManager& entityManager, Population& population);

    /**
     * @brief Start recruitment interaction with NPC
     * @param npcId Entity ID of NPC
     * @param player Player entity (for range check)
     * @return true if interaction started
     */
    bool StartRecruitmentInteraction(Entity::EntityId npcId, Player* player);

    /**
     * @brief Update ongoing recruitment interaction
     * @param npcId Entity ID of NPC being recruited
     * @param deltaTime Time since last update
     * @param player Player entity (for range check)
     * @return true if recruitment is complete
     */
    bool UpdateRecruitmentInteraction(Entity::EntityId npcId, float deltaTime, Player* player);

    /**
     * @brief Cancel recruitment interaction
     * @param npcId Entity ID of NPC
     */
    void CancelRecruitmentInteraction(Entity::EntityId npcId);

    /**
     * @brief Check if player can recruit a specific NPC
     * @param npcId Entity ID of NPC
     * @param population Population manager for housing check
     * @param outReason If provided, filled with reason if cannot recruit
     * @return true if can recruit
     */
    [[nodiscard]] bool CanRecruit(Entity::EntityId npcId, const Population& population,
                                   std::string* outReason = nullptr) const;

    // =========================================================================
    // Refugee Waves
    // =========================================================================

    /**
     * @brief Trigger a refugee wave (survivors coming to base)
     * @param count Number of refugees
     * @param entityManager Entity manager to create NPCs
     */
    void TriggerRefugeeWave(int count, EntityManager& entityManager);

    /**
     * @brief Get refugees currently heading to base
     */
    [[nodiscard]] std::vector<Entity::EntityId> GetIncomingRefugees() const;

    // =========================================================================
    // Spawning
    // =========================================================================

    /**
     * @brief Spawn a potential recruit at a random location
     * @param entityManager Entity manager to create NPC
     * @param playerPosition Player's current position
     * @return Entity ID of spawned NPC, or INVALID_ID
     */
    Entity::EntityId SpawnPotentialRecruit(EntityManager& entityManager,
                                           const glm::vec3& playerPosition);

    /**
     * @brief Set spawn area bounds
     * @param min Minimum world coordinate
     * @param max Maximum world coordinate
     */
    void SetSpawnBounds(const glm::vec2& min, const glm::vec2& max);

    // =========================================================================
    // Callbacks
    // =========================================================================

    using DiscoveryCallback = std::function<void(const RecruitInfo&)>;
    using RecruitmentCallback = std::function<void(Worker&)>;
    using RefugeeWaveCallback = std::function<void(int count)>;

    void SetOnSurvivorDiscovered(DiscoveryCallback cb) { m_onDiscovery = std::move(cb); }
    void SetOnWorkerRecruited(RecruitmentCallback cb) { m_onRecruitment = std::move(cb); }
    void SetOnRefugeeWave(RefugeeWaveCallback cb) { m_onRefugeeWave = std::move(cb); }

    // =========================================================================
    // Statistics
    // =========================================================================

    /** @brief Get total survivors discovered */
    [[nodiscard]] int GetTotalDiscovered() const noexcept { return m_totalDiscovered; }

    /** @brief Get total workers recruited */
    [[nodiscard]] int GetTotalRecruited() const noexcept { return m_totalRecruited; }

    /** @brief Get count of unrecruited survivors */
    [[nodiscard]] int GetUnrecruitedCount() const noexcept;

private:
    // Internal helpers
    RecruitType DetermineRecruitType() const;
    void GenerateRecruitInfo(RecruitInfo& info, NPC* npc);
    void GenerateSpecializedSkills(Worker* worker, RecruitType type, WorkerJob specialization);
    void UpdateDiscovery(EntityManager& entityManager, Player* player);
    void UpdateSpawning(float deltaTime, EntityManager& entityManager, Player* player);
    void UpdateRefugees(float deltaTime, EntityManager& entityManager);
    void CleanupRemovedNPCs(EntityManager& entityManager);

    glm::vec3 GetRandomSpawnPosition(const glm::vec3& playerPosition) const;
    std::string GeneratePersonalityHint(const WorkerPersonality& personality) const;
    std::string GetBestSkillName(const WorkerSkills& skills, WorkerJob& outJob) const;

    // Configuration
    RecruitmentConfig m_config;
    glm::vec3 m_basePosition{0.0f};
    glm::vec2 m_spawnMin{-100.0f, -100.0f};
    glm::vec2 m_spawnMax{100.0f, 100.0f};

    // Tracked NPCs
    std::unordered_map<Entity::EntityId, RecruitInfo> m_potentialRecruits;
    std::unordered_set<Entity::EntityId> m_discoveredNPCs;
    std::vector<Entity::EntityId> m_incomingRefugees;

    // Active recruitment interaction
    Entity::EntityId m_activeRecruitmentTarget = Entity::INVALID_ID;

    // Timers
    float m_spawnTimer = 0.0f;
    float m_refugeeWaveTimer = 0.0f;

    // Statistics
    int m_totalDiscovered = 0;
    int m_totalRecruited = 0;

    // Callbacks
    DiscoveryCallback m_onDiscovery;
    RecruitmentCallback m_onRecruitment;
    RefugeeWaveCallback m_onRefugeeWave;
};

} // namespace Vehement
