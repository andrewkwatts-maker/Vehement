#include "AssetBrowserEnhanced.hpp"
#include "Editor.hpp"
#include "ConfigEditor.hpp"
#include "../config/ConfigRegistry.hpp"
#include "../assets/PlaceholderGenerator.hpp"
#include <imgui.h>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace Vehement {

AssetBrowserEnhanced::AssetBrowserEnhanced(Editor* editor)
    : m_editor(editor) {
    RefreshAssets();
}

AssetBrowserEnhanced::~AssetBrowserEnhanced() {
    // Clean up OpenGL textures
    for (auto& [path, textureId] : m_thumbnailCache) {
        if (textureId != 0) {
            // glDeleteTextures(1, &textureId);  // Uncomment when OpenGL is available
        }
    }
}

void AssetBrowserEnhanced::Render() {
    if (!ImGui::Begin("Asset Browser (Enhanced)")) {
        ImGui::End();
        return;
    }

    RenderToolbar();
    ImGui::Separator();

    RenderCategoryTabs();
    RenderSearchBar();
    ImGui::Separator();

    // Split view: Assets grid on left, preview on right
    if (m_showPreview) {
        ImGui::BeginChild("AssetGridPanel", ImVec2(ImGui::GetContentRegionAvail().x * 0.6f, 0), true);
    } else {
        ImGui::BeginChild("AssetGridPanel", ImVec2(0, 0), true);
    }

    if (m_showAsGrid) {
        RenderAssetGrid();
    } else {
        RenderAssetList();
    }
    ImGui::EndChild();

    if (m_showPreview) {
        ImGui::SameLine();
        ImGui::BeginChild("PreviewPanel", ImVec2(0, 0), true);
        RenderPreviewPanel();
        ImGui::EndChild();
    }

    ImGui::End();

    // Dialogs
    if (m_showCreateDialog) {
        RenderCreateAssetDialog();
    }
}

void AssetBrowserEnhanced::Update(float deltaTime) {
    if (m_showPreview) {
        m_previewRotation += deltaTime * 30.0f;  // Slow auto-rotation
        if (m_previewRotation > 360.0f) m_previewRotation -= 360.0f;
    }
}

void AssetBrowserEnhanced::RenderToolbar() {
    if (ImGui::Button("Refresh")) {
        RefreshAssets();
    }
    ImGui::SameLine();

    if (ImGui::Button("New Asset")) {
        m_showCreateDialog = true;
    }
    ImGui::SameLine();

    if (ImGui::Button("Import")) {
        // TODO: File import dialog
        ImGui::OpenPopup("ImportAssetDialog");
    }

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // View mode toggle
    if (ImGui::RadioButton("Grid", m_showAsGrid)) {
        m_showAsGrid = true;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("List", !m_showAsGrid)) {
        m_showAsGrid = false;
    }

    ImGui::SameLine();
    if (m_showAsGrid) {
        ImGui::SetNextItemWidth(100);
        ImGui::SliderInt("Size", &m_thumbnailSize, 64, 256);
    }

    ImGui::SameLine();
    ImGui::Checkbox("Preview", &m_showPreview);

    ImGui::Text("Assets: %zu / %zu", m_filteredAssets.size(), m_allAssets.size());
}

void AssetBrowserEnhanced::RenderCategoryTabs() {
    const char* categoryNames[] = {
        "All", "Units", "Buildings", "Tiles", "Models",
        "Textures", "Scripts", "Configs", "Locations", "Spells", "Items"
    };

    for (int i = 0; i < IM_ARRAYSIZE(categoryNames); i++) {
        if (i > 0) ImGui::SameLine();

        bool selected = (static_cast<int>(m_categoryFilter) == i);
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.6f, 1.0f, 1.0f));
        }

        if (ImGui::Button(categoryNames[i], ImVec2(80, 0))) {
            SetCategoryFilter(static_cast<AssetCategory>(i));
        }

        if (selected) {
            ImGui::PopStyleColor();
        }
    }
}

void AssetBrowserEnhanced::RenderSearchBar() {
    static char searchBuffer[256] = "";
    ImGui::SetNextItemWidth(300);
    if (ImGui::InputTextWithHint("##search", "Search assets...", searchBuffer, sizeof(searchBuffer))) {
        SetSearchFilter(searchBuffer);
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        searchBuffer[0] = '\0';
        SetSearchFilter("");
    }
}

