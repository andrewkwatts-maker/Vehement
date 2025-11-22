#pragma once

namespace Vehement {

class Editor;

/**
 * @brief Top toolbar panel
 *
 * Contains:
 * - Tool selection
 * - Play/Pause/Stop
 * - Grid snap toggle
 * - View mode
 */
class Toolbar {
public:
    explicit Toolbar(Editor* editor);
    ~Toolbar();

    void Render();

private:
    Editor* m_editor = nullptr;

    // Tool state
    int m_currentTool = 0;  // 0=Select, 1=Move, 2=Rotate, 3=Scale, 4=Paint
    bool m_isPlaying = false;
    bool m_isPaused = false;
    bool m_snapEnabled = true;
    float m_snapSize = 1.0f;
    int m_viewMode = 0;  // 0=Game, 1=Editor, 2=Wireframe
};

} // namespace Vehement
