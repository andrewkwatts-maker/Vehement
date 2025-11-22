#include "ContentBrowser.hpp"
#include "../Editor.hpp"
#include "../web/WebView.hpp"
#include "../web/JSBridge.hpp"
#include <Json/Json.h>
#include <imgui.h>
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace fs = std::filesystem;

namespace Vehement {
namespace Content {

// =============================================================================
// SelectionInfo implementation
// =============================================================================

bool SelectionInfo::IsSelected(const std::string& id) const {
    return std::find(assetIds.begin(), assetIds.end(), id) != assetIds.end();
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

ContentBrowser::ContentBrowser(Editor* editor)
    : m_editor(editor)
{
    m_database = std::make_unique<ContentDatabase>(editor);
    m_filter = std::make_unique<ContentFilter>();
    m_thumbnailGenerator = std::make_unique<ThumbnailGenerator>(editor);
    m_importer = std::make_unique<AssetImporter>(editor);
    m_actions = std::make_unique<ContentActions>(editor);
}

ContentBrowser::~ContentBrowser() {
    Shutdown();
}

// =============================================================================
// Lifecycle
// =============================================================================

bool ContentBrowser::Initialize(const std::string& configsPath) {
    m_configsPath = configsPath;
    m_currentFolder = configsPath;

    // Initialize subsystems
    if (!m_database->Initialize(configsPath)) {
        return false;
    }

    if (!m_thumbnailGenerator->Initialize()) {
        return false;
    }

    if (!m_importer->Initialize()) {
        return false;
    }

    if (!m_actions->Initialize()) {
        return false;
    }

    // Set up callbacks
    m_database->OnAssetAdded = [this](const std::string& id) {
        HandleAssetCreated(id);
    };

    m_database->OnAssetRemoved = [this](const std::string& id) {
        HandleAssetDeleted(id);
    };

    m_database->OnAssetModified = [this](const std::string& id) {
        HandleAssetModified(id);
    };

    // Build folder tree
    BuildFolderTree();

    // Set up JS Bridge
    SetupJSBridge();

    // Add initial navigation entry
    m_navigationHistory.push_back(m_currentFolder);
    m_navigationIndex = 0;

    m_needsRefresh = true;
    return true;
}

void ContentBrowser::Shutdown() {
    if (m_actions) {
        m_actions->Shutdown();
    }

    if (m_importer) {
        m_importer->Shutdown();
    }

    if (m_thumbnailGenerator) {
        m_thumbnailGenerator->Shutdown();
    }

    if (m_database) {
        m_database->Shutdown();
    }

    m_bookmarks.clear();
    m_recentFiles.clear();
    m_visibleAssets.clear();
    m_selection.Clear();
}

void ContentBrowser::Update(float deltaTime) {
    // Update subsystems
    m_database->Update(deltaTime);
    m_thumbnailGenerator->Update(deltaTime);
    m_importer->Update(deltaTime);

    // Refresh visible assets if needed
    if (m_needsRefresh) {
        UpdateVisibleAssets();
        m_needsRefresh = false;
    }
}

void ContentBrowser::Render() {
    // Main content browser window
    ImGui::Begin("Content Browser", nullptr, ImGuiWindowFlags_MenuBar);

    // Menu bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Grid", nullptr, m_viewMode == ViewMode::Grid)) {
                SetViewMode(ViewMode::Grid);
            }
            if (ImGui::MenuItem("List", nullptr, m_viewMode == ViewMode::List)) {
                SetViewMode(ViewMode::List);
            }
            if (ImGui::MenuItem("Details", nullptr, m_viewMode == ViewMode::Details)) {
                SetViewMode(ViewMode::Details);
            }
            ImGui::Separator();
            ImGui::MenuItem("Folder Tree", nullptr, &m_showFolderTree);
            ImGui::MenuItem("Preview Panel", nullptr, &m_showPreviewPanel);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Create")) {
            if (ImGui::MenuItem("Unit")) {
                CreateAsset(AssetType::Unit);
            }
            if (ImGui::MenuItem("Building")) {
                CreateAsset(AssetType::Building);
            }
            if (ImGui::MenuItem("Spell")) {
                CreateAsset(AssetType::Spell);
            }
            if (ImGui::MenuItem("Tile")) {
                CreateAsset(AssetType::Tile);
            }
            if (ImGui::MenuItem("Effect")) {
                CreateAsset(AssetType::Effect);
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    // Toolbar
    RenderToolbar();

    // Main content area
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float leftPanelWidth = m_showFolderTree ? m_folderTreeWidth : 0;
    float rightPanelWidth = m_showPreviewPanel ? m_previewPanelWidth : 0;
    float contentWidth = availableWidth - leftPanelWidth - rightPanelWidth;

    // Left panel - Folder tree
    if (m_showFolderTree) {
        ImGui::BeginChild("FolderTree", ImVec2(leftPanelWidth, 0), true);
        RenderFolderTree();
        ImGui::EndChild();
        ImGui::SameLine();
    }

    // Center panel - Content grid/list
    ImGui::BeginChild("Content", ImVec2(contentWidth, 0), true);
    RenderContentArea();
    ImGui::EndChild();

    // Right panel - Preview
    if (m_showPreviewPanel) {
        ImGui::SameLine();
        ImGui::BeginChild("Preview", ImVec2(rightPanelWidth, 0), true);
        RenderPreviewPanel();
        ImGui::EndChild();
    }

    // Status bar
    RenderStatusBar();

    // Context menu
    RenderContextMenu();

    // Dialogs
    RenderCreateDialog();
    RenderRenameDialog();

    // Handle keyboard input
    HandleKeyboardInput();

    ImGui::End();
}

// =============================================================================
// Navigation
// =============================================================================

void ContentBrowser::NavigateToFolder(const std::string& path) {
    if (path == m_currentFolder) {
        return;
    }

    // Validate path
    if (!fs::exists(path) || !fs::is_directory(path)) {
        return;
    }

    m_currentFolder = path;
    m_selectedFolder = path;

    // Update navigation history
    if (m_navigationIndex < static_cast<int>(m_navigationHistory.size()) - 1) {
        m_navigationHistory.resize(m_navigationIndex + 1);
    }
    m_navigationHistory.push_back(path);
    m_navigationIndex = static_cast<int>(m_navigationHistory.size()) - 1;

    // Clear selection
    ClearSelection();

    // Refresh
    m_needsRefresh = true;

    // Callback
    if (OnFolderChanged) {
        OnFolderChanged(path);
    }
}

void ContentBrowser::NavigateToAsset(const std::string& assetId) {
    auto metadata = m_database->GetAssetMetadata(assetId);
    if (!metadata) {
        return;
    }

    // Navigate to folder containing asset
    fs::path assetPath(metadata->path);
    std::string folder = assetPath.parent_path().string();
    NavigateToFolder(folder);

    // Select asset
    Select(assetId);

    // Add to recent files
    AddToRecentFiles(assetId);
}

void ContentBrowser::NavigateUp() {
    fs::path current(m_currentFolder);
    fs::path parent = current.parent_path();

    if (!parent.empty() && parent.string() >= m_configsPath) {
        NavigateToFolder(parent.string());
    }
}

void ContentBrowser::NavigateBack() {
    if (m_navigationIndex > 0) {
        m_navigationIndex--;
        m_currentFolder = m_navigationHistory[m_navigationIndex];
        m_selectedFolder = m_currentFolder;
        m_needsRefresh = true;

        if (OnFolderChanged) {
            OnFolderChanged(m_currentFolder);
        }
    }
}

void ContentBrowser::NavigateForward() {
    if (m_navigationIndex < static_cast<int>(m_navigationHistory.size()) - 1) {
        m_navigationIndex++;
        m_currentFolder = m_navigationHistory[m_navigationIndex];
        m_selectedFolder = m_currentFolder;
        m_needsRefresh = true;

        if (OnFolderChanged) {
            OnFolderChanged(m_currentFolder);
        }
    }
}

// =============================================================================
// Selection
// =============================================================================

void ContentBrowser::Select(const std::string& assetId, bool addToSelection, bool rangeSelect) {
    if (rangeSelect && !m_lastSelectedId.empty()) {
        // Range selection
        UpdateSelectionRange(m_lastSelectedId, assetId);
    } else if (addToSelection) {
        // Toggle selection
        auto it = std::find(m_selection.assetIds.begin(), m_selection.assetIds.end(), assetId);
        if (it != m_selection.assetIds.end()) {
            m_selection.assetIds.erase(it);
        } else {
            m_selection.assetIds.push_back(assetId);
        }
    } else {
        // Single selection
        m_selection.assetIds.clear();
        m_selection.assetIds.push_back(assetId);
    }

    m_selection.primaryId = assetId;
    m_selection.hasMultiple = m_selection.assetIds.size() > 1;
    m_lastSelectedId = assetId;

    // Callbacks
    if (OnAssetSelected) {
        OnAssetSelected(assetId);
    }
    if (OnSelectionChanged) {
        OnSelectionChanged(m_selection);
    }
}

void ContentBrowser::SelectMultiple(const std::vector<std::string>& assetIds) {
    m_selection.assetIds = assetIds;
    m_selection.hasMultiple = assetIds.size() > 1;
    if (!assetIds.empty()) {
        m_selection.primaryId = assetIds.back();
        m_lastSelectedId = assetIds.back();
    }

    if (OnSelectionChanged) {
        OnSelectionChanged(m_selection);
    }
}

void ContentBrowser::SelectAll() {
    m_selection.assetIds.clear();
    for (const auto& asset : m_visibleAssets) {
        m_selection.assetIds.push_back(asset.id);
    }
    m_selection.hasMultiple = m_selection.assetIds.size() > 1;
    if (!m_selection.assetIds.empty()) {
        m_selection.primaryId = m_selection.assetIds.back();
    }

    if (OnSelectionChanged) {
        OnSelectionChanged(m_selection);
    }
}

void ContentBrowser::ClearSelection() {
    m_selection.Clear();
    m_lastSelectedId.clear();

    if (OnSelectionChanged) {
        OnSelectionChanged(m_selection);
    }
}

void ContentBrowser::InvertSelection() {
    std::vector<std::string> newSelection;
    for (const auto& asset : m_visibleAssets) {
        if (!IsSelected(asset.id)) {
            newSelection.push_back(asset.id);
        }
    }
    SelectMultiple(newSelection);
}

bool ContentBrowser::IsSelected(const std::string& assetId) const {
    return m_selection.IsSelected(assetId);
}

// =============================================================================
// View Options
// =============================================================================

void ContentBrowser::SetViewMode(ViewMode mode) {
    m_viewMode = mode;
    m_needsRefresh = true;
}

void ContentBrowser::SetThumbnailSize(int size) {
    m_thumbnailSize = std::clamp(size, 32, 256);
}

void ContentBrowser::SetPreviewPanelVisible(bool visible) {
    m_showPreviewPanel = visible;
}

void ContentBrowser::SetFolderTreeVisible(bool visible) {
    m_showFolderTree = visible;
}

// =============================================================================
// Search & Filter
// =============================================================================

void ContentBrowser::SetSearchQuery(const std::string& query) {
    m_searchQuery = query;
    m_filter->SetSearchQuery(query);
    m_needsRefresh = true;
}

const std::string& ContentBrowser::GetSearchQuery() const {
    return m_searchQuery;
}

void ContentBrowser::FilterByType(AssetType type) {
    m_filter->FilterByType(type);
    m_needsRefresh = true;
}

void ContentBrowser::ClearFilters() {
    m_filter->ClearAll();
    m_searchQuery.clear();
    m_needsRefresh = true;
}

void ContentBrowser::ApplyFilterPreset(const std::string& presetName) {
    m_filter->ApplyBuiltInFilter(presetName);
    m_needsRefresh = true;
}

// =============================================================================
// Sorting
// =============================================================================

void ContentBrowser::SetSortField(SortField field) {
    m_filter->SetSortField(field);
    m_needsRefresh = true;
}

void ContentBrowser::SetSortDirection(SortDirection direction) {
    m_filter->SetSortDirection(direction);
    m_needsRefresh = true;
}

void ContentBrowser::ToggleSortDirection() {
    SortDirection current = m_filter->GetSortDirection();
    SetSortDirection(current == SortDirection::Ascending ? SortDirection::Descending : SortDirection::Ascending);
}

SortField ContentBrowser::GetSortField() const {
    return m_filter->GetSortField();
}

SortDirection ContentBrowser::GetSortDirection() const {
    return m_filter->GetSortDirection();
}

// =============================================================================
// Bookmarks & Favorites
// =============================================================================

void ContentBrowser::AddBookmark(const std::string& assetId) {
    // Check if already bookmarked
    for (const auto& bookmark : m_bookmarks) {
        if (bookmark.assetId == assetId) {
            return;
        }
    }

    auto metadata = m_database->GetAssetMetadata(assetId);
    if (!metadata) {
        return;
    }

    Bookmark bookmark;
    bookmark.id = "bookmark_" + std::to_string(m_bookmarks.size());
    bookmark.name = metadata->name;
    bookmark.assetId = assetId;
    bookmark.order = static_cast<int>(m_bookmarks.size());
    m_bookmarks.push_back(bookmark);
}

void ContentBrowser::RemoveBookmark(const std::string& assetId) {
    m_bookmarks.erase(
        std::remove_if(m_bookmarks.begin(), m_bookmarks.end(),
            [&](const Bookmark& b) { return b.assetId == assetId; }),
        m_bookmarks.end()
    );
}

void ContentBrowser::ToggleBookmark(const std::string& assetId) {
    for (const auto& bookmark : m_bookmarks) {
        if (bookmark.assetId == assetId) {
            RemoveBookmark(assetId);
            return;
        }
    }
    AddBookmark(assetId);
}

std::vector<Bookmark> ContentBrowser::GetBookmarks() const {
    return m_bookmarks;
}

void ContentBrowser::AddFolderBookmark(const std::string& folderPath, const std::string& name) {
    Bookmark bookmark;
    bookmark.id = "folder_bookmark_" + std::to_string(m_bookmarks.size());
    bookmark.name = name.empty() ? fs::path(folderPath).filename().string() : name;
    bookmark.folderPath = folderPath;
    bookmark.order = static_cast<int>(m_bookmarks.size());
    m_bookmarks.push_back(bookmark);
}

void ContentBrowser::RemoveFolderBookmark(const std::string& folderPath) {
    m_bookmarks.erase(
        std::remove_if(m_bookmarks.begin(), m_bookmarks.end(),
            [&](const Bookmark& b) { return b.folderPath == folderPath; }),
        m_bookmarks.end()
    );
}

// =============================================================================
// Recent Files
// =============================================================================

std::vector<RecentEntry> ContentBrowser::GetRecentFiles(size_t count) const {
    size_t n = std::min(count, m_recentFiles.size());
    return std::vector<RecentEntry>(m_recentFiles.begin(), m_recentFiles.begin() + n);
}

void ContentBrowser::ClearRecentFiles() {
    m_recentFiles.clear();
}

void ContentBrowser::AddToRecentFiles(const std::string& assetId) {
    // Remove existing entry if present
    m_recentFiles.erase(
        std::remove_if(m_recentFiles.begin(), m_recentFiles.end(),
            [&](const RecentEntry& e) { return e.assetId == assetId; }),
        m_recentFiles.end()
    );

    // Add new entry at front
    auto metadata = m_database->GetAssetMetadata(assetId);
    if (metadata) {
        RecentEntry entry;
        entry.assetId = assetId;
        entry.name = metadata->name;
        entry.type = metadata->type;
        entry.accessTime = std::chrono::system_clock::now();
        m_recentFiles.insert(m_recentFiles.begin(), entry);

        // Limit size
        if (m_recentFiles.size() > MAX_RECENT_FILES) {
            m_recentFiles.resize(MAX_RECENT_FILES);
        }
    }
}

// =============================================================================
// Drag & Drop
// =============================================================================

void ContentBrowser::BeginDrag(const std::vector<std::string>& assetIds) {
    m_dragDrop.assetIds = assetIds;
    m_dragDrop.sourceFolder = m_currentFolder;
    m_dragDrop.isDragging = true;
    m_dragDrop.isExternal = false;
}

void ContentBrowser::HandleDrop(const std::string& targetFolder) {
    if (!m_dragDrop.isDragging) {
        return;
    }

    if (m_dragDrop.isExternal) {
        // Import external files
        HandleExternalDrop(m_dragDrop.externalPaths, targetFolder);
    } else {
        // Move assets
        MoveOptions options;
        options.targetFolder = targetFolder;

        for (const auto& assetId : m_dragDrop.assetIds) {
            m_actions->MoveAsset(assetId, options);
        }
    }

    // Clear drag state
    m_dragDrop.assetIds.clear();
    m_dragDrop.isDragging = false;
    m_dragDrop.isExternal = false;
    m_dragDrop.externalPaths.clear();

    m_needsRefresh = true;
}

void ContentBrowser::HandleExternalDrop(const std::vector<std::string>& paths,
                                         const std::string& targetFolder) {
    ImportOptions options;
    options.targetDirectory = targetFolder;

    m_importer->ImportBatchAsync(paths, options, [this](const BatchImportResult& result) {
        m_needsRefresh = true;
    });
}

bool ContentBrowser::IsValidDropTarget(const std::string& targetFolder) const {
    if (!m_dragDrop.isDragging) {
        return false;
    }

    // Can't drop on source folder
    if (targetFolder == m_dragDrop.sourceFolder) {
        return false;
    }

    // Can't drop into itself or child
    for (const auto& assetId : m_dragDrop.assetIds) {
        auto metadata = m_database->GetAssetMetadata(assetId);
        if (metadata) {
            fs::path assetPath(metadata->path);
            fs::path target(targetFolder);
            if (target.string().find(assetPath.parent_path().string()) == 0) {
                return false;
            }
        }
    }

    return true;
}

// =============================================================================
// Context Menu
// =============================================================================

void ContentBrowser::ShowAssetContextMenu(const std::string& assetId, int x, int y) {
    m_contextMenuAssetId = assetId;
    m_contextMenuX = x;
    m_contextMenuY = y;
    m_contextMenuOpen = true;

    // Ensure asset is selected
    if (!IsSelected(assetId)) {
        Select(assetId);
    }
}

void ContentBrowser::ShowFolderContextMenu(const std::string& folderPath, int x, int y) {
    m_contextMenuAssetId.clear();
    m_contextMenuX = x;
    m_contextMenuY = y;
    m_contextMenuOpen = true;
    m_selectedFolder = folderPath;
}

void ContentBrowser::ShowBackgroundContextMenu(int x, int y) {
    m_contextMenuAssetId.clear();
    m_contextMenuX = x;
    m_contextMenuY = y;
    m_contextMenuOpen = true;
}

std::vector<ContextMenuAction> ContentBrowser::GetContextMenuActions() const {
    std::vector<ContextMenuAction> actions;

    if (!m_selection.IsEmpty()) {
        // Asset context menu
        actions.push_back({"open", "Open", "icon_open", "Enter", true, false, {}, nullptr});
        actions.push_back({"separator1", "", "", "", false, true, {}, nullptr});
        actions.push_back({"cut", "Cut", "icon_cut", "Ctrl+X", true, false, {}, nullptr});
        actions.push_back({"copy", "Copy", "icon_copy", "Ctrl+C", true, false, {}, nullptr});
        actions.push_back({"paste", "Paste", "icon_paste", "Ctrl+V", !m_actions->IsClipboardEmpty(), false, {}, nullptr});
        actions.push_back({"separator2", "", "", "", false, true, {}, nullptr});
        actions.push_back({"duplicate", "Duplicate", "icon_duplicate", "Ctrl+D", true, false, {}, nullptr});
        actions.push_back({"rename", "Rename", "icon_rename", "F2", m_selection.Count() == 1, false, {}, nullptr});
        actions.push_back({"delete", "Delete", "icon_delete", "Delete", true, false, {}, nullptr});
        actions.push_back({"separator3", "", "", "", false, true, {}, nullptr});
        actions.push_back({"bookmark", "Toggle Bookmark", "icon_star", "", true, false, {}, nullptr});
        actions.push_back({"show_in_explorer", "Show in Explorer", "icon_folder", "", true, false, {}, nullptr});
    } else {
        // Background context menu
        actions.push_back({"paste", "Paste", "icon_paste", "Ctrl+V", !m_actions->IsClipboardEmpty(), false, {}, nullptr});
        actions.push_back({"separator1", "", "", "", false, true, {}, nullptr});

        // Create submenu
        std::vector<ContextMenuAction> createSubmenu;
        createSubmenu.push_back({"create_unit", "Unit", "icon_unit", "", true, false, {}, nullptr});
        createSubmenu.push_back({"create_building", "Building", "icon_building", "", true, false, {}, nullptr});
        createSubmenu.push_back({"create_spell", "Spell", "icon_spell", "", true, false, {}, nullptr});
        createSubmenu.push_back({"create_tile", "Tile", "icon_tile", "", true, false, {}, nullptr});
        createSubmenu.push_back({"create_effect", "Effect", "icon_effect", "", true, false, {}, nullptr});
        createSubmenu.push_back({"create_folder", "Folder", "icon_folder", "", true, false, {}, nullptr});
        actions.push_back({"create", "Create", "icon_add", "", true, false, createSubmenu, nullptr});

        actions.push_back({"separator2", "", "", "", false, true, {}, nullptr});
        actions.push_back({"refresh", "Refresh", "icon_refresh", "F5", true, false, {}, nullptr});
        actions.push_back({"select_all", "Select All", "", "Ctrl+A", true, false, {}, nullptr});
    }

    return actions;
}

// =============================================================================
// Actions
// =============================================================================

void ContentBrowser::CreateAsset(AssetType type, const std::string& name) {
    m_createDialogType = type;
    m_createDialogName = name.empty() ? "New Asset" : name;
    m_showCreateDialog = true;
}

void ContentBrowser::DeleteSelected() {
    if (m_selection.IsEmpty()) {
        return;
    }

    DeleteOptions options;
    options.moveToTrash = true;

    for (const auto& assetId : m_selection.assetIds) {
        m_actions->DeleteAsset(assetId, options);
    }

    ClearSelection();
    m_needsRefresh = true;
}

void ContentBrowser::DuplicateSelected() {
    if (m_selection.IsEmpty()) {
        return;
    }

    DuplicateOptions options;

    for (const auto& assetId : m_selection.assetIds) {
        m_actions->DuplicateAsset(assetId, options);
    }

    m_needsRefresh = true;
}

void ContentBrowser::RenameSelected(const std::string& newName) {
    if (m_selection.Count() != 1) {
        return;
    }

    RenameOptions options;
    options.newName = newName;

    m_actions->RenameAsset(m_selection.primaryId, options);
    m_needsRefresh = true;
}

void ContentBrowser::OpenSelected() {
    if (m_selection.IsEmpty()) {
        return;
    }

    for (const auto& assetId : m_selection.assetIds) {
        OpenInHtmlEditor(assetId);
        AddToRecentFiles(assetId);

        if (OnAssetOpened) {
            OnAssetOpened(assetId);
        }
    }
}

void ContentBrowser::ShowInExplorer(const std::string& assetId) {
    auto metadata = m_database->GetAssetMetadata(assetId);
    if (!metadata) {
        return;
    }

#ifdef _WIN32
    std::string command = "explorer /select,\"" + metadata->path + "\"";
    system(command.c_str());
#elif __APPLE__
    std::string command = "open -R \"" + metadata->path + "\"";
    system(command.c_str());
#else
    std::string command = "xdg-open \"" + fs::path(metadata->path).parent_path().string() + "\"";
    system(command.c_str());
#endif
}

void ContentBrowser::CopySelected() {
    if (m_selection.IsEmpty()) {
        return;
    }
    m_actions->CopyToClipboard(m_selection.assetIds);
}

void ContentBrowser::CutSelected() {
    if (m_selection.IsEmpty()) {
        return;
    }
    m_actions->CutToClipboard(m_selection.assetIds);
}

void ContentBrowser::Paste() {
    m_actions->PasteFromClipboard(m_currentFolder);
    m_needsRefresh = true;
}

// =============================================================================
// Refresh
// =============================================================================

void ContentBrowser::Refresh() {
    m_needsRefresh = true;
    UpdateFolderCounts();
}

void ContentBrowser::Rescan() {
    m_database->Rescan();
    BuildFolderTree();
    m_needsRefresh = true;
}

// =============================================================================
// HTML Integration
// =============================================================================

void ContentBrowser::SendToHtml(const std::string& eventType, const std::string& jsonData) {
    if (m_bridge) {
        std::string script = "window.dispatchEvent(new CustomEvent('" + eventType + "', {detail: " + jsonData + "}));";
        // m_bridge->ExecuteScript(script);
    }
}

void ContentBrowser::OpenInHtmlEditor(const std::string& assetId) {
    auto metadata = m_database->GetAssetMetadata(assetId);
    if (!metadata) {
        return;
    }

    // Read asset content
    std::ifstream file(metadata->path);
    if (!file.is_open()) {
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    // Send to HTML editor
    Json::Value data;
    data["assetId"] = assetId;
    data["path"] = metadata->path;
    data["name"] = metadata->name;
    data["type"] = static_cast<int>(metadata->type);
    data["content"] = content;

    Json::StreamWriterBuilder writer;
    SendToHtml("openAsset", Json::writeString(writer, data));
}

// =============================================================================
// Private - UI Rendering
// =============================================================================

void ContentBrowser::RenderToolbar() {
    // Navigation buttons
    bool canGoBack = m_navigationIndex > 0;
    bool canGoForward = m_navigationIndex < static_cast<int>(m_navigationHistory.size()) - 1;

    ImGui::BeginDisabled(!canGoBack);
    if (ImGui::Button("<")) {
        NavigateBack();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();

    ImGui::BeginDisabled(!canGoForward);
    if (ImGui::Button(">")) {
        NavigateForward();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();

    if (ImGui::Button("^")) {
        NavigateUp();
    }

    ImGui::SameLine();

    if (ImGui::Button("Refresh")) {
        Refresh();
    }

    ImGui::SameLine();

    // Path breadcrumb
    ImGui::Text("Path: %s", m_currentFolder.c_str());

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 300);

    // Search box
    char searchBuffer[256] = {0};
    strncpy(searchBuffer, m_searchQuery.c_str(), sizeof(searchBuffer) - 1);

    ImGui::PushItemWidth(200);
    if (ImGui::InputText("##Search", searchBuffer, sizeof(searchBuffer))) {
        SetSearchQuery(searchBuffer);
    }
    ImGui::PopItemWidth();

    ImGui::SameLine();

    // View mode buttons
    if (ImGui::Button("Grid")) {
        SetViewMode(ViewMode::Grid);
    }
    ImGui::SameLine();
    if (ImGui::Button("List")) {
        SetViewMode(ViewMode::List);
    }
}

void ContentBrowser::RenderFolderTree() {
    ImGui::Text("Folders");
    ImGui::Separator();

    // Bookmarks section
    if (ImGui::TreeNode("Bookmarks")) {
        for (const auto& bookmark : m_bookmarks) {
            if (!bookmark.folderPath.empty()) {
                if (ImGui::Selectable(bookmark.name.c_str(), m_currentFolder == bookmark.folderPath)) {
                    NavigateToFolder(bookmark.folderPath);
                }
            }
        }
        ImGui::TreePop();
    }

    // Recent section
    if (ImGui::TreeNode("Recent")) {
        auto recent = GetRecentFiles(10);
        for (const auto& entry : recent) {
            if (ImGui::Selectable(entry.name.c_str())) {
                NavigateToAsset(entry.assetId);
            }
        }
        ImGui::TreePop();
    }

    ImGui::Separator();

    // Folder tree
    RenderFolderNode(m_rootFolder);
}

void ContentBrowser::RenderFolderNode(FolderNode& node) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

    if (node.children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    if (node.path == m_selectedFolder) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    std::string label = node.name;
    if (node.assetCount > 0) {
        label += " (" + std::to_string(node.assetCount) + ")";
    }

    bool isOpen = ImGui::TreeNodeEx(node.path.c_str(), flags, "%s", label.c_str());

    // Handle selection
    if (ImGui::IsItemClicked()) {
        NavigateToFolder(node.path);
    }

    // Context menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Add Bookmark")) {
            AddFolderBookmark(node.path);
        }
        if (ImGui::MenuItem("Create Folder")) {
            // Create folder dialog
        }
        ImGui::EndPopup();
    }

    if (isOpen) {
        for (auto& child : node.children) {
            RenderFolderNode(child);
        }
        ImGui::TreePop();
    }
}

void ContentBrowser::RenderContentArea() {
    switch (m_viewMode) {
        case ViewMode::Grid:
            RenderGridView();
            break;
        case ViewMode::List:
            RenderListView();
            break;
        case ViewMode::Details:
            RenderDetailsView();
            break;
        default:
            RenderGridView();
            break;
    }
}

void ContentBrowser::RenderGridView() {
    float itemSize = static_cast<float>(m_thumbnailSize + 20);
    float windowWidth = ImGui::GetContentRegionAvail().x;
    int columns = std::max(1, static_cast<int>(windowWidth / itemSize));

    int col = 0;
    for (const auto& asset : m_visibleAssets) {
        bool selected = IsSelected(asset.id);

        ImGui::PushID(asset.id.c_str());

        // Grid cell
        ImGui::BeginGroup();

        // Thumbnail
        ImVec2 thumbSize(static_cast<float>(m_thumbnailSize), static_cast<float>(m_thumbnailSize));

        // Use placeholder color based on type
        ImVec4 color;
        switch (asset.type) {
            case AssetType::Unit: color = ImVec4(0.2f, 0.6f, 0.2f, 1.0f); break;
            case AssetType::Building: color = ImVec4(0.6f, 0.4f, 0.2f, 1.0f); break;
            case AssetType::Spell: color = ImVec4(0.4f, 0.2f, 0.8f, 1.0f); break;
            case AssetType::Tile: color = ImVec4(0.3f, 0.5f, 0.3f, 1.0f); break;
            default: color = ImVec4(0.4f, 0.4f, 0.4f, 1.0f); break;
        }

        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
        }

        if (ImGui::ColorButton("##thumb", color, 0, thumbSize)) {
            bool ctrl = ImGui::GetIO().KeyCtrl;
            bool shift = ImGui::GetIO().KeyShift;
            Select(asset.id, ctrl, shift);
        }

        if (selected) {
            ImGui::PopStyleColor();
        }

        // Double-click to open
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            OpenSelected();
            if (OnAssetDoubleClicked) {
                OnAssetDoubleClicked(asset.id);
            }
        }

        // Context menu
        if (ImGui::IsItemClicked(1)) {
            ShowAssetContextMenu(asset.id, static_cast<int>(ImGui::GetMousePos().x),
                                 static_cast<int>(ImGui::GetMousePos().y));
        }

        // Drag source
        if (ImGui::BeginDragDropSource()) {
            if (!IsSelected(asset.id)) {
                Select(asset.id);
            }
            BeginDrag(m_selection.assetIds);

            ImGui::SetDragDropPayload("ASSET", asset.id.c_str(), asset.id.size() + 1);
            ImGui::Text("Move %zu item(s)", m_selection.Count());
            ImGui::EndDragDropSource();
        }

        // Name label
        std::string displayName = asset.name;
        if (displayName.length() > 12) {
            displayName = displayName.substr(0, 10) + "...";
        }
        ImGui::TextWrapped("%s", displayName.c_str());

        ImGui::EndGroup();

        ImGui::PopID();

        // Grid layout
        col++;
        if (col < columns) {
            ImGui::SameLine();
        } else {
            col = 0;
        }
    }

