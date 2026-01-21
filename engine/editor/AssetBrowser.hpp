/**
 * @file AssetBrowser.hpp
 * @brief Asset browser panel for the Nova3D/Vehement editor
 *
 * Provides a comprehensive file browser for managing project assets with:
 * - Multiple view modes (Grid, List, Column)
 * - Folder tree navigation with breadcrumbs
 * - Drag-and-drop for moving files and instantiating in scene
 * - Multi-selection and batch operations
 * - Async thumbnail generation with caching
 * - Search and filtering by name/type
 * - Import pipeline for external files
 * - Context menus for file operations
 */

#pragma once

#include "../ui/EditorPanel.hpp"
#include "../ui/EditorWidgets.hpp"
#include "../graphics/Texture.hpp"
#include "AssetThumbnailCache.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <memory>
#include <filesystem>
#include <chrono>

namespace fs = std::filesystem;

namespace Nova {

// Forward declarations
class CommandHistory;
class Scene;

// =============================================================================
// Asset Types
// =============================================================================

/**
 * @brief Types of assets that can be browsed
 */
enum class AssetType {
    Unknown,
    Folder,
    SDFModel,       ///< SDF model (.sdf, .nova)
    Mesh,           ///< Polygon mesh (.obj, .fbx, .gltf)
    Texture,        ///< Image texture (.png, .jpg, .tga, .hdr)
    Material,       ///< Material definition (.mat, .material)
    Animation,      ///< Animation clip (.anim)
    Audio,          ///< Audio file (.wav, .ogg, .mp3)
    Script,         ///< Script file (.lua, .py)
    Prefab,         ///< Prefab template (.prefab)
    Scene,          ///< Scene file (.scene)
    Shader          ///< Shader program (.glsl, .hlsl, .vert, .frag)
};

/**
 * @brief Get icon string for asset type
 */
const char* GetAssetTypeIcon(AssetType type);

/**
 * @brief Get display name for asset type
 */
const char* GetAssetTypeName(AssetType type);

/**
 * @brief Get file filter string for asset type
 */
const char* GetAssetTypeFilter(AssetType type);

/**
 * @brief Detect asset type from file extension
 */
AssetType DetectAssetType(const std::string& extension);

/**
 * @brief Get color for asset type badge
 */
glm::vec4 GetAssetTypeColor(AssetType type);

// =============================================================================
// Asset Entry
// =============================================================================

/**
 * @brief Represents a single asset in the browser
 */
struct AssetEntry {
    std::string path;               ///< Full absolute path
    std::string filename;           ///< Filename with extension
    std::string displayName;        ///< Name without extension (for display)
    std::string extension;          ///< File extension (lowercase, with dot)
    AssetType type = AssetType::Unknown;

    // Metadata
    uint64_t fileSize = 0;          ///< File size in bytes
    std::chrono::system_clock::time_point modifiedTime;  ///< Last modification time

    // Thumbnail
    std::shared_ptr<Texture> thumbnail;
    bool thumbnailLoading = false;
    bool thumbnailFailed = false;

    // UI state
    bool isSelected = false;
    bool isRenaming = false;
    bool isCut = false;             ///< Marked for cut operation
    bool isHovered = false;

    /**
     * @brief Check if this is a folder
     */
    [[nodiscard]] bool IsFolder() const { return type == AssetType::Folder; }

    /**
     * @brief Get formatted file size string
     */
    [[nodiscard]] std::string GetFormattedSize() const;

    /**
     * @brief Get formatted modification time string
     */
    [[nodiscard]] std::string GetFormattedTime() const;
};

// =============================================================================
// View Modes
// =============================================================================

/**
 * @brief Asset browser view mode
 */
enum class AssetViewMode {
    Grid,       ///< Thumbnail grid
    List,       ///< Detailed list with columns
    Column      ///< Finder-style column view
};

/**
 * @brief Sort criteria for assets
 */
enum class AssetSortBy {
    Name,
    Type,
    Size,
    DateModified
};

/**
 * @brief Sort direction
 */
enum class SortDirection {
    Ascending,
    Descending
};

// =============================================================================
// Favorites/Bookmarks
// =============================================================================

/**
 * @brief Bookmark entry for quick access
 */
struct AssetBookmark {
    std::string name;               ///< Display name
    std::string path;               ///< Full path
    std::string icon;               ///< Optional custom icon
    glm::vec4 color{1.0f};          ///< Bookmark color
    bool isBuiltIn = false;         ///< System bookmark (cannot delete)
};

// =============================================================================
// Import Settings
// =============================================================================

/**
 * @brief Settings for importing assets
 */
struct ImportSettings {
    // Texture settings
    bool textureGenerateMipmaps = true;
    bool textureSRGB = true;
    int textureMaxSize = 4096;
    bool textureCompress = true;

