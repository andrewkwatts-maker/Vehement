#pragma once

/**
 * @file WorldBuilding.hpp
 * @brief World building as core gameplay mechanic
 *
 * Provides comprehensive building and terrain modification systems:
 * - Multi-story construction (floors, walls, roofs, stairs)
 * - Terrain modification (raise, lower, flatten, paint)
 * - Blueprint system for saving/loading designs
 * - Procedural building assistance
 * - Creative building modes
 */

#include "../world/Tile.hpp"
#include "Resource.hpp"
#include "Culture.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <optional>
#include <map>
#include <unordered_map>

namespace Nova {
    class Renderer;
    class Camera;
}

namespace Vehement {

class TileMap;
class World;

namespace RTS {

// Forward declarations
struct Blueprint;
class BlueprintLibrary;
class StructuralIntegrity;

// ============================================================================
// Build Mode System
// ============================================================================

/**
 * @brief Building interaction modes
 */
enum class BuildMode : uint8_t {
    Off,            ///< Normal gameplay, no building
    Place,          ///< Place new structures/voxels
    Demolish,       ///< Remove structures
    Terraform,      ///< Modify terrain height
    Paint,          ///< Change terrain/surface textures
    Clone,          ///< Copy existing structures
    Blueprint,      ///< Save/load building patterns
    Interior,       ///< Place furniture and decorations
    Electrical,     ///< Wire up power and lighting
    Plumbing        ///< Water and irrigation systems
};

/**
 * @brief Convert build mode to string
 */
inline const char* BuildModeToString(BuildMode mode) {
    switch (mode) {
        case BuildMode::Off:        return "Off";
        case BuildMode::Place:      return "Place";
        case BuildMode::Demolish:   return "Demolish";
        case BuildMode::Terraform:  return "Terraform";
        case BuildMode::Paint:      return "Paint";
        case BuildMode::Clone:      return "Clone";
        case BuildMode::Blueprint:  return "Blueprint";
        case BuildMode::Interior:   return "Interior";
        case BuildMode::Electrical: return "Electrical";
        case BuildMode::Plumbing:   return "Plumbing";
        default:                    return "Unknown";
    }
}

// ============================================================================
// Terrain System
// ============================================================================

/**
 * @brief Types of terrain that can be set
 */
enum class TerrainType : uint8_t {
    Grass,
    Dirt,
    Sand,
    Stone,
    Snow,
    Mud,
    Gravel,
    Water,
    Lava,
    Farm,           ///< Tilled farmland
    Path,           ///< Packed dirt path
    Road,           ///< Paved road
    COUNT
};

/**
 * @brief Convert terrain type to tile type
 */
inline TileType TerrainToTileType(TerrainType terrain) {
    switch (terrain) {
        case TerrainType::Grass:    return TileType::GroundGrass1;
        case TerrainType::Dirt:     return TileType::GroundDirt;
        case TerrainType::Stone:    return TileType::StoneRaw;
        case TerrainType::Water:    return TileType::Water1;
        case TerrainType::Path:     return TileType::GroundDirt;
        case TerrainType::Road:     return TileType::ConcreteAsphalt1;
        default:                    return TileType::GroundGrass1;
    }
}

// ============================================================================
// Roof System
// ============================================================================

/**
 * @brief Types of roofs available
 */
enum class RoofType : uint8_t {
    Flat,           ///< Flat roof (can build on top)
    Peaked,         ///< Simple peaked roof
    Hipped,         ///< Four-sided sloped roof
    Gabled,         ///< Two-sided sloped roof
    Dome,           ///< Rounded dome
    Pyramid,        ///< Pyramid shape
    Thatched,       ///< Medieval straw roof
    Tiled,          ///< Modern tile roof
    Metal,          ///< Industrial corrugated metal
    Green,          ///< Living green roof with plants
    COUNT
};

/**
 * @brief Get display name for roof type
 */
inline const char* RoofTypeToString(RoofType type) {
    switch (type) {
        case RoofType::Flat:        return "Flat";
        case RoofType::Peaked:      return "Peaked";
        case RoofType::Hipped:      return "Hipped";
        case RoofType::Gabled:      return "Gabled";
        case RoofType::Dome:        return "Dome";
        case RoofType::Pyramid:     return "Pyramid";
        case RoofType::Thatched:    return "Thatched";
        case RoofType::Tiled:       return "Tiled";
        case RoofType::Metal:       return "Metal";
        case RoofType::Green:       return "Green";
        default:                    return "Unknown";
    }
}

// ============================================================================
// Room System
// ============================================================================

/**
 * @brief Pre-defined room types for procedural generation
 */
enum class RoomType : uint8_t {
    Generic,        ///< Empty room
    Bedroom,        ///< Sleeping quarters
    Kitchen,        ///< Food preparation
    Storage,        ///< Storage space
    Workshop,       ///< Crafting area
    Armory,         ///< Weapon storage
    Barracks,       ///< Military housing
    ThroneRoom,     ///< Grand hall
    Library,        ///< Book storage
    Laboratory,     ///< Research area
    Prison,         ///< Holding cells
    Garden,         ///< Indoor plants
    Bathroom,       ///< Sanitation
    DiningHall,     ///< Eating area
    COUNT
};

/**
 * @brief Get default furniture for a room type
 */
std::vector<std::string> GetDefaultFurniture(RoomType type);

// ============================================================================
// Voxel Structure for 3D Building
// ============================================================================

/**
 * @brief Single voxel in a 3D building structure
 */
struct Voxel {
    glm::ivec3 position{0, 0, 0};   ///< Position in structure coordinates
    TileType type = TileType::None; ///< Material type

