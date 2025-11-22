#pragma once

#include "LocationDefinition.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>

namespace Vehement {

// Forward declarations
class TileMap;
class BuildingPlacer;
class EntityPlacer;
class RoadEditor;

/**
 * @brief Preset category for organization
 */
enum class PresetCategory : uint8_t {
    Town,           ///< Towns and villages
    Military,       ///< Forts, camps, outposts
    Ruins,          ///< Ruined structures
    Natural,        ///< Natural landmarks
    Industrial,     ///< Industrial facilities
    Residential,    ///< Residential areas
    Commercial,     ///< Shops, markets
    Custom          ///< User-defined
};

/**
 * @brief Get display name for preset category
 */
inline const char* GetPresetCategoryName(PresetCategory category) {
    switch (category) {
        case PresetCategory::Town:       return "Town";
        case PresetCategory::Military:   return "Military";
        case PresetCategory::Ruins:      return "Ruins";
        case PresetCategory::Natural:    return "Natural";
        case PresetCategory::Industrial: return "Industrial";
        case PresetCategory::Residential: return "Residential";
        case PresetCategory::Commercial: return "Commercial";
        case PresetCategory::Custom:     return "Custom";
        default:                         return "Unknown";
    }
}

/**
 * @brief Preset parameter definition
 */
struct PresetParameter {
    std::string name;
    std::string displayName;
    std::string description;

    enum class Type : uint8_t {
        Int,
        Float,
        Bool,
        String,
        Enum
    };

    Type type = Type::Float;

    // Default and range
    float defaultFloat = 1.0f;
    float minFloat = 0.0f;
    float maxFloat = 10.0f;
    int defaultInt = 1;
    int minInt = 0;
    int maxInt = 100;
    bool defaultBool = true;
    std::string defaultString;
    std::vector<std::string> enumValues;

    // Current value
    float valueFloat = 1.0f;
    int valueInt = 1;
    bool valueBool = true;
    std::string valueString;
};

/**
 * @brief Tile placement in preset
 */
struct PresetTile {
    glm::ivec2 offset{0, 0};        ///< Offset from preset origin
    TileType type = TileType::None;
    uint8_t variant = 0;
    bool isWall = false;
    float wallHeight = 0.0f;
};

/**
 * @brief Building placement in preset
 */
struct PresetBuilding {
    glm::ivec2 offset{0, 0};
    std::string buildingType;
    float rotation = 0.0f;
    int variant = 0;
    bool optional = false;          ///< Can be omitted for smaller presets
};

/**
 * @brief Entity placement in preset
 */
struct PresetEntity {
    glm::vec2 offset{0.0f};
    std::string entityType;
    std::string template_;
    float rotation = 0.0f;
    bool optional = false;
};

/**
 * @brief Road segment in preset
 */
struct PresetRoad {
    glm::vec2 startOffset{0.0f};
    glm::vec2 endOffset{0.0f};
    std::string roadType;
    int width = 2;
};

/**
 * @brief Complete location preset definition
 */
struct LocationPreset {
    std::string name;
    std::string description;
    PresetCategory category = PresetCategory::Custom;
    std::vector<std::string> tags;

    // Base size
    glm::ivec2 baseSize{20, 20};

    // Scalable size range
    glm::ivec2 minSize{10, 10};
    glm::ivec2 maxSize{50, 50};

    // Content
    std::vector<PresetTile> tiles;
    std::vector<PresetBuilding> buildings;
    std::vector<PresetEntity> entities;
    std::vector<PresetRoad> roads;

    // Parameters
    std::vector<PresetParameter> parameters;

    // Metadata
    std::string author;
    std::string version;
    std::string filePath;
    bool isBuiltIn = false;

    // Preview
    std::string previewImage;
};

/**
 * @brief Result of applying a preset
 */
struct PresetApplyResult {
    bool success = true;
    std::string errorMessage;
    int tilesPlaced = 0;
    int buildingsPlaced = 0;
    int entitiesPlaced = 0;
    int roadsDrawn = 0;
    glm::ivec2 actualSize{0, 0};
    LocationDefinition* createdLocation = nullptr;
};

/**
 * @brief Pre-made location template system
 *
 * Features:
 * - Save current location as preset
 * - Load preset into new location
 * - Preset categories (town, village, fort, ruins, etc.)
 * - Parameterized presets (size, density, style)
 * - Built-in and user-created presets
 *
 * Usage:
 * 1. Load available presets
 * 2. Select preset to apply
 * 3. Configure parameters (size, density, etc.)
 * 4. Apply to world at specified position
 */
class LocationPresets {
public:
    using PresetCallback = std::function<void(const LocationPreset&)>;

    LocationPresets();
    ~LocationPresets() = default;

    // Allow copy and move
    LocationPresets(const LocationPresets&) = default;
    LocationPresets& operator=(const LocationPresets&) = default;
    LocationPresets(LocationPresets&&) noexcept = default;
    LocationPresets& operator=(LocationPresets&&) noexcept = default;

    // =========================================================================
    // Loading / Saving
    // =========================================================================

    /**
     * @brief Initialize with presets directory
     * @param presetsDirectory Directory containing preset files
     */
    void Initialize(const std::string& presetsDirectory);

    /**
     * @brief Load all presets from directory
     * @return Number of presets loaded
     */
    int LoadAllPresets();

    /**
     * @brief Load a single preset from file
     * @param filePath Path to preset file
     * @return Pointer to loaded preset, or nullptr on failure
     */
    LocationPreset* LoadPreset(const std::string& filePath);