    // Mesh settings
    bool meshImportNormals = true;
    bool meshImportTangents = true;
    bool meshImportUVs = true;
    bool meshOptimize = true;
    float meshScale = 1.0f;

    // Animation settings
    bool animationImportAll = true;
    float animationSampleRate = 30.0f;

    // Audio settings
    bool audioCompress = true;
    int audioSampleRate = 44100;
};

// =============================================================================
// Drag and Drop
// =============================================================================

/**
 * @brief Drag payload for assets
 */
struct AssetDragPayload {
    std::vector<std::string> paths;
    AssetType primaryType = AssetType::Unknown;
    bool isValid = false;
};

/**
 * @brief External file drop info
 */
struct ExternalFileDrop {
    std::vector<std::string> paths;
    std::string targetFolder;
};

// =============================================================================
// AI Asset Generation Settings
// =============================================================================

/**
 * @brief Settings for AI-powered asset generation
 */
struct AIGenerationSettings {
    // Text prompt
    std::string assetDescription;       ///< Text description of asset to generate

    // References
    std::string referenceImagePath;     ///< Optional reference concept art

    // Asset configuration
    AssetType targetAssetType = AssetType::SDFModel;  ///< Type of asset to generate

    // Quality settings
    enum class GenerationQuality {
        Draft,      ///< Fast generation with basic quality
        Standard,   ///< Balanced quality and generation time
        High,       ///< High quality with longer generation time
        Ultra       ///< Maximum quality for important assets
    };
    GenerationQuality quality = GenerationQuality::Standard;

    // Model-specific settings
    struct SDFModelParams {
        float detailLevel = 0.5f;       ///< Detail level (0.0-1.0)
        float complexity = 0.5f;        ///< Shape complexity (0.0-1.0)
        bool enableSmoothing = true;    ///< Apply smoothing to generated model
    };

    struct MaterialParams {
        float metallicBias = 0.5f;      ///< Metallic to diffuse bias
        float roughnessBias = 0.5f;     ///< Roughness level
        bool enablePBR = true;          ///< Use PBR textures
    };

    struct TextureParams {
        int resolution = 2048;          ///< Output texture resolution (512-4096)
        bool generateNormals = true;    ///< Auto-generate normal maps
        bool generateRoughness = true;  ///< Auto-generate roughness maps
        bool generateMetallic = true;   ///< Auto-generate metallic maps
    };

    SDFModelParams sdfParams;
    MaterialParams materialParams;
    TextureParams textureParams;

    // Generation state
    bool isGenerating = false;          ///< Currently generating asset
    float generationProgress = 0.0f;    ///< Progress 0.0-1.0
    std::string generationStatus;       ///< Current status message
};

// =============================================================================
// Callbacks
// =============================================================================

/**
 * @brief Callback signatures for asset browser events
 */
struct AssetBrowserCallbacks {
    /// Called when an asset is opened (double-click)
    std::function<void(const std::string& path, AssetType type)> onAssetOpened;

    /// Called when selection changes
    std::function<void(const std::vector<std::string>& paths)> onSelectionChanged;

    /// Called when an asset is dragged to scene
    std::function<void(const std::string& path, AssetType type)> onAssetDroppedToScene;

    /// Called when an asset is assigned to a property
    std::function<void(const std::string& path, const std::string& propertyPath)> onAssetAssigned;

    /// Called when assets are imported
    std::function<void(const std::vector<std::string>& paths)> onAssetsImported;

    /// Called when assets are deleted
    std::function<void(const std::vector<std::string>& paths)> onAssetsDeleted;

    /// Called when assets are moved/renamed
    std::function<void(const std::string& oldPath, const std::string& newPath)> onAssetMoved;