    bool isWall = false;            ///< Is this a wall voxel
    bool isFloor = false;           ///< Is this a floor voxel
    bool isRoof = false;            ///< Is this a roof voxel
    bool isStairs = false;          ///< Is this stairs
    bool isDoor = false;            ///< Door opening
    bool isWindow = false;          ///< Window opening

    glm::ivec3 direction{0, 0, 1};  ///< Facing direction (for stairs, doors)
    float rotation = 0.0f;          ///< Rotation in degrees

    uint8_t health = 100;           ///< Structural health (0-100)
    bool isSupport = false;         ///< Is this a load-bearing element

    // Serialization
    [[nodiscard]] std::string ToJson() const;
    static Voxel FromJson(const std::string& json);

    bool operator==(const Voxel& other) const {
        return position == other.position && type == other.type;
    }
};

// ============================================================================
// 3D Voxel Map for Buildings
// ============================================================================

/**
 * @brief 3D map of voxels representing structures in the world
 */
class Voxel3DMap {
public:
    Voxel3DMap();
    ~Voxel3DMap() = default;

    /**
     * @brief Initialize with world size
     */
    void Initialize(int width, int height, int depth);

    /**
     * @brief Get voxel at position
     */
    Voxel* GetVoxel(int x, int y, int z);
    const Voxel* GetVoxel(int x, int y, int z) const;
    Voxel* GetVoxel(const glm::ivec3& pos) { return GetVoxel(pos.x, pos.y, pos.z); }
    const Voxel* GetVoxel(const glm::ivec3& pos) const { return GetVoxel(pos.x, pos.y, pos.z); }

    /**
     * @brief Set voxel at position
     */
    bool SetVoxel(int x, int y, int z, const Voxel& voxel);
    bool SetVoxel(const glm::ivec3& pos, const Voxel& voxel) {
        return SetVoxel(pos.x, pos.y, pos.z, voxel);
    }

    /**
     * @brief Remove voxel at position
     */
    bool RemoveVoxel(int x, int y, int z);
    bool RemoveVoxel(const glm::ivec3& pos) { return RemoveVoxel(pos.x, pos.y, pos.z); }

    /**
     * @brief Check if position is within bounds
     */
    [[nodiscard]] bool IsInBounds(int x, int y, int z) const;
    [[nodiscard]] bool IsInBounds(const glm::ivec3& pos) const {
        return IsInBounds(pos.x, pos.y, pos.z);
    }

    /**
     * @brief Check if position has a solid voxel
     */
    [[nodiscard]] bool IsSolid(int x, int y, int z) const;
    [[nodiscard]] bool IsSolid(const glm::ivec3& pos) const {
        return IsSolid(pos.x, pos.y, pos.z);
    }

