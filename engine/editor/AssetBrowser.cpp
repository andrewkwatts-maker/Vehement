/**
 * @file AssetBrowser.cpp
 * @brief Implementation of the asset browser panel
 */

#include "AssetBrowser.hpp"
#include "../ui/EditorTheme.hpp"
#include <imgui.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstring>

#ifdef _WIN32
#include <shellapi.h>
#include <windows.h>
#endif

namespace Nova {

// =============================================================================
// Asset Type Utilities
// =============================================================================

const char* GetAssetTypeIcon(AssetType type) {
    switch (type) {
        case AssetType::Folder:     return "\xef\x81\xbc";  // folder icon
        case AssetType::SDFModel:   return "\xef\x86\xb8";  // cube icon
        case AssetType::Mesh:       return "\xef\x86\xb8";  // cube icon
        case AssetType::Texture:    return "\xef\x87\x85";  // image icon
        case AssetType::Material:   return "\xef\x83\xab";  // paint brush
        case AssetType::Animation:  return "\xef\x80\x88";  // film icon
        case AssetType::Audio:      return "\xef\x80\xa8";  // volume icon
        case AssetType::Script:     return "\xef\x84\xa1";  // code icon
        case AssetType::Prefab:     return "\xef\x86\x8e";  // box icon
        case AssetType::Scene:      return "\xef\x82\xac";  // globe icon
        case AssetType::Shader:     return "\xef\x83\xa8";  // magic wand
        default:                    return "\xef\x85\x9b";  // file icon
    }
}

const char* GetAssetTypeName(AssetType type) {
    switch (type) {
        case AssetType::Folder:     return "Folder";
        case AssetType::SDFModel:   return "SDF Model";
        case AssetType::Mesh:       return "Mesh";
        case AssetType::Texture:    return "Texture";
        case AssetType::Material:   return "Material";
        case AssetType::Animation:  return "Animation";
        case AssetType::Audio:      return "Audio";
        case AssetType::Script:     return "Script";
        case AssetType::Prefab:     return "Prefab";
        case AssetType::Scene:      return "Scene";
        case AssetType::Shader:     return "Shader";
        default:                    return "Unknown";
    }
}

const char* GetAssetTypeFilter(AssetType type) {
    switch (type) {
        case AssetType::SDFModel:   return "*.sdf;*.nova";
        case AssetType::Mesh:       return "*.obj;*.fbx;*.gltf;*.glb";
        case AssetType::Texture:    return "*.png;*.jpg;*.jpeg;*.tga;*.hdr;*.bmp";
        case AssetType::Material:   return "*.mat;*.material";
        case AssetType::Animation:  return "*.anim";
        case AssetType::Audio:      return "*.wav;*.ogg;*.mp3;*.flac";
        case AssetType::Script:     return "*.lua;*.py";
        case AssetType::Prefab:     return "*.prefab";
        case AssetType::Scene:      return "*.scene";
        case AssetType::Shader:     return "*.glsl;*.hlsl;*.vert;*.frag;*.comp";
        default:                    return "*.*";
    }
}

AssetType DetectAssetType(const std::string& extension) {
    std::string ext = extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    // Ensure extension starts with dot
    if (!ext.empty() && ext[0] != '.') {
        ext = "." + ext;
    }

    // SDF Models
    if (ext == ".sdf" || ext == ".nova") {
        return AssetType::SDFModel;
    }

    // Meshes
    if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb") {
        return AssetType::Mesh;
    }

    // Textures
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" ||
        ext == ".hdr" || ext == ".bmp" || ext == ".dds") {
        return AssetType::Texture;
    }

    // Materials
    if (ext == ".mat" || ext == ".material") {
        return AssetType::Material;
    }

    // Animation
    if (ext == ".anim") {
        return AssetType::Animation;
    }

    // Audio
    if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac") {
        return AssetType::Audio;
    }

    // Scripts
    if (ext == ".lua" || ext == ".py") {
        return AssetType::Script;
    }

    // Prefabs
    if (ext == ".prefab") {
        return AssetType::Prefab;
    }

    // Scenes
    if (ext == ".scene") {
        return AssetType::Scene;
    }

    // Shaders
    if (ext == ".glsl" || ext == ".hlsl" || ext == ".vert" || ext == ".frag" ||
        ext == ".comp" || ext == ".geom" || ext == ".tesc" || ext == ".tese") {
        return AssetType::Shader;
    }

    return AssetType::Unknown;
}

glm::vec4 GetAssetTypeColor(AssetType type) {
    switch (type) {
        case AssetType::Folder:     return glm::vec4(0.9f, 0.8f, 0.3f, 1.0f);  // Yellow
        case AssetType::SDFModel:   return glm::vec4(0.4f, 0.7f, 1.0f, 1.0f);  // Blue
        case AssetType::Mesh:       return glm::vec4(0.5f, 0.8f, 0.5f, 1.0f);  // Green
        case AssetType::Texture:    return glm::vec4(0.9f, 0.5f, 0.3f, 1.0f);  // Orange
        case AssetType::Material:   return glm::vec4(0.8f, 0.4f, 0.8f, 1.0f);  // Purple
        case AssetType::Animation:  return glm::vec4(0.3f, 0.8f, 0.9f, 1.0f);  // Cyan
        case AssetType::Audio:      return glm::vec4(0.4f, 0.9f, 0.4f, 1.0f);  // Light green
        case AssetType::Script:     return glm::vec4(0.9f, 0.9f, 0.4f, 1.0f);  // Yellow-green
        case AssetType::Prefab:     return glm::vec4(0.6f, 0.6f, 0.9f, 1.0f);  // Light blue
        case AssetType::Scene:      return glm::vec4(0.9f, 0.6f, 0.6f, 1.0f);  // Light red
        case AssetType::Shader:     return glm::vec4(0.7f, 0.5f, 0.9f, 1.0f);  // Violet
        default:                    return glm::vec4(0.6f, 0.6f, 0.6f, 1.0f);  // Gray
    }
}

std::vector<std::string> GetAssetTypeExtensions(AssetType type) {
    std::vector<std::string> extensions;

    switch (type) {
        case AssetType::SDFModel:
            extensions = {".sdf", ".nova"};
            break;
        case AssetType::Mesh:
            extensions = {".obj", ".fbx", ".gltf", ".glb"};
            break;
        case AssetType::Texture:
            extensions = {".png", ".jpg", ".jpeg", ".tga", ".hdr", ".bmp", ".dds"};
            break;
        case AssetType::Material:
            extensions = {".mat", ".material"};
            break;
        case AssetType::Animation:
            extensions = {".anim"};
            break;
        case AssetType::Audio:
            extensions = {".wav", ".ogg", ".mp3", ".flac"};
            break;
        case AssetType::Script:
            extensions = {".lua", ".py"};
            break;
        case AssetType::Prefab:
            extensions = {".prefab"};
            break;
        case AssetType::Scene:
            extensions = {".scene"};
            break;
        case AssetType::Shader:
            extensions = {".glsl", ".hlsl", ".vert", ".frag", ".comp"};
            break;
        default:
            break;
    }

    return extensions;
}

bool IsImportableFile(const std::string& extension) {
    AssetType type = DetectAssetType(extension);
    return type != AssetType::Unknown && type != AssetType::Folder;
}

// =============================================================================
// AssetEntry Implementation
// =============================================================================

std::string AssetEntry::GetFormattedSize() const {
    return FormatFileSize(fileSize);
}

std::string AssetEntry::GetFormattedTime() const {
    return FormatFileTime(modifiedTime);
}

// =============================================================================
// FolderTreeNode Implementation
// =============================================================================

void FolderTreeNode::LoadChildren() {
    if (childrenLoaded) return;

    children.clear();

    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.is_directory()) {
                std::string dirName = entry.path().filename().string();

                // Skip hidden folders
                if (!dirName.empty() && dirName[0] == '.') {
                    continue;
                }

                auto child = std::make_unique<FolderTreeNode>();
                child->path = entry.path().string();
                child->name = dirName;
                child->parent = this;

                // Check if this folder has subfolders (for expand arrow)
                child->hasSubfolders = false;
                for (const auto& subEntry : fs::directory_iterator(entry.path())) {
                    if (subEntry.is_directory()) {
                        std::string subName = subEntry.path().filename().string();
                        if (!subName.empty() && subName[0] != '.') {
                            child->hasSubfolders = true;
                            break;
                        }
                    }
                }

                children.push_back(std::move(child));
            }
        }

        // Sort children alphabetically
        std::sort(children.begin(), children.end(),
            [](const auto& a, const auto& b) {
                return a->name < b->name;
            });

    } catch (const fs::filesystem_error&) {
        // Handle permission errors gracefully
    }

    childrenLoaded = true;
}

FolderTreeNode* FolderTreeNode::FindChild(const std::string& childPath) {
    if (path == childPath) {
        return this;
    }

    LoadChildren();

    for (auto& child : children) {
        if (childPath.find(child->path) == 0) {
            FolderTreeNode* result = child->FindChild(childPath);
            if (result) return result;
        }
    }

    return nullptr;
}

// =============================================================================
// Utility Functions
// =============================================================================

std::string FormatFileSize(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unitIndex < 4) {
        size /= 1024.0;
        unitIndex++;
    }

    std::ostringstream oss;
    if (unitIndex == 0) {
        oss << bytes << " " << units[unitIndex];
    } else {
        oss << std::fixed << std::setprecision(1) << size << " " << units[unitIndex];
    }

    return oss.str();
}