void AssetBrowserEnhanced::RenderAssetGrid() {
    int columns = std::max(1, (int)(ImGui::GetContentRegionAvail().x / (m_thumbnailSize + 20)));
    int col = 0;

    for (auto* assetPtr : m_filteredAssets) {
        if (!assetPtr) continue;
        auto& asset = *assetPtr;

        ImGui::PushID(asset.path.c_str());
        ImGui::BeginGroup();

        // Thumbnail
        ImVec2 thumbSize((float)m_thumbnailSize, (float)m_thumbnailSize);
        bool selected = (asset.path == m_selectedAssetPath);

        // Background color based on selection
        ImVec4 bgColor = selected ? ImVec4(0.3f, 0.5f, 0.8f, 1.0f) : ImVec4(0.2f, 0.2f, 0.25f, 1.0f);

        // Category color coding
        switch (asset.category) {
            case AssetCategory::Units: bgColor.b = 0.6f; break;
            case AssetCategory::Buildings: bgColor.r = 0.6f; break;
            case AssetCategory::Tiles: bgColor.g = 0.5f; break;
            default: break;
        }

        ImGui::PushStyleColor(ImGuiCol_Button, bgColor);

        if (asset.thumbnailTexture != 0) {
            // Render actual thumbnail texture
            if (ImGui::ImageButton((void*)(intptr_t)asset.thumbnailTexture, thumbSize)) {
                SelectAsset(asset.path);
            }
        } else {
            // Placeholder button
            if (ImGui::Button("##thumb", thumbSize)) {
                SelectAsset(asset.path);
            }

            // Draw icon based on category
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 pMin = ImGui::GetItemRectMin();
            ImVec2 pMax = ImGui::GetItemRectMax();
            ImVec2 center((pMin.x + pMax.x) * 0.5f, (pMin.y + pMax.y) * 0.5f);

            // Simple icon drawing
            ImU32 iconColor = IM_COL32(200, 200, 200, 255);
            switch (asset.category) {
                case AssetCategory::Units:
                    // Draw a simple person icon
                    drawList->AddCircle(center, m_thumbnailSize * 0.2f, iconColor, 16, 2.0f);
                    drawList->AddLine(ImVec2(center.x, center.y + m_thumbnailSize * 0.2f),
                                     ImVec2(center.x, center.y + m_thumbnailSize * 0.4f), iconColor, 2.0f);
                    break;
                case AssetCategory::Buildings:
                    // Draw a simple house icon
                    drawList->AddRect(ImVec2(center.x - m_thumbnailSize * 0.25f, center.y),
                                     ImVec2(center.x + m_thumbnailSize * 0.25f, center.y + m_thumbnailSize * 0.35f),
                                     iconColor, 0, 0, 2.0f);
                    break;
                case AssetCategory::Models:
                    // Draw a cube icon
                    drawList->AddRect(ImVec2(center.x - m_thumbnailSize * 0.2f, center.y - m_thumbnailSize * 0.2f),
                                     ImVec2(center.x + m_thumbnailSize * 0.2f, center.y + m_thumbnailSize * 0.2f),
                                     iconColor, 0, 0, 2.0f);
                    break;
                default:
                    break;
            }
        }

        ImGui::PopStyleColor();

        // Double-click handling
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            if (OnAssetDoubleClicked) {
                OnAssetDoubleClicked(asset.path);
            }
        }

        // Context menu
        if (ImGui::BeginPopupContextItem()) {
            RenderContextMenu();
            ImGui::EndPopup();
        }

        // Asset name
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + m_thumbnailSize);
        ImGui::TextWrapped("%s", asset.name.c_str());
        ImGui::PopTextWrapPos();

        // Asset type label
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
        ImGui::Text("%s", asset.extension.c_str());
        ImGui::PopStyleColor();

        ImGui::EndGroup();
        ImGui::PopID();

        col++;
        if (col < columns) {
            ImGui::SameLine();
        } else {
            col = 0;
        }
    }
}

