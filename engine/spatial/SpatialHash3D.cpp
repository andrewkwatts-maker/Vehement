#include "SpatialHash3D.hpp"
#include <cmath>
#include <queue>

namespace Nova {

SpatialHash3D::SpatialHash3D(float cellSize)
    : m_grids(1)
    , m_cells(m_grids[0]) {
    m_config.cellSize = cellSize;
}

SpatialHash3D::SpatialHash3D(const Config& config)
    : m_config(config)
    , m_grids(config.numResolutionLevels)
    , m_cells(m_grids[0]) {
    m_objects.reserve(config.expectedObjectCount);
}

void SpatialHash3D::Insert(uint64_t id, const AABB& bounds, uint64_t layer) {
    if (m_objects.contains(id)) {
        Update(id, bounds);
        return;
    }

    Object obj;
    obj.id = id;
    obj.bounds = bounds;
    obj.layer = layer;
    obj.resolutionLevel = CalculateResolutionLevel(bounds);

    ObjectData data;
    data.object = obj;
    data.cells = GetOverlappingCells(bounds);

    for (const auto& cell : data.cells) {
        InsertIntoCell(cell, id);
    }

    m_objects[id] = std::move(data);
}

bool SpatialHash3D::Remove(uint64_t id) {
    auto it = m_objects.find(id);
    if (it == m_objects.end()) {
        return false;
    }

    for (const auto& cell : it->second.cells) {
        RemoveFromCell(cell, id);
    }

    m_objects.erase(it);
    return true;
}

bool SpatialHash3D::Update(uint64_t id, const AABB& newBounds) {
    auto it = m_objects.find(id);
    if (it == m_objects.end()) {
        return false;
    }

    // Check if object has moved to different cells
    std::vector<CellCoord> newCells = GetOverlappingCells(newBounds);

    // Quick check if cells changed
    bool cellsChanged = newCells.size() != it->second.cells.size();
    if (!cellsChanged) {
        for (size_t i = 0; i < newCells.size(); ++i) {
            if (!(newCells[i] == it->second.cells[i])) {
                cellsChanged = true;
                break;
            }
        }
    }

    if (cellsChanged) {
        // Remove from old cells
        for (const auto& cell : it->second.cells) {
            RemoveFromCell(cell, id);
        }

        // Add to new cells
        for (const auto& cell : newCells) {
            InsertIntoCell(cell, id);
        }

        it->second.cells = std::move(newCells);
    }

    it->second.object.bounds = newBounds;
    return true;
}

void SpatialHash3D::Clear() {
    for (auto& grid : m_grids) {
        grid.clear();
    }
    m_objects.clear();
}

void SpatialHash3D::InsertIntoCell(const CellCoord& cell, uint64_t id) {
    m_cells[cell].objectIds.push_back(id);
}

void SpatialHash3D::RemoveFromCell(const CellCoord& cell, uint64_t id) {
    auto cellIt = m_cells.find(cell);
    if (cellIt == m_cells.end()) return;

    auto& objects = cellIt->second.objectIds;
    auto objIt = std::find(objects.begin(), objects.end(), id);
    if (objIt != objects.end()) {
        // Swap and pop for O(1) removal
        *objIt = objects.back();
        objects.pop_back();
    }

    // Remove empty cells
    if (objects.empty()) {
        m_cells.erase(cellIt);
    }
}

SpatialHash3D::CellCoord SpatialHash3D::PositionToCell(const glm::vec3& pos) const noexcept {
    float invCellSize = 1.0f / m_config.cellSize;
    return CellCoord{
        static_cast<int32_t>(std::floor(pos.x * invCellSize)),
        static_cast<int32_t>(std::floor(pos.y * invCellSize)),
        static_cast<int32_t>(std::floor(pos.z * invCellSize))
    };
}

SpatialHash3D::CellCoord SpatialHash3D::PositionToCell(const glm::vec3& pos, int level) const noexcept {
    float cellSize = GetCellSizeForLevel(level);
    float invCellSize = 1.0f / cellSize;
    return CellCoord{
        static_cast<int32_t>(std::floor(pos.x * invCellSize)),
        static_cast<int32_t>(std::floor(pos.y * invCellSize)),
        static_cast<int32_t>(std::floor(pos.z * invCellSize))
    };
}

std::vector<SpatialHash3D::CellCoord> SpatialHash3D::GetOverlappingCells(const AABB& bounds) const {
    std::vector<CellCoord> cells;

    CellCoord minCell = PositionToCell(bounds.min);
    CellCoord maxCell = PositionToCell(bounds.max);

    cells.reserve(static_cast<size_t>((maxCell.x - minCell.x + 1) *
                                      (maxCell.y - minCell.y + 1) *
                                      (maxCell.z - minCell.z + 1)));

    for (int32_t x = minCell.x; x <= maxCell.x; ++x) {
        for (int32_t y = minCell.y; y <= maxCell.y; ++y) {
            for (int32_t z = minCell.z; z <= maxCell.z; ++z) {
                cells.push_back(CellCoord{x, y, z});
            }
        }
    }

    return cells;
}

int SpatialHash3D::CalculateResolutionLevel(const AABB& bounds) const {
    glm::vec3 size = bounds.GetSize();
    float maxDim = std::max({size.x, size.y, size.z});

    // Choose level where object fits in roughly 1-4 cells
    for (int level = 0; level < m_config.numResolutionLevels; ++level) {
        float cellSize = GetCellSizeForLevel(level);
        if (maxDim <= cellSize * 2.0f) {
            return level;
        }
    }

    return m_config.numResolutionLevels - 1;
}

float SpatialHash3D::GetCellSizeForLevel(int level) const {
    return m_config.cellSize * std::pow(2.0f, static_cast<float>(level));
}

std::vector<uint64_t> SpatialHash3D::QueryAABB(const AABB& query,
                                               const SpatialQueryFilter& filter) {
    std::vector<uint64_t> results;
    m_lastStats.Reset();

    std::unordered_set<uint64_t> testedIds;
    QueryCellsForAABB(query, filter, testedIds);

    results.reserve(testedIds.size());
    for (uint64_t id : testedIds) {
        results.push_back(id);
    }

    m_lastStats.objectsReturned = results.size();
    return results;
}

void SpatialHash3D::QueryCellsForAABB(const AABB& query, const SpatialQueryFilter& filter,
                                      std::unordered_set<uint64_t>& results) const {
    std::vector<CellCoord> cells = GetOverlappingCells(query);

    for (const auto& cell : cells) {
        m_lastStats.nodesVisited++;

        auto cellIt = m_cells.find(cell);
        if (cellIt == m_cells.end()) continue;

        for (uint64_t id : cellIt->second.objectIds) {
            if (results.contains(id)) continue;

            m_lastStats.objectsTested++;

            auto objIt = m_objects.find(id);
            if (objIt == m_objects.end()) continue;

            const Object& obj = objIt->second.object;
            if (!filter.PassesFilter(id, obj.layer)) continue;

            if (obj.bounds.Intersects(query)) {
                results.insert(id);
            }
        }
    }
}

std::vector<uint64_t> SpatialHash3D::QuerySphere(const glm::vec3& center, float radius,
                                                  const SpatialQueryFilter& filter) {
    std::vector<uint64_t> results;
    m_lastStats.Reset();

    AABB sphereAABB = AABB::FromCenterExtents(center, glm::vec3(radius));
    std::vector<CellCoord> cells = GetOverlappingCells(sphereAABB);

    float radius2 = radius * radius;
    std::unordered_set<uint64_t> testedIds;

    for (const auto& cell : cells) {
        m_lastStats.nodesVisited++;

        auto cellIt = m_cells.find(cell);
        if (cellIt == m_cells.end()) continue;

        for (uint64_t id : cellIt->second.objectIds) {
            if (testedIds.contains(id)) continue;
            testedIds.insert(id);

            m_lastStats.objectsTested++;

            auto objIt = m_objects.find(id);
            if (objIt == m_objects.end()) continue;

            const Object& obj = objIt->second.object;
            if (!filter.PassesFilter(id, obj.layer)) continue;

            if (obj.bounds.IntersectsSphere(center, radius)) {
                results.push_back(id);
            }
        }
    }

    m_lastStats.objectsReturned = results.size();
    return results;
}

std::vector<uint64_t> SpatialHash3D::QueryFrustum(const Frustum& frustum,
                                                   const SpatialQueryFilter& filter) {
    std::vector<uint64_t> results;
    m_lastStats.Reset();

    // For frustum, we need to test all objects (spatial hash not ideal for frustum)
    // Could be optimized with frustum AABB first
    AABB frustumAABB;
    auto corners = frustum.GetCorners();
    for (const auto& corner : corners) {
        frustumAABB.Expand(corner);
    }

    std::unordered_set<uint64_t> testedIds;
    QueryCellsForAABB(frustumAABB, filter, testedIds);

    for (uint64_t id : testedIds) {
        auto objIt = m_objects.find(id);
        if (objIt == m_objects.end()) continue;

        if (!frustum.IsAABBOutside(objIt->second.object.bounds)) {
            results.push_back(id);
        }
    }

    m_lastStats.objectsReturned = results.size();
    return results;
}

std::vector<RayHit> SpatialHash3D::QueryRay(const Ray& ray, float maxDist,
                                            const SpatialQueryFilter& filter) {
    std::vector<RayHit> results;
    m_lastStats.Reset();

    // 3D DDA ray marching through grid
    glm::vec3 invDir = ray.GetInverseDirection();

    CellCoord currentCell = PositionToCell(ray.origin);
    glm::vec3 cellMin = glm::vec3(currentCell.x, currentCell.y, currentCell.z) * m_config.cellSize;

    // Step direction
    int stepX = ray.direction.x >= 0 ? 1 : -1;
    int stepY = ray.direction.y >= 0 ? 1 : -1;
    int stepZ = ray.direction.z >= 0 ? 1 : -1;

    // Distance to next cell boundary
    glm::vec3 tMax;
    tMax.x = (ray.direction.x >= 0 ? cellMin.x + m_config.cellSize - ray.origin.x
                                   : ray.origin.x - cellMin.x) * std::abs(invDir.x);
    tMax.y = (ray.direction.y >= 0 ? cellMin.y + m_config.cellSize - ray.origin.y
                                   : ray.origin.y - cellMin.y) * std::abs(invDir.y);
    tMax.z = (ray.direction.z >= 0 ? cellMin.z + m_config.cellSize - ray.origin.z
                                   : ray.origin.z - cellMin.z) * std::abs(invDir.z);

    glm::vec3 tDelta = glm::abs(glm::vec3(m_config.cellSize) * invDir);

    std::unordered_set<uint64_t> testedIds;

    float t = 0.0f;
    while (t < maxDist) {
        m_lastStats.nodesVisited++;

        // Test objects in current cell
        auto cellIt = m_cells.find(currentCell);
        if (cellIt != m_cells.end()) {
            for (uint64_t id : cellIt->second.objectIds) {
                if (testedIds.contains(id)) continue;
                testedIds.insert(id);

                m_lastStats.objectsTested++;

                auto objIt = m_objects.find(id);
                if (objIt == m_objects.end()) continue;

                const Object& obj = objIt->second.object;
                if (!filter.PassesFilter(id, obj.layer)) continue;

                float hitT = obj.bounds.RayIntersect(ray.origin, ray.direction, maxDist);
                if (hitT >= 0.0f && hitT <= maxDist) {
                    RayHit hit;
                    hit.entityId = id;
                    hit.distance = hitT;
                    hit.point = ray.GetPoint(hitT);
                    results.push_back(hit);
                }
            }
        }

        // Step to next cell
        if (tMax.x < tMax.y && tMax.x < tMax.z) {
            t = tMax.x;
            tMax.x += tDelta.x;
            currentCell.x += stepX;
        } else if (tMax.y < tMax.z) {
            t = tMax.y;
            tMax.y += tDelta.y;
            currentCell.y += stepY;
        } else {
            t = tMax.z;
            tMax.z += tDelta.z;
            currentCell.z += stepZ;
        }
    }

    std::sort(results.begin(), results.end());
    m_lastStats.objectsReturned = results.size();
    return results;
}

uint64_t SpatialHash3D::QueryNearest(const glm::vec3& point, float maxDist,
                                      const SpatialQueryFilter& filter) {
    uint64_t nearest = 0;
    float nearestDist2 = maxDist * maxDist;

    // Expand search from center cell outward
    CellCoord centerCell = PositionToCell(point);
    int maxRadius = static_cast<int>(std::ceil(maxDist / m_config.cellSize));

    for (int r = 0; r <= maxRadius; ++r) {
        // Search shell at distance r
        for (int dx = -r; dx <= r; ++dx) {
            for (int dy = -r; dy <= r; ++dy) {
                for (int dz = -r; dz <= r; ++dz) {
                    // Only search cells on the shell
                    if (std::abs(dx) != r && std::abs(dy) != r && std::abs(dz) != r) continue;

                    CellCoord cell{centerCell.x + dx, centerCell.y + dy, centerCell.z + dz};

                    auto cellIt = m_cells.find(cell);
                    if (cellIt == m_cells.end()) continue;

                    for (uint64_t id : cellIt->second.objectIds) {
                        auto objIt = m_objects.find(id);
                        if (objIt == m_objects.end()) continue;

                        const Object& obj = objIt->second.object;
                        if (!filter.PassesFilter(id, obj.layer)) continue;

                        float dist2 = obj.bounds.DistanceSquared(point);
                        if (dist2 < nearestDist2) {
                            nearestDist2 = dist2;
                            nearest = id;
                        }
                    }
                }
            }
        }

        // Early out if we found something within the cell radius
        float shellMinDist = (r - 1) * m_config.cellSize;
        if (nearest != 0 && nearestDist2 <= shellMinDist * shellMinDist) {
            break;
        }
    }

    return nearest;
}

std::vector<uint64_t> SpatialHash3D::QueryKNearest(const glm::vec3& point, size_t k,
                                                    float maxDist,
                                                    const SpatialQueryFilter& filter) {
    // Use priority queue for k nearest
    using DistId = std::pair<float, uint64_t>;
    std::priority_queue<DistId> heap;

    auto results = QuerySphere(point, maxDist, filter);

    for (uint64_t id : results) {
        auto objIt = m_objects.find(id);
        if (objIt == m_objects.end()) continue;

        float dist2 = objIt->second.object.bounds.DistanceSquared(point);
        heap.push({dist2, id});

        if (heap.size() > k) {
            heap.pop();
        }
    }

    std::vector<uint64_t> kNearest;
    kNearest.reserve(heap.size());
    while (!heap.empty()) {
        kNearest.push_back(heap.top().second);
        heap.pop();
    }
    std::reverse(kNearest.begin(), kNearest.end());

    return kNearest;
}

void SpatialHash3D::QueryAABB(const AABB& query, const VisitorCallback& callback,
                              const SpatialQueryFilter& filter) {
    std::vector<CellCoord> cells = GetOverlappingCells(query);
    std::unordered_set<uint64_t> testedIds;

    for (const auto& cell : cells) {
        auto cellIt = m_cells.find(cell);
        if (cellIt == m_cells.end()) continue;

        for (uint64_t id : cellIt->second.objectIds) {
            if (testedIds.contains(id)) continue;
            testedIds.insert(id);

            auto objIt = m_objects.find(id);
            if (objIt == m_objects.end()) continue;

            const Object& obj = objIt->second.object;
            if (!filter.PassesFilter(id, obj.layer)) continue;

            if (obj.bounds.Intersects(query)) {
                if (!callback(id, obj.bounds)) {
                    return;
                }
            }
        }
    }
}

void SpatialHash3D::QuerySphere(const glm::vec3& center, float radius,
                                const VisitorCallback& callback,
                                const SpatialQueryFilter& filter) {
    AABB sphereAABB = AABB::FromCenterExtents(center, glm::vec3(radius));
    std::vector<CellCoord> cells = GetOverlappingCells(sphereAABB);
    std::unordered_set<uint64_t> testedIds;

    for (const auto& cell : cells) {
        auto cellIt = m_cells.find(cell);
        if (cellIt == m_cells.end()) continue;

        for (uint64_t id : cellIt->second.objectIds) {
            if (testedIds.contains(id)) continue;
            testedIds.insert(id);

            auto objIt = m_objects.find(id);
            if (objIt == m_objects.end()) continue;

            const Object& obj = objIt->second.object;
            if (!filter.PassesFilter(id, obj.layer)) continue;

            if (obj.bounds.IntersectsSphere(center, radius)) {
                if (!callback(id, obj.bounds)) {
                    return;
                }
            }
        }
    }
}

AABB SpatialHash3D::GetBounds() const noexcept {
    AABB bounds;
    for (const auto& [id, data] : m_objects) {
        bounds.Expand(data.object.bounds);
    }
    return bounds;
}

size_t SpatialHash3D::GetMemoryUsage() const noexcept {
    size_t memory = 0;

    // Cells
    for (const auto& [coord, cell] : m_cells) {
        memory += sizeof(CellCoord) + sizeof(Cell);
        memory += cell.objectIds.capacity() * sizeof(uint64_t);
    }

    // Objects
    for (const auto& [id, data] : m_objects) {
        memory += sizeof(uint64_t) + sizeof(ObjectData);
        memory += data.cells.capacity() * sizeof(CellCoord);
    }

    return memory;
}

AABB SpatialHash3D::GetObjectBounds(uint64_t id) const noexcept {
    auto it = m_objects.find(id);
    return it != m_objects.end() ? it->second.object.bounds : AABB::Invalid();
}

bool SpatialHash3D::Contains(uint64_t id) const noexcept {
    return m_objects.contains(id);
}

void SpatialHash3D::SetCellSize(float cellSize) {
    if (std::abs(cellSize - m_config.cellSize) < 1e-6f) return;

    // Save all objects
    std::vector<Object> objects;
    objects.reserve(m_objects.size());
    for (const auto& [id, data] : m_objects) {
        objects.push_back(data.object);
    }

    // Clear and rehash
    Clear();
    m_config.cellSize = cellSize;

    for (const auto& obj : objects) {
        Insert(obj.id, obj.bounds, obj.layer);
    }
}

std::vector<uint64_t> SpatialHash3D::GetObjectsInCell(const CellCoord& cell) const {
    auto it = m_cells.find(cell);
    if (it == m_cells.end()) {
        return {};
    }
    return it->second.objectIds;
}

std::vector<uint64_t> SpatialHash3D::QueryNeighbors(const CellCoord& cell,
                                                    const SpatialQueryFilter& filter) const {
    std::vector<uint64_t> results;
    std::unordered_set<uint64_t> addedIds;

    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dz = -1; dz <= 1; ++dz) {
                CellCoord neighbor{cell.x + dx, cell.y + dy, cell.z + dz};

                auto cellIt = m_cells.find(neighbor);
                if (cellIt == m_cells.end()) continue;

                for (uint64_t id : cellIt->second.objectIds) {
                    if (addedIds.contains(id)) continue;

                    auto objIt = m_objects.find(id);
                    if (objIt == m_objects.end()) continue;

                    if (filter.PassesFilter(id, objIt->second.object.layer)) {
                        results.push_back(id);
                        addedIds.insert(id);
                    }
                }
            }
        }
    }

    return results;
}

