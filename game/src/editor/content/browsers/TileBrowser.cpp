#include "TileBrowser.hpp"
#include "../ContentBrowser.hpp"
#include "../ContentDatabase.hpp"
#include "../../Editor.hpp"
#include <Json/Json.h>
#include <imgui.h>
#include <fstream>
#include <algorithm>
#include <cmath>

namespace Vehement {
namespace Content {

// =============================================================================
// Constructor / Destructor
// =============================================================================

TileBrowser::TileBrowser(Editor* editor, ContentBrowser* contentBrowser)
    : m_editor(editor)
    , m_contentBrowser(contentBrowser)
{
}

TileBrowser::~TileBrowser() {
    Shutdown();
}

// =============================================================================
// Lifecycle
// =============================================================================

bool TileBrowser::Initialize() {
    if (m_initialized) {
        return true;
    }

    CacheTiles();

    // Create default palette groups
    CreatePaletteGroup("Favorites");
    CreatePaletteGroup("Ground");
    CreatePaletteGroup("Water");
    CreatePaletteGroup("Decoration");

    m_initialized = true;
    return true;
}

void TileBrowser::Shutdown() {
    m_allTiles.clear();
    m_filteredTiles.clear();
    m_paletteGroups.clear();
    m_initialized = false;
}

void TileBrowser::Render() {
    ImGui::Begin("Tile Browser", nullptr, ImGuiWindowFlags_MenuBar);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Show Walkability", nullptr, &m_showWalkability);
            ImGui::MenuItem("Show Resources", nullptr, &m_showResources);
            ImGui::Separator();
            ImGui::MenuItem("Palette View", nullptr, &m_paletteView);
            ImGui::Separator();
            if (ImGui::BeginMenu("Grid Columns")) {
                if (ImGui::MenuItem("3", nullptr, m_gridColumns == 3)) m_gridColumns = 3;
                if (ImGui::MenuItem("4", nullptr, m_gridColumns == 4)) m_gridColumns = 4;
                if (ImGui::MenuItem("5", nullptr, m_gridColumns == 5)) m_gridColumns = 5;
                if (ImGui::MenuItem("6", nullptr, m_gridColumns == 6)) m_gridColumns = 6;
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Filter")) {
            if (ImGui::MenuItem("Clear Filters")) {
                ClearFilters();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Palette")) {
            if (ImGui::MenuItem("Create Group...")) {
                // TODO: Show create dialog
            }
            ImGui::Separator();
            for (const auto& group : m_paletteGroups) {
                if (ImGui::MenuItem(group.name.c_str())) {
                    // Navigate to palette group
                }
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    RenderToolbar();

    // Filters panel
    ImGui::BeginChild("TileFilterPanel", ImVec2(180, 0), true);
    RenderFilters();
    ImGui::EndChild();

    ImGui::SameLine();

    // Content area
    ImGui::BeginChild("TileContent", ImVec2(0, 0), true);

    if (m_paletteView) {
        RenderPaletteView();
    } else {
        RenderTileGrid();
    }

    ImGui::EndChild();

    ImGui::End();
}

void TileBrowser::Update(float deltaTime) {
    if (m_needsRefresh) {
        CacheTiles();
        m_needsRefresh = false;
    }
}

// =============================================================================
// Tile Access
// =============================================================================

std::vector<TileStats> TileBrowser::GetAllTiles() const {
    return m_allTiles;
}

std::optional<TileStats> TileBrowser::GetTile(const std::string& id) const {
    for (const auto& tile : m_allTiles) {
        if (tile.id == id) {
            return tile;
        }
    }
    return std::nullopt;
}

std::vector<TileStats> TileBrowser::GetFilteredTiles() const {
    return m_filteredTiles;
}

void TileBrowser::RefreshTiles() {
    m_needsRefresh = true;
}

// =============================================================================
// Filtering
// =============================================================================

void TileBrowser::SetFilter(const TileFilterOptions& filter) {
    m_filter = filter;

    m_filteredTiles.clear();
    for (const auto& tile : m_allTiles) {
        if (MatchesFilter(tile)) {
            m_filteredTiles.push_back(tile);
        }
    }
}

void TileBrowser::FilterByTerrainType(TerrainType type) {
    m_filter.terrainTypes.clear();
    m_filter.terrainTypes.push_back(type);
    SetFilter(m_filter);
}

void TileBrowser::FilterByBiome(TileBiome biome) {
    m_filter.biomes.clear();
    m_filter.biomes.push_back(biome);
    SetFilter(m_filter);
}

void TileBrowser::FilterByWalkability(Walkability walkability) {
    m_filter.walkabilities.clear();
    m_filter.walkabilities.push_back(walkability);
    SetFilter(m_filter);
}

void TileBrowser::ClearFilters() {
    m_filter = TileFilterOptions();
    m_filteredTiles = m_allTiles;
}

// =============================================================================
// Palette Management
// =============================================================================

std::vector<TilePaletteGroup> TileBrowser::GetPaletteGroups() const {
    return m_paletteGroups;
}

void TileBrowser::CreatePaletteGroup(const std::string& name) {
    // Check if already exists
    for (const auto& group : m_paletteGroups) {
        if (group.name == name) {
            return;
        }
    }

    TilePaletteGroup group;
    group.name = name;
    group.color = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
    group.expanded = true;
    m_paletteGroups.push_back(group);
}

void TileBrowser::AddToPaletteGroup(const std::string& groupName, const std::string& tileId) {
    for (auto& group : m_paletteGroups) {
        if (group.name == groupName) {
            // Check if already in group
            if (std::find(group.tileIds.begin(), group.tileIds.end(), tileId) == group.tileIds.end()) {
                group.tileIds.push_back(tileId);
            }
            return;
        }
    }
}

void TileBrowser::RemoveFromPaletteGroup(const std::string& groupName, const std::string& tileId) {
    for (auto& group : m_paletteGroups) {
        if (group.name == groupName) {
            group.tileIds.erase(
                std::remove(group.tileIds.begin(), group.tileIds.end(), tileId),
                group.tileIds.end());
            return;
        }
    }
}

void TileBrowser::DeletePaletteGroup(const std::string& name) {
    m_paletteGroups.erase(
        std::remove_if(m_paletteGroups.begin(), m_paletteGroups.end(),
            [&](const TilePaletteGroup& g) { return g.name == name; }),
        m_paletteGroups.end());
}

std::vector<TileStats> TileBrowser::GetTilesByTerrainType(TerrainType type) const {
    std::vector<TileStats> result;
    for (const auto& tile : m_allTiles) {
        if (tile.terrainType == type) {
            result.push_back(tile);
        }
    }
    return result;
}

std::vector<TileStats> TileBrowser::GetTilesByBiome(TileBiome biome) const {
    std::vector<TileStats> result;
    for (const auto& tile : m_allTiles) {
        if (tile.biome == biome) {
            result.push_back(tile);
        }
    }
    return result;
}

// =============================================================================
// Statistics
// =============================================================================

std::unordered_map<TerrainType, int> TileBrowser::GetTileCountByTerrain() const {
    std::unordered_map<TerrainType, int> counts;
    for (const auto& tile : m_allTiles) {
        counts[tile.terrainType]++;
    }
    return counts;
}

std::unordered_map<TileBiome, int> TileBrowser::GetTileCountByBiome() const {
    std::unordered_map<TileBiome, int> counts;
    for (const auto& tile : m_allTiles) {
        counts[tile.biome]++;
    }
    return counts;
}

std::vector<std::string> TileBrowser::GetResourceTypes() const {
    std::vector<std::string> resources;
    for (const auto& tile : m_allTiles) {
        if (!tile.resourceType.empty()) {
            if (std::find(resources.begin(), resources.end(), tile.resourceType) == resources.end()) {
                resources.push_back(tile.resourceType);
            }
        }
    }
    std::sort(resources.begin(), resources.end());
    return resources;
}

// =============================================================================
// Preview
// =============================================================================

std::string TileBrowser::GetTilePreview(const std::string& tileId) const {
    auto tileOpt = GetTile(tileId);
    if (!tileOpt) {
        return "";
    }
    return tileOpt->texturePath;
}

void TileBrowser::RenderTerrainPreview(const std::string& tileId, float x, float y, float size) {
    auto tileOpt = GetTile(tileId);
    if (!tileOpt) {
        return;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos(x, y);
    ImVec2 endPos(x + size, y + size);

    // Draw terrain color
    ImVec4 color = GetTerrainColor(tileOpt->terrainType);
    drawList->AddRectFilled(pos, endPos, IM_COL32(
        static_cast<int>(color.x * 255),
        static_cast<int>(color.y * 255),
        static_cast<int>(color.z * 255),
        static_cast<int>(color.w * 255)
    ));

    // Draw border
    drawList->AddRect(pos, endPos, IM_COL32(100, 100, 100, 255));
}

// =============================================================================
// Painting Support
// =============================================================================

void TileBrowser::SetPaintTile(const std::string& tileId) {
    m_paintTileId = tileId;

    if (OnPaintTileChanged) {
        OnPaintTileChanged(tileId);
    }
}

void TileBrowser::SetBrushTiles(const std::vector<std::string>& tileIds) {
    m_brushTiles = tileIds;
}

void TileBrowser::ToggleBrushTile(const std::string& tileId) {
    auto it = std::find(m_brushTiles.begin(), m_brushTiles.end(), tileId);
    if (it != m_brushTiles.end()) {
        m_brushTiles.erase(it);
    } else {
        m_brushTiles.push_back(tileId);
    }
}

// =============================================================================
// Private - Rendering
// =============================================================================

void TileBrowser::RenderToolbar() {
    // Search
    char searchBuffer[256] = {0};
    strncpy(searchBuffer, m_filter.searchQuery.c_str(), sizeof(searchBuffer) - 1);

    ImGui::PushItemWidth(150);
    if (ImGui::InputText("Search##TileSearch", searchBuffer, sizeof(searchBuffer))) {
        m_filter.searchQuery = searchBuffer;
        SetFilter(m_filter);
    }
    ImGui::PopItemWidth();

    ImGui::SameLine();

    if (ImGui::Button("Refresh")) {
        RefreshTiles();
    }

    ImGui::SameLine();

    // View toggle
    if (ImGui::Checkbox("Palette Mode", &m_paletteView)) {
        // View changed
    }

    ImGui::SameLine();

    // Current paint tile indicator
    if (!m_paintTileId.empty()) {
        auto tileOpt = GetTile(m_paintTileId);
        if (tileOpt) {
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "Paint: %s", tileOpt->name.c_str());
        }
    } else {
        ImGui::TextDisabled("No paint tile selected");
    }

    ImGui::Separator();
}

void TileBrowser::RenderFilters() {
    ImGui::Text("Filters");
    ImGui::Separator();

    // Terrain type filter
    if (ImGui::CollapsingHeader("Terrain", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* terrainNames[] = {
            "Ground", "Water", "Cliff", "Forest", "Mountain",
            "Desert", "Snow", "Swamp", "Road", "Bridge", "Special"
        };

        for (int i = 0; i < 11; ++i) {
            TerrainType type = static_cast<TerrainType>(i);
            bool selected = std::find(m_filter.terrainTypes.begin(), m_filter.terrainTypes.end(), type)
                            != m_filter.terrainTypes.end();

            ImVec4 color = GetTerrainColor(type);
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            if (ImGui::Checkbox(terrainNames[i], &selected)) {
                if (selected) {
                    m_filter.terrainTypes.push_back(type);
                } else {
                    m_filter.terrainTypes.erase(
                        std::remove(m_filter.terrainTypes.begin(), m_filter.terrainTypes.end(), type),
                        m_filter.terrainTypes.end());
                }
                SetFilter(m_filter);
            }
            ImGui::PopStyleColor();
        }
    }

    // Biome filter
    if (ImGui::CollapsingHeader("Biome", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* biomeNames[] = {
            "Temperate", "Desert", "Arctic", "Tropical", "Volcanic", "Underground", "Alien"
        };

        for (int i = 0; i < 7; ++i) {
            TileBiome biome = static_cast<TileBiome>(i);
            bool selected = std::find(m_filter.biomes.begin(), m_filter.biomes.end(), biome)
                            != m_filter.biomes.end();

            ImVec4 color = GetBiomeColor(biome);
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            if (ImGui::Checkbox(biomeNames[i], &selected)) {
                if (selected) {
                    m_filter.biomes.push_back(biome);
                } else {
                    m_filter.biomes.erase(
                        std::remove(m_filter.biomes.begin(), m_filter.biomes.end(), biome),
                        m_filter.biomes.end());
                }
                SetFilter(m_filter);
            }
            ImGui::PopStyleColor();
        }
    }

    // Walkability filter
    if (ImGui::CollapsingHeader("Walkability")) {
        const char* walkNames[] = {
            "Walkable", "Blocked", "Slow Walk", "Water Only", "Flying Only", "Destructible"
        };

        for (int i = 0; i < 6; ++i) {
            Walkability walk = static_cast<Walkability>(i);
            bool selected = std::find(m_filter.walkabilities.begin(), m_filter.walkabilities.end(), walk)
                            != m_filter.walkabilities.end();

            if (ImGui::Checkbox(walkNames[i], &selected)) {
                if (selected) {
                    m_filter.walkabilities.push_back(walk);
                } else {
                    m_filter.walkabilities.erase(
                        std::remove(m_filter.walkabilities.begin(), m_filter.walkabilities.end(), walk),
                        m_filter.walkabilities.end());
                }
                SetFilter(m_filter);
            }
        }
    }

    // Quick toggles
    if (ImGui::CollapsingHeader("Quick Filters")) {
        if (ImGui::Checkbox("Walkable Only", &m_filter.showWalkable)) {
            SetFilter(m_filter);
        }
        if (ImGui::Checkbox("Blocked Only", &m_filter.showBlocked)) {
            SetFilter(m_filter);
        }
        if (ImGui::Checkbox("Resource Tiles", &m_filter.showResourceTiles)) {
            SetFilter(m_filter);
        }
        if (ImGui::Checkbox("Buildable Tiles", &m_filter.showBuildableTiles)) {
            SetFilter(m_filter);
        }
    }

    ImGui::Separator();

    // Statistics
    if (ImGui::CollapsingHeader("Statistics")) {
        ImGui::Text("Total Tiles: %zu", m_allTiles.size());
        ImGui::Text("Filtered: %zu", m_filteredTiles.size());

        auto terrainCounts = GetTileCountByTerrain();
        ImGui::Text("By Terrain:");
        for (const auto& [type, count] : terrainCounts) {
            ImVec4 color = GetTerrainColor(type);
            ImGui::TextColored(color, "  %s: %d", GetTerrainTypeName(type).c_str(), count);
        }

        auto biomeCounts = GetTileCountByBiome();
        ImGui::Text("By Biome:");
        for (const auto& [biome, count] : biomeCounts) {
            ImVec4 color = GetBiomeColor(biome);
            ImGui::TextColored(color, "  %s: %d", GetBiomeName(biome).c_str(), count);
        }
    }

    // Palette groups
    if (ImGui::CollapsingHeader("Palette Groups")) {
        for (auto& group : m_paletteGroups) {
            ImGui::PushID(group.name.c_str());
            if (ImGui::TreeNode(group.name.c_str())) {
                ImGui::Text("%zu tiles", group.tileIds.size());

                if (ImGui::Button("Clear")) {
                    group.tileIds.clear();
                }
                ImGui::SameLine();
                if (ImGui::Button("Delete Group")) {
                    DeletePaletteGroup(group.name);
                }

                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        if (ImGui::Button("+ New Group")) {
            static int groupCounter = 0;
            CreatePaletteGroup("Group " + std::to_string(++groupCounter));
        }
    }
}

void TileBrowser::RenderPaletteView() {
    for (auto& group : m_paletteGroups) {
        ImGui::PushID(group.name.c_str());

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;
        if (ImGui::CollapsingHeader(group.name.c_str(), flags)) {
            if (group.tileIds.empty()) {
                ImGui::TextDisabled("No tiles in this group");
                ImGui::TextDisabled("Drag tiles here to add");
            } else {
                float tileSize = 48.0f;
                int col = 0;
                int maxCols = std::max(1, static_cast<int>((ImGui::GetContentRegionAvail().x - 20) / (tileSize + 4)));

                for (const auto& tileId : group.tileIds) {
                    auto tileOpt = GetTile(tileId);
                    if (!tileOpt) continue;

                    ImGui::PushID(tileId.c_str());

                    // Tile button
                    ImVec4 color = GetTerrainColor(tileOpt->terrainType);
                    bool isPaint = (tileId == m_paintTileId);

                    if (isPaint) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
                    }

                    if (ImGui::ColorButton(("##tile_" + tileId).c_str(), color, 0, ImVec2(tileSize, tileSize))) {
                        SetPaintTile(tileId);
                    }

                    if (isPaint) {
                        ImGui::PopStyleColor();
                    }

                    // Tooltip
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::Text("%s", tileOpt->name.c_str());
                        ImGui::EndTooltip();
                    }

                    // Context menu
                    if (ImGui::BeginPopupContextItem()) {
                        if (ImGui::MenuItem("Set as Paint Tile")) {
                            SetPaintTile(tileId);
                        }
                        if (ImGui::MenuItem("Add to Brush")) {
                            ToggleBrushTile(tileId);
                        }
                        ImGui::Separator();
                        if (ImGui::MenuItem("Remove from Group")) {
                            RemoveFromPaletteGroup(group.name, tileId);
                        }
                        ImGui::EndPopup();
                    }

                    ImGui::PopID();

                    col++;
                    if (col < maxCols) {
                        ImGui::SameLine();
                    } else {
                        col = 0;
                    }
                }
            }
        }

        // Drop target for adding tiles
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TILE")) {
                std::string tileId(static_cast<const char*>(payload->Data));
                AddToPaletteGroup(group.name, tileId);
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::PopID();
    }
}

void TileBrowser::RenderTileGrid() {
    const auto& tiles = m_filteredTiles.empty() && m_filter.searchQuery.empty()
                        ? m_allTiles : m_filteredTiles;

    float tileSize = 64.0f;
    int col = 0;

    for (const auto& tile : tiles) {
        ImGui::PushID(tile.id.c_str());
        RenderTileCard(tile);
        ImGui::PopID();

        col++;
        if (col < m_gridColumns && col < static_cast<int>(tiles.size())) {
            ImGui::SameLine();
        } else {
            col = 0;
        }
    }

    if (tiles.empty()) {
        ImGui::TextDisabled("No tiles found");
    }
}

void TileBrowser::RenderTileCard(const TileStats& tile) {
    bool selected = (tile.id == m_selectedTileId);
    bool isPaintTile = (tile.id == m_paintTileId);
    bool inBrush = std::find(m_brushTiles.begin(), m_brushTiles.end(), tile.id) != m_brushTiles.end();

    float cardWidth = (ImGui::GetContentRegionAvail().x - (m_gridColumns - 1) * 6) / m_gridColumns;
    float cardHeight = 100.0f;

    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.3f, 0.4f, 0.3f, 0.5f));
    } else if (isPaintTile) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.2f, 0.5f, 0.2f, 0.5f));
    }

