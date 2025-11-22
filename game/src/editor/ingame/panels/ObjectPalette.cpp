#include "ObjectPalette.hpp"
#include "../MapEditor.hpp"
#include <imgui.h>
#include <algorithm>

namespace Vehement {

ObjectPalette::ObjectPalette() = default;
ObjectPalette::~ObjectPalette() = default;

void ObjectPalette::Initialize(MapEditor& mapEditor) {
    m_mapEditor = &mapEditor;
    LoadPaletteEntries();
}

void ObjectPalette::Shutdown() {
    m_mapEditor = nullptr;
    m_entries.clear();
    m_recentObjects.clear();
}

void ObjectPalette::LoadPaletteEntries() {
    // Units
    {
        std::vector<PaletteEntry> units;
        units.push_back({"unit_worker", "Worker", "workers", "", "Basic worker unit"});
        units.push_back({"unit_soldier", "Soldier", "military", "", "Infantry unit"});
        units.push_back({"unit_archer", "Archer", "military", "", "Ranged unit"});
        units.push_back({"unit_cavalry", "Cavalry", "military", "", "Fast melee unit"});
        units.push_back({"unit_siege", "Siege Engine", "military", "", "Anti-building unit"});
        units.push_back({"unit_hero", "Hero", "heroes", "", "Powerful unique unit"});
        m_entries[PaletteCategory::Units] = units;
    }

    // Buildings
    {
        std::vector<PaletteEntry> buildings;
        buildings.push_back({"building_town_hall", "Town Hall", "base", "", "Main building"});
        buildings.push_back({"building_barracks", "Barracks", "military", "", "Train soldiers"});
        buildings.push_back({"building_farm", "Farm", "economy", "", "Provides food"});
        buildings.push_back({"building_tower", "Watch Tower", "defense", "", "Defensive structure"});
        buildings.push_back({"building_wall", "Wall", "defense", "", "Blocks movement"});
        buildings.push_back({"building_gate", "Gate", "defense", "", "Controllable passage"});
        buildings.push_back({"building_mine", "Gold Mine", "economy", "", "Gold resource"});
        buildings.push_back({"building_lumber", "Lumber Mill", "economy", "", "Wood processing"});
        m_entries[PaletteCategory::Buildings] = buildings;
    }

    // Doodads
    {
        std::vector<PaletteEntry> doodads;
        doodads.push_back({"doodad_tree_oak", "Oak Tree", "trees", "", "Provides wood"});
        doodads.push_back({"doodad_tree_pine", "Pine Tree", "trees", "", "Provides wood"});
        doodads.push_back({"doodad_rock_large", "Large Rock", "rocks", "", "Decorative"});
        doodads.push_back({"doodad_rock_small", "Small Rock", "rocks", "", "Decorative"});
        doodads.push_back({"doodad_bush", "Bush", "foliage", "", "Decorative"});
        doodads.push_back({"doodad_flower", "Flowers", "foliage", "", "Decorative"});
        doodads.push_back({"doodad_ruins", "Ruins", "structures", "", "Ancient ruins"});
        doodads.push_back({"doodad_statue", "Statue", "structures", "", "Decorative statue"});
        doodads.push_back({"doodad_campfire", "Campfire", "props", "", "Light source"});
        doodads.push_back({"doodad_crate", "Crate", "props", "", "Wooden crate"});
        m_entries[PaletteCategory::Doodads] = doodads;
    }

    // Items
    {
        std::vector<PaletteEntry> items;
        items.push_back({"item_gold_pile", "Gold Pile", "resources", "", "Pickupable gold"});
        items.push_back({"item_health_potion", "Health Potion", "consumables", "", "Restores health"});
        items.push_back({"item_mana_potion", "Mana Potion", "consumables", "", "Restores mana"});
        items.push_back({"item_sword", "Sword", "equipment", "", "Weapon"});
        items.push_back({"item_armor", "Armor", "equipment", "", "Protection"});
        items.push_back({"item_scroll", "Scroll", "consumables", "", "Magic scroll"});
        m_entries[PaletteCategory::Items] = items;
    }

    // Special
    {
        std::vector<PaletteEntry> special;
        special.push_back({"spawn_player", "Player Start", "spawns", "", "Player spawn point"});
        special.push_back({"spawn_creep", "Creep Camp", "spawns", "", "Neutral creep spawn"});
        special.push_back({"resource_gold", "Gold Mine", "resources", "", "Gold resource node"});
        special.push_back({"resource_wood", "Forest", "resources", "", "Wood resource area"});
        special.push_back({"trigger_zone", "Trigger Zone", "triggers", "", "Event trigger area"});
        special.push_back({"camera_waypoint", "Camera Point", "cinematic", "", "Cinematic camera"});
        m_entries[PaletteCategory::Special] = special;
    }
}

void ObjectPalette::Update(float deltaTime) {
    // Update any animations
}

void ObjectPalette::Render() {
    ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Objects", nullptr, ImGuiWindowFlags_NoCollapse)) {
        RenderCategoryTabs();
        ImGui::Separator();
        RenderSearchBar();
        ImGui::Separator();
        RenderObjectGrid();

        if (m_showPreview && !m_selectedId.empty()) {
            ImGui::Separator();
            RenderPreview();
        }
    }
    ImGui::End();
}

