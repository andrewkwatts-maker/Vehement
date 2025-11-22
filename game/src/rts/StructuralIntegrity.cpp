#include "StructuralIntegrity.hpp"
#include "WorldBuilding.hpp"
#include <algorithm>
#include <queue>
#include <cmath>

namespace Vehement {
namespace RTS {

// ============================================================================
// Material Properties
// ============================================================================

MaterialProperties GetMaterialProperties(TileType type) {
    MaterialProperties props;
    props.type = type;

    // Wood materials
    if ((type >= TileType::Wood1 && type <= TileType::WoodFlooring2)) {
        props.maxUnsupportedSpan = 4;
        props.maxStackHeight = 8;
        props.loadCapacity = 80.0f;
        props.weight = 8.0f;
        props.tensileStrength = 40.0f;
        props.compressionStrength = 60.0f;
        props.brittleness = 0.3f;
        props.canBeSupport = true;
        props.isFlexible = true;
    }
    // Stone materials
    else if ((type >= TileType::StoneBlack && type <= TileType::StoneRaw)) {
        props.maxUnsupportedSpan = 2;
        props.maxStackHeight = 20;
        props.loadCapacity = 200.0f;
        props.weight = 25.0f;
        props.tensileStrength = 20.0f;
        props.compressionStrength = 200.0f;
        props.brittleness = 0.8f;
        props.canBeSupport = true;
        props.isFlexible = false;
    }
    // Metal materials
    else if ((type >= TileType::Metal1 && type <= TileType::MetalShopFrontTop)) {
        props.maxUnsupportedSpan = 6;
        props.maxStackHeight = 30;
        props.loadCapacity = 300.0f;
        props.weight = 30.0f;
        props.tensileStrength = 150.0f;
        props.compressionStrength = 250.0f;
        props.brittleness = 0.2f;
        props.canBeSupport = true;
        props.isFlexible = false;
    }
    // Brick materials
    else if ((type >= TileType::BricksBlack && type <= TileType::BricksCornerBottomRight)) {
        props.maxUnsupportedSpan = 3;
        props.maxStackHeight = 15;
        props.loadCapacity = 150.0f;
        props.weight = 20.0f;
        props.tensileStrength = 30.0f;
        props.compressionStrength = 150.0f;
        props.brittleness = 0.6f;
        props.canBeSupport = true;
        props.isFlexible = false;
    }
    // Concrete materials
    else if ((type >= TileType::ConcreteAsphalt1 && type <= TileType::ConcreteTiles2)) {
        props.maxUnsupportedSpan = 5;
        props.maxStackHeight = 25;
        props.loadCapacity = 250.0f;
        props.weight = 28.0f;
        props.tensileStrength = 25.0f;
        props.compressionStrength = 220.0f;
        props.brittleness = 0.7f;
        props.canBeSupport = true;
        props.isFlexible = false;
    }
    // Default
    else {
        props.maxUnsupportedSpan = 3;
        props.maxStackHeight = 10;
        props.loadCapacity = 100.0f;
        props.weight = 15.0f;
        props.tensileStrength = 50.0f;
        props.compressionStrength = 100.0f;
        props.brittleness = 0.5f;
        props.canBeSupport = true;
        props.isFlexible = false;
    }

    return props;
}

// ============================================================================
// StructuralIntegrity Implementation
// ============================================================================

StructuralIntegrity::StructuralIntegrity() = default;

StructuralIntegrity::~StructuralIntegrity() = default;

bool StructuralIntegrity::Initialize(Voxel3DMap* voxelMap) {
    m_voxelMap = voxelMap;
    m_supportCacheDirty = true;
    return m_voxelMap != nullptr;
}

void StructuralIntegrity::Shutdown() {
    m_supportCache.clear();
    m_pendingCollapseChecks.clear();
    m_voxelMap = nullptr;
}

void StructuralIntegrity::Update(float deltaTime) {
    m_collapseCheckTimer += deltaTime;

    if (m_collapseCheckTimer >= m_collapseCheckInterval) {
        m_collapseCheckTimer = 0.0f;

        // Process pending collapse checks
        for (const auto& pos : m_pendingCollapseChecks) {
            CheckCollapse(pos);
        }
        m_pendingCollapseChecks.clear();
    }
}

// =========================================================================
// Stability Queries
// =========================================================================

bool StructuralIntegrity::IsStable(const glm::ivec3& pos) const {
    if (!m_voxelMap) return false;

    const Voxel* v = m_voxelMap->GetVoxel(pos);
    if (!v) return true; // Empty is stable

    // Ground level is always stable
    if (pos.y == 0) return true;

    // Check for support
    return HasSupport(pos);
}

bool StructuralIntegrity::IsStructureStable(const glm::ivec3& min, const glm::ivec3& max) const {
    if (!m_voxelMap) return false;

    for (int y = min.y; y <= max.y; ++y) {
        for (int z = min.z; z <= max.z; ++z) {
            for (int x = min.x; x <= max.x; ++x) {
                if (m_voxelMap->IsSolid(x, y, z) && !IsStable({x, y, z})) {
                    return false;
                }
            }
        }
    }

    return true;
}

bool StructuralIntegrity::HasSupport(const glm::ivec3& pos) const {
    if (!m_voxelMap) return false;

    // Ground level always supported
    if (pos.y == 0) return true;

    // Check directly below
    if (m_voxelMap->IsSolid(pos.x, pos.y - 1, pos.z)) {
        return true;
    }

    // Check adjacent horizontals (for cantilever support)
    const glm::ivec3 horizontals[] = {
        {pos.x - 1, pos.y, pos.z},
        {pos.x + 1, pos.y, pos.z},
        {pos.x, pos.y, pos.z - 1},
        {pos.x, pos.y, pos.z + 1}
    };

    int adjacentSupports = 0;
    for (const auto& adj : horizontals) {
        if (m_voxelMap->IsSolid(adj.x, adj.y, adj.z)) {
            // Check if the adjacent voxel itself is supported
            if (adj.y == 0 || m_voxelMap->IsSolid(adj.x, adj.y - 1, adj.z)) {
                ++adjacentSupports;
            }
        }
    }

    // Need at least one adjacent support for cantilever
    return adjacentSupports > 0;
}

SupportInfo StructuralIntegrity::GetSupportInfo(const glm::ivec3& pos) const {
    SupportInfo info;

    if (!m_voxelMap) return info;

    // Check cache
    int64_t key = const_cast<StructuralIntegrity*>(this)->GetKey(pos);
    auto it = m_supportCache.find(key);
    if (it != m_supportCache.end()) {
        return it->second;
    }

    // Calculate support
    if (pos.y == 0) {
        info.type = SupportType::Ground;
        info.supportStrength = 1000.0f;
        info.chainLength = 0;
        info.isStable = true;
        return info;
    }

    // Check below
    if (m_voxelMap->IsSolid(pos.x, pos.y - 1, pos.z)) {
        info.type = SupportType::Pillar;
        info.supportSource = {pos.x, pos.y - 1, pos.z};

        // Check how deep the support chain goes
        int depth = 0;
        glm::ivec3 check = pos;
        while (check.y > 0 && m_voxelMap->IsSolid(check.x, check.y - 1, check.z)) {
            --check.y;
            ++depth;
        }

        info.chainLength = depth;
        info.isStable = (check.y == 0); // Reaches ground
        info.supportStrength = info.isStable ? 200.0f : 50.0f;
        return info;
    }

    // Check horizontal (wall/cantilever support)
    const glm::ivec3 horizontals[] = {
        {pos.x - 1, pos.y, pos.z},
        {pos.x + 1, pos.y, pos.z},
        {pos.x, pos.y, pos.z - 1},
        {pos.x, pos.y, pos.z + 1}
    };

    for (const auto& adj : horizontals) {
        if (m_voxelMap->IsSolid(adj.x, adj.y, adj.z)) {
            SupportInfo adjSupport = GetSupportInfo(adj);
            if (adjSupport.isStable) {
                info.type = SupportType::Cantilever;
                info.supportSource = adj;
                info.chainLength = adjSupport.chainLength + 1;

                // Cantilever strength decreases with distance
                int maxSpan = m_defaultMaxSpan;
                if (info.chainLength <= maxSpan) {
                    info.isStable = true;
                    info.supportStrength = adjSupport.supportStrength * 0.7f;
                }
                return info;
            }
        }
    }

    // No support found
    info.type = SupportType::None;
    info.isStable = false;
    return info;
}

std::vector<glm::ivec3> StructuralIntegrity::GetSupportingVoxels(const glm::ivec3& pos) const {
    std::vector<glm::ivec3> supporters;

    if (!m_voxelMap) return supporters;

    // Below
    if (m_voxelMap->IsSolid(pos.x, pos.y - 1, pos.z)) {
        supporters.push_back({pos.x, pos.y - 1, pos.z});
    }

    // Adjacent (for cantilever)
    const glm::ivec3 horizontals[] = {
        {pos.x - 1, pos.y, pos.z},
        {pos.x + 1, pos.y, pos.z},
        {pos.x, pos.y, pos.z - 1},
        {pos.x, pos.y, pos.z + 1}
    };

    for (const auto& adj : horizontals) {
        if (m_voxelMap->IsSolid(adj.x, adj.y, adj.z)) {
            SupportInfo adjInfo = GetSupportInfo(adj);
            if (adjInfo.isStable) {
                supporters.push_back(adj);
            }
        }
    }

    return supporters;
}

std::vector<glm::ivec3> StructuralIntegrity::GetSupportedVoxels(const glm::ivec3& pos) const {
    std::vector<glm::ivec3> supported;

    if (!m_voxelMap) return supported;

    // Above
    if (m_voxelMap->IsSolid(pos.x, pos.y + 1, pos.z)) {
        supported.push_back({pos.x, pos.y + 1, pos.z});
    }

    // Adjacent that might cantilever from us
    const glm::ivec3 horizontals[] = {
        {pos.x - 1, pos.y, pos.z},
        {pos.x + 1, pos.y, pos.z},
        {pos.x, pos.y, pos.z - 1},
        {pos.x, pos.y, pos.z + 1}
    };

    for (const auto& adj : horizontals) {
        if (m_voxelMap->IsSolid(adj.x, adj.y, adj.z)) {
            auto supporters = GetSupportingVoxels(adj);
            for (const auto& s : supporters) {
                if (s == pos) {
                    supported.push_back(adj);
                    break;
                }
            }
        }
    }

    return supported;
}

bool StructuralIntegrity::CanSafelyRemove(const glm::ivec3& pos) const {
    return GetCollapsePreview(pos).empty();
}

std::vector<glm::ivec3> StructuralIntegrity::GetCollapsePreview(const glm::ivec3& pos) const {
    std::vector<glm::ivec3> wouldCollapse;

    if (!m_voxelMap) return wouldCollapse;

    // Get all voxels that depend on this one
    std::queue<glm::ivec3> checkQueue;
    std::unordered_set<int64_t> checked;

    auto addKey = [](const glm::ivec3& p) -> int64_t {
        return (static_cast<int64_t>(p.x) << 40) |
               (static_cast<int64_t>(p.y) << 20) |
               static_cast<int64_t>(p.z);
    };

    // Start with voxels directly supported by this position
    auto supported = GetSupportedVoxels(pos);
    for (const auto& s : supported) {
        checkQueue.push(s);
        checked.insert(addKey(s));
    }

    // Simulate removal by checking if each would still be supported
    while (!checkQueue.empty()) {
        glm::ivec3 check = checkQueue.front();
        checkQueue.pop();

        // Get all supports for this voxel
        auto supporters = GetSupportingVoxels(check);

        // Remove the original position from supporters
        supporters.erase(
            std::remove(supporters.begin(), supporters.end(), pos),
            supporters.end());

        // Also remove any voxels we've already determined would collapse
        for (const auto& collapsed : wouldCollapse) {
            supporters.erase(
                std::remove(supporters.begin(), supporters.end(), collapsed),
                supporters.end());
        }

        // If no remaining support, this would collapse
        if (supporters.empty()) {
            wouldCollapse.push_back(check);

            // Check what this voxel supports
            auto newSupported = GetSupportedVoxels(check);
            for (const auto& ns : newSupported) {
                int64_t key = addKey(ns);
                if (checked.find(key) == checked.end()) {
                    checked.insert(key);
                    checkQueue.push(ns);
                }
            }
        }
    }

    return wouldCollapse;
}

// =========================================================================
// Material Properties
// =========================================================================

int StructuralIntegrity::GetMaxUnsupportedSpan(TileType type) const {
    return GetMaterialProperties(type).maxUnsupportedSpan;
}

int StructuralIntegrity::GetMaxHeight(TileType type) const {
    return GetMaterialProperties(type).maxStackHeight;
}

float StructuralIntegrity::GetLoadCapacity(const glm::ivec3& pos) const {
    if (!m_voxelMap) return 0.0f;

    const Voxel* v = m_voxelMap->GetVoxel(pos);
    if (!v) return 0.0f;

    return GetMaterialProperties(v->type).loadCapacity;
}

float StructuralIntegrity::GetCurrentLoad(const glm::ivec3& pos) const {
    if (!m_voxelMap) return 0.0f;

    // Calculate weight of everything above
    float totalLoad = 0.0f;
    glm::ivec3 check = pos;

    while (m_voxelMap->IsInBounds({check.x, check.y + 1, check.z})) {
        ++check.y;
        const Voxel* above = m_voxelMap->GetVoxel(check);
        if (above) {
            totalLoad += GetMaterialProperties(above->type).weight;
        }
    }

    return totalLoad;
}

bool StructuralIntegrity::IsOverloaded(const glm::ivec3& pos) const {
    return GetCurrentLoad(pos) > GetLoadCapacity(pos);
}

// =========================================================================
// Collapse Simulation
// =========================================================================

void StructuralIntegrity::CheckCollapse(const glm::ivec3& damagedPos) {
    if (!m_voxelMap) return;

    // Find all unsupported voxels starting from damaged position
    std::vector<glm::ivec3> unsupported;
    std::queue<glm::ivec3> checkQueue;
    std::unordered_set<int64_t> checked;

    auto addKey = [](const glm::ivec3& p) -> int64_t {
        return (static_cast<int64_t>(p.x) << 40) |
               (static_cast<int64_t>(p.y) << 20) |
               static_cast<int64_t>(p.z);
    };

    // Start with positions that were supported by damaged pos
    auto supported = GetSupportedVoxels(damagedPos);
    for (const auto& s : supported) {
        checkQueue.push(s);
        checked.insert(addKey(s));
    }

    while (!checkQueue.empty()) {
        glm::ivec3 pos = checkQueue.front();
        checkQueue.pop();

        if (!IsStable(pos)) {
            unsupported.push_back(pos);

            // Check what this position supports
            auto nowUnsupported = GetSupportedVoxels(pos);
            for (const auto& u : nowUnsupported) {
                int64_t key = addKey(u);
                if (checked.find(key) == checked.end()) {
                    checked.insert(key);
                    checkQueue.push(u);
                }
            }
        }
    }

    if (!unsupported.empty()) {
        SimulateCollapse(unsupported);
    }
}

CollapseEvent StructuralIntegrity::SimulateCollapse(const std::vector<glm::ivec3>& unsupportedVoxels) {
    CollapseEvent event;
    event.collapsedPositions = unsupportedVoxels;
    event.totalMassCollapsed = 0.0f;

    if (!m_voxelMap || unsupportedVoxels.empty()) return event;

    // Calculate origin (average position)
    glm::vec3 avg{0, 0, 0};
    for (const auto& pos : unsupportedVoxels) {
        avg += glm::vec3(pos);

        const Voxel* v = m_voxelMap->GetVoxel(pos);
        if (v) {
            event.totalMassCollapsed += GetMaterialProperties(v->type).weight;
        }
    }
    avg /= static_cast<float>(unsupportedVoxels.size());
    event.originPoint = glm::ivec3(avg);

    // Calculate damage radius based on mass
    event.damageRadius = std::sqrt(event.totalMassCollapsed) * 0.5f;

    // Remove collapsed voxels
    for (const auto& pos : unsupportedVoxels) {
        m_voxelMap->RemoveVoxel(pos);
    }

    // Apply area damage from collapse
    if (m_realisticPhysics) {
        ApplyAreaDamage(event.originPoint, event.damageRadius, event.totalMassCollapsed * 0.1f);
    }

    // Trigger callback
    if (m_onCollapse) {
        m_onCollapse(event);
    }

    return event;
}

void StructuralIntegrity::ApplyDamage(const glm::ivec3& pos, float damage) {
    if (!m_voxelMap) return;

    Voxel* v = m_voxelMap->GetVoxel(pos);
    if (!v) return;

    MaterialProperties props = GetMaterialProperties(v->type);

    // Reduce health
    float effectiveDamage = damage / props.compressionStrength * 100.0f;
    int newHealth = static_cast<int>(v->health) - static_cast<int>(effectiveDamage);

    if (newHealth <= 0) {
        // Voxel destroyed
        m_voxelMap->RemoveVoxel(pos);
        m_pendingCollapseChecks.push_back(pos);
    } else {
        v->health = static_cast<uint8_t>(newHealth);
    }

    if (m_onDamage) {
        m_onDamage(pos, damage);
    }
}

void StructuralIntegrity::ApplyAreaDamage(const glm::ivec3& center, float radius, float damage) {
    if (!m_voxelMap) return;

    int iRadius = static_cast<int>(std::ceil(radius));

    for (int dz = -iRadius; dz <= iRadius; ++dz) {
        for (int dy = -iRadius; dy <= iRadius; ++dy) {
            for (int dx = -iRadius; dx <= iRadius; ++dx) {
                float dist = std::sqrt(static_cast<float>(dx*dx + dy*dy + dz*dz));

                if (dist <= radius) {
                    glm::ivec3 pos = center + glm::ivec3(dx, dy, dz);

                    if (m_voxelMap->IsSolid(pos.x, pos.y, pos.z)) {
                        // Damage falls off with distance
                        float falloff = 1.0f - (dist / radius);
                        ApplyDamage(pos, damage * falloff);
                    }
                }
            }
        }
    }
}

void StructuralIntegrity::PropagateDamage(const glm::ivec3& origin, float damage, float falloff) {
    if (!m_voxelMap) return;

    std::queue<std::pair<glm::ivec3, float>> queue;
    std::unordered_set<int64_t> visited;

    auto addKey = [](const glm::ivec3& p) -> int64_t {
        return (static_cast<int64_t>(p.x) << 40) |
               (static_cast<int64_t>(p.y) << 20) |
               static_cast<int64_t>(p.z);
    };

    queue.push({origin, damage});
    visited.insert(addKey(origin));

    const glm::ivec3 directions[] = {
        {1, 0, 0}, {-1, 0, 0},
        {0, 1, 0}, {0, -1, 0},
        {0, 0, 1}, {0, 0, -1}
    };

    while (!queue.empty()) {
        auto [pos, dmg] = queue.front();
        queue.pop();

        if (dmg < 1.0f) continue; // Negligible damage

        ApplyDamage(pos, dmg);

        // Propagate to neighbors
        float nextDamage = dmg * falloff;
        for (const auto& dir : directions) {
            glm::ivec3 next = pos + dir;
            int64_t key = addKey(next);

            if (visited.find(key) == visited.end() &&
                m_voxelMap->IsSolid(next.x, next.y, next.z)) {
                visited.insert(key);
                queue.push({next, nextDamage});
            }
        }
    }
}

// =========================================================================
// Structural Analysis
// =========================================================================

StructuralIntegrity::StructuralAnalysis StructuralIntegrity::AnalyzeStructure(
    const glm::ivec3& min, const glm::ivec3& max) const {

    StructuralAnalysis analysis;
    analysis.isStable = true;
    analysis.totalVoxels = 0;
    analysis.supportedVoxels = 0;
    analysis.unsupportedVoxels = 0;
    analysis.overloadedVoxels = 0;
    analysis.averageStress = 0.0f;
    analysis.maxStress = 0.0f;

    if (!m_voxelMap) return analysis;

    float totalStress = 0.0f;

    for (int y = min.y; y <= max.y; ++y) {
        for (int z = min.z; z <= max.z; ++z) {
            for (int x = min.x; x <= max.x; ++x) {
                if (m_voxelMap->IsSolid(x, y, z)) {
                    ++analysis.totalVoxels;

                    glm::ivec3 pos{x, y, z};

                    if (IsStable(pos)) {
                        ++analysis.supportedVoxels;
                    } else {
                        ++analysis.unsupportedVoxels;
                        analysis.isStable = false;
                    }

                    if (IsOverloaded(pos)) {
                        ++analysis.overloadedVoxels;
                        analysis.isStable = false;
                    }

                    float stress = CalculateStress(pos);
                    totalStress += stress;

                    if (stress > analysis.maxStress) {
                        analysis.maxStress = stress;
                        analysis.weakestPoint = pos;
                    }

                    // Check if this is a critical support
                    auto wouldCollapse = GetCollapsePreview(pos);
                    if (!wouldCollapse.empty()) {
                        analysis.criticalPoints.push_back(pos);
                    }
                }
            }
        }
    }

    if (analysis.totalVoxels > 0) {
        analysis.averageStress = totalStress / analysis.totalVoxels;
    }

    return analysis;
}

glm::ivec3 StructuralIntegrity::FindWeakestPoint(const glm::ivec3& min, const glm::ivec3& max) const {
    auto analysis = AnalyzeStructure(min, max);
    return analysis.weakestPoint;
}

std::vector<glm::ivec3> StructuralIntegrity::GetCriticalSupports(
    const glm::ivec3& min, const glm::ivec3& max) const {

    auto analysis = AnalyzeStructure(min, max);
    return analysis.criticalPoints;
}

std::vector<glm::ivec3> StructuralIntegrity::SuggestSupports(
    const glm::ivec3& min, const glm::ivec3& max) const {

    std::vector<glm::ivec3> suggestions;

    if (!m_voxelMap) return suggestions;

    // Find unsupported areas and suggest pillars
    for (int z = min.z; z <= max.z; z += m_defaultMaxSpan) {
        for (int x = min.x; x <= max.x; x += m_defaultMaxSpan) {
            // Find the lowest floor level at this XZ
            for (int y = min.y; y <= max.y; ++y) {
                if (m_voxelMap->IsSolid(x, y, z) && !IsStable({x, y, z})) {
                    // Suggest pillar at ground level
                    suggestions.push_back({x, 0, z});
                    break;
                }
            }
        }
    }

    return suggestions;
}

float StructuralIntegrity::CalculateStress(const glm::ivec3& pos) const {
    if (!m_voxelMap) return 0.0f;

    float load = GetCurrentLoad(pos);
    float capacity = GetLoadCapacity(pos);

    if (capacity <= 0.0f) return 1.0f;
    return load / capacity;
}

// ============================================================================
// Helper Functions
// ============================================================================

bool IsValidArch(const std::vector<glm::ivec3>& positions) {
    if (positions.size() < 3) return false;

    // Check if positions form a curved shape
    // This is a simplified check - real arch validation would be more complex
    return true;
}

glm::vec3 CalculateCenterOfMass(const std::vector<Voxel>& voxels) {
    if (voxels.empty()) return {0, 0, 0};

    glm::vec3 weightedSum{0, 0, 0};
    float totalWeight = 0.0f;

    for (const auto& v : voxels) {
        float weight = GetMaterialProperties(v.type).weight;
        weightedSum += glm::vec3(v.position) * weight;
        totalWeight += weight;
    }

    if (totalWeight <= 0.0f) return {0, 0, 0};
    return weightedSum / totalWeight;
}

bool IsTopHeavy(const std::vector<Voxel>& voxels) {
    if (voxels.empty()) return false;

    glm::vec3 com = CalculateCenterOfMass(voxels);

    // Find base center
    glm::vec3 baseCenter{0, 0, 0};
    int baseCount = 0;
    int minY = voxels[0].position.y;

    for (const auto& v : voxels) {
        minY = std::min(minY, v.position.y);
    }

    for (const auto& v : voxels) {
        if (v.position.y == minY) {
            baseCenter += glm::vec3(v.position);
            ++baseCount;
        }
    }

    if (baseCount == 0) return true;
    baseCenter /= static_cast<float>(baseCount);

    // Check if center of mass is above base footprint
    float horizontalOffset = glm::length(glm::vec2(com.x - baseCenter.x, com.z - baseCenter.z));

    return horizontalOffset > 2.0f; // Threshold for top-heavy
}

std::vector<glm::ivec3> GetOptimalPillarPositions(const glm::ivec3& min,
                                                   const glm::ivec3& max,
                                                   int maxSpan) {
    std::vector<glm::ivec3> pillars;

    for (int z = min.z; z <= max.z; z += maxSpan) {
        for (int x = min.x; x <= max.x; x += maxSpan) {
            pillars.push_back({x, min.y, z});
        }
    }

    return pillars;
}

} // namespace RTS
} // namespace Vehement
