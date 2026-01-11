#include "AssetBrowser.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <cctype>

namespace fs = std::filesystem;

// ============================================================================
// ThumbnailCache Implementation
// ============================================================================

ThumbnailCache::ThumbnailCache() {
}

ThumbnailCache::~ThumbnailCache() {
    Clear();
}

ImTextureID ThumbnailCache::GetThumbnail(const std::string& path, const std::string& type) {
    // Check if already cached
    auto it = m_thumbnails.find(path);
    if (it != m_thumbnails.end()) {
        return it->second;
    }

    // Try to load image thumbnail for supported image types
    if (type == "Texture" || type == "Image") {
        ImTextureID texture = LoadImageThumbnail(path);
        if (texture) {
            m_thumbnails[path] = texture;
            return texture;
        }
    }

    // Generate placeholder for other types
    ImTextureID placeholder = GeneratePlaceholder(type);
    m_thumbnails[path] = placeholder;
    return placeholder;
}

void ThumbnailCache::Clear() {
    // Note: In a real implementation, we would properly release GPU resources here
    // For now, we just clear the map since placeholders are simple pointers
    m_thumbnails.clear();
}

ImVec4 ThumbnailCache::GetTypeColor(const std::string& type) const {
    // Return color based on asset type for placeholder rendering
    static const std::unordered_map<std::string, ImVec4> typeColors = {
        {"Texture",   ImVec4(0.8f, 0.3f, 0.8f, 1.0f)}, // Purple
        {"Image",     ImVec4(0.8f, 0.3f, 0.8f, 1.0f)}, // Purple
        {"Model",     ImVec4(0.3f, 0.8f, 0.3f, 1.0f)}, // Green
        {"Material",  ImVec4(0.8f, 0.5f, 0.2f, 1.0f)}, // Orange
        {"Shader",    ImVec4(0.3f, 0.5f, 0.8f, 1.0f)}, // Blue
        {"Script",    ImVec4(0.9f, 0.9f, 0.3f, 1.0f)}, // Yellow
        {"Audio",     ImVec4(0.3f, 0.8f, 0.8f, 1.0f)}, // Cyan
        {"Scene",     ImVec4(0.8f, 0.3f, 0.3f, 1.0f)}, // Red
        {"Prefab",    ImVec4(0.5f, 0.3f, 0.8f, 1.0f)}, // Violet
        {"Font",      ImVec4(0.6f, 0.6f, 0.6f, 1.0f)}, // Gray
        {"Directory", ImVec4(0.9f, 0.8f, 0.4f, 1.0f)}, // Light yellow
        {"Unknown",   ImVec4(0.5f, 0.5f, 0.5f, 1.0f)}  // Dark gray
    };

    auto it = typeColors.find(type);
    if (it != typeColors.end()) {
        return it->second;
    }
    return typeColors.at("Unknown");
}

ImTextureID ThumbnailCache::LoadImageThumbnail(const std::string& path) {
    // TODO: Implement actual image loading using stb_image or similar
    // For now, return nullptr to fall back to placeholder
    //
    // Example implementation:
    // 1. Load image using stb_image_load()
    // 2. Create OpenGL texture
    // 3. Upload image data to GPU
    // 4. Store texture ID
    // 5. Free CPU image data

    return (ImTextureID)0;
}

ImTextureID ThumbnailCache::GeneratePlaceholder(const std::string& type) {
    // For now, we return a simple pointer cast of a hash value
    // In a real implementation, we would create a small colored texture on the GPU
    // The UI code can use GetTypeColor() to render colored squares instead

    std::hash<std::string> hasher;
    size_t hash = hasher(type);
    return (ImTextureID)(uintptr_t)hash;
}

// ============================================================================
// AssetBrowser Implementation
// ============================================================================

AssetBrowser::AssetBrowser()
    : m_historyIndex(-1)
    , m_viewMode(ViewMode::Grid) {
}

AssetBrowser::~AssetBrowser() {
}