std::string FormatFileTime(const std::chrono::system_clock::time_point& time) {
    auto timeT = std::chrono::system_clock::to_time_t(time);
    std::tm tm = {};
#ifdef _WIN32
    localtime_s(&tm, &timeT);
#else
    localtime_r(&timeT, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M");
    return oss.str();
}

// =============================================================================
// AssetBrowser Implementation
// =============================================================================

AssetBrowser::AssetBrowser() {
    // Default config
    m_config.title = "Asset Browser";
    m_config.flags = EditorPanel::Flags::HasToolbar | EditorPanel::Flags::HasStatusBar |
                     EditorPanel::Flags::HasSearch;
    m_config.minSize = glm::vec2(400, 300);
    m_config.defaultSize = glm::vec2(800, 600);

    // Initialize default bookmarks
    m_bookmarks.push_back({"Project", "", "", glm::vec4(0.4f, 0.6f, 1.0f, 1.0f), true});
}

AssetBrowser::~AssetBrowser() = default;

// =============================================================================
// Path Management
// =============================================================================

void AssetBrowser::SetRootPath(const std::string& path) {
    try {
        fs::path absPath = fs::absolute(path);
        if (!fs::exists(absPath)) {
            return;
        }

        m_rootPath = absPath.string();
        m_currentPath = m_rootPath;

        // Update project bookmark
        if (!m_bookmarks.empty() && m_bookmarks[0].isBuiltIn) {
            m_bookmarks[0].path = m_rootPath;
            m_bookmarks[0].name = fs::path(m_rootPath).filename().string();
        }

        // Clear history and start fresh
        m_history.clear();
        m_historyIndex = 0;
        PushHistory(m_currentPath);

        // Rebuild folder tree
        BuildFolderTree();

        m_needsRescan = true;
    } catch (const fs::filesystem_error&) {
        // Handle error
    }
}

void AssetBrowser::NavigateTo(const std::string& path) {
    try {
        fs::path absPath = fs::absolute(path);

        if (!fs::exists(absPath) || !fs::is_directory(absPath)) {
            return;
        }

        if (!IsPathWithinRoot(absPath.string())) {
            return;
        }

        m_currentPath = absPath.string();
        PushHistory(m_currentPath);
        UpdateFolderTreeSelection();
        m_needsRescan = true;

        // Clear selection when changing directory
        ClearSelection();

        if (Callbacks.onDirectoryChanged) {
            Callbacks.onDirectoryChanged(m_currentPath);
        }
    } catch (const fs::filesystem_error&) {
        // Handle error
    }
}

void AssetBrowser::NavigateUp() {
    if (m_currentPath == m_rootPath) {
        return;
    }

    try {
        fs::path parentPath = fs::path(m_currentPath).parent_path();
        if (IsPathWithinRoot(parentPath.string())) {
            NavigateTo(parentPath.string());
        }
    } catch (const fs::filesystem_error&) {
        // Handle error
    }
}

void AssetBrowser::NavigateBack() {
    if (!CanNavigateBack()) {
        return;
    }

    m_historyIndex--;
    m_currentPath = m_history[m_historyIndex];
    UpdateFolderTreeSelection();
    m_needsRescan = true;
    ClearSelection();

    if (Callbacks.onDirectoryChanged) {
        Callbacks.onDirectoryChanged(m_currentPath);
    }
}

void AssetBrowser::NavigateForward() {
    if (!CanNavigateForward()) {
        return;
    }

    m_historyIndex++;
    m_currentPath = m_history[m_historyIndex];
    UpdateFolderTreeSelection();
    m_needsRescan = true;
    ClearSelection();

    if (Callbacks.onDirectoryChanged) {
        Callbacks.onDirectoryChanged(m_currentPath);
    }
}

void AssetBrowser::Refresh() {
    m_needsRescan = true;

    // Invalidate thumbnails in current directory
    if (m_thumbnailCache) {
        m_thumbnailCache->InvalidateDirectory(m_currentPath);
    }
}

void AssetBrowser::PushHistory(const std::string& path) {
    // Remove forward history
    while (m_history.size() > m_historyIndex + 1) {
        m_history.pop_back();
    }

    // Don't add duplicate
    if (!m_history.empty() && m_history.back() == path) {
        return;
    }

    m_history.push_back(path);
    m_historyIndex = m_history.size() - 1;

    // Limit history size
    while (m_history.size() > MAX_HISTORY) {
        m_history.pop_front();
        m_historyIndex--;
    }
}

// =============================================================================
// View Configuration
// =============================================================================

void AssetBrowser::SetViewMode(AssetViewMode mode) {
    m_viewMode = mode;

    // For column view, initialize with current path
    if (mode == AssetViewMode::Column) {
        m_columnPaths.clear();
        m_columnPaths.push_back(m_currentPath);
        m_columnEntries.clear();
        m_columnEntries.resize(1);
    }
}

void AssetBrowser::SetIconSize(int size) {
    m_iconSize = std::clamp(size, m_minIconSize, m_maxIconSize);
}

void AssetBrowser::SetSortBy(AssetSortBy sortBy, SortDirection direction) {
    m_sortBy = sortBy;
    m_sortDirection = direction;
    SortEntries();
}

void AssetBrowser::SetShowHiddenFiles(bool show) {
    m_showHiddenFiles = show;
    m_needsRescan = true;
}

// =============================================================================
// Selection
// =============================================================================

std::vector<std::string> AssetBrowser::GetSelectedPaths() const {
    std::vector<std::string> paths;
    paths.reserve(m_selectedPaths.size());
    for (const auto& path : m_selectedPaths) {
        paths.push_back(path);
    }
    return paths;
}

std::vector<const AssetEntry*> AssetBrowser::GetSelectedEntries() const {
    std::vector<const AssetEntry*> entries;
    for (const auto& entry : m_entries) {
        if (m_selectedPaths.count(entry.path) > 0) {
            entries.push_back(&entry);
        }
    }
    return entries;
}

void AssetBrowser::Select(const std::string& path, bool addToSelection) {
    if (!addToSelection) {
        ClearSelection();
    }

    m_selectedPaths.insert(path);
    m_lastSelectedPath = path;

    // Update entry selection state
    for (auto& entry : m_entries) {
        entry.isSelected = m_selectedPaths.count(entry.path) > 0;
    }

    if (Callbacks.onSelectionChanged) {
        Callbacks.onSelectionChanged(GetSelectedPaths());
    }
}

void AssetBrowser::ClearSelection() {
    m_selectedPaths.clear();
    m_lastSelectedPath.clear();

    for (auto& entry : m_entries) {
        entry.isSelected = false;
    }

    if (Callbacks.onSelectionChanged) {
        Callbacks.onSelectionChanged({});
    }
}

void AssetBrowser::SelectAll() {
    m_selectedPaths.clear();
    for (auto& entry : m_entries) {
        if (!entry.IsFolder() || m_typeFilters.empty()) {
            m_selectedPaths.insert(entry.path);
            entry.isSelected = true;
        }
    }

    if (Callbacks.onSelectionChanged) {
        Callbacks.onSelectionChanged(GetSelectedPaths());
    }
}

size_t AssetBrowser::GetSelectionCount() const {
    return m_selectedPaths.size();
}

void AssetBrowser::HandleSelection(AssetEntry& entry, bool ctrlHeld, bool shiftHeld) {
    if (shiftHeld && !m_lastSelectedPath.empty()) {
        // Range selection
        int startIdx = -1;
        int endIdx = -1;
        int lastIdx = -1;

        for (int i = 0; i < static_cast<int>(m_filteredEntries.size()); i++) {
            if (m_filteredEntries[i]->path == m_lastSelectedPath) {
                lastIdx = i;
            }
            if (m_filteredEntries[i]->path == entry.path) {
                endIdx = i;
            }
        }

        if (lastIdx >= 0 && endIdx >= 0) {
            startIdx = std::min(lastIdx, endIdx);
            endIdx = std::max(lastIdx, endIdx);

            if (!ctrlHeld) {
                ClearSelection();
            }

            for (int i = startIdx; i <= endIdx; i++) {
                m_selectedPaths.insert(m_filteredEntries[i]->path);
                m_filteredEntries[i]->isSelected = true;
            }
        }
    } else if (ctrlHeld) {
        // Toggle selection
        if (m_selectedPaths.count(entry.path) > 0) {
            m_selectedPaths.erase(entry.path);
            entry.isSelected = false;
        } else {
            m_selectedPaths.insert(entry.path);
            entry.isSelected = true;
        }
        m_lastSelectedPath = entry.path;
    } else {
        // Single selection
        ClearSelection();
        m_selectedPaths.insert(entry.path);
        entry.isSelected = true;
        m_lastSelectedPath = entry.path;
    }

    if (Callbacks.onSelectionChanged) {
        Callbacks.onSelectionChanged(GetSelectedPaths());
    }
}

void AssetBrowser::HandleDoubleClick(AssetEntry& entry) {
    if (entry.IsFolder()) {
        NavigateTo(entry.path);
    } else {
        if (Callbacks.onAssetOpened) {
            Callbacks.onAssetOpened(entry.path, entry.type);
        }
    }
}

int AssetBrowser::GetEntryIndex(const AssetEntry& entry) const {
    for (size_t i = 0; i < m_filteredEntries.size(); i++) {
        if (m_filteredEntries[i]->path == entry.path) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

AssetEntry* AssetBrowser::GetEntryByPath(const std::string& path) {
    for (auto& entry : m_entries) {
        if (entry.path == path) {
            return &entry;
        }
    }
    return nullptr;
}

// =============================================================================
// Search and Filter
// =============================================================================

void AssetBrowser::SetSearchQuery(const std::string& query) {
    m_searchQuery = query;
    std::strncpy(m_searchBuffer, query.c_str(), sizeof(m_searchBuffer) - 1);
    m_searchBuffer[sizeof(m_searchBuffer) - 1] = '\0';

    if (query.empty()) {
        m_isSearching = false;
        ApplyFilters();
    } else {
        m_isSearching = true;
        PerformSearch();
    }
}

void AssetBrowser::ClearSearch() {
    SetSearchQuery("");
}

void AssetBrowser::SetTypeFilter(AssetType type, bool enabled) {
    if (enabled) {
        m_typeFilters.insert(type);
    } else {
        m_typeFilters.erase(type);
    }
    ApplyFilters();
}

void AssetBrowser::ClearTypeFilters() {
    m_typeFilters.clear();
    ApplyFilters();
}

void AssetBrowser::PerformSearch() {
    m_searchResults.clear();

    if (m_searchQuery.empty()) {
        return;
    }

    std::string lowerQuery = m_searchQuery;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    if (m_recursiveSearch) {
        std::vector<AssetEntry> allEntries;
        ScanDirectoryRecursive(m_currentPath, allEntries);

        for (auto& entry : allEntries) {
            if (MatchesSearch(entry)) {
                SearchResult result;
                result.entry = entry;
                result.matchedOn = "name";
                result.relevance = 1.0f;
                m_searchResults.push_back(result);
            }
        }
    } else {
        for (auto& entry : m_entries) {
            if (MatchesSearch(entry)) {
                SearchResult result;
                result.entry = entry;
                result.matchedOn = "name";
                result.relevance = 1.0f;
                m_searchResults.push_back(result);
            }
        }
    }

    // Sort by relevance
    std::sort(m_searchResults.begin(), m_searchResults.end(),
        [](const SearchResult& a, const SearchResult& b) {
            return a.relevance > b.relevance;
        });
}

bool AssetBrowser::MatchesFilter(const AssetEntry& entry) const {
    // Type filter
    if (!m_typeFilters.empty()) {
        if (m_typeFilters.count(entry.type) == 0) {
            return false;
        }
    }

    return true;
}

bool AssetBrowser::MatchesSearch(const AssetEntry& entry) const {
    if (m_searchQuery.empty()) {
        return true;
    }

    std::string lowerQuery = m_searchQuery;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    std::string lowerName = entry.filename;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

    return lowerName.find(lowerQuery) != std::string::npos;
}

void AssetBrowser::ApplyFilters() {
    m_filteredEntries.clear();

    for (auto& entry : m_entries) {
        if (MatchesFilter(entry) && MatchesSearch(entry)) {
            m_filteredEntries.push_back(&entry);
        }
    }
}

// =============================================================================
// File Operations
// =============================================================================

bool AssetBrowser::CreateFolder(const std::string& name) {
    try {
        fs::path newPath = fs::path(m_currentPath) / name;

        if (fs::exists(newPath)) {
            return false;
        }

        fs::create_directory(newPath);
        Refresh();
        return true;
    } catch (const fs::filesystem_error&) {
        return false;
    }
}

bool AssetBrowser::CreateAsset(AssetType type, const std::string& name) {
    try {
        std::string extension;

        switch (type) {
            case AssetType::Material:   extension = ".material"; break;
            case AssetType::Scene:      extension = ".scene"; break;
            case AssetType::Prefab:     extension = ".prefab"; break;
            case AssetType::Script:     extension = ".lua"; break;
            case AssetType::Shader:     extension = ".glsl"; break;
            default:                    return false;
        }

        fs::path newPath = fs::path(m_currentPath) / (name + extension);

        if (fs::exists(newPath)) {
            return false;
        }

        // Create empty file
        std::ofstream file(newPath);
        file.close();

        Refresh();
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void AssetBrowser::RenameSelected() {
    if (m_selectedPaths.size() != 1) {
        return;
    }

    AssetEntry* entry = GetEntryByPath(*m_selectedPaths.begin());
    if (entry) {
        m_isRenaming = true;
        m_renamingEntry = entry;
        std::strncpy(m_renameBuffer, entry->displayName.c_str(), sizeof(m_renameBuffer) - 1);
        m_renameBuffer[sizeof(m_renameBuffer) - 1] = '\0';
        m_renameNeedsFocus = true;
    }
}

void AssetBrowser::DeleteSelected() {
    if (m_selectedPaths.empty()) {
        return;
    }

    m_pendingDelete.clear();
    for (const auto& path : m_selectedPaths) {
        m_pendingDelete.push_back(path);
    }

    m_showDeleteConfirmation = true;
}

void AssetBrowser::DuplicateSelected() {
    try {
        for (const auto& path : m_selectedPaths) {
            fs::path srcPath(path);
            std::string baseName = srcPath.stem().string();
            std::string extension = srcPath.extension().string();

            std::string newName = GenerateUniqueName(baseName + "_copy", extension);
            fs::path dstPath = srcPath.parent_path() / newName;

            if (fs::is_directory(srcPath)) {
                fs::copy(srcPath, dstPath, fs::copy_options::recursive);
            } else {
                fs::copy_file(srcPath, dstPath);
            }
        }

        Refresh();
    } catch (const fs::filesystem_error&) {
        // Handle error
    }
}

void AssetBrowser::CutSelected() {
    m_clipboard.clear();
    for (const auto& path : m_selectedPaths) {
        m_clipboard.push_back(path);
    }
    m_clipboardIsCut = true;

    // Mark entries as cut
    for (auto& entry : m_entries) {
        entry.isCut = m_selectedPaths.count(entry.path) > 0;
    }
}

void AssetBrowser::CopySelected() {
    m_clipboard.clear();
    for (const auto& path : m_selectedPaths) {
        m_clipboard.push_back(path);
    }
    m_clipboardIsCut = false;

    // Clear cut marks
    for (auto& entry : m_entries) {
        entry.isCut = false;
    }
}

void AssetBrowser::Paste() {
    if (m_clipboard.empty()) {
        return;
    }

    try {
        for (const auto& srcPath : m_clipboard) {
            fs::path src(srcPath);
            std::string baseName = src.stem().string();
            std::string extension = src.extension().string();

            std::string newName = GenerateUniqueName(baseName, extension);
            fs::path dstPath = fs::path(m_currentPath) / newName;

            if (m_clipboardIsCut) {
                fs::rename(src, dstPath);

                if (Callbacks.onAssetMoved) {
                    Callbacks.onAssetMoved(srcPath, dstPath.string());
                }
            } else {
                if (fs::is_directory(src)) {
                    fs::copy(src, dstPath, fs::copy_options::recursive);
                } else {
                    fs::copy_file(src, dstPath);
                }
            }
        }

        if (m_clipboardIsCut) {
            m_clipboard.clear();
            m_clipboardIsCut = false;
        }

        Refresh();
    } catch (const fs::filesystem_error&) {
        // Handle error
    }
}

// =============================================================================
// Bookmarks
// =============================================================================

void AssetBrowser::AddBookmark(const std::string& name) {
    AssetBookmark bookmark;
    bookmark.path = m_currentPath;
    bookmark.name = name.empty() ? fs::path(m_currentPath).filename().string() : name;
    bookmark.color = glm::vec4(0.4f, 0.7f, 0.4f, 1.0f);
    bookmark.isBuiltIn = false;

    m_bookmarks.push_back(bookmark);
}

void AssetBrowser::RemoveBookmark(const std::string& path) {
    auto it = std::remove_if(m_bookmarks.begin(), m_bookmarks.end(),
        [&path](const AssetBookmark& b) {
            return !b.isBuiltIn && b.path == path;
        });
    m_bookmarks.erase(it, m_bookmarks.end());
}

void AssetBrowser::GoToBookmark(const std::string& path) {
    NavigateTo(path);
}

// =============================================================================
// Import
// =============================================================================

void AssetBrowser::ImportFiles(const std::vector<std::string>& paths) {
    try {
        for (const auto& srcPath : paths) {
            fs::path src(srcPath);
            fs::path dst = fs::path(m_currentPath) / src.filename();

            if (fs::exists(dst)) {
                // Generate unique name
                std::string baseName = dst.stem().string();
                std::string extension = dst.extension().string();
                std::string newName = GenerateUniqueName(baseName, extension);
                dst = fs::path(m_currentPath) / newName;
            }

            fs::copy_file(src, dst);
        }

        if (Callbacks.onAssetsImported) {
            Callbacks.onAssetsImported(paths);
        }

        Refresh();
    } catch (const fs::filesystem_error&) {
        // Handle error
    }
}

void AssetBrowser::ShowImportDialog() {
    m_showImportDialog = true;
}

// =============================================================================
// Directory Scanning
// =============================================================================

void AssetBrowser::ScanDirectory(const std::string& path) {
    m_entries.clear();

    try {
        for (const auto& dirEntry : fs::directory_iterator(path)) {
            std::string filename = dirEntry.path().filename().string();

            // Skip hidden files unless enabled
            if (!m_showHiddenFiles && !filename.empty() && filename[0] == '.') {
                continue;
            }

            AssetEntry entry = CreateAssetEntry(dirEntry.path());
            m_entries.push_back(std::move(entry));
        }
    } catch (const fs::filesystem_error&) {
        // Handle permission errors
    }

    SortEntries();
    ApplyFilters();
    m_needsRescan = false;
    m_lastScanTime = 0.0f;
}

void AssetBrowser::ScanDirectoryRecursive(const std::string& path, std::vector<AssetEntry>& results) {
    try {
        for (const auto& dirEntry : fs::recursive_directory_iterator(path)) {
            std::string filename = dirEntry.path().filename().string();

            if (!m_showHiddenFiles && !filename.empty() && filename[0] == '.') {
                continue;
            }

            AssetEntry entry = CreateAssetEntry(dirEntry.path());
            results.push_back(std::move(entry));
        }
    } catch (const fs::filesystem_error&) {
        // Handle permission errors
    }
}

AssetEntry AssetBrowser::CreateAssetEntry(const fs::path& path) {
    AssetEntry entry;
    entry.path = path.string();
    entry.filename = path.filename().string();
    entry.extension = path.extension().string();

    // Convert extension to lowercase
    std::transform(entry.extension.begin(), entry.extension.end(),
                   entry.extension.begin(), ::tolower);

    // Display name (without extension for files)
    if (fs::is_directory(path)) {
        entry.type = AssetType::Folder;
        entry.displayName = entry.filename;
    } else {
        entry.type = DetectAssetType(entry.extension);
        entry.displayName = path.stem().string();
    }

    // File metadata
    try {
        if (!fs::is_directory(path)) {
            entry.fileSize = fs::file_size(path);
        }

        auto ftime = fs::last_write_time(path);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        entry.modifiedTime = sctp;
    } catch (const fs::filesystem_error&) {
        // Handle errors
    }

    return entry;
}

void AssetBrowser::BuildFolderTree() {
    m_folderTreeRoot = std::make_unique<FolderTreeNode>();
    m_folderTreeRoot->path = m_rootPath;
    m_folderTreeRoot->name = fs::path(m_rootPath).filename().string();
    m_folderTreeRoot->expanded = true;
    m_folderTreeRoot->LoadChildren();

    UpdateFolderTreeSelection();
}

void AssetBrowser::UpdateFolderTreeSelection() {
    if (!m_folderTreeRoot) {
        return;
    }

    // Deselect all
    std::function<void(FolderTreeNode*)> deselectAll = [&](FolderTreeNode* node) {
        node->selected = false;
        for (auto& child : node->children) {
            deselectAll(child.get());
        }
    };
    deselectAll(m_folderTreeRoot.get());

    // Find and select current path
    m_selectedFolder = m_folderTreeRoot->FindChild(m_currentPath);
    if (m_selectedFolder) {
        m_selectedFolder->selected = true;

        // Expand ancestors
        FolderTreeNode* parent = m_selectedFolder->parent;
        while (parent) {
            parent->expanded = true;
            parent = parent->parent;
        }
    }
}

// =============================================================================
// Sorting
// =============================================================================

void AssetBrowser::SortEntries() {
    // Folders always first
    std::stable_partition(m_entries.begin(), m_entries.end(),
        [](const AssetEntry& entry) {
            return entry.IsFolder();
        });

    auto compare = [this](const AssetEntry& a, const AssetEntry& b) -> bool {
        // Keep folders at top
        if (a.IsFolder() != b.IsFolder()) {
            return a.IsFolder();
        }

        bool result = false;

        switch (m_sortBy) {
            case AssetSortBy::Name:
                result = a.filename < b.filename;
                break;
            case AssetSortBy::Type:
                if (a.type != b.type) {
                    result = static_cast<int>(a.type) < static_cast<int>(b.type);
                } else {
                    result = a.filename < b.filename;
                }
                break;
            case AssetSortBy::Size:
                result = a.fileSize < b.fileSize;
                break;
            case AssetSortBy::DateModified:
                result = a.modifiedTime < b.modifiedTime;
                break;
        }

        return m_sortDirection == SortDirection::Ascending ? result : !result;
    };

    // Sort folders
    auto folderEnd = std::stable_partition(m_entries.begin(), m_entries.end(),
        [](const AssetEntry& e) { return e.IsFolder(); });
    std::sort(m_entries.begin(), folderEnd, compare);

    // Sort files
    std::sort(folderEnd, m_entries.end(), compare);

    ApplyFilters();
}

// =============================================================================
// Thumbnails
// =============================================================================

void AssetBrowser::RequestThumbnail(AssetEntry& entry) {
    if (entry.thumbnailLoading || entry.thumbnailFailed) {
        return;
    }

    if (!m_thumbnailCache) {
        entry.thumbnail = GetDefaultIcon(entry.type);
        return;
    }

    entry.thumbnailLoading = true;

    // Get priority based on visibility
    int priority = 5;

    auto thumbnail = m_thumbnailCache->GetThumbnail(entry.path, m_iconSize, priority);
    if (thumbnail) {
        entry.thumbnail = thumbnail;
        entry.thumbnailLoading = false;
    }
}

void AssetBrowser::UpdateThumbnails() {
    if (!m_thumbnailCache) {
        return;
    }

    // Process thumbnail queue
    m_thumbnailCache->ProcessQueue(8.0f);  // 8ms budget per frame

    // Update entries with completed thumbnails
    for (auto& entry : m_entries) {
        if (entry.thumbnailLoading) {
            if (m_thumbnailCache->HasValidThumbnail(entry.path)) {
                entry.thumbnail = m_thumbnailCache->GetThumbnail(entry.path, m_iconSize);
                entry.thumbnailLoading = false;
            }
        }
    }
}

std::shared_ptr<Texture> AssetBrowser::GetDefaultIcon(AssetType type) {
    auto it = m_defaultIcons.find(type);
    if (it != m_defaultIcons.end()) {
        return it->second;
    }

    // Return unknown icon if no specific icon exists
    return m_unknownIcon;
}

// =============================================================================
// Drag and Drop
// =============================================================================

bool AssetBrowser::HandleDragSource(AssetEntry& entry) {
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        // Build payload
        m_dragPayload.paths.clear();
        m_dragPayload.paths.reserve(m_selectedPaths.size());

        for (const auto& path : m_selectedPaths) {
            m_dragPayload.paths.push_back(path);
        }

        if (m_dragPayload.paths.empty()) {
            m_dragPayload.paths.push_back(entry.path);
        }

        m_dragPayload.primaryType = entry.type;
        m_dragPayload.isValid = true;

        // Set the complex payload for internal asset browser operations
        ImGui::SetDragDropPayload("ASSET_BROWSER_ITEM",
                                  &m_dragPayload, sizeof(AssetDragPayload*));

        // Also set a simple ASSET_PATH payload for viewport drop targets
        // This allows viewports and other consumers to easily accept asset drops
        // For single selection, provide the path directly; for multi-select, use first item
        const std::string& primaryPath = m_dragPayload.paths.empty() ? entry.path : m_dragPayload.paths[0];
        ImGui::SetDragDropPayload("ASSET_PATH", primaryPath.c_str(), primaryPath.size() + 1);

        // Preview with drag hint
        ImGui::Text("Drop to add: %s", entry.displayName.c_str());
        if (m_dragPayload.paths.size() > 1) {
            ImGui::Text("(+%zu more)", m_dragPayload.paths.size() - 1);
        }

        ImGui::EndDragDropSource();
        m_isDragging = true;
        return true;
    }

    return false;
}

bool AssetBrowser::HandleDropTarget() {
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_ITEM")) {
            // Move files to current directory
            AssetDragPayload* dragPayload = *(AssetDragPayload**)payload->Data;
            ExecuteMove(dragPayload->paths, m_currentPath);
        }

        ImGui::EndDragDropTarget();
        return true;
    }

    return false;
}

bool AssetBrowser::HandleExternalFileDrop(const std::vector<std::string>& paths) {
    m_pendingImports = paths;
    m_showImportDialog = true;
    return true;
}

void AssetBrowser::ExecuteMove(const std::vector<std::string>& sources, const std::string& destination) {
    try {
        for (const auto& srcPath : sources) {
            fs::path src(srcPath);
            fs::path dst = fs::path(destination) / src.filename();

            // Check if moving to same location
            if (src.parent_path() == fs::path(destination)) {
                continue;
            }

            // Check if destination already exists
            if (fs::exists(dst)) {
                std::string baseName = dst.stem().string();
                std::string extension = dst.extension().string();
                std::string newName = GenerateUniqueName(baseName, extension);
                dst = fs::path(destination) / newName;
            }

            fs::rename(src, dst);

            if (Callbacks.onAssetMoved) {
                Callbacks.onAssetMoved(srcPath, dst.string());
            }
        }

        Refresh();
    } catch (const fs::filesystem_error&) {
        // Handle error
    }
}

// =============================================================================
// Keyboard Handling
// =============================================================================

void AssetBrowser::HandleKeyboardInput() {
    if (!IsFocused()) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();

    // Navigation
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
        NavigateSelection(0, -1);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
        NavigateSelection(0, 1);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
        NavigateSelection(-1, 0);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
        NavigateSelection(1, 0);
    }

    // Enter to open
    if (ImGui::IsKeyPressed(ImGuiKey_Enter) && !m_selectedPaths.empty()) {
        AssetEntry* entry = GetEntryByPath(*m_selectedPaths.begin());
        if (entry) {
            HandleDoubleClick(*entry);
        }
    }

    // Backspace to go up
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
        NavigateUp();
    }

    // F2 to rename
    if (ImGui::IsKeyPressed(ImGuiKey_F2)) {
        RenameSelected();
    }

    // Delete
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        DeleteSelected();
    }

    // Ctrl+A to select all
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
        SelectAll();
    }

    // Ctrl+C to copy
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
        CopySelected();
    }

    // Ctrl+X to cut
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_X)) {
        CutSelected();
    }

    // Ctrl+V to paste
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V)) {
        Paste();
    }

    // Ctrl+D to duplicate
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
        DuplicateSelected();
    }

    // F5 to refresh
    if (ImGui::IsKeyPressed(ImGuiKey_F5)) {
        Refresh();
    }

    // Alt+Left for back
    if (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
        NavigateBack();
    }

    // Alt+Right for forward
    if (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
        NavigateForward();
    }

    // Alt+Up for parent directory
    if (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
        NavigateUp();
    }
}

