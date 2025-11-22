#include "WorldBuilding.hpp"
#include "Blueprint.hpp"
#include "StructuralIntegrity.hpp"
#include "../world/TileMap.hpp"
#include <algorithm>
#include <queue>
#include <cmath>
#include <sstream>

namespace Vehement {
namespace RTS {

// ============================================================================
// Voxel Implementation
// ============================================================================

std::string Voxel::ToJson() const {
    std::ostringstream ss;
    ss << "{";
    ss << "\"x\":" << position.x << ",";
    ss << "\"y\":" << position.y << ",";
    ss << "\"z\":" << position.z << ",";
    ss << "\"type\":" << static_cast<int>(type) << ",";
    ss << "\"isWall\":" << (isWall ? "true" : "false") << ",";
    ss << "\"isFloor\":" << (isFloor ? "true" : "false") << ",";
    ss << "\"isRoof\":" << (isRoof ? "true" : "false") << ",";
    ss << "\"isStairs\":" << (isStairs ? "true" : "false") << ",";
    ss << "\"isDoor\":" << (isDoor ? "true" : "false") << ",";
    ss << "\"isWindow\":" << (isWindow ? "true" : "false") << ",";
    ss << "\"dirX\":" << direction.x << ",";
    ss << "\"dirY\":" << direction.y << ",";
    ss << "\"dirZ\":" << direction.z << ",";
    ss << "\"rotation\":" << rotation << ",";
    ss << "\"health\":" << static_cast<int>(health) << ",";
    ss << "\"isSupport\":" << (isSupport ? "true" : "false");
    ss << "}";
    return ss.str();
}

Voxel Voxel::FromJson(const std::string& /* json */) {
    // Simplified - would use proper JSON parsing in production
    Voxel v;
    return v;
}

// ============================================================================
// Voxel3DMap Implementation
// ============================================================================

Voxel3DMap::Voxel3DMap() = default;

void Voxel3DMap::Initialize(int width, int height, int depth) {
    m_width = width;
    m_height = height;
    m_depth = depth;
    m_voxels.clear();
    m_voxelCount = 0;
}

Voxel* Voxel3DMap::GetVoxel(int x, int y, int z) {
    if (!IsInBounds(x, y, z)) return nullptr;

    auto key = GetKey(x, y, z);
    auto it = m_voxels.find(key);
    return (it != m_voxels.end()) ? &it->second : nullptr;
}

const Voxel* Voxel3DMap::GetVoxel(int x, int y, int z) const {
    if (!IsInBounds(x, y, z)) return nullptr;

    auto key = GetKey(x, y, z);
    auto it = m_voxels.find(key);
    return (it != m_voxels.end()) ? &it->second : nullptr;
}

bool Voxel3DMap::SetVoxel(int x, int y, int z, const Voxel& voxel) {
    if (!IsInBounds(x, y, z)) return false;

    auto key = GetKey(x, y, z);
    bool wasNew = (m_voxels.find(key) == m_voxels.end());

    Voxel v = voxel;
    v.position = glm::ivec3(x, y, z);
    m_voxels[key] = v;

    if (wasNew) ++m_voxelCount;
    return true;
}

bool Voxel3DMap::RemoveVoxel(int x, int y, int z) {
    if (!IsInBounds(x, y, z)) return false;

    auto key = GetKey(x, y, z);
    auto it = m_voxels.find(key);
    if (it != m_voxels.end()) {
        m_voxels.erase(it);
        --m_voxelCount;
        return true;
    }
    return false;
}

bool Voxel3DMap::IsInBounds(int x, int y, int z) const {
    return x >= 0 && x < m_width &&
           y >= 0 && y < m_height &&
           z >= 0 && z < m_depth;
}

bool Voxel3DMap::IsSolid(int x, int y, int z) const {
    const Voxel* v = GetVoxel(x, y, z);
    return v != nullptr && v->type != TileType::None;
}

std::vector<Voxel> Voxel3DMap::GetVoxelsInRegion(
    const glm::ivec3& min, const glm::ivec3& max) const {

    std::vector<Voxel> result;

    for (const auto& [key, voxel] : m_voxels) {
        if (voxel.position.x >= min.x && voxel.position.x <= max.x &&
            voxel.position.y >= min.y && voxel.position.y <= max.y &&
            voxel.position.z >= min.z && voxel.position.z <= max.z) {
            result.push_back(voxel);
        }
    }

    return result;
}

void Voxel3DMap::Clear() {
    m_voxels.clear();
    m_voxelCount = 0;
}

// ============================================================================
// Room Furniture Helper
// ============================================================================

std::vector<std::string> GetDefaultFurniture(RoomType type) {
    switch (type) {
        case RoomType::Bedroom:
            return {"bed", "chest", "table", "chair"};
        case RoomType::Kitchen:
            return {"stove", "table", "cabinet", "barrel"};
        case RoomType::Storage:
            return {"crate", "barrel", "shelf"};
        case RoomType::Workshop:
            return {"workbench", "anvil", "tool_rack"};
        case RoomType::Armory:
            return {"weapon_rack", "armor_stand", "chest"};
        case RoomType::Barracks:
            return {"bunk_bed", "chest", "weapon_rack"};
        case RoomType::ThroneRoom:
            return {"throne", "banner", "carpet", "torch"};
        case RoomType::Library:
            return {"bookshelf", "desk", "chair", "candle"};
        case RoomType::Laboratory:
            return {"table", "cauldron", "shelf", "torch"};
        case RoomType::Prison:
            return {"bars", "chain", "bucket"};
        case RoomType::Garden:
            return {"planter", "fountain", "bench"};
        case RoomType::Bathroom:
            return {"tub", "basin"};
        case RoomType::DiningHall:
            return {"long_table", "bench", "chandelier", "banner"};
        default:
            return {};
    }
}

// ============================================================================
// WorldBuilding Implementation
// ============================================================================

WorldBuilding::WorldBuilding()
    : m_blueprintLibrary(std::make_unique<BlueprintLibrary>())
    , m_structuralIntegrity(std::make_unique<StructuralIntegrity>()) {
}

WorldBuilding::~WorldBuilding() {
    Shutdown();
}

bool WorldBuilding::Initialize(TileMap* tileMap, Voxel3DMap* voxelMap) {
    m_tileMap = tileMap;
    m_voxelMap = voxelMap;

    if (!m_tileMap || !m_voxelMap) {
        return false;
    }

    // Initialize terrain heights from tile map
    m_terrainWidth = m_tileMap->GetWidth();
    m_terrainDepth = m_tileMap->GetHeight();
    m_terrainHeights.resize(m_terrainWidth * m_terrainDepth, 0);

    // Initialize subsystems
    m_blueprintLibrary->LoadDefaultBlueprints();
    m_structuralIntegrity->Initialize(m_voxelMap);

    return true;
}

void WorldBuilding::Shutdown() {
    m_buildMode = BuildMode::Off;
    m_tileMap = nullptr;
    m_voxelMap = nullptr;
}

void WorldBuilding::Update(float /* deltaTime */) {
    // Update any in-progress building operations
    // Could handle animated construction here
}

void WorldBuilding::Render(Nova::Renderer& /* renderer */, const Nova::Camera& /* camera */) {
    // Render building previews, ghosts, selection boxes
    // Would draw placement preview in actual rendering
}

// =========================================================================
// Build Mode Control
// =========================================================================

void WorldBuilding::SetBuildMode(BuildMode mode) {
    m_buildMode = mode;
}

void WorldBuilding::ToggleBuildMode() {
    if (m_buildMode == BuildMode::Off) {
        m_buildMode = BuildMode::Place;
    } else {
        m_buildMode = BuildMode::Off;
    }
}

// =========================================================================
// Terrain Modification
// =========================================================================

void WorldBuilding::RaiseTerrain(const glm::ivec3& pos, int amount) {
    if (pos.x < 0 || pos.x >= m_terrainWidth ||
        pos.z < 0 || pos.z >= m_terrainDepth) {
        return;
    }

    int idx = pos.z * m_terrainWidth + pos.x;
    int oldHeight = m_terrainHeights[idx];
    int newHeight = std::min(oldHeight + amount, 32); // Max height 32

    m_terrainHeights[idx] = newHeight;

    if (m_onTerrainChange) {
        m_onTerrainChange(pos, oldHeight, newHeight);
    }
}

void WorldBuilding::LowerTerrain(const glm::ivec3& pos, int amount) {
    if (pos.x < 0 || pos.x >= m_terrainWidth ||
        pos.z < 0 || pos.z >= m_terrainDepth) {
        return;
    }

    int idx = pos.z * m_terrainWidth + pos.x;
    int oldHeight = m_terrainHeights[idx];
    int newHeight = std::max(oldHeight - amount, 0);

    m_terrainHeights[idx] = newHeight;

    if (m_onTerrainChange) {
        m_onTerrainChange(pos, oldHeight, newHeight);
    }
}

void WorldBuilding::FlattenTerrain(const glm::ivec3& pos, int radius) {
    // Calculate average height in radius
    int totalHeight = 0;
    int count = 0;

    for (int dz = -radius; dz <= radius; ++dz) {
        for (int dx = -radius; dx <= radius; ++dx) {
            int x = pos.x + dx;
            int z = pos.z + dz;

            if (x >= 0 && x < m_terrainWidth &&
                z >= 0 && z < m_terrainDepth) {

                // Check if within circular radius
                if (dx * dx + dz * dz <= radius * radius) {
                    totalHeight += m_terrainHeights[z * m_terrainWidth + x];
                    ++count;
                }
            }
        }
    }

    if (count == 0) return;

    int avgHeight = totalHeight / count;
    FlattenTerrainToHeight(pos, radius, avgHeight);
}

void WorldBuilding::FlattenTerrainToHeight(const glm::ivec3& pos, int radius, int targetHeight) {
    for (int dz = -radius; dz <= radius; ++dz) {
        for (int dx = -radius; dx <= radius; ++dx) {
            int x = pos.x + dx;
            int z = pos.z + dz;

            if (x >= 0 && x < m_terrainWidth &&
                z >= 0 && z < m_terrainDepth) {

                if (dx * dx + dz * dz <= radius * radius) {
                    int idx = z * m_terrainWidth + x;
                    int oldHeight = m_terrainHeights[idx];
                    m_terrainHeights[idx] = targetHeight;

                    if (m_onTerrainChange && oldHeight != targetHeight) {
                        m_onTerrainChange({x, 0, z}, oldHeight, targetHeight);
                    }
                }
            }
        }
    }
}

void WorldBuilding::SetTerrainType(const glm::ivec3& pos, TerrainType type) {
    if (!m_tileMap) return;

    TileType tileType = TerrainToTileType(type);
    Tile tile = Tile::Ground(tileType);

    m_tileMap->SetTile(pos.x, pos.z, tile);
}

void WorldBuilding::PaintTerrain(const glm::ivec3& pos, int radius, TerrainType type) {
    for (int dz = -radius; dz <= radius; ++dz) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dz * dz <= radius * radius) {
                SetTerrainType({pos.x + dx, pos.y, pos.z + dz}, type);
            }
        }
    }
}

