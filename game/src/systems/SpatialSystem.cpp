#include "SpatialSystem.hpp"
#include <algorithm>
#include <cmath>

namespace Vehement {

SpatialSystem::SpatialSystem()
    : SpatialSystem(Config{}) {}

SpatialSystem::SpatialSystem(const Config& config)
    : m_config(config) {
    Nova::SpatialManager::Config managerConfig;
    managerConfig.worldBounds = config.worldBounds;
    managerConfig.defaultIndexType = Nova::SpatialIndexType::BVH;
    managerConfig.spatialHashCellSize = config.unitCellSize;
    managerConfig.enableQueryCaching = true;
    managerConfig.enableProfiling = false;
    managerConfig.threadSafe = true;

    m_spatialManager = Nova::SpatialManager(managerConfig);
}

SpatialSystem::~SpatialSystem() {
    Shutdown();
}

void SpatialSystem::Initialize() {
    m_spatialManager.Initialize();
}

void SpatialSystem::Update(float deltaTime) {
    m_spatialManager.Update(deltaTime);

    if (m_config.enableRangeTriggers) {
        ProcessRangeTriggers();
    }
}

void SpatialSystem::Shutdown() {
    m_spatialManager.Shutdown();
    m_unitData.clear();
    m_buildingTeams.clear();
    m_rangeTriggers.clear();
}

void SpatialSystem::RegisterUnit(uint64_t entityId, const glm::vec3& position,
                                 float radius, TeamId team) {
    Nova::AABB bounds = Nova::AABB::FromCenterExtents(position, glm::vec3(radius));
    m_spatialManager.RegisterObject(entityId, bounds, Nova::SpatialLayer::Units);

    UnitSpatialData data;
    data.entityId = entityId;
    data.team = team;
    data.radius = radius;
    data.isAlive = true;
    data.canBeTargeted = true;

    {
        std::unique_lock lock(m_mutex);
        m_unitData[entityId] = data;
        ++m_unitCount;
    }
}

void SpatialSystem::RegisterBuilding(uint64_t entityId, const Nova::AABB& bounds,
                                     TeamId team) {
    m_spatialManager.RegisterObject(entityId, bounds, Nova::SpatialLayer::Buildings);

    {
        std::unique_lock lock(m_mutex);
        m_buildingTeams[entityId] = team;
        ++m_buildingCount;
    }
}

void SpatialSystem::RegisterProjectile(uint64_t entityId, const glm::vec3& position,
                                       float radius) {
    Nova::AABB bounds = Nova::AABB::FromCenterExtents(position, glm::vec3(radius));
    m_spatialManager.RegisterObject(entityId, bounds, Nova::SpatialLayer::Projectiles);

    {
        std::unique_lock lock(m_mutex);
        ++m_projectileCount;
    }
}

void SpatialSystem::RegisterTerrainChunk(uint64_t chunkId, const Nova::AABB& bounds) {
    m_spatialManager.RegisterObject(chunkId, bounds, Nova::SpatialLayer::Terrain);
}

void SpatialSystem::UnregisterEntity(uint64_t entityId) {
    m_spatialManager.UnregisterObject(entityId);

    std::unique_lock lock(m_mutex);

    auto unitIt = m_unitData.find(entityId);
    if (unitIt != m_unitData.end()) {
        m_unitData.erase(unitIt);
        --m_unitCount;
        return;
    }

    auto buildingIt = m_buildingTeams.find(entityId);
    if (buildingIt != m_buildingTeams.end()) {
        m_buildingTeams.erase(buildingIt);
        --m_buildingCount;
        return;
    }

    // Assume projectile
    --m_projectileCount;
}

void SpatialSystem::UpdateEntityPosition(uint64_t entityId, const glm::vec3& position) {
    std::shared_lock lock(m_mutex);

    auto unitIt = m_unitData.find(entityId);
    if (unitIt != m_unitData.end()) {
        Nova::AABB bounds = Nova::AABB::FromCenterExtents(position, glm::vec3(unitIt->second.radius));
        m_spatialManager.UpdateObject(entityId, bounds);
        return;
    }

    // For other entities, get existing bounds and translate
    Nova::AABB currentBounds = m_spatialManager.GetObjectBounds(entityId);
    if (currentBounds.IsValid()) {
        glm::vec3 center = currentBounds.GetCenter();
        glm::vec3 offset = position - center;
        currentBounds.Translate(offset);
        m_spatialManager.UpdateObject(entityId, currentBounds);
    }
}

void SpatialSystem::UpdateEntityBounds(uint64_t entityId, const Nova::AABB& bounds) {
    m_spatialManager.UpdateObject(entityId, bounds);
}

void SpatialSystem::SetUnitAlive(uint64_t entityId, bool alive) {
    std::unique_lock lock(m_mutex);
    auto it = m_unitData.find(entityId);
    if (it != m_unitData.end()) {
        it->second.isAlive = alive;
    }
}

void SpatialSystem::SetUnitTargetable(uint64_t entityId, bool targetable) {
    std::unique_lock lock(m_mutex);
    auto it = m_unitData.find(entityId);
    if (it != m_unitData.end()) {
        it->second.canBeTargeted = targetable;
    }
}

bool SpatialSystem::PassesTeamFilter(uint64_t entityId, TeamId filter) const {
    if (filter == TeamId::None) return true;

    auto it = m_unitData.find(entityId);
    if (it != m_unitData.end()) {
        return it->second.team == filter;
    }

    auto buildingIt = m_buildingTeams.find(entityId);
    if (buildingIt != m_buildingTeams.end()) {
        return buildingIt->second == filter;
    }

    return true;
}

std::vector<uint64_t> SpatialSystem::GetUnitsInRange(
    const glm::vec3& position,
    float radius,
    TeamId teamFilter,
    bool aliveOnly,
    bool targetableOnly) {

    auto candidates = m_spatialManager.QuerySphere(
        position, radius,
        Nova::LayerMask(Nova::SpatialLayer::Units)
    );

    std::vector<uint64_t> results;
    results.reserve(candidates.size());

    std::shared_lock lock(m_mutex);

    for (uint64_t id : candidates) {
        auto it = m_unitData.find(id);
        if (it == m_unitData.end()) continue;

        const UnitSpatialData& data = it->second;

        if (aliveOnly && !data.isAlive) continue;
        if (targetableOnly && !data.canBeTargeted) continue;
        if (teamFilter != TeamId::None && data.team != teamFilter) continue;

        results.push_back(id);
    }

    return results;
}

std::vector<std::pair<uint64_t, float>> SpatialSystem::GetUnitsInRangeSorted(
    const glm::vec3& position,
    float radius,
    TeamId teamFilter,
    bool aliveOnly) {

    auto units = GetUnitsInRange(position, radius, teamFilter, aliveOnly, false);

    std::vector<std::pair<uint64_t, float>> sorted;
    sorted.reserve(units.size());

    for (uint64_t id : units) {
        Nova::AABB bounds = m_spatialManager.GetObjectBounds(id);
        float dist = bounds.Distance(position);
        sorted.emplace_back(id, dist);
    }

    std::sort(sorted.begin(), sorted.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });

