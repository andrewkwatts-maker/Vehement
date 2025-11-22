#pragma once

#include "Building.hpp"
#include "Construction.hpp"
#include "../world/TileMap.hpp"
#include <vector>
#include <memory>
#include <functional>
#include <unordered_set>
#include <glm/glm.hpp>

namespace Nova {
    class Renderer;
    class Graph;
}

namespace Vehement {

// Forward declarations
class Entity;
class Zombie;
class World;

// ============================================================================
// Defense Types
// ============================================================================

/**
 * @brief Types of defensive structures
 */
enum class DefenseType : uint8_t {
    Passive,        // Walls, gates - block movement only
    Ranged,         // Watch towers - fire arrows
    Area,           // Fortress - area damage
    Support         // Vision only, no damage
};

/**
 * @brief Get defense type for a building
 */
inline DefenseType GetDefenseType(BuildingType type) {
    switch (type) {
        case BuildingType::Wall:
        case BuildingType::Gate:
            return DefenseType::Passive;

        case BuildingType::WatchTower:
            return DefenseType::Ranged;

        case BuildingType::Fortress:
            return DefenseType::Area;

        default:
            return DefenseType::Support;
    }
}

/**
 * @brief Check if building type is a defensive structure
 */
inline bool IsDefensiveBuilding(BuildingType type) {
    return type == BuildingType::WatchTower ||
           type == BuildingType::Wall ||
           type == BuildingType::Gate ||
           type == BuildingType::Fortress;
}

// ============================================================================
// Defensive Structure Stats
// ============================================================================

/**
 * @brief Combat stats for defensive structures
 */
struct DefenseStats {
    float damage = 0.0f;            // Damage per attack
    float attackRange = 0.0f;       // Attack range in world units
    float attackCooldown = 1.0f;    // Time between attacks
    float visionRange = 10.0f;      // Fog of war reveal range
    int maxTargets = 1;             // Number of targets for area attacks
    float splashRadius = 0.0f;      // Splash damage radius (0 = single target)

    // Projectile properties (for ranged)
    float projectileSpeed = 20.0f;
    std::string projectileTexture;
};

/**
 * @brief Get defense stats for a building type and level
 */
inline DefenseStats GetDefenseStats(BuildingType type, int level = 1) {
    DefenseStats stats;

    switch (type) {
        case BuildingType::WatchTower:
            stats.damage = 15.0f + (level - 1) * 5.0f;
            stats.attackRange = 12.0f + (level - 1) * 2.0f;
            stats.attackCooldown = 1.5f - (level - 1) * 0.2f;
            stats.visionRange = 15.0f + (level - 1) * 3.0f;
            stats.maxTargets = 1;
            stats.projectileSpeed = 25.0f;
            stats.projectileTexture = "Vehement2/images/Weapons/AK47TopFiring.png";
            break;

        case BuildingType::Wall:
            stats.damage = 0.0f;  // Walls don't attack
            stats.attackRange = 0.0f;
            stats.visionRange = 2.0f;
            break;

        case BuildingType::Gate:
            stats.damage = 0.0f;  // Gates don't attack
            stats.attackRange = 0.0f;
            stats.visionRange = 3.0f;
            break;

        case BuildingType::Fortress:
            stats.damage = 30.0f + (level - 1) * 10.0f;
            stats.attackRange = 15.0f + (level - 1) * 3.0f;
            stats.attackCooldown = 1.0f - (level - 1) * 0.15f;
            stats.visionRange = 20.0f + (level - 1) * 5.0f;
            stats.maxTargets = 3 + (level - 1);  // Multi-target
            stats.splashRadius = 2.0f + (level - 1) * 0.5f;
            stats.projectileSpeed = 20.0f;
            stats.projectileTexture = "Vehement2/images/Weapons/GrenadeRed.png";
            break;

        default:
            stats.visionRange = 5.0f;
            break;
    }

    return stats;
}

// ============================================================================
// Target Tracking
// ============================================================================

/**
 * @brief Target priority for defensive AI
 */
enum class TargetPriority : uint8_t {
    Nearest,            // Attack closest enemy
    Weakest,            // Attack lowest HP enemy
    Strongest,          // Attack highest HP enemy
    AttackingBuilding,  // Prioritize enemies attacking buildings
    AttackingWorker     // Prioritize enemies attacking workers
};

/**
 * @brief Information about a potential target
 */
struct TargetInfo {
    Entity* entity = nullptr;
    float distance = 0.0f;
    float health = 0.0f;
    float maxHealth = 0.0f;
    bool isAttackingBuilding = false;
    bool isAttackingWorker = false;
    float threat = 0.0f;  // Calculated threat level

