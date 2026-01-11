        if (ImGui::BeginMenu("Edit")) {
            bool canUndo = m_commandHistory && m_commandHistory->CanUndo();
            bool canRedo = m_commandHistory && m_commandHistory->CanRedo();

            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, canUndo)) {
                if (m_commandHistory) {
                    m_commandHistory->Undo();
                }
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, canRedo)) {
                if (m_commandHistory) {
                    m_commandHistory->Redo();
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "Ctrl+X", false, m_selectedObjectIndex >= 0)) {
                CopySelectedObjects();
                DeleteSelectedObjects();
            }
            if (ImGui::MenuItem("Copy", "Ctrl+C", false, m_selectedObjectIndex >= 0)) {
                CopySelectedObjects();
            }
            if (ImGui::MenuItem("Paste", "Ctrl+V", false, false)) {
                // Clipboard paste not implemented yet
                spdlog::warn("Paste not yet implemented");
            }
            if (ImGui::MenuItem("Delete", "Del", false, m_selectedObjectIndex >= 0)) {
                DeleteSelectedObjects();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Select All", "Ctrl+A")) {
                SelectAllObjects();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Map Properties")) {
                m_showMapPropertiesDialog = true;
            }
            if (ImGui::MenuItem("Preferences...")) {
                m_showSettingsDialog = true;
            }
            ImGui::EndMenu();
        }