SpatialHash3D::CellStats SpatialHash3D::GetCellStats() const {
    CellStats stats;
    stats.totalCells = m_cells.size();

    size_t totalObjects = 0;
    for (const auto& [coord, cell] : m_cells) {
        if (cell.objectIds.empty()) {
            stats.emptyCells++;
        } else {
            stats.maxObjectsPerCell = std::max(stats.maxObjectsPerCell, cell.objectIds.size());
            totalObjects += cell.objectIds.size();
        }
    }

    if (stats.totalCells > 0) {
        stats.avgObjectsPerCell = static_cast<float>(totalObjects) / stats.totalCells;
    }

    return stats;
}

void SpatialHash3D::Optimize() {
    // Rehash with optimal bucket count
    // This is a no-op for unordered_map but could be customized
}

// =========================================================================
// HierarchicalSpatialHash Implementation
// =========================================================================

HierarchicalSpatialHash::HierarchicalSpatialHash(float baseCellSize, int levels)
    : m_baseCellSize(baseCellSize) {
    m_levels.reserve(levels);
    for (int i = 0; i < levels; ++i) {
        float cellSize = baseCellSize * std::pow(2.0f, static_cast<float>(i));
        m_levels.push_back(std::make_unique<SpatialHash3D>(cellSize));
    }
}

