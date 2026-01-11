#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

namespace Vehement {

class Editor;

/**
 * @brief Enhanced asset browser with improved filtering and preview
 *
 * Enhancements over basic AssetBrowser:
 * - Asset type categorization (units, buildings, tiles, etc.)
 * - Visual thumbnails with caching
 * - Advanced filtering by type, tags, and search
 * - Quick preview panel with 3D model viewer
 * - Integration with ConfigEditor for inline editing
 * - Support for asset creation from templates
 */
class AssetBrowserEnhanced {
public:
    enum class AssetCategory {
        All,
        Units,
        Buildings,
        Tiles,
        Models,
        Textures,
        Scripts,
        Configs,
        Locations,
        Spells,
        Items
    };

    struct AssetInfo {
        std::string id;
        std::string name;
        std::string path;
        std::string extension;
        AssetCategory category;
        std::vector<std::string> tags;
        size_t size;
        bool isDirectory;
        unsigned int thumbnailTexture;  // OpenGL texture ID for thumbnail
    };

    explicit AssetBrowserEnhanced(Editor* editor);
    ~AssetBrowserEnhanced();

    void Render();
    void Update(float deltaTime);

    // Asset selection
    void SelectAsset(const std::string& path);
    const std::string& GetSelectedAsset() const { return m_selectedAssetPath; }

    // Filtering
    void SetCategoryFilter(AssetCategory category);
    void SetSearchFilter(const std::string& filter);
    void SetTagFilter(const std::vector<std::string>& tags);

    // Asset operations
    void CreateNewAsset(AssetCategory category, const std::string& templateId = "");
    void DuplicateAsset(const std::string& path);
    void DeleteAsset(const std::string& path);
    void RenameAsset(const std::string& oldPath, const std::string& newPath);

    // Preview
    void EnablePreview(bool enable) { m_showPreview = enable; }
    bool IsPreviewEnabled() const { return m_showPreview; }

    // Callbacks
    std::function<void(const std::string&)> OnAssetSelected;
    std::function<void(const std::string&)> OnAssetDoubleClicked;
    std::function<void(const std::string&, AssetCategory)> OnAssetCreated;
    std::function<void(const std::string&)> OnAssetDeleted;

private:
    void RenderToolbar();
    void RenderCategoryTabs();
    void RenderSearchBar();
    void RenderAssetGrid();
    void RenderAssetList();
    void RenderPreviewPanel();
    void RenderContextMenu();
    void RenderCreateAssetDialog();

    // Asset scanning and loading
    void RefreshAssets();
    void ScanDirectory(const std::string& path, AssetCategory category);
    AssetCategory DetermineCategory(const std::string& path, const std::string& ext);
    void LoadThumbnail(AssetInfo& asset);
    unsigned int GeneratePlaceholderThumbnail(AssetCategory category);

    // Preview rendering
    void RenderModelPreview(const AssetInfo& asset);
    void RenderTexturePreview(const AssetInfo& asset);
    void RenderConfigPreview(const AssetInfo& asset);

    // Asset templates
    struct AssetTemplate {
        std::string id;
        std::string name;
        std::string description;
        AssetCategory category;
        std::string jsonTemplate;
    };
    std::vector<AssetTemplate> GetTemplatesForCategory(AssetCategory category);

    Editor* m_editor = nullptr;

    // Asset database
    std::vector<AssetInfo> m_allAssets;
    std::vector<AssetInfo*> m_filteredAssets;

    // Selection and view state
    std::string m_selectedAssetPath;
    AssetInfo* m_selectedAsset = nullptr;

    // Filtering
    AssetCategory m_categoryFilter = AssetCategory::All;
    std::string m_searchFilter;
    std::vector<std::string> m_tagFilter;

    // View options
    bool m_showAsGrid = true;
    int m_thumbnailSize = 128;
    bool m_showPreview = true;

    // UI state
    bool m_showCreateDialog = false;
    AssetCategory m_createCategory = AssetCategory::Units;
    char m_createNameBuffer[256] = "";
    int m_selectedTemplateIdx = 0;

    // Thumbnail cache
    std::unordered_map<std::string, unsigned int> m_thumbnailCache;

    // Preview state
    float m_previewRotation = 0.0f;
    float m_previewZoom = 1.0f;
};

} // namespace Vehement