bool AssetBrowser::Initialize(const std::string& rootDirectory) {
    try {
        m_rootDirectory = rootDirectory;

        // Normalize path separators
        std::replace(m_rootDirectory.begin(), m_rootDirectory.end(), '\\', '/');

        // Ensure root directory exists
        if (!fs::exists(m_rootDirectory)) {
            spdlog::warn("Asset root directory does not exist, creating: {}", m_rootDirectory);
            if (!fs::create_directories(m_rootDirectory)) {
                spdlog::error("Failed to create asset root directory: {}", m_rootDirectory);
                return false;
            }
        }

        if (!fs::is_directory(m_rootDirectory)) {
            spdlog::error("Asset root path is not a directory: {}", m_rootDirectory);
            return false;
        }

        // Set current directory to root
        m_currentDirectory = m_rootDirectory;

        // Clear history
        m_directoryHistory.clear();
        m_historyIndex = -1;

        // Perform initial scan
        ScanDirectory(m_currentDirectory);

        // Build directory tree
        BuildDirectoryTree(m_rootDirectory);

        spdlog::info("AssetBrowser initialized with root: {}", m_rootDirectory);
        return true;

    } catch (const fs::filesystem_error& e) {
        spdlog::error("Filesystem error during AssetBrowser initialization: {}", e.what());
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Error during AssetBrowser initialization: {}", e.what());
        return false;
    }
}

void AssetBrowser::ScanDirectory(const std::string& path) {
    try {
        m_assets.clear();

        if (!fs::exists(path)) {
            spdlog::warn("Directory does not exist: {}", path);
            return;
        }

        if (!fs::is_directory(path)) {
            spdlog::warn("Path is not a directory: {}", path);
            return;
        }

        // Iterate through directory entries
        for (const auto& entry : fs::directory_iterator(path)) {
            try {
                AssetEntry asset;
                asset.path = entry.path().string();
                asset.name = entry.path().filename().string();
                asset.isDirectory = entry.is_directory();

                // Normalize path separators
                std::replace(asset.path.begin(), asset.path.end(), '\\', '/');

                if (asset.isDirectory) {
                    asset.type = "Directory";
                    asset.fileSize = 0;
                } else {
                    asset.type = GetAssetType(asset.path);
                    asset.fileSize = entry.file_size();
                }

                // Get last modified time
                auto ftime = fs::last_write_time(entry.path());
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
                );
                asset.modifiedTime = std::chrono::system_clock::to_time_t(sctp);

                m_assets.push_back(asset);

            } catch (const fs::filesystem_error& e) {
                spdlog::warn("Error accessing file entry: {}", e.what());
                continue;
            }
        }

        // Sort assets: directories first, then alphabetically
        SortAssets();

        spdlog::info("Scanned directory: {} ({} items)", path, m_assets.size());

    } catch (const fs::filesystem_error& e) {
        spdlog::error("Filesystem error scanning directory {}: {}", path, e.what());
    } catch (const std::exception& e) {
        spdlog::error("Error scanning directory {}: {}", path, e.what());
    }
}

std::vector<AssetEntry> AssetBrowser::GetFilteredAssets() const {
    if (m_searchFilter.empty()) {
        return m_assets;
    }

    std::vector<AssetEntry> filtered;
    filtered.reserve(m_assets.size());

    for (const auto& asset : m_assets) {
        if (MatchesFilter(asset.name)) {
            filtered.push_back(asset);
        }
    }

    return filtered;
}

std::vector<AssetEntry> AssetBrowser::GetSubdirectories() const {
    std::vector<AssetEntry> subdirs;
    subdirs.reserve(m_assets.size());

    for (const auto& asset : m_assets) {
        if (asset.isDirectory) {
            subdirs.push_back(asset);
        }
    }

    return subdirs;
}

void AssetBrowser::NavigateToParent() {
    try {
        fs::path currentPath(m_currentDirectory);
        fs::path rootPath(m_rootDirectory);

        // Check if we're already at root
        if (currentPath == rootPath || currentPath.parent_path() == currentPath) {
            spdlog::debug("Already at root directory");
            return;
        }

        // Get parent path
        fs::path parentPath = currentPath.parent_path();

        // Don't navigate above root
        if (parentPath.string().length() < rootPath.string().length()) {
            spdlog::debug("Cannot navigate above root directory");
            return;
        }

        NavigateToDirectory(parentPath.string());

    } catch (const fs::filesystem_error& e) {
        spdlog::error("Error navigating to parent: {}", e.what());
    } catch (const std::exception& e) {
        spdlog::error("Error navigating to parent: {}", e.what());
    }
}

