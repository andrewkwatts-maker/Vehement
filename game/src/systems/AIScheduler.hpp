#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <queue>
#include <memory>
#include <cstdint>
#include <chrono>

namespace Vehement {
namespace Systems {

using EntityId = uint32_t;
constexpr EntityId INVALID_ENTITY_ID = 0;

/**
 * @brief Level of Detail for AI processing
 */
enum class AILodLevel : uint8_t {
    Full = 0,       // Full AI processing - pathfinding, complex decisions
    Medium = 1,     // Reduced AI - simpler pathfinding, basic decisions
    Low = 2,        // Minimal AI - steering only, no pathfinding
    Dormant = 3     // No AI - frozen in place, only visibility checks
};

/**
 * @brief AI priority for update scheduling
 */
enum class AIPriority : uint8_t {
    Critical = 0,   // Always update every frame (player threats)
    High = 1,       // Update frequently
    Normal = 2,     // Standard update rate
    Low = 3,        // Update infrequently
    Background = 4  // Update only when idle time available
};

/**
 * @brief AI entity registration info
 */
struct AIEntityInfo {
    EntityId entityId = INVALID_ENTITY_ID;
    AIPriority priority = AIPriority::Normal;
    AILodLevel lodLevel = AILodLevel::Full;
    glm::vec3 position{0.0f};
    float distanceToPlayer = 0.0f;

    // Update timing
    uint32_t lastUpdateFrame = 0;
    float timeSinceUpdate = 0.0f;
    float updateInterval = 0.0f;  // Computed based on LOD and priority

    // Group AI
    uint32_t groupId = 0;
    bool isGroupLeader = false;

    // Cached calculations
    bool hasValidPath = false;
    glm::vec3 cachedTarget{0.0f};
    float cachedThreatLevel = 0.0f;
};

/**
 * @brief Group AI information for shared pathfinding
 */
struct AIGroup {
    uint32_t groupId = 0;
    EntityId leaderId = INVALID_ENTITY_ID;
    std::vector<EntityId> members;
    glm::vec3 groupCenter{0.0f};
    glm::vec3 sharedTarget{0.0f};
    bool hasSharedPath = false;
    float lastPathUpdate = 0.0f;
};

/**
 * @brief Statistics for AI scheduling performance
 */
struct AISchedulerStats {
    uint32_t totalEntities = 0;
    uint32_t updatedThisFrame = 0;
    uint32_t skippedDueToLod = 0;
    uint32_t groupsUpdated = 0;

    // Per-LOD counts
    uint32_t fullLodCount = 0;
    uint32_t mediumLodCount = 0;
    uint32_t lowLodCount = 0;
    uint32_t dormantCount = 0;

    // Timing
    float updateTimeMs = 0.0f;
    float averageUpdateTimeMs = 0.0f;
    float peakUpdateTimeMs = 0.0f;

    void Reset() {
        updatedThisFrame = 0;
        skippedDueToLod = 0;
        groupsUpdated = 0;
        updateTimeMs = 0.0f;
    }
};

/**
 * @brief AI update callback type
 *
 * @param entityId Entity being updated
 * @param deltaTime Time since entity's last update
 * @param lodLevel Current LOD level for this entity
 * @return true if entity should remain active
 */
using AIUpdateCallback = std::function<bool(EntityId, float, AILodLevel)>;

/**
 * @brief Group AI update callback type
 *
 * @param group The AI group being updated
 * @param deltaTime Time since last update
 * @return true to continue group processing
 */
using GroupUpdateCallback = std::function<bool(AIGroup&, float)>;

// ============================================================================
// AI Scheduler - Distributes AI Updates Across Frames
// ============================================================================

/**
 * @brief High-performance AI scheduler with LOD support
 *
 * Features:
 * - Spreads AI updates across multiple frames to maintain framerate
 * - Distance-based LOD reduces processing for distant entities
 * - Priority system ensures important AI gets processed first
 * - Group AI for efficient shared pathfinding
 * - Time budget enforcement
 */
class AIScheduler {
public:
    struct Config {
        // Update limits
        uint32_t maxUpdatesPerFrame = 50;
        float timeBudgetMs = 2.0f;        // Max time to spend on AI per frame