int HierarchicalSpatialHash::SelectLevel(const AABB& bounds) const {
    glm::vec3 size = bounds.GetSize();
    float maxDim = std::max({size.x, size.y, size.z});

    for (size_t i = 0; i < m_levels.size(); ++i) {
        if (maxDim <= m_levels[i]->GetCellSize() * 2.0f) {
            return static_cast<int>(i);
        }
    }

    return static_cast<int>(m_levels.size() - 1);
}

void HierarchicalSpatialHash::Insert(uint64_t id, const AABB& bounds, uint64_t layer) {
    int level = SelectLevel(bounds);
    m_levels[level]->Insert(id, bounds, layer);
    m_objectLevels[id] = level;
}

bool HierarchicalSpatialHash::Remove(uint64_t id) {
    auto it = m_objectLevels.find(id);
    if (it == m_objectLevels.end()) return false;

    bool removed = m_levels[it->second]->Remove(id);
    m_objectLevels.erase(it);
    return removed;
}

bool HierarchicalSpatialHash::Update(uint64_t id, const AABB& newBounds) {
    auto it = m_objectLevels.find(id);
    if (it == m_objectLevels.end()) return false;

    int newLevel = SelectLevel(newBounds);

    if (newLevel != it->second) {
        // Object changed levels
        uint64_t layer = 0;
        AABB oldBounds = m_levels[it->second]->GetObjectBounds(id);

        m_levels[it->second]->Remove(id);
        m_levels[newLevel]->Insert(id, newBounds, layer);
        it->second = newLevel;
    } else {
        m_levels[it->second]->Update(id, newBounds);
    }

    return true;
}