void AssetBrowser::NavigateToDirectory(const std::string& path) {
    try {
        if (!fs::exists(path)) {
            spdlog::warn("Cannot navigate to non-existent directory: {}", path);
            return;
        }

        if (!fs::is_directory(path)) {
            spdlog::warn("Cannot navigate to non-directory path: {}", path);
            return;
        }

        std::string normalizedPath = path;
        std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');

        // Don't navigate if we're already there
        if (normalizedPath == m_currentDirectory) {
            return;
        }

        // Update history
        // Remove any forward history if we're in the middle of the stack
        if (m_historyIndex >= 0 && m_historyIndex < static_cast<int>(m_directoryHistory.size()) - 1) {
            m_directoryHistory.erase(
                m_directoryHistory.begin() + m_historyIndex + 1,
                m_directoryHistory.end()
            );
        }

        // Add current directory to history (if we have one)
        if (!m_currentDirectory.empty() && m_historyIndex < 0) {
            m_directoryHistory.push_back(m_currentDirectory);
            m_historyIndex = 0;
        } else if (!m_currentDirectory.empty()) {
            m_directoryHistory.push_back(m_currentDirectory);
            m_historyIndex++;
        }

        // Navigate to new directory
        m_currentDirectory = normalizedPath;
        ScanDirectory(m_currentDirectory);

        spdlog::debug("Navigated to: {}", m_currentDirectory);

    } catch (const fs::filesystem_error& e) {
        spdlog::error("Filesystem error navigating to directory {}: {}", path, e.what());
    } catch (const std::exception& e) {
        spdlog::error("Error navigating to directory {}: {}", path, e.what());
    }
}

void AssetBrowser::NavigateBack() {
    if (!CanNavigateBack()) {
        return;
    }

    try {
        std::string previousDir = m_directoryHistory[m_historyIndex];
        m_historyIndex--;

        m_currentDirectory = previousDir;
        ScanDirectory(m_currentDirectory);

        spdlog::debug("Navigated back to: {}", m_currentDirectory);

    } catch (const std::exception& e) {
        spdlog::error("Error navigating back: {}", e.what());
    }
}

void AssetBrowser::NavigateForward() {
    if (!CanNavigateForward()) {
        return;
    }

    try {
        m_historyIndex++;
        std::string nextDir = m_directoryHistory[m_historyIndex];

        m_currentDirectory = nextDir;
        ScanDirectory(m_currentDirectory);

        spdlog::debug("Navigated forward to: {}", m_currentDirectory);

    } catch (const std::exception& e) {
        spdlog::error("Error navigating forward: {}", e.what());
        m_historyIndex--; // Revert on error
    }
}

bool AssetBrowser::CanNavigateBack() const {
    return m_historyIndex > 0;
}

bool AssetBrowser::CanNavigateForward() const {
    return m_historyIndex >= 0 &&
           m_historyIndex < static_cast<int>(m_directoryHistory.size()) - 1;
}

bool AssetBrowser::CreateFolder(const std::string& name) {
    try {
        // Validate folder name
        if (name.empty()) {
            spdlog::warn("Cannot create folder with empty name");
            return false;
        }

        // Check for invalid characters
        const std::string invalidChars = "<>:\"/\\|?*";
        if (name.find_first_of(invalidChars) != std::string::npos) {
            spdlog::warn("Folder name contains invalid characters: {}", name);
            return false;
        }

        // Build full path
        fs::path folderPath = fs::path(m_currentDirectory) / name;

        // Check if already exists
        if (fs::exists(folderPath)) {
            spdlog::warn("Folder already exists: {}", folderPath.string());
            return false;
        }

        // Create folder
        if (!fs::create_directory(folderPath)) {
            spdlog::error("Failed to create folder: {}", folderPath.string());
            return false;
        }

        spdlog::info("Created folder: {}", folderPath.string());

        // Refresh directory listing
        Refresh();

        return true;

    } catch (const fs::filesystem_error& e) {
        spdlog::error("Filesystem error creating folder: {}", e.what());
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Error creating folder: {}", e.what());
        return false;
    }
}