    return sorted;
}

std::optional<uint64_t> SpatialSystem::GetNearestUnit(
    const glm::vec3& position,
    float maxRange,
    TeamId teamFilter,
    bool aliveOnly,
    uint64_t excludeId) {

    auto sorted = GetUnitsInRangeSorted(position, maxRange, teamFilter, aliveOnly);

    for (const auto& [id, dist] : sorted) {
        if (id != excludeId) {
            return id;
        }
    }

    return std::nullopt;
}

std::vector<uint64_t> SpatialSystem::GetKNearestUnits(
    const glm::vec3& position,
    size_t k,
    float maxRange,
    TeamId teamFilter,
    bool aliveOnly) {

    auto sorted = GetUnitsInRangeSorted(position, maxRange, teamFilter, aliveOnly);

    std::vector<uint64_t> results;
    results.reserve(std::min(k, sorted.size()));

    for (size_t i = 0; i < k && i < sorted.size(); ++i) {
        results.push_back(sorted[i].first);
    }

    return results;
}

std::vector<uint64_t> SpatialSystem::GetFriendlyUnitsInRange(
    const glm::vec3& position,
    float radius,
    TeamId myTeam,
    bool aliveOnly) {

    return GetUnitsInRange(position, radius, myTeam, aliveOnly, false);
}

