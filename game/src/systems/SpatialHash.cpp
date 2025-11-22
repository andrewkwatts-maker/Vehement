#include "SpatialHash.hpp"
#include <cmath>
#include <algorithm>
#include <queue>

namespace Vehement {
namespace Systems {

// ============================================================================
// SpatialHashGrid Implementation
// ============================================================================

SpatialHashGrid::SpatialHashGrid(const Config& config)
    : m_config(config)
    , m_invCellSize(1.0f / config.cellSize)
{
    m_entries.reserve(config.expectedEntityCount);
    m_cells.reserve(config.expectedEntityCount / config.maxEntitiesPerCell + 1);
}

void SpatialHashGrid::Insert(EntityId id, const glm::vec3& position, float radius, bool isStatic) {
    if (m_entries.count(id) > 0) {
        Update(id, position);
        return;
    }

    SpatialEntry entry;
    entry.entityId = id;
    entry.position = position;
    entry.radius = radius;
    entry.isStatic = isStatic;
    entry.isDirty = false;
    entry.lastUpdateFrame = 0;

    m_entries[id] = entry;

    // Add to cell
    CellKey key = GetCellKey(position);
    m_cells[key].entityIds.push_back(id);
}

bool SpatialHashGrid::Remove(EntityId id) {
    auto it = m_entries.find(id);
    if (it == m_entries.end()) {
        return false;
    }

    // Remove from cell
    CellKey key = GetCellKey(it->second.position);
    auto cellIt = m_cells.find(key);
    if (cellIt != m_cells.end()) {
        auto& ids = cellIt->second.entityIds;
        ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());

        // Clean up empty cells
        if (ids.empty()) {
            m_cells.erase(cellIt);
        }
    }

    // Remove from dirty list if present
    m_dirtyStatics.erase(
        std::remove(m_dirtyStatics.begin(), m_dirtyStatics.end(), id),
        m_dirtyStatics.end()
    );

