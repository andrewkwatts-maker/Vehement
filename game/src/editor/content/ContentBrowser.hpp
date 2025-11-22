#pragma once

#include "ContentDatabase.hpp"
#include "ContentFilter.hpp"
#include "ThumbnailGenerator.hpp"
#include "AssetImporter.hpp"
#include "ContentActions.hpp"

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <optional>

namespace Vehement {

class Editor;

namespace WebEditor {
    class WebView;
    class JSBridge;
}

namespace Content {

/**
 * @brief View mode for content display
 */
enum class ViewMode {
    Grid,
    List,
    Details,
    Tiles
};

/**
 * @brief Folder tree node
 */
struct FolderNode {
    std::string path;
    std::string name;
    std::vector<FolderNode> children;
    int assetCount = 0;
    bool expanded = false;
    bool selected = false;
};

/**
 * @brief Recent file entry
 */
struct RecentEntry {
    std::string assetId;
    std::string name;
    AssetType type;
    std::chrono::system_clock::time_point accessTime;
};

/**
 * @brief Bookmark/favorite entry
 */
struct Bookmark {
    std::string id;
    std::string name;
    std::string assetId;       // For asset bookmarks
    std::string folderPath;    // For folder bookmarks
    std::string icon;
    int order = 0;
};

/**
 * @brief Selection info
 */
struct SelectionInfo {
    std::vector<std::string> assetIds;
    std::string primaryId;     // Last selected
    bool hasMultiple = false;

    [[nodiscard]] bool IsSelected(const std::string& id) const;
    [[nodiscard]] size_t Count() const { return assetIds.size(); }
    [[nodiscard]] bool IsEmpty() const { return assetIds.empty(); }
    void Clear() { assetIds.clear(); primaryId.clear(); hasMultiple = false; }
};

/**
 * @brief Drag-drop info
 */
struct DragDropInfo {
    std::vector<std::string> assetIds;
    std::string sourceFolder;
    bool isDragging = false;
    bool isExternal = false;   // From outside the app
    std::vector<std::string> externalPaths;
};

/**
 * @brief Context menu action
 */
struct ContextMenuAction {
    std::string id;
    std::string label;
    std::string icon;
    std::string shortcut;
    bool enabled = true;
    bool separator = false;
    std::vector<ContextMenuAction> submenu;
    std::function<void()> action;
};

/**
 * @brief Content Browser Panel
 *
 * Main content browser providing comprehensive asset management:
 * - Folder tree view (left panel)
 * - Asset grid/list view (right panel)
 * - Thumbnail generation and caching
 * - Drag and drop support
 * - Context menu (create, delete, duplicate, rename)
 * - Multi-select operations
 * - Search with filters
 * - Sort by name/date/type/size
 * - Favorites/bookmarks
 * - Recent files
 *
 * Integrates with HTML viewers for rich editing experience.
 */
class ContentBrowser {
public:
    explicit ContentBrowser(Editor* editor);
    ~ContentBrowser();