    ImGui::BeginChild(("TileCard_" + tile.id).c_str(), ImVec2(cardWidth, cardHeight), true);

    // Tile preview
    ImVec2 previewSize(40, 40);
    ImVec4 terrainColor = GetTerrainColor(tile.terrainType);

    if (ImGui::ColorButton("##preview", terrainColor, 0, previewSize)) {
        SetPaintTile(tile.id);
    }

    // Drag source
    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("TILE", tile.id.c_str(), tile.id.size() + 1);
        ImGui::Text("Drag: %s", tile.name.c_str());
        ImGui::EndDragDropSource();
    }

    ImGui::SameLine();

    // Info
    ImGui::BeginGroup();
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.8f, 1.0f), "%s", tile.name.c_str());

    if (m_showWalkability) {
        RenderWalkabilityIcon(tile.walkability);
    }

    if (m_showResources && !tile.resourceType.empty()) {
        RenderResourceIcon(tile.resourceType);
    }

    // Brush indicator
    if (inBrush) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[B]");
    }

    ImGui::EndGroup();

    // Movement cost
    ImGui::Text("Move: %.1f", tile.movementCost);

    // Defense bonus
    if (tile.defenseBonus > 0) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "+%.0f%% def", tile.defenseBonus * 100);
    }

    ImGui::EndChild();

    if (selected || isPaintTile) {
        ImGui::PopStyleColor();
    }

    // Click handling
    if (ImGui::IsItemClicked()) {
        m_selectedTileId = tile.id;
        if (OnTileSelected) {
            OnTileSelected(tile.id);
        }
    }

    // Double-click to set paint tile
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
        SetPaintTile(tile.id);
        if (OnTileDoubleClicked) {
            OnTileDoubleClicked(tile.id);
        }
    }

    // Tooltip
    if (ImGui::IsItemHovered()) {
        RenderTileTooltip(tile);
    }

    // Context menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Set as Paint Tile")) {
            SetPaintTile(tile.id);
        }

        if (inBrush) {
            if (ImGui::MenuItem("Remove from Brush")) {
                ToggleBrushTile(tile.id);
            }
        } else {
            if (ImGui::MenuItem("Add to Brush")) {
                ToggleBrushTile(tile.id);
            }
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("Add to Palette")) {
            for (const auto& group : m_paletteGroups) {
                if (ImGui::MenuItem(group.name.c_str())) {
                    AddToPaletteGroup(group.name, tile.id);
                }
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Open in Editor")) {
            if (OnTileDoubleClicked) {
                OnTileDoubleClicked(tile.id);
            }
        }

        ImGui::EndPopup();
    }
}