void AssetBrowser::NavigateSelection(int dx, int dy) {
    if (m_filteredEntries.empty()) {
        return;
    }

    int currentIndex = m_focusedEntryIndex;
    if (currentIndex < 0) {
        currentIndex = 0;
    }

    int newIndex = currentIndex;

    if (m_viewMode == AssetViewMode::Grid) {
        // Calculate grid dimensions
        float contentWidth = ImGui::GetContentRegionAvail().x;
        int columns = std::max(1, static_cast<int>(contentWidth / (m_iconSize + 10)));

        if (dy != 0) {
            newIndex = currentIndex + dy * columns;
        }
        if (dx != 0) {
            newIndex = currentIndex + dx;
        }
    } else {
        newIndex = currentIndex + dy + dx;
    }

    // Clamp to valid range
    newIndex = std::clamp(newIndex, 0, static_cast<int>(m_filteredEntries.size()) - 1);

    if (newIndex != currentIndex && newIndex >= 0 && newIndex < static_cast<int>(m_filteredEntries.size())) {
        m_focusedEntryIndex = newIndex;
        ClearSelection();
        AssetEntry* entry = m_filteredEntries[newIndex];
        m_selectedPaths.insert(entry->path);
        entry->isSelected = true;
        m_lastSelectedPath = entry->path;
        m_scrollToEntry = entry;

        if (Callbacks.onSelectionChanged) {
            Callbacks.onSelectionChanged(GetSelectedPaths());
        }
    }
}

