#pragma once

#include "Tile.hpp"
#include "HexGrid.hpp"
#include "WorldConfig.hpp"
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <nlohmann/json.hpp>

namespace Vehement {

/**
 * @brief Configuration for the 3D voxel map
 */
struct VoxelConfig {
    float tileSizeXY = 1.0f;       // World meters per tile in X/Y directions
    float tileSizeZ = 0.333f;      // World meters per tile in Z (1/3 of X/Y default)
    int maxHeight = 32;            // Max Z levels
    bool useHexGrid = true;        // Use hex grid (true) or rectangular grid (false)
    HexOrientation hexOrientation = HexOrientation::PointyTop;

    // Map dimensions
    int mapWidth = 256;            // Width in tiles
    int mapHeight = 256;           // Height in tiles (depth in world)

    /**
     * @brief Create VoxelConfig from WorldConfig
     */
    static VoxelConfig FromWorldConfig(const WorldConfig& worldConfig) {
        VoxelConfig config;
        config.tileSizeXY = worldConfig.tileSizeXY;
        config.tileSizeZ = worldConfig.tileSizeZ;
        config.maxHeight = worldConfig.maxZLevels;
        config.useHexGrid = worldConfig.useHexGrid;
        config.hexOrientation = worldConfig.hexOrientation;
        config.mapWidth = worldConfig.mapWidth;
        config.mapHeight = worldConfig.mapHeight;
        return config;
    }
};

/**
 * @brief Represents a single voxel in 3D space
 *
 * A voxel is a 3D extension of a tile, containing information about
 * what occupies that space, its properties, and optional 3D model data.
 */
struct Voxel {
    TileType type = TileType::None;      // Base tile type (texture/appearance)
    uint8_t variant = 0;                  // Texture variant

    // Physical properties
    bool isSolid = false;                 // Blocks movement through this voxel
    bool blocksLight = false;             // Blocks visibility/light
    bool isFloor = false;                 // Can stand on top of this voxel
    bool isCeiling = false;               // Has ceiling above (blocks upward movement)
    bool isClimbable = false;             // Can climb (ladders, etc.)
    bool isTransparent = false;           // Partially see-through (windows, etc.)

    // Movement properties
    float movementCost = 1.0f;            // Cost multiplier for pathfinding
    bool isWalkable = true;               // Can walk on/through
    bool isSwimmable = false;             // Can swim through (water)
    bool isDamaging = false;              // Causes damage
    float damagePerSecond = 0.0f;         // Damage amount

    // 3D model override
    int modelId = -1;                     // 3D model ID (-1 = use tile texture)
    glm::vec3 modelScale = {1.0f, 1.0f, 1.0f};
    glm::quat modelRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Identity
    glm::vec3 modelOffset = {0.0f, 0.0f, 0.0f};  // Offset from voxel center

    // Lighting
    float lightEmission = 0.0f;           // How much light this voxel emits (0-1)
    glm::vec3 lightColor = {1.0f, 1.0f, 1.0f}; // Light color if emitting

    // Metadata
    uint32_t entityId = 0;                // Associated entity ID (0 = none)
    uint16_t flags = 0;                   // Custom game flags

    // ========== Factory Methods ==========

    /**
     * @brief Create an empty (air) voxel
     */
    static Voxel Empty() {
        Voxel v;
        v.type = TileType::None;
        v.isWalkable = true;
        return v;
    }

    /**
     * @brief Create a solid block voxel
     */
    static Voxel Solid(TileType tileType) {
        Voxel v;
        v.type = tileType;
        v.isSolid = true;
        v.blocksLight = true;
        v.isFloor = true;
        v.isWalkable = false;
        return v;
    }

    /**
     * @brief Create a floor voxel (walkable surface)
     */
    static Voxel Floor(TileType tileType) {
        Voxel v;
        v.type = tileType;
        v.isSolid = false;
        v.isFloor = true;
        v.isWalkable = true;
        return v;
    }

    /**
     * @brief Create a wall voxel
     */
    static Voxel Wall(TileType tileType) {
        Voxel v;
        v.type = tileType;
        v.isSolid = true;
        v.blocksLight = true;
        v.isFloor = false;
        v.isWalkable = false;
        return v;
    }