    m_entries.erase(it);
    return true;
}

void SpatialHashGrid::Update(EntityId id, const glm::vec3& newPosition) {
    auto it = m_entries.find(id);
    if (it == m_entries.end()) {
        return;
    }

    SpatialEntry& entry = it->second;
    CellKey oldKey = GetCellKey(entry.position);
    CellKey newKey = GetCellKey(newPosition);

    // Lazy update for static entities
    if (entry.isStatic && m_config.trackStaticEntities) {
        entry.isDirty = true;
        entry.position = newPosition;

        if (oldKey != newKey) {
            // Only add to dirty list once
            if (std::find(m_dirtyStatics.begin(), m_dirtyStatics.end(), id) == m_dirtyStatics.end()) {
                m_dirtyStatics.push_back(id);
            }
        }
        return;
    }

    // Immediate update for dynamic entities
    if (oldKey != newKey) {
        // Remove from old cell
        auto cellIt = m_cells.find(oldKey);
        if (cellIt != m_cells.end()) {
            auto& ids = cellIt->second.entityIds;
            ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
            if (ids.empty()) {
                m_cells.erase(cellIt);
            }
        }

        // Add to new cell
        m_cells[newKey].entityIds.push_back(id);
    }

    entry.position = newPosition;
}

void SpatialHashGrid::FlushDirtyStatics() {
    for (EntityId id : m_dirtyStatics) {
        auto it = m_entries.find(id);
        if (it == m_entries.end()) continue;

        SpatialEntry& entry = it->second;
        if (!entry.isDirty) continue;

        // Re-insert to correct cell
        CellKey oldKey = GetCellKey(entry.position);

        // Find and remove from current cell
        for (auto& [key, cell] : m_cells) {
            auto pos = std::find(cell.entityIds.begin(), cell.entityIds.end(), id);
            if (pos != cell.entityIds.end()) {
                cell.entityIds.erase(pos);
                break;
            }
        }

        // Add to correct cell
        CellKey newKey = GetCellKey(entry.position);
        m_cells[newKey].entityIds.push_back(id);
        entry.isDirty = false;
    }

    m_dirtyStatics.clear();
}

void SpatialHashGrid::Clear() {
    m_entries.clear();
    m_cells.clear();
    m_dirtyStatics.clear();
}

void SpatialHashGrid::QueryRadius(const glm::vec3& center, float radius,
                                   std::vector<EntityId>& result,
                                   EntityId excludeId) const {
    result.clear();
    float radiusSq = radius * radius;

    int minX, maxX, minY, maxY, minZ, maxZ;
    GetCellRange(center, radius, minX, maxX, minY, maxY, minZ, maxZ);

    for (int x = minX; x <= maxX; ++x) {
        for (int y = minY; y <= maxY; ++y) {
            for (int z = minZ; z <= maxZ; ++z) {
                CellKey key = GetCellKey(x, y, z);
                auto cellIt = m_cells.find(key);
                if (cellIt == m_cells.end()) continue;

                for (EntityId id : cellIt->second.entityIds) {
                    if (id == excludeId) continue;

                    auto entryIt = m_entries.find(id);
                    if (entryIt == m_entries.end()) continue;

                    const SpatialEntry& entry = entryIt->second;
                    glm::vec3 diff = entry.position - center;
                    float distSq = glm::dot(diff, diff);
                    float combinedRadius = radius + entry.radius;

                    if (distSq <= combinedRadius * combinedRadius) {
                        result.push_back(id);
                    }
                }
            }
        }
    }

    // Remove duplicates (entity can span multiple cells)
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
}

void SpatialHashGrid::QueryAABB(const AABB& bounds, std::vector<EntityId>& result) const {
    result.clear();

    int minX = static_cast<int>(std::floor(bounds.min.x * m_invCellSize));
    int maxX = static_cast<int>(std::floor(bounds.max.x * m_invCellSize));
    int minY = static_cast<int>(std::floor(bounds.min.y * m_invCellSize));
    int maxY = static_cast<int>(std::floor(bounds.max.y * m_invCellSize));
    int minZ = static_cast<int>(std::floor(bounds.min.z * m_invCellSize));
    int maxZ = static_cast<int>(std::floor(bounds.max.z * m_invCellSize));

    for (int x = minX; x <= maxX; ++x) {
        for (int y = minY; y <= maxY; ++y) {
            for (int z = minZ; z <= maxZ; ++z) {
                CellKey key = GetCellKey(x, y, z);
                auto cellIt = m_cells.find(key);
                if (cellIt == m_cells.end()) continue;

                for (EntityId id : cellIt->second.entityIds) {
                    auto entryIt = m_entries.find(id);
                    if (entryIt == m_entries.end()) continue;

                    if (bounds.Contains(entryIt->second.position)) {
                        result.push_back(id);
                    }
                }
            }
        }
    }

    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
}

EntityId SpatialHashGrid::FindNearest(const glm::vec3& position,
                                       float maxDistance,
                                       EntityId excludeId) const {
    EntityId nearest = INVALID_ENTITY_ID;
    float nearestDistSq = maxDistance * maxDistance;

    // Expand search outward from center
    int cellRadius = static_cast<int>(std::ceil(maxDistance * m_invCellSize));
    int cx = static_cast<int>(std::floor(position.x * m_invCellSize));
    int cy = static_cast<int>(std::floor(position.y * m_invCellSize));
    int cz = static_cast<int>(std::floor(position.z * m_invCellSize));

    for (int r = 0; r <= cellRadius; ++r) {
        for (int x = cx - r; x <= cx + r; ++x) {
            for (int y = cy - r; y <= cy + r; ++y) {
                for (int z = cz - r; z <= cz + r; ++z) {
                    // Only check boundary cells of this radius
                    if (r > 0 && std::abs(x - cx) < r &&
                        std::abs(y - cy) < r && std::abs(z - cz) < r) {
                        continue;
                    }

                    CellKey key = GetCellKey(x, y, z);
                    auto cellIt = m_cells.find(key);
                    if (cellIt == m_cells.end()) continue;

                    for (EntityId id : cellIt->second.entityIds) {
                        if (id == excludeId) continue;

                        auto entryIt = m_entries.find(id);
                        if (entryIt == m_entries.end()) continue;

                        glm::vec3 diff = entryIt->second.position - position;
                        float distSq = glm::dot(diff, diff);

                        if (distSq < nearestDistSq) {
                            nearestDistSq = distSq;
                            nearest = id;
                        }
                    }
                }
            }
        }

        // Early out if we found something and searched far enough
        if (nearest != INVALID_ENTITY_ID &&
            r * m_config.cellSize > std::sqrt(nearestDistSq) + m_config.cellSize) {
            break;
        }
    }

    return nearest;
}

void SpatialHashGrid::FindKNearest(const glm::vec3& position, size_t k,
                                    std::vector<EntityId>& result,
                                    float maxDistance,
                                    EntityId excludeId) const {
    result.clear();

    struct DistEntry {
        float distSq;
        EntityId id;
        bool operator>(const DistEntry& other) const { return distSq > other.distSq; }
    };

    std::priority_queue<DistEntry, std::vector<DistEntry>, std::greater<DistEntry>> pq;
    float maxDistSq = maxDistance * maxDistance;

    // Search all entities (could be optimized with expanding search)
    for (const auto& [id, entry] : m_entries) {
        if (id == excludeId) continue;

        glm::vec3 diff = entry.position - position;
        float distSq = glm::dot(diff, diff);

        if (distSq <= maxDistSq) {
            pq.push({distSq, id});
        }
    }

    // Extract k nearest
    while (!pq.empty() && result.size() < k) {
        result.push_back(pq.top().id);
        pq.pop();
    }
}

bool SpatialHashGrid::HasEntityInRadius(const glm::vec3& center, float radius,
                                         EntityId excludeId) const {
    float radiusSq = radius * radius;

    int minX, maxX, minY, maxY, minZ, maxZ;
    GetCellRange(center, radius, minX, maxX, minY, maxY, minZ, maxZ);

    for (int x = minX; x <= maxX; ++x) {
        for (int y = minY; y <= maxY; ++y) {
            for (int z = minZ; z <= maxZ; ++z) {
                CellKey key = GetCellKey(x, y, z);
                auto cellIt = m_cells.find(key);
                if (cellIt == m_cells.end()) continue;

                for (EntityId id : cellIt->second.entityIds) {
                    if (id == excludeId) continue;

                    auto entryIt = m_entries.find(id);
                    if (entryIt == m_entries.end()) continue;

                    glm::vec3 diff = entryIt->second.position - center;
                    if (glm::dot(diff, diff) <= radiusSq) {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

void SpatialHashGrid::GetPotentialCollisions(EntityId id, std::vector<EntityId>& result) const {
    result.clear();

    auto it = m_entries.find(id);
    if (it == m_entries.end()) return;

    const SpatialEntry& entry = it->second;
    QueryRadius(entry.position, entry.radius * 2.0f, result, id);
}

void SpatialHashGrid::ForEachInRadius(const glm::vec3& center, float radius,
                                       std::function<bool(EntityId, const glm::vec3&, float)> callback) const {
    float radiusSq = radius * radius;

    int minX, maxX, minY, maxY, minZ, maxZ;
    GetCellRange(center, radius, minX, maxX, minY, maxY, minZ, maxZ);

    std::unordered_set<EntityId> visited;

    for (int x = minX; x <= maxX; ++x) {
        for (int y = minY; y <= maxY; ++y) {
            for (int z = minZ; z <= maxZ; ++z) {
                CellKey key = GetCellKey(x, y, z);
                auto cellIt = m_cells.find(key);
                if (cellIt == m_cells.end()) continue;

                for (EntityId id : cellIt->second.entityIds) {
                    if (visited.count(id) > 0) continue;
                    visited.insert(id);

                    auto entryIt = m_entries.find(id);
                    if (entryIt == m_entries.end()) continue;

                    const SpatialEntry& entry = entryIt->second;
                    glm::vec3 diff = entry.position - center;

                    if (glm::dot(diff, diff) <= radiusSq) {
                        if (!callback(id, entry.position, entry.radius)) {
                            return;
                        }
                    }
                }
            }
        }
    }
}

float SpatialHashGrid::GetAverageCellOccupancy() const {
    if (m_cells.empty()) return 0.0f;

    size_t total = 0;
    for (const auto& [key, cell] : m_cells) {
        total += cell.entityIds.size();
    }
    return static_cast<float>(total) / m_cells.size();
}

void SpatialHashGrid::Optimize() {
    // Remove empty cells
    for (auto it = m_cells.begin(); it != m_cells.end();) {
        if (it->second.entityIds.empty()) {
            it = m_cells.erase(it);
        } else {
            ++it;
        }
    }

    // Shrink vectors that are much larger than needed
    for (auto& [key, cell] : m_cells) {
        if (cell.entityIds.capacity() > cell.entityIds.size() * 4) {
            cell.entityIds.shrink_to_fit();
        }
    }

    FlushDirtyStatics();
}

SpatialHashGrid::CellKey SpatialHashGrid::GetCellKey(const glm::vec3& position) const {
    int x = static_cast<int>(std::floor(position.x * m_invCellSize));
    int y = static_cast<int>(std::floor(position.y * m_invCellSize));
    int z = static_cast<int>(std::floor(position.z * m_invCellSize));
    return GetCellKey(x, y, z);
}

SpatialHashGrid::CellKey SpatialHashGrid::GetCellKey(int x, int y, int z) const {
    // Pack into 64-bit key: 21 bits for x, 21 bits for z, 21 bits for y
    // This allows ~2 million cells in each dimension
    return (static_cast<int64_t>(x & 0x1FFFFF) << 42) |
           (static_cast<int64_t>(z & 0x1FFFFF) << 21) |
           (static_cast<int64_t>(y & 0x1FFFFF));
}

void SpatialHashGrid::GetCellRange(const glm::vec3& center, float radius,
                                    int& minX, int& maxX, int& minY, int& maxY,
                                    int& minZ, int& maxZ) const {
    minX = static_cast<int>(std::floor((center.x - radius) * m_invCellSize));
    maxX = static_cast<int>(std::floor((center.x + radius) * m_invCellSize));
    minY = static_cast<int>(std::floor((center.y - radius) * m_invCellSize));
    maxY = static_cast<int>(std::floor((center.y + radius) * m_invCellSize));
    minZ = static_cast<int>(std::floor((center.z - radius) * m_invCellSize));
    maxZ = static_cast<int>(std::floor((center.z + radius) * m_invCellSize));
}

// ============================================================================
// Quadtree Implementation
// ============================================================================

Quadtree::Quadtree(const AABB& bounds, const Config& config)
    : m_config(config)
    , m_root(std::make_unique<Node>())
{
    m_root->bounds = bounds;
    m_root->depth = 0;
}

Quadtree::~Quadtree() = default;

void Quadtree::Insert(EntityId id, const glm::vec3& position, float radius) {
    // Store position and radius
    m_entityPositions[id] = position;
    m_entityRadii[id] = radius;

    InsertIntoNode(m_root.get(), id, position, radius);
    ++m_entityCount;
}

bool Quadtree::Remove(EntityId id) {
    auto it = m_entityPositions.find(id);
    if (it == m_entityPositions.end()) {
        return false;
    }

    RemoveFromNode(m_root.get(), id);
    m_entityPositions.erase(it);
    m_entityRadii.erase(id);
    --m_entityCount;

    return true;
}

void Quadtree::Update(EntityId id, const glm::vec3& newPosition) {
    auto it = m_entityPositions.find(id);
    if (it == m_entityPositions.end()) {
        return;
    }

    float radius = m_entityRadii[id];
    RemoveFromNode(m_root.get(), id);
    it->second = newPosition;
    InsertIntoNode(m_root.get(), id, newPosition, radius);
}

void Quadtree::Clear() {
    m_root = std::make_unique<Node>();
    m_root->bounds = m_root->bounds;  // Preserve bounds
    m_root->depth = 0;
    m_entityPositions.clear();
    m_entityRadii.clear();
    m_entityCount = 0;
}

void Quadtree::QueryRadius(const glm::vec3& center, float radius,
                            std::vector<EntityId>& result) const {
    result.clear();
    float radiusSq = radius * radius;
    QueryRadiusNode(m_root.get(), center, radiusSq, result);
}

void Quadtree::QueryAABB(const AABB& bounds, std::vector<EntityId>& result) const {
    result.clear();
    QueryAABBNode(m_root.get(), bounds, result);
}

EntityId Quadtree::FindNearest(const glm::vec3& position, float maxDistance) const {
    EntityId nearest = INVALID_ENTITY_ID;
    float nearestDistSq = maxDistance * maxDistance;

    for (const auto& [id, pos] : m_entityPositions) {
        glm::vec3 diff = pos - position;
        float distSq = glm::dot(diff, diff);
        if (distSq < nearestDistSq) {
            nearestDistSq = distSq;
            nearest = id;
        }
    }

    return nearest;
}

size_t Quadtree::GetNodeCount() const {
    return CountNodes(m_root.get());
}

size_t Quadtree::GetMaxDepth() const {
    return GetMaxDepthRecursive(m_root.get());
}

void Quadtree::Rebuild() {
    // Store all entities
    auto positions = std::move(m_entityPositions);
    auto radii = std::move(m_entityRadii);

    // Clear and re-insert
    AABB bounds = m_root->bounds;
    m_root = std::make_unique<Node>();
    m_root->bounds = bounds;
    m_root->depth = 0;
    m_entityCount = 0;

    for (const auto& [id, pos] : positions) {
        m_entityPositions[id] = pos;
        m_entityRadii[id] = radii[id];
        InsertIntoNode(m_root.get(), id, pos, radii[id]);
        ++m_entityCount;
    }
}

void Quadtree::InsertIntoNode(Node* node, EntityId id, const glm::vec3& position, float radius) {
    if (node->IsLeaf()) {
        node->entities.push_back(id);

        // Split if needed
        if (node->entities.size() > m_config.maxEntitiesPerNode &&
            node->depth < m_config.maxDepth) {
            float nodeSize = node->bounds.max.x - node->bounds.min.x;
            if (nodeSize > m_config.minNodeSize * 2.0f) {
                SplitNode(node);
            }
        }
    } else {
        // Find which quadrant(s) the entity belongs to
        glm::vec3 center = node->bounds.GetCenter();

        // Determine quadrant based on XZ position
        int quadrant = 0;
        if (position.x >= center.x) quadrant |= 1;  // East
        if (position.z >= center.z) quadrant |= 2;  // South

        InsertIntoNode(node->children[quadrant].get(), id, position, radius);
    }
}

void Quadtree::RemoveFromNode(Node* node, EntityId id) {
    if (node->IsLeaf()) {
        auto& entities = node->entities;
        entities.erase(std::remove(entities.begin(), entities.end(), id), entities.end());
    } else {
        // Check all children
        for (auto& child : node->children) {
            if (child) {
                RemoveFromNode(child.get(), id);
            }
        }

        // Merge if children are mostly empty
        MergeNode(node);
    }
}

void Quadtree::QueryRadiusNode(const Node* node, const glm::vec3& center,
                                float radiusSq, std::vector<EntityId>& result) const {
    // Check if node bounds intersect with query sphere (approximate with AABB)
    float radius = std::sqrt(radiusSq);
    AABB queryBounds(center, radius);

    if (!node->bounds.Intersects(queryBounds)) {
        return;
    }

    if (node->IsLeaf()) {
        for (EntityId id : node->entities) {
            auto it = m_entityPositions.find(id);
            if (it == m_entityPositions.end()) continue;

            glm::vec3 diff = it->second - center;
            if (glm::dot(diff, diff) <= radiusSq) {
                result.push_back(id);
            }
        }
    } else {
        for (const auto& child : node->children) {
            if (child) {
                QueryRadiusNode(child.get(), center, radiusSq, result);
            }
        }
    }
}

void Quadtree::QueryAABBNode(const Node* node, const AABB& bounds,
                              std::vector<EntityId>& result) const {
    if (!node->bounds.Intersects(bounds)) {
        return;
    }

    if (node->IsLeaf()) {
        for (EntityId id : node->entities) {
            auto it = m_entityPositions.find(id);
            if (it == m_entityPositions.end()) continue;

            if (bounds.Contains(it->second)) {
                result.push_back(id);
            }
        }
    } else {
        for (const auto& child : node->children) {
            if (child) {
                QueryAABBNode(child.get(), bounds, result);
            }
        }
    }
}

void Quadtree::SplitNode(Node* node) {
    glm::vec3 center = node->bounds.GetCenter();
    glm::vec3 halfSize = node->bounds.GetSize() * 0.25f;

    // Create four children (NW, NE, SW, SE based on XZ)
    for (int i = 0; i < 4; ++i) {
        node->children[i] = std::make_unique<Node>();
        node->children[i]->depth = node->depth + 1;

        float offsetX = (i & 1) ? halfSize.x : -halfSize.x;
        float offsetZ = (i & 2) ? halfSize.z : -halfSize.z;

        glm::vec3 childCenter = center + glm::vec3(offsetX, 0.0f, offsetZ);
        node->children[i]->bounds = AABB(
            glm::vec3(childCenter.x - halfSize.x, node->bounds.min.y, childCenter.z - halfSize.z),
            glm::vec3(childCenter.x + halfSize.x, node->bounds.max.y, childCenter.z + halfSize.z)
        );
    }

    // Redistribute entities
    for (EntityId id : node->entities) {
        auto it = m_entityPositions.find(id);
        if (it == m_entityPositions.end()) continue;

        int quadrant = 0;
        if (it->second.x >= center.x) quadrant |= 1;
        if (it->second.z >= center.z) quadrant |= 2;

        node->children[quadrant]->entities.push_back(id);
    }

    node->entities.clear();
    node->entities.shrink_to_fit();
}

void Quadtree::MergeNode(Node* node) {
    if (node->IsLeaf()) return;

    // Count total entities in children
    size_t total = 0;
    for (const auto& child : node->children) {
        if (child && child->IsLeaf()) {
            total += child->entities.size();
        } else if (child && !child->IsLeaf()) {
            return;  // Don't merge if any child has children
        }
    }

    // Merge if below threshold
    if (total <= m_config.maxEntitiesPerNode / 2) {
        node->entities.reserve(total);
        for (auto& child : node->children) {
            if (child) {
                node->entities.insert(node->entities.end(),
                                      child->entities.begin(),
                                      child->entities.end());
                child.reset();
            }
        }
    }
}

size_t Quadtree::CountNodes(const Node* node) const {
    if (!node) return 0;

    size_t count = 1;
    for (const auto& child : node->children) {
        count += CountNodes(child.get());
    }
    return count;
}

size_t Quadtree::GetMaxDepthRecursive(const Node* node) const {
    if (!node || node->IsLeaf()) {
        return node ? node->depth : 0;
    }

    size_t maxDepth = node->depth;
    for (const auto& child : node->children) {
        maxDepth = std::max(maxDepth, GetMaxDepthRecursive(child.get()));
    }
    return maxDepth;
}

// ============================================================================
// HybridSpatialSystem Implementation
// ============================================================================

HybridSpatialSystem::HybridSpatialSystem(const Config& config)
    : m_config(config)
    , m_dynamicGrid(config.gridConfig)
{
    if (config.useQuadtreeForStatics) {
        m_staticTree = std::make_unique<Quadtree>(config.worldBounds, config.treeConfig);
    }
}

void HybridSpatialSystem::Insert(EntityId id, const glm::vec3& position, float radius, bool isStatic) {
    if (isStatic && m_staticTree && m_config.useQuadtreeForStatics) {
        m_staticTree->Insert(id, position, radius);
        m_staticEntities.insert(id);
    } else {
        m_dynamicGrid.Insert(id, position, radius, false);
    }
}

bool HybridSpatialSystem::Remove(EntityId id) {
    if (m_staticEntities.count(id) > 0) {
        m_staticEntities.erase(id);
        return m_staticTree ? m_staticTree->Remove(id) : false;
    }
    return m_dynamicGrid.Remove(id);
}

void HybridSpatialSystem::Update(EntityId id, const glm::vec3& newPosition) {
    if (m_staticEntities.count(id) > 0 && m_staticTree) {
        m_staticTree->Update(id, newPosition);
    } else {
        m_dynamicGrid.Update(id, newPosition);
    }
}

void HybridSpatialSystem::MarkStatic(EntityId id) {
    // This would require moving entity between structures
    // For now, just track it
    m_staticEntities.insert(id);
}

void HybridSpatialSystem::MarkDynamic(EntityId id) {
    m_staticEntities.erase(id);
}

void HybridSpatialSystem::Clear() {
    m_dynamicGrid.Clear();
    if (m_staticTree) {
        m_staticTree->Clear();
    }
    m_staticEntities.clear();
}

void HybridSpatialSystem::QueryRadius(const glm::vec3& center, float radius,
                                       std::vector<EntityId>& result,
                                       EntityId excludeId) const {
    m_dynamicGrid.QueryRadius(center, radius, result, excludeId);

    if (m_staticTree) {
        std::vector<EntityId> staticResult;
        m_staticTree->QueryRadius(center, radius, staticResult);

        for (EntityId id : staticResult) {
            if (id != excludeId) {
                result.push_back(id);
            }
        }
    }
}

void HybridSpatialSystem::QueryAABB(const AABB& bounds, std::vector<EntityId>& result) const {
    m_dynamicGrid.QueryAABB(bounds, result);

    if (m_staticTree) {
        std::vector<EntityId> staticResult;
        m_staticTree->QueryAABB(bounds, staticResult);
        result.insert(result.end(), staticResult.begin(), staticResult.end());
    }
}

EntityId HybridSpatialSystem::FindNearest(const glm::vec3& position,
                                           float maxDistance,
                                           EntityId excludeId) const {
    EntityId nearestDynamic = m_dynamicGrid.FindNearest(position, maxDistance, excludeId);

    if (!m_staticTree) {
        return nearestDynamic;
    }

    EntityId nearestStatic = m_staticTree->FindNearest(position, maxDistance);
    if (nearestStatic == excludeId) {
        nearestStatic = INVALID_ENTITY_ID;
    }

    if (nearestDynamic == INVALID_ENTITY_ID) return nearestStatic;
    if (nearestStatic == INVALID_ENTITY_ID) return nearestDynamic;

    // Compare distances (would need position lookup)
    return nearestDynamic;  // Simplified
}

void HybridSpatialSystem::GetPotentialCollisions(EntityId id, std::vector<EntityId>& result) const {
    m_dynamicGrid.GetPotentialCollisions(id, result);

    // For static entities, we'd need to query the quadtree as well
    // This is simplified
}

void HybridSpatialSystem::BatchInsert(const std::vector<std::tuple<EntityId, glm::vec3, float, bool>>& entities) {
    for (const auto& [id, pos, radius, isStatic] : entities) {
        Insert(id, pos, radius, isStatic);
    }
}

void HybridSpatialSystem::BatchUpdate(const std::vector<std::pair<EntityId, glm::vec3>>& updates) {
    for (const auto& [id, pos] : updates) {
        Update(id, pos);
    }
}

void HybridSpatialSystem::Optimize() {
    m_dynamicGrid.Optimize();
    if (m_staticTree) {
        m_staticTree->Rebuild();
    }
}

void HybridSpatialSystem::Flush() {
    m_dynamicGrid.FlushDirtyStatics();
}

// ============================================================================
// CollisionLayers Implementation
// ============================================================================

void CollisionLayers::SetLayerCollision(LayerMask layer, LayerMask canCollideWith) {
    m_collisionMatrix[layer] = canCollideWith;
}

bool CollisionLayers::CanCollide(LayerMask layerA, LayerMask layerB) const {
    auto itA = m_collisionMatrix.find(layerA);
    if (itA != m_collisionMatrix.end()) {
        if (!(itA->second & layerB)) return false;
    }

    auto itB = m_collisionMatrix.find(layerB);
    if (itB != m_collisionMatrix.end()) {
        if (!(itB->second & layerA)) return false;
    }

    return true;
}

CollisionLayers::LayerMask CollisionLayers::GetCollisionMask(LayerMask layer) const {
    auto it = m_collisionMatrix.find(layer);
    return it != m_collisionMatrix.end() ? it->second : LAYER_ALL;
}

} // namespace Systems
} // namespace Vehement