void TileBrowser::RenderWalkabilityIcon(Walkability walkability) {
    const char* icon = "";
    ImVec4 color(0.8f, 0.8f, 0.8f, 1.0f);

    switch (walkability) {
        case Walkability::Walkable:
            icon = "[W]";
            color = ImVec4(0.3f, 0.8f, 0.3f, 1.0f);
            break;
        case Walkability::Blocked:
            icon = "[X]";
            color = ImVec4(0.8f, 0.3f, 0.3f, 1.0f);
            break;
        case Walkability::SlowWalk:
            icon = "[S]";
            color = ImVec4(0.8f, 0.8f, 0.3f, 1.0f);
            break;
        case Walkability::WaterOnly:
            icon = "[~]";
            color = ImVec4(0.3f, 0.5f, 0.8f, 1.0f);
            break;
        case Walkability::FlyingOnly:
            icon = "[F]";
            color = ImVec4(0.5f, 0.8f, 1.0f, 1.0f);
            break;
        case Walkability::Destructible:
            icon = "[D]";
            color = ImVec4(0.8f, 0.5f, 0.3f, 1.0f);
            break;
    }

    ImGui::TextColored(color, "%s", icon);
}

void TileBrowser::RenderResourceIcon(const std::string& resourceType) {
    ImVec4 color(0.8f, 0.8f, 0.2f, 1.0f);  // Default gold color

    if (resourceType == "wood") {
        color = ImVec4(0.6f, 0.4f, 0.2f, 1.0f);
    } else if (resourceType == "stone") {
        color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    } else if (resourceType == "food") {
        color = ImVec4(0.3f, 0.8f, 0.3f, 1.0f);
    } else if (resourceType == "iron" || resourceType == "ore") {
        color = ImVec4(0.4f, 0.4f, 0.5f, 1.0f);
    }

    ImGui::SameLine();
    ImGui::TextColored(color, "[%s]", resourceType.substr(0, 1).c_str());
}