void WorldBuilding::SmoothTerrain(const glm::ivec3& pos, int radius, float strength) {
    std::vector<int> newHeights = m_terrainHeights;

    for (int dz = -radius; dz <= radius; ++dz) {
        for (int dx = -radius; dx <= radius; ++dx) {
            int x = pos.x + dx;
            int z = pos.z + dz;

            if (x >= 0 && x < m_terrainWidth &&
                z >= 0 && z < m_terrainDepth) {

                if (dx * dx + dz * dz <= radius * radius) {
                    // Calculate average of neighbors
                    int total = 0;
                    int count = 0;

                    for (int nz = -1; nz <= 1; ++nz) {
                        for (int nx = -1; nx <= 1; ++nx) {
                            int nx2 = x + nx;
                            int nz2 = z + nz;

                            if (nx2 >= 0 && nx2 < m_terrainWidth &&
                                nz2 >= 0 && nz2 < m_terrainDepth) {
                                total += m_terrainHeights[nz2 * m_terrainWidth + nx2];
                                ++count;
                            }
                        }
                    }

                    int idx = z * m_terrainWidth + x;
                    int avg = total / count;
                    newHeights[idx] = static_cast<int>(
                        m_terrainHeights[idx] * (1.0f - strength) + avg * strength
                    );
                }
            }
        }
    }

    m_terrainHeights = std::move(newHeights);
}

