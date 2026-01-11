        if (ImGui::BeginMenu("File")) {
            if (ImGui::BeginMenu("New")) {
                if (ImGui::MenuItem("World Map", "Ctrl+Shift+N")) {
                    NewWorldMap();
                }
                if (ImGui::MenuItem("Local Map", "Ctrl+N")) {
                    m_showNewMapDialog = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Open Map", "Ctrl+O")) {
                m_showLoadMapDialog = true;
            }
            if (ImGui::MenuItem("Save Map", "Ctrl+S")) {
                if (!m_currentMapPath.empty()) {
                    SaveMap(m_currentMapPath);
                } else {
                    m_showSaveMapDialog = true;
                }
            }
            if (ImGui::MenuItem("Save Map As", "Ctrl+Shift+S")) {
                m_showSaveMapDialog = true;
            }
            ImGui::Separator();

            // Import/Export submenu
            if (ImGui::BeginMenu("Import")) {
                if (ImGui::MenuItem("Heightmap...")) {
                    std::string path = OpenNativeFileDialog("Image Files (*.png;*.jpg;*.tga;*.bmp)\0*.png;*.jpg;*.tga;*.bmp\0All Files\0*.*\0", "Import Heightmap");
                    if (!path.empty()) {
                        ImportHeightmap(path);
                    }
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Export")) {
                if (ImGui::MenuItem("Heightmap...")) {
                    std::string path = SaveNativeFileDialog("PNG Image (*.png)\0*.png\0All Files\0*.*\0", "Export Heightmap", ".png");
                    if (!path.empty()) {
                        ExportHeightmap(path);
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::Separator();

            // Recent files
            if (ImGui::BeginMenu("Recent Files")) {
                if (m_recentFiles.empty()) {
                    ImGui::MenuItem("(No recent files)", nullptr, false, false);
                } else {
                    for (const auto& recentFile : m_recentFiles) {
                        if (ImGui::MenuItem(recentFile.c_str())) {
                            LoadMap(recentFile);
                        }
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Clear Recent Files")) {
                        ClearRecentFiles();
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Exit Editor", "Alt+F4")) {
                Nova::Engine::Instance().Shutdown();
            }
            ImGui::EndMenu();
        }