    bool operator<(const TargetInfo& other) const {
        return threat > other.threat;  // Higher threat = higher priority
    }
};

// ============================================================================
// Wall Segment
// ============================================================================

/**
 * @brief Represents a connected wall segment for pathfinding optimization
 */
struct WallSegment {
    std::vector<glm::ivec2> tiles;
    glm::vec2 start;
    glm::vec2 end;
    bool isHorizontal;
    bool hasGate;
    int gateIndex = -1;

    /**
     * @brief Get length of wall segment
     */
    [[nodiscard]] float GetLength() const {
        return glm::length(end - start);
    }

    /**
     * @brief Check if a point is blocked by this wall
     */
    [[nodiscard]] bool BlocksPoint(const glm::vec2& point, float tolerance = 0.5f) const;
};

// ============================================================================
// Defense Manager
// ============================================================================

/**
 * @brief Manages all defensive structures and their behavior
 */
class DefenseManager {
public:
    DefenseManager();
    ~DefenseManager() = default;

    /**
     * @brief Initialize with references
     */
    void Initialize(World* world, Construction* construction, TileMap* tileMap);

    /**
     * @brief Update all defenses
     */
    void Update(float deltaTime);

    /**
     * @brief Render defense effects (projectiles, targeting lines, etc.)
     */
    void Render(Nova::Renderer& renderer);

    // =========================================================================
    // Targeting
    // =========================================================================

    /**
     * @brief Set global target priority
     */
    void SetTargetPriority(TargetPriority priority) { m_targetPriority = priority; }

    /**
     * @brief Get current target priority
     */
    [[nodiscard]] TargetPriority GetTargetPriority() const { return m_targetPriority; }

    /**
     * @brief Set target priority for a specific building
     */
    void SetBuildingTargetPriority(Building* building, TargetPriority priority);

    /**
     * @brief Find best target for a defensive building
     */
    Entity* FindBestTarget(Building* building) const;

    /**
     * @brief Get all enemies in range of a building
     */
    std::vector<TargetInfo> GetEnemiesInRange(Building* building) const;

    /**
     * @brief Get all enemies in range of a position
     */
    std::vector<Entity*> GetEnemiesInRange(const glm::vec3& position, float range) const;

    // =========================================================================
    // Gate Control
    // =========================================================================

    /**
     * @brief Open a gate
     */
    void OpenGate(Building* gate);

    /**
     * @brief Close a gate
     */
    void CloseGate(Building* gate);

    /**
     * @brief Toggle gate state
     */
    void ToggleGate(Building* gate);

    /**
     * @brief Open all gates
     */
    void OpenAllGates();

    /**
     * @brief Close all gates
     */
    void CloseAllGates();

    /**
     * @brief Set gate auto-close (closes when enemies nearby)
     */
    void SetGateAutoClose(Building* gate, bool autoClose);

    /**
     * @brief Get all gates
     */
    std::vector<Building*> GetAllGates() const;

    // =========================================================================
    // Wall Management
    // =========================================================================

    /**
     * @brief Rebuild wall segment data
     */
    void RebuildWallSegments();

    /**
     * @brief Get all wall segments
     */
    [[nodiscard]] const std::vector<WallSegment>& GetWallSegments() const {
        return m_wallSegments;
    }

    /**
     * @brief Check if position is blocked by walls
     */
    [[nodiscard]] bool IsBlockedByWall(const glm::vec3& position) const;

    /**
     * @brief Check if line of movement is blocked by walls
     */
    [[nodiscard]] bool IsPathBlockedByWall(const glm::vec3& from, const glm::vec3& to) const;

    /**
     * @brief Get wall health at position (for zombie attacks)
     */
    [[nodiscard]] float GetWallHealthAt(int x, int y) const;

    // =========================================================================
    // Vision System
    // =========================================================================

    /**
     * @brief Get all positions revealed by defensive structures
     */
    std::vector<glm::ivec2> GetRevealedTiles() const;

