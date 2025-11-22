#pragma once

#include "../ContentDatabase.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <optional>

namespace Vehement {

class Editor;

namespace Content {

class ContentBrowser;

/**
 * @brief Tile terrain type
 */
enum class TerrainType {
    Ground,
    Water,
    Cliff,
    Forest,
    Mountain,
    Desert,
    Snow,
    Swamp,
    Road,
    Bridge,
    Special
};

/**
 * @brief Tile walkability
 */
enum class Walkability {
    Walkable,
    Blocked,
    SlowWalk,
    WaterOnly,
    FlyingOnly,
    Destructible
};

/**
 * @brief Tile biome
 */
enum class TileBiome {
    Temperate,
    Desert,
    Arctic,
    Tropical,
    Volcanic,
    Underground,
    Alien
};

/**
 * @brief Tile stats for preview
 */
struct TileStats {
    std::string id;
    std::string name;
    TerrainType terrainType = TerrainType::Ground;
    TileBiome biome = TileBiome::Temperate;
    Walkability walkability = Walkability::Walkable;

    // Movement modifiers
    float movementCost = 1.0f;
    float infantryModifier = 1.0f;
    float vehicleModifier = 1.0f;
    float navalModifier = 0.0f;

    // Combat modifiers
    float defenseBonus = 0.0f;
    float coverBonus = 0.0f;
    float visibilityModifier = 1.0f;
    bool blocksLineOfSight = false;
    bool providesHighGround = false;

    // Resources
    std::string resourceType;       // gold, wood, food, stone, etc.
    float resourceAmount = 0.0f;
    float resourceRegen = 0.0f;
    bool isDepletable = false;

    // Building
    bool allowsBuilding = true;
    std::vector<std::string> allowedBuildingTypes;
    float buildingHealthMod = 1.0f;

    // Visual
    std::string texturePath;
    std::string normalMapPath;
    int variations = 1;
    bool hasTransitions = true;
    bool isAnimated = false;

    // Classification
    std::vector<std::string> tags;
    std::string description;
};

/**
 * @brief Tile filter options
 */
struct TileFilterOptions {
    std::string searchQuery;
    std::vector<TerrainType> terrainTypes;
    std::vector<TileBiome> biomes;
    std::vector<Walkability> walkabilities;

    bool showWalkable = true;
    bool showBlocked = true;
    bool showResourceTiles = true;
    bool showBuildableTiles = true;

    std::optional<float> minMovementCost;
    std::optional<float> maxMovementCost;
    std::optional<float> minDefenseBonus;
};

/**
 * @brief Tile palette group
 */
struct TilePaletteGroup {
    std::string name;
    std::vector<std::string> tileIds;
    glm::vec4 color;
    bool expanded = true;
};

/**
 * @brief Tile Browser
 *
 * Specialized browser for tile assets:
 * - Terrain preview
 * - Walkability indicators
 * - Resource types
 * - Biome categories
 * - Palette organization
 */
class TileBrowser {
public:
    explicit TileBrowser(Editor* editor, ContentBrowser* contentBrowser);
    ~TileBrowser();

