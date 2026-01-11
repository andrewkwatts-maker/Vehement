// ============================================================================
// ADDITION TO SettingsMenu.cpp - RenderGraphicsSettings() method
// Insert this code after line 420, before "Advanced Settings" section
// ============================================================================

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Rendering Backend");
    ImGui::Spacing();

    // Rendering backend selector
    const char* backendNames[] = { "SDF Rasterizer", "Polygon Rasterizer", "Hybrid (SDF + Polygon)" };
    ImGui::Text("Backend:");
    ImGui::SameLine(200);
    if (ImGui::Combo("##RenderBackend", &m_graphics.renderBackend, backendNames, 3)) {
        MarkAsModified();
    }

    ImGui::SameLine();
    if (ImGui::Button("?##BackendHelp")) {
        ImGui::OpenPopup("BackendHelpPopup");
    }

    if (ImGui::BeginPopup("BackendHelpPopup")) {
        ImGui::Text("Rendering Backend Information:");
        ImGui::Separator();
        ImGui::BulletText("SDF Rasterizer: Uses compute shaders for raymarching");
        ImGui::BulletText("  - Best for smooth organic shapes");
        ImGui::BulletText("  - Works without RTX hardware");
        ImGui::BulletText("Polygon Rasterizer: Traditional OpenGL rasterization");
        ImGui::BulletText("  - Best for traditional mesh geometry");
        ImGui::BulletText("Hybrid: Combines both with Z-buffer interleaving");
        ImGui::BulletText("  - Best quality and flexibility");
        ImGui::EndPopup();
    }

    // Backend-specific settings
    bool backendSettingChanged = false;

    if (m_graphics.renderBackend == 0) { // SDF Rasterizer
        ImGui::Spacing();
        ImGui::Text("SDF Settings");
        ImGui::Indent();

        ImGui::Text("Tile Size:");
        ImGui::SameLine(200);
        const char* tileSizes[] = { "8x8", "16x16", "32x32" };
        int tileSizeIndex = m_graphics.sdfTileSize == 8 ? 0 : m_graphics.sdfTileSize == 16 ? 1 : 2;
        if (ImGui::Combo("##TileSize", &tileSizeIndex, tileSizes, 3)) {
            m_graphics.sdfTileSize = tileSizeIndex == 0 ? 8 : tileSizeIndex == 1 ? 16 : 32;
            backendSettingChanged = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Smaller tiles = better culling but more overhead");
        }

        ImGui::Text("Max Raymarch Steps:");
        ImGui::SameLine(200);
        if (ImGui::SliderInt("##RaymarchSteps", &m_graphics.maxRaymarchSteps, 64, 256)) {
            backendSettingChanged = true;
        }

        ImGui::Text("SDF Shadows:");
        ImGui::SameLine(200);
        if (ImGui::Checkbox("##SDFShadows", &m_graphics.sdfEnableShadows)) {
            backendSettingChanged = true;
        }

        ImGui::Text("SDF Ambient Occlusion:");
        ImGui::SameLine(200);
        if (ImGui::Checkbox("##SDFAO", &m_graphics.sdfEnableAO)) {
            backendSettingChanged = true;
        }

        if (m_graphics.sdfEnableAO) {
            ImGui::Text("AO Samples:");
            ImGui::SameLine(200);
            if (ImGui::SliderInt("##AOSamples", &m_graphics.sdfAOSamples, 2, 8)) {
                backendSettingChanged = true;
            }
        }

        ImGui::Unindent();
    } else if (m_graphics.renderBackend == 2) { // Hybrid
        ImGui::Spacing();
        ImGui::Text("Hybrid Settings");
        ImGui::Indent();

        ImGui::Text("Render Order:");
        ImGui::SameLine(200);
        const char* renderOrders[] = { "SDF First", "Polygon First", "Auto" };
        if (ImGui::Combo("##RenderOrder", &m_graphics.hybridRenderOrder, renderOrders, 3)) {
            backendSettingChanged = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("SDF First: Better early-Z rejection\nPolygon First: Better for polygon-heavy scenes\nAuto: Dynamically choose based on scene");
        }

        ImGui::Text("Depth Interleaving:");
        ImGui::SameLine(200);
        if (ImGui::Checkbox("##DepthInterleaving", &m_graphics.enableDepthInterleaving)) {
            backendSettingChanged = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Enable proper Z-buffer merging between SDF and polygon passes");
        }

        // Show SDF settings for hybrid mode
        ImGui::Spacing();
        ImGui::Text("SDF Quality:");
        ImGui::SameLine(200);
        const char* hybridTileSizes[] = { "8x8", "16x16", "32x32" };
        int hybridTileSizeIndex = m_graphics.sdfTileSize == 8 ? 0 : m_graphics.sdfTileSize == 16 ? 1 : 2;
        if (ImGui::Combo("##HybridTileSize", &hybridTileSizeIndex, hybridTileSizes, 3)) {
            m_graphics.sdfTileSize = hybridTileSizeIndex == 0 ? 8 : hybridTileSizeIndex == 1 ? 16 : 32;
            backendSettingChanged = true;
        }

        ImGui::Text("Max Raymarch Steps:");
        ImGui::SameLine(200);
        if (ImGui::SliderInt("##HybridRaymarchSteps", &m_graphics.maxRaymarchSteps, 64, 256)) {
            backendSettingChanged = true;
        }

        ImGui::Unindent();
    }

    if (backendSettingChanged) {
        m_graphics.qualityPreset = QualityPreset::Custom;
        MarkAsModified();
    }

    // Performance statistics display
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Performance Stats");
    ImGui::Spacing();

    if (ImGui::Button("Toggle Performance Overlay")) {
        m_graphics.showPerformanceOverlay = !m_graphics.showPerformanceOverlay;
        MarkAsModified();
    }

    ImGui::SameLine();
    ImGui::TextDisabled("(Press F5 to switch backends in-game)");

    // Display FPS comparison if available
    if (m_graphics.showPerformanceOverlay) {
        ImGui::Spacing();
        ImGui::BeginChild("PerfStats", ImVec2(0, 180), true);

        ImGui::Text("Backend Performance Comparison");
        ImGui::Separator();

        ImGui::Columns(5, "perfcolumns");
        ImGui::Separator();
        ImGui::Text("Backend"); ImGui::NextColumn();
        ImGui::Text("FPS"); ImGui::NextColumn();
        ImGui::Text("Frame Time"); ImGui::NextColumn();
        ImGui::Text("GPU Time"); ImGui::NextColumn();
        ImGui::Text("Objects"); ImGui::NextColumn();
        ImGui::Separator();

        // SDF stats (would be populated from actual performance data)
        ImGui::Text("SDF Rasterizer"); ImGui::NextColumn();
        ImGui::Text("--"); ImGui::NextColumn();
        ImGui::Text("-- ms"); ImGui::NextColumn();
        ImGui::Text("-- ms"); ImGui::NextColumn();
        ImGui::Text("-- SDF"); ImGui::NextColumn();

        // Polygon stats
        ImGui::Text("Polygon Rasterizer"); ImGui::NextColumn();
        ImGui::Text("--"); ImGui::NextColumn();
        ImGui::Text("-- ms"); ImGui::NextColumn();
        ImGui::Text("-- ms"); ImGui::NextColumn();
        ImGui::Text("-- tris"); ImGui::NextColumn();

        // Hybrid stats
        ImGui::Text("Hybrid"); ImGui::NextColumn();
        ImGui::Text("--"); ImGui::NextColumn();
        ImGui::Text("-- ms"); ImGui::NextColumn();
        ImGui::Text("-- ms"); ImGui::NextColumn();
        ImGui::Text("-- mixed"); ImGui::NextColumn();

        ImGui::Separator();
        ImGui::Columns(1);

        ImGui::Spacing();
        ImGui::Text("Tile Statistics:");
        ImGui::BulletText("Active Tiles: --");
        ImGui::BulletText("Culled Tiles: --");
        ImGui::BulletText("Compute Dispatches: --");
        ImGui::BulletText("Draw Calls: --");

        ImGui::EndChild();

        // Debug visualization options
        ImGui::Spacing();
        ImGui::Text("Debug Visualization:");
        ImGui::Checkbox("Show Tiles", &m_graphics.showTiles);
        ImGui::SameLine();
        ImGui::Checkbox("Show Depth Buffer", &m_graphics.showDepthBuffer);
    }

// ============================================================================
// ADDITION TO SettingsMenu.hpp - GraphicsSettings struct
// Add these members to the GraphicsSettings struct:
// ============================================================================

/*
    // Rendering backend settings
    int renderBackend = 1;                  // 0=SDF, 1=Polygon, 2=Hybrid
    int sdfTileSize = 16;                   // 8, 16, or 32
    int maxRaymarchSteps = 128;             // 64-256
    bool sdfEnableShadows = true;
    bool sdfEnableAO = true;
    int sdfAOSamples = 4;
    int hybridRenderOrder = 0;              // 0=SDF first, 1=Polygon first, 2=Auto
    bool enableDepthInterleaving = true;
    bool showPerformanceOverlay = false;
    bool showTiles = false;
    bool showDepthBuffer = false;
*/

// ============================================================================
// USAGE NOTES:
// ============================================================================
// 1. Add the struct members above to GraphicsSettings in SettingsMenu.hpp
// 2. Insert the UI code at line 420 in SettingsMenu.cpp
// 3. Initialize the new members in SettingsMenu::Initialize()
// 4. Save/load them in ApplySettings() and RevertSettings()
// 5. Connect to actual RenderBackend in your render loop
