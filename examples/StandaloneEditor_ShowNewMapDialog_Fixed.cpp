// Fixed version of ShowNewMapDialog with spherical world support
// This should replace lines 1060-1086 in StandaloneEditor.cpp

void StandaloneEditor::ShowNewMapDialog() {
    ImGui::OpenPopup("New Map");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("New Map", &m_showNewMapDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        static int width = 64;
        static int height = 64;
        static int worldTypeIndex = 1;  // Default to Spherical
        static float planetRadius = 6371.0f;  // Default to Earth radius in km
        const char* worldTypes[] = { "Flat", "Spherical" };

        ImGui::Text("Map Properties");
        ImGui::Separator();

        // World Type Selection
        ImGui::Text("World Type:");
        if (ImGui::RadioButton("Flat", &worldTypeIndex, 0)) {
            // Switched to Flat
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Spherical", &worldTypeIndex, 1)) {
            // Switched to Spherical
        }

        ImGui::Spacing();

        // Show different options based on world type
        if (worldTypeIndex == 0) {
            // Flat world - show width/height
            ImGui::Text("Map Dimensions:");
            ImGui::InputInt("Width", &width);
            ImGui::InputInt("Height", &height);
        } else {
            // Spherical world - show radius and presets
            ImGui::Text("Spherical World Settings:");
            ImGui::SliderFloat("Planet Radius (km)", &planetRadius, 100.0f, 50000.0f, "%.1f");

            ImGui::Spacing();
            ImGui::Text("Presets:");
            if (ImGui::Button("Earth", ImVec2(80, 0))) {
                planetRadius = 6371.0f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Mars", ImVec2(80, 0))) {
                planetRadius = 3389.5f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Moon", ImVec2(80, 0))) {
                planetRadius = 1737.4f;
            }

            ImGui::Spacing();
            ImGui::TextWrapped("Creates a spherical world with latitude/longitude coordinates");
        }

        ImGui::Separator();

        if (ImGui::Button("Create", ImVec2(120, 0))) {
            // Set world type based on selection
            WorldType selectedWorldType = (worldTypeIndex == 0) ? WorldType::Flat : WorldType::Spherical;

            if (selectedWorldType == WorldType::Spherical) {
                // Store radius before creating world
                m_worldRadius = planetRadius;
                m_worldType = WorldType::Spherical;
                NewWorldMap();
                spdlog::info("Creating spherical world with radius {} km", planetRadius);
            } else {
                // Create flat world
                m_worldType = WorldType::Flat;
                NewLocalMap(width, height);
                spdlog::info("Creating flat world {}x{}", width, height);
            }

            m_showNewMapDialog = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showNewMapDialog = false;
        }

        ImGui::EndPopup();
    }
}
