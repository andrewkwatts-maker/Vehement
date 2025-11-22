#include "Hierarchy.hpp"
#include "Editor.hpp"
#include "WorldView.hpp"
#include "../entities/EntityManager.hpp"
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
        // Delete selected entity
        // First delete from entity manager
        if (m_editor && m_editor->GetEntityManager()) {
            m_editor->GetEntityManager()->DestroyEntity(m_selectedEntity);
        }
        // Then remove from hierarchy list
        auto it = std::find_if(m_entities.begin(), m_entities.end(),
            [this](const HierarchyNode& node) { return node.id == m_selectedEntity; });
        if (it != m_entities.end()) {
            m_entities.erase(it);
        }
        m_selectedEntity = 0;
        if (m_editor) m_editor->MarkDirty();
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
                    // Reparent entity
                    if (droppedId != entity.id) {  // Don't parent to self
                        // Find the dropped entity and update its parent
                        for (auto& e : m_entities) {
                            if (e.id == droppedId) {
                                // Check to prevent circular parenting
                                bool wouldBeCircular = false;
                                uint64_t checkId = entity.id;
                                while (checkId != 0) {
                                    if (checkId == droppedId) {
                                        wouldBeCircular = true;
                                        break;
                                    }
                                    // Find parent
                                    for (const auto& p : m_entities) {
                                        if (p.id == checkId) {
                                            checkId = p.parentId;
                                            break;
                                        }
                                    }
                                }

                                if (!wouldBeCircular) {
                                    e.parentId = entity.id;
                                    if (m_editor) m_editor->MarkDirty();
                                }
                                break;
                            }
                        }
                    }
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
        if (ImGui::MenuItem("Rename")) {
            // Open rename popup (would need modal)
        }
        if (ImGui::MenuItem("Duplicate")) {
            // Duplicate the entity
            for (const auto& e : m_entities) {
                if (e.id == entityId) {
                    HierarchyNode newNode = e;
                    newNode.id = m_entities.size() + 1 + (rand() % 1000);  // Generate new ID
                    newNode.name = e.name + "_copy";
                    m_entities.push_back(newNode);

                    // Also duplicate in entity manager
                    if (m_editor && m_editor->GetEntityManager()) {
                        // Would call entity manager duplicate here
                        m_editor->MarkDirty();
                    }
                    break;
                }
            }
        }
        if (ImGui::MenuItem("Delete")) {
            // Delete entity
            if (m_editor && m_editor->GetEntityManager()) {
                m_editor->GetEntityManager()->DestroyEntity(entityId);
            }
            auto it = std::find_if(m_entities.begin(), m_entities.end(),
                [entityId](const HierarchyNode& node) { return node.id == entityId; });
            if (it != m_entities.end()) {
                m_entities.erase(it);
            }
            if (m_selectedEntity == entityId) m_selectedEntity = 0;
            if (m_editor) m_editor->MarkDirty();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Focus")) {
            // Focus camera on entity using WorldView
            if (m_editor) {
                if (auto* worldView = m_editor->GetWorldView()) {
                    // Find entity position
                    if (auto* entityMgr = m_editor->GetEntityManager()) {
                        auto* entity = entityMgr->GetEntity(entityId);
                        if (entity) {
                            glm::vec3 pos = entity->GetPosition();
                            worldView->GoToLocation(pos.x, pos.y, pos.z);
                            worldView->FocusOnSelection();
                        }
                    }
                }
            }
        }
        if (ImGui::MenuItem("Select Children")) {
            // Select all children
            std::vector<uint64_t> childIds;
            std::function<void(uint64_t)> findChildren = [&](uint64_t parentId) {
                for (const auto& e : m_entities) {
                    if (e.parentId == parentId) {
                        childIds.push_back(e.id);
                        findChildren(e.id);
                    }
                }
            };
            findChildren(entityId);
            // Would need multi-selection support to use childIds
        }
        ImGui::EndPopup();
    }
}

void Hierarchy::Refresh() {
    // Refresh from EntityManager
    if (!m_editor || !m_editor->GetEntityManager()) return;

    auto* entityMgr = m_editor->GetEntityManager();

    // Clear existing entities
    m_entities.clear();

    // Create root node
    m_entities.push_back({1, "World", "root", 0, true});

    // Add category groups
    uint64_t unitsGroupId = 2;
    uint64_t buildingsGroupId = 3;
    uint64_t resourcesGroupId = 4;

    m_entities.push_back({unitsGroupId, "Units", "group", 1, true});
    m_entities.push_back({buildingsGroupId, "Buildings", "group", 1, true});
    m_entities.push_back({resourcesGroupId, "Resources", "group", 1, true});

    // Populate from entity manager
    entityMgr->ForEachEntity([&](Entity& entity) {
        HierarchyNode node;
        node.id = entity.GetId();
        node.name = entity.GetName();
        if (node.name.empty()) {
            node.name = "Entity_" + std::to_string(node.id);
        }
        node.type = entity.GetTypeName();
        node.expanded = false;

        // Assign to appropriate group based on type
        if (node.type == "unit" || node.type == "npc" || node.type == "enemy") {
            node.parentId = unitsGroupId;
        } else if (node.type == "building") {
            node.parentId = buildingsGroupId;
        } else if (node.type == "resource") {
            node.parentId = resourcesGroupId;
        } else {
            node.parentId = 1;  // World root
        }

        m_entities.push_back(node);
    });
}

void Hierarchy::SetFilter(const std::string& filter) {
    m_typeFilter = filter;
}

} // namespace Vehement