bool AssetBrowser::DeleteAsset(const std::string& path) {
    try {
        if (!fs::exists(path)) {
            spdlog::warn("Cannot delete non-existent path: {}", path);
            return false;
        }

        // Safety check: don't allow deleting root directory
        if (path == m_rootDirectory) {
            spdlog::error("Cannot delete root directory");
            return false;
        }

        // Safety check: ensure path is within root directory
        fs::path assetPath(path);
        fs::path rootPath(m_rootDirectory);

        auto relativePath = fs::relative(assetPath, rootPath);
        if (relativePath.string().find("..") == 0) {
            spdlog::error("Cannot delete asset outside root directory: {}", path);
            return false;
        }

        // Delete file or directory
        if (fs::is_directory(path)) {
            // Remove directory and all contents
            std::uintmax_t removedCount = fs::remove_all(path);
            spdlog::info("Deleted directory and {} items: {}", removedCount, path);
        } else {
            // Remove single file
            fs::remove(path);
            spdlog::info("Deleted file: {}", path);
        }

        // Refresh directory listing
        Refresh();

        return true;

    } catch (const fs::filesystem_error& e) {
        spdlog::error("Filesystem error deleting asset {}: {}", path, e.what());
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Error deleting asset {}: {}", path, e.what());
        return false;
    }
}

bool AssetBrowser::RenameAsset(const std::string& oldPath, const std::string& newPath) {
    try {
        if (!fs::exists(oldPath)) {
            spdlog::warn("Cannot rename non-existent path: {}", oldPath);
            return false;
        }

        if (fs::exists(newPath)) {
            spdlog::warn("Cannot rename to existing path: {}", newPath);
            return false;
        }

        // Safety check: ensure both paths are within root directory
        fs::path oldAssetPath(oldPath);
        fs::path newAssetPath(newPath);
        fs::path rootPath(m_rootDirectory);

        auto oldRelativePath = fs::relative(oldAssetPath, rootPath);
        auto newRelativePath = fs::relative(newAssetPath, rootPath);

        if (oldRelativePath.string().find("..") == 0 ||
            newRelativePath.string().find("..") == 0) {
            spdlog::error("Cannot rename assets outside root directory");
            return false;
        }

        // Perform rename
        fs::rename(oldPath, newPath);

        spdlog::info("Renamed: {} -> {}", oldPath, newPath);

        // Refresh directory listing
        Refresh();

        return true;

    } catch (const fs::filesystem_error& e) {
        spdlog::error("Filesystem error renaming asset: {}", e.what());
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Error renaming asset: {}", e.what());
        return false;
    }
}

void AssetBrowser::Refresh() {
    ScanDirectory(m_currentDirectory);
}

void AssetBrowser::BuildDirectoryTree(const std::string& path, int depth, int maxDepth) {
    try {
        if (depth >= maxDepth) {
            return;
        }

        if (depth == 0) {
            m_directoryTree.clear();
        }

        if (!fs::exists(path) || !fs::is_directory(path)) {
            return;
        }

        // Iterate through directory entries
        for (const auto& entry : fs::directory_iterator(path)) {
            try {
                if (entry.is_directory()) {
                    AssetEntry dirEntry;
                    dirEntry.path = entry.path().string();
                    dirEntry.name = entry.path().filename().string();
                    dirEntry.isDirectory = true;
                    dirEntry.type = "Directory";
                    dirEntry.fileSize = 0;

                    // Normalize path separators
                    std::replace(dirEntry.path.begin(), dirEntry.path.end(), '\\', '/');

                    // Get last modified time
                    auto ftime = fs::last_write_time(entry.path());
                    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
                    );
                    dirEntry.modifiedTime = std::chrono::system_clock::to_time_t(sctp);

                    m_directoryTree.push_back(dirEntry);

                    // Recursively build subdirectories
                    BuildDirectoryTree(dirEntry.path, depth + 1, maxDepth);
                }
            } catch (const fs::filesystem_error& e) {
                spdlog::warn("Error accessing directory entry: {}", e.what());
                continue;
            }
        }

        if (depth == 0) {
            spdlog::debug("Built directory tree with {} directories", m_directoryTree.size());
        }

    } catch (const fs::filesystem_error& e) {
        spdlog::error("Filesystem error building directory tree: {}", e.what());
    } catch (const std::exception& e) {
        spdlog::error("Error building directory tree: {}", e.what());
    }
}