    /// Called when directory changes
    std::function<void(const std::string& path)> onDirectoryChanged;
};

// =============================================================================
// Folder Tree Node
// =============================================================================

/**
 * @brief Node in the folder tree sidebar
 */
struct FolderTreeNode {
    std::string path;
    std::string name;
    std::vector<std::unique_ptr<FolderTreeNode>> children;
    FolderTreeNode* parent = nullptr;

    bool expanded = false;
    bool selected = false;
    bool hasSubfolders = true;      ///< Lazy-loaded
    bool childrenLoaded = false;    ///< Children have been scanned

    /**
     * @brief Load children from filesystem
     */
    void LoadChildren();

    /**
     * @brief Find child by path
     */
    FolderTreeNode* FindChild(const std::string& childPath);
};

// =============================================================================
// Search Result
// =============================================================================

/**
 * @brief Search result entry
 */
struct SearchResult {
    AssetEntry entry;
    std::string matchedOn;          ///< What matched (name, path, type)
    float relevance = 0.0f;         ///< Match relevance score
};

// =============================================================================
// Asset Browser Panel
// =============================================================================

/**
 * @brief Main asset browser panel
 *
 * Features:
 * - Multiple view modes (Grid, List, Column)
 * - Folder tree sidebar with favorites
 * - Breadcrumb navigation with history
 * - Search and filtering
 * - Async thumbnail generation
 * - Drag-and-drop operations
 * - File operations (create, rename, delete, move, copy)
 * - Import pipeline for external files
 * - Context menus
 *
 * Usage:
 *   AssetBrowser browser;
 *   browser.SetRootPath("assets/");
 *   browser.Callbacks.onAssetOpened = [](const std::string& path, AssetType type) {
 *       // Open asset in appropriate editor
 *   };
 *   // In render loop:
 *   browser.Render();
 */
class AssetBrowser : public EditorPanel {
public:
    AssetBrowser();
    ~AssetBrowser() override;

    // Non-copyable
    AssetBrowser(const AssetBrowser&) = delete;
    AssetBrowser& operator=(const AssetBrowser&) = delete;

    // =========================================================================
    // Path Management
    // =========================================================================

    /**
     * @brief Set the root path for browsing
     * @param path Absolute or relative path to asset root
     */
    void SetRootPath(const std::string& path);

    /**
     * @brief Get current root path
     */
    [[nodiscard]] const std::string& GetRootPath() const { return m_rootPath; }

    /**
     * @brief Navigate to a specific path
     * @param path Path to navigate to (must be within root)
     */
    void NavigateTo(const std::string& path);

    /**
     * @brief Navigate to parent directory
     */
    void NavigateUp();

    /**
     * @brief Navigate back in history
     */
    void NavigateBack();

    /**
     * @brief Navigate forward in history
     */
    void NavigateForward();

    /**
     * @brief Get current browsing path
     */
    [[nodiscard]] const std::string& GetCurrentPath() const { return m_currentPath; }

    /**
     * @brief Check if back navigation is available
     */
    [[nodiscard]] bool CanNavigateBack() const { return m_historyIndex > 0; }

    /**
     * @brief Check if forward navigation is available
     */
    [[nodiscard]] bool CanNavigateForward() const { return m_historyIndex < m_history.size() - 1; }

    /**
     * @brief Refresh current directory
     */
    void Refresh();

    // =========================================================================
    // View Configuration
    // =========================================================================

    /**
     * @brief Set view mode
     */
    void SetViewMode(AssetViewMode mode);

    /**
     * @brief Get current view mode
     */
    [[nodiscard]] AssetViewMode GetViewMode() const { return m_viewMode; }

    /**
     * @brief Set icon size for grid view (32-256 pixels)
     */
    void SetIconSize(int size);

    /**
     * @brief Get current icon size
     */
    [[nodiscard]] int GetIconSize() const { return m_iconSize; }

    /**
     * @brief Set sort criteria
     */
    void SetSortBy(AssetSortBy sortBy, SortDirection direction = SortDirection::Ascending);

    /**
     * @brief Get current sort criteria
     */
    [[nodiscard]] AssetSortBy GetSortBy() const { return m_sortBy; }

    /**
     * @brief Get current sort direction
     */
    [[nodiscard]] SortDirection GetSortDirection() const { return m_sortDirection; }

    /**
     * @brief Toggle folder tree sidebar visibility
     */
    void SetShowFolderTree(bool show) { m_showFolderTree = show; }

