#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <glm/glm.hpp>

namespace Vehement {

class MapEditor;

/**
 * @brief Asset category for the palette
 */
enum class AssetCategory : uint8_t {
    All,
    Units,
    Buildings,
    Doodads,        // Decorative props
    Resources,      // Trees, rocks, etc.
    Environment,    // Terrain features
    Lights,
    Triggers,
    Recent,
    Favorites
};

/**
 * @brief Asset entry in the palette
 */
struct AssetEntry {
    std::string id;
    std::string name;
    std::string displayName;
    AssetCategory category;
    std::string subCategory;

    // Visual
    std::string iconPath;
    std::string modelPath;
    std::string thumbnailPath;

    // Placement properties
    glm::vec3 defaultScale{1.0f};
    float defaultRotation = 0.0f;
    bool snapToGrid = true;
    float gridSize = 1.0f;
    bool canRotate = true;
    bool canScale = true;

    // Metadata
    std::string description;
    std::vector<std::string> tags;
    bool isCustom = false;
    int useCount = 0;  // Track popularity
};

/**
 * @brief Asset Palette - Browse and select assets to place in the scene
 *
 * Features:
 * - Organized by category (Units, Buildings, Doodads, etc.)
 * - Search and filter
 * - Thumbnail previews
 * - Drag and drop to viewport
 * - Recent and favorite assets
 * - Custom asset support
 */
class AssetPalette {
public:
    AssetPalette();
    ~AssetPalette();

    // Delete copy, allow move
    AssetPalette(const AssetPalette&) = delete;
    AssetPalette& operator=(const AssetPalette&) = delete;
    AssetPalette(AssetPalette&&) noexcept = default;
    AssetPalette& operator=(AssetPalette&&) noexcept = default;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the asset palette
     * @param mapEditor Reference to map editor
     * @return true if successful
     */
    bool Initialize(MapEditor& mapEditor);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    // =========================================================================
    // Update and Render
    // =========================================================================

    /**
     * @brief Update palette state
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    /**
     * @brief Render the palette UI
     */
    void Render();

    // =========================================================================
    // Asset Management
    // =========================================================================

    /**
     * @brief Load assets from game data
     */
    void LoadAssets();

    /**
     * @brief Add custom asset
     * @param entry Asset entry to add
     */
    void AddCustomAsset(const AssetEntry& entry);

    /**
     * @brief Remove custom asset
     * @param id Asset ID
     */
    void RemoveCustomAsset(const std::string& id);

    /**
     * @brief Get asset by ID
     */
    [[nodiscard]] const AssetEntry* GetAsset(const std::string& id) const;

    /**
     * @brief Get all assets in category
     */
    [[nodiscard]] std::vector<const AssetEntry*> GetAssetsByCategory(AssetCategory category) const;

    // =========================================================================
    // Category and Filter
    // =========================================================================

    /**
     * @brief Set current category
     */
    void SetCategory(AssetCategory category);

    /**
     * @brief Get current category
     */
    [[nodiscard]] AssetCategory GetCategory() const noexcept { return m_currentCategory; }

    /**
     * @brief Set search filter
     */
    void SetFilter(const std::string& filter);

    /**
     * @brief Clear search filter
     */
    void ClearFilter();

    /**
     * @brief Get category name
     */
    [[nodiscard]] static const char* GetCategoryName(AssetCategory category) noexcept;

    // =========================================================================
    // Selection
    // =========================================================================

    /**
     * @brief Select asset
     * @param id Asset ID
     */
    void SelectAsset(const std::string& id);

    /**
     * @brief Get selected asset ID
     */
    [[nodiscard]] const std::string& GetSelectedAssetId() const noexcept { return m_selectedAssetId; }

    /**
     * @brief Get selected asset entry
     */
    [[nodiscard]] const AssetEntry* GetSelectedAsset() const;

    /**
     * @brief Clear selection
     */
    void ClearSelection();

    // =========================================================================
    // Favorites
    // =========================================================================

    /**
     * @brief Toggle favorite status
     */
    void ToggleFavorite(const std::string& id);

    /**
     * @brief Check if asset is favorite
     */
    [[nodiscard]] bool IsFavorite(const std::string& id) const;

    /**
     * @brief Get all favorites
     */
    [[nodiscard]] const std::vector<std::string>& GetFavorites() const noexcept { return m_favorites; }

    // =========================================================================
    // Recent Assets
    // =========================================================================

    /**
     * @brief Add to recent list
     */
    void AddToRecent(const std::string& id);

    /**
     * @brief Get recent assets
     */
    [[nodiscard]] const std::vector<std::string>& GetRecent() const noexcept { return m_recent; }

    /**
     * @brief Clear recent list
     */
    void ClearRecent();

    // =========================================================================
    // Drag and Drop
    // =========================================================================

    /**
     * @brief Check if dragging an asset
     */
    [[nodiscard]] bool IsDragging() const noexcept { return m_isDragging; }

    /**
     * @brief Get asset being dragged
     */
    [[nodiscard]] const std::string& GetDraggedAssetId() const noexcept { return m_draggedAssetId; }

    /**
     * @brief Start dragging an asset
     */
    void StartDrag(const std::string& id);

    /**
     * @brief Stop dragging
     */
    void StopDrag();

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(const std::string&)> OnAssetSelected;
    std::function<void(const std::string&)> OnAssetDragStart;
    std::function<void(const std::string&)> OnAssetDragEnd;
    std::function<void(const std::string&, const glm::vec3&)> OnAssetPlaced;

private:
    // Rendering helpers
    void RenderCategoryTabs();
    void RenderSearchBar();
    void RenderAssetGrid();
    void RenderAssetCard(const AssetEntry& entry, int index);
    void RenderAssetTooltip(const AssetEntry& entry);
    void RenderPreviewPanel();

    // Filtering
    [[nodiscard]] std::vector<const AssetEntry*> GetFilteredAssets() const;
    [[nodiscard]] bool MatchesFilter(const AssetEntry& entry) const;

    // Update helpers
    void UpdateRecent(const std::string& id);

    // State
    bool m_initialized = false;
    MapEditor* m_mapEditor = nullptr;

    // Asset data
    std::vector<AssetEntry> m_assets;
    std::unordered_map<std::string, size_t> m_assetIndexMap;

    // UI state
    AssetCategory m_currentCategory = AssetCategory::All;
    std::string m_searchFilter;
    std::string m_selectedAssetId;
    std::string m_hoveredAssetId;

    // Favorites and recent
    std::vector<std::string> m_favorites;
    std::vector<std::string> m_recent;
    static constexpr size_t MAX_RECENT = 10;

    // Drag and drop
    bool m_isDragging = false;
    std::string m_draggedAssetId;
    glm::vec2 m_dragStartPos{0.0f};

    // UI layout
    int m_gridColumns = 4;
    float m_cardWidth = 80.0f;
    float m_cardHeight = 100.0f;
    float m_thumbnailSize = 64.0f;
    float m_padding = 8.0f;

    // Animation
    float m_hoverAnimTime = 0.0f;
};

} // namespace Vehement