    /**
     * @brief Get all voxels in a region
     */
    [[nodiscard]] std::vector<Voxel> GetVoxelsInRegion(
        const glm::ivec3& min, const glm::ivec3& max) const;

    /**
     * @brief Get dimensions
     */
    [[nodiscard]] int GetWidth() const { return m_width; }
    [[nodiscard]] int GetHeight() const { return m_height; }
    [[nodiscard]] int GetDepth() const { return m_depth; }
    [[nodiscard]] glm::ivec3 GetSize() const { return {m_width, m_height, m_depth}; }

    /**
     * @brief Get total voxel count
     */
    [[nodiscard]] size_t GetVoxelCount() const { return m_voxelCount; }

    /**
     * @brief Clear all voxels
     */
    void Clear();

private:
    int m_width = 0;
    int m_height = 0;
    int m_depth = 0;
    size_t m_voxelCount = 0;

    // Sparse storage using hash map
    std::unordered_map<int64_t, Voxel> m_voxels;

    [[nodiscard]] int64_t GetKey(int x, int y, int z) const {
        return (static_cast<int64_t>(x) << 40) |
               (static_cast<int64_t>(y) << 20) |
               static_cast<int64_t>(z);
    }
};

// ============================================================================
// World Building System
// ============================================================================

/**
 * @brief Core world building gameplay system
 *
 * This is the main class for all building/construction mechanics.
 * Makes construction central to gameplay through:
 * - Creative terrain modification
 * - Multi-story building construction
 * - Blueprint save/load system
 * - Procedural assistance tools
 */
class WorldBuilding {
public:
    WorldBuilding();
    ~WorldBuilding();

    // Non-copyable
    WorldBuilding(const WorldBuilding&) = delete;
    WorldBuilding& operator=(const WorldBuilding&) = delete;

    /**
     * @brief Initialize the building system
     */
    bool Initialize(TileMap* tileMap, Voxel3DMap* voxelMap);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Update building system
     */
    void Update(float deltaTime);

    /**
     * @brief Render building previews and UI
     */
    void Render(Nova::Renderer& renderer, const Nova::Camera& camera);

    // =========================================================================
    // Build Mode Control
    // =========================================================================

    /**
     * @brief Set current build mode
     */
    void SetBuildMode(BuildMode mode);

    /**
     * @brief Get current build mode
     */
    [[nodiscard]] BuildMode GetBuildMode() const { return m_buildMode; }

    /**
     * @brief Check if in any building mode
     */
    [[nodiscard]] bool IsBuilding() const { return m_buildMode != BuildMode::Off; }

    /**
     * @brief Toggle build mode on/off
     */
    void ToggleBuildMode();

    // =========================================================================
    // Terrain Modification
    // =========================================================================

    /**
     * @brief Raise terrain at position
     * @param pos World position
     * @param amount Height units to raise
     */
    void RaiseTerrain(const glm::ivec3& pos, int amount);

    /**
     * @brief Lower terrain at position
     */
    void LowerTerrain(const glm::ivec3& pos, int amount);

    /**
     * @brief Flatten terrain in radius to average height
     */
    void FlattenTerrain(const glm::ivec3& pos, int radius);

    /**
     * @brief Flatten terrain to specific height
     */
    void FlattenTerrainToHeight(const glm::ivec3& pos, int radius, int targetHeight);

    /**
     * @brief Set terrain type at position
     */
    void SetTerrainType(const glm::ivec3& pos, TerrainType type);

    /**
     * @brief Paint terrain in radius
     */
    void PaintTerrain(const glm::ivec3& pos, int radius, TerrainType type);

    /**
     * @brief Smooth terrain to reduce sharp edges
     */
    void SmoothTerrain(const glm::ivec3& pos, int radius, float strength = 0.5f);

    /**
     * @brief Create a hill at position
     */
    void CreateHill(const glm::ivec3& pos, int radius, int height);

    /**
     * @brief Dig a hole/pit at position
     */
    void DigHole(const glm::ivec3& pos, int radius, int depth);

    /**
     * @brief Create a moat around position
     */
    void CreateMoat(const glm::ivec3& center, int innerRadius, int outerRadius, int depth);

    // =========================================================================
    // Multi-Story Building
    // =========================================================================

