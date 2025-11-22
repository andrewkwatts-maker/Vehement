#include "PCGPanel.hpp"
#include "Editor.hpp"
#include <imgui.h>

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
    auto scriptSelector = [](const char* label, std::string& script) {
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
            // TODO: Script browser
        }
        ImGui::SameLine();
        if (ImGui::Button("Edit")) {
            // TODO: Open script editor
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
    }

    // Preview image placeholder
    ImVec2 previewSize(256, 256);
    ImGui::BeginChild("PreviewImage", previewSize, true);

    // TODO: Render actual preview texture
    ImGui::TextDisabled("Preview will appear here");
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
        // TODO: Fetch real-world data
    }
    ImGui::SameLine();
    if (ImGui::Button("Use Current View")) {
        // TODO: Get coordinates from WorldView
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

} // namespace Vehement