    // Handle background click
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered()) {
        ClearSelection();
    }

    // Background context menu
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(1) && !ImGui::IsAnyItemHovered()) {
        ShowBackgroundContextMenu(static_cast<int>(ImGui::GetMousePos().x),
                                  static_cast<int>(ImGui::GetMousePos().y));
    }
}

void ContentBrowser::RenderListView() {
    // Column headers
    ImGui::Columns(4, "AssetColumns");
    ImGui::Text("Name"); ImGui::NextColumn();
    ImGui::Text("Type"); ImGui::NextColumn();
    ImGui::Text("Modified"); ImGui::NextColumn();
    ImGui::Text("Size"); ImGui::NextColumn();
    ImGui::Separator();

    for (const auto& asset : m_visibleAssets) {
        bool selected = IsSelected(asset.id);

        ImGui::PushID(asset.id.c_str());

        if (ImGui::Selectable(asset.name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
            bool ctrl = ImGui::GetIO().KeyCtrl;
            bool shift = ImGui::GetIO().KeyShift;
            Select(asset.id, ctrl, shift);
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            OpenSelected();
        }

        if (ImGui::IsItemClicked(1)) {
            ShowAssetContextMenu(asset.id, static_cast<int>(ImGui::GetMousePos().x),
                                 static_cast<int>(ImGui::GetMousePos().y));
        }

        ImGui::NextColumn();

        // Type
        const char* typeName = "Unknown";
        switch (asset.type) {
            case AssetType::Unit: typeName = "Unit"; break;
            case AssetType::Building: typeName = "Building"; break;
            case AssetType::Spell: typeName = "Spell"; break;
            case AssetType::Tile: typeName = "Tile"; break;
            case AssetType::Effect: typeName = "Effect"; break;
            case AssetType::Hero: typeName = "Hero"; break;
            default: break;
        }
        ImGui::Text("%s", typeName);
        ImGui::NextColumn();

        // Modified date
        auto time = std::chrono::system_clock::to_time_t(asset.modifiedTime);
        std::tm tm = *std::localtime(&time);
        char dateBuffer[32];
        std::strftime(dateBuffer, sizeof(dateBuffer), "%Y-%m-%d %H:%M", &tm);
        ImGui::Text("%s", dateBuffer);
        ImGui::NextColumn();

        // Size
        if (asset.fileSize < 1024) {
            ImGui::Text("%zu B", asset.fileSize);
        } else if (asset.fileSize < 1024 * 1024) {
            ImGui::Text("%.1f KB", asset.fileSize / 1024.0f);
        } else {
            ImGui::Text("%.1f MB", asset.fileSize / (1024.0f * 1024.0f));
        }
        ImGui::NextColumn();

        ImGui::PopID();
    }

    ImGui::Columns(1);
}

void ContentBrowser::RenderDetailsView() {
    // Similar to ListView but with more columns
    RenderListView();
}

void ContentBrowser::RenderPreviewPanel() {
    ImGui::Text("Preview");
    ImGui::Separator();

    if (m_selection.IsEmpty()) {
        ImGui::TextDisabled("No selection");
        return;
    }

    if (m_selection.hasMultiple) {
        ImGui::Text("%zu items selected", m_selection.Count());
        return;
    }

    auto metadata = m_database->GetAssetMetadata(m_selection.primaryId);
    if (!metadata) {
        ImGui::TextDisabled("Asset not found");
        return;
    }

    // Preview thumbnail
    float previewSize = ImGui::GetContentRegionAvail().x - 20;
    ImVec4 color;
    switch (metadata->type) {
        case AssetType::Unit: color = ImVec4(0.2f, 0.6f, 0.2f, 1.0f); break;
        case AssetType::Building: color = ImVec4(0.6f, 0.4f, 0.2f, 1.0f); break;
        case AssetType::Spell: color = ImVec4(0.4f, 0.2f, 0.8f, 1.0f); break;
        case AssetType::Tile: color = ImVec4(0.3f, 0.5f, 0.3f, 1.0f); break;
        default: color = ImVec4(0.4f, 0.4f, 0.4f, 1.0f); break;
    }
    ImGui::ColorButton("##preview", color, 0, ImVec2(previewSize, previewSize));

    ImGui::Separator();

    // Asset info
    ImGui::Text("Name: %s", metadata->name.c_str());
    ImGui::Text("ID: %s", metadata->id.c_str());

    const char* typeName = "Unknown";
    switch (metadata->type) {
        case AssetType::Unit: typeName = "Unit"; break;
        case AssetType::Building: typeName = "Building"; break;
        case AssetType::Spell: typeName = "Spell"; break;
        case AssetType::Tile: typeName = "Tile"; break;
        case AssetType::Effect: typeName = "Effect"; break;
        case AssetType::Hero: typeName = "Hero"; break;
        default: break;
    }
    ImGui::Text("Type: %s", typeName);

    ImGui::Separator();

    // Tags
    if (!metadata->tags.empty()) {
        ImGui::Text("Tags:");
        for (const auto& tag : metadata->tags) {
            ImGui::SameLine();
            ImGui::SmallButton(tag.c_str());
        }
    }

    ImGui::Separator();

    // Actions
    if (ImGui::Button("Open", ImVec2(-1, 0))) {
        OpenSelected();
    }
    if (ImGui::Button("Duplicate", ImVec2(-1, 0))) {
        DuplicateSelected();
    }
    if (ImGui::Button("Delete", ImVec2(-1, 0))) {
        DeleteSelected();
    }
}

void ContentBrowser::RenderStatusBar() {
    ImGui::Separator();
    ImGui::Text("%zu items | %zu selected", m_visibleAssets.size(), m_selection.Count());
}

void ContentBrowser::RenderContextMenu() {
    if (!m_contextMenuOpen) {
        return;
    }

    ImGui::OpenPopup("ContentContextMenu");
    m_contextMenuOpen = false;

    if (ImGui::BeginPopup("ContentContextMenu")) {
        auto actions = GetContextMenuActions();

        for (const auto& action : actions) {
            if (action.separator) {
                ImGui::Separator();
                continue;
            }

            if (!action.submenu.empty()) {
                if (ImGui::BeginMenu(action.label.c_str())) {
                    for (const auto& subAction : action.submenu) {
                        if (ImGui::MenuItem(subAction.label.c_str(), nullptr, false, subAction.enabled)) {
                            // Handle submenu action
                            if (subAction.id == "create_unit") CreateAsset(AssetType::Unit);
                            else if (subAction.id == "create_building") CreateAsset(AssetType::Building);
                            else if (subAction.id == "create_spell") CreateAsset(AssetType::Spell);
                            else if (subAction.id == "create_tile") CreateAsset(AssetType::Tile);
                            else if (subAction.id == "create_effect") CreateAsset(AssetType::Effect);
                        }
                    }
                    ImGui::EndMenu();
                }
            } else {
                if (ImGui::MenuItem(action.label.c_str(), action.shortcut.c_str(), false, action.enabled)) {
                    // Handle action
                    if (action.id == "open") OpenSelected();
                    else if (action.id == "cut") CutSelected();
                    else if (action.id == "copy") CopySelected();
                    else if (action.id == "paste") Paste();
                    else if (action.id == "duplicate") DuplicateSelected();
                    else if (action.id == "rename") {
                        m_renameDialogName = m_database->GetAssetMetadata(m_selection.primaryId)->name;
                        m_showRenameDialog = true;
                    }
                    else if (action.id == "delete") DeleteSelected();
                    else if (action.id == "bookmark") ToggleBookmark(m_selection.primaryId);
                    else if (action.id == "show_in_explorer") ShowInExplorer(m_selection.primaryId);
                    else if (action.id == "refresh") Refresh();
                    else if (action.id == "select_all") SelectAll();
                }
            }
        }

        ImGui::EndPopup();
    }
}

void ContentBrowser::RenderCreateDialog() {
    if (!m_showCreateDialog) {
        return;
    }

    ImGui::OpenPopup("Create Asset");

    if (ImGui::BeginPopupModal("Create Asset", &m_showCreateDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        // Name input
        char nameBuffer[256] = {0};
        strncpy(nameBuffer, m_createDialogName.c_str(), sizeof(nameBuffer) - 1);

        ImGui::Text("Name:");
        if (ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer))) {
            m_createDialogName = nameBuffer;
        }

        ImGui::Separator();

        if (ImGui::Button("Create", ImVec2(120, 0))) {
            CreateOptions options;
            options.type = m_createDialogType;
            options.name = m_createDialogName;
            options.targetFolder = m_currentFolder;

            m_actions->CreateAsset(options);
            m_needsRefresh = true;
            m_showCreateDialog = false;
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showCreateDialog = false;
        }

        ImGui::EndPopup();
    }
}

