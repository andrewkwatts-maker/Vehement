#pragma once

#include <string>
#include <vector>
#include <functional>

namespace Vehement {

class Editor;

/**
 * @brief Asset browser panel
 *
 * Browse and preview assets:
 * - Directory tree view with breadcrumb navigation
 * - Thumbnail previews with grid/list view modes
 * - Drag-drop support (ASSET_PATH payload)
 * - Right-click context menu
 * - Search/filter by name (case-insensitive)
 * - Import assets
 */
class AssetBrowser {
public:
    explicit AssetBrowser(Editor* editor);
    ~AssetBrowser();

    void Render();

    void SetRootPath(const std::string& path);
    void Refresh();

    // Navigation
    void NavigateTo(const std::string& path);
    void NavigateUp();
    void NavigateBack();

    // Asset operations
    void OpenAsset(const std::string& path);
    void DeleteAsset(const std::string& path);
    void StartRename(const std::string& path);
    void ShowInExplorer(const std::string& path);
    void CopyPathToClipboard(const std::string& path);

    // Callbacks
    std::function<void(const std::string&)> OnAssetSelected;
    std::function<void(const std::string&)> OnAssetDoubleClicked;
    std::function<void(const std::string&)> OnAssetDeleted;
    std::function<void(const std::string&, const std::string&)> OnAssetRenamed;

private:
    void RenderToolbar();
    void RenderBreadcrumbs();
    void RenderDirectoryTree();
    void RenderFileGrid();
    void RenderContextMenu(const std::string& assetPath, const std::string& filename, bool isDirectory);
    void RenderRenamePopup();
    void RenderDeleteConfirmation();
    void RenderPreview();

    void ScanDirectory(const std::string& path);
    bool MatchesFilter(const std::string& name) const;

    Editor* m_editor = nullptr;

    std::string m_rootPath = "game/assets";
    std::string m_currentPath;
    std::string m_selectedFile;

    struct FileEntry {
        std::string name;
        std::string path;
        std::string extension;
        bool isDirectory;
        size_t size;
    };
    std::vector<FileEntry> m_currentFiles;
    std::vector<std::string> m_directoryStack;

    // View options
    bool m_showGrid = true;
    int m_thumbnailSize = 64;
    std::string m_searchFilter;
    char m_searchBuffer[256] = "";

    // Rename state
    bool m_showRenamePopup = false;
    std::string m_renamingPath;
    char m_renameBuffer[256] = "";

    // Delete confirmation state
    bool m_showDeleteConfirmation = false;
    std::string m_pendingDeletePath;

    // New folder state
    bool m_showNewFolderPopup = false;
    char m_newFolderBuffer[256] = "";
};

} // namespace Vehement