void ObjectPalette::SelectObject(const std::string& id) {
    m_selectedId = id;

    // Determine type from category
    switch (m_category) {
        case PaletteCategory::Units:
            m_selectedType = "unit";
            break;
        case PaletteCategory::Buildings:
            m_selectedType = "building";
            break;
        case PaletteCategory::Doodads:
            m_selectedType = "doodad";
            break;
        case PaletteCategory::Items:
            m_selectedType = "item";
            break;
        default:
            m_selectedType = "object";
            break;
    }

    // Update map editor
    if (m_mapEditor) {
        m_mapEditor->SetCurrentObjectType(m_selectedType, m_selectedId);
        m_mapEditor->SetTool(MapTool::PlaceObject);
    }

    // Add to recent
    AddToRecent(id);
}

void ObjectPalette::SetCategory(PaletteCategory category) {
    m_category = category;
}

void ObjectPalette::SetFilter(const std::string& filter) {
    m_searchFilter = filter;
}

void ObjectPalette::ClearFilter() {
    m_searchFilter.clear();
}

void ObjectPalette::AddToRecent(const std::string& id) {
    // Remove if already exists
    m_recentObjects.erase(
        std::remove(m_recentObjects.begin(), m_recentObjects.end(), id),
        m_recentObjects.end());

    // Add to front
    m_recentObjects.insert(m_recentObjects.begin(), id);

    // Trim to max
    if (m_recentObjects.size() > MAX_RECENT) {
        m_recentObjects.resize(MAX_RECENT);
    }
}

void ObjectPalette::ClearRecent() {
    m_recentObjects.clear();
}

