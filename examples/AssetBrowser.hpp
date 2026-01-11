#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <ctime>
#include <filesystem>
#include <imgui.h>

/**
 * @brief Asset entry representing a file or directory in the asset browser
 */
struct AssetEntry {
    std::string name;           // File/folder name
    std::string path;           // Full path from project root
    std::string type;           // Asset type (Texture, Model, Material, etc.)
    bool isDirectory;           // True if this is a folder
    uint64_t fileSize;          // File size in bytes (0 for directories)
    std::time_t modifiedTime;   // Last modified timestamp

    AssetEntry()
        : isDirectory(false), fileSize(0), modifiedTime(0) {}
};

/**
 * @brief Thumbnail cache for asset previews
 */
class ThumbnailCache {
public:
    ThumbnailCache();
    ~ThumbnailCache();

    /**
     * @brief Get or load thumbnail for an asset
     * @param path Asset path
     * @param type Asset type
     * @return ImTextureID for rendering (nullptr if no thumbnail)
     */
    ImTextureID GetThumbnail(const std::string& path, const std::string& type);

    /**
     * @brief Clear all cached thumbnails
     */
    void Clear();

    /**
     * @brief Get placeholder icon color based on asset type
     */
    ImVec4 GetTypeColor(const std::string& type) const;

private:
    std::unordered_map<std::string, ImTextureID> m_thumbnails;

    /**
     * @brief Load thumbnail from image file
     */
    ImTextureID LoadImageThumbnail(const std::string& path);

    /**
     * @brief Generate placeholder thumbnail
     */
    ImTextureID GeneratePlaceholder(const std::string& type);
};

/**
 * @brief Asset browser with filesystem operations
 */
class AssetBrowser {
public:
    enum class ViewMode {
        Grid,
        List
    };

    AssetBrowser();
    ~AssetBrowser();

    /**
     * @brief Initialize the asset browser
     * @param rootDirectory Root assets directory (default: "assets/")
     */
    bool Initialize(const std::string& rootDirectory = "assets/");

    /**
     * @brief Scan directory and populate asset list
     * @param path Directory path to scan
     */
    void ScanDirectory(const std::string& path);

    /**
     * @brief Get filtered asset list based on search filter
     * @return Vector of filtered assets
     */
    std::vector<AssetEntry> GetFilteredAssets() const;

    /**
     * @brief Get all subdirectories of current directory
     * @return Vector of subdirectory entries
     */
    std::vector<AssetEntry> GetSubdirectories() const;

    /**
     * @brief Navigate to parent directory
     */
    void NavigateToParent();

    /**
     * @brief Navigate to specific directory
     * @param path Directory path (relative to root or absolute)
     */
    void NavigateToDirectory(const std::string& path);

    /**
     * @brief Navigate back in history
     */
    void NavigateBack();

    /**
     * @brief Navigate forward in history
     */
    void NavigateForward();

    /**
     * @brief Check if can navigate back
     */
    bool CanNavigateBack() const;

    /**
     * @brief Check if can navigate forward
     */
    bool CanNavigateForward() const;

    /**
     * @brief Create new folder
     * @param name Folder name
     * @return True if successful
     */
    bool CreateFolder(const std::string& name);

    /**
     * @brief Delete asset (file or folder)
     * @param path Asset path
     * @return True if successful
     */
    bool DeleteAsset(const std::string& path);

    /**
     * @brief Rename asset
     * @param oldPath Current path
     * @param newPath New path
     * @return True if successful
     */
    bool RenameAsset(const std::string& oldPath, const std::string& newPath);

    /**
     * @brief Refresh current directory
     */
    void Refresh();

    /**
     * @brief Get current directory path
     */
    const std::string& GetCurrentDirectory() const { return m_currentDirectory; }

    /**
     * @brief Get root directory path
     */
    const std::string& GetRootDirectory() const { return m_rootDirectory; }

    /**
     * @brief Set search filter
     */
    void SetSearchFilter(const std::string& filter) { m_searchFilter = filter; }

    /**
     * @brief Get search filter
     */
    const std::string& GetSearchFilter() const { return m_searchFilter; }

    /**
     * @brief Set view mode
     */
    void SetViewMode(ViewMode mode) { m_viewMode = mode; }

    /**
     * @brief Get view mode
     */
    ViewMode GetViewMode() const { return m_viewMode; }

    /**
     * @brief Get thumbnail cache
     */
    ThumbnailCache& GetThumbnailCache() { return m_thumbnailCache; }

    /**
     * @brief Get selected asset path (for drag-drop)
     */
    const std::string& GetSelectedAsset() const { return m_selectedAsset; }

    /**
     * @brief Set selected asset
     */
    void SetSelectedAsset(const std::string& path) { m_selectedAsset = path; }

    /**
     * @brief Build directory tree recursively
     * @param path Directory path
     * @param depth Current recursion depth
     * @param maxDepth Maximum recursion depth
     */
    void BuildDirectoryTree(const std::string& path, int depth = 0, int maxDepth = 3);

    /**
     * @brief Get directory tree (cached)
     */
    const std::vector<AssetEntry>& GetDirectoryTree() const { return m_directoryTree; }

private:
    std::string m_rootDirectory;              // Root assets directory
    std::string m_currentDirectory;           // Current browsing directory
    std::vector<AssetEntry> m_assets;         // All assets in current directory
    std::vector<AssetEntry> m_directoryTree;  // Cached directory tree
    std::vector<std::string> m_directoryHistory;  // Navigation history
    int m_historyIndex;                       // Current position in history
    std::string m_searchFilter;               // Search filter text
    ViewMode m_viewMode;                      // Grid or List view
    ThumbnailCache m_thumbnailCache;          // Thumbnail cache
    std::string m_selectedAsset;              // Currently selected asset path

    /**
     * @brief Determine asset type from file extension
     */
    std::string GetAssetType(const std::string& path) const;

    /**
     * @brief Format file size as human-readable string
     */
    std::string FormatFileSize(uint64_t size) const;

    /**
     * @brief Format time as human-readable string
     */
    std::string FormatTime(std::time_t time) const;

    /**
     * @brief Check if path matches search filter
     */
    bool MatchesFilter(const std::string& name) const;

    /**
     * @brief Sort assets (directories first, then alphabetically)
     */
    void SortAssets();
};