void WorldBuilding::CreateHill(const glm::ivec3& pos, int radius, int height) {
    for (int dz = -radius; dz <= radius; ++dz) {
        for (int dx = -radius; dx <= radius; ++dx) {
            int x = pos.x + dx;
            int z = pos.z + dz;

            if (x >= 0 && x < m_terrainWidth &&
                z >= 0 && z < m_terrainDepth) {

                float dist = std::sqrt(static_cast<float>(dx * dx + dz * dz));
                if (dist <= radius) {
                    // Smooth falloff
                    float factor = 1.0f - (dist / radius);
                    factor = factor * factor; // Quadratic falloff

                    int addHeight = static_cast<int>(height * factor);
                    RaiseTerrain({x, 0, z}, addHeight);
                }
            }
        }
    }
}

void WorldBuilding::DigHole(const glm::ivec3& pos, int radius, int depth) {
    for (int dz = -radius; dz <= radius; ++dz) {
        for (int dx = -radius; dx <= radius; ++dx) {
            int x = pos.x + dx;
            int z = pos.z + dz;

            if (x >= 0 && x < m_terrainWidth &&
                z >= 0 && z < m_terrainDepth) {

                float dist = std::sqrt(static_cast<float>(dx * dx + dz * dz));
                if (dist <= radius) {
                    float factor = 1.0f - (dist / radius);
                    int subDepth = static_cast<int>(depth * factor);
                    LowerTerrain({x, 0, z}, subDepth);
                }
            }
        }
    }
}

