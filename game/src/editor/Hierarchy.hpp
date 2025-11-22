#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace Vehement {

class Editor;

/**
 * @brief Entity hierarchy panel
 *
 * Shows all entities in the scene:
 * - Tree view organization
 * - Filter by type
 * - Select to inspect
 * - Create/delete entities
 */
class Hierarchy {
public:
    explicit Hierarchy(Editor* editor);
    ~Hierarchy();

    void Render();

    void Refresh();
    void SetFilter(const std::string& filter);

    std::function<void(uint64_t)> OnEntitySelected;

private:
    void RenderToolbar();
    void RenderEntityTree();
    void RenderContextMenu(uint64_t entityId);

    Editor* m_editor = nullptr;

    struct EntityInfo {
        uint64_t id;
        std::string name;
        std::string type;
        uint64_t parentId;
        bool expanded;
    };
    std::vector<EntityInfo> m_entities;

    std::string m_searchFilter;
    std::string m_typeFilter;  // "all", "unit", "building", "tile", etc.
    uint64_t m_selectedEntity = 0;
};

} // namespace Vehement
