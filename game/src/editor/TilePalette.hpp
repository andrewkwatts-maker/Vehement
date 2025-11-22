#pragma once

#include "LevelEditor.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <functional>

namespace Nova {
class Texture;
class TextureManager;
}

namespace Vehement {

/**
 * @brief Structure representing a tile entry in the palette
 */
struct TileEntry {
    TileType type = TileType::Empty;
    uint8_t variant = 0;
    std::string name;
    std::string texturePath;
    std::shared_ptr<Nova::Texture> thumbnail;
    bool isFavorite = false;
    int useCount = 0;      ///< For tracking recently used
};

/**
 * @brief Texture atlas for efficient tile rendering
 */
class TileAtlas {
public:
    TileAtlas() = default;
    ~TileAtlas() = default;

    /**
     * @brief Initialize the atlas with a texture manager
     * @param textureManager The texture manager to load textures from
     * @param basePath Base path to tile textures (e.g., "Vehement2/images/")
     */
    void Initialize(Nova::TextureManager& textureManager, const std::string& basePath);

    /**
     * @brief Check if atlas is initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    /**
     * @brief Get texture for a tile type
     * @param type The tile type
     * @return Shared pointer to texture, or nullptr if not found
     */
    [[nodiscard]] std::shared_ptr<Nova::Texture> GetTexture(TileType type) const;

    /**
     * @brief Get all loaded tile entries
     */
    [[nodiscard]] const std::vector<TileEntry>& GetAllTiles() const noexcept { return m_tiles; }

    /**
     * @brief Get tiles by category
     * @param category Category index (from GetTileCategory)
     */
    [[nodiscard]] std::vector<const TileEntry*> GetTilesByCategory(int category) const;

    /**
     * @brief Get tile entry by type
     */
    [[nodiscard]] const TileEntry* GetTileEntry(TileType type) const;

private:
    bool m_initialized = false;
    std::vector<TileEntry> m_tiles;
    std::unordered_map<TileType, size_t> m_tileIndexMap;
    Nova::TextureManager* m_textureManager = nullptr;
    std::string m_basePath;

    void LoadAllTiles();
    void LoadTile(TileType type);
};

/**
 * @brief Tile Palette UI Panel
 *
 * Provides a UI for browsing and selecting tiles organized by category.
 * Features include:
 * - Categorized tile browser matching Vehement2/images/ folder structure
 * - Thumbnail preview of each tile
 * - Search and filter functionality
 * - Recently used tiles tracking
 * - Favorites system
 */
class TilePalette {
public:
    /**
     * @brief Categories matching Vehement2/images/ folders
     */
    enum class Category {
        All = -1,       ///< Show all tiles
        Ground = 1,     ///< Ground tiles (grass, dirt, rocks)
        Concrete = 2,   ///< Roads, sidewalks, concrete surfaces
        Bricks = 3,     ///< Brick walls and surfaces
        Wood = 4,       ///< Wooden structures and flooring
        Stone = 5,      ///< Stone walls and marble
        Metal = 6,      ///< Metal surfaces and shop fronts
        Foliage = 7,    ///< Trees, plants, planters
        Water = 8,      ///< Water features
        Objects = 9,    ///< Props and decorations
        Textiles = 10,  ///< Fabric, carpet, rope
        FadeOut = 11,   ///< Transition/fade tiles
        Favorites = 100,///< User favorite tiles
        Recent = 101    ///< Recently used tiles
    };

    /**
     * @brief Palette configuration
     */
    struct Config {
        int thumbnailSize = 64;             ///< Size of tile thumbnails in pixels
        int tilesPerRow = 4;                ///< Number of tiles per row
        int maxRecentTiles = 12;            ///< Maximum recent tiles to track
        float padding = 4.0f;               ///< Padding between tiles
        glm::vec4 selectedColor{1.0f, 0.8f, 0.0f, 1.0f};  ///< Highlight color
        glm::vec4 hoverColor{1.0f, 1.0f, 1.0f, 0.3f};     ///< Hover color
    };

public:
    TilePalette();
    ~TilePalette();

    // Delete copy, allow move
    TilePalette(const TilePalette&) = delete;
    TilePalette& operator=(const TilePalette&) = delete;
    TilePalette(TilePalette&&) noexcept = default;
    TilePalette& operator=(TilePalette&&) noexcept = default;

    /**
     * @brief Initialize the palette with a tile atlas
     * @param atlas The tile atlas containing loaded tile textures
     * @param config Optional configuration
     */
    void Initialize(const TileAtlas& atlas, const Config& config = {});

    /**
     * @brief Check if palette is initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    /**
     * @brief Render the palette UI
     * This should be called each frame when the palette is visible.
     * Uses ImGui for rendering.
     */
    void Render();

    /**
     * @brief Update palette state
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    // -------------------------------------------------------------------------
    // Category Navigation
    // -------------------------------------------------------------------------

    /**
     * @brief Set the current category to display
     * @param cat The category to show
     */
    void SetCategory(Category cat);

    /**
     * @brief Get the current category
     */
    [[nodiscard]] Category GetCategory() const noexcept { return m_currentCategory; }

    /**
     * @brief Get display name for a category
     */
    [[nodiscard]] static const char* GetCategoryName(Category cat) noexcept;

    /**
     * @brief Get the number of tiles in a category
     */
    [[nodiscard]] int GetTileCount(Category cat) const;