void WorldBuilding::CreateMoat(const glm::ivec3& center, int innerRadius, int outerRadius, int depth) {
    for (int dz = -outerRadius; dz <= outerRadius; ++dz) {
        for (int dx = -outerRadius; dx <= outerRadius; ++dx) {
            int x = center.x + dx;
            int z = center.z + dz;

            if (x >= 0 && x < m_terrainWidth &&
                z >= 0 && z < m_terrainDepth) {

                float dist = std::sqrt(static_cast<float>(dx * dx + dz * dz));

                if (dist >= innerRadius && dist <= outerRadius) {
                    LowerTerrain({x, 0, z}, depth);
                    SetTerrainType({x, 0, z}, TerrainType::Water);
                }
            }
        }
    }
}

// =========================================================================
// Multi-Story Building
// =========================================================================

bool WorldBuilding::PlaceFloor(const glm::ivec3& pos, TileType type) {
    if (!m_voxelMap || !CheckPlacementValid(pos, type)) {
        return false;
    }

    Voxel voxel;
    voxel.position = pos;
    voxel.type = type;
    voxel.isFloor = true;

    PlaceVoxelInternal(pos, voxel);
    return true;
}

bool WorldBuilding::PlaceWall(const glm::ivec3& pos, const glm::ivec3& direction, TileType type) {
    if (!m_voxelMap || !CheckPlacementValid(pos, type)) {
        return false;
    }

    Voxel voxel;
    voxel.position = pos;
    voxel.type = type;
    voxel.isWall = true;
    voxel.direction = direction;

    PlaceVoxelInternal(pos, voxel);
    return true;
}

bool WorldBuilding::PlaceRoof(const glm::ivec3& pos, RoofType type, TileType material) {
    if (!m_voxelMap || !CheckPlacementValid(pos, material)) {
        return false;
    }

    Voxel voxel;
    voxel.position = pos;
    voxel.type = material;
    voxel.isRoof = true;

    // Rotation based on roof type
    switch (type) {
        case RoofType::Peaked:
        case RoofType::Gabled:
            voxel.rotation = 0.0f; // Would be calculated based on orientation
            break;
        default:
            break;
    }

    PlaceVoxelInternal(pos, voxel);
    return true;
}

bool WorldBuilding::PlaceStairs(const glm::ivec3& pos, const glm::ivec3& direction) {
    if (!m_voxelMap) return false;

    Voxel voxel;
    voxel.position = pos;
    voxel.type = m_currentMaterial;
    voxel.isStairs = true;
    voxel.direction = direction;

    PlaceVoxelInternal(pos, voxel);
    return true;
}

bool WorldBuilding::PlaceDoor(const glm::ivec3& wallPos) {
    if (!m_voxelMap) return false;

    Voxel* existing = m_voxelMap->GetVoxel(wallPos);
    if (!existing || !existing->isWall) {
        return false; // Must place door in existing wall
    }

    existing->isDoor = true;
    return true;
}

bool WorldBuilding::PlaceWindow(const glm::ivec3& wallPos) {
    if (!m_voxelMap) return false;

    Voxel* existing = m_voxelMap->GetVoxel(wallPos);
    if (!existing || !existing->isWall) {
        return false; // Must place window in existing wall
    }

    existing->isWindow = true;
    return true;
}

bool WorldBuilding::PlacePillar(const glm::ivec3& pos, int height, TileType type) {
    if (!m_voxelMap) return false;

    for (int y = 0; y < height; ++y) {
        glm::ivec3 pillarPos = pos + glm::ivec3(0, y, 0);

        Voxel voxel;
        voxel.position = pillarPos;
        voxel.type = type;
        voxel.isSupport = true;

        PlaceVoxelInternal(pillarPos, voxel);
    }

    return true;
}

bool WorldBuilding::PlaceRamp(const glm::ivec3& pos, const glm::ivec3& direction, int height) {
    if (!m_voxelMap) return false;

    for (int i = 0; i < height; ++i) {
        glm::ivec3 rampPos = pos + direction * i + glm::ivec3(0, i, 0);

        Voxel voxel;
        voxel.position = rampPos;
        voxel.type = m_currentMaterial;
        voxel.isStairs = true; // Ramps are similar to stairs
        voxel.direction = direction;

        PlaceVoxelInternal(rampPos, voxel);
    }

    return true;
}