void ContentBrowser::RenderRenameDialog() {
    if (!m_showRenameDialog) {
        return;
    }

    ImGui::OpenPopup("Rename Asset");

    if (ImGui::BeginPopupModal("Rename Asset", &m_showRenameDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        char nameBuffer[256] = {0};
        strncpy(nameBuffer, m_renameDialogName.c_str(), sizeof(nameBuffer) - 1);

        ImGui::Text("New name:");
        if (ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer))) {
            m_renameDialogName = nameBuffer;
        }

        ImGui::Separator();

        if (ImGui::Button("Rename", ImVec2(120, 0))) {
            RenameSelected(m_renameDialogName);
            m_showRenameDialog = false;
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showRenameDialog = false;
        }

        ImGui::EndPopup();
    }
}

// =============================================================================
// Private - Folder Tree
// =============================================================================

void ContentBrowser::BuildFolderTree() {
    m_rootFolder = CreateFolderNode(m_configsPath);
    UpdateFolderCounts();
}

void ContentBrowser::UpdateFolderCounts() {
    // Count assets in each folder recursively
    std::function<void(FolderNode&)> updateCounts = [&](FolderNode& node) {
        node.assetCount = 0;

        try {
            for (const auto& entry : fs::directory_iterator(node.path)) {
                if (entry.is_regular_file() && entry.path().extension() == ".json") {
                    node.assetCount++;
                }
            }
        } catch (...) {}

        for (auto& child : node.children) {
            updateCounts(child);
        }
    };

    updateCounts(m_rootFolder);
}