void TileBrowser::RenderTileTooltip(const TileStats& tile) {
    ImGui::BeginTooltip();

    ImVec4 terrainColor = GetTerrainColor(tile.terrainType);
    ImGui::TextColored(terrainColor, "%s", tile.name.c_str());
    ImGui::TextDisabled("%s / %s", GetTerrainTypeName(tile.terrainType).c_str(),
                        GetBiomeName(tile.biome).c_str());

    ImGui::Separator();

    if (!tile.description.empty()) {
        ImGui::TextWrapped("%s", tile.description.c_str());
        ImGui::Separator();
    }

    ImGui::Text("Walkability: %s", GetWalkabilityName(tile.walkability).c_str());
    ImGui::Text("Movement Cost: %.2f", tile.movementCost);

    ImGui::Separator();
    ImGui::Text("Movement Modifiers:");
    ImGui::BulletText("Infantry: %.1fx", tile.infantryModifier);
    ImGui::BulletText("Vehicle: %.1fx", tile.vehicleModifier);
    ImGui::BulletText("Naval: %.1fx", tile.navalModifier);

    ImGui::Separator();
    ImGui::Text("Combat Modifiers:");
    ImGui::BulletText("Defense: +%.0f%%", tile.defenseBonus * 100);
    ImGui::BulletText("Cover: +%.0f%%", tile.coverBonus * 100);
    ImGui::BulletText("Visibility: %.1fx", tile.visibilityModifier);

    if (tile.blocksLineOfSight) {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.3f, 1.0f), "* Blocks Line of Sight");
    }
    if (tile.providesHighGround) {
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "* Provides High Ground");
    }

    if (!tile.resourceType.empty()) {
        ImGui::Separator();
        ImGui::Text("Resource: %s", tile.resourceType.c_str());
        ImGui::Text("Amount: %.0f (Regen: %.1f/s)", tile.resourceAmount, tile.resourceRegen);
        if (tile.isDepletable) {
            ImGui::TextColored(ImVec4(0.8f, 0.5f, 0.3f, 1.0f), "* Depletable");
        }
    }

    ImGui::Separator();
    ImGui::Text("Building: %s", tile.allowsBuilding ? "Allowed" : "Not Allowed");
    if (!tile.allowedBuildingTypes.empty()) {
        ImGui::Text("Allowed Types: ");
        for (const auto& type : tile.allowedBuildingTypes) {
            ImGui::SameLine();
            ImGui::SmallButton(type.c_str());
        }
    }

    ImGui::Separator();
    ImGui::Text("Visual:");
    ImGui::BulletText("Variations: %d", tile.variations);
    if (tile.hasTransitions) {
        ImGui::BulletText("Has Transitions");
    }
    if (tile.isAnimated) {
        ImGui::BulletText("Animated");
    }

    ImGui::EndTooltip();
}