    /**
     * @brief Create a water voxel
     */
    static Voxel Water() {
        Voxel v;
        v.type = TileType::Water1;
        v.isSolid = false;
        v.isFloor = false;
        v.isWalkable = false;
        v.isSwimmable = true;
        v.isTransparent = true;
        v.movementCost = 2.0f;
        return v;
    }

    /**
     * @brief Create a window voxel (transparent but solid)
     */
    static Voxel Window(TileType tileType) {
        Voxel v;
        v.type = tileType;
        v.isSolid = true;
        v.blocksLight = false;
        v.isTransparent = true;
        v.isWalkable = false;
        return v;
    }

    /**
     * @brief Create a ladder voxel
     */
    static Voxel Ladder(TileType tileType) {
        Voxel v;
        v.type = tileType;
        v.isSolid = false;
        v.isClimbable = true;
        v.isWalkable = true;
        return v;
    }

    /**
     * @brief Create a ceiling voxel
     */
    static Voxel Ceiling(TileType tileType) {
        Voxel v;
        v.type = tileType;
        v.isSolid = false;
        v.isCeiling = true;
        v.isWalkable = true;  // Can walk under it
        return v;
    }

    /**
     * @brief Create a voxel with a 3D model
     */
    static Voxel Model(int modelId, bool solid = false) {
        Voxel v;
        v.type = TileType::None;  // Model overrides texture
        v.modelId = modelId;
        v.isSolid = solid;
        v.isWalkable = !solid;
        return v;
    }

    // ========== Queries ==========

    /**
     * @brief Check if this voxel uses a 3D model instead of tile texture
     */
    bool HasModel() const { return modelId >= 0; }

    /**
     * @brief Check if this voxel is empty (air)
     */
    bool IsEmpty() const { return type == TileType::None && modelId < 0; }

    /**
     * @brief Check if movement is blocked through this voxel
     */
    bool BlocksMovement() const { return isSolid || (isCeiling && !isClimbable); }

    // ========== Serialization ==========

    nlohmann::json ToJson() const {
        nlohmann::json j;
        j["type"] = static_cast<int>(type);
        j["variant"] = variant;
        j["isSolid"] = isSolid;
        j["blocksLight"] = blocksLight;
        j["isFloor"] = isFloor;
        j["isCeiling"] = isCeiling;
        j["isClimbable"] = isClimbable;
        j["isTransparent"] = isTransparent;
        j["movementCost"] = movementCost;
        j["modelId"] = modelId;
        if (modelId >= 0) {
            j["modelScale"] = {modelScale.x, modelScale.y, modelScale.z};
            j["modelRotation"] = {modelRotation.w, modelRotation.x, modelRotation.y, modelRotation.z};
            j["modelOffset"] = {modelOffset.x, modelOffset.y, modelOffset.z};
        }
        if (lightEmission > 0.0f) {
            j["lightEmission"] = lightEmission;
            j["lightColor"] = {lightColor.x, lightColor.y, lightColor.z};
        }
        if (entityId != 0) j["entityId"] = entityId;
        if (flags != 0) j["flags"] = flags;
        return j;
    }

    static Voxel FromJson(const nlohmann::json& j) {
        Voxel v;
        if (j.contains("type")) v.type = static_cast<TileType>(j["type"].get<int>());
        if (j.contains("variant")) v.variant = j["variant"];
        if (j.contains("isSolid")) v.isSolid = j["isSolid"];
        if (j.contains("blocksLight")) v.blocksLight = j["blocksLight"];
        if (j.contains("isFloor")) v.isFloor = j["isFloor"];
        if (j.contains("isCeiling")) v.isCeiling = j["isCeiling"];
        if (j.contains("isClimbable")) v.isClimbable = j["isClimbable"];
        if (j.contains("isTransparent")) v.isTransparent = j["isTransparent"];
        if (j.contains("movementCost")) v.movementCost = j["movementCost"];
        if (j.contains("modelId")) v.modelId = j["modelId"];
        if (j.contains("modelScale")) {
            auto& arr = j["modelScale"];
            v.modelScale = glm::vec3(arr[0], arr[1], arr[2]);
        }
        if (j.contains("modelRotation")) {
            auto& arr = j["modelRotation"];
            v.modelRotation = glm::quat(arr[0], arr[1], arr[2], arr[3]);
        }
        if (j.contains("modelOffset")) {
            auto& arr = j["modelOffset"];
            v.modelOffset = glm::vec3(arr[0], arr[1], arr[2]);
        }
        if (j.contains("lightEmission")) v.lightEmission = j["lightEmission"];
        if (j.contains("lightColor")) {
            auto& arr = j["lightColor"];
            v.lightColor = glm::vec3(arr[0], arr[1], arr[2]);
        }
        if (j.contains("entityId")) v.entityId = j["entityId"];
        if (j.contains("flags")) v.flags = j["flags"];
        return v;
    }
};

/**
 * @brief 3D chunk for large map support
 */
struct VoxelChunk {
    static constexpr int CHUNK_SIZE_XY = 16;  // 16x16 tiles per chunk
    static constexpr int CHUNK_SIZE_Z = 8;    // 8 Z levels per chunk