bool WorldBuilding::RemoveElement(const glm::ivec3& pos) {
    if (!m_voxelMap) return false;

    // Check structural integrity before removal
    if (m_structuralIntegrity && !m_structuralIntegrity->CanSafelyRemove(pos)) {
        // Would cause collapse - maybe warn player
    }

    bool removed = m_voxelMap->RemoveVoxel(pos);

    if (removed && m_onDemolish) {
        m_onDemolish(pos);
    }

    // Trigger collapse check
    if (m_structuralIntegrity) {
        m_structuralIntegrity->CheckCollapse(pos);
    }

    return removed;
}

// =========================================================================
// Wall Building Tools
// =========================================================================

void WorldBuilding::BuildWallLine(const glm::ivec3& start, const glm::ivec3& end,
                                   TileType type, int height) {
    // Bresenham's line algorithm in 3D (XZ plane)
    int dx = std::abs(end.x - start.x);
    int dz = std::abs(end.z - start.z);
    int sx = (start.x < end.x) ? 1 : -1;
    int sz = (start.z < end.z) ? 1 : -1;
    int err = dx - dz;

    int x = start.x;
    int z = start.z;

    while (true) {
        // Place wall segment at each height level
        for (int y = 0; y < height; ++y) {
            glm::ivec3 wallDir = (dx > dz) ? glm::ivec3(0, 0, 1) : glm::ivec3(1, 0, 0);
            PlaceWall({x, start.y + y, z}, wallDir, type);
        }

        if (x == end.x && z == end.z) break;

        int e2 = 2 * err;
        if (e2 > -dz) {
            err -= dz;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            z += sz;
        }
    }

    ++m_totalStructuresBuilt;
}

void WorldBuilding::BuildWallRect(const glm::ivec3& min, const glm::ivec3& max,
                                   TileType type, int height) {
    // Build four walls
    BuildWallLine({min.x, min.y, min.z}, {max.x, min.y, min.z}, type, height); // South
    BuildWallLine({max.x, min.y, min.z}, {max.x, min.y, max.z}, type, height); // East
    BuildWallLine({max.x, min.y, max.z}, {min.x, min.y, max.z}, type, height); // North
    BuildWallLine({min.x, min.y, max.z}, {min.x, min.y, min.z}, type, height); // West

    ++m_totalStructuresBuilt;
}

void WorldBuilding::BuildWallCircle(const glm::ivec3& center, int radius,
                                     TileType type, int height) {
    // Midpoint circle algorithm
    int x = radius;
    int z = 0;
    int err = 0;

    while (x >= z) {
        // Eight octants
        auto placeAtOctants = [&](int px, int pz) {
            for (int y = 0; y < height; ++y) {
                PlaceWall({center.x + px, center.y + y, center.z + pz}, {0, 0, 1}, type);
                PlaceWall({center.x + pz, center.y + y, center.z + px}, {1, 0, 0}, type);
                PlaceWall({center.x - pz, center.y + y, center.z + px}, {1, 0, 0}, type);
                PlaceWall({center.x - px, center.y + y, center.z + pz}, {0, 0, 1}, type);
                PlaceWall({center.x - px, center.y + y, center.z - pz}, {0, 0, 1}, type);
                PlaceWall({center.x - pz, center.y + y, center.z - px}, {1, 0, 0}, type);
                PlaceWall({center.x + pz, center.y + y, center.z - px}, {1, 0, 0}, type);
                PlaceWall({center.x + px, center.y + y, center.z - pz}, {0, 0, 1}, type);
            }
        };

        placeAtOctants(x, z);

        if (err <= 0) {
            z++;
            err += 2 * z + 1;
        }

        if (err > 0) {
            x--;
            err -= 2 * x + 1;
        }
    }

    ++m_totalStructuresBuilt;
}

// =========================================================================
// Blueprint System
// =========================================================================

bool WorldBuilding::SaveBlueprint(const std::string& name,
                                   const glm::ivec3& min, const glm::ivec3& max) {
    if (!m_blueprintLibrary || !m_voxelMap) return false;

    Blueprint bp;
    bp.name = name;
    bp.size = max - min + glm::ivec3(1);
    bp.voxels = m_voxelMap->GetVoxelsInRegion(min, max);

    // Adjust voxel positions to be relative to blueprint origin
    for (auto& voxel : bp.voxels) {
        voxel.position -= min;
    }

    // Calculate total cost
    for (const auto& voxel : bp.voxels) {
        ResourceCost cost = GetPlacementCost(voxel.position, voxel.type);
        bp.totalCost = bp.totalCost + cost;
    }

    m_blueprintLibrary->SaveUserBlueprint(bp);
    return true;
}