void ObjectPalette::RenderCategoryTabs() {
    if (ImGui::BeginTabBar("CategoryTabs")) {
        if (ImGui::BeginTabItem("Units")) {
            m_category = PaletteCategory::Units;
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Buildings")) {
            m_category = PaletteCategory::Buildings;
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Doodads")) {
            m_category = PaletteCategory::Doodads;
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Items")) {
            m_category = PaletteCategory::Items;
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Special")) {
            m_category = PaletteCategory::Special;
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Recent")) {
            m_category = PaletteCategory::Recent;
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void ObjectPalette::RenderSearchBar() {
    char searchBuffer[128] = "";
    strncpy(searchBuffer, m_searchFilter.c_str(), sizeof(searchBuffer) - 1);

    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##Search", "Search objects...", searchBuffer, sizeof(searchBuffer))) {
        m_searchFilter = searchBuffer;
    }
}

void ObjectPalette::RenderObjectGrid() {
    auto entries = GetFilteredEntries();

    if (entries.empty()) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No objects found");
        return;
    }

    // Calculate grid layout
    float windowWidth = ImGui::GetContentRegionAvail().x;
    float itemWidth = m_iconSize + ImGui::GetStyle().ItemSpacing.x;
    int columns = std::max(1, static_cast<int>(windowWidth / itemWidth));

    ImGui::BeginChild("ObjectGrid", ImVec2(0, 200), true);

    int column = 0;
    for (const auto& entry : entries) {
        bool isSelected = (m_selectedId == entry.id);

        ImGui::PushID(entry.id.c_str());

        // Draw button/selectable
        if (ImGui::Selectable(("##" + entry.id).c_str(), isSelected,
                              ImGuiSelectableFlags_None,
                              ImVec2(m_iconSize, m_iconSize + 20))) {
            SelectObject(entry.id);
        }

        // Tooltip
        if (ImGui::IsItemHovered()) {
            RenderObjectTooltip(entry);
        }

        // Draw icon placeholder
        ImVec2 itemPos = ImGui::GetItemRectMin();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        ImU32 bgColor = isSelected ? IM_COL32(100, 150, 200, 255) : IM_COL32(60, 60, 60, 255);
        drawList->AddRectFilled(itemPos,
                                ImVec2(itemPos.x + m_iconSize, itemPos.y + m_iconSize),
                                bgColor, 4.0f);

        // Draw name
        ImVec2 textPos(itemPos.x, itemPos.y + m_iconSize + 2);
        drawList->AddText(textPos, IM_COL32(255, 255, 255, 255),
                          entry.name.substr(0, 8).c_str());

        ImGui::PopID();

        column++;
        if (column < columns) {
            ImGui::SameLine();
        } else {
            column = 0;
        }
    }

    ImGui::EndChild();
}

void ObjectPalette::RenderPreview() {
    ImGui::Text("Preview");

    // Find selected entry
    for (const auto& [cat, entries] : m_entries) {
        for (const auto& entry : entries) {
            if (entry.id == m_selectedId) {
                ImGui::Text("Name: %s", entry.name.c_str());
                ImGui::Text("Category: %s", entry.category.c_str());
                ImGui::TextWrapped("Description: %s", entry.tooltip.c_str());

                // 3D preview would go here
                ImGui::BeginChild("3DPreview", ImVec2(0, 100), true);
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[3D Preview]");
                ImGui::EndChild();

                return;
            }
        }
    }
}

void ObjectPalette::RenderObjectTooltip(const PaletteEntry& entry) {
    ImGui::BeginTooltip();
    ImGui::Text("%s", entry.name.c_str());
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", entry.tooltip.c_str());
    ImGui::EndTooltip();
}

std::vector<PaletteEntry> ObjectPalette::GetFilteredEntries() const {
    std::vector<PaletteEntry> result;

    // Handle recent category specially
    if (m_category == PaletteCategory::Recent) {
        for (const auto& recentId : m_recentObjects) {
            for (const auto& [cat, entries] : m_entries) {
                for (const auto& entry : entries) {
                    if (entry.id == recentId) {
                        result.push_back(entry);
                        break;
                    }
                }
            }
        }
        return result;
    }

    // Get entries for current category
    auto it = m_entries.find(m_category);
    if (it == m_entries.end()) {
        return result;
    }

    const auto& entries = it->second;

    // Apply search filter
    if (m_searchFilter.empty()) {
        return entries;
    }

    std::string filterLower = m_searchFilter;
    std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);

    for (const auto& entry : entries) {
        std::string nameLower = entry.name;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

        if (nameLower.find(filterLower) != std::string::npos) {
            result.push_back(entry);
        }
    }

    return result;
}

} // namespace Vehement