void AssetBrowserEnhanced::RenderAssetList() {
    if (ImGui::BeginTable("AssetList", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                          ImGuiTableFlags_Sortable | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort);
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Category");
        ImGui::TableSetupColumn("Size");
        ImGui::TableHeadersRow();

        for (auto* assetPtr : m_filteredAssets) {
            if (!assetPtr) continue;
            auto& asset = *assetPtr;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            bool selected = (asset.path == m_selectedAssetPath);
            if (ImGui::Selectable(asset.name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                SelectAsset(asset.path);
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                if (OnAssetDoubleClicked) {
                    OnAssetDoubleClicked(asset.path);
                }
            }

            ImGui::TableNextColumn();
            ImGui::Text("%s", asset.extension.c_str());

            ImGui::TableNextColumn();
            const char* categoryNames[] = {
                "All", "Unit", "Building", "Tile", "Model", "Texture", "Script", "Config", "Location", "Spell", "Item"
            };
            ImGui::Text("%s", categoryNames[static_cast<int>(asset.category)]);

            ImGui::TableNextColumn();
            if (!asset.isDirectory) {
                ImGui::Text("%zu KB", asset.size / 1024);
            }
        }

        ImGui::EndTable();
    }
}

void AssetBrowserEnhanced::RenderPreviewPanel() {
    if (!m_selectedAsset) {
        ImGui::TextDisabled("No asset selected");
        return;
    }

    auto& asset = *m_selectedAsset;

    ImGui::Text("Preview: %s", asset.name.c_str());
    ImGui::Separator();

    // Asset info
    ImGui::Text("Path: %s", asset.path.c_str());
    ImGui::Text("Type: %s", asset.extension.c_str());
    ImGui::Text("Size: %zu KB", asset.size / 1024);

    if (!asset.tags.empty()) {
        ImGui::Text("Tags:");
        for (const auto& tag : asset.tags) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[%s]", tag.c_str());
        }
    }

    ImGui::Separator();

    // Category-specific preview
    switch (asset.category) {
        case AssetCategory::Units:
        case AssetCategory::Buildings:
        case AssetCategory::Models:
            RenderModelPreview(asset);
            break;
        case AssetCategory::Textures:
            RenderTexturePreview(asset);
            break;
        case AssetCategory::Configs:
            RenderConfigPreview(asset);
            break;
        default:
            ImGui::TextDisabled("No preview available for this asset type");
            break;
    }

    ImGui::Separator();

    // Actions
    if (ImGui::Button("Edit", ImVec2(100, 0))) {
        if (OnAssetDoubleClicked) {
            OnAssetDoubleClicked(asset.path);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Duplicate", ImVec2(100, 0))) {
        DuplicateAsset(asset.path);
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete", ImVec2(100, 0))) {
        ImGui::OpenPopup("DeleteConfirm");
    }

    // Delete confirmation
    if (ImGui::BeginPopupModal("DeleteConfirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Are you sure you want to delete:");
        ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "%s", asset.name.c_str());
        ImGui::Separator();

        if (ImGui::Button("Delete", ImVec2(120, 0))) {
            DeleteAsset(asset.path);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void AssetBrowserEnhanced::RenderModelPreview(const AssetInfo& asset) {
    ImGui::Text("3D Model Preview");

    ImGui::SliderFloat("Zoom", &m_previewZoom, 0.5f, 3.0f);
    ImGui::SliderFloat("Rotation", &m_previewRotation, 0.0f, 360.0f);

    // Preview viewport
    ImVec2 previewSize = ImVec2(ImGui::GetContentRegionAvail().x, 300);
    ImGui::BeginChild("ModelPreview", previewSize, true);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();

    // Background
    drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(30, 30, 35, 255));

    // Simple wireframe preview (placeholder - would use actual 3D rendering)
    ImVec2 center(pos.x + size.x / 2, pos.y + size.y / 2);
    float scale = 40.0f * m_previewZoom;

    float cosR = cos(glm::radians(m_previewRotation));
    float sinR = sin(glm::radians(m_previewRotation));

    auto project3D = [&](float x, float y, float z) -> ImVec2 {
        float rx = x * cosR - z * sinR;
        float rz = x * sinR + z * cosR;
        float px = center.x + (rx - rz) * scale * 0.7f;
        float py = center.y - y * scale + (rx + rz) * scale * 0.3f;
        return ImVec2(px, py);
    };

    // Draw simple cube wireframe
    ImVec2 v0 = project3D(-1, -1, -1);
    ImVec2 v1 = project3D( 1, -1, -1);
    ImVec2 v2 = project3D( 1, -1,  1);
    ImVec2 v3 = project3D(-1, -1,  1);
    ImVec2 v4 = project3D(-1,  1, -1);
    ImVec2 v5 = project3D( 1,  1, -1);
    ImVec2 v6 = project3D( 1,  1,  1);
    ImVec2 v7 = project3D(-1,  1,  1);

    ImU32 modelColor = IM_COL32(200, 200, 200, 255);

    // Draw edges
    drawList->AddLine(v0, v1, modelColor, 2.0f);
    drawList->AddLine(v1, v2, modelColor, 2.0f);
    drawList->AddLine(v2, v3, modelColor, 2.0f);
    drawList->AddLine(v3, v0, modelColor, 2.0f);
    drawList->AddLine(v4, v5, modelColor, 2.0f);
    drawList->AddLine(v5, v6, modelColor, 2.0f);
    drawList->AddLine(v6, v7, modelColor, 2.0f);
    drawList->AddLine(v7, v4, modelColor, 2.0f);
    drawList->AddLine(v0, v4, modelColor, 2.0f);
    drawList->AddLine(v1, v5, modelColor, 2.0f);
    drawList->AddLine(v2, v6, modelColor, 2.0f);
    drawList->AddLine(v3, v7, modelColor, 2.0f);

    ImGui::EndChild();

    if (ImGui::Button("Reset View")) {
        m_previewRotation = 0.0f;
        m_previewZoom = 1.0f;
    }
}

void AssetBrowserEnhanced::RenderTexturePreview(const AssetInfo& asset) {
    ImGui::Text("Texture Preview");

    // TODO: Load and display actual texture
    ImVec2 previewSize = ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().x);
    ImGui::BeginChild("TexturePreview", previewSize, true);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();

    // Checkerboard background
    drawList->AddRectFilled(pos, ImVec2(pos.x + previewSize.x, pos.y + previewSize.y),
                           IM_COL32(64, 64, 64, 255));

    ImGui::Text("Texture: %s", asset.name.c_str());

    ImGui::EndChild();
}

void AssetBrowserEnhanced::RenderConfigPreview(const AssetInfo& asset) {
    ImGui::Text("Config Preview");

    // Load and display JSON structure
    try {
        std::ifstream file(asset.path);
        if (file.is_open()) {
            json j;
            file >> j;

            ImGui::Text("Config ID: %s", j.value("id", "N/A").c_str());
            ImGui::Text("Name: %s", j.value("name", "N/A").c_str());
            ImGui::Text("Type: %s", j.value("type", "N/A").c_str());

            if (j.contains("health")) {
                ImGui::Text("Health: %.0f", j["health"].get<float>());
            }
            if (j.contains("damage")) {
                ImGui::Text("Damage: %.0f", j["damage"].get<float>());
            }
            if (j.contains("speed")) {
                ImGui::Text("Speed: %.1f", j["speed"].get<float>());
            }

            // Show tags
            if (j.contains("tags") && j["tags"].is_array()) {
                ImGui::Text("Tags:");
                for (const auto& tag : j["tags"]) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[%s]", tag.get<std::string>().c_str());
                }
            }
        }
    } catch (...) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "Error loading config");
    }
}