    // Non-copyable
    ContentBrowser(const ContentBrowser&) = delete;
    ContentBrowser& operator=(const ContentBrowser&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Initialize the content browser
     * @param configsPath Path to the configs directory
     * @return true if successful
     */
    bool Initialize(const std::string& configsPath);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Update state
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    /**
     * @brief Render the panel
     */
    void Render();

    // =========================================================================
    // Subsystem Access
    // =========================================================================

    [[nodiscard]] ContentDatabase* GetDatabase() { return m_database.get(); }
    [[nodiscard]] ContentFilter* GetFilter() { return m_filter.get(); }
    [[nodiscard]] ThumbnailGenerator* GetThumbnailGenerator() { return m_thumbnailGenerator.get(); }
    [[nodiscard]] AssetImporter* GetImporter() { return m_importer.get(); }
    [[nodiscard]] ContentActions* GetActions() { return m_actions.get(); }

    // =========================================================================
    // Navigation
    // =========================================================================

    /**
     * @brief Navigate to folder
     */
    void NavigateToFolder(const std::string& path);

    /**
     * @brief Navigate to asset (select and scroll to)
     */
    void NavigateToAsset(const std::string& assetId);

    /**
     * @brief Go to parent folder
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
     * @brief Get current folder path
     */
    [[nodiscard]] const std::string& GetCurrentFolder() const { return m_currentFolder; }

    /**
     * @brief Get navigation history
     */
    [[nodiscard]] const std::vector<std::string>& GetNavigationHistory() const { return m_navigationHistory; }

    // =========================================================================
    // Selection
    // =========================================================================

    /**
     * @brief Select asset
     * @param assetId Asset to select
     * @param addToSelection Add to current selection (Ctrl+Click)
     * @param rangeSelect Range select (Shift+Click)
     */
    void Select(const std::string& assetId, bool addToSelection = false, bool rangeSelect = false);

    /**
     * @brief Select multiple assets
     */
    void SelectMultiple(const std::vector<std::string>& assetIds);

    /**
     * @brief Select all in current view
     */
    void SelectAll();

    /**
     * @brief Clear selection
     */
    void ClearSelection();

    /**
     * @brief Invert selection
     */
    void InvertSelection();

    /**
     * @brief Get selection info
     */
    [[nodiscard]] const SelectionInfo& GetSelection() const { return m_selection; }

    /**
     * @brief Check if asset is selected
     */
    [[nodiscard]] bool IsSelected(const std::string& assetId) const;

    // =========================================================================
    // View Options
    // =========================================================================

    /**
     * @brief Set view mode
     */
    void SetViewMode(ViewMode mode);

    /**
     * @brief Get current view mode
     */
    [[nodiscard]] ViewMode GetViewMode() const { return m_viewMode; }

    /**
     * @brief Set thumbnail size
     */
    void SetThumbnailSize(int size);

    /**
     * @brief Get thumbnail size
     */
    [[nodiscard]] int GetThumbnailSize() const { return m_thumbnailSize; }

    /**
     * @brief Toggle preview panel
     */
    void SetPreviewPanelVisible(bool visible);

    /**
     * @brief Check if preview panel is visible
     */
    [[nodiscard]] bool IsPreviewPanelVisible() const { return m_showPreviewPanel; }

    /**
     * @brief Set folder tree visible
     */
    void SetFolderTreeVisible(bool visible);

    /**
     * @brief Check if folder tree is visible
     */
    [[nodiscard]] bool IsFolderTreeVisible() const { return m_showFolderTree; }

    // =========================================================================
    // Search & Filter
    // =========================================================================

    /**
     * @brief Set search query
     */
    void SetSearchQuery(const std::string& query);

    /**
     * @brief Get current search query
     */
    [[nodiscard]] const std::string& GetSearchQuery() const;

    /**
     * @brief Quick filter by type
     */
    void FilterByType(AssetType type);

    /**
     * @brief Clear all filters
     */
    void ClearFilters();

    /**
     * @brief Apply saved filter preset
     */
    void ApplyFilterPreset(const std::string& presetName);

    // =========================================================================
    // Sorting
    // =========================================================================

    /**
     * @brief Set sort field
     */
    void SetSortField(SortField field);

    /**
     * @brief Set sort direction
     */
    void SetSortDirection(SortDirection direction);

    /**
     * @brief Toggle sort direction
     */
    void ToggleSortDirection();

    /**
     * @brief Get current sort field
     */
    [[nodiscard]] SortField GetSortField() const;

    /**
     * @brief Get current sort direction
     */
    [[nodiscard]] SortDirection GetSortDirection() const;

    // =========================================================================
    // Bookmarks & Favorites
    // =========================================================================

    /**
     * @brief Add bookmark
     */
    void AddBookmark(const std::string& assetId);

    /**
     * @brief Remove bookmark
     */
    void RemoveBookmark(const std::string& assetId);

    /**
     * @brief Toggle bookmark
     */
    void ToggleBookmark(const std::string& assetId);

    /**
     * @brief Get all bookmarks
     */
    [[nodiscard]] std::vector<Bookmark> GetBookmarks() const;

    /**
     * @brief Add folder bookmark
     */
    void AddFolderBookmark(const std::string& folderPath, const std::string& name = "");

    /**
     * @brief Remove folder bookmark
     */
    void RemoveFolderBookmark(const std::string& folderPath);

    // =========================================================================
    // Recent Files
    // =========================================================================

    /**
     * @brief Get recent files
     */
    [[nodiscard]] std::vector<RecentEntry> GetRecentFiles(size_t count = 20) const;

    /**
     * @brief Clear recent files
     */
    void ClearRecentFiles();

    /**
     * @brief Add to recent files
     */
    void AddToRecentFiles(const std::string& assetId);

    // =========================================================================
    // Drag & Drop
    // =========================================================================

    /**
     * @brief Begin drag operation
     */
    void BeginDrag(const std::vector<std::string>& assetIds);

    /**
     * @brief Handle drop on folder
     */
    void HandleDrop(const std::string& targetFolder);

    /**
     * @brief Handle external file drop
     */
    void HandleExternalDrop(const std::vector<std::string>& paths, const std::string& targetFolder);

    /**
     * @brief Get drag-drop info
     */
    [[nodiscard]] const DragDropInfo& GetDragDropInfo() const { return m_dragDrop; }

    /**
     * @brief Check if drop is valid at target
     */
    [[nodiscard]] bool IsValidDropTarget(const std::string& targetFolder) const;

    // =========================================================================
    // Context Menu
    // =========================================================================

    /**
     * @brief Show context menu for asset
     */
    void ShowAssetContextMenu(const std::string& assetId, int x, int y);

    /**
     * @brief Show context menu for folder
     */
    void ShowFolderContextMenu(const std::string& folderPath, int x, int y);

    /**
     * @brief Show context menu for empty area
     */
    void ShowBackgroundContextMenu(int x, int y);

    /**
     * @brief Get context menu actions for current selection
     */
    [[nodiscard]] std::vector<ContextMenuAction> GetContextMenuActions() const;

    // =========================================================================
    // Actions (shortcuts to ContentActions)
    // =========================================================================

    /**
     * @brief Create new asset
     */
    void CreateAsset(AssetType type, const std::string& name = "");

    /**
     * @brief Delete selected assets
     */
    void DeleteSelected();

    /**
     * @brief Duplicate selected assets
     */
    void DuplicateSelected();

    /**
     * @brief Rename selected asset
     */
    void RenameSelected(const std::string& newName);

    /**
     * @brief Open selected asset in editor
     */
    void OpenSelected();

    /**
     * @brief Show selected asset in explorer/finder
     */
    void ShowInExplorer(const std::string& assetId);

    /**
     * @brief Copy selected to clipboard
     */
    void CopySelected();

    /**
     * @brief Cut selected to clipboard
     */
    void CutSelected();

    /**
     * @brief Paste from clipboard
     */
    void Paste();

    // =========================================================================
    // Refresh
    // =========================================================================

    /**
     * @brief Refresh content view
     */
    void Refresh();

    /**
     * @brief Force full rescan
     */
    void Rescan();

    // =========================================================================
    // HTML Integration
    // =========================================================================

    /**
     * @brief Get WebView for HTML rendering
     */
    [[nodiscard]] WebEditor::WebView* GetWebView() { return m_webView.get(); }

    /**
     * @brief Send data to HTML view
     */
    void SendToHtml(const std::string& eventType, const std::string& jsonData);

    /**
     * @brief Open asset in HTML editor
     */
    void OpenInHtmlEditor(const std::string& assetId);

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(const std::string&)> OnAssetSelected;
    std::function<void(const std::string&)> OnAssetDoubleClicked;
    std::function<void(const std::string&)> OnAssetOpened;
    std::function<void(const std::string&)> OnFolderChanged;
    std::function<void(const SelectionInfo&)> OnSelectionChanged;

private:
    // UI Rendering (ImGui)
    void RenderToolbar();
    void RenderFolderTree();
    void RenderFolderNode(FolderNode& node);
    void RenderContentArea();
    void RenderGridView();
    void RenderListView();
    void RenderDetailsView();
    void RenderPreviewPanel();
    void RenderStatusBar();
    void RenderContextMenu();
    void RenderCreateDialog();
    void RenderRenameDialog();

    // Folder tree management
    void BuildFolderTree();
    void UpdateFolderCounts();
    FolderNode CreateFolderNode(const std::string& path);

    // Content fetching
    std::vector<AssetMetadata> GetVisibleAssets();
    void UpdateVisibleAssets();

    // Selection helpers
    void UpdateSelectionRange(const std::string& fromId, const std::string& toId);
    std::string GetLastVisibleAsset() const;
    std::string GetFirstVisibleAsset() const;

    // Keyboard navigation
    void HandleKeyboardInput();
    void SelectNext();
    void SelectPrevious();
    void SelectNextRow();
    void SelectPreviousRow();

    // JS Bridge setup
    void SetupJSBridge();
    void RegisterBridgeFunctions();

    // Event handling
    void HandleAssetCreated(const std::string& assetId);
    void HandleAssetDeleted(const std::string& assetId);
    void HandleAssetModified(const std::string& assetId);

    Editor* m_editor = nullptr;

    // Subsystems
    std::unique_ptr<ContentDatabase> m_database;
    std::unique_ptr<ContentFilter> m_filter;
    std::unique_ptr<ThumbnailGenerator> m_thumbnailGenerator;
    std::unique_ptr<AssetImporter> m_importer;
    std::unique_ptr<ContentActions> m_actions;

    // Web view for HTML integration
    std::unique_ptr<WebEditor::WebView> m_webView;
    std::unique_ptr<WebEditor::JSBridge> m_bridge;

    // Paths
    std::string m_configsPath;
    std::string m_currentFolder;

    // Navigation
    std::vector<std::string> m_navigationHistory;
    int m_navigationIndex = -1;

    // Folder tree
    FolderNode m_rootFolder;
    std::string m_selectedFolder;

    // Content cache
    std::vector<AssetMetadata> m_visibleAssets;
    bool m_needsRefresh = true;

    // Selection
    SelectionInfo m_selection;
    std::string m_lastSelectedId;  // For range selection

    // Drag-drop
    DragDropInfo m_dragDrop;

    // View options
    ViewMode m_viewMode = ViewMode::Grid;
    int m_thumbnailSize = 128;
    bool m_showPreviewPanel = true;
    bool m_showFolderTree = true;
    float m_folderTreeWidth = 200.0f;
    float m_previewPanelWidth = 300.0f;

    // Bookmarks & Recent
    std::vector<Bookmark> m_bookmarks;
    std::vector<RecentEntry> m_recentFiles;
    static constexpr size_t MAX_RECENT_FILES = 50;

    // UI State
    bool m_showCreateDialog = false;
    bool m_showRenameDialog = false;
    AssetType m_createDialogType = AssetType::Unknown;
    std::string m_createDialogName;
    std::string m_renameDialogName;
    std::string m_contextMenuAssetId;
    bool m_contextMenuOpen = false;
    int m_contextMenuX = 0;
    int m_contextMenuY = 0;

    // Search
    std::string m_searchQuery;
    bool m_searchFocused = false;
};

} // namespace Content
} // namespace Vehement