bool WorldBuilding::LoadBlueprint(const std::string& name, const glm::ivec3& pos) {
    if (!m_blueprintLibrary || !m_voxelMap) return false;

    const Blueprint* bp = m_blueprintLibrary->GetBlueprint(name);
    if (!bp) return false;

    // Check if can afford
    if (m_resourceStock && !m_resourceStock->CanAfford(bp->totalCost)) {
        return false;
    }

    // Place all voxels
    for (const auto& voxel : bp->voxels) {
        Voxel placed = voxel;
        placed.position = voxel.position + pos;
        PlaceVoxelInternal(placed.position, placed);
    }

    // Spend resources
    if (m_resourceStock) {
        m_resourceStock->Spend(bp->totalCost);
    }

    ++m_totalStructuresBuilt;
    return true;
}

std::vector<std::string> WorldBuilding::GetSavedBlueprints() const {
    if (!m_blueprintLibrary) return {};
    return m_blueprintLibrary->GetBlueprintNames();
}

bool WorldBuilding::DeleteBlueprint(const std::string& name) {
    if (!m_blueprintLibrary) return false;
    m_blueprintLibrary->DeleteUserBlueprint(name);
    return true;
}

// =========================================================================
// Procedural Building Assistance
// =========================================================================

void WorldBuilding::AutoRoof(const std::vector<glm::ivec3>& wallOutline, RoofType type) {
    if (wallOutline.empty()) return;

    switch (type) {
        case RoofType::Flat:
            GenerateRoofFlat(wallOutline);
            break;
        case RoofType::Peaked:
        case RoofType::Gabled:
            GenerateRoofPeaked(wallOutline);
            break;
        default:
            GenerateRoofFlat(wallOutline);
            break;
    }
}

void WorldBuilding::GenerateRoofFlat(const std::vector<glm::ivec3>& outline) {
    if (outline.empty()) return;

    // Find bounds and height
    glm::ivec3 min = outline[0];
    glm::ivec3 max = outline[0];

    for (const auto& p : outline) {
        min = glm::min(min, p);
        max = glm::max(max, p);
    }

    int roofY = max.y + 1;

    // Fill interior
    FillInterior(outline, roofY, m_currentMaterial);
}

void WorldBuilding::GenerateRoofPeaked(const std::vector<glm::ivec3>& outline) {
    if (outline.empty()) return;

    glm::ivec3 min = outline[0];
    glm::ivec3 max = outline[0];

    for (const auto& p : outline) {
        min = glm::min(min, p);
        max = glm::max(max, p);
    }

    int width = max.x - min.x;
    int depth = max.z - min.z;
    int roofHeight = std::min(width, depth) / 2 + 1;

    // Build peaked roof layer by layer
    for (int layer = 0; layer < roofHeight; ++layer) {
        int y = max.y + 1 + layer;
        int inset = layer;

        for (int z = min.z + inset; z <= max.z - inset; ++z) {
            for (int x = min.x + inset; x <= max.x - inset; ++x) {
                PlaceRoof({x, y, z}, RoofType::Peaked, m_currentMaterial);
            }
        }
    }
}

void WorldBuilding::AutoFloor(const std::vector<glm::ivec3>& wallOutline, TileType type) {
    if (wallOutline.empty()) return;

    // Find the Y level (assuming all walls at same Y)
    int y = wallOutline[0].y;

    FillInterior(wallOutline, y, type);
}

void WorldBuilding::FillInterior(const std::vector<glm::ivec3>& outline, int y, TileType type) {
    if (outline.empty()) return;

    glm::ivec3 min = outline[0];
    glm::ivec3 max = outline[0];

    for (const auto& p : outline) {
        min = glm::min(min, p);
        max = glm::max(max, p);
    }

    // Simple rectangular fill (for complex shapes, use flood fill)
    for (int z = min.z; z <= max.z; ++z) {
        for (int x = min.x; x <= max.x; ++x) {
            PlaceFloor({x, y, z}, type);
        }
    }
}

void WorldBuilding::GenerateRoom(const glm::ivec3& min, const glm::ivec3& max, RoomType type) {
    // Build walls
    int height = max.y - min.y;
    if (height <= 0) height = 3;

    BuildWallRect({min.x, min.y, min.z}, {max.x, min.y, max.z},
                  m_currentMaterial, height);

    // Add floor
    for (int z = min.z + 1; z < max.z; ++z) {
        for (int x = min.x + 1; x < max.x; ++x) {
            PlaceFloor({x, min.y, z}, TileType::WoodFlooring1);
        }
    }

    // Add door on one wall
    glm::ivec3 doorPos = {(min.x + max.x) / 2, min.y, min.z};
    PlaceDoor(doorPos);

    // Add window on opposite wall
    glm::ivec3 windowPos = {(min.x + max.x) / 2, min.y + 1, max.z};
    PlaceWindow(windowPos);

    // Room type specific features would be added here
    // GetDefaultFurniture(type) gives list of furniture to place

    ++m_totalStructuresBuilt;
}

