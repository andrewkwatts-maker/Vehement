#pragma once

#include "Entity.hpp"
#include <engine/pathfinding/Pathfinder.hpp>
#include <vector>

namespace Nova {
    class Graph;
}

namespace Vehement {

class EntityManager;

/**
 * @brief Zombie AI states
 */
enum class ZombieState : uint8_t {
    Idle,       // Standing still, no target
    Wander,     // Randomly moving around
    Chase,      // Pursuing a target
    Attack,     // Attacking a target in range
    Infecting   // Infecting an NPC (brief state)
};

/**
 * @brief Convert zombie state to string for debugging
 */
inline const char* ZombieStateToString(ZombieState state) {
    switch (state) {
        case ZombieState::Idle:      return "Idle";
        case ZombieState::Wander:    return "Wander";
        case ZombieState::Chase:     return "Chase";
        case ZombieState::Attack:    return "Attack";
        case ZombieState::Infecting: return "Infecting";
        default:                     return "Unknown";
    }
}

/**
 * @brief Zombie type variants with different stats
 */
enum class ZombieType : uint8_t {
    Standard,   // Normal zombie
    Slow,       // Slower but tougher
    Fast,       // Fast but weak
    Tank        // Very slow, very tough, high damage
};

/**
 * @brief Get display name for zombie type
 */
inline const char* ZombieTypeToString(ZombieType type) {
    switch (type) {
        case ZombieType::Standard: return "Zombie";
        case ZombieType::Slow:     return "Shambler";
        case ZombieType::Fast:     return "Runner";
        case ZombieType::Tank:     return "Brute";
        default:                   return "Zombie";
    }
}

/**
 * @brief Configuration for zombie types
 */
struct ZombieConfig {
    float moveSpeed = 3.0f;
    float health = 50.0f;
    float damage = 10.0f;
    float attackRange = 1.5f;
    float attackCooldown = 1.0f;
    float detectionRadius = 15.0f;
    float infectionChance = 0.3f;     // Chance to infect NPC on hit
    int coinDropMin = 1;
    int coinDropMax = 5;

    /** @brief Get config for a zombie type */
    static ZombieConfig GetConfig(ZombieType type);
};

/**
 * @brief Zombie entity for Vehement2
 *
 * Zombies use AI states to hunt down players and NPCs. They can
 * pathfind to targets, attack when in range, and infect NPCs.
 */
class Zombie : public Entity {
public:
    static constexpr float WANDER_RADIUS = 10.0f;
    static constexpr float WANDER_WAIT_TIME = 2.0f;
    static constexpr float PATH_UPDATE_INTERVAL = 0.5f;
    static constexpr float LOSE_TARGET_DISTANCE = 25.0f;

    /**
     * @brief Construct a standard zombie
     */
    Zombie();

    /**
     * @brief Construct a zombie of specific type
     * @param type The zombie variant type
     */
    explicit Zombie(ZombieType type);

    ~Zombie() override = default;

    // =========================================================================
    // Core Update/Render
    // =========================================================================

    /**
     * @brief Update zombie AI and movement
     * @param deltaTime Time since last frame
     */
    void Update(float deltaTime) override;

    /**
     * @brief Render the zombie
     */
    void Render(Nova::Renderer& renderer) override;

    /**
     * @brief Main AI update with access to entity manager
     * @param deltaTime Time since last frame
     * @param entityManager For finding targets
     * @param navGraph Navigation graph for pathfinding
     */
    void UpdateAI(float deltaTime, EntityManager& entityManager, Nova::Graph* navGraph);

    // =========================================================================
    // AI State
    // =========================================================================

    /** @brief Get current AI state */
    [[nodiscard]] ZombieState GetState() const noexcept { return m_state; }

    /** @brief Force set AI state (for debugging) */
    void SetState(ZombieState state) noexcept { m_state = state; }

    /** @brief Get zombie type */
    [[nodiscard]] ZombieType GetZombieType() const noexcept { return m_zombieType; }

    /** @brief Get current target entity ID */
    [[nodiscard]] Entity::EntityId GetTarget() const noexcept { return m_targetId; }