// =============================================================================
// Private - Data Loading
// =============================================================================

TileStats TileBrowser::LoadTileStats(const std::string& assetId) const {
    TileStats stats;
    stats.id = assetId;

    auto* database = m_contentBrowser->GetDatabase();
    auto metadata = database->GetAssetMetadata(assetId);
    if (!metadata) {
        return stats;
    }

    std::ifstream file(metadata->path);
    if (!file.is_open()) {
        return stats;
    }

    Json::Value root;
    Json::CharReaderBuilder reader;
    std::string errors;
    if (!Json::parseFromStream(reader, file, &root, &errors)) {
        return stats;
    }

    stats.name = root.get("name", "Unknown").asString();
    stats.description = root.get("description", "").asString();

    // Terrain type
    std::string terrainStr = root.get("terrain", "ground").asString();
    if (terrainStr == "ground") stats.terrainType = TerrainType::Ground;
    else if (terrainStr == "water") stats.terrainType = TerrainType::Water;
    else if (terrainStr == "cliff") stats.terrainType = TerrainType::Cliff;
    else if (terrainStr == "forest") stats.terrainType = TerrainType::Forest;
    else if (terrainStr == "mountain") stats.terrainType = TerrainType::Mountain;
    else if (terrainStr == "desert") stats.terrainType = TerrainType::Desert;
    else if (terrainStr == "snow") stats.terrainType = TerrainType::Snow;
    else if (terrainStr == "swamp") stats.terrainType = TerrainType::Swamp;
    else if (terrainStr == "road") stats.terrainType = TerrainType::Road;
    else if (terrainStr == "bridge") stats.terrainType = TerrainType::Bridge;
    else stats.terrainType = TerrainType::Special;

    // Biome
    std::string biomeStr = root.get("biome", "temperate").asString();
    if (biomeStr == "temperate") stats.biome = TileBiome::Temperate;
    else if (biomeStr == "desert") stats.biome = TileBiome::Desert;
    else if (biomeStr == "arctic") stats.biome = TileBiome::Arctic;
    else if (biomeStr == "tropical") stats.biome = TileBiome::Tropical;
    else if (biomeStr == "volcanic") stats.biome = TileBiome::Volcanic;
    else if (biomeStr == "underground") stats.biome = TileBiome::Underground;
    else stats.biome = TileBiome::Alien;

    // Walkability
    std::string walkStr = root.get("walkability", "walkable").asString();
    if (walkStr == "walkable") stats.walkability = Walkability::Walkable;
    else if (walkStr == "blocked") stats.walkability = Walkability::Blocked;
    else if (walkStr == "slow") stats.walkability = Walkability::SlowWalk;
    else if (walkStr == "water") stats.walkability = Walkability::WaterOnly;
    else if (walkStr == "flying") stats.walkability = Walkability::FlyingOnly;
    else stats.walkability = Walkability::Destructible;

    // Movement
    stats.movementCost = root.get("movementCost", 1.0f).asFloat();
    stats.infantryModifier = root.get("infantryModifier", 1.0f).asFloat();
    stats.vehicleModifier = root.get("vehicleModifier", 1.0f).asFloat();
    stats.navalModifier = root.get("navalModifier", 0.0f).asFloat();

    // Combat
    stats.defenseBonus = root.get("defenseBonus", 0.0f).asFloat();
    stats.coverBonus = root.get("coverBonus", 0.0f).asFloat();
    stats.visibilityModifier = root.get("visibilityModifier", 1.0f).asFloat();
    stats.blocksLineOfSight = root.get("blocksLineOfSight", false).asBool();
    stats.providesHighGround = root.get("providesHighGround", false).asBool();

    // Resources
    stats.resourceType = root.get("resourceType", "").asString();
    stats.resourceAmount = root.get("resourceAmount", 0.0f).asFloat();
    stats.resourceRegen = root.get("resourceRegen", 0.0f).asFloat();
    stats.isDepletable = root.get("isDepletable", false).asBool();

    // Building
    stats.allowsBuilding = root.get("allowsBuilding", true).asBool();
    stats.buildingHealthMod = root.get("buildingHealthMod", 1.0f).asFloat();

    if (root.isMember("allowedBuildingTypes")) {
        for (const auto& type : root["allowedBuildingTypes"]) {
            stats.allowedBuildingTypes.push_back(type.asString());
        }
    }

    // Visual
    if (root.isMember("texture")) {
        stats.texturePath = root["texture"].get("path", "").asString();
        stats.normalMapPath = root["texture"].get("normalMap", "").asString();
        stats.variations = root["texture"].get("variations", 1).asInt();
        stats.hasTransitions = root["texture"].get("hasTransitions", true).asBool();
        stats.isAnimated = root["texture"].get("isAnimated", false).asBool();
    }

    // Tags
    if (root.isMember("tags")) {
        for (const auto& tag : root["tags"]) {
            stats.tags.push_back(tag.asString());
        }
    }

    return stats;
}

