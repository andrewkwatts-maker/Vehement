#pragma once

#include "Entity.hpp"
#include <engine/pathfinding/Pathfinder.hpp>
#include <vector>
#include <functional>

namespace Nova {
    class Graph;
}

namespace Vehement {

class EntityManager;

/**
 * @brief NPC AI states
 */
enum class NPCState : uint8_t {
    Idle,       // Standing still
    Wander,     // Walking between waypoints / daily routine
    Flee,       // Running away from zombies
    Infected,   // Infected but not yet turning
    Turning     // Transforming into zombie
};

/**
 * @brief Convert NPC state to string for debugging
 */
inline const char* NPCStateToString(NPCState state) {
    switch (state) {
        case NPCState::Idle:     return "Idle";
        case NPCState::Wander:   return "Wander";
        case NPCState::Flee:     return "Flee";
        case NPCState::Infected: return "Infected";
        case NPCState::Turning:  return "Turning";
        default:                 return "Unknown";
    }
}

/**
 * @brief Waypoint for NPC daily routines
 */
struct NPCWaypoint {
    glm::vec3 position{0.0f};
    float waitTime = 0.0f;      // Time to wait at this waypoint
    std::string tag;             // Optional tag (e.g., "home", "work", "shop")

    NPCWaypoint() = default;
    NPCWaypoint(const glm::vec3& pos, float wait = 0.0f, const std::string& t = "")
        : position(pos), waitTime(wait), tag(t) {}
};

/**
 * @brief Daily routine schedule for NPC
 */
struct NPCRoutine {
    std::vector<NPCWaypoint> waypoints;
    bool loop = true;           // Loop through waypoints
    size_t currentIndex = 0;    // Current waypoint index

    /** @brief Add a waypoint to the routine */
    void AddWaypoint(const glm::vec3& pos, float waitTime = 2.0f, const std::string& tag = "") {
        waypoints.emplace_back(pos, waitTime, tag);
    }

    /** @brief Get current waypoint */
    [[nodiscard]] const NPCWaypoint* GetCurrentWaypoint() const {
        if (waypoints.empty()) return nullptr;
        return &waypoints[currentIndex];
    }

    /** @brief Advance to next waypoint */
    void NextWaypoint() {
        if (waypoints.empty()) return;
        currentIndex++;
        if (currentIndex >= waypoints.size()) {
            if (loop) {
                currentIndex = 0;
            } else {
                currentIndex = waypoints.size() - 1;
            }
        }
    }

    /** @brief Reset to first waypoint */
    void Reset() { currentIndex = 0; }

    /** @brief Check if routine is complete (for non-looping) */
    [[nodiscard]] bool IsComplete() const {
        return !loop && currentIndex >= waypoints.size() - 1;
    }
};

/**
 * @brief NPC (civilian) entity for Vehement2
 *
 * NPCs follow daily routines, flee from zombies, and can become
 * infected and eventually turn into zombies.
 */
class NPC : public Entity {
public:
    static constexpr float DEFAULT_MOVE_SPEED = 4.0f;
    static constexpr float FLEE_SPEED_MULTIPLIER = 1.5f;
    static constexpr float DETECTION_RADIUS = 12.0f;
    static constexpr float SAFE_DISTANCE = 20.0f;
    static constexpr float DEFAULT_INFECTION_TIME = 30.0f;
    static constexpr float PATH_UPDATE_INTERVAL = 0.5f;

    /**
     * @brief Construct an NPC
     */
    NPC();

    /**
     * @brief Construct an NPC with specific appearance
     * @param appearanceIndex Person texture index (1-9)
     */
    explicit NPC(int appearanceIndex);

    ~NPC() override = default;

    // =========================================================================
    // Core Update/Render
    // =========================================================================

    /**
     * @brief Update NPC state and AI
     * @param deltaTime Time since last frame
     */
    void Update(float deltaTime) override;

    /**
     * @brief Render the NPC
     */
    void Render(Nova::Renderer& renderer) override;

    /**
     * @brief Main AI update with entity manager access
     * @param deltaTime Time since last frame
     * @param entityManager For finding threats
     * @param navGraph Navigation graph for pathfinding
     */
    void UpdateAI(float deltaTime, EntityManager& entityManager, Nova::Graph* navGraph);

    // =========================================================================
    // AI State
    // =========================================================================

    /** @brief Get current AI state */
    [[nodiscard]] NPCState GetNPCState() const noexcept { return m_state; }

    /** @brief Force set AI state */
    void SetState(NPCState state) noexcept { m_state = state; }

    // =========================================================================
    // Infection System
    // =========================================================================

    /** @brief Check if NPC is infected */
    [[nodiscard]] bool IsInfected() const noexcept {
        return m_state == NPCState::Infected || m_state == NPCState::Turning;
    }

    /** @brief Check if NPC is currently turning into zombie */
    [[nodiscard]] bool IsTurning() const noexcept { return m_state == NPCState::Turning; }

    /**
     * @brief Infect the NPC (starts infection timer)
     */
    void Infect();