FolderNode ContentBrowser::CreateFolderNode(const std::string& path) {
    FolderNode node;
    node.path = path;
    node.name = fs::path(path).filename().string();
    if (node.name.empty()) {
        node.name = "Root";
    }

    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.is_directory()) {
                node.children.push_back(CreateFolderNode(entry.path().string()));
            }
        }
    } catch (...) {}

    // Sort children by name
    std::sort(node.children.begin(), node.children.end(),
        [](const FolderNode& a, const FolderNode& b) {
            return a.name < b.name;
        });

    return node;
}

// =============================================================================
// Private - Content Fetching
// =============================================================================

std::vector<AssetMetadata> ContentBrowser::GetVisibleAssets() {
    return m_visibleAssets;
}

void ContentBrowser::UpdateVisibleAssets() {
    // Get all assets in current folder
    auto allAssets = m_database->GetAllAssets();

    std::vector<AssetMetadata> folderAssets;
    for (const auto& asset : allAssets) {
        fs::path assetPath(asset.path);
        if (assetPath.parent_path().string() == m_currentFolder) {
            folderAssets.push_back(asset);
        }
    }

    // Apply filter
    FilterConfig config = m_filter->GetConfig();
    m_visibleAssets = m_filter->Filter(folderAssets, config);
}

void ContentBrowser::UpdateSelectionRange(const std::string& fromId, const std::string& toId) {
    // Find indices
    int fromIndex = -1, toIndex = -1;

    for (size_t i = 0; i < m_visibleAssets.size(); ++i) {
        if (m_visibleAssets[i].id == fromId) fromIndex = static_cast<int>(i);
        if (m_visibleAssets[i].id == toId) toIndex = static_cast<int>(i);
    }

    if (fromIndex < 0 || toIndex < 0) {
        return;
    }

    // Select range
    int start = std::min(fromIndex, toIndex);
    int end = std::max(fromIndex, toIndex);

    m_selection.assetIds.clear();
    for (int i = start; i <= end; ++i) {
        m_selection.assetIds.push_back(m_visibleAssets[i].id);
    }
    m_selection.hasMultiple = m_selection.assetIds.size() > 1;
}