    glm::ivec3 chunkPos{0, 0, 0};  // Chunk position (in chunk coordinates)
    std::vector<Voxel> voxels;     // CHUNK_SIZE_XY * CHUNK_SIZE_XY * CHUNK_SIZE_Z voxels
    bool dirty = false;            // Needs re-rendering
    bool loaded = false;
    bool modified = false;         // Has been modified since load

    VoxelChunk() : voxels(CHUNK_SIZE_XY * CHUNK_SIZE_XY * CHUNK_SIZE_Z) {}

    /**
     * @brief Get voxel at local chunk coordinates
     */
    Voxel& GetVoxel(int localX, int localY, int localZ) {
        return voxels[GetIndex(localX, localY, localZ)];
    }

    const Voxel& GetVoxel(int localX, int localY, int localZ) const {
        return voxels[GetIndex(localX, localY, localZ)];
    }

    /**
     * @brief Calculate flat array index from 3D coordinates
     */
    static int GetIndex(int x, int y, int z) {
        return z * CHUNK_SIZE_XY * CHUNK_SIZE_XY + y * CHUNK_SIZE_XY + x;
    }

    /**
     * @brief Get chunk key for hashmap storage
     */
    static int64_t GetChunkKey(int chunkX, int chunkY, int chunkZ) {
        // Pack into 64 bits: 21 bits each for x, y, z (signed)
        int64_t key = 0;
        key |= (static_cast<int64_t>(chunkX & 0x1FFFFF)) << 42;
        key |= (static_cast<int64_t>(chunkY & 0x1FFFFF)) << 21;
        key |= (static_cast<int64_t>(chunkZ & 0x1FFFFF));
        return key;
    }
};

/**
 * @brief Large object placement information
 */
struct LargeObject {
    glm::ivec3 origin{0, 0, 0};    // Origin voxel position
    glm::ivec3 size{1, 1, 1};      // Size in voxels
    Voxel baseVoxel;               // Voxel template for this object
    int modelId = -1;              // 3D model ID
    bool placed = false;
    uint32_t objectId = 0;
};

/**
 * @brief 3D voxel map for multi-story buildings
 *
 * Provides a full 3D grid of voxels with support for:
 * - Multi-story buildings (Z levels)
 * - Hex or rectangular grid in XY plane
 * - Large objects spanning multiple voxels
 * - Efficient chunk-based storage
 * - Pathfinding integration
 * - Firebase serialization
 */
class Voxel3DMap {
public:
    Voxel3DMap();
    explicit Voxel3DMap(const VoxelConfig& config);
    ~Voxel3DMap() = default;

    // Non-copyable
    Voxel3DMap(const Voxel3DMap&) = delete;
    Voxel3DMap& operator=(const Voxel3DMap&) = delete;
    Voxel3DMap(Voxel3DMap&&) noexcept = default;
    Voxel3DMap& operator=(Voxel3DMap&&) noexcept = default;

    // ========== Initialization ==========

    /**
     * @brief Initialize the voxel map with given configuration
     */
    void Initialize(const VoxelConfig& config);

    /**
     * @brief Clear all voxels
     */
    void Clear();

    /**
     * @brief Fill a region with a voxel type
     */
    void FillRegion(glm::ivec3 min, glm::ivec3 max, const Voxel& voxel);

    // ========== Coordinate Conversion ==========

    /**
     * @brief Convert world position to voxel coordinates
     */
    glm::ivec3 WorldToVoxel(const glm::vec3& worldPos) const;

    /**
     * @brief Convert voxel coordinates to world position (corner)
     */
    glm::vec3 VoxelToWorld(const glm::ivec3& voxelPos) const;