    /**
     * @brief Get folder tree visibility
     */
    [[nodiscard]] bool GetShowFolderTree() const { return m_showFolderTree; }

    /**
     * @brief Set whether to show hidden files
     */
    void SetShowHiddenFiles(bool show);

    // =========================================================================
    // Selection
    // =========================================================================

    /**
     * @brief Get selected asset paths
     */
    [[nodiscard]] std::vector<std::string> GetSelectedPaths() const;

    /**
     * @brief Get selected asset entries
     */
    [[nodiscard]] std::vector<const AssetEntry*> GetSelectedEntries() const;

    /**
     * @brief Select an asset by path
     */
    void Select(const std::string& path, bool addToSelection = false);

    /**
     * @brief Clear all selection
     */
    void ClearSelection();

    /**
     * @brief Select all assets in current directory
     */
    void SelectAll();

    /**
     * @brief Get number of selected items
     */
    [[nodiscard]] size_t GetSelectionCount() const;

    // =========================================================================
    // Search and Filter
    // =========================================================================

    /**
     * @brief Set search query
     */
    void SetSearchQuery(const std::string& query);

    /**
     * @brief Get current search query
     */
    [[nodiscard]] const std::string& GetSearchQuery() const { return m_searchQuery; }

    /**
     * @brief Clear search
     */
    void ClearSearch();

    /**
     * @brief Set type filter (show only specific types)
     */
    void SetTypeFilter(AssetType type, bool enabled);

    /**
     * @brief Clear all type filters
     */
    void ClearTypeFilters();

    /**
     * @brief Enable recursive search in subdirectories
     */
    void SetRecursiveSearch(bool recursive) { m_recursiveSearch = recursive; }

    /**
     * @brief Get recursive search state
     */
    [[nodiscard]] bool GetRecursiveSearch() const { return m_recursiveSearch; }

    // =========================================================================
    // File Operations
    // =========================================================================

    /**
     * @brief Create a new folder
     */
    bool CreateFolder(const std::string& name);

    /**
     * @brief Create a new asset of given type
     */
    bool CreateAsset(AssetType type, const std::string& name);

    /**
     * @brief Start renaming selected asset (F2)
     */
    void RenameSelected();

    /**
     * @brief Delete selected assets (with confirmation)
     */
    void DeleteSelected();

    /**
     * @brief Duplicate selected assets
     */
    void DuplicateSelected();

    /**
     * @brief Cut selected assets for move
     */
    void CutSelected();

    /**
     * @brief Copy selected assets
     */
    void CopySelected();

    /**
     * @brief Paste cut/copied assets
     */
    void Paste();

    /**
     * @brief Check if clipboard has assets
     */
    [[nodiscard]] bool HasClipboard() const { return !m_clipboard.empty(); }

    // =========================================================================
    // Bookmarks
    // =========================================================================

    /**
     * @brief Add current path to bookmarks
     */
    void AddBookmark(const std::string& name = "");

    /**
     * @brief Remove a bookmark
     */
    void RemoveBookmark(const std::string& path);

    /**
     * @brief Get all bookmarks
     */
    [[nodiscard]] const std::vector<AssetBookmark>& GetBookmarks() const { return m_bookmarks; }

    /**
     * @brief Navigate to a bookmark
     */
    void GoToBookmark(const std::string& path);

    // =========================================================================
    // Import
    // =========================================================================

    /**
     * @brief Import external files into current directory
     */
    void ImportFiles(const std::vector<std::string>& paths);

    /**
     * @brief Show import dialog
     */
    void ShowImportDialog();

    /**
     * @brief Get current import settings
     */
    [[nodiscard]] ImportSettings& GetImportSettings() { return m_importSettings; }

    // =========================================================================
    // AI Asset Generation
    // =========================================================================

    /**
     * @brief Show the AI-powered asset generation dialog
     *
     * Opens a dialog that allows users to generate new assets using AI,
     * including:
     * - Text descriptions for asset generation
     * - Reference concept art uploads
     * - Asset type selection (SDF Model, Material, Texture, etc.)
     * - Quality and parameter configuration
     */
    void ShowAIGenerateAssetDialog();

    /**
     * @brief Check if AI generate dialog is currently visible
     */
    [[nodiscard]] bool IsAIGenerateDialogVisible() const { return m_showAIGenerateDialog; }