// =============================================================================
// Utility
// =============================================================================

std::string AssetBrowser::GetRelativePath(const std::string& absolutePath) const {
    try {
        fs::path abs(absolutePath);
        fs::path root(m_rootPath);
        return fs::relative(abs, root).string();
    } catch (const fs::filesystem_error&) {
        return absolutePath;
    }
}

std::string AssetBrowser::GetAbsolutePath(const std::string& relativePath) const {
    try {
        return (fs::path(m_rootPath) / relativePath).string();
    } catch (const fs::filesystem_error&) {
        return relativePath;
    }
}

bool AssetBrowser::IsPathWithinRoot(const std::string& path) const {
    try {
        fs::path absPath = fs::absolute(path);
        fs::path rootPath = fs::absolute(m_rootPath);

        std::string absStr = absPath.string();
        std::string rootStr = rootPath.string();

        return absStr.find(rootStr) == 0;
    } catch (const fs::filesystem_error&) {
        return false;
    }
}

std::string AssetBrowser::GenerateUniqueName(const std::string& baseName, const std::string& extension) {
    std::string name = baseName + extension;
    fs::path testPath = fs::path(m_currentPath) / name;

    if (!fs::exists(testPath)) {
        return name;
    }

    int counter = 1;
    while (counter < 1000) {
        name = baseName + "_" + std::to_string(counter) + extension;
        testPath = fs::path(m_currentPath) / name;

        if (!fs::exists(testPath)) {
            return name;
        }

        counter++;
    }

    return baseName + "_copy" + extension;
}

