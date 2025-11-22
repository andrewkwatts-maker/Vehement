#include "AssetBrowser.hpp"
#include "Editor.hpp"
#include <imgui.h>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

namespace Vehement {

AssetBrowser::AssetBrowser(Editor* editor) : m_editor(editor) {
    m_currentPath = m_rootPath;
    Refresh();
}

AssetBrowser::~AssetBrowser() = default;

void AssetBrowser::Render() {
    if (!ImGui::Begin("Asset Browser")) {
        ImGui::End();
        return;
    }

    RenderToolbar();
    ImGui::Separator();

    // Left: directory tree, Right: file grid/preview
    ImGui::BeginChild("DirTree", ImVec2(200, 0), true);
    RenderDirectoryTree();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("FileArea", ImVec2(0, 0), true);
    RenderFileGrid();
    ImGui::EndChild();

    ImGui::End();
}

void AssetBrowser::RenderToolbar() {
    // Navigation
    if (ImGui::Button("<-") && !m_directoryStack.empty()) {
        m_currentPath = m_directoryStack.back();
        m_directoryStack.pop_back();
        Refresh();
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        Refresh();
    }
    ImGui::SameLine();
    if (ImGui::Button("Home")) {
        m_directoryStack.clear();
        m_currentPath = m_rootPath;
        Refresh();
    }

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // View options
    ImGui::Checkbox("Grid", &m_showGrid);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::SliderInt("Size", &m_thumbnailSize, 32, 128);

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // Search
    static char searchBuffer[256] = "";
    ImGui::SetNextItemWidth(150);
    if (ImGui::InputTextWithHint("##search", "Search...", searchBuffer, sizeof(searchBuffer))) {
        m_searchFilter = searchBuffer;
    }

    // Current path display
    ImGui::Text("Path: %s", m_currentPath.c_str());
}

void AssetBrowser::RenderDirectoryTree() {
    // Root
    ImGuiTreeNodeFlags rootFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
    if (ImGui::TreeNodeEx("assets", rootFlags)) {
        // Subdirectories
        const char* subdirs[] = {"configs", "models", "textures", "scripts", "shaders", "sounds", "locations"};
        for (const char* subdir : subdirs) {
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            std::string fullPath = m_rootPath + "/" + subdir;
            if (m_currentPath == fullPath) flags |= ImGuiTreeNodeFlags_Selected;

            ImGui::TreeNodeEx(subdir, flags);
            if (ImGui::IsItemClicked()) {
                if (m_currentPath != fullPath) {
                    m_directoryStack.push_back(m_currentPath);
                }
                m_currentPath = fullPath;
                Refresh();
            }
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
            // Filter
            if (!m_searchFilter.empty() && file.name.find(m_searchFilter) == std::string::npos) {
                continue;
            }

            ImGui::BeginGroup();
            ImGui::PushID(file.path.c_str());

            // Thumbnail placeholder
            ImVec2 thumbSize((float)m_thumbnailSize, (float)m_thumbnailSize);
            bool selected = (file.path == m_selectedFile);

            // Color by type
            ImVec4 bgColor = file.isDirectory ? ImVec4(0.3f, 0.3f, 0.4f, 1.0f) : ImVec4(0.2f, 0.2f, 0.25f, 1.0f);
            if (selected) bgColor = ImVec4(0.3f, 0.5f, 0.8f, 1.0f);

            ImGui::PushStyleColor(ImGuiCol_Button, bgColor);
            if (ImGui::Button("##thumb", thumbSize)) {
                m_selectedFile = file.path;
                if (OnAssetSelected) OnAssetSelected(file.path);
            }
            ImGui::PopStyleColor();

            // Double-click handling
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                if (file.isDirectory) {
                    m_directoryStack.push_back(m_currentPath);
                    m_currentPath = file.path;
                    Refresh();
                } else {
                    if (OnAssetDoubleClicked) OnAssetDoubleClicked(file.path);
                }
            }

            // Drag source
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("ASSET_PATH", file.path.c_str(), file.path.size() + 1);
                ImGui::Text("%s", file.name.c_str());
                ImGui::EndDragDropSource();
            }

            // File name
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
                if (!m_searchFilter.empty() && file.name.find(m_searchFilter) == std::string::npos) {
                    continue;
                }

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                bool selected = (file.path == m_selectedFile);
                if (ImGui::Selectable(file.name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                    m_selectedFile = file.path;
                    if (OnAssetSelected) OnAssetSelected(file.path);
                }

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    if (file.isDirectory) {
                        m_directoryStack.push_back(m_currentPath);
                        m_currentPath = file.path;
                        Refresh();
                    }
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
}

void AssetBrowser::SetRootPath(const std::string& path) {
    m_rootPath = path;
    m_currentPath = path;
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

void AssetBrowser::ScanDirectory(const std::string& path) {
    // Recursive scan - not used by default
}

} // namespace Vehement
