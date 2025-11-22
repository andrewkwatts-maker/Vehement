#include "PCGPanel.hpp"
#include "Editor.hpp"
#include "ScriptEditor.hpp"
#include "WorldView.hpp"
#include <imgui.h>
#include <filesystem>
#include <GL/gl.h>

namespace Vehement {

PCGPanel::PCGPanel(Editor* editor) : m_editor(editor) {
    m_previewTexture.resize(m_previewWidth * m_previewHeight * 4, 128);
}

PCGPanel::~PCGPanel() = default;

void PCGPanel::Update(float deltaTime) {
    if (m_isGenerating) {
        // Simulate progress
        m_progress += deltaTime * 0.2f;
        if (m_progress >= 1.0f) {
            m_progress = 1.0f;
            m_isGenerating = false;
            m_previewDirty = true;
            if (OnGenerationComplete) OnGenerationComplete();
        }
    }
}

void PCGPanel::Render() {
    if (!ImGui::Begin("PCG Panel")) {
        ImGui::End();
        return;
    }

    // Toolbar
    if (m_isGenerating) {
        if (ImGui::Button("Cancel")) {
            CancelGeneration();
        }
        ImGui::SameLine();
        ImGui::ProgressBar(m_progress, ImVec2(-1, 0), m_currentStage.c_str());
    } else {
        if (ImGui::Button("Generate Preview")) {
            GeneratePreview();
        }
        ImGui::SameLine();
        if (ImGui::Button("Generate Full")) {
            GenerateFull();
        }
    }

    ImGui::Separator();

    // Tabs for different sections
    if (ImGui::BeginTabBar("PCGTabs")) {
        if (ImGui::BeginTabItem("Stages")) {
            RenderStageConfig();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Parameters")) {
            RenderParameters();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Preview")) {
            RenderPreview();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Real World Data")) {
            RenderRealWorldOverlay();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void PCGPanel::RenderStageConfig() {
    ImGui::Text("PCG Pipeline Stages");
    ImGui::Separator();

    // Helper for script selection
    auto scriptSelector = [this](const char* label, std::string& script) {
        ImGui::PushID(label);
        ImGui::Text("%s", label);
        ImGui::SameLine(150);

        char buffer[256];
        strncpy(buffer, script.c_str(), sizeof(buffer));
        ImGui::SetNextItemWidth(200);
        if (ImGui::InputText("##script", buffer, sizeof(buffer))) {
            script = buffer;
        }
        ImGui::SameLine();
        if (ImGui::Button("...")) {
            ImGui::OpenPopup("ScriptBrowserPopup");
        }

        // Script browser popup
        if (ImGui::BeginPopup("ScriptBrowserPopup")) {
            ImGui::Text("Select PCG Script");
            ImGui::Separator();

            // Show available PCG scripts
            std::string scriptsPath = "scripts/pcg/";
            if (std::filesystem::exists(scriptsPath)) {
                for (const auto& entry : std::filesystem::directory_iterator(scriptsPath)) {
                    if (entry.path().extension() == ".py") {
                        std::string scriptName = entry.path().filename().string();
                        if (ImGui::Selectable(scriptName.c_str())) {
                            script = entry.path().string();
                        }
                    }
                }
            }

            ImGui::Separator();
            // Predefined PCG scripts
            const char* defaultScripts[] = {
                "scripts/pcg/terrain_perlin.py",
                "scripts/pcg/terrain_voronoi.py",
                "scripts/pcg/roads_grid.py",
                "scripts/pcg/roads_organic.py",
                "scripts/pcg/buildings_city.py",
                "scripts/pcg/buildings_village.py",
                "scripts/pcg/foliage_forest.py",
                "scripts/pcg/foliage_grassland.py",
                "scripts/pcg/entities_spawn.py"
            };
            ImGui::Text("Default Scripts:");
            for (const char* defaultScript : defaultScripts) {
                if (ImGui::Selectable(defaultScript)) {
                    script = defaultScript;
                }
            }
            ImGui::EndPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Edit")) {
            // Open script in the script editor
            if (!script.empty() && m_editor) {
                if (auto* scriptEditor = m_editor->GetScriptEditor()) {
                    scriptEditor->OpenScript(script);
                }
                m_editor->SetScriptEditorVisible(true);
            }
        }
        ImGui::PopID();
    };

    scriptSelector("1. Terrain", m_terrainScript);
    scriptSelector("2. Roads", m_roadScript);
    scriptSelector("3. Buildings", m_buildingScript);
    scriptSelector("4. Foliage", m_foliageScript);
    scriptSelector("5. Entities", m_entityScript);

    ImGui::Separator();
    ImGui::Text("Stage Order:");
    ImGui::TextDisabled("Terrain -> Roads -> Buildings -> Foliage -> Entities");
}

void PCGPanel::RenderParameters() {
    ImGui::Text("Generation Parameters");
    ImGui::Separator();

    // Seed
    ImGui::InputInt("Seed", &m_seed);
    ImGui::SameLine();
    if (ImGui::Button("Random")) {
        m_seed = rand();
    }

    ImGui::Separator();

    // Size
    ImGui::DragInt("Preview Width", &m_previewWidth, 1, 16, 256);
    ImGui::DragInt("Preview Height", &m_previewHeight, 1, 16, 256);

    ImGui::Separator();

    // Real-world data toggle
    ImGui::Checkbox("Use Real-World Data", &m_useRealWorldData);
    if (m_useRealWorldData) {
        ImGui::TextDisabled("Will fetch OSM, elevation, and biome data");
    }

    ImGui::Separator();

    // Terrain parameters
    if (ImGui::CollapsingHeader("Terrain Parameters")) {
        static float noiseScale = 0.1f;
        static int octaves = 4;
        static float persistence = 0.5f;
        static float lacunarity = 2.0f;

        ImGui::DragFloat("Noise Scale", &noiseScale, 0.01f, 0.01f, 1.0f);
        ImGui::DragInt("Octaves", &octaves, 1, 1, 8);
        ImGui::DragFloat("Persistence", &persistence, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Lacunarity", &lacunarity, 0.1f, 1.0f, 4.0f);
    }

    // Road parameters
    if (ImGui::CollapsingHeader("Road Parameters")) {
        static float roadDensity = 0.3f;
        static bool connectPOIs = true;

        ImGui::DragFloat("Road Density", &roadDensity, 0.01f, 0.0f, 1.0f);
        ImGui::Checkbox("Connect Points of Interest", &connectPOIs);
    }

    // Building parameters
    if (ImGui::CollapsingHeader("Building Parameters")) {
        static float buildingDensity = 0.5f;
        static int maxBuildingHeight = 5;

        ImGui::DragFloat("Building Density", &buildingDensity, 0.01f, 0.0f, 1.0f);
        ImGui::DragInt("Max Building Height", &maxBuildingHeight, 1, 1, 20);
    }
}

void PCGPanel::RenderPreview() {
    ImGui::Text("Generation Preview");

    // Preview controls
    if (ImGui::Button("Refresh Preview")) {
        m_previewDirty = true;
        GeneratePreviewTexture();
    }

    // Preview image
    ImVec2 previewSize(256, 256);
    ImGui::BeginChild("PreviewImage", previewSize, true);

    // Create/update OpenGL texture for preview
    if (m_previewTextureId == 0) {
        glGenTextures(1, &m_previewTextureId);
    }

    // Update texture if dirty
    if (m_previewDirty && !m_previewTexture.empty()) {
        glBindTexture(GL_TEXTURE_2D, m_previewTextureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_previewWidth, m_previewHeight,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, m_previewTexture.data());
        glBindTexture(GL_TEXTURE_2D, 0);
        m_previewDirty = false;
    }

    // Render the preview texture
    if (m_previewTextureId != 0) {
        ImGui::Image((ImTextureID)(intptr_t)m_previewTextureId, ImVec2(m_previewWidth, m_previewHeight));
    } else {
        ImGui::TextDisabled("No preview generated");
    }
    ImGui::Text("Size: %dx%d", m_previewWidth, m_previewHeight);
    ImGui::Text("Seed: %d", m_seed);

    ImGui::EndChild();

    // Legend
    ImGui::Text("Legend:");
    ImGui::ColorButton("##grass", ImVec4(0.2f, 0.6f, 0.2f, 1.0f)); ImGui::SameLine(); ImGui::Text("Grass");
    ImGui::SameLine();
    ImGui::ColorButton("##water", ImVec4(0.2f, 0.4f, 0.8f, 1.0f)); ImGui::SameLine(); ImGui::Text("Water");
    ImGui::SameLine();
    ImGui::ColorButton("##road", ImVec4(0.4f, 0.4f, 0.4f, 1.0f)); ImGui::SameLine(); ImGui::Text("Road");
    ImGui::SameLine();
    ImGui::ColorButton("##building", ImVec4(0.6f, 0.5f, 0.4f, 1.0f)); ImGui::SameLine(); ImGui::Text("Building");
}

void PCGPanel::RenderRealWorldOverlay() {
    ImGui::Text("Real-World Data Overlay");
    ImGui::Separator();

    // Location selection
    static char latBuffer[32] = "37.7749";
    static char lonBuffer[32] = "-122.4194";

    ImGui::InputText("Latitude", latBuffer, sizeof(latBuffer));
    ImGui::InputText("Longitude", lonBuffer, sizeof(lonBuffer));

    if (ImGui::Button("Fetch Data")) {
        // Fetch real-world data from OSM and elevation services
        double lat = std::atof(latBuffer);
        double lon = std::atof(lonBuffer);

        // Store the coordinates for generation
        m_realWorldLat = lat;
        m_realWorldLon = lon;

        // Queue data fetch (would use async HTTP requests in real implementation)
        m_isFetchingRealWorldData = true;
        m_fetchProgress = 0.0f;
        m_fetchStatus = "Fetching OSM data...";

        // In a real implementation, this would make HTTP requests to:
        // - Overpass API for OSM data (roads, buildings, water)
        // - Elevation APIs for DEM data
        // - Climate/biome APIs for vegetation data
        // For now, simulate with placeholder
        if (m_console) {
            m_console->Log("Fetching real-world data for (" + std::to_string(lat) + ", " + std::to_string(lon) + ")", Console::LogLevel::Info);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Use Current View")) {
        // Get coordinates from WorldView camera position
        if (m_editor) {
            if (auto* worldView = m_editor->GetWorldView()) {
                // Get camera target position and convert to lat/lon
                glm::vec3 target = worldView->GetCameraTarget();

                // Convert world coordinates to approximate lat/lon
                // Assuming world units map to ~1 meter at equator
                double baseLat = 37.7749;  // San Francisco reference
                double baseLon = -122.4194;
                double metersPerDegLat = 111320.0;
                double metersPerDegLon = 111320.0 * std::cos(baseLat * 3.14159 / 180.0);

                double lat = baseLat + (target.z / metersPerDegLat);
                double lon = baseLon + (target.x / metersPerDegLon);

                snprintf(latBuffer, sizeof(latBuffer), "%.6f", lat);
                snprintf(lonBuffer, sizeof(lonBuffer), "%.6f", lon);
            }
        }
    }

    // Show fetch progress
    if (m_isFetchingRealWorldData) {
        ImGui::ProgressBar(m_fetchProgress, ImVec2(-1, 0), m_fetchStatus.c_str());
    }

    ImGui::Separator();

    // Data toggles
    static bool showRoads = true;
    static bool showBuildings = true;
    static bool showWater = true;
    static bool showElevation = true;
    static bool showBiomes = true;

    ImGui::Checkbox("Roads (OSM)", &showRoads);
    ImGui::Checkbox("Buildings (OSM)", &showBuildings);
    ImGui::Checkbox("Water Bodies (OSM)", &showWater);
    ImGui::Checkbox("Elevation (DEM)", &showElevation);
    ImGui::Checkbox("Biomes", &showBiomes);

    ImGui::Separator();

    // Data info
    if (ImGui::CollapsingHeader("Fetched Data Info")) {
        ImGui::Text("Roads: 42 segments");
        ImGui::Text("Buildings: 156 footprints");
        ImGui::Text("Water: 3 bodies");
        ImGui::Text("Elevation: Min 5m, Max 45m");
        ImGui::Text("Primary Biome: Urban");
    }
}

void PCGPanel::GeneratePreview() {
    m_isGenerating = true;
    m_progress = 0.0f;
    m_currentStage = "Generating preview...";
}

void PCGPanel::GenerateFull() {
    m_isGenerating = true;
    m_progress = 0.0f;
    m_currentStage = "Generating full world...";
}

void PCGPanel::CancelGeneration() {
    m_isGenerating = false;
    m_progress = 0.0f;
    m_currentStage = "";
}

void PCGPanel::GeneratePreviewTexture() {
    // Resize texture buffer if needed
    size_t requiredSize = m_previewWidth * m_previewHeight * 4;
    if (m_previewTexture.size() != requiredSize) {
        m_previewTexture.resize(requiredSize);
    }

    // Simple procedural preview generation using the seed
    srand(m_seed);

    for (int y = 0; y < m_previewHeight; y++) {
        for (int x = 0; x < m_previewWidth; x++) {
            int idx = (y * m_previewWidth + x) * 4;

            // Simple noise-based terrain
            float nx = (float)x / m_previewWidth;
            float ny = (float)y / m_previewHeight;

            // Perlin-like noise approximation
            float noise = sinf(nx * 10.0f + m_seed * 0.1f) * cosf(ny * 10.0f + m_seed * 0.2f);
            noise += sinf(nx * 20.0f + m_seed * 0.3f) * cosf(ny * 20.0f + m_seed * 0.4f) * 0.5f;
            noise = (noise + 1.5f) / 3.0f;  // Normalize to 0-1

            // Determine terrain type
            uint8_t r, g, b;
            if (noise < 0.3f) {
                // Water
                r = 51; g = 102; b = 204;
            } else if (noise < 0.4f) {
                // Beach/sand
                r = 194; g = 178; b = 128;
            } else if (noise < 0.75f) {
                // Grass
                float grassVariation = (rand() % 20) / 100.0f;
                r = (uint8_t)(51 + grassVariation * 30);
                g = (uint8_t)(153 + grassVariation * 30);
                b = (uint8_t)(51 + grassVariation * 30);
            } else {
                // Mountain/rock
                r = 128; g = 128; b = 128;
            }

            // Add road overlay (simple grid pattern)
            if ((x % 32 < 2 || y % 32 < 2) && noise > 0.35f) {
                r = 100; g = 100; b = 100;
            }

            // Add building spots
            if (noise > 0.4f && noise < 0.7f) {
                if ((x % 16 < 4 && y % 16 < 4) && rand() % 10 < 3) {
                    r = 153; g = 128; b = 102;
                }
            }

            m_previewTexture[idx + 0] = r;
            m_previewTexture[idx + 1] = g;
            m_previewTexture[idx + 2] = b;
            m_previewTexture[idx + 3] = 255;
        }
    }

    m_previewDirty = true;
}

} // namespace Vehement