void AssetBrowser::ShowInExplorer(const std::string& path) {
#ifdef _WIN32
    std::wstring wpath(path.begin(), path.end());
    ShellExecuteW(nullptr, L"explore", wpath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
    std::string command = "open \"" + path + "\"";
    system(command.c_str());
#else
    std::string command = "xdg-open \"" + path + "\"";
    system(command.c_str());
#endif
}

void AssetBrowser::CopyPathToClipboard(const std::string& path) {
    ImGui::SetClipboardText(path.c_str());
}

// =============================================================================
// EditorPanel Overrides
// =============================================================================

void AssetBrowser::OnInitialize() {
    // Set up default bookmarks
    m_bookmarks.push_back({"Textures", "", "", glm::vec4(0.9f, 0.5f, 0.3f, 1.0f), false});
    m_bookmarks.push_back({"Models", "", "", glm::vec4(0.5f, 0.8f, 0.5f, 1.0f), false});
    m_bookmarks.push_back({"Materials", "", "", glm::vec4(0.8f, 0.4f, 0.8f, 1.0f), false});
}

void AssetBrowser::OnShutdown() {
    m_entries.clear();
    m_filteredEntries.clear();
    m_folderTreeRoot.reset();
}

void AssetBrowser::Update(float deltaTime) {
    EditorPanel::Update(deltaTime);

    // Auto-rescan periodically
    m_lastScanTime += deltaTime;
    if (m_lastScanTime >= RESCAN_INTERVAL) {
        m_needsRescan = true;
    }

    // Scan directory if needed
    if (m_needsRescan && !m_currentPath.empty()) {
        ScanDirectory(m_currentPath);
    }

    // Update thumbnails
    UpdateThumbnails();
}

void AssetBrowser::OnRender() {
    HandleKeyboardInput();

    float folderTreeWidth = m_showFolderTree ? m_folderTreeWidth : 0.0f;

    // Main layout with optional folder tree
    if (m_showFolderTree) {
        // Left panel - folder tree
        ImGui::BeginChild("FolderTree", ImVec2(folderTreeWidth, 0), true);
        RenderBookmarks();
        ImGui::Separator();
        RenderFolderTree();
        ImGui::EndChild();

        // Resize handle
        ImGui::SameLine();
        ImGui::Button("##resize_handle", ImVec2(4, -1));
        if (ImGui::IsItemActive()) {
            m_folderTreeWidth += ImGui::GetIO().MouseDelta.x;
            m_folderTreeWidth = std::clamp(m_folderTreeWidth, 100.0f, 400.0f);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }

        ImGui::SameLine();
    }

    // Right panel - content area
    ImGui::BeginChild("ContentArea", ImVec2(0, 0), false);

    // Breadcrumb path
    RenderBreadcrumbs();

    ImGui::Separator();

    // Content based on view mode
    RenderContentArea();

    ImGui::EndChild();

    // Context menu
    RenderContextMenu();

    // Popups
    RenderCreateAssetPopup();
    RenderDeleteConfirmation();
    RenderImportDialog();
    RenderRenamePopup();
}

void AssetBrowser::OnRenderToolbar() {
    auto& theme = EditorTheme::Instance();

    // Navigation buttons
    {
        ScopedDisable disable(!CanNavigateBack());
        if (EditorWidgets::IconButton("\xef\x81\x93", "Back (Alt+Left)")) {
            NavigateBack();
        }
    }

    ImGui::SameLine();

    {
        ScopedDisable disable(!CanNavigateForward());
        if (EditorWidgets::IconButton("\xef\x81\x94", "Forward (Alt+Right)")) {
            NavigateForward();
        }
    }

    ImGui::SameLine();

    {
        ScopedDisable disable(m_currentPath == m_rootPath);
        if (EditorWidgets::IconButton("\xef\x81\xa2", "Up (Alt+Up)")) {
            NavigateUp();
        }
    }

    ImGui::SameLine();

    if (EditorWidgets::IconButton("\xef\x80\xa1", "Refresh (F5)")) {
        Refresh();
    }

    ImGui::SameLine();
    EditorWidgets::ToolbarSeparator();
    ImGui::SameLine();

    // View mode buttons
    bool gridView = m_viewMode == AssetViewMode::Grid;
    bool listView = m_viewMode == AssetViewMode::List;
    bool columnView = m_viewMode == AssetViewMode::Column;

    if (EditorWidgets::ToolbarButton("\xef\x80\x8a", "Grid View", gridView)) {
        SetViewMode(AssetViewMode::Grid);
    }

    ImGui::SameLine();

    if (EditorWidgets::ToolbarButton("\xef\x80\x8b", "List View", listView)) {
        SetViewMode(AssetViewMode::List);
    }

    ImGui::SameLine();

    if (EditorWidgets::ToolbarButton("\xef\x82\x9b", "Column View", columnView)) {
        SetViewMode(AssetViewMode::Column);
    }

    ImGui::SameLine();
    EditorWidgets::ToolbarSeparator();
    ImGui::SameLine();

    // Icon size slider (only for grid view)
    if (m_viewMode == AssetViewMode::Grid) {
        ImGui::SetNextItemWidth(100);
        if (ImGui::SliderInt("##IconSize", &m_iconSize, m_minIconSize, m_maxIconSize, "%d px")) {
            // Icon size changed
        }
        ImGui::SameLine();
    }

    // Toggle folder tree
    if (EditorWidgets::ToolbarButton("\xef\x81\xbc", "Toggle Folder Tree", m_showFolderTree)) {
        m_showFolderTree = !m_showFolderTree;
    }

    ImGui::SameLine();
    EditorWidgets::ToolbarSpacer();

    // Search box
    ImGui::SetNextItemWidth(200);
    if (EditorWidgets::SearchInput("##AssetSearch", m_searchBuffer, sizeof(m_searchBuffer), "Search assets...")) {
        SetSearchQuery(m_searchBuffer);
    }

    ImGui::SameLine();

    // Create new button
    if (EditorWidgets::IconButton("\xef\x81\xa7", "Create New Asset")) {
        ImGui::OpenPopup("CreateAssetMenu");
    }

    if (ImGui::BeginPopup("CreateAssetMenu")) {
        if (ImGui::MenuItem("New Folder")) {
            m_showCreateAssetPopup = true;
            m_createAssetType = AssetType::Folder;
            std::strncpy(m_createAssetName, "New Folder", sizeof(m_createAssetName));
        }
        ImGui::Separator();
        if (ImGui::MenuItem("New Material")) {
            m_showCreateAssetPopup = true;
            m_createAssetType = AssetType::Material;
            std::strncpy(m_createAssetName, "New Material", sizeof(m_createAssetName));
        }
        if (ImGui::MenuItem("New Scene")) {
            m_showCreateAssetPopup = true;
            m_createAssetType = AssetType::Scene;
            std::strncpy(m_createAssetName, "New Scene", sizeof(m_createAssetName));
        }
        if (ImGui::MenuItem("New Prefab")) {
            m_showCreateAssetPopup = true;
            m_createAssetType = AssetType::Prefab;
            std::strncpy(m_createAssetName, "New Prefab", sizeof(m_createAssetName));
        }
        if (ImGui::MenuItem("New Script")) {
            m_showCreateAssetPopup = true;
            m_createAssetType = AssetType::Script;
            std::strncpy(m_createAssetName, "New Script", sizeof(m_createAssetName));
        }
        if (ImGui::MenuItem("New Shader")) {
            m_showCreateAssetPopup = true;
            m_createAssetType = AssetType::Shader;
            std::strncpy(m_createAssetName, "New Shader", sizeof(m_createAssetName));
        }
        ImGui::EndPopup();
    }
}

void AssetBrowser::OnRenderMenuBar() {
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Import...", "Ctrl+I")) {
            ShowImportDialog();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Refresh", "F5")) {
            Refresh();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Cut", "Ctrl+X", false, !m_selectedPaths.empty())) {
            CutSelected();
        }
        if (ImGui::MenuItem("Copy", "Ctrl+C", false, !m_selectedPaths.empty())) {
            CopySelected();
        }
        if (ImGui::MenuItem("Paste", "Ctrl+V", false, !m_clipboard.empty())) {
            Paste();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, !m_selectedPaths.empty())) {
            DuplicateSelected();
        }
        if (ImGui::MenuItem("Rename", "F2", false, m_selectedPaths.size() == 1)) {
            RenameSelected();
        }
        if (ImGui::MenuItem("Delete", "Del", false, !m_selectedPaths.empty())) {
            DeleteSelected();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Select All", "Ctrl+A")) {
            SelectAll();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("Grid View", nullptr, m_viewMode == AssetViewMode::Grid)) {
            SetViewMode(AssetViewMode::Grid);
        }
        if (ImGui::MenuItem("List View", nullptr, m_viewMode == AssetViewMode::List)) {
            SetViewMode(AssetViewMode::List);
        }
        if (ImGui::MenuItem("Column View", nullptr, m_viewMode == AssetViewMode::Column)) {
            SetViewMode(AssetViewMode::Column);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Show Folder Tree", nullptr, m_showFolderTree)) {
            m_showFolderTree = !m_showFolderTree;
        }
        if (ImGui::MenuItem("Show Hidden Files", nullptr, m_showHiddenFiles)) {
            SetShowHiddenFiles(!m_showHiddenFiles);
        }
        ImGui::Separator();
        if (ImGui::BeginMenu("Sort By")) {
            if (ImGui::MenuItem("Name", nullptr, m_sortBy == AssetSortBy::Name)) {
                SetSortBy(AssetSortBy::Name);
            }
            if (ImGui::MenuItem("Type", nullptr, m_sortBy == AssetSortBy::Type)) {
                SetSortBy(AssetSortBy::Type);
            }
            if (ImGui::MenuItem("Size", nullptr, m_sortBy == AssetSortBy::Size)) {
                SetSortBy(AssetSortBy::Size);
            }
            if (ImGui::MenuItem("Date Modified", nullptr, m_sortBy == AssetSortBy::DateModified)) {
                SetSortBy(AssetSortBy::DateModified);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Ascending", nullptr, m_sortDirection == SortDirection::Ascending)) {
                SetSortBy(m_sortBy, SortDirection::Ascending);
            }
            if (ImGui::MenuItem("Descending", nullptr, m_sortDirection == SortDirection::Descending)) {
                SetSortBy(m_sortBy, SortDirection::Descending);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Filter")) {
        bool noFilter = m_typeFilters.empty();
        if (ImGui::MenuItem("Show All", nullptr, noFilter)) {
            ClearTypeFilters();
        }
        ImGui::Separator();

        static const AssetType filterTypes[] = {
            AssetType::SDFModel, AssetType::Mesh, AssetType::Texture,
            AssetType::Material, AssetType::Animation, AssetType::Audio,
            AssetType::Script, AssetType::Prefab, AssetType::Scene, AssetType::Shader
        };

        for (auto type : filterTypes) {
            bool isFiltered = m_typeFilters.count(type) > 0;
            if (ImGui::MenuItem(GetAssetTypeName(type), nullptr, isFiltered)) {
                SetTypeFilter(type, !isFiltered);
            }
        }

        ImGui::EndMenu();
    }
}