void AssetBrowserEnhanced::RenderContextMenu() {
    if (!m_selectedAsset) return;

    if (ImGui::MenuItem("Open")) {
        if (OnAssetDoubleClicked) {
            OnAssetDoubleClicked(m_selectedAsset->path);
        }
    }
    if (ImGui::MenuItem("Duplicate")) {
        DuplicateAsset(m_selectedAsset->path);
    }
    if (ImGui::MenuItem("Delete")) {
        DeleteAsset(m_selectedAsset->path);
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Reveal in Explorer")) {
        // Open file explorer to this location
        #ifdef _WIN32
        std::string cmd = "explorer /select,\"" + m_selectedAsset->path + "\"";
        system(cmd.c_str());
        #endif
    }
    if (ImGui::MenuItem("Copy Path")) {
        ImGui::SetClipboardText(m_selectedAsset->path.c_str());
    }
}

void AssetBrowserEnhanced::RenderCreateAssetDialog() {
    ImGui::OpenPopup("Create New Asset");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 400));

    if (ImGui::BeginPopupModal("Create New Asset", &m_showCreateDialog, ImGuiWindowFlags_NoResize)) {
        ImGui::Text("Create New Asset");
        ImGui::Separator();

        // Category selection
        const char* categoryNames[] = {"Unit", "Building", "Tile", "Spell", "Item"};
        const AssetCategory categories[] = {
            AssetCategory::Units, AssetCategory::Buildings, AssetCategory::Tiles,
            AssetCategory::Spells, AssetCategory::Items
        };

        int categoryIdx = 0;
        for (int i = 0; i < IM_ARRAYSIZE(categories); i++) {
            if (categories[i] == m_createCategory) {
                categoryIdx = i;
                break;
            }
        }

        if (ImGui::Combo("Asset Type", &categoryIdx, categoryNames, IM_ARRAYSIZE(categoryNames))) {
            m_createCategory = categories[categoryIdx];
        }

        // Asset name
        ImGui::InputText("Name", m_createNameBuffer, sizeof(m_createNameBuffer));

        // Template selection
        auto templates = GetTemplatesForCategory(m_createCategory);
        if (!templates.empty()) {
            ImGui::Text("Template:");
            for (size_t i = 0; i < templates.size(); i++) {
                if (ImGui::Selectable(templates[i].name.c_str(), m_selectedTemplateIdx == (int)i)) {
                    m_selectedTemplateIdx = (int)i;
                }
                ImGui::Indent();
                ImGui::TextDisabled("%s", templates[i].description.c_str());
                ImGui::Unindent();
            }
        }

        ImGui::Separator();

        // Create/Cancel buttons
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            std::string templateId = templates.empty() ? "" : templates[m_selectedTemplateIdx].id;
            CreateNewAsset(m_createCategory, templateId);
            m_showCreateDialog = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showCreateDialog = false;
        }

        ImGui::EndPopup();
    }
}

