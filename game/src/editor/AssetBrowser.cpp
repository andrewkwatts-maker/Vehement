/**
 * @file AssetBrowser.cpp
 * @brief Asset browser panel implementation for Vehement editor
 *
 * Features:
 * - Double-click to open assets
 * - Right-click context menu with Open, Rename, Delete, Show in Explorer, Copy Path
 * - Drag-drop with ASSET_PATH payload for viewport drop targets
 * - Search/filter with case-insensitive matching
 * - Breadcrumb navigation and folder icons
 */

#include "AssetBrowser.hpp"
#include "Editor.hpp"
#include <imgui.h>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace fs = std::filesystem;

namespace Vehement {

AssetBrowser::AssetBrowser(Editor* editor) : m_editor(editor) {
    m_currentPath = m_rootPath;
    std::memset(m_searchBuffer, 0, sizeof(m_searchBuffer));
    std::memset(m_renameBuffer, 0, sizeof(m_renameBuffer));
    Refresh();
}

AssetBrowser::~AssetBrowser() = default;

void AssetBrowser::Render() {
    if (!ImGui::Begin("Asset Browser")) {
        ImGui::End();
        return;
    }

    RenderToolbar();
    RenderBreadcrumbs();
    ImGui::Separator();

    // Left: directory tree, Right: file grid/preview
    ImGui::BeginChild("DirTree", ImVec2(200, 0), true);
    RenderDirectoryTree();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("FileArea", ImVec2(0, 0), true);
    RenderFileGrid();
    ImGui::EndChild();

    // Render popups
    RenderRenamePopup();
    RenderDeleteConfirmation();

    ImGui::End();
}

void AssetBrowser::RenderToolbar() {
    // Back button
    bool canGoBack = !m_directoryStack.empty();
    if (!canGoBack) ImGui::BeginDisabled();
    if (ImGui::Button("<-")) {
        NavigateBack();
    }
    if (!canGoBack) ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Back");

    ImGui::SameLine();

    // Up button
    bool canGoUp = (m_currentPath != m_rootPath);
    if (!canGoUp) ImGui::BeginDisabled();
    if (ImGui::Button("^")) {
        NavigateUp();
    }
    if (!canGoUp) ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Up one folder");

    ImGui::SameLine();

    // Refresh button
    if (ImGui::Button("Refresh")) {
        Refresh();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Refresh (F5)");

    ImGui::SameLine();

    // Home button
    if (ImGui::Button("Home")) {
        m_directoryStack.clear();
        m_currentPath = m_rootPath;
        Refresh();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Go to root folder");

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // View options
    ImGui::Checkbox("Grid", &m_showGrid);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::SliderInt("Size", &m_thumbnailSize, 32, 128);

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // Search filter
    ImGui::SetNextItemWidth(200);
    if (ImGui::InputTextWithHint("##search", "Search assets...", m_searchBuffer, sizeof(m_searchBuffer))) {
        m_searchFilter = m_searchBuffer;
        // Convert to lowercase for case-insensitive matching
        std::transform(m_searchFilter.begin(), m_searchFilter.end(), m_searchFilter.begin(),
                       [](unsigned char c) { return std::tolower(c); });
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Filter assets by name (case-insensitive)");

    // Clear search button
    if (!m_searchFilter.empty()) {
        ImGui::SameLine();
        if (ImGui::Button("X##clearSearch")) {
            m_searchFilter.clear();
            std::memset(m_searchBuffer, 0, sizeof(m_searchBuffer));
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear search");
    }
}

void AssetBrowser::RenderBreadcrumbs() {
    // Build breadcrumb segments from root to current path
    std::vector<std::pair<std::string, std::string>> segments; // name, path

    fs::path current(m_currentPath);
    fs::path root(m_rootPath);

    // Collect path segments
    while (current.string().length() >= root.string().length()) {
        segments.push_back({current.filename().string(), current.string()});
        if (current == root || current == current.parent_path()) {
            break;
        }
        current = current.parent_path();
    }

    // Reverse to get root-to-current order
    std::reverse(segments.begin(), segments.end());

    // Render breadcrumb buttons
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));

    for (size_t i = 0; i < segments.size(); i++) {
        if (i > 0) {
            ImGui::SameLine(0, 2);
            ImGui::TextDisabled(">");
            ImGui::SameLine(0, 2);
        }

        bool isLast = (i == segments.size() - 1);

        // Folder icon
        ImGui::SameLine(0, 0);
        if (isLast) {
            ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.3f, 1.0f), "[F]"); // Folder icon placeholder
        } else {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[F]");
        }
        ImGui::SameLine(0, 4);

        if (isLast) {
            // Current folder - just text
            ImGui::TextUnformatted(segments[i].first.c_str());
        } else {
            // Clickable folder
            if (ImGui::Button(segments[i].first.c_str())) {
                NavigateTo(segments[i].second);
            }
        }
    }

    ImGui::PopStyleColor(2);
}

void AssetBrowser::RenderDirectoryTree() {
    // Root
    ImGuiTreeNodeFlags rootFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;

    // Use folder icon
    ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.3f, 1.0f), "[F]");
    ImGui::SameLine();

    if (ImGui::TreeNodeEx("assets", rootFlags)) {
        // Scan for subdirectories dynamically
        try {
            for (const auto& entry : fs::directory_iterator(m_rootPath)) {
                if (entry.is_directory()) {
                    std::string dirName = entry.path().filename().string();
                    std::string fullPath = entry.path().string();

                    // Skip hidden directories
                    if (dirName[0] == '.') continue;

                    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    if (m_currentPath == fullPath) flags |= ImGuiTreeNodeFlags_Selected;

                    // Folder icon
                    ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.3f, 1.0f), "[F]");
                    ImGui::SameLine();

                    ImGui::TreeNodeEx(dirName.c_str(), flags);
                    if (ImGui::IsItemClicked()) {
                        NavigateTo(fullPath);
                    }

                    // Context menu for folders in tree
                    if (ImGui::BeginPopupContextItem()) {
                        if (ImGui::MenuItem("Open")) {
                            NavigateTo(fullPath);
                        }
                        ImGui::Separator();
                        if (ImGui::MenuItem("Show in Explorer")) {
                            ShowInExplorer(fullPath);
                        }
                        if (ImGui::MenuItem("Copy Path")) {
                            CopyPathToClipboard(fullPath);
                        }
                        ImGui::EndPopup();
                    }
                }
            }
        } catch (const std::exception&) {
            // Directory access error
        }
        ImGui::TreePop();
    }
}