void AssetBrowser::OnRenderStatusBar() {
    // Current path
    std::string relativePath = GetRelativePath(m_currentPath);
    ImGui::TextUnformatted(relativePath.c_str());

    EditorWidgets::StatusBarSeparator();

    // Item count
    size_t totalItems = m_entries.size();
    size_t visibleItems = m_filteredEntries.size();

    if (totalItems == visibleItems) {
        ImGui::Text("%zu items", totalItems);
    } else {
        ImGui::Text("%zu of %zu items", visibleItems, totalItems);
    }

    EditorWidgets::StatusBarSeparator();

    // Selection count
    if (!m_selectedPaths.empty()) {
        ImGui::Text("%zu selected", m_selectedPaths.size());
    }

    // Thumbnail loading indicator
    if (m_thumbnailCache && m_thumbnailCache->HasPendingRequests()) {
        EditorWidgets::StatusBarSeparator();
        EditorWidgets::Spinner("Loading", 6.0f, 2.0f);
        ImGui::SameLine();
        ImGui::Text("Loading thumbnails (%zu)", m_thumbnailCache->GetPendingCount());
    }
}

void AssetBrowser::OnSearchChanged(const std::string& filter) {
    SetSearchQuery(filter);
}

// =============================================================================
// Rendering - Folder Tree
// =============================================================================

void AssetBrowser::RenderBookmarks() {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "FAVORITES");
    ImGui::Spacing();

    for (auto& bookmark : m_bookmarks) {
        if (bookmark.path.empty()) {
            continue;
        }

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                   ImGuiTreeNodeFlags_SpanAvailWidth;

        if (m_currentPath == bookmark.path) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(bookmark.color.x, bookmark.color.y,
                                                     bookmark.color.z, bookmark.color.w));

        if (ImGui::TreeNodeEx(bookmark.name.c_str(), flags)) {
            if (ImGui::IsItemClicked()) {
                NavigateTo(bookmark.path);
            }

            // Context menu for removing bookmark
            if (ImGui::BeginPopupContextItem()) {
                if (!bookmark.isBuiltIn && ImGui::MenuItem("Remove Bookmark")) {
                    RemoveBookmark(bookmark.path);
                }
                ImGui::EndPopup();
            }
        }

        ImGui::PopStyleColor();
    }
}

void AssetBrowser::RenderFolderTree() {
    if (!m_folderTreeRoot) {
        return;
    }

    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "FOLDERS");
    ImGui::Spacing();

    RenderFolderTreeNode(m_folderTreeRoot.get());
}

void AssetBrowser::RenderFolderTreeNode(FolderTreeNode* node) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

    if (!node->hasSubfolders) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    if (node->selected) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    if (node->expanded) {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    }

    bool isOpen = ImGui::TreeNodeEx(node->name.c_str(), flags);

    // Handle click
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        NavigateTo(node->path);
    }

    // Drag drop target for folders
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_ITEM")) {
            AssetDragPayload* dragPayload = *(AssetDragPayload**)payload->Data;
            ExecuteMove(dragPayload->paths, node->path);
        }
        ImGui::EndDragDropTarget();
    }

    // Context menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Open in Explorer")) {
            ShowInExplorer(node->path);
        }
        if (ImGui::MenuItem("Add to Favorites")) {
            AddBookmark(node->name);
        }
        ImGui::EndPopup();
    }

    node->expanded = isOpen;

    if (isOpen) {
        node->LoadChildren();

        for (auto& child : node->children) {
            RenderFolderTreeNode(child.get());
        }

        ImGui::TreePop();
    }
}

// =============================================================================
// Rendering - Breadcrumbs
// =============================================================================

void AssetBrowser::RenderBreadcrumbs() {
    // Build path segments
    std::vector<std::pair<std::string, std::string>> segments;  // name, path

    fs::path current(m_currentPath);
    fs::path root(m_rootPath);

    while (current != root.parent_path()) {
        segments.push_back({current.filename().string(), current.string()});

        if (current == root) {
            break;
        }

        current = current.parent_path();
    }

    std::reverse(segments.begin(), segments.end());

    // Render breadcrumbs
    for (size_t i = 0; i < segments.size(); i++) {
        if (i > 0) {
            ImGui::SameLine();
            ImGui::TextDisabled(">");
            ImGui::SameLine();
        }

        bool isLast = (i == segments.size() - 1);

        if (isLast) {
            ImGui::TextUnformatted(segments[i].first.c_str());
        } else {
            if (ImGui::SmallButton(segments[i].first.c_str())) {
                NavigateTo(segments[i].second);
            }
        }

        // Drop target for each segment
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_ITEM")) {
                AssetDragPayload* dragPayload = *(AssetDragPayload**)payload->Data;
                ExecuteMove(dragPayload->paths, segments[i].second);
            }
            ImGui::EndDragDropTarget();
        }
    }
}

// =============================================================================
// Rendering - Content Area
// =============================================================================

void AssetBrowser::RenderContentArea() {
    if (m_isSearching && !m_searchQuery.empty()) {
        RenderSearchResults();
        return;
    }

    switch (m_viewMode) {
        case AssetViewMode::Grid:
            RenderGridView();
            break;
        case AssetViewMode::List:
            RenderListView();
            break;
        case AssetViewMode::Column:
            RenderColumnView();
            break;
    }
}

