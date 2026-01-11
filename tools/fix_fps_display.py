#!/usr/bin/env python3
import re

file_path = "H:/Github/Old3DEngine/engine/core/Engine.cpp"

with open(file_path, 'r', encoding='utf-8') as f:
    content = f.read()

old_code = """        // Engine debug UI
        if (m_debugDrawEnabled) {
            ImGui::Begin("Engine Stats");
            ImGui::Text("FPS: %.1f", m_time->GetFPS());
            ImGui::Text("Frame Time: %.3f ms", m_time->GetDeltaTime() * 1000.0f);
            ImGui::Text("Total Time: %.2f s", m_time->GetTotalTime());
            ImGui::End();
        }"""

new_code = """        // Engine debug UI - FPS in top right corner
        if (m_debugDrawEnabled) {
            ImGuiIO& io = ImGui::GetIO();
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 100, 10), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.35f);
            ImGui::Begin("FPS", nullptr,
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("FPS: %.0f", m_time->GetFPS());
            ImGui::End();
        }"""

if old_code in content:
    content = content.replace(old_code, new_code)
    with open(file_path, 'w', encoding='utf-8') as f:
        f.write(content)
    print("FPS display updated successfully")
else:
    print("Could not find the code to replace")