std::string ContentBrowser::GetLastVisibleAsset() const {
    if (m_visibleAssets.empty()) return "";
    return m_visibleAssets.back().id;
}

std::string ContentBrowser::GetFirstVisibleAsset() const {
    if (m_visibleAssets.empty()) return "";
    return m_visibleAssets.front().id;
}

// =============================================================================
// Private - Keyboard Navigation
// =============================================================================

void ContentBrowser::HandleKeyboardInput() {
    if (!ImGui::IsWindowFocused()) {
        return;
    }

    // Delete
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        DeleteSelected();
    }

    // F2 - Rename
    if (ImGui::IsKeyPressed(ImGuiKey_F2) && m_selection.Count() == 1) {
        auto metadata = m_database->GetAssetMetadata(m_selection.primaryId);
        if (metadata) {
            m_renameDialogName = metadata->name;
            m_showRenameDialog = true;
        }
    }

    // F5 - Refresh
    if (ImGui::IsKeyPressed(ImGuiKey_F5)) {
        Refresh();
    }

    // Enter - Open
    if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        OpenSelected();
    }

    // Ctrl+A - Select All
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
        SelectAll();
    }

    // Ctrl+C - Copy
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
        CopySelected();
    }

    // Ctrl+X - Cut
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_X)) {
        CutSelected();
    }

    // Ctrl+V - Paste
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V)) {
        Paste();
    }

    // Ctrl+D - Duplicate
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
        DuplicateSelected();
    }

    // Arrow keys for navigation
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
        SelectNext();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
        SelectPrevious();
    }

    // Backspace - Go up
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
        NavigateUp();
    }
}

