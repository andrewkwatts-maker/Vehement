#include "AIScheduler.hpp"
#include <algorithm>
#include <cmath>

namespace Vehement {
namespace Systems {

// ============================================================================
// AIScheduler Implementation
// ============================================================================

AIScheduler::AIScheduler(const Config& config)
    : m_config(config)
    , m_adaptedBudget(config.timeBudgetMs)
{
    m_updateQueue.reserve(config.maxUpdatesPerFrame * 2);
}

void AIScheduler::RegisterEntity(EntityId entityId, const glm::vec3& position,
                                  AIPriority priority) {
    if (m_entities.count(entityId) > 0) {
        return;  // Already registered
    }

    AIEntityInfo info;
    info.entityId = entityId;
    info.position = position;
    info.priority = priority;
    info.lodLevel = ComputeLodLevel(glm::dot(position - m_playerPosition,
                                              position - m_playerPosition));
    info.updateInterval = ComputeUpdateInterval(info);
    info.lastUpdateFrame = m_currentFrame;

    m_entities[entityId] = info;

    // Update stats
    m_stats.totalEntities = static_cast<uint32_t>(m_entities.size());
}

void AIScheduler::UnregisterEntity(EntityId entityId) {
    auto it = m_entities.find(entityId);
    if (it == m_entities.end()) {
        return;
    }

    // Remove from group if in one
    if (it->second.groupId != 0) {
        RemoveFromGroup(entityId);
    }

    // Remove from force update set
    m_forceUpdateSet.erase(entityId);

    m_entities.erase(it);
    m_stats.totalEntities = static_cast<uint32_t>(m_entities.size());
}

bool AIScheduler::IsRegistered(EntityId entityId) const {
    return m_entities.count(entityId) > 0;
}

void AIScheduler::UpdateEntityPosition(EntityId entityId, const glm::vec3& position) {
    auto it = m_entities.find(entityId);
    if (it == m_entities.end()) {
        return;
    }

    it->second.position = position;

    // Recalculate distance to player
    glm::vec3 toPlayer = position - m_playerPosition;
    it->second.distanceToPlayer = std::sqrt(glm::dot(toPlayer, toPlayer));
}

void AIScheduler::SetEntityPriority(EntityId entityId, AIPriority priority) {
    auto it = m_entities.find(entityId);
    if (it != m_entities.end()) {
        it->second.priority = priority;
        it->second.updateInterval = ComputeUpdateInterval(it->second);
    }
}

void AIScheduler::ForceUpdate(EntityId entityId) {
    if (m_entities.count(entityId) > 0) {
        m_forceUpdateSet.insert(entityId);
    }
}

AILodLevel AIScheduler::GetEntityLod(EntityId entityId) const {
    auto it = m_entities.find(entityId);
    return it != m_entities.end() ? it->second.lodLevel : AILodLevel::Dormant;
}

void AIScheduler::SetPlayerPosition(const glm::vec3& position) {
    m_playerPosition = position;
}

void AIScheduler::AddLodReference(const glm::vec3& position) {
    m_lodReferences.push_back(position);
}

void AIScheduler::ClearLodReferences() {
    m_lodReferences.clear();
}

uint32_t AIScheduler::CreateGroup() {
    uint32_t groupId = m_nextGroupId++;

    AIGroup group;
    group.groupId = groupId;

    m_groups[groupId] = group;
    return groupId;
}

void AIScheduler::DestroyGroup(uint32_t groupId) {
    auto it = m_groups.find(groupId);
    if (it == m_groups.end()) {
        return;
    }

    // Remove all members from the group
    for (EntityId memberId : it->second.members) {
        auto entityIt = m_entities.find(memberId);
        if (entityIt != m_entities.end()) {
            entityIt->second.groupId = 0;
            entityIt->second.isGroupLeader = false;
        }
    }

    m_groups.erase(it);
}

void AIScheduler::AddToGroup(EntityId entityId, uint32_t groupId) {
    auto entityIt = m_entities.find(entityId);
    if (entityIt == m_entities.end()) {
        return;
    }

    auto groupIt = m_groups.find(groupId);
    if (groupIt == m_groups.end()) {
        return;
    }

    // Remove from current group if any
    if (entityIt->second.groupId != 0) {
        RemoveFromGroup(entityId);
    }

    // Add to new group
    groupIt->second.members.push_back(entityId);
    entityIt->second.groupId = groupId;

    // First member becomes leader
    if (groupIt->second.members.size() == 1) {
        groupIt->second.leaderId = entityId;
        entityIt->second.isGroupLeader = true;
    }
}

void AIScheduler::RemoveFromGroup(EntityId entityId) {
    auto entityIt = m_entities.find(entityId);
    if (entityIt == m_entities.end() || entityIt->second.groupId == 0) {
        return;
    }

    uint32_t groupId = entityIt->second.groupId;
    auto groupIt = m_groups.find(groupId);

    if (groupIt != m_groups.end()) {
        auto& members = groupIt->second.members;
        members.erase(std::remove(members.begin(), members.end(), entityId),
                      members.end());

        // Reassign leader if needed
        if (groupIt->second.leaderId == entityId && !members.empty()) {
            groupIt->second.leaderId = members[0];
            auto newLeaderIt = m_entities.find(members[0]);
            if (newLeaderIt != m_entities.end()) {
                newLeaderIt->second.isGroupLeader = true;
            }
        }

        // Destroy empty groups
        if (members.empty()) {
            m_groups.erase(groupIt);
        }
    }

    entityIt->second.groupId = 0;
    entityIt->second.isGroupLeader = false;
}

uint32_t AIScheduler::GetEntityGroup(EntityId entityId) const {
    auto it = m_entities.find(entityId);
    return it != m_entities.end() ? it->second.groupId : 0;
}

void AIScheduler::SetGroupTarget(uint32_t groupId, const glm::vec3& target) {
    auto it = m_groups.find(groupId);
    if (it != m_groups.end()) {
        it->second.sharedTarget = target;
        it->second.hasSharedPath = false;  // Invalidate path
    }
}

const AIGroup* AIScheduler::GetGroup(uint32_t groupId) const {
    auto it = m_groups.find(groupId);
    return it != m_groups.end() ? &it->second : nullptr;
}

void AIScheduler::AutoFormGroups() {
    // Simple proximity-based group formation
    // Could be optimized with spatial hash

    std::unordered_set<EntityId> ungrouped;
    for (const auto& [id, info] : m_entities) {
        if (info.groupId == 0) {
            ungrouped.insert(id);
        }
    }

    float radiusSq = m_config.groupFormationRadius * m_config.groupFormationRadius;

    while (!ungrouped.empty()) {
        EntityId seedId = *ungrouped.begin();
        ungrouped.erase(seedId);

        const auto& seedInfo = m_entities.at(seedId);

        // Create new group
        uint32_t groupId = CreateGroup();
        AddToGroup(seedId, groupId);

        // Find nearby ungrouped entities
        std::vector<EntityId> toAdd;
        for (EntityId otherId : ungrouped) {
            if (m_groups[groupId].members.size() >= m_config.maxGroupSize) {
                break;
            }

            const auto& otherInfo = m_entities.at(otherId);
            glm::vec3 diff = otherInfo.position - seedInfo.position;
            float distSq = glm::dot(diff, diff);

            if (distSq <= radiusSq) {
                toAdd.push_back(otherId);
            }
        }

        for (EntityId id : toAdd) {
            AddToGroup(id, groupId);
            ungrouped.erase(id);
        }

        // Don't create single-member groups
        if (m_groups[groupId].members.size() == 1) {
            DestroyGroup(groupId);
        }
    }
}

void AIScheduler::Update(float deltaTime,
                          AIUpdateCallback updateCallback,
                          GroupUpdateCallback groupCallback) {
    auto startTime = std::chrono::high_resolution_clock::now();

    m_stats.Reset();
    ++m_currentFrame;
    m_accumulatedTime += deltaTime;

    // Update LOD levels for all entities
    UpdateLodLevels();

    // Update time since last update for all entities
    for (auto& [id, info] : m_entities) {
        info.timeSinceUpdate += deltaTime;
    }

    // Build priority queue of entities to update
    BuildUpdateQueue();

    // Process updates within budget
    uint32_t updateCount = 0;
    float budgetMs = m_config.enableAdaptiveBudget ? m_adaptedBudget : m_config.timeBudgetMs;

    for (AIEntityInfo* info : m_updateQueue) {
        // Check time budget
        auto currentTime = std::chrono::high_resolution_clock::now();
        float elapsedMs = std::chrono::duration<float, std::milli>(
            currentTime - startTime).count();

        if (elapsedMs >= budgetMs && updateCount > 0 &&
            info->priority != AIPriority::Critical) {
            break;
        }

        // Check update count limit
        if (updateCount >= m_config.maxUpdatesPerFrame &&
            info->priority != AIPriority::Critical) {
            break;
        }

        // Skip dormant entities unless forced
        if (info->lodLevel == AILodLevel::Dormant &&
            m_forceUpdateSet.count(info->entityId) == 0) {
            ++m_stats.skippedDueToLod;
            continue;
        }

        // Execute update callback
        float entityDeltaTime = info->timeSinceUpdate;
        if (updateCallback(info->entityId, entityDeltaTime, info->lodLevel)) {
            info->lastUpdateFrame = m_currentFrame;
            info->timeSinceUpdate = 0.0f;
            ++updateCount;
            ++m_stats.updatedThisFrame;
        }

        // Remove from force update set
        m_forceUpdateSet.erase(info->entityId);
    }

    // Update groups
    if (groupCallback) {
        UpdateGroups(deltaTime, groupCallback);
    }

    // Calculate stats
    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.updateTimeMs = std::chrono::duration<float, std::milli>(
        endTime - startTime).count();

    m_stats.averageUpdateTimeMs = (m_stats.averageUpdateTimeMs * 0.9f) +
                                   (m_stats.updateTimeMs * 0.1f);
    m_stats.peakUpdateTimeMs = std::max(m_stats.peakUpdateTimeMs, m_stats.updateTimeMs);

    // Adapt budget based on frame time
    if (m_config.enableAdaptiveBudget) {
        AdaptBudget(deltaTime * 1000.0f);
    }
}

std::vector<EntityId> AIScheduler::GetScheduledEntities() const {
    std::vector<EntityId> result;
    result.reserve(m_config.maxUpdatesPerFrame);

    for (size_t i = 0; i < m_updateQueue.size() && i < m_config.maxUpdatesPerFrame; ++i) {
        result.push_back(m_updateQueue[i]->entityId);
    }

    return result;
}

void AIScheduler::ProcessEntity(EntityId entityId, float deltaTime,
                                 AIUpdateCallback callback) {
    auto it = m_entities.find(entityId);
    if (it == m_entities.end()) {
        return;
    }

    callback(entityId, deltaTime, it->second.lodLevel);
    it->second.lastUpdateFrame = m_currentFrame;
    it->second.timeSinceUpdate = 0.0f;
}

void AIScheduler::SetLodDistances(float full, float medium, float low) {
    m_config.lodFullDistanceSq = full * full;
    m_config.lodMediumDistanceSq = medium * medium;
    m_config.lodLowDistanceSq = low * low;
}

void AIScheduler::ResetStats() {
    m_stats = AISchedulerStats{};
    m_stats.totalEntities = static_cast<uint32_t>(m_entities.size());
}

std::vector<EntityId> AIScheduler::GetEntitiesAtLod(AILodLevel lod) const {
    std::vector<EntityId> result;

    for (const auto& [id, info] : m_entities) {
        if (info.lodLevel == lod) {
            result.push_back(id);
        }
    }

    return result;
}

const AIEntityInfo* AIScheduler::GetEntityInfo(EntityId entityId) const {
    auto it = m_entities.find(entityId);
    return it != m_entities.end() ? &it->second : nullptr;
}

void AIScheduler::UpdateLodLevels() {
    m_stats.fullLodCount = 0;
    m_stats.mediumLodCount = 0;
    m_stats.lowLodCount = 0;
    m_stats.dormantCount = 0;

    for (auto& [id, info] : m_entities) {
        // Calculate minimum distance to any LOD reference
        float minDistSq = std::numeric_limits<float>::max();

        // Distance to player
        glm::vec3 toPlayer = info.position - m_playerPosition;
        float distSqPlayer = glm::dot(toPlayer, toPlayer);
        minDistSq = std::min(minDistSq, distSqPlayer);
        info.distanceToPlayer = std::sqrt(distSqPlayer);

        // Distance to other references
        for (const auto& ref : m_lodReferences) {
            glm::vec3 toRef = info.position - ref;
            minDistSq = std::min(minDistSq, glm::dot(toRef, toRef));
        }

        // Compute LOD level
        AILodLevel newLod = ComputeLodLevel(minDistSq);

        // Hysteresis to prevent LOD flickering
        if (newLod != info.lodLevel) {
            // Allow transition to higher detail immediately
            // Add delay for transition to lower detail
            if (static_cast<uint8_t>(newLod) > static_cast<uint8_t>(info.lodLevel)) {
                // Transitioning to lower detail - could add hysteresis here
            }
            info.lodLevel = newLod;
            info.updateInterval = ComputeUpdateInterval(info);
        }

        // Update LOD stats
        switch (info.lodLevel) {
            case AILodLevel::Full:    ++m_stats.fullLodCount; break;
            case AILodLevel::Medium:  ++m_stats.mediumLodCount; break;
            case AILodLevel::Low:     ++m_stats.lowLodCount; break;
            case AILodLevel::Dormant: ++m_stats.dormantCount; break;
        }
    }
}

void AIScheduler::BuildUpdateQueue() {
    m_updateQueue.clear();

    for (auto& [id, info] : m_entities) {
        // Check if entity needs update
        bool needsUpdate = info.timeSinceUpdate >= info.updateInterval;
        bool forcedUpdate = m_forceUpdateSet.count(id) > 0;
        bool isCritical = info.priority == AIPriority::Critical;

        if (needsUpdate || forcedUpdate || isCritical) {
            m_updateQueue.push_back(&info);
        }
    }

    // Sort by priority
    std::sort(m_updateQueue.begin(), m_updateQueue.end(),
              UpdatePriorityCompare{});
}

float AIScheduler::ComputeUpdateInterval(const AIEntityInfo& info) const {
    // Base interval from priority
    float baseInterval;
    switch (info.priority) {
        case AIPriority::Critical:   baseInterval = m_config.criticalUpdateInterval; break;
        case AIPriority::High:       baseInterval = m_config.highUpdateInterval; break;
        case AIPriority::Normal:     baseInterval = m_config.normalUpdateInterval; break;
        case AIPriority::Low:        baseInterval = m_config.lowUpdateInterval; break;
        case AIPriority::Background: baseInterval = m_config.backgroundUpdateInterval; break;
        default:                     baseInterval = m_config.normalUpdateInterval; break;
    }

    // Apply LOD multiplier
    float lodMultiplier = 1.0f;
    switch (info.lodLevel) {
        case AILodLevel::Full:    lodMultiplier = 1.0f; break;
        case AILodLevel::Medium:  lodMultiplier = m_config.mediumLodMultiplier; break;
        case AILodLevel::Low:     lodMultiplier = m_config.lowLodMultiplier; break;
        case AILodLevel::Dormant: lodMultiplier = m_config.dormantLodMultiplier; break;
    }

    return baseInterval * lodMultiplier;
}

AILodLevel AIScheduler::ComputeLodLevel(float distanceSq) const {
    if (distanceSq <= m_config.lodFullDistanceSq) {
        return AILodLevel::Full;
    } else if (distanceSq <= m_config.lodMediumDistanceSq) {
        return AILodLevel::Medium;
    } else if (distanceSq <= m_config.lodLowDistanceSq) {
        return AILodLevel::Low;
    }
    return AILodLevel::Dormant;
}

void AIScheduler::UpdateGroups(float deltaTime, GroupUpdateCallback callback) {
    for (auto& [groupId, group] : m_groups) {
        if (group.members.empty()) continue;

        // Calculate group center
        glm::vec3 center{0.0f};
        for (EntityId memberId : group.members) {
            auto it = m_entities.find(memberId);
            if (it != m_entities.end()) {
                center += it->second.position;
            }
        }
        center /= static_cast<float>(group.members.size());
        group.groupCenter = center;

        // Call group update callback
        callback(group, deltaTime);
        ++m_stats.groupsUpdated;
    }
}

void AIScheduler::AdaptBudget(float frameTime) {
    m_lastFrameTime = frameTime;

    // If frame time is over target, reduce budget
    if (frameTime > m_config.targetFrameTime) {
        float overage = frameTime - m_config.targetFrameTime;
        m_adaptedBudget = std::max(0.5f, m_adaptedBudget - overage * 0.5f);
    }
    // If frame time is under target, slowly increase budget
    else {
        float headroom = m_config.targetFrameTime - frameTime;
        m_adaptedBudget = std::min(m_config.timeBudgetMs * 2.0f,
                                    m_adaptedBudget + headroom * 0.1f);
    }
}

// ============================================================================
// AICalculationCache Implementation
// ============================================================================

void AICalculationCache::InvalidateEntity(EntityId entityId) {
    // Remove all entries for this entity
    // This is O(n) but could be optimized with per-entity index
    uint64_t entityMask = static_cast<uint64_t>(entityId) << 32;

    for (auto it = m_floatCache.begin(); it != m_floatCache.end();) {
        if ((it->first & 0xFFFFFFFF00000000ULL) == entityMask) {
            it = m_floatCache.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_vectorCache.begin(); it != m_vectorCache.end();) {
        if ((it->first & 0xFFFFFFFF00000000ULL) == entityMask) {
            it = m_vectorCache.erase(it);
        } else {
            ++it;
        }
    }
}

void AICalculationCache::Clear() {
    m_floatCache.clear();
    m_vectorCache.clear();
}

void AICalculationCache::PruneExpired(float currentTime) {
    for (auto it = m_floatCache.begin(); it != m_floatCache.end();) {
        if (currentTime - it->second.timestamp > it->second.validDuration * 2.0f) {
            it = m_floatCache.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_vectorCache.begin(); it != m_vectorCache.end();) {
        if (currentTime - it->second.timestamp > it->second.validDuration * 2.0f) {
            it = m_vectorCache.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace Systems
} // namespace Vehement