    /**
     * @brief Convert voxel coordinates to world position (center)
     */
    glm::vec3 VoxelToWorldCenter(const glm::ivec3& voxelPos) const;

    /**
     * @brief Convert hex coordinate + Z level to voxel position
     */
    glm::ivec3 HexToVoxel(const HexCoord& hex, int zLevel) const;

    /**
     * @brief Convert voxel position to hex coordinate + Z level
     */
    HexCoord VoxelToHex(const glm::ivec3& voxelPos) const;
    int VoxelToZLevel(const glm::ivec3& voxelPos) const;

    // ========== Voxel Access ==========

    /**
     * @brief Get voxel at position (returns default empty if out of bounds)
     */
    const Voxel& GetVoxel(const glm::ivec3& pos) const;
    Voxel& GetVoxel(const glm::ivec3& pos);

    /**
     * @brief Set voxel at position
     * @return true if successful, false if out of bounds
     */
    bool SetVoxel(const glm::ivec3& pos, const Voxel& voxel);

    /**
     * @brief Get voxel at world position
     */
    const Voxel& GetVoxelAtWorld(const glm::vec3& worldPos) const;

    /**
     * @brief Set voxel at world position
     */
    bool SetVoxelAtWorld(const glm::vec3& worldPos, const Voxel& voxel);

    /**
     * @brief Check if position is within bounds
     */
    bool IsInBounds(const glm::ivec3& pos) const;

    // ========== Floor/Ground Management ==========

    /**
     * @brief Get the highest solid voxel Z level at XY position
     * @param xy XY position (uses x, y from ivec2)
     * @return Z level of highest solid voxel, or -1 if none
     */
    int GetGroundLevel(const glm::ivec2& xy) const;

    /**
     * @brief Get ground level at world XZ position
     */
    int GetGroundLevelAtWorld(float worldX, float worldZ) const;

    /**
     * @brief Check if an entity can stand at a position
     * @param pos Voxel position (feet position)
     * @return true if standing is possible
     */
    bool CanStandAt(const glm::ivec3& pos) const;

    /**
     * @brief Get the standing position above ground at XY
     * @param xy XY position
     * @return Voxel position where entity can stand
     */
    glm::ivec3 GetStandingPosition(const glm::ivec2& xy) const;

    // ========== Large Objects ==========

    /**
     * @brief Place a large object spanning multiple voxels
     * @param origin Origin voxel position
     * @param size Size in voxels (x, y, z)
     * @param voxel Voxel template for the object
     * @return Object ID, or 0 if placement failed
     */
    uint32_t PlaceLargeObject(const glm::ivec3& origin, const glm::ivec3& size, const Voxel& voxel);

    /**
     * @brief Remove a large object
     * @param objectId Object ID from PlaceLargeObject
     */
    void RemoveLargeObject(uint32_t objectId);

    /**
     * @brief Check if a large object can be placed
     */
    bool CanPlaceLargeObject(const glm::ivec3& origin, const glm::ivec3& size) const;

    /**
     * @brief Get all large objects
     */
    const std::vector<LargeObject>& GetLargeObjects() const { return m_largeObjects; }

    // ========== Pathfinding Integration ==========

    /**
     * @brief Check if a voxel position is walkable
     */
    bool IsWalkable(const glm::ivec3& pos) const;

    /**
     * @brief Get walkable neighbors for 3D pathfinding
     * @param pos Current position
     * @return Vector of walkable neighbor positions
     */
    std::vector<glm::ivec3> GetWalkableNeighbors(const glm::ivec3& pos) const;

    /**
     * @brief Get movement cost between adjacent voxels
     */
    float GetMovementCost(const glm::ivec3& from, const glm::ivec3& to) const;

    /**
     * @brief Check if can move from one voxel to another
     */
    bool CanMoveTo(const glm::ivec3& from, const glm::ivec3& to) const;

    // ========== Visibility/Line of Sight ==========

    /**
     * @brief Check if a voxel blocks light
     */
    bool BlocksLight(const glm::ivec3& pos) const;

    /**
     * @brief Check line of sight between two voxel positions
     */
    bool HasLineOfSight(const glm::ivec3& from, const glm::ivec3& to) const;

    /**
     * @brief Get visible voxels from a position within range
     */
    std::vector<glm::ivec3> GetVisibleVoxels(const glm::ivec3& origin, int range) const;

    // ========== Column/Layer Operations ==========