    /**
     * @brief Cure the NPC (if infected but not turning)
     * @return true if cured successfully
     */
    bool Cure();

    /** @brief Get infection timer (time until turning) */
    [[nodiscard]] float GetInfectionTimer() const noexcept { return m_infectionTimer; }

    /** @brief Get infection progress (0-1) */
    [[nodiscard]] float GetInfectionProgress() const noexcept {
        if (m_infectionDuration <= 0.0f) return 0.0f;
        return 1.0f - (m_infectionTimer / m_infectionDuration);
    }

    /** @brief Set infection duration (time before turning) */
    void SetInfectionDuration(float duration) noexcept { m_infectionDuration = duration; }

    /** @brief Get infection duration */
    [[nodiscard]] float GetInfectionDuration() const noexcept { return m_infectionDuration; }

    /**
     * @brief Callback when NPC is about to turn into zombie
     * Set this to handle the conversion in game logic
     */
    using TurnCallback = std::function<void(NPC&)>;
    void SetTurnCallback(TurnCallback callback) { m_onTurn = std::move(callback); }

    // =========================================================================
    // Daily Routine
    // =========================================================================

    /** @brief Get routine (mutable) */
    NPCRoutine& GetRoutine() noexcept { return m_routine; }

    /** @brief Get routine (const) */
    [[nodiscard]] const NPCRoutine& GetRoutine() const noexcept { return m_routine; }

    /** @brief Set a new routine */
    void SetRoutine(const NPCRoutine& routine) { m_routine = routine; }

    /** @brief Check if NPC has a routine */
    [[nodiscard]] bool HasRoutine() const noexcept { return !m_routine.waypoints.empty(); }

    // =========================================================================
    // Fleeing
    // =========================================================================

    /** @brief Get current threat entity ID */
    [[nodiscard]] Entity::EntityId GetThreat() const noexcept { return m_threatId; }

    /** @brief Clear threat (stop fleeing) */
    void ClearThreat() noexcept { m_threatId = Entity::INVALID_ID; }

    // =========================================================================
    // Appearance
    // =========================================================================

    /** @brief Get appearance index (1-9) */
    [[nodiscard]] int GetAppearanceIndex() const noexcept { return m_appearanceIndex; }

    /** @brief Set appearance index */
    void SetAppearanceIndex(int index);

    /**
     * @brief Get texture path for appearance
     * @param index Appearance index (1-9)
     */
    static std::string GetAppearanceTexturePath(int index);

    // =========================================================================
    // Health/Combat
    // =========================================================================

    /**
     * @brief Handle NPC death
     */
    void Die() override;

    /**
     * @brief Take damage - may trigger fleeing
     */
    float TakeDamage(float amount, EntityId source = INVALID_ID) override;

    // =========================================================================
    // Pathfinding
    // =========================================================================

    /** @brief Check if NPC has a valid path */
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

private:
    // AI state machine methods
    void UpdateIdle(float deltaTime, EntityManager& entityManager);
    void UpdateWander(float deltaTime, EntityManager& entityManager, Nova::Graph* navGraph);
    void UpdateFlee(float deltaTime, EntityManager& entityManager, Nova::Graph* navGraph);
    void UpdateInfected(float deltaTime);
    void UpdateTurning(float deltaTime);

    /**
     * @brief Check for nearby zombies (threats)
     * @param entityManager Entity manager for queries
     * @return Entity ID of nearest threat, or INVALID_ID
     */
    Entity::EntityId DetectThreat(EntityManager& entityManager);

    /**
     * @brief Calculate flee direction (away from threats)
     * @param entityManager For finding all nearby threats
     */
    glm::vec3 CalculateFleeDirection(EntityManager& entityManager);

    /**
     * @brief Find a safe position to flee to
     */
    glm::vec3 FindSafePosition(EntityManager& entityManager);

    /**
     * @brief Follow current path
     * @param deltaTime Time since last frame
     */
    void FollowPath(float deltaTime);

    /**
     * @brief Move toward a position
     * @param target Target position
     * @param deltaTime Time since last frame
     */
    void MoveToward(const glm::vec3& target, float deltaTime);

    // State
    NPCState m_state = NPCState::Idle;
    NPCState m_preInfectionState = NPCState::Idle;  // State before getting infected

    // Routine
    NPCRoutine m_routine;
    float m_waypointWaitTimer = 0.0f;

    // Infection
    float m_infectionTimer = 0.0f;
    float m_infectionDuration = DEFAULT_INFECTION_TIME;
    TurnCallback m_onTurn;

    // Threat/Fleeing
    Entity::EntityId m_threatId = Entity::INVALID_ID;
    glm::vec3 m_fleeTarget{0.0f};
    float m_fleeReassessTimer = 0.0f;

    // Pathfinding
    Nova::PathResult m_currentPath;
    size_t m_pathIndex = 0;
    float m_pathUpdateTimer = 0.0f;

    // Appearance
    int m_appearanceIndex = 1;
};

} // namespace Vehement