void HierarchicalSpatialHash::Clear() {
    for (auto& level : m_levels) {
        level->Clear();
    }
    m_objectLevels.clear();
}

std::vector<uint64_t> HierarchicalSpatialHash::QueryAABB(const AABB& query,
                                                          const SpatialQueryFilter& filter) {
    std::vector<uint64_t> results;
    std::unordered_set<uint64_t> addedIds;

    for (auto& level : m_levels) {
        auto levelResults = level->QueryAABB(query, filter);
        for (uint64_t id : levelResults) {
            if (!addedIds.contains(id)) {
                results.push_back(id);
                addedIds.insert(id);
            }
        }
    }

    return results;
}

std::vector<uint64_t> HierarchicalSpatialHash::QuerySphere(const glm::vec3& center,
                                                            float radius,
                                                            const SpatialQueryFilter& filter) {
    std::vector<uint64_t> results;
    std::unordered_set<uint64_t> addedIds;

    for (auto& level : m_levels) {
        auto levelResults = level->QuerySphere(center, radius, filter);
        for (uint64_t id : levelResults) {
            if (!addedIds.contains(id)) {
                results.push_back(id);
                addedIds.insert(id);
            }
        }
    }

    return results;
}

std::vector<uint64_t> HierarchicalSpatialHash::QueryFrustum(const Frustum& frustum,
                                                             const SpatialQueryFilter& filter) {
    std::vector<uint64_t> results;
    std::unordered_set<uint64_t> addedIds;

    for (auto& level : m_levels) {
        auto levelResults = level->QueryFrustum(frustum, filter);
        for (uint64_t id : levelResults) {
            if (!addedIds.contains(id)) {
                results.push_back(id);
                addedIds.insert(id);
            }
        }
    }

    return results;
}

