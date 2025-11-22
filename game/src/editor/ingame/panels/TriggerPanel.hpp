#pragma once

#include "../TriggerEditor.hpp"
#include <string>
#include <vector>

namespace Vehement {

class TriggerEditor;

/**
 * @brief Trigger Panel - Trigger list and quick editor
 *
 * Features:
 * - Trigger tree view
 * - Event/condition/action quick builders
 * - Variable manager shortcut
 * - Trigger debugger
 */
class TriggerPanel {
public:
    TriggerPanel();
    ~TriggerPanel();

    void Initialize(TriggerEditor& triggerEditor);
    void Shutdown();

    void Update(float deltaTime);
    void Render();

private:
    void RenderTriggerList();
    void RenderQuickBuilder();
    void RenderEventBuilder();
    void RenderConditionBuilder();
    void RenderActionBuilder();
    void RenderDebugger();

    TriggerEditor* m_triggerEditor = nullptr;

    // Quick builder state
    int m_builderMode = 0;  // 0=event, 1=condition, 2=action
    std::string m_selectedTemplate;

    // Debugger state
    bool m_showDebugger = false;
    std::vector<std::string> m_debugLog;
    bool m_stepMode = false;
    int m_breakpointTriggerId = -1;
};

} // namespace Vehement