void AssetBrowser::RenderGridView() {
    float contentWidth = ImGui::GetContentRegionAvail().x;
    float tileWidth = static_cast<float>(m_iconSize) + 10;
    float tileHeight = static_cast<float>(m_iconSize) + 30;  // Icon + label
    int columns = std::max(1, static_cast<int>(contentWidth / tileWidth));

    ImGui::BeginChild("GridContent", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    // Handle drop on empty space
    HandleDropTarget();

    int row = 0;
    int col = 0;

    for (size_t i = 0; i < m_filteredEntries.size(); i++) {
        AssetEntry& entry = *m_filteredEntries[i];

        glm::vec2 pos(col * tileWidth, row * tileHeight);
        glm::vec2 size(tileWidth - 4, tileHeight - 4);

        // Check if visible (simple culling)
        float scrollY = ImGui::GetScrollY();
        float windowHeight = ImGui::GetWindowHeight();

        if (pos.y + size.y < scrollY - 100 || pos.y > scrollY + windowHeight + 100) {
            // Skip rendering but advance position
            col++;
            if (col >= columns) {
                col = 0;
                row++;
            }
            continue;
        }

        ImGui::SetCursorPos(ImVec2(pos.x, pos.y));

        RenderAssetTile(entry, pos, size);

        // Scroll to entry if requested
        if (m_scrollToEntry == &entry) {
            ImGui::SetScrollHereY();
            m_scrollToEntry = nullptr;
        }

        col++;
        if (col >= columns) {
            col = 0;
            row++;
        }
    }

    // Set content size for scrolling
    float totalHeight = (row + 1) * tileHeight;
    ImGui::Dummy(ImVec2(0, totalHeight - ImGui::GetCursorPosY()));

    ImGui::EndChild();
}

void AssetBrowser::RenderListView() {
    ImGui::BeginChild("ListContent", ImVec2(0, 0), false);

    // Table headers
    if (ImGui::BeginTable("AssetTable", 4, ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable |
                                           ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        // Handle sorting
        ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs();
        if (sortSpecs && sortSpecs->SpecsDirty) {
            if (sortSpecs->SpecsCount > 0) {
                switch (sortSpecs->Specs[0].ColumnIndex) {
                    case 0: m_sortBy = AssetSortBy::Name; break;
                    case 1: m_sortBy = AssetSortBy::Type; break;
                    case 2: m_sortBy = AssetSortBy::Size; break;
                    case 3: m_sortBy = AssetSortBy::DateModified; break;
                }
                m_sortDirection = sortSpecs->Specs[0].SortDirection == ImGuiSortDirection_Ascending
                    ? SortDirection::Ascending : SortDirection::Descending;
                SortEntries();
            }
            sortSpecs->SpecsDirty = false;
        }

        // Render rows
        for (int i = 0; i < static_cast<int>(m_filteredEntries.size()); i++) {
            RenderAssetListRow(*m_filteredEntries[i], i);
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
}

void AssetBrowser::RenderColumnView() {
    float columnWidth = 200.0f;
    float totalWidth = columnWidth * m_columnPaths.size();

    ImGui::BeginChild("ColumnContent", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    for (size_t i = 0; i < m_columnPaths.size(); i++) {
        if (i > 0) {
            ImGui::SameLine();
        }

        ImGui::BeginChild(("Column" + std::to_string(i)).c_str(),
                          ImVec2(columnWidth, 0), true);

        RenderColumnViewColumn(m_columnPaths[i], static_cast<int>(i));

        ImGui::EndChild();
    }

    ImGui::EndChild();
}

void AssetBrowser::RenderAssetTile(AssetEntry& entry, const glm::vec2& pos, const glm::vec2& size) {
    ImGui::PushID(entry.path.c_str());

    // Background
    ImVec2 tileMin = ImGui::GetCursorScreenPos();
    ImVec2 tileMax(tileMin.x + size.x, tileMin.y + size.y);

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImU32 bgColor = entry.isSelected ? IM_COL32(60, 100, 160, 200) :
                    entry.isHovered ? IM_COL32(50, 50, 60, 150) :
                    entry.isCut ? IM_COL32(60, 60, 60, 100) :
                    IM_COL32(0, 0, 0, 0);

    drawList->AddRectFilled(tileMin, tileMax, bgColor, 4.0f);

    // Invisible button for interaction
    ImGui::InvisibleButton("##tile", ImVec2(size.x, size.y));

    entry.isHovered = ImGui::IsItemHovered();

    // Handle click
    if (ImGui::IsItemClicked()) {
        bool ctrlHeld = ImGui::GetIO().KeyCtrl;
        bool shiftHeld = ImGui::GetIO().KeyShift;
        HandleSelection(entry, ctrlHeld, shiftHeld);

        // Update focused index
        for (size_t i = 0; i < m_filteredEntries.size(); i++) {
            if (m_filteredEntries[i] == &entry) {
                m_focusedEntryIndex = static_cast<int>(i);
                break;
            }
        }
    }

    // Handle double click
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
        HandleDoubleClick(entry);
    }

    // Context menu
    if (ImGui::BeginPopupContextItem()) {
        m_showContextMenu = true;
        m_contextMenuPos = glm::vec2(ImGui::GetMousePos().x, ImGui::GetMousePos().y);

        // Make sure this entry is selected
        if (m_selectedPaths.count(entry.path) == 0) {
            ClearSelection();
            Select(entry.path);
        }
    }

    // Drag source
    HandleDragSource(entry);

    // Drop target for folders
    if (entry.IsFolder()) {
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_ITEM")) {
                AssetDragPayload* dragPayload = *(AssetDragPayload**)payload->Data;
                ExecuteMove(dragPayload->paths, entry.path);
            }
            ImGui::EndDragDropTarget();
        }
    }

    // Thumbnail
    float iconSize = static_cast<float>(m_iconSize);
    ImVec2 iconPos(tileMin.x + (size.x - iconSize) * 0.5f, tileMin.y + 2);

    if (entry.thumbnail && entry.thumbnail->IsValid()) {
        drawList->AddImage(
            (ImTextureID)(intptr_t)entry.thumbnail->GetID(),
            iconPos,
            ImVec2(iconPos.x + iconSize, iconPos.y + iconSize)
        );
    } else {
        // Request thumbnail if not loading
        if (!entry.thumbnailLoading && !entry.IsFolder()) {
            RequestThumbnail(entry);
        }

        // Draw default icon
        glm::vec4 iconColor = GetAssetTypeColor(entry.type);
        drawList->AddRectFilled(
            iconPos,
            ImVec2(iconPos.x + iconSize, iconPos.y + iconSize),
            IM_COL32(iconColor.x * 100, iconColor.y * 100, iconColor.z * 100, 200),
            4.0f
        );

        // Draw type text
        const char* typeIcon = GetAssetTypeIcon(entry.type);
        ImVec2 textSize = ImGui::CalcTextSize(typeIcon);
        ImVec2 textPos(iconPos.x + (iconSize - textSize.x) * 0.5f,
                       iconPos.y + (iconSize - textSize.y) * 0.5f);
        drawList->AddText(textPos, IM_COL32(255, 255, 255, 200), typeIcon);
    }

    // Loading indicator
    if (entry.thumbnailLoading) {
        drawList->AddRectFilled(
            iconPos,
            ImVec2(iconPos.x + iconSize, iconPos.y + 4),
            IM_COL32(100, 150, 255, 200)
        );
    }

    // Cut overlay
    if (entry.isCut) {
        drawList->AddRectFilled(
            iconPos,
            ImVec2(iconPos.x + iconSize, iconPos.y + iconSize),
            IM_COL32(128, 128, 128, 128)
        );
    }

    // Label
    ImVec2 labelPos(tileMin.x + 4, iconPos.y + iconSize + 4);
    float labelWidth = size.x - 8;

    // Truncate text if needed
    std::string displayText = entry.displayName;
    ImVec2 textSize = ImGui::CalcTextSize(displayText.c_str());
    if (textSize.x > labelWidth) {
        while (displayText.length() > 3 && ImGui::CalcTextSize((displayText + "...").c_str()).x > labelWidth) {
            displayText.pop_back();
        }
        displayText += "...";
    }

    ImVec2 centeredPos(tileMin.x + (size.x - ImGui::CalcTextSize(displayText.c_str()).x) * 0.5f,
                       labelPos.y);
    drawList->AddText(centeredPos, IM_COL32(255, 255, 255, 255), displayText.c_str());

    ImGui::PopID();
}

void AssetBrowser::RenderAssetListRow(AssetEntry& entry, int rowIndex) {
    ImGui::PushID(entry.path.c_str());

    ImGui::TableNextRow();

    // Name column
    ImGui::TableNextColumn();

    ImGuiSelectableFlags selectableFlags = ImGuiSelectableFlags_SpanAllColumns |
                                           ImGuiSelectableFlags_AllowDoubleClick;

    if (ImGui::Selectable("##row", entry.isSelected, selectableFlags)) {
        bool ctrlHeld = ImGui::GetIO().KeyCtrl;
        bool shiftHeld = ImGui::GetIO().KeyShift;
        HandleSelection(entry, ctrlHeld, shiftHeld);
        m_focusedEntryIndex = rowIndex;
    }

    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
        HandleDoubleClick(entry);
    }

    // Context menu
    if (ImGui::BeginPopupContextItem()) {
        m_showContextMenu = true;
        if (m_selectedPaths.count(entry.path) == 0) {
            ClearSelection();
            Select(entry.path);
        }
    }

    HandleDragSource(entry);

    if (entry.IsFolder()) {
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_ITEM")) {
                AssetDragPayload* dragPayload = *(AssetDragPayload**)payload->Data;
                ExecuteMove(dragPayload->paths, entry.path);
            }
            ImGui::EndDragDropTarget();
        }
    }

    // Icon and name
    ImGui::SameLine();
    glm::vec4 iconColor = GetAssetTypeColor(entry.type);
    ImGui::TextColored(ImVec4(iconColor.x, iconColor.y, iconColor.z, iconColor.w),
                       "%s", GetAssetTypeIcon(entry.type));
    ImGui::SameLine();
    ImGui::TextUnformatted(entry.filename.c_str());

    // Type column
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(GetAssetTypeName(entry.type));

    // Size column
    ImGui::TableNextColumn();
    if (!entry.IsFolder()) {
        ImGui::TextUnformatted(entry.GetFormattedSize().c_str());
    }

    // Modified column
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(entry.GetFormattedTime().c_str());

    ImGui::PopID();
}

void AssetBrowser::RenderColumnViewColumn(const std::string& path, int columnIndex) {
    // Scan this column's directory if needed
    if (columnIndex >= static_cast<int>(m_columnEntries.size())) {
        m_columnEntries.resize(columnIndex + 1);
    }

    auto& entries = m_columnEntries[columnIndex];
    if (entries.empty() || (columnIndex < static_cast<int>(m_columnPaths.size()) &&
                            m_columnPaths[columnIndex] != path)) {
        entries.clear();
        try {
            for (const auto& dirEntry : fs::directory_iterator(path)) {
                entries.push_back(CreateAssetEntry(dirEntry.path()));
            }
        } catch (const fs::filesystem_error&) {
            // Handle error
        }

        // Sort entries
        std::sort(entries.begin(), entries.end(),
            [](const AssetEntry& a, const AssetEntry& b) {
                if (a.IsFolder() != b.IsFolder()) {
                    return a.IsFolder();
                }
                return a.filename < b.filename;
            });
    }

    // Render entries
    for (auto& entry : entries) {
        ImGuiSelectableFlags flags = ImGuiSelectableFlags_AllowDoubleClick;

        bool isSelected = (columnIndex + 1 < static_cast<int>(m_columnPaths.size()) &&
                          m_columnPaths[columnIndex + 1] == entry.path);

        if (ImGui::Selectable(entry.filename.c_str(), isSelected, flags)) {
            if (entry.IsFolder()) {
                // Truncate columns after this one and add new
                m_columnPaths.resize(columnIndex + 1);
                m_columnPaths.push_back(entry.path);
                m_columnEntries.resize(columnIndex + 1);
                m_currentPath = entry.path;
            } else {
                HandleDoubleClick(entry);
            }
        }

        // Show arrow for folders
        if (entry.IsFolder()) {
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 10);
            ImGui::TextDisabled(">");
        }
    }
}