void ContentBrowser::SelectNext() {
    if (m_visibleAssets.empty()) return;

    if (m_selection.IsEmpty()) {
        Select(m_visibleAssets.front().id);
        return;
    }

    // Find current index
    for (size_t i = 0; i < m_visibleAssets.size(); ++i) {
        if (m_visibleAssets[i].id == m_selection.primaryId) {
            if (i + 1 < m_visibleAssets.size()) {
                Select(m_visibleAssets[i + 1].id);
            }
            return;
        }
    }
}

void ContentBrowser::SelectPrevious() {
    if (m_visibleAssets.empty()) return;

    if (m_selection.IsEmpty()) {
        Select(m_visibleAssets.back().id);
        return;
    }

    // Find current index
    for (size_t i = 0; i < m_visibleAssets.size(); ++i) {
        if (m_visibleAssets[i].id == m_selection.primaryId) {
            if (i > 0) {
                Select(m_visibleAssets[i - 1].id);
            }
            return;
        }
    }
}

void ContentBrowser::SelectNextRow() {
    // For grid view - calculate columns and jump by that amount
    float itemSize = static_cast<float>(m_thumbnailSize + 20);
    float windowWidth = ImGui::GetContentRegionAvail().x;
    int columns = std::max(1, static_cast<int>(windowWidth / itemSize));

    if (m_visibleAssets.empty()) return;

    for (size_t i = 0; i < m_visibleAssets.size(); ++i) {
        if (m_visibleAssets[i].id == m_selection.primaryId) {
            size_t newIndex = std::min(i + columns, m_visibleAssets.size() - 1);
            Select(m_visibleAssets[newIndex].id);
            return;
        }
    }
}

