#include "Toolbar.hpp"
#include "Editor.hpp"
#include <imgui.h>

namespace Vehement {

Toolbar::Toolbar(Editor* editor) : m_editor(editor) {}
Toolbar::~Toolbar() = default;

void Toolbar::Render() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                            ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings |
                            ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + 19));  // Below menu bar
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, 35));

    if (ImGui::Begin("##Toolbar", nullptr, flags)) {
        // Tool selection
        ImGui::Text("Tools:");
        ImGui::SameLine();

        const char* tools[] = {"Select", "Move", "Rotate", "Scale", "Paint"};
        for (int i = 0; i < 5; i++) {
            if (i > 0) ImGui::SameLine();

            bool selected = (m_currentTool == i);
            if (selected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
            }

            if (ImGui::Button(tools[i], ImVec2(60, 0))) {
                m_currentTool = i;
            }

            if (selected) {
                ImGui::PopStyleColor();
            }

            // Tooltips
            if (ImGui::IsItemHovered()) {
                const char* shortcuts[] = {"Q", "W", "E", "R", "B"};
                ImGui::SetTooltip("%s (%s)", tools[i], shortcuts[i]);
            }
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // Play/Pause/Stop
        ImGui::Text("Simulation:");
        ImGui::SameLine();

        if (!m_isPlaying) {
            if (ImGui::Button("Play", ImVec2(50, 0))) {
                m_isPlaying = true;
                m_isPaused = false;
                if (m_editor && m_editor->OnPlay) m_editor->OnPlay();
            }
        } else {
            if (ImGui::Button(m_isPaused ? "Resume" : "Pause", ImVec2(50, 0))) {
                m_isPaused = !m_isPaused;
                if (m_isPaused) {
                    if (m_editor && m_editor->OnPause) m_editor->OnPause();
                } else {
                    if (m_editor && m_editor->OnPlay) m_editor->OnPlay();
                }
            }
        }
        ImGui::SameLine();

        if (ImGui::Button("Stop", ImVec2(50, 0))) {
            m_isPlaying = false;
            m_isPaused = false;
            if (m_editor && m_editor->OnStop) m_editor->OnStop();
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // Snap settings
        ImGui::Checkbox("Snap", &m_snapEnabled);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60);
        ImGui::DragFloat("##snapsize", &m_snapSize, 0.1f, 0.1f, 10.0f, "%.1f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Snap Size");

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // View mode
        ImGui::Text("View:");
        ImGui::SameLine();
        const char* viewModes[] = {"Game", "Editor", "Wire"};
        ImGui::SetNextItemWidth(80);
        ImGui::Combo("##viewmode", &m_viewMode, viewModes, IM_ARRAYSIZE(viewModes));

        // Right side - coordinates
        ImGui::SameLine(ImGui::GetWindowWidth() - 250);
        ImGui::TextDisabled("Mouse: (0, 0, 0)");
        ImGui::SameLine();
        ImGui::TextDisabled("| Grid: %.1f", m_snapSize);
    }
    ImGui::End();

    ImGui::PopStyleVar(2);
}

} // namespace Vehement
