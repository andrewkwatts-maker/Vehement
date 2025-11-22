#pragma once

#include <string>
#include <vector>

namespace Vehement {

class Editor;

/**
 * @brief Python script editor panel
 *
 * Features:
 * - Syntax-highlighted text editor
 * - Error display
 * - Run script button
 * - Variable watch
 * - Console output
 */
class ScriptEditor {
public:
    explicit ScriptEditor(Editor* editor);
    ~ScriptEditor();

    void Render();

    void OpenScript(const std::string& path);
    void SaveScript();
    void RunScript();

private:
    void RenderToolbar();
    void RenderEditor();
    void RenderOutput();
    void RenderVariables();

    Editor* m_editor = nullptr;

    std::string m_currentFile;
    std::string m_scriptContent;
    bool m_modified = false;

    std::vector<std::string> m_output;
    std::vector<std::string> m_errors;

    // Variables watch
    struct WatchVariable {
        std::string name;
        std::string value;
        std::string type;
    };
    std::vector<WatchVariable> m_watchVariables;
};

} // namespace Vehement
