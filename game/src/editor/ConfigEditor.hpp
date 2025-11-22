#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace Vehement {

class Editor;

/**
 * @brief Entity configuration editor panel
 *
 * Provides UI for editing unit, building, and tile JSON configs:
 * - Tree view of all configs organized by type
 * - JSON editor with syntax highlighting
 * - Property inspector with typed fields
 * - Model preview viewport
 * - Collision shape visualization
 * - Python script path browser
 * - Hot-reload on save
 */
class ConfigEditor {
public:
    explicit ConfigEditor(Editor* editor);
    ~ConfigEditor();

    void Update(float deltaTime);
    void Render();

    // Config selection
    void SelectConfig(const std::string& configId);
    const std::string& GetSelectedConfig() const { return m_selectedConfigId; }

    // Actions
    void CreateNewConfig(const std::string& type);
    void DuplicateConfig(const std::string& configId);
    void DeleteConfig(const std::string& configId);
    void SaveConfig(const std::string& configId);
    void ReloadConfig(const std::string& configId);
    void ValidateConfig(const std::string& configId);

    // Callbacks
    std::function<void(const std::string&)> OnConfigSelected;
    std::function<void(const std::string&)> OnConfigModified;

private:
    void RenderTreeView();
    void RenderConfigDetails();
    void RenderJsonEditor();
    void RenderPropertyInspector();
    void RenderModelPreview();
    void RenderCollisionPreview();
    void RenderScriptBrowser();
    void RenderToolbar();
    void RenderContextMenu();

    void RefreshConfigList();
    void LoadConfigIntoEditor(const std::string& configId);
    void SaveEditorToConfig();

    Editor* m_editor = nullptr;

    // Selection state
    std::string m_selectedConfigId;
    std::string m_selectedType;  // "unit", "building", "tile"

    // Config lists
    std::vector<std::string> m_unitConfigs;
    std::vector<std::string> m_buildingConfigs;
    std::vector<std::string> m_tileConfigs;

    // Editor state
    std::string m_jsonBuffer;
    bool m_jsonModified = false;
    std::string m_searchFilter;

    // Preview state
    bool m_showModelPreview = true;
    bool m_showCollisionShapes = true;
    float m_previewRotation = 0.0f;
    float m_previewZoom = 1.0f;

    // Validation
    std::vector<std::string> m_validationErrors;
    std::vector<std::string> m_validationWarnings;
};

} // namespace Vehement