std::vector<uint64_t> SpatialSystem::GetEnemyUnitsInRange(
    const glm::vec3& position,
    float radius,
    TeamId myTeam,
    bool aliveOnly) {

    auto candidates = m_spatialManager.QuerySphere(
        position, radius,
        Nova::LayerMask(Nova::SpatialLayer::Units)
    );

    std::vector<uint64_t> results;
    results.reserve(candidates.size());

    std::shared_lock lock(m_mutex);

    for (uint64_t id : candidates) {
        auto it = m_unitData.find(id);
        if (it == m_unitData.end()) continue;

        const UnitSpatialData& data = it->second;

        if (aliveOnly && !data.isAlive) continue;
        if (data.team == myTeam || data.team == TeamId::Neutral) continue;

        results.push_back(id);
    }

    return results;
}

std::vector<uint64_t> SpatialSystem::GetBuildingsInArea(const Nova::AABB& area) {
    return m_spatialManager.QueryAABB(
        area,
        Nova::LayerMask(Nova::SpatialLayer::Buildings)
    );
}

std::vector<uint64_t> SpatialSystem::GetBuildingsInArea(
    const Nova::AABB& area, TeamId team) {

    auto candidates = GetBuildingsInArea(area);

    std::vector<uint64_t> results;
    results.reserve(candidates.size());

    std::shared_lock lock(m_mutex);

    for (uint64_t id : candidates) {
        auto it = m_buildingTeams.find(id);
        if (it != m_buildingTeams.end() && it->second == team) {
            results.push_back(id);
        }
    }

    return results;
}

bool SpatialSystem::IsInCone(const glm::vec3& point, const ConeQuery& cone) const {
    glm::vec3 toPoint = point - cone.origin;
    float dist = glm::length(toPoint);

    if (dist > cone.range || dist < 0.001f) {
        return false;
    }

    toPoint /= dist;

    float cosAngle = glm::dot(toPoint, glm::normalize(cone.direction));
    float coneRadians = glm::radians(cone.angle);

    return cosAngle >= std::cos(coneRadians);
}

std::vector<uint64_t> SpatialSystem::GetEntitiesInCone(
    const ConeQuery& cone,
    uint64_t layerMask) {

    // Use sphere query as broad phase
    auto candidates = m_spatialManager.QuerySphere(
        cone.origin, cone.range, layerMask
    );

    std::vector<uint64_t> results;
    results.reserve(candidates.size());

    for (uint64_t id : candidates) {
        Nova::AABB bounds = m_spatialManager.GetObjectBounds(id);
        if (!bounds.IsValid()) continue;

        // Test if any part of the AABB is in the cone
        glm::vec3 center = bounds.GetCenter();
        if (IsInCone(center, cone)) {
            results.push_back(id);
        }
    }

    return results;
}

std::vector<uint64_t> SpatialSystem::GetUnitsInCone(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float halfAngleDegrees,
    float range,
    TeamId teamFilter,
    bool aliveOnly) {

    ConeQuery cone;
    cone.origin = origin;
    cone.direction = direction;
    cone.angle = halfAngleDegrees;
    cone.range = range;

    auto candidates = GetEntitiesInCone(cone, Nova::LayerMask(Nova::SpatialLayer::Units));

    std::vector<uint64_t> results;
    results.reserve(candidates.size());

    std::shared_lock lock(m_mutex);

    for (uint64_t id : candidates) {
        auto it = m_unitData.find(id);
        if (it == m_unitData.end()) continue;

        const UnitSpatialData& data = it->second;

        if (aliveOnly && !data.isAlive) continue;
        if (teamFilter != TeamId::None && data.team != teamFilter) continue;

        results.push_back(id);
    }

    return results;
}

TerrainHit SpatialSystem::RaycastTerrain(const Nova::Ray& ray, float maxDistance) {
    TerrainHit result;

    auto hits = m_spatialManager.QueryRay(
        ray, maxDistance,
        Nova::LayerMask(Nova::SpatialLayer::Terrain)
    );

    if (!hits.empty()) {
        const Nova::RayHit& hit = hits[0];
        result.hit = true;
        result.point = hit.point;
        result.normal = hit.normal;
        result.distance = hit.distance;
        result.tileId = static_cast<uint32_t>(hit.entityId);
    }

    return result;
}

std::vector<Nova::RayHit> SpatialSystem::RaycastEntities(
    const Nova::Ray& ray,
    float maxDistance,
    uint64_t layerMask) {

    return m_spatialManager.QueryRay(ray, maxDistance, layerMask);
}

std::optional<Nova::RayHit> SpatialSystem::RaycastFirst(
    const Nova::Ray& ray,
    float maxDistance,
    uint64_t layerMask,
    uint64_t excludeId) {

    auto hits = RaycastEntities(ray, maxDistance, layerMask);

    for (const auto& hit : hits) {
        if (hit.entityId != excludeId) {
            return hit;
        }
    }

    return std::nullopt;
}