void AssetBrowser::RenderSearchResults() {
    ImGui::Text("Search results for \"%s\":", m_searchQuery.c_str());
    ImGui::Separator();

    if (m_searchResults.empty()) {
        ImGui::TextDisabled("No results found");
        return;
    }

    for (auto& result : m_searchResults) {
        ImGuiSelectableFlags flags = ImGuiSelectableFlags_AllowDoubleClick;

        glm::vec4 iconColor = GetAssetTypeColor(result.entry.type);
        ImGui::TextColored(ImVec4(iconColor.x, iconColor.y, iconColor.z, iconColor.w),
                          "%s", GetAssetTypeIcon(result.entry.type));
        ImGui::SameLine();

        if (ImGui::Selectable(result.entry.filename.c_str(), result.entry.isSelected, flags)) {
            if (ImGui::IsMouseDoubleClicked(0)) {
                HandleDoubleClick(result.entry);
            } else {
                // Navigate to parent and select
                fs::path parentPath = fs::path(result.entry.path).parent_path();
                NavigateTo(parentPath.string());
                ClearSelection();
                Select(result.entry.path);
            }
        }

        // Show relative path
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)", GetRelativePath(result.entry.path).c_str());
    }
}

// =============================================================================
// Rendering - Popups
// =============================================================================

void AssetBrowser::RenderContextMenu() {
    if (ImGui::BeginPopupContextItem("AssetContextMenu")) {
        if (m_selectedPaths.empty()) {
            // Empty area context menu
            if (ImGui::MenuItem("New Folder")) {
                m_showCreateAssetPopup = true;
                m_createAssetType = AssetType::Folder;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Paste", "Ctrl+V", false, !m_clipboard.empty())) {
                Paste();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Refresh", "F5")) {
                Refresh();
            }
            if (ImGui::MenuItem("Show in Explorer")) {
                ShowInExplorer(m_currentPath);
            }
        } else {
            // Selected items context menu
            if (m_selectedPaths.size() == 1) {
                const std::string& path = *m_selectedPaths.begin();
                AssetEntry* entry = GetEntryByPath(path);

                if (entry && entry->IsFolder()) {
                    if (ImGui::MenuItem("Open")) {
                        NavigateTo(path);
                    }
                } else {
                    if (ImGui::MenuItem("Open")) {
                        if (Callbacks.onAssetOpened && entry) {
                            Callbacks.onAssetOpened(path, entry->type);
                        }
                    }
                }
                ImGui::Separator();
            }

            if (ImGui::MenuItem("Cut", "Ctrl+X")) {
                CutSelected();
            }
            if (ImGui::MenuItem("Copy", "Ctrl+C")) {
                CopySelected();
            }
            if (ImGui::MenuItem("Paste", "Ctrl+V", false, !m_clipboard.empty())) {
                Paste();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
                DuplicateSelected();
            }
            if (ImGui::MenuItem("Rename", "F2", false, m_selectedPaths.size() == 1)) {
                RenameSelected();
            }
            if (ImGui::MenuItem("Delete", "Del")) {
                DeleteSelected();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Copy Path")) {
                if (!m_selectedPaths.empty()) {
                    CopyPathToClipboard(*m_selectedPaths.begin());
                }
            }
            if (ImGui::MenuItem("Show in Explorer")) {
                if (!m_selectedPaths.empty()) {
                    ShowInExplorer(*m_selectedPaths.begin());
                }
            }
        }

        ImGui::EndPopup();
    }
}

void AssetBrowser::RenderCreateAssetPopup() {
    if (!m_showCreateAssetPopup) {
        return;
    }

    ImGui::OpenPopup("Create Asset");

    if (ImGui::BeginPopupModal("Create Asset", &m_showCreateAssetPopup,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter name for new %s:", GetAssetTypeName(m_createAssetType));
        ImGui::Spacing();

        ImGui::SetNextItemWidth(300);
        bool submitted = ImGui::InputText("##name", m_createAssetName, sizeof(m_createAssetName),
                                          ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::Spacing();

        if (ImGui::Button("Create", ImVec2(120, 0)) || submitted) {
            if (m_createAssetType == AssetType::Folder) {
                CreateFolder(m_createAssetName);
            } else {
                CreateAsset(m_createAssetType, m_createAssetName);
            }
            m_showCreateAssetPopup = false;
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showCreateAssetPopup = false;
        }

        ImGui::EndPopup();
    }
}

void AssetBrowser::RenderDeleteConfirmation() {
    if (!m_showDeleteConfirmation) {
        return;
    }

    ImGui::OpenPopup("Confirm Delete");

    if (ImGui::BeginPopupModal("Confirm Delete", &m_showDeleteConfirmation,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Are you sure you want to delete %zu item(s)?", m_pendingDelete.size());
        ImGui::Spacing();

        // Show list of items to delete
        if (m_pendingDelete.size() <= 10) {
            for (const auto& path : m_pendingDelete) {
                ImGui::BulletText("%s", fs::path(path).filename().string().c_str());
            }
        } else {
            for (size_t i = 0; i < 10; i++) {
                ImGui::BulletText("%s", fs::path(m_pendingDelete[i]).filename().string().c_str());
            }
            ImGui::BulletText("... and %zu more", m_pendingDelete.size() - 10);
        }

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "This action cannot be undone!");
        ImGui::Spacing();

        if (ImGui::Button("Delete", ImVec2(120, 0))) {
            try {
                for (const auto& path : m_pendingDelete) {
                    fs::remove_all(path);
                }

                if (Callbacks.onAssetsDeleted) {
                    Callbacks.onAssetsDeleted(m_pendingDelete);
                }

                ClearSelection();
                Refresh();
            } catch (const fs::filesystem_error&) {
                // Handle error
            }

            m_pendingDelete.clear();
            m_showDeleteConfirmation = false;
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_pendingDelete.clear();
            m_showDeleteConfirmation = false;
        }

        ImGui::EndPopup();
    }
}

void AssetBrowser::RenderImportDialog() {
    if (!m_showImportDialog) {
        return;
    }

    ImGui::OpenPopup("Import Assets");

    if (ImGui::BeginPopupModal("Import Assets", &m_showImportDialog,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Import %zu file(s) to current directory", m_pendingImports.size());
        ImGui::Separator();

        // Import settings
        if (ImGui::CollapsingHeader("Texture Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Generate Mipmaps", &m_importSettings.textureGenerateMipmaps);
            ImGui::Checkbox("sRGB", &m_importSettings.textureSRGB);
            ImGui::Checkbox("Compress", &m_importSettings.textureCompress);
            ImGui::SliderInt("Max Size", &m_importSettings.textureMaxSize, 256, 8192);
        }

        if (ImGui::CollapsingHeader("Mesh Settings")) {
            ImGui::Checkbox("Import Normals", &m_importSettings.meshImportNormals);
            ImGui::Checkbox("Import Tangents", &m_importSettings.meshImportTangents);
            ImGui::Checkbox("Import UVs", &m_importSettings.meshImportUVs);
            ImGui::Checkbox("Optimize", &m_importSettings.meshOptimize);
            ImGui::DragFloat("Scale", &m_importSettings.meshScale, 0.01f, 0.01f, 100.0f);
        }

        if (ImGui::CollapsingHeader("Audio Settings")) {
            ImGui::Checkbox("Compress", &m_importSettings.audioCompress);
            ImGui::SliderInt("Sample Rate", &m_importSettings.audioSampleRate, 22050, 96000);
        }

        ImGui::Separator();

        if (ImGui::Button("Import", ImVec2(120, 0))) {
            ImportFiles(m_pendingImports);
            m_pendingImports.clear();
            m_showImportDialog = false;
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_pendingImports.clear();
            m_showImportDialog = false;
        }

        ImGui::EndPopup();
    }
}

void AssetBrowser::RenderRenamePopup() {
    if (!m_isRenaming || !m_renamingEntry) {
        return;
    }

    ImGui::OpenPopup("Rename");

    if (ImGui::BeginPopupModal("Rename", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter new name:");
        ImGui::Spacing();

        if (m_renameNeedsFocus) {
            ImGui::SetKeyboardFocusHere();
            m_renameNeedsFocus = false;
        }

        ImGui::SetNextItemWidth(300);
        bool submitted = ImGui::InputText("##rename", m_renameBuffer, sizeof(m_renameBuffer),
                                          ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::Spacing();

        if (ImGui::Button("Rename", ImVec2(120, 0)) || submitted) {
            try {
                std::string newName = m_renameBuffer;
                if (!newName.empty()) {
                    fs::path oldPath(m_renamingEntry->path);
                    std::string extension = m_renamingEntry->IsFolder() ? "" : m_renamingEntry->extension;
                    fs::path newPath = oldPath.parent_path() / (newName + extension);

                    if (oldPath != newPath) {
                        fs::rename(oldPath, newPath);

                        if (Callbacks.onAssetMoved) {
                            Callbacks.onAssetMoved(oldPath.string(), newPath.string());
                        }

                        Refresh();
                    }
                }
            } catch (const fs::filesystem_error&) {
                // Handle error
            }

            m_isRenaming = false;
            m_renamingEntry = nullptr;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_isRenaming = false;
            m_renamingEntry = nullptr;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

} // namespace Nova
