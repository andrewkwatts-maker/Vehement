#include "Hierarchy.hpp"
#include "Editor.hpp"
#include <imgui.h>
#include <algorithm>

namespace Vehement {

Hierarchy::Hierarchy(Editor* editor) : m_editor(editor) {
    // Populate with some test entities
    m_entities = {
        {1, "World", "root", 0, true},
        {2, "Terrain", "terrain", 1, true},
        {3, "Buildings", "group", 1, true},
        {4, "House_01", "building", 3, false},
        {5, "House_02", "building", 3, false},
        {6, "Barracks", "building", 3, false},
        {7, "Units", "group", 1, true},
        {8, "Soldier_01", "unit", 7, false},
        {9, "Worker_01", "unit", 7, false},
        {10, "Worker_02", "unit", 7, false},
        {11, "Enemies", "group", 1, true},
        {12, "Zombie_01", "unit", 11, false},
        {13, "Zombie_02", "unit", 11, false},
    };
}

Hierarchy::~Hierarchy() = default;

void Hierarchy::Render() {
    if (!ImGui::Begin("Hierarchy")) {
        ImGui::End();
        return;
    }

    RenderToolbar();
    ImGui::Separator();

    ImGui::BeginChild("EntityList", ImVec2(0, 0), false);
    RenderEntityTree();
    ImGui::EndChild();

    ImGui::End();
}

void Hierarchy::RenderToolbar() {
    // Create entity button
    if (ImGui::Button("+")) {
        ImGui::OpenPopup("CreateEntityPopup");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Create Entity");

    if (ImGui::BeginPopup("CreateEntityPopup")) {
        if (ImGui::MenuItem("Empty")) {}
        ImGui::Separator();
        if (ImGui::BeginMenu("Unit")) {
            if (ImGui::MenuItem("Soldier")) {}
            if (ImGui::MenuItem("Worker")) {}
            if (ImGui::MenuItem("Scout")) {}
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Building")) {
            if (ImGui::MenuItem("House")) {}
            if (ImGui::MenuItem("Barracks")) {}
            if (ImGui::MenuItem("Farm")) {}
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Resource")) {
            if (ImGui::MenuItem("Tree")) {}
            if (ImGui::MenuItem("Rock")) {}
            if (ImGui::MenuItem("Bush")) {}
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();

    // Delete button
    if (ImGui::Button("X") && m_selectedEntity != 0) {
        // TODO: Delete selected entity
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete Selected");

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // Type filter
    static const char* typeFilters[] = {"All", "Units", "Buildings", "Resources", "Groups"};
    static int currentFilter = 0;
    ImGui::SetNextItemWidth(100);
    ImGui::Combo("##typefilter", &currentFilter, typeFilters, IM_ARRAYSIZE(typeFilters));

    ImGui::SameLine();

    // Search
    static char searchBuffer[128] = "";
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##search", "Search...", searchBuffer, sizeof(searchBuffer))) {
        m_searchFilter = searchBuffer;
    }
}

void Hierarchy::RenderEntityTree() {
    // Build tree structure
    std::function<void(uint64_t, int)> renderNode = [&](uint64_t parentId, int depth) {
        for (auto& entity : m_entities) {
            if (entity.parentId != parentId) continue;

            // Apply search filter
            if (!m_searchFilter.empty()) {
                std::string nameLower = entity.name;
                std::string filterLower = m_searchFilter;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);
                if (nameLower.find(filterLower) == std::string::npos) continue;
            }

            // Count children
            int childCount = 0;
            for (const auto& e : m_entities) {
                if (e.parentId == entity.id) childCount++;
            }

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (childCount == 0) flags |= ImGuiTreeNodeFlags_Leaf;
            if (entity.id == m_selectedEntity) flags |= ImGuiTreeNodeFlags_Selected;
            if (entity.expanded) flags |= ImGuiTreeNodeFlags_DefaultOpen;

            // Icon by type
            const char* icon = "";
            if (entity.type == "unit") icon = "[U] ";
            else if (entity.type == "building") icon = "[B] ";
            else if (entity.type == "group") icon = "[G] ";
            else if (entity.type == "terrain") icon = "[T] ";

            std::string label = icon + entity.name;

            bool open = ImGui::TreeNodeEx((void*)(intptr_t)entity.id, flags, "%s", label.c_str());

            // Selection
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                m_selectedEntity = entity.id;
                if (OnEntitySelected) OnEntitySelected(entity.id);
            }

            // Context menu
            RenderContextMenu(entity.id);

            // Drag-drop for reparenting
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("ENTITY_ID", &entity.id, sizeof(entity.id));
                ImGui::Text("%s", entity.name.c_str());
                ImGui::EndDragDropSource();
            }

            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID")) {
                    uint64_t droppedId = *(uint64_t*)payload->Data;
                    // TODO: Reparent entity
                }
                ImGui::EndDragDropTarget();
            }

            if (open) {
                entity.expanded = true;
                renderNode(entity.id, depth + 1);
                ImGui::TreePop();
            } else {
                entity.expanded = false;
            }
        }
    };

    renderNode(0, 0);
}

void Hierarchy::RenderContextMenu(uint64_t entityId) {
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Rename")) {}
        if (ImGui::MenuItem("Duplicate")) {}
        if (ImGui::MenuItem("Delete")) {}
        ImGui::Separator();
        if (ImGui::MenuItem("Focus")) {
            // TODO: Focus camera on entity
        }
        if (ImGui::MenuItem("Select Children")) {}
        ImGui::EndPopup();
    }
}

void Hierarchy::Refresh() {
    // TODO: Refresh from EntityManager
}

void Hierarchy::SetFilter(const std::string& filter) {
    m_typeFilter = filter;
}

} // namespace Vehement