std::vector<RayHit> HierarchicalSpatialHash::QueryRay(const Ray& ray, float maxDist,
                                                       const SpatialQueryFilter& filter) {
    std::vector<RayHit> results;
    std::unordered_set<uint64_t> addedIds;

    for (auto& level : m_levels) {
        auto levelResults = level->QueryRay(ray, maxDist, filter);
        for (const auto& hit : levelResults) {
            if (!addedIds.contains(hit.entityId)) {
                results.push_back(hit);
                addedIds.insert(hit.entityId);
            }
        }
    }

    std::sort(results.begin(), results.end());
    return results;
}

uint64_t HierarchicalSpatialHash::QueryNearest(const glm::vec3& point, float maxDist,
                                                const SpatialQueryFilter& filter) {
    uint64_t nearest = 0;
    float nearestDist2 = maxDist * maxDist;

    for (auto& level : m_levels) {
        uint64_t levelNearest = level->QueryNearest(point, std::sqrt(nearestDist2), filter);
        if (levelNearest != 0) {
            AABB bounds = level->GetObjectBounds(levelNearest);
            float dist2 = bounds.DistanceSquared(point);
            if (dist2 < nearestDist2) {
                nearestDist2 = dist2;
                nearest = levelNearest;
            }
        }
    }

    return nearest;
}