void TileBrowser::CacheTiles() {
    m_allTiles.clear();

    auto* database = m_contentBrowser->GetDatabase();
    auto allAssets = database->GetAllAssets();

    for (const auto& asset : allAssets) {
        if (asset.type == AssetType::Tile) {
            TileStats stats = LoadTileStats(asset.id);
            m_allTiles.push_back(stats);
        }
    }

    SetFilter(m_filter);
}

bool TileBrowser::MatchesFilter(const TileStats& tile) const {
    // Search query
    if (!m_filter.searchQuery.empty()) {
        std::string query = m_filter.searchQuery;
        std::transform(query.begin(), query.end(), query.begin(), ::tolower);

        std::string name = tile.name;
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);

        if (name.find(query) == std::string::npos) {
            return false;
        }
    }

    // Terrain type filter
    if (!m_filter.terrainTypes.empty()) {
        if (std::find(m_filter.terrainTypes.begin(), m_filter.terrainTypes.end(), tile.terrainType)
            == m_filter.terrainTypes.end()) {
            return false;
        }
    }

    // Biome filter
    if (!m_filter.biomes.empty()) {
        if (std::find(m_filter.biomes.begin(), m_filter.biomes.end(), tile.biome)
            == m_filter.biomes.end()) {
            return false;
        }
    }

    // Walkability filter
    if (!m_filter.walkabilities.empty()) {
        if (std::find(m_filter.walkabilities.begin(), m_filter.walkabilities.end(), tile.walkability)
            == m_filter.walkabilities.end()) {
            return false;
        }
    }

    // Quick filters
    if (m_filter.showWalkable && tile.walkability != Walkability::Walkable) return false;
    if (m_filter.showBlocked && tile.walkability != Walkability::Blocked) return false;
    if (m_filter.showResourceTiles && tile.resourceType.empty()) return false;
    if (m_filter.showBuildableTiles && !tile.allowsBuilding) return false;

    // Stat ranges
    if (m_filter.minMovementCost && tile.movementCost < *m_filter.minMovementCost) return false;
    if (m_filter.maxMovementCost && tile.movementCost > *m_filter.maxMovementCost) return false;
    if (m_filter.minDefenseBonus && tile.defenseBonus < *m_filter.minDefenseBonus) return false;

    return true;
}

