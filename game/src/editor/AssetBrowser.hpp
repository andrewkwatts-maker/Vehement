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
 * - Directory tree view
 * - Thumbnail previews
 * - Drag-drop support
 * - Import assets
 */
class AssetBrowser {
public:
    explicit AssetBrowser(Editor* editor);
    ~AssetBrowser();

    void Render();

    void SetRootPath(const std::string& path);
    void Refresh();

    std::function<void(const std::string&)> OnAssetSelected;
    std::function<void(const std::string&)> OnAssetDoubleClicked;

private:
    void RenderToolbar();
    void RenderDirectoryTree();
    void RenderFileGrid();
    void RenderPreview();

    void ScanDirectory(const std::string& path);

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
};

} // namespace Vehement