        // LOD distances (squared for efficiency)
        float lodFullDistanceSq = 400.0f;      // 20 units
        float lodMediumDistanceSq = 2500.0f;   // 50 units
        float lodLowDistanceSq = 10000.0f;     // 100 units
        // Beyond lodLowDistance: Dormant

        // Update intervals by priority (seconds)
        float criticalUpdateInterval = 0.0f;   // Every frame
        float highUpdateInterval = 0.033f;     // ~30 Hz
        float normalUpdateInterval = 0.1f;     // 10 Hz
        float lowUpdateInterval = 0.25f;       // 4 Hz
        float backgroundUpdateInterval = 1.0f; // 1 Hz

        // LOD multipliers for update intervals
        float mediumLodMultiplier = 2.0f;
        float lowLodMultiplier = 4.0f;
        float dormantLodMultiplier = 10.0f;

        // Group AI settings
        size_t maxGroupSize = 10;
        float groupFormationRadius = 15.0f;
        float groupPathShareRadius = 5.0f;

        // Adaptive settings
        bool enableAdaptiveBudget = true;
        float targetFrameTime = 16.67f;       // 60 FPS target
    };

    explicit AIScheduler(const Config& config = {});
    ~AIScheduler() = default;

    // Non-copyable, movable
    AIScheduler(const AIScheduler&) = delete;
    AIScheduler& operator=(const AIScheduler&) = delete;
    AIScheduler(AIScheduler&&) noexcept = default;
    AIScheduler& operator=(AIScheduler&&) noexcept = default;

    // =========================================================================
    // Entity Registration
    // =========================================================================

    /**
     * @brief Register an entity for AI scheduling
     * @param entityId Entity ID
     * @param position Initial position
     * @param priority AI priority
     */
    void RegisterEntity(EntityId entityId, const glm::vec3& position,
                        AIPriority priority = AIPriority::Normal);

    /**
     * @brief Unregister an entity from AI scheduling
     * @param entityId Entity ID
     */
    void UnregisterEntity(EntityId entityId);

    /**
     * @brief Check if entity is registered
     */
    [[nodiscard]] bool IsRegistered(EntityId entityId) const;

    /**
     * @brief Update entity position (for LOD calculations)
     * @param entityId Entity ID
     * @param position New position
     */
    void UpdateEntityPosition(EntityId entityId, const glm::vec3& position);

    /**
     * @brief Set entity priority
     */
    void SetEntityPriority(EntityId entityId, AIPriority priority);

    /**
     * @brief Force entity to update next frame
     */
    void ForceUpdate(EntityId entityId);

    /**
     * @brief Get entity's current LOD level
     */
    [[nodiscard]] AILodLevel GetEntityLod(EntityId entityId) const;

    // =========================================================================
    // Player Reference (for LOD calculations)
    // =========================================================================

    /**
     * @brief Set the player/camera position for LOD calculations
     * @param position Player world position
     */
    void SetPlayerPosition(const glm::vec3& position);

    /**
     * @brief Add additional reference point for LOD (e.g., camera)
     */
    void AddLodReference(const glm::vec3& position);

    /**
     * @brief Clear additional LOD references
     */
    void ClearLodReferences();

    // =========================================================================
    // Group AI
    // =========================================================================

    /**
     * @brief Create an AI group
     * @return Group ID
     */
    uint32_t CreateGroup();

    /**
     * @brief Destroy an AI group
     * @param groupId Group ID
     */
    void DestroyGroup(uint32_t groupId);

    /**
     * @brief Add entity to a group
     */
    void AddToGroup(EntityId entityId, uint32_t groupId);

    /**
     * @brief Remove entity from its group
     */
    void RemoveFromGroup(EntityId entityId);

    /**
     * @brief Get entity's group ID (0 if not in group)
     */
    [[nodiscard]] uint32_t GetEntityGroup(EntityId entityId) const;

    /**
     * @brief Set group's shared target position
     */
    void SetGroupTarget(uint32_t groupId, const glm::vec3& target);

    /**
     * @brief Get group information
     */
    [[nodiscard]] const AIGroup* GetGroup(uint32_t groupId) const;

