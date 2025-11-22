#pragma once

/**
 * @file Blueprint.hpp
 * @brief Building blueprint/template system
 *
 * Allows players to:
 * - Save building designs as reusable templates
 * - Share blueprints with the community via Firebase
 * - Download and use blueprints from other players
 * - Built-in starter blueprints for common structures
 */

#include "../world/Tile.hpp"
#include "Resource.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <cstdint>

// Forward declare OpenGL type
using GLuint = unsigned int;

namespace Vehement {

class TileMap;

namespace RTS {

// Forward declaration
struct Voxel;
class Voxel3DMap;

// ============================================================================
// Blueprint Categories
// ============================================================================

/**
 * @brief Categories for organizing blueprints
 */
enum class BlueprintCategory : uint8_t {
    Housing,        ///< Residential buildings
    Defense,        ///< Walls, towers, fortifications
    Production,     ///< Workshops, farms, mines
    Storage,        ///< Warehouses, silos
    Decoration,     ///< Aesthetic structures
    Infrastructure, ///< Roads, bridges, utilities
    Military,       ///< Barracks, training grounds
    Custom,         ///< User-created uncategorized
    COUNT
};

/**
 * @brief Convert category to string
 */
inline const char* BlueprintCategoryToString(BlueprintCategory cat) {
    switch (cat) {
        case BlueprintCategory::Housing:        return "Housing";
        case BlueprintCategory::Defense:        return "Defense";
        case BlueprintCategory::Production:     return "Production";
        case BlueprintCategory::Storage:        return "Storage";
        case BlueprintCategory::Decoration:     return "Decoration";
        case BlueprintCategory::Infrastructure: return "Infrastructure";
        case BlueprintCategory::Military:       return "Military";
        case BlueprintCategory::Custom:         return "Custom";
        default:                                return "Unknown";
    }
}

// ============================================================================
// Blueprint Tags
// ============================================================================

/**
 * @brief Tags for filtering blueprints
 */
enum class BlueprintTag : uint32_t {
    None            = 0,
    Starter         = 1 << 0,   ///< Good for beginners
    Advanced        = 1 << 1,   ///< Complex/expensive
    Medieval        = 1 << 2,   ///< Medieval style
    Modern          = 1 << 3,   ///< Modern style
    Industrial      = 1 << 4,   ///< Industrial style
    Fantasy         = 1 << 5,   ///< Fantasy style
    Efficient       = 1 << 6,   ///< Resource efficient
    Large           = 1 << 7,   ///< Large structure
    Small           = 1 << 8,   ///< Small structure
    MultiStory      = 1 << 9,   ///< Multiple floors
    Defensive       = 1 << 10,  ///< Good for defense
    Aesthetic       = 1 << 11,  ///< Visually appealing
    Modular         = 1 << 12,  ///< Can be combined
    Featured        = 1 << 13,  ///< Staff pick
};

inline BlueprintTag operator|(BlueprintTag a, BlueprintTag b) {
    return static_cast<BlueprintTag>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline BlueprintTag operator&(BlueprintTag a, BlueprintTag b) {
    return static_cast<BlueprintTag>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool HasTag(BlueprintTag tags, BlueprintTag test) {
    return (static_cast<uint32_t>(tags) & static_cast<uint32_t>(test)) != 0;
}

// ============================================================================
// Blueprint Structure
// ============================================================================

/**
 * @brief A saved building design that can be placed in the world
 */
struct Blueprint {
    // Identity
    std::string id;                 ///< Unique identifier (UUID for community)
    std::string name;               ///< Display name
    std::string description;        ///< Detailed description
    std::string author;             ///< Creator's name/ID
    int64_t createdTime = 0;        ///< Unix timestamp of creation
    int64_t modifiedTime = 0;       ///< Last modification time

    // Classification
    BlueprintCategory category = BlueprintCategory::Custom;
    BlueprintTag tags = BlueprintTag::None;
    int version = 1;                ///< Blueprint format version

    // Size and bounds
    glm::ivec3 size{0, 0, 0};       ///< Dimensions (width, height, depth)
    glm::ivec3 origin{0, 0, 0};     ///< Placement origin point

    // Voxel data - the actual structure
    std::vector<Voxel> voxels;

    // Resource requirements
    ResourceCost totalCost;         ///< Total resource cost to build

    // Materials used (for filtering/info)
    std::map<TileType, int> materialCounts;

    // Preview/thumbnail
    GLuint previewTexture = 0;      ///< OpenGL texture ID for preview
    std::vector<uint8_t> previewData; ///< Raw preview image data (PNG)

    // Community stats (for downloaded blueprints)
    int downloads = 0;
    int likes = 0;
    float rating = 0.0f;
    int ratingCount = 0;

    // =========================================================================
    // Serialization
    // =========================================================================

    /**
     * @brief Serialize blueprint to JSON
     */
    [[nodiscard]] std::string ToJson() const;

    /**
     * @brief Deserialize blueprint from JSON
     */
    static Blueprint FromJson(const std::string& json);

    /**
     * @brief Save blueprint to binary format (more compact)
     */
    [[nodiscard]] std::vector<uint8_t> ToBinary() const;

    /**
     * @brief Load blueprint from binary format
     */
    static Blueprint FromBinary(const std::vector<uint8_t>& data);

    // =========================================================================
    // Validation
    // =========================================================================

    /**
     * @brief Check if blueprint is valid
     */
    [[nodiscard]] bool IsValid() const;

    /**
     * @brief Check if blueprint can be placed at position
     */
    [[nodiscard]] bool CanPlace(const Voxel3DMap& map, const glm::ivec3& pos) const;

    /**
     * @brief Get placement issues (if any)
     */
    [[nodiscard]] std::vector<std::string> GetPlacementIssues(
        const Voxel3DMap& map, const glm::ivec3& pos) const;

    // =========================================================================
    // Manipulation
    // =========================================================================

    /**
     * @brief Rotate blueprint 90 degrees around Y axis
     */
    void Rotate90();

    /**
     * @brief Flip blueprint along X axis
     */
    void FlipX();

    /**
     * @brief Flip blueprint along Z axis
     */
    void FlipZ();

    /**
     * @brief Recalculate bounds and costs
     */
    void Recalculate();

    /**
     * @brief Generate preview texture from voxel data
     */
    void GeneratePreview();

    // =========================================================================
    // Utility
    // =========================================================================

    /**
     * @brief Get volume (number of voxels)
     */
    [[nodiscard]] int GetVolume() const { return static_cast<int>(voxels.size()); }

    /**
     * @brief Get floor count
     */
    [[nodiscard]] int GetFloorCount() const;

    /**
     * @brief Get dominant material
     */
    [[nodiscard]] TileType GetDominantMaterial() const;

    /**
     * @brief Check if blueprint fits in dimensions
     */
    [[nodiscard]] bool FitsIn(int maxWidth, int maxHeight, int maxDepth) const;
};

// ============================================================================
// Blueprint Info (lightweight for listings)
// ============================================================================

/**
 * @brief Lightweight blueprint info for browsing
 */
struct BlueprintInfo {
    std::string id;
    std::string name;
    std::string author;
    BlueprintCategory category;
    BlueprintTag tags;
    glm::ivec3 size;
    int voxelCount;
    int downloads;
    int likes;
    float rating;
    int64_t createdTime;
    std::vector<uint8_t> thumbnailData;
};

// ============================================================================
// Blueprint Library
// ============================================================================

/**
 * @brief Manages collection of blueprints
 *
 * Handles:
 * - Built-in default blueprints
 * - User-created blueprints (local storage)
 * - Community blueprints (Firebase integration)
 */
class BlueprintLibrary {
public:
    BlueprintLibrary();
    ~BlueprintLibrary();

    // Non-copyable
    BlueprintLibrary(const BlueprintLibrary&) = delete;
    BlueprintLibrary& operator=(const BlueprintLibrary&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Load default built-in blueprints
     */
    void LoadDefaultBlueprints();

    /**
     * @brief Load user blueprints from disk
     */
    bool LoadUserBlueprints();

    /**
     * @brief Save user blueprints to disk
     */
    bool SaveUserBlueprints() const;

    /**
     * @brief Get blueprints directory path
     */
    [[nodiscard]] static std::string GetBlueprintsPath();

    // =========================================================================
    // Blueprint Access
    // =========================================================================

    /**
     * @brief Get blueprint by name
     */
    [[nodiscard]] const Blueprint* GetBlueprint(const std::string& name) const;

    /**
     * @brief Get blueprint by ID
     */
    [[nodiscard]] const Blueprint* GetBlueprintById(const std::string& id) const;

    /**
     * @brief Get all blueprint names
     */
    [[nodiscard]] std::vector<std::string> GetBlueprintNames() const;

    /**
     * @brief Get all blueprints
     */
    [[nodiscard]] const std::vector<Blueprint>& GetAllBlueprints() const {
        return m_blueprints;
    }

    /**
     * @brief Get blueprints by category
     */
    [[nodiscard]] std::vector<const Blueprint*> GetByCategory(BlueprintCategory cat) const;

    /**
     * @brief Get blueprints by tag
     */
    [[nodiscard]] std::vector<const Blueprint*> GetByTag(BlueprintTag tag) const;

    /**
     * @brief Search blueprints by name
     */
    [[nodiscard]] std::vector<const Blueprint*> Search(const std::string& query) const;

    /**
     * @brief Get default blueprints (built-in)
     */
    [[nodiscard]] std::vector<const Blueprint*> GetDefaultBlueprints() const;

    /**
     * @brief Get user blueprints
     */
    [[nodiscard]] std::vector<const Blueprint*> GetUserBlueprints() const;

    // =========================================================================
    // User Blueprint Management
    // =========================================================================

    /**
     * @brief Save a user blueprint
     */
    bool SaveUserBlueprint(const Blueprint& bp);

    /**
     * @brief Update existing blueprint
     */
    bool UpdateUserBlueprint(const std::string& name, const Blueprint& bp);

    /**
     * @brief Delete a user blueprint
     */
    bool DeleteUserBlueprint(const std::string& name);

    /**
     * @brief Rename a blueprint
     */
    bool RenameBlueprint(const std::string& oldName, const std::string& newName);

    /**
     * @brief Duplicate a blueprint
     */
    Blueprint* DuplicateBlueprint(const std::string& name);

    // =========================================================================
    // Community Blueprints (Firebase Integration)
    // =========================================================================

    /**
     * @brief Upload blueprint to community
     */
    void UploadBlueprint(const Blueprint& bp,
                         std::function<void(bool success, const std::string& id)> callback);

    /**
     * @brief Download blueprint from community
     */
    void DownloadBlueprint(const std::string& id,
                           std::function<void(bool success, const Blueprint& bp)> callback);

    /**
     * @brief Browse community blueprints
     */
    void BrowseCommunityBlueprints(
        int page, int perPage,
        BlueprintCategory categoryFilter,
        const std::string& sortBy,
        std::function<void(const std::vector<BlueprintInfo>& results)> callback);

    /**
     * @brief Search community blueprints
     */
    void SearchCommunity(
        const std::string& query,
        std::function<void(const std::vector<BlueprintInfo>& results)> callback);

    /**
     * @brief Rate a community blueprint
     */
    void RateBlueprint(const std::string& id, int stars);

    /**
     * @brief Like/favorite a community blueprint
     */
    void LikeBlueprint(const std::string& id);

    /**
     * @brief Report inappropriate blueprint
     */
    void ReportBlueprint(const std::string& id, const std::string& reason);

    /**
     * @brief Check if connected to community
     */
    [[nodiscard]] bool IsOnline() const { return m_isOnline; }

    /**
     * @brief Set online status
     */
    void SetOnlineStatus(bool online) { m_isOnline = online; }

    // =========================================================================
    // Import/Export
    // =========================================================================

    /**
     * @brief Export blueprint to file
     */
    bool ExportToFile(const std::string& name, const std::string& filepath) const;

    /**
     * @brief Import blueprint from file
     */
    bool ImportFromFile(const std::string& filepath);

    /**
     * @brief Export blueprint as shareable string (base64)
     */
    [[nodiscard]] std::string ExportAsString(const std::string& name) const;

    /**
     * @brief Import blueprint from shareable string
     */
    bool ImportFromString(const std::string& data);

private:
    void CreateDefaultHouse();
    void CreateDefaultWatchTower();
    void CreateDefaultWall();
    void CreateDefaultFarm();
    void CreateDefaultWorkshop();
    void CreateDefaultBarracks();
    void CreateDefaultFortress();
    void CreateDefaultBridge();

    std::string GenerateUUID() const;

    std::vector<Blueprint> m_blueprints;
    std::vector<Blueprint> m_defaultBlueprints;
    bool m_isOnline = false;
};

// ============================================================================
// Blueprint Builder Helper
// ============================================================================

/**
 * @brief Fluent interface for building blueprints programmatically
 */
class BlueprintBuilder {
public:
    BlueprintBuilder(const std::string& name);

    BlueprintBuilder& SetDescription(const std::string& desc);
    BlueprintBuilder& SetCategory(BlueprintCategory cat);
    BlueprintBuilder& AddTag(BlueprintTag tag);
    BlueprintBuilder& SetAuthor(const std::string& author);

    BlueprintBuilder& AddFloor(int x, int y, int z, TileType type);
    BlueprintBuilder& AddWall(int x, int y, int z, TileType type, int dx = 0, int dy = 0, int dz = 1);
    BlueprintBuilder& AddRoof(int x, int y, int z, TileType type);
    BlueprintBuilder& AddDoor(int x, int y, int z);
    BlueprintBuilder& AddWindow(int x, int y, int z);
    BlueprintBuilder& AddStairs(int x, int y, int z, int dx, int dy, int dz);

    BlueprintBuilder& FillFloor(int minX, int maxX, int y, int minZ, int maxZ, TileType type);
    BlueprintBuilder& BuildWallLine(int x1, int z1, int x2, int z2, int y, int height, TileType type);
    BlueprintBuilder& BuildWallRect(int minX, int minZ, int maxX, int maxZ, int y, int height, TileType type);

    [[nodiscard]] Blueprint Build();

private:
    Blueprint m_blueprint;
};

} // namespace RTS
} // namespace Vehement