    /**
     * @brief Get all voxels in a vertical column
     */
    std::vector<const Voxel*> GetColumn(int x, int y) const;

    /**
     * @brief Get a horizontal slice at a Z level
     */
    std::vector<std::pair<glm::ivec2, const Voxel*>> GetLayer(int z) const;

    /**
     * @brief Fill an entire Z level with a voxel type
     */
    void FillLayer(int z, const Voxel& voxel);

    // ========== Iteration ==========

    /**
     * @brief Iterate over all non-empty voxels
     */
    template<typename Func>
    void ForEachVoxel(Func&& func) const {
        for (int z = 0; z < m_config.maxHeight; ++z) {
            for (int y = 0; y < m_config.mapHeight; ++y) {
                for (int x = 0; x < m_config.mapWidth; ++x) {
                    const Voxel& v = GetVoxelInternal(x, y, z);
                    if (!v.IsEmpty()) {
                        func(glm::ivec3(x, y, z), v);
                    }
                }
            }
        }
    }

    /**
     * @brief Iterate over voxels in a region
     */
    template<typename Func>
    void ForEachVoxelInRegion(glm::ivec3 min, glm::ivec3 max, Func&& func) const {
        min = glm::max(min, glm::ivec3(0));
        max = glm::min(max, glm::ivec3(m_config.mapWidth - 1, m_config.mapHeight - 1, m_config.maxHeight - 1));

        for (int z = min.z; z <= max.z; ++z) {
            for (int y = min.y; y <= max.y; ++y) {
                for (int x = min.x; x <= max.x; ++x) {
                    func(glm::ivec3(x, y, z), GetVoxelInternal(x, y, z));
                }
            }
        }
    }

    // ========== Serialization (JSON for Firebase) ==========

    /**
     * @brief Convert to JSON for Firebase storage
     * @param sparseMode Only save non-empty voxels (recommended)
     */
    nlohmann::json ToJson(bool sparseMode = true) const;

    /**
     * @brief Load from JSON
     */
    void FromJson(const nlohmann::json& json);

    /**
     * @brief Save to file
     */
    bool SaveToFile(const std::string& filepath) const;

    /**
     * @brief Load from file
     */
    bool LoadFromFile(const std::string& filepath);

    // ========== Configuration Access ==========

    const VoxelConfig& GetConfig() const { return m_config; }
    int GetWidth() const { return m_config.mapWidth; }
    int GetHeight() const { return m_config.mapHeight; }
    int GetMaxZ() const { return m_config.maxHeight; }
    bool IsHexGrid() const { return m_config.useHexGrid; }

    /**
     * @brief Get the hex grid (for hex coordinate operations)
     */
    HexGrid& GetHexGrid() { return m_hexGrid; }
    const HexGrid& GetHexGrid() const { return m_hexGrid; }

    // ========== Dirty State ==========

    bool IsDirty() const { return m_dirty; }
    void ClearDirty() { m_dirty = false; }
    void MarkDirty() { m_dirty = true; }

    /**
     * @brief Mark a region as dirty
     */
    void MarkRegionDirty(const glm::ivec3& min, const glm::ivec3& max);

    /**
     * @brief Get dirty regions for optimized re-rendering
     */
    const std::vector<std::pair<glm::ivec3, glm::ivec3>>& GetDirtyRegions() const { return m_dirtyRegions; }
    void ClearDirtyRegions() { m_dirtyRegions.clear(); }

private:
    /**
     * @brief Internal voxel access without bounds checking
     */
    const Voxel& GetVoxelInternal(int x, int y, int z) const;
    Voxel& GetVoxelInternal(int x, int y, int z);

    /**
     * @brief Get flat array index from 3D coordinates
     */
    size_t GetIndex(int x, int y, int z) const;

    /**
     * @brief Ensure voxel storage is allocated
     */
    void EnsureAllocated();

    VoxelConfig m_config;
    HexGrid m_hexGrid;

    // Flat storage (for now - can be optimized with chunks later)
    std::vector<Voxel> m_voxels;
    Voxel m_emptyVoxel;  // Return reference for out-of-bounds

    // Large object tracking
    std::vector<LargeObject> m_largeObjects;
    uint32_t m_nextObjectId = 1;

    // Dirty tracking
    bool m_dirty = false;
    std::vector<std::pair<glm::ivec3, glm::ivec3>> m_dirtyRegions;
};

} // namespace Vehement