void ContentBrowser::SelectPreviousRow() {
    float itemSize = static_cast<float>(m_thumbnailSize + 20);
    float windowWidth = ImGui::GetContentRegionAvail().x;
    int columns = std::max(1, static_cast<int>(windowWidth / itemSize));

    if (m_visibleAssets.empty()) return;

    for (size_t i = 0; i < m_visibleAssets.size(); ++i) {
        if (m_visibleAssets[i].id == m_selection.primaryId) {
            size_t newIndex = (i >= static_cast<size_t>(columns)) ? i - columns : 0;
            Select(m_visibleAssets[newIndex].id);
            return;
        }
    }
}

// =============================================================================
// Private - JS Bridge
// =============================================================================

void ContentBrowser::SetupJSBridge() {
    // JS Bridge setup would be done here
    // This integrates with the HTML content browser
}

void ContentBrowser::RegisterBridgeFunctions() {
    // Register functions that can be called from JavaScript
}

// =============================================================================
// Private - Event Handling
// =============================================================================

void ContentBrowser::HandleAssetCreated(const std::string& assetId) {
    m_needsRefresh = true;
    UpdateFolderCounts();
}

void ContentBrowser::HandleAssetDeleted(const std::string& assetId) {
    // Remove from selection if selected
    auto it = std::find(m_selection.assetIds.begin(), m_selection.assetIds.end(), assetId);
    if (it != m_selection.assetIds.end()) {
        m_selection.assetIds.erase(it);
        if (m_selection.primaryId == assetId) {
            m_selection.primaryId = m_selection.assetIds.empty() ? "" : m_selection.assetIds.back();
        }
    }

    // Remove from bookmarks
    RemoveBookmark(assetId);

    // Remove from recent
    m_recentFiles.erase(
        std::remove_if(m_recentFiles.begin(), m_recentFiles.end(),
            [&](const RecentEntry& e) { return e.assetId == assetId; }),
        m_recentFiles.end()
    );

    m_needsRefresh = true;
    UpdateFolderCounts();
}

void ContentBrowser::HandleAssetModified(const std::string& assetId) {
    m_needsRefresh = true;
}

} // namespace Content
} // namespace Vehement
