#include "Voxel3DMap.hpp"
#include <cmath>
#include <fstream>
#include <algorithm>
#include <queue>

namespace Vehement {

// ============================================================================
// Voxel3DMap Implementation
// ============================================================================

Voxel3DMap::Voxel3DMap() {
    m_emptyVoxel = Voxel::Empty();
}

Voxel3DMap::Voxel3DMap(const VoxelConfig& config)
    : m_config(config) {
    m_emptyVoxel = Voxel::Empty();
    Initialize(config);
}

void Voxel3DMap::Initialize(const VoxelConfig& config) {
    m_config = config;

    // Setup hex grid if using hex coordinates
    if (m_config.useHexGrid) {
        m_hexGrid.SetHexSize(m_config.tileSizeXY);
        m_hexGrid.SetOrientation(m_config.hexOrientation);
    }

    // Allocate voxel storage
    EnsureAllocated();

    m_dirty = true;
}

void Voxel3DMap::EnsureAllocated() {
    size_t totalVoxels = static_cast<size_t>(m_config.mapWidth) *
                         static_cast<size_t>(m_config.mapHeight) *
                         static_cast<size_t>(m_config.maxHeight);

    if (m_voxels.size() != totalVoxels) {
        m_voxels.resize(totalVoxels);
        // Initialize all to empty
        for (auto& v : m_voxels) {
            v = Voxel::Empty();
        }
    }
}

void Voxel3DMap::Clear() {
    for (auto& v : m_voxels) {
        v = Voxel::Empty();
    }
    m_largeObjects.clear();
    m_dirty = true;
    m_dirtyRegions.clear();
}

void Voxel3DMap::FillRegion(glm::ivec3 min, glm::ivec3 max, const Voxel& voxel) {
    min = glm::max(min, glm::ivec3(0));
    max = glm::min(max, glm::ivec3(m_config.mapWidth - 1, m_config.mapHeight - 1, m_config.maxHeight - 1));

    for (int z = min.z; z <= max.z; ++z) {
        for (int y = min.y; y <= max.y; ++y) {
            for (int x = min.x; x <= max.x; ++x) {
                GetVoxelInternal(x, y, z) = voxel;
            }
        }
    }

    MarkRegionDirty(min, max);
}

// ============================================================================
// Coordinate Conversion
// ============================================================================

glm::ivec3 Voxel3DMap::WorldToVoxel(const glm::vec3& worldPos) const {
    int x, y, z;

    if (m_config.useHexGrid) {
        // Convert world XZ to hex, then to offset coordinates
        HexCoord hex = m_hexGrid.WorldToHex(glm::vec2(worldPos.x, worldPos.z));
        glm::ivec2 offset = hex.ToOffset(m_config.hexOrientation);
        x = offset.x;
        y = offset.y;
    } else {
        // Direct rectangular conversion
        x = static_cast<int>(std::floor(worldPos.x / m_config.tileSizeXY));
        y = static_cast<int>(std::floor(worldPos.z / m_config.tileSizeXY));
    }

    // Z level from world Y
    z = static_cast<int>(std::floor(worldPos.y / m_config.tileSizeZ));

    return glm::ivec3(x, y, z);
}

glm::vec3 Voxel3DMap::VoxelToWorld(const glm::ivec3& voxelPos) const {
    float worldX, worldZ;

    if (m_config.useHexGrid) {
        // Convert offset to hex, then to world
        HexCoord hex = HexCoord::FromOffset(voxelPos.x, voxelPos.y, m_config.hexOrientation);
        glm::vec2 worldXZ = m_hexGrid.HexToWorld(hex);
        worldX = worldXZ.x;
        worldZ = worldXZ.y;
    } else {
        // Direct rectangular conversion (corner of tile)
        worldX = static_cast<float>(voxelPos.x) * m_config.tileSizeXY;
        worldZ = static_cast<float>(voxelPos.y) * m_config.tileSizeXY;
    }

    float worldY = static_cast<float>(voxelPos.z) * m_config.tileSizeZ;

    return glm::vec3(worldX, worldY, worldZ);
}

glm::vec3 Voxel3DMap::VoxelToWorldCenter(const glm::ivec3& voxelPos) const {
    glm::vec3 corner = VoxelToWorld(voxelPos);

    if (m_config.useHexGrid) {
        // Hex center is already returned by HexToWorld
        return glm::vec3(corner.x, corner.y + m_config.tileSizeZ * 0.5f, corner.z);
    } else {
        // Add half tile size for center
        return glm::vec3(
            corner.x + m_config.tileSizeXY * 0.5f,
            corner.y + m_config.tileSizeZ * 0.5f,
            corner.z + m_config.tileSizeXY * 0.5f
        );
    }
}

glm::ivec3 Voxel3DMap::HexToVoxel(const HexCoord& hex, int zLevel) const {
    glm::ivec2 offset = hex.ToOffset(m_config.hexOrientation);
    return glm::ivec3(offset.x, offset.y, zLevel);
}

HexCoord Voxel3DMap::VoxelToHex(const glm::ivec3& voxelPos) const {
    return HexCoord::FromOffset(voxelPos.x, voxelPos.y, m_config.hexOrientation);
}

int Voxel3DMap::VoxelToZLevel(const glm::ivec3& voxelPos) const {
    return voxelPos.z;
}

// ============================================================================
// Voxel Access
// ============================================================================

size_t Voxel3DMap::GetIndex(int x, int y, int z) const {
    return static_cast<size_t>(z) * m_config.mapWidth * m_config.mapHeight +
           static_cast<size_t>(y) * m_config.mapWidth +
           static_cast<size_t>(x);
}

const Voxel& Voxel3DMap::GetVoxelInternal(int x, int y, int z) const {
    return m_voxels[GetIndex(x, y, z)];
}

Voxel& Voxel3DMap::GetVoxelInternal(int x, int y, int z) {
    return m_voxels[GetIndex(x, y, z)];
}

bool Voxel3DMap::IsInBounds(const glm::ivec3& pos) const {
    return pos.x >= 0 && pos.x < m_config.mapWidth &&
           pos.y >= 0 && pos.y < m_config.mapHeight &&
           pos.z >= 0 && pos.z < m_config.maxHeight;
}

const Voxel& Voxel3DMap::GetVoxel(const glm::ivec3& pos) const {
    if (!IsInBounds(pos)) {
        return m_emptyVoxel;
    }
    return GetVoxelInternal(pos.x, pos.y, pos.z);
}

Voxel& Voxel3DMap::GetVoxel(const glm::ivec3& pos) {
    if (!IsInBounds(pos)) {
        return m_emptyVoxel;
    }
    return GetVoxelInternal(pos.x, pos.y, pos.z);
}

bool Voxel3DMap::SetVoxel(const glm::ivec3& pos, const Voxel& voxel) {
    if (!IsInBounds(pos)) {
        return false;
    }

    GetVoxelInternal(pos.x, pos.y, pos.z) = voxel;
    MarkRegionDirty(pos, pos);
    return true;
}

const Voxel& Voxel3DMap::GetVoxelAtWorld(const glm::vec3& worldPos) const {
    return GetVoxel(WorldToVoxel(worldPos));
}

bool Voxel3DMap::SetVoxelAtWorld(const glm::vec3& worldPos, const Voxel& voxel) {
    return SetVoxel(WorldToVoxel(worldPos), voxel);
}

// ============================================================================
// Floor/Ground Management
// ============================================================================

int Voxel3DMap::GetGroundLevel(const glm::ivec2& xy) const {
    if (xy.x < 0 || xy.x >= m_config.mapWidth || xy.y < 0 || xy.y >= m_config.mapHeight) {
        return -1;
    }

    // Search from top to bottom for highest solid voxel
    for (int z = m_config.maxHeight - 1; z >= 0; --z) {
        const Voxel& v = GetVoxelInternal(xy.x, xy.y, z);
        if (v.isSolid || v.isFloor) {
            return z;
        }
    }

    return -1; // No ground found
}

int Voxel3DMap::GetGroundLevelAtWorld(float worldX, float worldZ) const {
    glm::ivec3 voxelPos = WorldToVoxel(glm::vec3(worldX, 0.0f, worldZ));
    return GetGroundLevel(glm::ivec2(voxelPos.x, voxelPos.y));
}

bool Voxel3DMap::CanStandAt(const glm::ivec3& pos) const {
    if (!IsInBounds(pos)) {
        return false;
    }

    const Voxel& current = GetVoxel(pos);

    // Can't stand inside a solid voxel
    if (current.isSolid) {
        return false;
    }

    // Need floor below or solid below
    if (pos.z > 0) {
        const Voxel& below = GetVoxel(glm::ivec3(pos.x, pos.y, pos.z - 1));
        if (below.isSolid || below.isFloor) {
            return true;
        }
    } else if (pos.z == 0) {
        // Ground level
        return true;
    }

    // Check if current voxel is a floor (platform)
    if (current.isFloor) {
        return true;
    }

    return false;
}

glm::ivec3 Voxel3DMap::GetStandingPosition(const glm::ivec2& xy) const {
    int groundZ = GetGroundLevel(xy);
    if (groundZ < 0) {
        return glm::ivec3(xy.x, xy.y, 0); // Default to ground level
    }

    // Stand on top of the ground voxel
    return glm::ivec3(xy.x, xy.y, groundZ + 1);
}

// ============================================================================
// Large Objects
// ============================================================================

bool Voxel3DMap::CanPlaceLargeObject(const glm::ivec3& origin, const glm::ivec3& size) const {
    // Check bounds
    glm::ivec3 max = origin + size - glm::ivec3(1);
    if (!IsInBounds(origin) || !IsInBounds(max)) {
        return false;
    }

    // Check if all voxels in the area are empty
    for (int z = origin.z; z <= max.z; ++z) {
        for (int y = origin.y; y <= max.y; ++y) {
            for (int x = origin.x; x <= max.x; ++x) {
                const Voxel& v = GetVoxelInternal(x, y, z);
                if (!v.IsEmpty()) {
                    return false;
                }
            }
        }
    }

    return true;
}

uint32_t Voxel3DMap::PlaceLargeObject(const glm::ivec3& origin, const glm::ivec3& size, const Voxel& voxel) {
    if (!CanPlaceLargeObject(origin, size)) {
        return 0;
    }

    // Create large object entry
    LargeObject obj;
    obj.origin = origin;
    obj.size = size;
    obj.baseVoxel = voxel;
    obj.modelId = voxel.modelId;
    obj.objectId = m_nextObjectId++;
    obj.placed = true;

    // Fill voxels with the object
    glm::ivec3 max = origin + size - glm::ivec3(1);
    for (int z = origin.z; z <= max.z; ++z) {
        for (int y = origin.y; y <= max.y; ++y) {
            for (int x = origin.x; x <= max.x; ++x) {
                Voxel& v = GetVoxelInternal(x, y, z);
                v = voxel;
                v.entityId = obj.objectId; // Link voxel to large object
            }
        }
    }

    m_largeObjects.push_back(obj);
    MarkRegionDirty(origin, max);

    return obj.objectId;
}

void Voxel3DMap::RemoveLargeObject(uint32_t objectId) {
    auto it = std::find_if(m_largeObjects.begin(), m_largeObjects.end(),
        [objectId](const LargeObject& obj) { return obj.objectId == objectId; });

    if (it == m_largeObjects.end()) {
        return;
    }

    const LargeObject& obj = *it;
    glm::ivec3 max = obj.origin + obj.size - glm::ivec3(1);

    // Clear voxels
    for (int z = obj.origin.z; z <= max.z; ++z) {
        for (int y = obj.origin.y; y <= max.y; ++y) {
            for (int x = obj.origin.x; x <= max.x; ++x) {
                GetVoxelInternal(x, y, z) = Voxel::Empty();
            }
        }
    }

    MarkRegionDirty(obj.origin, max);
    m_largeObjects.erase(it);
}

// ============================================================================
// Pathfinding Integration
// ============================================================================

bool Voxel3DMap::IsWalkable(const glm::ivec3& pos) const {
    if (!IsInBounds(pos)) {
        return false;
    }

    const Voxel& current = GetVoxel(pos);

    // Can't walk through solid voxels
    if (current.isSolid) {
        return false;
    }

    // Need to be able to stand here
    return CanStandAt(pos);
}

std::vector<glm::ivec3> Voxel3DMap::GetWalkableNeighbors(const glm::ivec3& pos) const {
    std::vector<glm::ivec3> neighbors;
    neighbors.reserve(26); // Max possible neighbors in 3D

    if (m_config.useHexGrid) {
        // Hex grid - 6 horizontal neighbors
        HexCoord hex = VoxelToHex(pos);
        auto hexNeighbors = hex.GetNeighbors();

        for (const auto& neighborHex : hexNeighbors) {
            glm::ivec3 neighborPos = HexToVoxel(neighborHex, pos.z);

            // Same level
            if (IsWalkable(neighborPos) && CanMoveTo(pos, neighborPos)) {
                neighbors.push_back(neighborPos);
            }

            // One level up (stairs/ramps)
            glm::ivec3 upPos(neighborPos.x, neighborPos.y, neighborPos.z + 1);
            if (IsWalkable(upPos) && CanMoveTo(pos, upPos)) {
                neighbors.push_back(upPos);
            }

            // One level down (stairs/ramps)
            glm::ivec3 downPos(neighborPos.x, neighborPos.y, neighborPos.z - 1);
            if (IsWalkable(downPos) && CanMoveTo(pos, downPos)) {
                neighbors.push_back(downPos);
            }
        }
    } else {
        // Rectangular grid - 4 cardinal directions + diagonals
        static const glm::ivec2 cardinals[] = {
            {1, 0}, {-1, 0}, {0, 1}, {0, -1}
        };
        static const glm::ivec2 diagonals[] = {
            {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
        };

        // Cardinal directions
        for (const auto& dir : cardinals) {
            glm::ivec3 neighborPos(pos.x + dir.x, pos.y + dir.y, pos.z);

            if (IsWalkable(neighborPos) && CanMoveTo(pos, neighborPos)) {
                neighbors.push_back(neighborPos);
            }

            // Vertical movement
            glm::ivec3 upPos(neighborPos.x, neighborPos.y, neighborPos.z + 1);
            if (IsWalkable(upPos) && CanMoveTo(pos, upPos)) {
                neighbors.push_back(upPos);
            }

            glm::ivec3 downPos(neighborPos.x, neighborPos.y, neighborPos.z - 1);
            if (IsWalkable(downPos) && CanMoveTo(pos, downPos)) {
                neighbors.push_back(downPos);
            }
        }

        // Diagonal movement (only if both adjacent cardinal directions are walkable)
        for (const auto& dir : diagonals) {
            glm::ivec3 neighborPos(pos.x + dir.x, pos.y + dir.y, pos.z);

            // Check if cardinal directions are clear
            bool canMoveDiag = IsWalkable(glm::ivec3(pos.x + dir.x, pos.y, pos.z)) &&
                              IsWalkable(glm::ivec3(pos.x, pos.y + dir.y, pos.z));

            if (canMoveDiag && IsWalkable(neighborPos) && CanMoveTo(pos, neighborPos)) {
                neighbors.push_back(neighborPos);
            }
        }
    }

    // Vertical movement (ladders, etc.)
    const Voxel& current = GetVoxel(pos);
    if (current.isClimbable) {
        // Can climb up
        glm::ivec3 upPos(pos.x, pos.y, pos.z + 1);
        if (IsInBounds(upPos)) {
            const Voxel& upVoxel = GetVoxel(upPos);
            if (upVoxel.isClimbable || !upVoxel.isSolid) {
                neighbors.push_back(upPos);
            }
        }
    }

    // Can climb down
    if (pos.z > 0) {
        glm::ivec3 downPos(pos.x, pos.y, pos.z - 1);
        const Voxel& downVoxel = GetVoxel(downPos);
        if (downVoxel.isClimbable) {
            neighbors.push_back(downPos);
        }
    }

    return neighbors;
}

float Voxel3DMap::GetMovementCost(const glm::ivec3& from, const glm::ivec3& to) const {
    const Voxel& toVoxel = GetVoxel(to);
    float baseCost = toVoxel.movementCost;

    // Add cost for vertical movement
    int zDiff = std::abs(to.z - from.z);
    if (zDiff > 0) {
        baseCost += static_cast<float>(zDiff) * 0.5f;
    }

    // Add cost for diagonal movement (rectangular grid)
    if (!m_config.useHexGrid) {
        int xDiff = std::abs(to.x - from.x);
        int yDiff = std::abs(to.y - from.y);
        if (xDiff > 0 && yDiff > 0) {
            baseCost *= 1.414f; // sqrt(2) for diagonal
        }
    }

    return baseCost;
}

bool Voxel3DMap::CanMoveTo(const glm::ivec3& from, const glm::ivec3& to) const {
    if (!IsInBounds(to)) {
        return false;
    }

    const Voxel& toVoxel = GetVoxel(to);

    // Can't move into solid voxels
    if (toVoxel.isSolid) {
        return false;
    }

    // Check height difference
    int zDiff = to.z - from.z;

    // Can only go up 1 level at a time (unless climbing)
    if (zDiff > 1) {
        const Voxel& fromVoxel = GetVoxel(from);
        if (!fromVoxel.isClimbable) {
            return false;
        }
    }

    // Can fall any distance (but this affects movement cost/damage elsewhere)
    // For now, limit falling to prevent unrealistic pathing
    if (zDiff < -2) {
        return false;
    }

    return true;
}

// ============================================================================
// Visibility/Line of Sight
// ============================================================================

bool Voxel3DMap::BlocksLight(const glm::ivec3& pos) const {
    if (!IsInBounds(pos)) {
        return false;
    }
    return GetVoxel(pos).blocksLight;
}

bool Voxel3DMap::HasLineOfSight(const glm::ivec3& from, const glm::ivec3& to) const {
    // 3D Bresenham line algorithm
    glm::ivec3 d = to - from;
    glm::ivec3 step(
        d.x > 0 ? 1 : (d.x < 0 ? -1 : 0),
        d.y > 0 ? 1 : (d.y < 0 ? -1 : 0),
        d.z > 0 ? 1 : (d.z < 0 ? -1 : 0)
    );

    d = glm::abs(d);
    int maxDist = std::max({d.x, d.y, d.z});

    if (maxDist == 0) {
        return true; // Same position
    }

    glm::vec3 pos(from);
    glm::vec3 delta(
        static_cast<float>(to.x - from.x) / maxDist,
        static_cast<float>(to.y - from.y) / maxDist,
        static_cast<float>(to.z - from.z) / maxDist
    );

    for (int i = 1; i < maxDist; ++i) {
        pos += delta;
        glm::ivec3 checkPos(
            static_cast<int>(std::round(pos.x)),
            static_cast<int>(std::round(pos.y)),
            static_cast<int>(std::round(pos.z))
        );

        if (BlocksLight(checkPos)) {
            return false;
        }
    }

    return true;
}

std::vector<glm::ivec3> Voxel3DMap::GetVisibleVoxels(const glm::ivec3& origin, int range) const {
    std::vector<glm::ivec3> visible;

    // Check all voxels in a cubic range
    for (int z = origin.z - range; z <= origin.z + range; ++z) {
        for (int y = origin.y - range; y <= origin.y + range; ++y) {
            for (int x = origin.x - range; x <= origin.x + range; ++x) {
                glm::ivec3 pos(x, y, z);

                if (!IsInBounds(pos)) {
                    continue;
                }

                // Check distance
                int dist = std::abs(x - origin.x) + std::abs(y - origin.y) + std::abs(z - origin.z);
                if (dist > range) {
                    continue;
                }

                if (HasLineOfSight(origin, pos)) {
                    visible.push_back(pos);
                }
            }
        }
    }

    return visible;
}

// ============================================================================
// Column/Layer Operations
// ============================================================================

std::vector<const Voxel*> Voxel3DMap::GetColumn(int x, int y) const {
    std::vector<const Voxel*> column;

    if (x < 0 || x >= m_config.mapWidth || y < 0 || y >= m_config.mapHeight) {
        return column;
    }

    column.reserve(m_config.maxHeight);
    for (int z = 0; z < m_config.maxHeight; ++z) {
        column.push_back(&GetVoxelInternal(x, y, z));
    }

    return column;
}

std::vector<std::pair<glm::ivec2, const Voxel*>> Voxel3DMap::GetLayer(int z) const {
    std::vector<std::pair<glm::ivec2, const Voxel*>> layer;

    if (z < 0 || z >= m_config.maxHeight) {
        return layer;
    }

    layer.reserve(m_config.mapWidth * m_config.mapHeight);
    for (int y = 0; y < m_config.mapHeight; ++y) {
        for (int x = 0; x < m_config.mapWidth; ++x) {
            layer.emplace_back(glm::ivec2(x, y), &GetVoxelInternal(x, y, z));
        }
    }

    return layer;
}

void Voxel3DMap::FillLayer(int z, const Voxel& voxel) {
    if (z < 0 || z >= m_config.maxHeight) {
        return;
    }

    for (int y = 0; y < m_config.mapHeight; ++y) {
        for (int x = 0; x < m_config.mapWidth; ++x) {
            GetVoxelInternal(x, y, z) = voxel;
        }
    }

    MarkRegionDirty(
        glm::ivec3(0, 0, z),
        glm::ivec3(m_config.mapWidth - 1, m_config.mapHeight - 1, z)
    );
}

// ============================================================================
// Dirty State Management
// ============================================================================

void Voxel3DMap::MarkRegionDirty(const glm::ivec3& min, const glm::ivec3& max) {
    m_dirty = true;
    m_dirtyRegions.emplace_back(min, max);
}

// ============================================================================
// Serialization
// ============================================================================

nlohmann::json Voxel3DMap::ToJson(bool sparseMode) const {
    nlohmann::json j;

    // Config
    j["config"]["tileSizeXY"] = m_config.tileSizeXY;
    j["config"]["tileSizeZ"] = m_config.tileSizeZ;
    j["config"]["maxHeight"] = m_config.maxHeight;
    j["config"]["useHexGrid"] = m_config.useHexGrid;
    j["config"]["hexOrientation"] = m_config.hexOrientation == HexOrientation::FlatTop ? "FlatTop" : "PointyTop";
    j["config"]["mapWidth"] = m_config.mapWidth;
    j["config"]["mapHeight"] = m_config.mapHeight;

    // Voxels
    if (sparseMode) {
        // Only save non-empty voxels
        nlohmann::json voxels = nlohmann::json::array();

        for (int z = 0; z < m_config.maxHeight; ++z) {
            for (int y = 0; y < m_config.mapHeight; ++y) {
                for (int x = 0; x < m_config.mapWidth; ++x) {
                    const Voxel& v = GetVoxelInternal(x, y, z);
                    if (!v.IsEmpty()) {
                        nlohmann::json voxelJson = v.ToJson();
                        voxelJson["x"] = x;
                        voxelJson["y"] = y;
                        voxelJson["z"] = z;
                        voxels.push_back(voxelJson);
                    }
                }
            }
        }

        j["voxels"] = voxels;
        j["sparseMode"] = true;
    } else {
        // Save all voxels (dense mode)
        nlohmann::json voxels = nlohmann::json::array();

        for (const auto& v : m_voxels) {
            voxels.push_back(v.ToJson());
        }

        j["voxels"] = voxels;
        j["sparseMode"] = false;
    }

    // Large objects
    nlohmann::json objects = nlohmann::json::array();
    for (const auto& obj : m_largeObjects) {
        nlohmann::json objJson;
        objJson["origin"] = {obj.origin.x, obj.origin.y, obj.origin.z};
        objJson["size"] = {obj.size.x, obj.size.y, obj.size.z};
        objJson["baseVoxel"] = obj.baseVoxel.ToJson();
        objJson["objectId"] = obj.objectId;
        objects.push_back(objJson);
    }
    j["largeObjects"] = objects;
    j["nextObjectId"] = m_nextObjectId;

    return j;
}

void Voxel3DMap::FromJson(const nlohmann::json& j) {
    // Load config
    if (j.contains("config")) {
        auto& cfg = j["config"];
        if (cfg.contains("tileSizeXY")) m_config.tileSizeXY = cfg["tileSizeXY"];
        if (cfg.contains("tileSizeZ")) m_config.tileSizeZ = cfg["tileSizeZ"];
        if (cfg.contains("maxHeight")) m_config.maxHeight = cfg["maxHeight"];
        if (cfg.contains("useHexGrid")) m_config.useHexGrid = cfg["useHexGrid"];
        if (cfg.contains("hexOrientation")) {
            std::string orient = cfg["hexOrientation"];
            m_config.hexOrientation = (orient == "FlatTop") ? HexOrientation::FlatTop : HexOrientation::PointyTop;
        }
        if (cfg.contains("mapWidth")) m_config.mapWidth = cfg["mapWidth"];
        if (cfg.contains("mapHeight")) m_config.mapHeight = cfg["mapHeight"];
    }

    // Initialize with loaded config
    EnsureAllocated();
    Clear();

    // Load voxels
    if (j.contains("voxels")) {
        bool sparseMode = j.value("sparseMode", true);

        if (sparseMode) {
            // Sparse mode - voxels have position embedded
            for (const auto& voxelJson : j["voxels"]) {
                int x = voxelJson["x"];
                int y = voxelJson["y"];
                int z = voxelJson["z"];

                if (IsInBounds(glm::ivec3(x, y, z))) {
                    GetVoxelInternal(x, y, z) = Voxel::FromJson(voxelJson);
                }
            }
        } else {
            // Dense mode - all voxels in order
            size_t i = 0;
            for (const auto& voxelJson : j["voxels"]) {
                if (i < m_voxels.size()) {
                    m_voxels[i] = Voxel::FromJson(voxelJson);
                }
                ++i;
            }
        }
    }

    // Load large objects
    if (j.contains("largeObjects")) {
        for (const auto& objJson : j["largeObjects"]) {
            LargeObject obj;
            auto& origin = objJson["origin"];
            obj.origin = glm::ivec3(origin[0], origin[1], origin[2]);
            auto& size = objJson["size"];
            obj.size = glm::ivec3(size[0], size[1], size[2]);
            obj.baseVoxel = Voxel::FromJson(objJson["baseVoxel"]);
            obj.objectId = objJson["objectId"];
            obj.placed = true;
            m_largeObjects.push_back(obj);
        }
    }

    if (j.contains("nextObjectId")) {
        m_nextObjectId = j["nextObjectId"];
    }

    m_dirty = true;
}

bool Voxel3DMap::SaveToFile(const std::string& filepath) const {
    try {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            return false;
        }

        file << ToJson(true).dump(2);
        return true;
    } catch (...) {
        return false;
    }
}

bool Voxel3DMap::LoadFromFile(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return false;
        }

        nlohmann::json j;
        file >> j;
        FromJson(j);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace Vehement