    /** @brief Set target entity */
    void SetTarget(Entity::EntityId targetId) noexcept { m_targetId = targetId; }

    /** @brief Clear current target */
    void ClearTarget() noexcept { m_targetId = Entity::INVALID_ID; }

    // =========================================================================
    // Combat
    // =========================================================================

    /** @brief Get attack damage */
    [[nodiscard]] float GetDamage() const noexcept { return m_config.damage; }

    /** @brief Get attack range */
    [[nodiscard]] float GetAttackRange() const noexcept { return m_config.attackRange; }

    /** @brief Get detection radius */
    [[nodiscard]] float GetDetectionRadius() const noexcept { return m_config.detectionRadius; }

    /** @brief Get infection chance (0-1) */
    [[nodiscard]] float GetInfectionChance() const noexcept { return m_config.infectionChance; }

    /** @brief Check if zombie can attack (cooldown ready) */
    [[nodiscard]] bool CanAttack() const noexcept { return m_attackCooldownTimer <= 0.0f; }

    /**
     * @brief Perform attack on target
     * @param target Entity to attack
     * @return Damage dealt
     */
    float Attack(Entity& target);

    // =========================================================================
    // Health/Death
    // =========================================================================

    /**
     * @brief Handle zombie death
     */
    void Die() override;

    /** @brief Get coins dropped on death */
    [[nodiscard]] int GetCoinDrop() const;

    // =========================================================================
    // Pathfinding
    // =========================================================================

    /** @brief Check if zombie has a valid path */
    [[nodiscard]] bool HasPath() const noexcept { return !m_currentPath.positions.empty(); }

    /** @brief Clear current path */
    void ClearPath();

    /**
     * @brief Request a path to position
     * @param target Target position
     * @param navGraph Navigation graph
     * @return true if path found
     */
    bool RequestPath(const glm::vec3& target, Nova::Graph& navGraph);

    // =========================================================================
    // Spawning
    // =========================================================================

    /** @brief Get home position (spawn point) */
    [[nodiscard]] const glm::vec3& GetHomePosition() const noexcept { return m_homePosition; }

    /** @brief Set home position */
    void SetHomePosition(const glm::vec3& pos) noexcept { m_homePosition = pos; }

    /** @brief Apply configuration for a zombie type */
    void ApplyConfig(ZombieType type);

private:
    // AI state machine methods
    void UpdateIdle(float deltaTime, EntityManager& entityManager);
    void UpdateWander(float deltaTime, EntityManager& entityManager, Nova::Graph* navGraph);
    void UpdateChase(float deltaTime, EntityManager& entityManager, Nova::Graph* navGraph);
    void UpdateAttack(float deltaTime, EntityManager& entityManager);
    void UpdateInfecting(float deltaTime);

    /**
     * @brief Find nearest valid target (player or NPC)
     * @param entityManager Entity manager for queries
     * @return Entity ID of target, or INVALID_ID if none found
     */
    Entity::EntityId FindTarget(EntityManager& entityManager);

    /**
     * @brief Check if target is still valid
     */
    bool ValidateTarget(EntityManager& entityManager);

    /**
     * @brief Move along current path
     * @param deltaTime Time since last frame
     */
    void FollowPath(float deltaTime);

    /**
     * @brief Move directly toward a position
     * @param target Target position
     * @param deltaTime Time since last frame
     */
    void MoveToward(const glm::vec3& target, float deltaTime);

    // State
    ZombieState m_state = ZombieState::Idle;
    ZombieType m_zombieType = ZombieType::Standard;
    ZombieConfig m_config;

    // Target tracking
    Entity::EntityId m_targetId = Entity::INVALID_ID;
    float m_lostTargetTimer = 0.0f;

    // Combat
    float m_attackCooldownTimer = 0.0f;
    float m_infectingTimer = 0.0f;

    // Pathfinding
    Nova::PathResult m_currentPath;
    size_t m_pathIndex = 0;
    float m_pathUpdateTimer = 0.0f;

    // Wandering
    glm::vec3 m_wanderTarget{0.0f};
    float m_wanderWaitTimer = 0.0f;
    glm::vec3 m_homePosition{0.0f};
};

} // namespace Vehement