void AssetBrowserEnhanced::SelectAsset(const std::string& path) {
    m_selectedAssetPath = path;
    m_selectedAsset = nullptr;

    for (auto& asset : m_allAssets) {
        if (asset.path == path) {
            m_selectedAsset = &asset;
            break;
        }
    }

    if (m_selectedAsset && OnAssetSelected) {
        OnAssetSelected(path);
    }
}

void AssetBrowserEnhanced::SetCategoryFilter(AssetCategory category) {
    m_categoryFilter = category;

    // Rebuild filtered list
    m_filteredAssets.clear();
    for (auto& asset : m_allAssets) {
        if (m_categoryFilter == AssetCategory::All || asset.category == m_categoryFilter) {
            if (m_searchFilter.empty() || asset.name.find(m_searchFilter) != std::string::npos) {
                m_filteredAssets.push_back(&asset);
            }
        }
    }
}

void AssetBrowserEnhanced::SetSearchFilter(const std::string& filter) {
    m_searchFilter = filter;
    SetCategoryFilter(m_categoryFilter);  // Refresh filtered list
}

void AssetBrowserEnhanced::SetTagFilter(const std::vector<std::string>& tags) {
    m_tagFilter = tags;
    SetCategoryFilter(m_categoryFilter);  // Refresh filtered list
}

void AssetBrowserEnhanced::CreateNewAsset(AssetCategory category, const std::string& templateId) {
    std::string name = m_createNameBuffer;
    if (name.empty()) {
        name = "new_asset";
    }

    // Determine file path based on category
    std::string subdir;
    std::string ext = ".json";
    switch (category) {
        case AssetCategory::Units: subdir = "configs/units"; break;
        case AssetCategory::Buildings: subdir = "configs/buildings"; break;
        case AssetCategory::Tiles: subdir = "configs/tiles"; break;
        case AssetCategory::Spells: subdir = "configs/spells"; break;
        case AssetCategory::Items: subdir = "configs/items"; break;
        default: subdir = "configs"; break;
    }

    std::string path = "game/assets/" + subdir + "/" + name + ext;

    // Create JSON from template
    json assetJson;
    assetJson["id"] = name;
    assetJson["name"] = name;
    assetJson["type"] = subdir.substr(subdir.find('/') + 1);  // Extract type from subdir
    assetJson["tags"] = json::array();

    // Apply template-specific defaults
    // (In a full implementation, load from template files)

    // Create directory if needed
    fs::create_directories(fs::path(path).parent_path());

    // Write file
    std::ofstream file(path);
    if (file.is_open()) {
        file << assetJson.dump(2);
        file.close();

        // Refresh and select new asset
        RefreshAssets();
        SelectAsset(path);

        if (OnAssetCreated) {
            OnAssetCreated(path, category);
        }
    }
}

void AssetBrowserEnhanced::DuplicateAsset(const std::string& path) {
    // TODO: Implement duplication
}

void AssetBrowserEnhanced::DeleteAsset(const std::string& path) {
    try {
        fs::remove(path);
        RefreshAssets();

        if (OnAssetDeleted) {
            OnAssetDeleted(path);
        }
    } catch (...) {
        // Error handling
    }
}