    // -------------------------------------------------------------------------
    // Tile Selection
    // -------------------------------------------------------------------------

    /**
     * @brief Get the currently selected tile type
     */
    [[nodiscard]] TileType GetSelectedTile() const noexcept { return m_selectedTile; }

    /**
     * @brief Get the currently selected tile variant
     */
    [[nodiscard]] uint8_t GetSelectedVariant() const noexcept { return m_selectedVariant; }

    /**
     * @brief Set the selected tile
     * @param type Tile type to select
     * @param variant Tile variant
     */
    void SetSelectedTile(TileType type, uint8_t variant = 0);

    /**
     * @brief Get the tile entry for the selected tile
     */
    [[nodiscard]] const TileEntry* GetSelectedEntry() const;

    // -------------------------------------------------------------------------
    // Input Handling
    // -------------------------------------------------------------------------

    /**
     * @brief Handle click on the palette
     * @param screenPos Click position in screen coordinates
     * @return true if click was handled
     */
    bool OnClick(const glm::vec2& screenPos);

    /**
     * @brief Handle mouse move for hover effects
     * @param screenPos Mouse position in screen coordinates
     */
    void OnMouseMove(const glm::vec2& screenPos);

    // -------------------------------------------------------------------------
    // Search and Filter
    // -------------------------------------------------------------------------

    /**
     * @brief Set search filter text
     * @param filter Text to filter tiles by name
     */
    void SetFilter(const std::string& filter);

    /**
     * @brief Get current filter text
     */
    [[nodiscard]] const std::string& GetFilter() const noexcept { return m_filterText; }

    /**
     * @brief Clear the search filter
     */
    void ClearFilter();

    // -------------------------------------------------------------------------
    // Favorites
    // -------------------------------------------------------------------------

    /**
     * @brief Toggle favorite status for a tile
     * @param type Tile type to toggle
     */
    void ToggleFavorite(TileType type);

    /**
     * @brief Check if a tile is favorited
     */
    [[nodiscard]] bool IsFavorite(TileType type) const;

    /**
     * @brief Get all favorite tile types
     */
    [[nodiscard]] std::vector<TileType> GetFavorites() const;

    /**
     * @brief Clear all favorites
     */
    void ClearFavorites();

    // -------------------------------------------------------------------------
    // Recent Tiles
    // -------------------------------------------------------------------------

    /**
     * @brief Add a tile to recent list (called when tile is used)
     * @param type Tile type that was used
     */
    void AddToRecent(TileType type);

    /**
     * @brief Get recent tiles list
     */
    [[nodiscard]] const std::vector<TileType>& GetRecentTiles() const noexcept {
        return m_recentTiles;
    }

    /**
     * @brief Clear recent tiles
     */
    void ClearRecent();

    // -------------------------------------------------------------------------
    // Layout
    // -------------------------------------------------------------------------

    /**
     * @brief Set the palette bounds in screen space
     * @param position Top-left corner position
     * @param size Width and height of the palette area
     */
    void SetBounds(const glm::vec2& position, const glm::vec2& size);

    /**
     * @brief Get palette bounds
     */
    [[nodiscard]] const glm::vec2& GetPosition() const noexcept { return m_position; }
    [[nodiscard]] const glm::vec2& GetSize() const noexcept { return m_size; }

    /**
     * @brief Get scroll offset for the tile list
     */
    [[nodiscard]] float GetScrollOffset() const noexcept { return m_scrollOffset; }

    /**
     * @brief Set scroll offset
     */
    void SetScrollOffset(float offset);

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    /**
     * @brief Callback when a tile is selected
     */
    std::function<void(TileType, uint8_t)> OnTileSelected;

    /**
     * @brief Callback when category changes
     */
    std::function<void(Category)> OnCategoryChanged;

private:
    // Render helpers
    void RenderCategoryTabs();
    void RenderSearchBar();
    void RenderTileGrid();
    void RenderTileTooltip(const TileEntry& entry);
    void RenderPreviewPanel();

    // Get filtered tiles for current view
    std::vector<const TileEntry*> GetVisibleTiles() const;

    // Hit testing
    int GetTileIndexAtPosition(const glm::vec2& screenPos) const;
    glm::vec2 GetTilePosition(int index) const;

    // Update recent tiles
    void UpdateRecentTile(TileType type);

private:
    bool m_initialized = false;

    // Configuration
    Config m_config;

    // Reference to atlas
    const TileAtlas* m_atlas = nullptr;

    // Current state
    Category m_currentCategory = Category::Ground;
    TileType m_selectedTile = TileType::GroundGrass1;
    uint8_t m_selectedVariant = 0;
    TileType m_hoveredTile = TileType::Empty;

    // Layout
    glm::vec2 m_position{0.0f};
    glm::vec2 m_size{300.0f, 400.0f};
    float m_scrollOffset = 0.0f;
    float m_maxScroll = 0.0f;

    // Search/filter
    std::string m_filterText;
    std::vector<const TileEntry*> m_filteredTiles;
    bool m_filterDirty = true;

    // Favorites and recent
    std::vector<TileType> m_favorites;
    std::vector<TileType> m_recentTiles;

    // Animation
    float m_hoverAnimTime = 0.0f;
    float m_selectAnimTime = 0.0f;
};

} // namespace Vehement