// =============================================================================
// Private - Helpers
// =============================================================================

std::string TileBrowser::GetTerrainTypeName(TerrainType type) const {
    switch (type) {
        case TerrainType::Ground: return "Ground";
        case TerrainType::Water: return "Water";
        case TerrainType::Cliff: return "Cliff";
        case TerrainType::Forest: return "Forest";
        case TerrainType::Mountain: return "Mountain";
        case TerrainType::Desert: return "Desert";
        case TerrainType::Snow: return "Snow";
        case TerrainType::Swamp: return "Swamp";
        case TerrainType::Road: return "Road";
        case TerrainType::Bridge: return "Bridge";
        case TerrainType::Special: return "Special";
        default: return "Unknown";
    }
}

std::string TileBrowser::GetBiomeName(TileBiome biome) const {
    switch (biome) {
        case TileBiome::Temperate: return "Temperate";
        case TileBiome::Desert: return "Desert";
        case TileBiome::Arctic: return "Arctic";
        case TileBiome::Tropical: return "Tropical";
        case TileBiome::Volcanic: return "Volcanic";
        case TileBiome::Underground: return "Underground";
        case TileBiome::Alien: return "Alien";
        default: return "Unknown";
    }
}

std::string TileBrowser::GetWalkabilityName(Walkability walkability) const {
    switch (walkability) {
        case Walkability::Walkable: return "Walkable";
        case Walkability::Blocked: return "Blocked";
        case Walkability::SlowWalk: return "Slow Walk";
        case Walkability::WaterOnly: return "Water Only";
        case Walkability::FlyingOnly: return "Flying Only";
        case Walkability::Destructible: return "Destructible";
        default: return "Unknown";
    }
}