void AssetBrowserEnhanced::RenameAsset(const std::string& oldPath, const std::string& newPath) {
    try {
        fs::rename(oldPath, newPath);
        RefreshAssets();
    } catch (...) {
        // Error handling
    }
}

void AssetBrowserEnhanced::RefreshAssets() {
    m_allAssets.clear();
    m_filteredAssets.clear();

    // Scan all asset directories
    ScanDirectory("game/assets/configs/units", AssetCategory::Units);
    ScanDirectory("game/assets/configs/buildings", AssetCategory::Buildings);
    ScanDirectory("game/assets/configs/tiles", AssetCategory::Tiles);
    ScanDirectory("game/assets/models", AssetCategory::Models);
    ScanDirectory("game/assets/textures", AssetCategory::Textures);
    ScanDirectory("game/assets/scripts", AssetCategory::Scripts);

    // Apply current filter
    SetCategoryFilter(m_categoryFilter);
}

void AssetBrowserEnhanced::ScanDirectory(const std::string& path, AssetCategory category) {
    if (!fs::exists(path)) return;

    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            AssetInfo asset;
            asset.path = entry.path().string();
            asset.name = entry.path().stem().string();
            asset.extension = entry.path().extension().string();
            asset.isDirectory = entry.is_directory();
            asset.category = category;
            asset.thumbnailTexture = 0;

            if (!asset.isDirectory) {
                asset.size = entry.file_size();

                // Load tags from JSON if it's a config file
                if (asset.extension == ".json") {
                    try {
                        std::ifstream file(asset.path);
                        json j;
                        file >> j;
                        if (j.contains("tags") && j["tags"].is_array()) {
                            for (const auto& tag : j["tags"]) {
                                asset.tags.push_back(tag.get<std::string>());
                            }
                        }
                    } catch (...) {}
                }

                m_allAssets.push_back(asset);
            }
        }
    } catch (...) {}
}

AssetBrowserEnhanced::AssetCategory AssetBrowserEnhanced::DetermineCategory(
    const std::string& path, const std::string& ext) {

    if (path.find("/units/") != std::string::npos) return AssetCategory::Units;
    if (path.find("/buildings/") != std::string::npos) return AssetCategory::Buildings;
    if (path.find("/tiles/") != std::string::npos) return AssetCategory::Tiles;
    if (path.find("/models/") != std::string::npos) return AssetCategory::Models;
    if (path.find("/textures/") != std::string::npos) return AssetCategory::Textures;
    if (path.find("/scripts/") != std::string::npos) return AssetCategory::Scripts;

    if (ext == ".json") return AssetCategory::Configs;
    if (ext == ".obj" || ext == ".fbx" || ext == ".gltf") return AssetCategory::Models;
    if (ext == ".png" || ext == ".jpg" || ext == ".tga") return AssetCategory::Textures;
    if (ext == ".py") return AssetCategory::Scripts;

    return AssetCategory::All;
}

void AssetBrowserEnhanced::LoadThumbnail(AssetInfo& asset) {
    // TODO: Load actual thumbnail from file or generate it
    asset.thumbnailTexture = GeneratePlaceholderThumbnail(asset.category);
}

unsigned int AssetBrowserEnhanced::GeneratePlaceholderThumbnail(AssetCategory category) {
    // TODO: Generate OpenGL texture for thumbnail
    // For now, return 0 (no texture)
    return 0;
}

std::vector<AssetBrowserEnhanced::AssetTemplate>
AssetBrowserEnhanced::GetTemplatesForCategory(AssetCategory category) {
    std::vector<AssetTemplate> templates;

    switch (category) {
        case AssetCategory::Units:
            templates.push_back({"basic_unit", "Basic Unit", "Simple melee unit with basic stats"});
            templates.push_back({"ranged_unit", "Ranged Unit", "Unit with ranged attack"});
            templates.push_back({"hero_unit", "Hero Unit", "Powerful hero unit"});
            break;
        case AssetCategory::Buildings:
            templates.push_back({"basic_building", "Basic Building", "Simple production building"});
            templates.push_back({"defense_tower", "Defense Tower", "Defensive structure"});
            templates.push_back({"resource_generator", "Resource Generator", "Building that produces resources"});
            break;
        case AssetCategory::Tiles:
            templates.push_back({"terrain_tile", "Terrain Tile", "Basic terrain tile"});
            templates.push_back({"special_tile", "Special Tile", "Tile with special properties"});
            break;
        default:
            break;
    }

    return templates;
}

} // namespace Vehement
