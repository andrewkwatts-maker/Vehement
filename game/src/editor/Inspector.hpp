#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>

namespace Vehement {

class Editor;

/**
 * @brief Property inspector panel
 *
 * Shows and edits properties of selected entity/tile/config:
 * - Transform component
 * - Custom components
 * - Add/remove components
 */
class Inspector {
public:
    explicit Inspector(Editor* editor);
    ~Inspector();

    void Render();

    void SetSelectedEntity(uint64_t entityId);
    void ClearSelection();

    std::function<void(uint64_t)> OnEntityModified;

private:
    void RenderEntityInspector();
    void RenderTransformComponent();
    void RenderComponents();
    void RenderAddComponent();

    Editor* m_editor = nullptr;

    uint64_t m_selectedEntity = 0;
    std::string m_entityName;
    std::string m_entityType;

    // Transform
    float m_position[3] = {0, 0, 0};
    float m_rotation[3] = {0, 0, 0};
    float m_scale[3] = {1, 1, 1};

    // Component list
    struct ComponentInfo {
        std::string name;
        std::string type;
        bool expanded;
    };
    std::vector<ComponentInfo> m_components;
};

} // namespace Vehement