bool SpatialSystem::HasLineOfSight(
    const glm::vec3& from,
    const glm::vec3& to,
    uint64_t excludeIdA,
    uint64_t excludeIdB) {

    glm::vec3 direction = to - from;
    float distance = glm::length(direction);

    if (distance < 0.001f) return true;

    direction /= distance;
    Nova::Ray ray(from, direction);

    // Check terrain
    TerrainHit terrainHit = RaycastTerrain(ray, distance);
    if (terrainHit.hit && terrainHit.distance < distance - 0.1f) {
        return false;
    }

    // Check buildings
    auto buildingHits = m_spatialManager.QueryRay(
        ray, distance,
        Nova::LayerMask(Nova::SpatialLayer::Buildings)
    );

    for (const auto& hit : buildingHits) {
        if (hit.entityId != excludeIdA && hit.entityId != excludeIdB) {
            if (hit.distance < distance - 0.1f) {
                return false;
            }
        }
    }

    return true;
}

std::vector<Nova::AABB> SpatialSystem::GetPathfindingObstacles(const Nova::AABB& area) {
    std::vector<Nova::AABB> obstacles;

    // Get buildings
    auto buildings = GetBuildingsInArea(area);
    for (uint64_t id : buildings) {
        Nova::AABB bounds = m_spatialManager.GetObjectBounds(id);
        if (bounds.IsValid()) {
            obstacles.push_back(bounds);
        }
    }

    // Could also include terrain obstacles, static units, etc.

    return obstacles;
}

bool SpatialSystem::IsPositionWalkable(const glm::vec3& position, float radius) {
    Nova::AABB testBounds = Nova::AABB::FromCenterExtents(position, glm::vec3(radius, 0.5f, radius));

    // Check for building collisions
    auto buildings = m_spatialManager.QueryAABB(
        testBounds,
        Nova::LayerMask(Nova::SpatialLayer::Buildings)
    );

    return buildings.empty();
}

std::vector<glm::vec3> SpatialSystem::GetNavigablePositions(
    const glm::vec3& center,
    float radius,
    float spacing) {

    std::vector<glm::vec3> positions;

    int steps = static_cast<int>(std::ceil(radius / spacing));

    for (int x = -steps; x <= steps; ++x) {
        for (int z = -steps; z <= steps; ++z) {
            glm::vec3 pos = center + glm::vec3(x * spacing, 0.0f, z * spacing);

            if (glm::distance(pos, center) <= radius && IsPositionWalkable(pos)) {
                positions.push_back(pos);
            }
        }
    }

    return positions;
}

uint64_t SpatialSystem::CreateRangeTrigger(
    uint64_t ownerId,
    const glm::vec3& center,
    float radius,
    SpatialEventCallback onEnter,
    SpatialEventCallback onExit,
    uint64_t layerMask,
    TeamId teamFilter) {

    RangeTrigger trigger;
    trigger.id = GenerateTriggerId();
    trigger.ownerId = ownerId;
    trigger.center = center;
    trigger.radius = radius;
    trigger.layerMask = layerMask;
    trigger.teamFilter = teamFilter;
    trigger.onEnter = std::move(onEnter);
    trigger.onExit = std::move(onExit);

    {
        std::unique_lock lock(m_mutex);
        m_rangeTriggers[trigger.id] = std::move(trigger);
    }

    return trigger.id;
}

void SpatialSystem::UpdateRangeTrigger(uint64_t triggerId, const glm::vec3& center) {
    std::unique_lock lock(m_mutex);
    auto it = m_rangeTriggers.find(triggerId);
    if (it != m_rangeTriggers.end()) {
        it->second.center = center;
    }
}

void SpatialSystem::UpdateRangeTriggerRadius(uint64_t triggerId, float radius) {
    std::unique_lock lock(m_mutex);
    auto it = m_rangeTriggers.find(triggerId);
    if (it != m_rangeTriggers.end()) {
        it->second.radius = radius;
    }
}

void SpatialSystem::RemoveRangeTrigger(uint64_t triggerId) {
    std::unique_lock lock(m_mutex);
    m_rangeTriggers.erase(triggerId);
}