std::vector<uint64_t> HierarchicalSpatialHash::QueryKNearest(const glm::vec3& point, size_t k,
                                                              float maxDist,
                                                              const SpatialQueryFilter& filter) {
    using DistId = std::pair<float, uint64_t>;
    std::priority_queue<DistId> heap;

    for (auto& level : m_levels) {
        auto results = level->QuerySphere(point, maxDist, filter);
        for (uint64_t id : results) {
            float dist2 = level->GetObjectBounds(id).DistanceSquared(point);
            heap.push({dist2, id});
            if (heap.size() > k) heap.pop();
        }
    }

    std::vector<uint64_t> kNearest;
    while (!heap.empty()) {
        kNearest.push_back(heap.top().second);
        heap.pop();
    }
    std::reverse(kNearest.begin(), kNearest.end());

    return kNearest;
}

void HierarchicalSpatialHash::QueryAABB(const AABB& query, const VisitorCallback& callback,
                                        const SpatialQueryFilter& filter) {
    for (auto& level : m_levels) {
        level->QueryAABB(query, callback, filter);
    }
}

void HierarchicalSpatialHash::QuerySphere(const glm::vec3& center, float radius,
                                          const VisitorCallback& callback,
                                          const SpatialQueryFilter& filter) {
    for (auto& level : m_levels) {
        level->QuerySphere(center, radius, callback, filter);
    }
}

size_t HierarchicalSpatialHash::GetObjectCount() const noexcept {
    return m_objectLevels.size();
}

AABB HierarchicalSpatialHash::GetBounds() const noexcept {
    AABB bounds;
    for (const auto& level : m_levels) {
        bounds.Expand(level->GetBounds());
    }
    return bounds;
}

size_t HierarchicalSpatialHash::GetMemoryUsage() const noexcept {
    size_t memory = m_objectLevels.size() * sizeof(std::pair<uint64_t, int>);
    for (const auto& level : m_levels) {
        memory += level->GetMemoryUsage();
    }
    return memory;
}

AABB HierarchicalSpatialHash::GetObjectBounds(uint64_t id) const noexcept {
    auto it = m_objectLevels.find(id);
    if (it == m_objectLevels.end()) return AABB::Invalid();
    return m_levels[it->second]->GetObjectBounds(id);
}

bool HierarchicalSpatialHash::Contains(uint64_t id) const noexcept {
    return m_objectLevels.contains(id);
}

} // namespace Nova