void AssetBrowser::RenderFileGrid() {
    if (m_showGrid) {
        // Grid view
        int columns = std::max(1, (int)(ImGui::GetContentRegionAvail().x / (m_thumbnailSize + 20)));
        int col = 0;

        for (const auto& file : m_currentFiles) {
            // Apply search filter
            if (!MatchesFilter(file.name)) {
                continue;
            }

            ImGui::BeginGroup();
            ImGui::PushID(file.path.c_str());

            // Thumbnail placeholder
            ImVec2 thumbSize((float)m_thumbnailSize, (float)m_thumbnailSize);
            bool selected = (file.path == m_selectedFile);

            // Color by type
            ImVec4 bgColor;
            if (selected) {
                bgColor = ImVec4(0.3f, 0.5f, 0.8f, 1.0f);
            } else if (file.isDirectory) {
                bgColor = ImVec4(0.3f, 0.3f, 0.4f, 1.0f);
            } else {
                bgColor = ImVec4(0.2f, 0.2f, 0.25f, 1.0f);
            }

            ImGui::PushStyleColor(ImGuiCol_Button, bgColor);
            if (ImGui::Button("##thumb", thumbSize)) {
                m_selectedFile = file.path;
                if (OnAssetSelected) OnAssetSelected(file.path);
            }
            ImGui::PopStyleColor();

            // Double-click to open
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                OpenAsset(file.path);
            }

            // Right-click context menu
            std::string menuId = "ctx_" + file.path;
            if (ImGui::BeginPopupContextItem(menuId.c_str())) {
                RenderContextMenu(file.path, file.name, file.isDirectory);
                ImGui::EndPopup();
            }

            // Drag source for viewport drop
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("ASSET_PATH", file.path.c_str(), file.path.size() + 1);
                ImGui::Text("Drop to add: %s", file.name.c_str());
                ImGui::EndDragDropSource();
            }

            // File name with folder icon for directories
            if (file.isDirectory) {
                ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.3f, 1.0f), "[F]");
                ImGui::SameLine(0, 2);
            }
            ImGui::TextWrapped("%s", file.name.c_str());

            ImGui::PopID();
            ImGui::EndGroup();

            col++;
            if (col < columns) {
                ImGui::SameLine();
            } else {
                col = 0;
            }
        }
    } else {
        // List view
        if (ImGui::BeginTable("FileList", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Sortable)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort);
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Size");
            ImGui::TableHeadersRow();

            for (const auto& file : m_currentFiles) {
                // Apply search filter
                if (!MatchesFilter(file.name)) {
                    continue;
                }

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                bool selected = (file.path == m_selectedFile);

                // Folder icon
                if (file.isDirectory) {
                    ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.3f, 1.0f), "[F]");
                    ImGui::SameLine();
                }

                if (ImGui::Selectable(file.name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                    m_selectedFile = file.path;
                    if (OnAssetSelected) OnAssetSelected(file.path);
                }

                // Double-click to open
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    OpenAsset(file.path);
                }

                // Right-click context menu
                std::string menuId = "ctx_list_" + file.path;
                if (ImGui::BeginPopupContextItem(menuId.c_str())) {
                    RenderContextMenu(file.path, file.name, file.isDirectory);
                    ImGui::EndPopup();
                }

                // Drag source
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                    ImGui::SetDragDropPayload("ASSET_PATH", file.path.c_str(), file.path.size() + 1);
                    ImGui::Text("Drop to add: %s", file.name.c_str());
                    ImGui::EndDragDropSource();
                }

                ImGui::TableNextColumn();
                ImGui::Text("%s", file.isDirectory ? "Folder" : file.extension.c_str());

                ImGui::TableNextColumn();
                if (!file.isDirectory) {
                    ImGui::Text("%zu KB", file.size / 1024);
                }
            }
            ImGui::EndTable();
        }
    }

    // Empty area context menu for creating new folders etc.
    if (ImGui::BeginPopupContextWindow("EmptyAreaContext", ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
        if (ImGui::MenuItem("New Folder")) {
            std::memset(m_newFolderBuffer, 0, sizeof(m_newFolderBuffer));
            m_showNewFolderPopup = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Refresh")) {
            Refresh();
        }
        if (ImGui::MenuItem("Show in Explorer")) {
            ShowInExplorer(m_currentPath);
        }
        ImGui::EndPopup();
    }

    // New folder popup
    if (m_showNewFolderPopup) {
        ImGui::OpenPopup("New Folder");
        m_showNewFolderPopup = false;
    }

    if (ImGui::BeginPopupModal("New Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter folder name:");
        ImGui::Spacing();

        ImGui::SetNextItemWidth(300);
        bool submitted = ImGui::InputText("##newfolder", m_newFolderBuffer, sizeof(m_newFolderBuffer),
                                          ImGuiInputTextFlags_EnterReturnsTrue);

        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere(-1);
        }

        ImGui::Spacing();

        if (ImGui::Button("Create", ImVec2(120, 0)) || submitted) {
            std::string folderName = m_newFolderBuffer;
            if (!folderName.empty()) {
                try {
                    fs::path newFolderPath = fs::path(m_currentPath) / folderName;
                    if (!fs::exists(newFolderPath)) {
                        fs::create_directory(newFolderPath);
                        Refresh();
                    }
                } catch (const std::exception&) {
                    // Handle folder creation error
                }
            }
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void AssetBrowser::RenderContextMenu(const std::string& assetPath, const std::string& filename, bool isDirectory) {
    if (ImGui::MenuItem("Open")) {
        OpenAsset(assetPath);
    }

    if (ImGui::MenuItem("Rename")) {
        StartRename(assetPath);
    }

    if (ImGui::MenuItem("Delete")) {
        DeleteAsset(assetPath);
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Show in Explorer")) {
        ShowInExplorer(assetPath);
    }

    if (ImGui::MenuItem("Copy Path")) {
        CopyPathToClipboard(assetPath);
    }
}

void AssetBrowser::RenderRenamePopup() {
    if (m_showRenamePopup) {
        ImGui::OpenPopup("Rename Asset");
        m_showRenamePopup = false;
    }

    if (ImGui::BeginPopupModal("Rename Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter new name:");
        ImGui::Spacing();

        ImGui::SetNextItemWidth(300);
        bool submitted = ImGui::InputText("##rename", m_renameBuffer, sizeof(m_renameBuffer),
                                          ImGuiInputTextFlags_EnterReturnsTrue);

        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere(-1);
        }

        ImGui::Spacing();

        if (ImGui::Button("Rename", ImVec2(120, 0)) || submitted) {
            try {
                std::string newName = m_renameBuffer;
                if (!newName.empty()) {
                    fs::path oldPath(m_renamingPath);
                    fs::path newPath = oldPath.parent_path() / newName;

                    if (oldPath != newPath && !fs::exists(newPath)) {
                        fs::rename(oldPath, newPath);

                        if (OnAssetRenamed) {
                            OnAssetRenamed(m_renamingPath, newPath.string());
                        }

                        Refresh();
                    }
                }
            } catch (const std::exception&) {
                // Handle rename error
            }

            m_renamingPath.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_renamingPath.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void AssetBrowser::RenderDeleteConfirmation() {
    if (m_showDeleteConfirmation) {
        ImGui::OpenPopup("Confirm Delete");
        m_showDeleteConfirmation = false;
    }

    if (ImGui::BeginPopupModal("Confirm Delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        fs::path deletePath(m_pendingDeletePath);
        ImGui::Text("Are you sure you want to delete '%s'?", deletePath.filename().string().c_str());
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "This action cannot be undone!");
        ImGui::Spacing();

        if (ImGui::Button("Delete", ImVec2(120, 0))) {
            try {
                fs::remove_all(m_pendingDeletePath);

                if (OnAssetDeleted) {
                    OnAssetDeleted(m_pendingDeletePath);
                }

                // Clear selection if deleted item was selected
                if (m_selectedFile == m_pendingDeletePath) {
                    m_selectedFile.clear();
                }

                Refresh();
            } catch (const std::exception&) {
                // Handle delete error
            }

            m_pendingDeletePath.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_pendingDeletePath.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void AssetBrowser::SetRootPath(const std::string& path) {
    m_rootPath = path;
    m_currentPath = path;
    m_directoryStack.clear();
    Refresh();
}

void AssetBrowser::Refresh() {
    m_currentFiles.clear();

    try {
        for (const auto& entry : fs::directory_iterator(m_currentPath)) {
            FileEntry file;
            file.name = entry.path().filename().string();
            file.path = entry.path().string();
            file.isDirectory = entry.is_directory();

            // Skip hidden files
            if (file.name[0] == '.') continue;

            if (!file.isDirectory) {
                file.extension = entry.path().extension().string();
                file.size = entry.file_size();
            } else {
                file.size = 0;
            }

            m_currentFiles.push_back(file);
        }
    } catch (const std::exception& e) {
        // Directory doesn't exist or access denied
    }

    // Sort: directories first, then by name
    std::sort(m_currentFiles.begin(), m_currentFiles.end(), [](const FileEntry& a, const FileEntry& b) {
        if (a.isDirectory != b.isDirectory) return a.isDirectory > b.isDirectory;
        return a.name < b.name;
    });
}

void AssetBrowser::NavigateTo(const std::string& path) {
    if (path == m_currentPath) return;

    try {
        if (fs::exists(path) && fs::is_directory(path)) {
            m_directoryStack.push_back(m_currentPath);
            m_currentPath = path;
            Refresh();
        }
    } catch (const std::exception&) {
        // Handle navigation error
    }
}

void AssetBrowser::NavigateUp() {
    if (m_currentPath == m_rootPath) return;

    try {
        fs::path parent = fs::path(m_currentPath).parent_path();
        if (parent.string().length() >= m_rootPath.length()) {
            m_directoryStack.push_back(m_currentPath);
            m_currentPath = parent.string();
            Refresh();
        }
    } catch (const std::exception&) {
        // Handle navigation error
    }
}

void AssetBrowser::NavigateBack() {
    if (m_directoryStack.empty()) return;

    m_currentPath = m_directoryStack.back();
    m_directoryStack.pop_back();
    Refresh();
}

void AssetBrowser::OpenAsset(const std::string& path) {
    try {
        if (fs::is_directory(path)) {
            NavigateTo(path);
        } else {
            if (OnAssetDoubleClicked) {
                OnAssetDoubleClicked(path);
            }
        }
    } catch (const std::exception&) {
        // Handle error
    }
}

void AssetBrowser::DeleteAsset(const std::string& path) {
    m_pendingDeletePath = path;
    m_showDeleteConfirmation = true;
}

void AssetBrowser::StartRename(const std::string& path) {
    m_renamingPath = path;

    // Extract filename (without path) for rename buffer
    fs::path fsPath(path);
    std::string filename = fsPath.filename().string();
    std::strncpy(m_renameBuffer, filename.c_str(), sizeof(m_renameBuffer) - 1);
    m_renameBuffer[sizeof(m_renameBuffer) - 1] = '\0';

    m_showRenamePopup = true;
}

void AssetBrowser::ShowInExplorer(const std::string& path) {
#ifdef _WIN32
    // Convert to wide string for Windows API
    std::wstring wpath(path.begin(), path.end());

    // Use /select to highlight the file/folder in Explorer
    std::wstring command = L"/select,\"" + wpath + L"\"";
    ShellExecuteW(nullptr, L"open", L"explorer.exe", command.c_str(), nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
    std::string command = "open -R \"" + path + "\"";
    system(command.c_str());
#else
    // Linux - open containing folder
    fs::path fsPath(path);
    std::string folder = fsPath.parent_path().string();
    std::string command = "xdg-open \"" + folder + "\"";
    system(command.c_str());
#endif
}

void AssetBrowser::CopyPathToClipboard(const std::string& path) {
    ImGui::SetClipboardText(path.c_str());
}

bool AssetBrowser::MatchesFilter(const std::string& name) const {
    if (m_searchFilter.empty()) {
        return true;
    }

    // Case-insensitive substring search
    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    return lowerName.find(m_searchFilter) != std::string::npos;
}

void AssetBrowser::ScanDirectory(const std::string& path) {
    // Recursive scan - used for search across all subdirectories
    // Not currently used, but available for future recursive search feature
}

} // namespace Vehement