    /**
     * @brief Auto-form groups based on proximity
     */
    void AutoFormGroups();

    // =========================================================================
    // Update Processing
    // =========================================================================

    /**
     * @brief Process AI updates for this frame
     *
     * @param deltaTime Time since last frame
     * @param updateCallback Callback to process each entity
     * @param groupCallback Optional callback for group updates
     */
    void Update(float deltaTime,
                AIUpdateCallback updateCallback,
                GroupUpdateCallback groupCallback = nullptr);

    /**
     * @brief Get entities scheduled for update this frame
     * Useful for custom update loops
     */
    [[nodiscard]] std::vector<EntityId> GetScheduledEntities() const;

    /**
     * @brief Manually process a specific entity
     * Bypasses scheduling, useful for forced updates
     */
    void ProcessEntity(EntityId entityId, float deltaTime,
                       AIUpdateCallback callback);

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Update configuration
     */
    void SetConfig(const Config& config) { m_config = config; }

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const Config& GetConfig() const { return m_config; }

    /**
     * @brief Set maximum updates per frame
     */
    void SetMaxUpdatesPerFrame(uint32_t max) { m_config.maxUpdatesPerFrame = max; }

    /**
     * @brief Set time budget in milliseconds
     */
    void SetTimeBudget(float ms) { m_config.timeBudgetMs = ms; }

    /**
     * @brief Set LOD distances
     */
    void SetLodDistances(float full, float medium, float low);

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get scheduling statistics
     */
    [[nodiscard]] const AISchedulerStats& GetStats() const { return m_stats; }

    /**
     * @brief Reset accumulated statistics
     */
    void ResetStats();

    /**
     * @brief Get registered entity count
     */
    [[nodiscard]] size_t GetEntityCount() const { return m_entities.size(); }

    /**
     * @brief Get group count
     */
    [[nodiscard]] size_t GetGroupCount() const { return m_groups.size(); }

    // =========================================================================
    // Debug
    // =========================================================================

    /**
     * @brief Get all entities at a specific LOD level
     */
    [[nodiscard]] std::vector<EntityId> GetEntitiesAtLod(AILodLevel lod) const;

    /**
     * @brief Get entity info for debugging
     */
    [[nodiscard]] const AIEntityInfo* GetEntityInfo(EntityId entityId) const;

private:
    // Internal methods
    void UpdateLodLevels();
    void BuildUpdateQueue();
    float ComputeUpdateInterval(const AIEntityInfo& info) const;
    AILodLevel ComputeLodLevel(float distanceSq) const;
    void UpdateGroups(float deltaTime, GroupUpdateCallback callback);
    void AdaptBudget(float frameTime);

    // Priority queue comparator
    struct UpdatePriorityCompare {
        bool operator()(const AIEntityInfo* a, const AIEntityInfo* b) const {
            // Critical entities first
            if (a->priority != b->priority) {
                return static_cast<uint8_t>(a->priority) > static_cast<uint8_t>(b->priority);
            }
            // Then by time since update
            return a->timeSinceUpdate < b->timeSinceUpdate;
        }
    };

    Config m_config;
    AISchedulerStats m_stats;
    uint32_t m_currentFrame = 0;
    float m_accumulatedTime = 0.0f;

    // Entity storage
    std::unordered_map<EntityId, AIEntityInfo> m_entities;

    // Group storage
    std::unordered_map<uint32_t, AIGroup> m_groups;
    uint32_t m_nextGroupId = 1;

    // Update scheduling
    std::vector<AIEntityInfo*> m_updateQueue;
    std::unordered_set<EntityId> m_forceUpdateSet;

    // LOD references (player positions, cameras, etc.)
    glm::vec3 m_playerPosition{0.0f};
    std::vector<glm::vec3> m_lodReferences;

    // Adaptive budget
    float m_lastFrameTime = 16.67f;
    float m_adaptedBudget = 2.0f;
};

// ============================================================================
// Behavior LOD Helpers
// ============================================================================

/**
 * @brief Helper class for LOD-aware AI behaviors
 *
 * Use this to implement different behavior complexity based on LOD level.
 */
class BehaviorLod {
public:
    /**
     * @brief Check if pathfinding should be performed
     */
    [[nodiscard]] static bool ShouldPathfind(AILodLevel lod) {
        return lod <= AILodLevel::Medium;
    }