    /**
     * @brief Save a preset to file
     * @param preset Preset to save
     * @param filePath File path (uses preset's path if empty)
     * @return true if successful
     */
    bool SavePreset(const LocationPreset& preset, const std::string& filePath = "");

    /**
     * @brief Reload all presets
     */
    void ReloadPresets();

    // =========================================================================
    // Preset Management
    // =========================================================================

    /**
     * @brief Get all available presets
     */
    [[nodiscard]] const std::vector<LocationPreset>& GetPresets() const noexcept {
        return m_presets;
    }

    /**
     * @brief Get presets by category
     */
    [[nodiscard]] std::vector<const LocationPreset*> GetPresetsByCategory(
        PresetCategory category) const;

    /**
     * @brief Get presets by tag
     */
    [[nodiscard]] std::vector<const LocationPreset*> GetPresetsByTag(
        const std::string& tag) const;

    /**
     * @brief Find preset by name
     */
    [[nodiscard]] const LocationPreset* FindPreset(const std::string& name) const;

    /**
     * @brief Get all categories with presets
     */
    [[nodiscard]] std::vector<PresetCategory> GetUsedCategories() const;

    /**
     * @brief Get all tags used by presets
     */
    [[nodiscard]] std::vector<std::string> GetAllTags() const;

    // =========================================================================
    // Create Preset from Location
    // =========================================================================

    /**
     * @brief Create a preset from an existing location
     * @param location Location to save as preset
     * @param map Tile map with location content
     * @param name Preset name
     * @param category Preset category
     * @return Created preset
     */
    LocationPreset CreateFromLocation(
        const LocationDefinition& location,
        const TileMap& map,
        const std::string& name,
        PresetCategory category);

    /**
     * @brief Create a preset from a selection
     * @param min Minimum tile coordinate
     * @param max Maximum tile coordinate
     * @param map Tile map
     * @param name Preset name
     * @return Created preset
     */
    LocationPreset CreateFromSelection(
        const glm::ivec2& min,
        const glm::ivec2& max,
        const TileMap& map,
        const std::string& name);

    // =========================================================================
    // Apply Preset
    // =========================================================================

    /**
     * @brief Apply a preset at a position
     * @param preset Preset to apply
     * @param position World position (center or corner)
     * @param map Tile map to modify
     * @param centerPlacement If true, position is center; otherwise top-left
     * @return Result of application
     */
    PresetApplyResult ApplyPreset(
        const LocationPreset& preset,
        const glm::ivec2& position,
        TileMap& map,
        bool centerPlacement = true);

    /**
     * @brief Apply a preset with custom parameters
     */
    PresetApplyResult ApplyPresetWithParameters(
        const LocationPreset& preset,
        const glm::ivec2& position,
        TileMap& map,
        const std::unordered_map<std::string, float>& parameters);

    /**
     * @brief Preview preset without applying
     */
    [[nodiscard]] std::vector<PresetTile> PreviewPreset(
        const LocationPreset& preset,
        const glm::ivec2& position) const;

    /**
     * @brief Get tiles that would be modified
     */
    [[nodiscard]] std::vector<glm::ivec2> GetAffectedTiles(
        const LocationPreset& preset,
        const glm::ivec2& position) const;

    // =========================================================================
    // Parameters
    // =========================================================================

    /**
     * @brief Scale a preset to new size
     */
    [[nodiscard]] LocationPreset ScalePreset(
        const LocationPreset& preset,
        const glm::ivec2& newSize) const;

    /**
     * @brief Apply parameters to preset
     */
    void ApplyParameters(
        LocationPreset& preset,
        const std::unordered_map<std::string, float>& parameters) const;

    /**
     * @brief Get default parameters for a preset
     */
    [[nodiscard]] std::unordered_map<std::string, float> GetDefaultParameters(
        const LocationPreset& preset) const;

    // =========================================================================
    // Built-in Presets
    // =========================================================================

    /**
     * @brief Load built-in presets
     */
    void LoadBuiltInPresets();

    /**
     * @brief Create small village preset
     */
    [[nodiscard]] LocationPreset CreateSmallVillagePreset() const;

    /**
     * @brief Create military outpost preset
     */
    [[nodiscard]] LocationPreset CreateMilitaryOutpostPreset() const;

    /**
     * @brief Create trading post preset
     */
    [[nodiscard]] LocationPreset CreateTradingPostPreset() const;

    /**
     * @brief Create ruins preset
     */
    [[nodiscard]] LocationPreset CreateRuinsPreset() const;

    // =========================================================================
    // Serialization
    // =========================================================================

    /**
     * @brief Convert preset to JSON
     */
    [[nodiscard]] std::string PresetToJson(const LocationPreset& preset) const;

    /**
     * @brief Parse preset from JSON
     */
    [[nodiscard]] LocationPreset PresetFromJson(const std::string& json) const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    void SetOnPresetLoaded(PresetCallback callback) { m_onPresetLoaded = std::move(callback); }
    void SetOnPresetSaved(PresetCallback callback) { m_onPresetSaved = std::move(callback); }
    void SetOnPresetApplied(PresetCallback callback) { m_onPresetApplied = std::move(callback); }

private:
    void AddBuiltInPreset(const LocationPreset& preset);
    std::vector<PresetTile> ScaleTiles(
        const std::vector<PresetTile>& tiles,
        const glm::ivec2& originalSize,
        const glm::ivec2& newSize) const;

    std::string m_presetsDirectory;
    std::vector<LocationPreset> m_presets;

    // Callbacks
    PresetCallback m_onPresetLoaded;
    PresetCallback m_onPresetSaved;
    PresetCallback m_onPresetApplied;
};

} // namespace Vehement