std::vector<uint64_t> SpatialSystem::GetEntitiesInTrigger(uint64_t triggerId) const {
    std::shared_lock lock(m_mutex);
    auto it = m_rangeTriggers.find(triggerId);
    if (it != m_rangeTriggers.end()) {
        return std::vector<uint64_t>(
            it->second.currentlyInRange.begin(),
            it->second.currentlyInRange.end()
        );
    }
    return {};
}

void SpatialSystem::ProcessRangeTriggers() {
    std::unique_lock lock(m_mutex);

    for (auto& [triggerId, trigger] : m_rangeTriggers) {
        auto currentInRange = m_spatialManager.QuerySphere(
            trigger.center, trigger.radius, trigger.layerMask
        );

        std::unordered_set<uint64_t> currentSet(currentInRange.begin(), currentInRange.end());

        // Filter by team
        if (trigger.teamFilter != TeamId::None) {
            std::unordered_set<uint64_t> filtered;
            for (uint64_t id : currentSet) {
                if (PassesTeamFilter(id, trigger.teamFilter)) {
                    filtered.insert(id);
                }
            }
            currentSet = std::move(filtered);
        }

        // Remove owner from set
        currentSet.erase(trigger.ownerId);

        // Check for entities that entered
        if (trigger.onEnter) {
            for (uint64_t id : currentSet) {
                if (!trigger.currentlyInRange.contains(id)) {
                    Nova::AABB bounds = m_spatialManager.GetObjectBounds(id);
                    SpatialEvent event;
                    event.type = SpatialEventType::OnEnterRange;
                    event.sourceId = trigger.ownerId;
                    event.targetId = id;
                    event.position = bounds.GetCenter();
                    event.distance = glm::distance(event.position, trigger.center);
                    trigger.onEnter(event);
                }
            }
        }

        // Check for entities that exited
        if (trigger.onExit) {
            for (uint64_t id : trigger.currentlyInRange) {
                if (!currentSet.contains(id)) {
                    Nova::AABB bounds = m_spatialManager.GetObjectBounds(id);
                    SpatialEvent event;
                    event.type = SpatialEventType::OnExitRange;
                    event.sourceId = trigger.ownerId;
                    event.targetId = id;
                    event.position = bounds.GetCenter();
                    event.distance = glm::distance(event.position, trigger.center);
                    trigger.onExit(event);
                }
            }
        }

        trigger.currentlyInRange = std::move(currentSet);
    }
}

uint64_t SpatialSystem::GenerateTriggerId() {
    return m_nextTriggerId++;
}

std::vector<uint64_t> SpatialSystem::GetVisibleEntities(
    const Nova::Frustum& frustum,
    uint64_t layerMask) {

    return m_spatialManager.QueryFrustum(frustum, layerMask);
}

std::vector<uint64_t> SpatialSystem::GetVisibleUnits(const Nova::Frustum& frustum) {
    return GetVisibleEntities(frustum, Nova::LayerMask(Nova::SpatialLayer::Units));
}

std::vector<uint64_t> SpatialSystem::GetVisibleBuildings(const Nova::Frustum& frustum) {
    return GetVisibleEntities(frustum, Nova::LayerMask(Nova::SpatialLayer::Buildings));
}

size_t SpatialSystem::GetUnitCount() const {
    std::shared_lock lock(m_mutex);
    return m_unitCount;
}

size_t SpatialSystem::GetBuildingCount() const {
    std::shared_lock lock(m_mutex);
    return m_buildingCount;
}

size_t SpatialSystem::GetProjectileCount() const {
    std::shared_lock lock(m_mutex);
    return m_projectileCount;
}

size_t SpatialSystem::GetMemoryUsage() const {
    size_t memory = m_spatialManager.GetMemoryUsage();

    std::shared_lock lock(m_mutex);
    memory += m_unitData.size() * sizeof(std::pair<uint64_t, UnitSpatialData>);
    memory += m_buildingTeams.size() * sizeof(std::pair<uint64_t, TeamId>);
    memory += m_rangeTriggers.size() * sizeof(std::pair<uint64_t, RangeTrigger>);

    return memory;
}

void SpatialSystem::SetDebugVisualization(bool enabled) {
    m_debugVisualization = enabled;
    m_spatialManager.SetDebugVisualization(enabled);
}

void SpatialSystem::DrawDebug() {
    if (!m_debugVisualization) return;

    m_spatialManager.DrawDebug();

    // Draw range triggers
    for (const auto& [id, trigger] : m_rangeTriggers) {
        // Would draw sphere at trigger.center with trigger.radius
    }
}

} // namespace Vehement