glm::vec4 TileBrowser::GetTerrainColor(TerrainType type) const {
    switch (type) {
        case TerrainType::Ground: return glm::vec4(0.4f, 0.6f, 0.3f, 1.0f);
        case TerrainType::Water: return glm::vec4(0.2f, 0.4f, 0.8f, 1.0f);
        case TerrainType::Cliff: return glm::vec4(0.5f, 0.4f, 0.3f, 1.0f);
        case TerrainType::Forest: return glm::vec4(0.2f, 0.5f, 0.2f, 1.0f);
        case TerrainType::Mountain: return glm::vec4(0.6f, 0.6f, 0.6f, 1.0f);
        case TerrainType::Desert: return glm::vec4(0.9f, 0.8f, 0.5f, 1.0f);
        case TerrainType::Snow: return glm::vec4(0.9f, 0.95f, 1.0f, 1.0f);
        case TerrainType::Swamp: return glm::vec4(0.3f, 0.4f, 0.3f, 1.0f);
        case TerrainType::Road: return glm::vec4(0.6f, 0.5f, 0.4f, 1.0f);
        case TerrainType::Bridge: return glm::vec4(0.5f, 0.4f, 0.3f, 1.0f);
        case TerrainType::Special: return glm::vec4(0.8f, 0.5f, 0.8f, 1.0f);
        default: return glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
    }
}

glm::vec4 TileBrowser::GetBiomeColor(TileBiome biome) const {
    switch (biome) {
        case TileBiome::Temperate: return glm::vec4(0.3f, 0.7f, 0.3f, 1.0f);
        case TileBiome::Desert: return glm::vec4(0.9f, 0.7f, 0.3f, 1.0f);
        case TileBiome::Arctic: return glm::vec4(0.7f, 0.9f, 1.0f, 1.0f);
        case TileBiome::Tropical: return glm::vec4(0.2f, 0.8f, 0.4f, 1.0f);
        case TileBiome::Volcanic: return glm::vec4(0.8f, 0.3f, 0.2f, 1.0f);
        case TileBiome::Underground: return glm::vec4(0.4f, 0.3f, 0.5f, 1.0f);
        case TileBiome::Alien: return glm::vec4(0.5f, 0.2f, 0.8f, 1.0f);
        default: return glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
    }
}

} // namespace Content
} // namespace Vehement