// ============================================================================
// Private Helper Methods
// ============================================================================

std::string AssetBrowser::GetAssetType(const std::string& path) const {
    fs::path filePath(path);
    std::string extension = filePath.extension().string();

    // Convert to lowercase for comparison
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // Map extensions to asset types
    static const std::unordered_map<std::string, std::string> extensionMap = {
        // Images/Textures
        {".png", "Texture"},
        {".jpg", "Texture"},
        {".jpeg", "Texture"},
        {".bmp", "Texture"},
        {".tga", "Texture"},
        {".dds", "Texture"},
        {".hdr", "Texture"},
        {".exr", "Texture"},

        // Models
        {".obj", "Model"},
        {".fbx", "Model"},
        {".gltf", "Model"},
        {".glb", "Model"},
        {".dae", "Model"},
        {".blend", "Model"},
        {".3ds", "Model"},

        // Materials
        {".mat", "Material"},
        {".mtl", "Material"},

        // Shaders
        {".glsl", "Shader"},
        {".hlsl", "Shader"},
        {".vert", "Shader"},
        {".frag", "Shader"},
        {".geom", "Shader"},
        {".comp", "Shader"},
        {".tesc", "Shader"},
        {".tese", "Shader"},
        {".shader", "Shader"},

        // Scripts
        {".cpp", "Script"},
        {".h", "Script"},
        {".hpp", "Script"},
        {".c", "Script"},
        {".cc", "Script"},
        {".cxx", "Script"},
        {".lua", "Script"},
        {".py", "Script"},
        {".js", "Script"},
        {".ts", "Script"},

        // Audio
        {".wav", "Audio"},
        {".mp3", "Audio"},
        {".ogg", "Audio"},
        {".flac", "Audio"},
        {".aiff", "Audio"},

        // Scenes/Levels
        {".scene", "Scene"},
        {".level", "Scene"},
        {".map", "Scene"},

        // Prefabs
        {".prefab", "Prefab"},

        // Fonts
        {".ttf", "Font"},
        {".otf", "Font"},

        // Data
        {".json", "Data"},
        {".xml", "Data"},
        {".yaml", "Data"},
        {".yml", "Data"},
        {".ini", "Data"},
        {".cfg", "Data"},
        {".txt", "Data"}
    };

    auto it = extensionMap.find(extension);
    if (it != extensionMap.end()) {
        return it->second;
    }

    return "Unknown";
}

std::string AssetBrowser::FormatFileSize(uint64_t size) const {
    const uint64_t KB = 1024;
    const uint64_t MB = KB * 1024;
    const uint64_t GB = MB * 1024;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);

    if (size >= GB) {
        oss << (static_cast<double>(size) / GB) << " GB";
    } else if (size >= MB) {
        oss << (static_cast<double>(size) / MB) << " MB";
    } else if (size >= KB) {
        oss << (static_cast<double>(size) / KB) << " KB";
    } else {
        oss << size << " B";
    }

    return oss.str();
}

std::string AssetBrowser::FormatTime(std::time_t time) const {
    char buffer[100];
    std::tm* timeinfo = std::localtime(&time);

    if (timeinfo) {
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
        return std::string(buffer);
    }

    return "Unknown";
}

bool AssetBrowser::MatchesFilter(const std::string& name) const {
    if (m_searchFilter.empty()) {
        return true;
    }

    // Convert both to lowercase for case-insensitive comparison
    std::string lowerName = name;
    std::string lowerFilter = m_searchFilter;

    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    return lowerName.find(lowerFilter) != std::string::npos;
}

void AssetBrowser::SortAssets() {
    std::sort(m_assets.begin(), m_assets.end(), [](const AssetEntry& a, const AssetEntry& b) {
        // Directories come first
        if (a.isDirectory != b.isDirectory) {
            return a.isDirectory;
        }

        // Then sort alphabetically (case-insensitive)
        std::string aLower = a.name;
        std::string bLower = b.name;
        std::transform(aLower.begin(), aLower.end(), aLower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        std::transform(bLower.begin(), bLower.end(), bLower.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        return aLower < bLower;
    });
}