    // Non-copyable
    TileBrowser(const TileBrowser&) = delete;
    TileBrowser& operator=(const TileBrowser&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    bool Initialize();
    void Shutdown();
    void Render();
    void Update(float deltaTime);

    // =========================================================================
    // Tile Access
    // =========================================================================

    [[nodiscard]] std::vector<TileStats> GetAllTiles() const;
    [[nodiscard]] std::optional<TileStats> GetTile(const std::string& id) const;
    [[nodiscard]] std::vector<TileStats> GetFilteredTiles() const;
    void RefreshTiles();

    // =========================================================================
    // Filtering
    // =========================================================================

    void SetFilter(const TileFilterOptions& filter);
    [[nodiscard]] const TileFilterOptions& GetFilter() const { return m_filter; }
    void FilterByTerrainType(TerrainType type);
    void FilterByBiome(TileBiome biome);
    void FilterByWalkability(Walkability walkability);
    void ClearFilters();

    // =========================================================================
    // Palette Management
    // =========================================================================

    /**
     * @brief Get palette groups
     */
    [[nodiscard]] std::vector<TilePaletteGroup> GetPaletteGroups() const;

    /**
     * @brief Create palette group
     */
    void CreatePaletteGroup(const std::string& name);

    /**
     * @brief Add tile to palette group
     */
    void AddToPaletteGroup(const std::string& groupName, const std::string& tileId);

    /**
     * @brief Remove tile from palette group
     */
    void RemoveFromPaletteGroup(const std::string& groupName, const std::string& tileId);

    /**
     * @brief Delete palette group
     */
    void DeletePaletteGroup(const std::string& name);

    /**
     * @brief Get tiles by terrain type
     */
    [[nodiscard]] std::vector<TileStats> GetTilesByTerrainType(TerrainType type) const;

    /**
     * @brief Get tiles by biome
     */
    [[nodiscard]] std::vector<TileStats> GetTilesByBiome(TileBiome biome) const;

    // =========================================================================
    // Statistics
    // =========================================================================

    [[nodiscard]] std::unordered_map<TerrainType, int> GetTileCountByTerrain() const;
    [[nodiscard]] std::unordered_map<TileBiome, int> GetTileCountByBiome() const;
    [[nodiscard]] std::vector<std::string> GetResourceTypes() const;

    // =========================================================================
    // Preview
    // =========================================================================

    /**
     * @brief Get tile preview texture
     */
    [[nodiscard]] std::string GetTilePreview(const std::string& tileId) const;

    /**
     * @brief Get tile terrain connections preview
     */
    void RenderTerrainPreview(const std::string& tileId, float x, float y, float size);

    // =========================================================================
    // Painting Support
    // =========================================================================

    /**
     * @brief Set selected tile for painting
     */
    void SetPaintTile(const std::string& tileId);

    /**
     * @brief Get currently selected paint tile
     */
    [[nodiscard]] const std::string& GetPaintTile() const { return m_paintTileId; }

    /**
     * @brief Get brush tiles (for multi-tile brush)
     */
    [[nodiscard]] std::vector<std::string> GetBrushTiles() const { return m_brushTiles; }

    /**
     * @brief Set brush tiles
     */
    void SetBrushTiles(const std::vector<std::string>& tileIds);

    /**
     * @brief Toggle tile in brush
     */
    void ToggleBrushTile(const std::string& tileId);

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(const std::string&)> OnTileSelected;
    std::function<void(const std::string&)> OnTileDoubleClicked;
    std::function<void(const std::string&)> OnPaintTileChanged;

private:
    // Rendering
    void RenderToolbar();
    void RenderFilters();
    void RenderPaletteView();
    void RenderTileGrid();
    void RenderTileCard(const TileStats& tile);
    void RenderWalkabilityIcon(Walkability walkability);
    void RenderResourceIcon(const std::string& resourceType);
    void RenderTileTooltip(const TileStats& tile);

    // Data loading
    TileStats LoadTileStats(const std::string& assetId) const;
    void CacheTiles();
    bool MatchesFilter(const TileStats& tile) const;

    // Helpers
    std::string GetTerrainTypeName(TerrainType type) const;
    std::string GetBiomeName(TileBiome biome) const;
    std::string GetWalkabilityName(Walkability walkability) const;
    glm::vec4 GetTerrainColor(TerrainType type) const;
    glm::vec4 GetBiomeColor(TileBiome biome) const;

    Editor* m_editor = nullptr;
    ContentBrowser* m_contentBrowser = nullptr;
    bool m_initialized = false;

    // Cached tiles
    std::vector<TileStats> m_allTiles;
    std::vector<TileStats> m_filteredTiles;
    bool m_needsRefresh = true;

    // Filter state
    TileFilterOptions m_filter;

    // Selection
    std::string m_selectedTileId;
    std::string m_paintTileId;
    std::vector<std::string> m_brushTiles;

    // Palette groups
    std::vector<TilePaletteGroup> m_paletteGroups;

    // View options
    int m_gridColumns = 5;
    bool m_showWalkability = true;
    bool m_showResources = true;
    bool m_paletteView = false;
};

} // namespace Content
} // namespace Vehement