void WorldBuilding::GenerateHouse(const glm::ivec3& pos, int width, int depth, int stories) {
    for (int story = 0; story < stories; ++story) {
        int baseY = pos.y + story * 4; // 4 units per story

        glm::ivec3 min = {pos.x, baseY, pos.z};
        glm::ivec3 max = {pos.x + width, baseY + 3, pos.z + depth};

        // Exterior walls
        BuildWallRect(min, max, TileType::BricksGrey, 3);

        // Interior floor
        for (int z = min.z + 1; z < max.z; ++z) {
            for (int x = min.x + 1; x < max.x; ++x) {
                PlaceFloor({x, baseY, z}, TileType::WoodFlooring1);
            }
        }

        // Add stairs between floors
        if (story < stories - 1) {
            PlaceStairs({pos.x + 1, baseY, pos.z + 1}, {0, 1, 1});
        }
    }

    // Add roof on top
    int topY = pos.y + stories * 4;
    std::vector<glm::ivec3> roofOutline;
    for (int x = pos.x; x <= pos.x + width; ++x) {
        roofOutline.push_back({x, topY - 1, pos.z});
        roofOutline.push_back({x, topY - 1, pos.z + depth});
    }
    AutoRoof(roofOutline, m_currentRoofType);

    ++m_totalStructuresBuilt;
}

void WorldBuilding::GenerateFortification(const glm::ivec3& center, int radius, int wallHeight) {
    // Outer wall circle
    BuildWallCircle(center, radius, TileType::BricksStacked, wallHeight);

    // Corner towers (at 4 cardinal points)
    int towerRadius = 3;
    int towerHeight = wallHeight + 2;

    std::vector<glm::ivec3> towerPositions = {
        {center.x + radius, center.y, center.z},
        {center.x - radius, center.y, center.z},
        {center.x, center.y, center.z + radius},
        {center.x, center.y, center.z - radius}
    };

    for (const auto& towerPos : towerPositions) {
        BuildWallCircle(towerPos, towerRadius, TileType::BricksStacked, towerHeight);
    }

    // Gate (opening in south wall)
    glm::ivec3 gatePos = {center.x, center.y, center.z - radius};
    for (int y = 0; y < wallHeight - 1; ++y) {
        m_voxelMap->RemoveVoxel({gatePos.x, gatePos.y + y, gatePos.z});
        m_voxelMap->RemoveVoxel({gatePos.x + 1, gatePos.y + y, gatePos.z});
    }

    // Moat around the wall
    CreateMoat(center, radius + 2, radius + 5, 2);

    ++m_totalStructuresBuilt;
}

void WorldBuilding::AutoSupport(const glm::ivec3& min, const glm::ivec3& max) {
    if (!m_structuralIntegrity) return;

    // Find all unsupported voxels and add pillars
    int maxSpan = m_structuralIntegrity->GetMaxUnsupportedSpan();

    for (int z = min.z; z <= max.z; z += maxSpan) {
        for (int x = min.x; x <= max.x; x += maxSpan) {
            // Check if needs support at this position
            for (int y = max.y; y >= min.y; --y) {
                const Voxel* v = m_voxelMap->GetVoxel(x, y, z);
                if (v && (v->isFloor || v->isRoof)) {
                    if (!m_structuralIntegrity->HasSupport({x, y, z})) {
                        // Add pillar from ground up
                        PlacePillar({x, min.y, z}, y - min.y, TileType::StoneMarble1);
                    }
                }
            }
        }
    }
}

std::vector<glm::ivec3> WorldBuilding::DetectEnclosedArea(const glm::ivec3& start) {
    std::vector<glm::ivec3> enclosed;

    if (!m_voxelMap) return enclosed;

    // Flood fill to find all connected empty spaces bounded by walls
    std::queue<glm::ivec3> queue;
    std::unordered_map<int64_t, bool> visited;

    auto getKey = [](const glm::ivec3& p) -> int64_t {
        return (static_cast<int64_t>(p.x) << 40) |
               (static_cast<int64_t>(p.y) << 20) |
               static_cast<int64_t>(p.z);
    };

    queue.push(start);
    visited[getKey(start)] = true;

    const glm::ivec3 directions[] = {
        {1, 0, 0}, {-1, 0, 0},
        {0, 0, 1}, {0, 0, -1}
    };

    while (!queue.empty() && enclosed.size() < 10000) { // Limit to prevent infinite
        glm::ivec3 pos = queue.front();
        queue.pop();

        if (!m_voxelMap->IsSolid(pos.x, pos.y, pos.z)) {
            enclosed.push_back(pos);

            for (const auto& dir : directions) {
                glm::ivec3 next = pos + dir;
                int64_t key = getKey(next);

                if (visited.find(key) == visited.end() &&
                    m_voxelMap->IsInBounds(next)) {
                    visited[key] = true;
                    queue.push(next);
                }
            }
        }
    }

    return enclosed;
}