    /**
     * @brief Place a floor tile
     */
    bool PlaceFloor(const glm::ivec3& pos, TileType type);

    /**
     * @brief Place a wall segment
     * @param pos Position of wall
     * @param direction Direction wall faces (N/S/E/W)
     * @param type Material type
     */
    bool PlaceWall(const glm::ivec3& pos, const glm::ivec3& direction, TileType type);

    /**
     * @brief Place a roof section
     */
    bool PlaceRoof(const glm::ivec3& pos, RoofType type, TileType material);

    /**
     * @brief Place stairs
     * @param pos Base position of stairs
     * @param direction Direction stairs ascend
     */
    bool PlaceStairs(const glm::ivec3& pos, const glm::ivec3& direction);

    /**
     * @brief Place a door in an existing wall
     */
    bool PlaceDoor(const glm::ivec3& wallPos);

    /**
     * @brief Place a window in an existing wall
     */
    bool PlaceWindow(const glm::ivec3& wallPos);

    /**
     * @brief Place a pillar/support column
     */
    bool PlacePillar(const glm::ivec3& pos, int height, TileType type);

    /**
     * @brief Place a ramp
     */
    bool PlaceRamp(const glm::ivec3& pos, const glm::ivec3& direction, int height);

    /**
     * @brief Remove a building element
     */
    bool RemoveElement(const glm::ivec3& pos);

    // =========================================================================
    // Wall Building Tools
    // =========================================================================

    /**
     * @brief Build wall between two points
     */
    void BuildWallLine(const glm::ivec3& start, const glm::ivec3& end,
                       TileType type, int height = 1);

    /**
     * @brief Build rectangular wall enclosure
     */
    void BuildWallRect(const glm::ivec3& min, const glm::ivec3& max,
                       TileType type, int height = 1);

    /**
     * @brief Build circular wall
     */
    void BuildWallCircle(const glm::ivec3& center, int radius,
                         TileType type, int height = 1);

    // =========================================================================
    // Blueprint System
    // =========================================================================

    /**
     * @brief Save region as blueprint
     */
    bool SaveBlueprint(const std::string& name, const glm::ivec3& min, const glm::ivec3& max);

    /**
     * @brief Load and place blueprint at position
     */
    bool LoadBlueprint(const std::string& name, const glm::ivec3& pos);

    /**
     * @brief Get list of saved blueprints
     */
    [[nodiscard]] std::vector<std::string> GetSavedBlueprints() const;

    /**
     * @brief Delete a blueprint
     */
    bool DeleteBlueprint(const std::string& name);

    /**
     * @brief Get blueprint library
     */
    [[nodiscard]] BlueprintLibrary* GetBlueprintLibrary() { return m_blueprintLibrary.get(); }

    // =========================================================================
    // Procedural Building Assistance
    // =========================================================================

    /**
     * @brief Auto-generate roof for wall outline
     */
    void AutoRoof(const std::vector<glm::ivec3>& wallOutline, RoofType type);

    /**
     * @brief Auto-fill floor within walls
     */
    void AutoFloor(const std::vector<glm::ivec3>& wallOutline, TileType type);

    /**
     * @brief Generate complete room
     */
    void GenerateRoom(const glm::ivec3& min, const glm::ivec3& max, RoomType type);

    /**
     * @brief Generate a house with rooms
     */
    void GenerateHouse(const glm::ivec3& pos, int width, int depth, int stories);

    /**
     * @brief Generate defensive wall with towers
     */
    void GenerateFortification(const glm::ivec3& center, int radius, int wallHeight);

    /**
     * @brief Auto-add support pillars where needed
     */
    void AutoSupport(const glm::ivec3& min, const glm::ivec3& max);

    /**
     * @brief Detect and fill enclosed areas
     */
    std::vector<glm::ivec3> DetectEnclosedArea(const glm::ivec3& start);

    // =========================================================================
    // Material Selection
    // =========================================================================

    /**
     * @brief Set current building material
     */
    void SetCurrentMaterial(TileType type) { m_currentMaterial = type; }

    /**
     * @brief Get current building material
     */
    [[nodiscard]] TileType GetCurrentMaterial() const { return m_currentMaterial; }

    /**
     * @brief Set current roof type
     */
    void SetCurrentRoofType(RoofType type) { m_currentRoofType = type; }