    /**
     * @brief Check if a tile is revealed by defenses
     */
    [[nodiscard]] bool IsTileRevealed(int x, int y) const;

    /**
     * @brief Get total vision coverage percentage
     */
    [[nodiscard]] float GetVisionCoverage() const;

    // =========================================================================
    // Hero Revival (Fortress)
    // =========================================================================

    /**
     * @brief Get fortress for hero revival (if any operational)
     */
    Building* GetHeroRevivalPoint() const;

    /**
     * @brief Get revival position
     */
    glm::vec3 GetRevivalPosition() const;

    /**
     * @brief Check if hero revival is available
     */
    [[nodiscard]] bool CanReviveHero() const;

    // =========================================================================
    // Guard Assignment
    // =========================================================================

    /**
     * @brief Assign a worker as guard to defensive building
     */
    bool AssignGuard(Worker* worker, Building* defense);

    /**
     * @brief Remove guard from defensive building
     */
    bool RemoveGuard(Worker* worker, Building* defense);

    /**
     * @brief Get all guards at a building
     */
    std::vector<Worker*> GetGuards(Building* defense) const;

    /**
     * @brief Get guard bonus damage (guards increase defense damage)
     */
    [[nodiscard]] float GetGuardBonusDamage(Building* defense) const;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get total defense score
     */
    [[nodiscard]] float GetDefenseScore() const;

    /**
     * @brief Get total kills by defenses
     */
    [[nodiscard]] int GetTotalKills() const { return m_totalKills; }

    /**
     * @brief Get damage dealt by defenses
     */
    [[nodiscard]] float GetTotalDamageDealt() const { return m_totalDamageDealt; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    using AttackCallback = std::function<void(Building* attacker, Entity* target, float damage)>;
    void SetOnAttack(AttackCallback callback) { m_onAttack = std::move(callback); }

    using KillCallback = std::function<void(Building* attacker, Entity* killed)>;
    void SetOnKill(KillCallback callback) { m_onKill = std::move(callback); }

    using BreachCallback = std::function<void(Building* wall, Entity* breacher)>;
    void SetOnWallBreach(BreachCallback callback) { m_onWallBreach = std::move(callback); }

private:
    void UpdateDefensiveBuilding(Building* building, float deltaTime);
    void PerformAttack(Building* building, Entity* target);
    void UpdateGateAutoClose(Building* gate);
    float CalculateThreat(const TargetInfo& target) const;

    World* m_world = nullptr;
    Construction* m_construction = nullptr;
    TileMap* m_tileMap = nullptr;

    // Targeting
    TargetPriority m_targetPriority = TargetPriority::Nearest;
    std::unordered_map<Building*, TargetPriority> m_buildingPriorities;

    // Attack tracking
    struct AttackState {
        float cooldownTimer = 0.0f;
        Entity* currentTarget = nullptr;
        std::vector<Entity*> recentTargets;
    };
    std::unordered_map<Building*, AttackState> m_attackStates;

    // Gate state
    std::unordered_set<Building*> m_autoCloseGates;

    // Wall segments
    std::vector<WallSegment> m_wallSegments;
    bool m_wallSegmentsDirty = true;

    // Guard tracking
    std::unordered_map<Building*, std::vector<Worker*>> m_guards;

    // Statistics
    int m_totalKills = 0;
    float m_totalDamageDealt = 0.0f;

    // Callbacks
    AttackCallback m_onAttack;
    KillCallback m_onKill;
    BreachCallback m_onWallBreach;
};

// ============================================================================
// Projectile for Defensive Structures
// ============================================================================

/**
 * @brief Projectile fired by defensive structures
 */
struct DefenseProjectile {
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 targetPosition{0.0f};
    Entity* target = nullptr;
    Building* source = nullptr;
    float damage = 0.0f;
    float splashRadius = 0.0f;
    float lifetime = 5.0f;
    bool active = true;
    std::string texturePath;

    /**
     * @brief Update projectile position
     */
    void Update(float deltaTime) {
        if (!active) return;

        position += velocity * deltaTime;
        lifetime -= deltaTime;

        if (lifetime <= 0.0f) {
            active = false;
        }
    }

    /**
     * @brief Check if projectile has reached target
     */
    [[nodiscard]] bool HasReachedTarget() const {
        return glm::length(position - targetPosition) < 0.5f;
    }
};

} // namespace Vehement