// =========================================================================
// Resource Integration
// =========================================================================

ResourceCost WorldBuilding::GetPlacementCost(const glm::ivec3& /* pos */, TileType type) const {
    ResourceCost cost;

    // Base costs by material category
    if (type >= TileType::Wood1 && type <= TileType::WoodFlooring2) {
        cost.Add(ResourceType::Wood, 2);
    } else if (type >= TileType::StoneBlack && type <= TileType::StoneRaw) {
        cost.Add(ResourceType::Stone, 3);
    } else if (type >= TileType::Metal1 && type <= TileType::MetalShopFrontTop) {
        cost.Add(ResourceType::Metal, 4);
    } else if (type >= TileType::BricksBlack && type <= TileType::BricksCornerBottomRight) {
        cost.Add(ResourceType::Stone, 2);
        cost.Add(ResourceType::Wood, 1);
    } else {
        cost.Add(ResourceType::Wood, 1);
    }

    return cost;
}

ResourceCost WorldBuilding::GetRemovalRefund(const glm::ivec3& pos) const {
    const Voxel* v = m_voxelMap ? m_voxelMap->GetVoxel(pos) : nullptr;
    if (!v) return {};

    // Return 50% of placement cost
    ResourceCost cost = GetPlacementCost(pos, v->type);
    return cost * 0.5f;
}

bool WorldBuilding::CanAfford(const glm::ivec3& pos, TileType type) const {
    if (!m_resourceStock) return true;
    return m_resourceStock->CanAfford(GetPlacementCost(pos, type));
}

// =========================================================================
// Internal Helpers
// =========================================================================

void WorldBuilding::PlaceVoxelInternal(const glm::ivec3& pos, const Voxel& voxel) {
    if (!m_voxelMap) return;

    // Spend resources
    if (m_resourceStock) {
        ResourceCost cost = GetPlacementCost(pos, voxel.type);
        if (!m_resourceStock->Spend(cost)) {
            return; // Can't afford
        }
    }

    m_voxelMap->SetVoxel(pos, voxel);
    ++m_totalVoxelsPlaced;

    if (m_onBuild) {
        m_onBuild(pos, voxel);
    }
}

bool WorldBuilding::CheckPlacementValid(const glm::ivec3& pos, TileType type) const {
    if (!m_voxelMap) return false;

    // Check bounds
    if (!m_voxelMap->IsInBounds(pos)) {
        return false;
    }

    // Check not already occupied
    if (m_voxelMap->IsSolid(pos.x, pos.y, pos.z)) {
        return false;
    }

    // Check can afford
    if (!CanAfford(pos, type)) {
        return false;
    }

    // Check structural validity
    if (m_structuralIntegrity && !WouldBeStable(pos, type)) {
        return false;
    }

    return true;
}

bool WorldBuilding::WouldBeStable(const glm::ivec3& pos, TileType /* type */) const {
    if (!m_structuralIntegrity) return true;

    // Ground level is always stable
    if (pos.y == 0) return true;

    // Check if has support below or adjacent
    return m_structuralIntegrity->HasSupport(pos) ||
           m_voxelMap->IsSolid(pos.x, pos.y - 1, pos.z);
}

WorldBuilding::BuildingStats WorldBuilding::CalculateBuildingStats(
    const glm::ivec3& min, const glm::ivec3& max) const {

    BuildingStats stats;
    stats.minBounds = min;
    stats.maxBounds = max;

    if (!m_voxelMap) return stats;

    auto voxels = m_voxelMap->GetVoxelsInRegion(min, max);

    for (const auto& v : voxels) {
        if (v.isFloor) ++stats.floors;
        if (v.isWall) ++stats.walls;
        if (v.isRoof) ++stats.roofs;
        if (v.isDoor) ++stats.doors;
        if (v.isWindow) ++stats.windows;
        ++stats.totalVolume;
    }

    return stats;
}

} // namespace RTS
} // namespace Vehement