    /**
     * @brief Close the AI generate dialog
     */
    void CloseAIGenerateAssetDialog() { m_showAIGenerateDialog = false; }

    /**
     * @brief Get current AI generation settings
     */
    [[nodiscard]] AIGenerationSettings& GetAIGenerationSettings() { return m_aiGenerationSettings; }

    // =========================================================================
    // Integration
    // =========================================================================

    /**
     * @brief Set the thumbnail cache
     */
    void SetThumbnailCache(AssetThumbnailCache* cache) { m_thumbnailCache = cache; }

    /**
     * @brief Set command history for undo/redo
     */
    void SetCommandHistory(CommandHistory* history) { m_commandHistory = history; }

    /**
     * @brief Set the scene (for drag-drop instantiation)
     */
    void SetScene(Scene* scene) { m_scene = scene; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    AssetBrowserCallbacks Callbacks;

protected:
    // =========================================================================
    // EditorPanel Overrides
    // =========================================================================

    void OnRender() override;
    void OnRenderToolbar() override;
    void OnRenderMenuBar() override;
    void OnRenderStatusBar() override;
    void OnSearchChanged(const std::string& filter) override;
    void OnInitialize() override;
    void OnShutdown() override;
    void Update(float deltaTime) override;

private:
    // =========================================================================
    // Internal Rendering
    // =========================================================================

    void RenderFolderTree();
    void RenderFolderTreeNode(FolderTreeNode* node);
    void RenderBreadcrumbs();
    void RenderContentArea();
    void RenderGridView();
    void RenderListView();
    void RenderColumnView();
    void RenderAssetTile(AssetEntry& entry, const glm::vec2& pos, const glm::vec2& size);
    void RenderAssetListRow(AssetEntry& entry, int rowIndex);
    void RenderColumnViewColumn(const std::string& path, int columnIndex);
    void RenderBookmarks();
    void RenderContextMenu();
    void RenderCreateAssetPopup();
    void RenderDeleteConfirmation();
    void RenderImportDialog();
    void RenderRenamePopup();
    void RenderSearchResults();
    void RenderAIGenerateAssetDialog();

    // =========================================================================
    // Directory Scanning
    // =========================================================================

    void ScanDirectory(const std::string& path);
    void ScanDirectoryRecursive(const std::string& path, std::vector<AssetEntry>& results);
    void BuildFolderTree();
    void UpdateFolderTreeSelection();
    AssetEntry CreateAssetEntry(const fs::path& path);

    // =========================================================================
    // Sorting and Filtering
    // =========================================================================

    void SortEntries();
    void ApplyFilters();
    bool MatchesFilter(const AssetEntry& entry) const;
    bool MatchesSearch(const AssetEntry& entry) const;
    void PerformSearch();

    // =========================================================================
    // Selection Handling
    // =========================================================================

    void HandleSelection(AssetEntry& entry, bool ctrlHeld, bool shiftHeld);
    void HandleDoubleClick(AssetEntry& entry);
    int GetEntryIndex(const AssetEntry& entry) const;
    AssetEntry* GetEntryByPath(const std::string& path);

    // =========================================================================
    // Drag and Drop
    // =========================================================================

    bool HandleDragSource(AssetEntry& entry);
    bool HandleDropTarget();
    bool HandleExternalFileDrop(const std::vector<std::string>& paths);
    void ExecuteMove(const std::vector<std::string>& sources, const std::string& destination);

    // =========================================================================
    // Thumbnails
    // =========================================================================

    void RequestThumbnail(AssetEntry& entry);
    void UpdateThumbnails();
    std::shared_ptr<Texture> GetDefaultIcon(AssetType type);

    // =========================================================================
    // Keyboard Handling
    // =========================================================================

    void HandleKeyboardInput();
    void NavigateSelection(int dx, int dy);

    // =========================================================================
    // History
    // =========================================================================

    void PushHistory(const std::string& path);

    // =========================================================================
    // Utility
    // =========================================================================

    std::string GetRelativePath(const std::string& absolutePath) const;
    std::string GetAbsolutePath(const std::string& relativePath) const;
    bool IsPathWithinRoot(const std::string& path) const;
    std::string GenerateUniqueName(const std::string& baseName, const std::string& extension);
    void ShowInExplorer(const std::string& path);
    void CopyPathToClipboard(const std::string& path);

    // =========================================================================
    // Member Variables
    // =========================================================================

    // Path management
    std::string m_rootPath;
    std::string m_currentPath;
    std::deque<std::string> m_history;
    size_t m_historyIndex = 0;
    static constexpr size_t MAX_HISTORY = 50;

    // Folder tree
    std::unique_ptr<FolderTreeNode> m_folderTreeRoot;
    FolderTreeNode* m_selectedFolder = nullptr;
    bool m_showFolderTree = true;
    float m_folderTreeWidth = 200.0f;

    // Content entries
    std::vector<AssetEntry> m_entries;
    std::vector<AssetEntry*> m_filteredEntries;     ///< Pointers to visible entries
    bool m_needsRescan = true;

    // View settings
    AssetViewMode m_viewMode = AssetViewMode::Grid;
    int m_iconSize = 96;
    int m_minIconSize = 32;
    int m_maxIconSize = 256;
    AssetSortBy m_sortBy = AssetSortBy::Name;
    SortDirection m_sortDirection = SortDirection::Ascending;
    bool m_showHiddenFiles = false;

    // Column view state
    std::vector<std::string> m_columnPaths;         ///< Path for each column
    std::vector<std::vector<AssetEntry>> m_columnEntries;

    // Selection
    std::unordered_set<std::string> m_selectedPaths;
    std::string m_lastSelectedPath;                 ///< For shift-click range
    int m_focusedEntryIndex = -1;

    // Search and filter
    std::string m_searchQuery;
    char m_searchBuffer[256] = "";
    bool m_recursiveSearch = false;
    bool m_isSearching = false;
    std::unordered_set<AssetType> m_typeFilters;
    std::vector<SearchResult> m_searchResults;

    // Clipboard
    std::vector<std::string> m_clipboard;
    bool m_clipboardIsCut = false;

    // Bookmarks
    std::vector<AssetBookmark> m_bookmarks;

    // Rename state
    bool m_isRenaming = false;
    AssetEntry* m_renamingEntry = nullptr;
    char m_renameBuffer[256] = "";
    bool m_renameNeedsFocus = false;

    // Context menu
    bool m_showContextMenu = false;
    glm::vec2 m_contextMenuPos{0.0f};

    // Create asset popup
    bool m_showCreateAssetPopup = false;
    AssetType m_createAssetType = AssetType::Folder;
    char m_createAssetName[256] = "";

    // Delete confirmation
    bool m_showDeleteConfirmation = false;
    std::vector<std::string> m_pendingDelete;

    // Import
    bool m_showImportDialog = false;
    ImportSettings m_importSettings;
    std::vector<std::string> m_pendingImports;

    // AI Asset Generation
    bool m_showAIGenerateDialog = false;
    AIGenerationSettings m_aiGenerationSettings;
    char m_aiPromptBuffer[2048] = "";           ///< Text input buffer for asset description
    char m_aiReferenceImageBuffer[512] = "";    ///< Buffer for reference image path

    // External integrations
    AssetThumbnailCache* m_thumbnailCache = nullptr;
    CommandHistory* m_commandHistory = nullptr;
    Scene* m_scene = nullptr;

    // Default icons
    std::unordered_map<AssetType, std::shared_ptr<Texture>> m_defaultIcons;
    std::shared_ptr<Texture> m_folderIcon;
    std::shared_ptr<Texture> m_unknownIcon;

    // Drag state
    bool m_isDragging = false;
    AssetDragPayload m_dragPayload;

    // Scroll state
    float m_scrollY = 0.0f;
    AssetEntry* m_scrollToEntry = nullptr;

    // Performance
    size_t m_visibleStartIndex = 0;
    size_t m_visibleEndIndex = 0;
    float m_lastScanTime = 0.0f;
    static constexpr float RESCAN_INTERVAL = 2.0f;  ///< Auto-rescan interval in seconds
};

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Format file size as human-readable string
 */
std::string FormatFileSize(uint64_t bytes);

/**
 * @brief Format time point as human-readable string
 */
std::string FormatFileTime(const std::chrono::system_clock::time_point& time);

/**
 * @brief Get all file extensions for an asset type
 */
std::vector<std::string> GetAssetTypeExtensions(AssetType type);

/**
 * @brief Check if a file can be imported
 */
bool IsImportableFile(const std::string& extension);

} // namespace Nova