    /**
     * @brief Get current roof type
     */
    [[nodiscard]] RoofType GetCurrentRoofType() const { return m_currentRoofType; }

    // =========================================================================
    // Resource Integration
    // =========================================================================

    /**
     * @brief Set resource stock for cost checking
     */
    void SetResourceStock(ResourceStock* stock) { m_resourceStock = stock; }

    /**
     * @brief Get cost to place element at position
     */
    [[nodiscard]] ResourceCost GetPlacementCost(const glm::ivec3& pos, TileType type) const;

    /**
     * @brief Get refund for removing element
     */
    [[nodiscard]] ResourceCost GetRemovalRefund(const glm::ivec3& pos) const;

    /**
     * @brief Check if player can afford placement
     */
    [[nodiscard]] bool CanAfford(const glm::ivec3& pos, TileType type) const;

    // =========================================================================
    // Structural Integrity
    // =========================================================================

    /**
     * @brief Get structural integrity system
     */
    [[nodiscard]] StructuralIntegrity* GetStructuralIntegrity() {
        return m_structuralIntegrity.get();
    }

    /**
     * @brief Check if placement would be structurally sound
     */
    [[nodiscard]] bool WouldBeStable(const glm::ivec3& pos, TileType type) const;

    // =========================================================================
    // Building Statistics
    // =========================================================================

    /**
     * @brief Get total voxels placed
     */
    [[nodiscard]] size_t GetTotalVoxelsPlaced() const { return m_totalVoxelsPlaced; }

    /**
     * @brief Get total structures built
     */
    [[nodiscard]] int GetTotalStructuresBuilt() const { return m_totalStructuresBuilt; }

    /**
     * @brief Get building by ID
     */
    struct BuildingStats {
        int floors = 0;
        int walls = 0;
        int roofs = 0;
        int doors = 0;
        int windows = 0;
        int totalVolume = 0;
        glm::ivec3 minBounds{0};
        glm::ivec3 maxBounds{0};
    };

    [[nodiscard]] BuildingStats CalculateBuildingStats(
        const glm::ivec3& min, const glm::ivec3& max) const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    using BuildCallback = std::function<void(const glm::ivec3& pos, const Voxel& voxel)>;
    using DemolishCallback = std::function<void(const glm::ivec3& pos)>;
    using TerrainCallback = std::function<void(const glm::ivec3& pos, int oldHeight, int newHeight)>;

    void SetOnBuild(BuildCallback cb) { m_onBuild = std::move(cb); }
    void SetOnDemolish(DemolishCallback cb) { m_onDemolish = std::move(cb); }
    void SetOnTerrainChange(TerrainCallback cb) { m_onTerrainChange = std::move(cb); }

private:
    void UpdateTerrainHeight(int x, int z, int newHeight);
    void PlaceVoxelInternal(const glm::ivec3& pos, const Voxel& voxel);
    bool CheckPlacementValid(const glm::ivec3& pos, TileType type) const;
    void GenerateRoofPeaked(const std::vector<glm::ivec3>& outline);
    void GenerateRoofFlat(const std::vector<glm::ivec3>& outline);
    void FillInterior(const std::vector<glm::ivec3>& outline, int y, TileType type);

    TileMap* m_tileMap = nullptr;
    Voxel3DMap* m_voxelMap = nullptr;
    ResourceStock* m_resourceStock = nullptr;

    std::unique_ptr<BlueprintLibrary> m_blueprintLibrary;
    std::unique_ptr<StructuralIntegrity> m_structuralIntegrity;

    BuildMode m_buildMode = BuildMode::Off;
    TileType m_currentMaterial = TileType::BricksGrey;
    RoofType m_currentRoofType = RoofType::Peaked;

    // Terrain height map (for 3D terrain)
    std::vector<int> m_terrainHeights;
    int m_terrainWidth = 0;
    int m_terrainDepth = 0;

    // Statistics
    size_t m_totalVoxelsPlaced = 0;
    int m_totalStructuresBuilt = 0;

    // Callbacks
    BuildCallback m_onBuild;
    DemolishCallback m_onDemolish;
    TerrainCallback m_onTerrainChange;
};

} // namespace RTS
} // namespace Vehement