    /**
     * @brief Check if complex decision making should be performed
     */
    [[nodiscard]] static bool ShouldMakeDecisions(AILodLevel lod) {
        return lod <= AILodLevel::Medium;
    }

    /**
     * @brief Check if animation should be updated
     */
    [[nodiscard]] static bool ShouldUpdateAnimation(AILodLevel lod) {
        return lod <= AILodLevel::Low;
    }

    /**
     * @brief Check if entity should move at all
     */
    [[nodiscard]] static bool ShouldMove(AILodLevel lod) {
        return lod < AILodLevel::Dormant;
    }

    /**
     * @brief Get pathfinding quality for LOD
     * @return 0.0 = skip, 1.0 = full quality
     */
    [[nodiscard]] static float GetPathfindingQuality(AILodLevel lod) {
        switch (lod) {
            case AILodLevel::Full:    return 1.0f;
            case AILodLevel::Medium:  return 0.5f;
            case AILodLevel::Low:     return 0.0f;
            case AILodLevel::Dormant: return 0.0f;
        }
        return 0.0f;
    }

    /**
     * @brief Get perception range multiplier for LOD
     */
    [[nodiscard]] static float GetPerceptionMultiplier(AILodLevel lod) {
        switch (lod) {
            case AILodLevel::Full:    return 1.0f;
            case AILodLevel::Medium:  return 0.75f;
            case AILodLevel::Low:     return 0.5f;
            case AILodLevel::Dormant: return 0.25f;
        }
        return 1.0f;
    }
};

// ============================================================================
// Cached AI Calculations
// ============================================================================

/**
 * @brief Cache for expensive AI calculations
 *
 * Stores results of expensive operations that don't need
 * to be recalculated every frame.
 */
class AICalculationCache {
public:
    struct CachedValue {
        float value = 0.0f;
        float timestamp = 0.0f;
        float validDuration = 1.0f;
    };

    struct CachedVector {
        glm::vec3 value{0.0f};
        float timestamp = 0.0f;
        float validDuration = 1.0f;
    };

    /**
     * @brief Get or compute a cached float value
     */
    template<typename ComputeFunc>
    float GetOrCompute(EntityId entityId, const std::string& key,
                       float currentTime, float validDuration,
                       ComputeFunc compute) {
        uint64_t cacheKey = MakeCacheKey(entityId, key);
        auto it = m_floatCache.find(cacheKey);

        if (it != m_floatCache.end()) {
            if (currentTime - it->second.timestamp < it->second.validDuration) {
                return it->second.value;
            }
        }

        float value = compute();
        m_floatCache[cacheKey] = {value, currentTime, validDuration};
        return value;
    }

    /**
     * @brief Get or compute a cached vector value
     */
    template<typename ComputeFunc>
    glm::vec3 GetOrComputeVec(EntityId entityId, const std::string& key,
                              float currentTime, float validDuration,
                              ComputeFunc compute) {
        uint64_t cacheKey = MakeCacheKey(entityId, key);
        auto it = m_vectorCache.find(cacheKey);

        if (it != m_vectorCache.end()) {
            if (currentTime - it->second.timestamp < it->second.validDuration) {
                return it->second.value;
            }
        }

        glm::vec3 value = compute();
        m_vectorCache[cacheKey] = {value, currentTime, validDuration};
        return value;
    }

    /**
     * @brief Invalidate cache for an entity
     */
    void InvalidateEntity(EntityId entityId);

    /**
     * @brief Clear all cached values
     */
    void Clear();

    /**
     * @brief Remove expired entries
     */
    void PruneExpired(float currentTime);

private:
    static uint64_t MakeCacheKey(EntityId entityId, const std::string& key) {
        // Simple hash combining entity ID and key hash
        std::hash<std::string> hasher;
        return (static_cast<uint64_t>(entityId) << 32) |
               static_cast<uint32_t>(hasher(key));
    }

    std::unordered_map<uint64_t, CachedValue> m_floatCache;
    std::unordered_map<uint64_t, CachedVector> m_vectorCache;
};

} // namespace Systems
} // namespace Vehement
