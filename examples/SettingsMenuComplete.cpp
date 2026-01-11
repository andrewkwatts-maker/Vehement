#include "SettingsMenuComplete.hpp"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cstdio>
#include <cstdarg>

namespace Nova {

// ============================================================================
// Helper UI Functions
// ============================================================================

namespace SettingsUI {

void BeginSettingGroup(const char* label) {
    ImGui::PushID(label);
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "%s", label);
    ImGui::Separator();
    ImGui::Spacing();
}

void EndSettingGroup() {
    ImGui::Spacing();
    ImGui::PopID();
}

void Separator() {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}

void HelpMarker(const char* desc) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void ColoredText(float r, float g, float b, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    ImGui::TextColored(ImVec4(r, g, b, 1.0f), "%s", buffer);
}

} // namespace SettingsUI

// ============================================================================
// SettingsMenuComplete Implementation
// ============================================================================

SettingsMenuComplete::SettingsMenuComplete() {
}

SettingsMenuComplete::~SettingsMenuComplete() {
}

void SettingsMenuComplete::Initialize() {
    // Load current settings from manager
    m_currentSettings = SettingsManager::Instance().GetSettings();
    m_originalSettings = m_currentSettings;

    // Initialize UI state from settings
    m_selectedBackend = static_cast<int>(m_currentSettings.rendering.backend);
    m_selectedGIMethod = static_cast<int>(m_currentSettings.lighting.giMethod);
    m_selectedLODQuality = static_cast<int>(m_currentSettings.lod.quality);
    m_selectedUpdateFreq = static_cast<int>(m_currentSettings.caching.lightCacheUpdate);

    ClearModifiedFlag();
    spdlog::info("SettingsMenuComplete initialized");
}

void SettingsMenuComplete::Render(bool* isOpen) {
    if (!isOpen || !*isOpen) return;

    // Update stats if callback is set
    if (m_statsCallback) {
        m_stats = m_statsCallback();
    }

    ImGui::SetNextWindowSize(ImVec2(1000, 700), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Nova3D Settings", isOpen, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    // Render menu bar
    RenderMenuBar();

    // Render tab bar
    RenderTabBar();

    ImGui::Separator();

    // Render content based on selected tab
    ImGui::BeginChild("SettingsContent", ImVec2(0, -50), false);
    {
        switch (m_currentTab) {
            case SettingsTab::Rendering:
                RenderRenderingTab();
                break;
            case SettingsTab::Lighting:
                RenderLightingTab();
                break;
            case SettingsTab::Materials:
                RenderMaterialsTab();
                break;
            case SettingsTab::LOD:
                RenderLODTab();
                break;
            case SettingsTab::Caching:
                RenderCachingTab();
                break;
            case SettingsTab::Performance:
                RenderPerformanceTab();
                break;
        }
    }
    ImGui::EndChild();

    ImGui::Separator();

    // Render control buttons
    RenderControlButtons();

    // Render validation errors if needed
    if (m_showValidationErrors) {
        ShowValidationErrors();
    }

    // Render confirmation dialogs
    if (m_showResetConfirm) {
        ImGui::OpenPopup("Reset to Default?");
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal("Reset to Default?", &m_showResetConfirm, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Reset all settings to default values?");
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "This will discard all custom settings!");
            ImGui::Spacing();

            if (ImGui::Button("Reset", ImVec2(120, 0))) {
                ResetToDefault();
                m_showResetConfirm = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                m_showResetConfirm = false;
            }

            ImGui::EndPopup();
        }
    }

    ImGui::End();
}

void SettingsMenuComplete::UpdateStats(const PerformanceStats& stats) {
    m_stats = stats;
}

// ============================================================================
// Menu Bar
// ============================================================================

void SettingsMenuComplete::RenderMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Presets")) {
            if (ImGui::MenuItem("Low (30 FPS)")) {
                ApplyPreset(QualityPreset::Low);
            }
            if (ImGui::MenuItem("Medium (60 FPS)")) {
                ApplyPreset(QualityPreset::Medium);
            }
            if (ImGui::MenuItem("High (60 FPS)")) {
                ApplyPreset(QualityPreset::High);
            }
            if (ImGui::MenuItem("Ultra (120 FPS)")) {
                ApplyPreset(QualityPreset::Ultra);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save Settings", "Ctrl+S")) {
                SaveSettings();
            }
            if (ImGui::MenuItem("Load Settings", "Ctrl+L")) {
                LoadSettings();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Reset to Default")) {
                m_showResetConfirm = true;
            }
            if (ImGui::MenuItem("Export Report")) {
                spdlog::info("Exporting settings report...");
                // TODO: Implement report export
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Show Preview", nullptr, &m_showPreview);
            ImGui::EndMenu();
        }

        // Show current preset
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Text("Preset: %s", QualityPresetToString(m_currentSettings.preset));

        ImGui::EndMenuBar();
    }
}

// ============================================================================
// Tab Bar
// ============================================================================

void SettingsMenuComplete::RenderTabBar() {
    if (ImGui::BeginTabBar("SettingsTabs", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("Rendering")) {
            m_currentTab = SettingsTab::Rendering;
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Lighting")) {
            m_currentTab = SettingsTab::Lighting;
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Materials")) {
            m_currentTab = SettingsTab::Materials;
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("LOD")) {
            m_currentTab = SettingsTab::LOD;
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Caching")) {
            m_currentTab = SettingsTab::Caching;
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Performance")) {
            m_currentTab = SettingsTab::Performance;
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

// ============================================================================
// Rendering Tab
// ============================================================================

void SettingsMenuComplete::RenderRenderingTab() {
    RenderRenderingBackend();
    SettingsUI::Separator();
    RenderSDFRasterizer();
    SettingsUI::Separator();
    RenderPolygonRasterizer();
    SettingsUI::Separator();
    RenderGPUDriven();
    SettingsUI::Separator();
    RenderAsyncCompute();
}

void SettingsMenuComplete::RenderRenderingBackend() {
    SettingsUI::BeginSettingGroup("Rendering Backend");

    const char* backends[] = {"SDF-First", "Polygon Only", "GPU-Driven", "Path Tracing"};
    RenderSettingRow("Backend:");
    if (ImGui::Combo("##Backend", &m_selectedBackend, backends, 4)) {
        m_currentSettings.rendering.backend = static_cast<RenderBackend>(m_selectedBackend);
        MarkAsModified();
    }
    SettingsUI::HelpMarker("Select the rendering backend. SDF-First provides hybrid rendering with SDFs and polygons.");

    RenderSettingRow("Resolution Scale:");
    if (ImGui::SliderInt("##ResScale", &m_currentSettings.rendering.resolutionScale, 25, 200, "%d%%")) {
        MarkAsModified();
    }
    RenderPerformanceIndicator(m_currentSettings.rendering.resolutionScale > 100 ? PerformanceImpact::High :
                               m_currentSettings.rendering.resolutionScale < 75 ? PerformanceImpact::Low : PerformanceImpact::Medium);

    RenderSettingRow("Target FPS:");
    if (ImGui::InputInt("##TargetFPS", &m_currentSettings.rendering.targetFPS)) {
        m_currentSettings.rendering.targetFPS = std::clamp(m_currentSettings.rendering.targetFPS, 30, 240);
        MarkAsModified();
    }

    RenderSettingRow("Enable Adaptive:");
    if (ImGui::Checkbox("##Adaptive", &m_currentSettings.rendering.enableAdaptive)) {
        MarkAsModified();
    }
    SettingsUI::HelpMarker("Automatically adjust quality to maintain target FPS");

    SettingsUI::EndSettingGroup();
}

void SettingsMenuComplete::RenderSDFRasterizer() {
    SettingsUI::BeginSettingGroup("SDF Rasterizer");

    const char* tileSizes[] = {"8x8", "16x16", "32x32", "64x64"};
    int tileIndex = m_currentSettings.rendering.sdfTileSize.x == 8 ? 0 :
                    m_currentSettings.rendering.sdfTileSize.x == 16 ? 1 :
                    m_currentSettings.rendering.sdfTileSize.x == 32 ? 2 : 3;

    RenderSettingRow("Tile Size:");
    if (ImGui::Combo("##TileSize", &tileIndex, tileSizes, 4)) {
        int size = (tileIndex == 0 ? 8 : tileIndex == 1 ? 16 : tileIndex == 2 ? 32 : 64);
        m_currentSettings.rendering.sdfTileSize = glm::ivec2(size, size);
        MarkAsModified();
    }

    RenderSettingRow("Max Raymarch Steps:");
    if (ImGui::SliderInt("##MaxSteps", &m_currentSettings.rendering.maxRaymarchSteps, 32, 512)) {
        MarkAsModified();
    }
    RenderPerformanceIndicator(m_currentSettings.rendering.maxRaymarchSteps > 200 ? PerformanceImpact::High : PerformanceImpact::Medium);

    RenderSettingRow("Enable Temporal:");
    if (ImGui::Checkbox("##Temporal", &m_currentSettings.rendering.enableTemporal)) {
        MarkAsModified();
    }
    SettingsUI::HelpMarker("Temporal reprojection for improved quality");

    RenderSettingRow("Enable Checkerboard:");
    if (ImGui::Checkbox("##Checkerboard", &m_currentSettings.rendering.enableCheckerboard)) {
        MarkAsModified();
    }
    SettingsUI::HelpMarker("Checkerboard rendering for 2x performance boost");

    SettingsUI::EndSettingGroup();
}

void SettingsMenuComplete::RenderPolygonRasterizer() {
    SettingsUI::BeginSettingGroup("Polygon Rasterizer");

    RenderSettingRow("Enable Instancing:");
    if (ImGui::Checkbox("##Instancing", &m_currentSettings.rendering.enableInstancing)) {
        MarkAsModified();
    }

    RenderSettingRow("Shadow Cascades:");
    if (ImGui::SliderInt("##Cascades", &m_currentSettings.rendering.shadowCascades, 1, 8)) {
        MarkAsModified();
    }
    RenderPerformanceIndicator(m_currentSettings.rendering.shadowCascades > 4 ? PerformanceImpact::High : PerformanceImpact::Medium);

    const char* msaaOptions[] = {"Off", "2x", "4x", "8x", "16x"};
    int msaaIndex = m_currentSettings.rendering.msaaSamples == 0 ? 0 :
                    m_currentSettings.rendering.msaaSamples == 2 ? 1 :
                    m_currentSettings.rendering.msaaSamples == 4 ? 2 :
                    m_currentSettings.rendering.msaaSamples == 8 ? 3 : 4;

    RenderSettingRow("MSAA Samples:");
    if (ImGui::Combo("##MSAA", &msaaIndex, msaaOptions, 5)) {
        m_currentSettings.rendering.msaaSamples = (msaaIndex == 0 ? 0 : msaaIndex == 1 ? 2 : msaaIndex == 2 ? 4 : msaaIndex == 3 ? 8 : 16);
        MarkAsModified();
    }
    RenderPerformanceIndicator(msaaIndex > 2 ? PerformanceImpact::High : PerformanceImpact::Medium);

    SettingsUI::EndSettingGroup();
}

void SettingsMenuComplete::RenderGPUDriven() {
    SettingsUI::BeginSettingGroup("GPU-Driven Rendering");

    RenderSettingRow("Enable GPU Culling:");
    if (ImGui::Checkbox("##GPUCulling", &m_currentSettings.rendering.enableGPUCulling)) {
        MarkAsModified();
    }

    RenderSettingRow("Job Size:");
    if (ImGui::InputInt("##JobSize", &m_currentSettings.rendering.gpuCullingJobSize)) {
        m_currentSettings.rendering.gpuCullingJobSize = std::clamp(m_currentSettings.rendering.gpuCullingJobSize, 64, 1024);
        MarkAsModified();
    }

    RenderSettingRow("Persistent Buffers:");
    if (ImGui::Checkbox("##PersistentBuffers", &m_currentSettings.rendering.enablePersistentBuffers)) {
        MarkAsModified();
    }

    SettingsUI::EndSettingGroup();
}

void SettingsMenuComplete::RenderAsyncCompute() {
    SettingsUI::BeginSettingGroup("Async Compute");

    RenderSettingRow("Enable:");
    if (ImGui::Checkbox("##AsyncEnable", &m_currentSettings.rendering.enableAsyncCompute)) {
        MarkAsModified();
    }

    RenderSettingRow("Overlap:");
    if (ImGui::SliderInt("##Overlap", &m_currentSettings.rendering.asyncComputeOverlap, 0, 100, "%d%%")) {
        MarkAsModified();
    }

    SettingsUI::EndSettingGroup();
}

// ============================================================================
// Lighting Tab
// ============================================================================

void SettingsMenuComplete::RenderLightingTab() {
    RenderClusteredLighting();
    SettingsUI::Separator();
    RenderShadowSettings();
    SettingsUI::Separator();
    RenderGlobalIllumination();
    SettingsUI::Separator();
    RenderLightTypes();
}

void SettingsMenuComplete::RenderClusteredLighting() {
    SettingsUI::BeginSettingGroup("Clustered Lighting");

    RenderSettingRow("Max Lights:");
    if (ImGui::InputInt("##MaxLights", &m_currentSettings.lighting.maxLights)) {
        m_currentSettings.lighting.maxLights = std::clamp(m_currentSettings.lighting.maxLights, 100, 1000000);
        MarkAsModified();
    }
    RenderPerformanceIndicator(m_currentSettings.lighting.maxLights > 100000 ? PerformanceImpact::High : PerformanceImpact::Medium);

    RenderSettingRow("Cluster Grid:");
    int grid[3] = {m_currentSettings.lighting.clusterGrid.x, m_currentSettings.lighting.clusterGrid.y, m_currentSettings.lighting.clusterGrid.z};
    if (ImGui::InputInt3("##ClusterGrid", grid)) {
        m_currentSettings.lighting.clusterGrid = glm::ivec3(
            std::clamp(grid[0], 8, 64),
            std::clamp(grid[1], 8, 64),
            std::clamp(grid[2], 8, 64)
        );
        MarkAsModified();
    }

    RenderSettingRow("Lights/Cluster:");
    if (ImGui::InputInt("##LightsPerCluster", &m_currentSettings.lighting.maxLightsPerCluster)) {
        m_currentSettings.lighting.maxLightsPerCluster = std::clamp(m_currentSettings.lighting.maxLightsPerCluster, 64, 4096);
        MarkAsModified();
    }

    RenderSettingRow("Enable Overflow:");
    if (ImGui::Checkbox("##Overflow", &m_currentSettings.lighting.enableOverflowHandling)) {
        MarkAsModified();
    }

    SettingsUI::EndSettingGroup();
}

void SettingsMenuComplete::RenderShadowSettings() {
    SettingsUI::BeginSettingGroup("Shadow Settings");

    const char* atlasSizes[] = {"2048x2048", "4096x4096", "8192x8192", "16384x16384", "32768x32768"};
    int atlasIndex = m_currentSettings.lighting.shadowAtlasSize.x == 2048 ? 0 :
                     m_currentSettings.lighting.shadowAtlasSize.x == 4096 ? 1 :
                     m_currentSettings.lighting.shadowAtlasSize.x == 8192 ? 2 :
                     m_currentSettings.lighting.shadowAtlasSize.x == 16384 ? 3 : 4;

    RenderSettingRow("Shadow Atlas Size:");
    if (ImGui::Combo("##AtlasSize", &atlasIndex, atlasSizes, 5)) {
        int size = (atlasIndex == 0 ? 2048 : atlasIndex == 1 ? 4096 : atlasIndex == 2 ? 8192 : atlasIndex == 3 ? 16384 : 32768);
        m_currentSettings.lighting.shadowAtlasSize = glm::ivec2(size, size);
        MarkAsModified();
    }
    RenderPerformanceIndicator(atlasIndex > 3 ? PerformanceImpact::High : atlasIndex > 1 ? PerformanceImpact::Medium : PerformanceImpact::Low);

    RenderSettingRow("Shadow Maps:");
    if (ImGui::InputInt("##ShadowMaps", &m_currentSettings.lighting.maxShadowMaps)) {
        m_currentSettings.lighting.maxShadowMaps = std::clamp(m_currentSettings.lighting.maxShadowMaps, 16, 1024);
        MarkAsModified();
    }

    RenderSettingRow("Cascade Splits:");
    ImGui::Text("  [");
    ImGui::SameLine();
    for (size_t i = 0; i < m_currentSettings.lighting.cascadeSplits.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        ImGui::SetNextItemWidth(60);
        if (ImGui::InputFloat("##Split", &m_currentSettings.lighting.cascadeSplits[i], 0.0f, 0.0f, "%.2f")) {
            MarkAsModified();
        }
        if (i < m_currentSettings.lighting.cascadeSplits.size() - 1) {
            ImGui::SameLine();
            ImGui::Text(",");
            ImGui::SameLine();
        }
        ImGui::PopID();
    }
    ImGui::SameLine();
    ImGui::Text("]");

    RenderSettingRow("Soft Shadow Samples:");
    if (ImGui::SliderInt("##SoftShadows", &m_currentSettings.lighting.softShadowSamples, 1, 64)) {
        MarkAsModified();
    }
    RenderPerformanceIndicator(m_currentSettings.lighting.softShadowSamples > 16 ? PerformanceImpact::High : PerformanceImpact::Medium);

    SettingsUI::EndSettingGroup();
}

void SettingsMenuComplete::RenderGlobalIllumination() {
    SettingsUI::BeginSettingGroup("Global Illumination");

    const char* giMethods[] = {"None", "ReSTIR", "SVGF", "ReSTIR+SVGF"};
    RenderSettingRow("GI Method:");
    if (ImGui::Combo("##GIMethod", &m_selectedGIMethod, giMethods, 4)) {
        m_currentSettings.lighting.giMethod = static_cast<GIMethod>(m_selectedGIMethod);
        MarkAsModified();
    }
    RenderPerformanceIndicator(m_selectedGIMethod == 0 ? PerformanceImpact::None :
                               m_selectedGIMethod == 3 ? PerformanceImpact::High : PerformanceImpact::Medium);

    if (m_selectedGIMethod != 0) {
        RenderSettingRow("Samples/Pixel:");
        if (ImGui::SliderInt("##GISamples", &m_currentSettings.lighting.giSamplesPerPixel, 1, 8)) {
            MarkAsModified();
        }

        if (m_selectedGIMethod == 1 || m_selectedGIMethod == 3) {
            RenderSettingRow("ReSTIR Reuse:");
            if (ImGui::SliderInt("##ReSTIRReuse", &m_currentSettings.lighting.restirReusePercent, 0, 100, "%d%%")) {
                MarkAsModified();
            }
        }

        if (m_selectedGIMethod == 2 || m_selectedGIMethod == 3) {
            RenderSettingRow("SVGF Iterations:");
            if (ImGui::SliderInt("##SVGFIter", &m_currentSettings.lighting.svgfIterations, 1, 10)) {
                MarkAsModified();
            }
        }
    }

    SettingsUI::EndSettingGroup();
}

void SettingsMenuComplete::RenderLightTypes() {
    SettingsUI::BeginSettingGroup("Light Types");

    RenderSettingRow("Enable Point:");
    if (ImGui::Checkbox("##Point", &m_currentSettings.lighting.enablePointLights)) {
        MarkAsModified();
    }

    RenderSettingRow("Enable Spot:");
    if (ImGui::Checkbox("##Spot", &m_currentSettings.lighting.enableSpotLights)) {
        MarkAsModified();
    }

    RenderSettingRow("Enable Directional:");
    if (ImGui::Checkbox("##Directional", &m_currentSettings.lighting.enableDirectionalLights)) {
        MarkAsModified();
    }

    RenderSettingRow("Enable Area:");
    if (ImGui::Checkbox("##Area", &m_currentSettings.lighting.enableAreaLights)) {
        MarkAsModified();
    }

    RenderSettingRow("Enable Emissive Geo:");
    if (ImGui::Checkbox("##Emissive", &m_currentSettings.lighting.enableEmissiveGeometry)) {
        MarkAsModified();
    }

    SettingsUI::EndSettingGroup();
}

// ============================================================================
// Materials Tab
// ============================================================================

void SettingsMenuComplete::RenderMaterialsTab() {
    RenderPhysicalProperties();
    SettingsUI::Separator();
    RenderTextureQuality();
    SettingsUI::Separator();
    RenderShaderCompilation();
}

void SettingsMenuComplete::RenderPhysicalProperties() {
    SettingsUI::BeginSettingGroup("Physical Properties");

    RenderSettingRow("Enable IOR:");
    if (ImGui::Checkbox("##IOR", &m_currentSettings.materials.enableIOR)) {
        MarkAsModified();
    }
    SettingsUI::HelpMarker("Index of refraction for realistic glass and water");

    RenderSettingRow("Enable Dispersion:");
    if (ImGui::Checkbox("##Dispersion", &m_currentSettings.materials.enableDispersion)) {
        MarkAsModified();
    }
    SettingsUI::HelpMarker("Chromatic dispersion for prisms and glass");

    RenderSettingRow("Enable Scattering:");
    if (ImGui::Checkbox("##Scattering", &m_currentSettings.materials.enableSubsurfaceScattering)) {
        MarkAsModified();
    }
    SettingsUI::HelpMarker("Subsurface scattering for skin, wax, marble");

    RenderSettingRow("Enable Blackbody:");
    if (ImGui::Checkbox("##Blackbody", &m_currentSettings.materials.enableBlackbodyEmission)) {
        MarkAsModified();
    }
    SettingsUI::HelpMarker("Physically-based emission based on temperature");

    SettingsUI::EndSettingGroup();
}

void SettingsMenuComplete::RenderTextureQuality() {
    SettingsUI::BeginSettingGroup("Texture Quality");

    const char* texSizes[] = {"512", "1024", "2048", "4096", "8192", "16384"};
    int texIndex = m_currentSettings.materials.maxTextureSize == 512 ? 0 :
                   m_currentSettings.materials.maxTextureSize == 1024 ? 1 :
                   m_currentSettings.materials.maxTextureSize == 2048 ? 2 :
                   m_currentSettings.materials.maxTextureSize == 4096 ? 3 :
                   m_currentSettings.materials.maxTextureSize == 8192 ? 4 : 5;

    RenderSettingRow("Max Texture Size:");
    if (ImGui::Combo("##MaxTexSize", &texIndex, texSizes, 6)) {
        m_currentSettings.materials.maxTextureSize = (texIndex == 0 ? 512 : texIndex == 1 ? 1024 : texIndex == 2 ? 2048 :
                                                      texIndex == 3 ? 4096 : texIndex == 4 ? 8192 : 16384);
        MarkAsModified();
    }

    const char* anisoLevels[] = {"1x", "2x", "4x", "8x", "16x"};
    int anisoIndex = m_currentSettings.materials.anisotropicFiltering == 1 ? 0 :
                     m_currentSettings.materials.anisotropicFiltering == 2 ? 1 :
                     m_currentSettings.materials.anisotropicFiltering == 4 ? 2 :
                     m_currentSettings.materials.anisotropicFiltering == 8 ? 3 : 4;

    RenderSettingRow("Anisotropic Filtering:");
    if (ImGui::Combo("##Aniso", &anisoIndex, anisoLevels, 5)) {
        m_currentSettings.materials.anisotropicFiltering = (anisoIndex == 0 ? 1 : anisoIndex == 1 ? 2 : anisoIndex == 2 ? 4 :
                                                            anisoIndex == 3 ? 8 : 16);
        MarkAsModified();
    }

    RenderSettingRow("Mipmap Bias:");
    if (ImGui::SliderFloat("##MipBias", &m_currentSettings.materials.mipmapBias, -2.0f, 2.0f)) {
        MarkAsModified();
    }

    SettingsUI::EndSettingGroup();
}

void SettingsMenuComplete::RenderShaderCompilation() {
    SettingsUI::BeginSettingGroup("Shader Compilation");

    RenderSettingRow("Optimize Shaders:");
    if (ImGui::Checkbox("##OptimizeShaders", &m_currentSettings.materials.optimizeShaders)) {
        MarkAsModified();
    }

    RenderSettingRow("Cache Shaders:");
    if (ImGui::Checkbox("##CacheShaders", &m_currentSettings.materials.cacheShaders)) {
        MarkAsModified();
    }

    RenderSettingRow("Debug Info:");
    if (ImGui::Checkbox("##DebugInfo", &m_currentSettings.materials.includeDebugInfo)) {
        MarkAsModified();
    }

    SettingsUI::EndSettingGroup();
}

// ============================================================================
// LOD Tab
// ============================================================================

void SettingsMenuComplete::RenderLODTab() {
    RenderGlobalLOD();
    SettingsUI::Separator();
    RenderDistanceThresholds();
    SettingsUI::Separator();
    RenderTransitionSettings();
    SettingsUI::Separator();
    RenderPerTypeSettings();
}

void SettingsMenuComplete::RenderGlobalLOD() {
    SettingsUI::BeginSettingGroup("Global LOD");

    const char* lodQualities[] = {"Very Low", "Low", "Medium", "High", "Very High"};
    RenderSettingRow("LOD Quality:");
    if (ImGui::Combo("##LODQuality", &m_selectedLODQuality, lodQualities, 5)) {
        m_currentSettings.lod.quality = static_cast<LODQuality>(m_selectedLODQuality);
        MarkAsModified();
    }

    RenderSettingRow("LOD Bias:");
    if (ImGui::SliderFloat("##LODBias", &m_currentSettings.lod.lodBias, -2.0f, 2.0f)) {
        MarkAsModified();
    }
    SettingsUI::HelpMarker("Positive values increase detail, negative values reduce detail");

    SettingsUI::EndSettingGroup();
}

void SettingsMenuComplete::RenderDistanceThresholds() {
    SettingsUI::BeginSettingGroup("Distance Thresholds");

    if (m_currentSettings.lod.lodDistances.size() >= 4) {
        RenderSettingRow("LOD 0 (Full):");
        if (ImGui::InputFloat("##LOD0", &m_currentSettings.lod.lodDistances[0])) {
            MarkAsModified();
        }
        ImGui::SameLine();
        ImGui::Text("m");

        RenderSettingRow("LOD 1 (High):");
        if (ImGui::InputFloat("##LOD1", &m_currentSettings.lod.lodDistances[1])) {
            MarkAsModified();
        }
        ImGui::SameLine();
        ImGui::Text("m");

        RenderSettingRow("LOD 2 (Medium):");
        if (ImGui::InputFloat("##LOD2", &m_currentSettings.lod.lodDistances[2])) {
            MarkAsModified();
        }
        ImGui::SameLine();
        ImGui::Text("m");

        RenderSettingRow("LOD 3 (Low):");
        if (ImGui::InputFloat("##LOD3", &m_currentSettings.lod.lodDistances[3])) {
            MarkAsModified();
        }
        ImGui::SameLine();
        ImGui::Text("m");
    }

    RenderSettingRow("Culling:");
    if (ImGui::InputFloat("##Culling", &m_currentSettings.lod.cullingDistance)) {
        MarkAsModified();
    }
    ImGui::SameLine();
    ImGui::Text("m");

    SettingsUI::EndSettingGroup();
}

void SettingsMenuComplete::RenderTransitionSettings() {
    SettingsUI::BeginSettingGroup("Transition Settings");

    RenderSettingRow("Enable Dithering:");
    if (ImGui::Checkbox("##Dithering", &m_currentSettings.lod.enableDithering)) {
        MarkAsModified();
    }
    SettingsUI::HelpMarker("Smooth LOD transitions using dithering");

    RenderSettingRow("Transition Width:");
    if (ImGui::SliderFloat("##TransWidth", &m_currentSettings.lod.transitionWidth, 0.0f, 20.0f, "%.1f m")) {
        MarkAsModified();
    }

    RenderSettingRow("Hysteresis:");
    if (ImGui::SliderInt("##Hysteresis", &m_currentSettings.lod.hysteresisPercent, 0, 50, "%d%%")) {
        MarkAsModified();
    }
    SettingsUI::HelpMarker("Prevents LOD popping when camera moves back and forth");

    SettingsUI::EndSettingGroup();
}

void SettingsMenuComplete::RenderPerTypeSettings() {
    SettingsUI::BeginSettingGroup("Per-Type Settings");

    if (ImGui::TreeNode("Buildings LOD")) {
        RenderSettingRow("Use Custom:");
        if (ImGui::Checkbox("##BuildingsCustom", &m_currentSettings.lod.buildings.useCustom)) {
            MarkAsModified();
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Units LOD")) {
        RenderSettingRow("Use Custom:");
        if (ImGui::Checkbox("##UnitsCustom", &m_currentSettings.lod.units.useCustom)) {
            MarkAsModified();
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Terrain LOD")) {
        RenderSettingRow("Use Custom:");
        if (ImGui::Checkbox("##TerrainCustom", &m_currentSettings.lod.terrain.useCustom)) {
            MarkAsModified();
        }
        ImGui::TreePop();
    }

    SettingsUI::EndSettingGroup();
}

// ============================================================================
// Caching Tab
// ============================================================================

void SettingsMenuComplete::RenderCachingTab() {
    RenderSDFBrickCache();
    SettingsUI::Separator();
    RenderShaderCache();
    SettingsUI::Separator();
    RenderLightCache();
}

void SettingsMenuComplete::RenderSDFBrickCache() {
    SettingsUI::BeginSettingGroup("SDF Brick Cache");

    RenderSettingRow("Enable:");
    if (ImGui::Checkbox("##BrickCacheEnable", &m_currentSettings.caching.enableBrickCache)) {
        MarkAsModified();
    }

    const char* atlasSizes[] = {"16x16x16", "24x24x24", "32x32x32", "48x48x48", "64x64x64"};
    int atlasIndex = m_currentSettings.caching.brickAtlasSize.x == 16 ? 0 :
                     m_currentSettings.caching.brickAtlasSize.x == 24 ? 1 :
                     m_currentSettings.caching.brickAtlasSize.x == 32 ? 2 :
                     m_currentSettings.caching.brickAtlasSize.x == 48 ? 3 : 4;

    RenderSettingRow("Atlas Size:");
    if (ImGui::Combo("##AtlasSize3D", &atlasIndex, atlasSizes, 5)) {
        int size = (atlasIndex == 0 ? 16 : atlasIndex == 1 ? 24 : atlasIndex == 2 ? 32 : atlasIndex == 3 ? 48 : 64);
        m_currentSettings.caching.brickAtlasSize = glm::ivec3(size, size, size);
        MarkAsModified();
    }

    RenderSettingRow("Max Memory:");
    if (ImGui::InputInt("##MaxCacheMem", &m_currentSettings.caching.maxCacheMemoryMB)) {
        m_currentSettings.caching.maxCacheMemoryMB = std::clamp(m_currentSettings.caching.maxCacheMemoryMB, 64, 8192);
        MarkAsModified();
    }
    ImGui::SameLine();
    ImGui::Text("MB");

    RenderSettingRow("Deduplication:");
    if (ImGui::Checkbox("##Dedup", &m_currentSettings.caching.enableDeduplication)) {
        MarkAsModified();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Cache statistics
    ImGui::Text("Current Usage:");
    ImGui::SameLine(200);
    ImGui::Text("%d MB / %d MB", m_stats.cacheMemoryUsedMB, m_stats.cacheMemoryTotalMB);

    ImGui::Text("Bricks Cached:");
    ImGui::SameLine(200);
    ImGui::Text("%d / %d", m_stats.bricksCached, m_stats.bricksTotal);

    if (m_currentSettings.caching.enableDeduplication) {
        ImGui::Text("Dedup Savings:");
        ImGui::SameLine(200);
        ImGui::Text("%d%%", m_stats.dedupSavingsPercent);
    }

    ImGui::Spacing();
    if (ImGui::Button("Clear Cache")) {
        spdlog::info("Clearing SDF brick cache...");
        // TODO: Call cache clear function
    }

    SettingsUI::EndSettingGroup();
}

void SettingsMenuComplete::RenderShaderCache() {
    SettingsUI::BeginSettingGroup("Shader Cache");

    RenderSettingRow("Enable:");
    if (ImGui::Checkbox("##ShaderCacheEnable", &m_currentSettings.caching.enableShaderCache)) {
        MarkAsModified();
    }

    RenderSettingRow("Cache Path:");
    char pathBuffer[256];
    strncpy(pathBuffer, m_currentSettings.caching.shaderCachePath.c_str(), sizeof(pathBuffer) - 1);
    pathBuffer[sizeof(pathBuffer) - 1] = '\0';
    if (ImGui::InputText("##ShaderPath", pathBuffer, sizeof(pathBuffer))) {
        m_currentSettings.caching.shaderCachePath = pathBuffer;
        MarkAsModified();
    }

    RenderSettingRow("Max Shaders:");
    if (ImGui::InputInt("##MaxShaders", &m_currentSettings.caching.maxCachedShaders)) {
        m_currentSettings.caching.maxCachedShaders = std::clamp(m_currentSettings.caching.maxCachedShaders, 64, 10000);
        MarkAsModified();
    }

    ImGui::Spacing();
    if (ImGui::Button("Rebuild All Shaders")) {
        spdlog::info("Rebuilding all shaders...");
        // TODO: Call shader rebuild function
    }

    SettingsUI::EndSettingGroup();
}

void SettingsMenuComplete::RenderLightCache() {
    SettingsUI::BeginSettingGroup("Light Cache");

    RenderSettingRow("Enable Caching:");
    if (ImGui::Checkbox("##LightCacheEnable", &m_currentSettings.caching.enableLightCache)) {
        MarkAsModified();
    }

    const char* updateFreqs[] = {"Per Frame", "On Change", "Manual"};
    RenderSettingRow("Update Frequency:");
    if (ImGui::Combo("##UpdateFreq", &m_selectedUpdateFreq, updateFreqs, 3)) {
        m_currentSettings.caching.lightCacheUpdate = static_cast<CachingSettings::UpdateFrequency>(m_selectedUpdateFreq);
        MarkAsModified();
    }

    RenderSettingRow("Static Light Cache:");
    if (ImGui::Checkbox("##StaticCache", &m_currentSettings.caching.enableStaticLightCache)) {
        MarkAsModified();
    }

    SettingsUI::EndSettingGroup();
}

// ============================================================================
// Performance Tab
// ============================================================================

void SettingsMenuComplete::RenderPerformanceTab() {
    RenderThreadPool();
    SettingsUI::Separator();
    RenderMemorySettings();
    SettingsUI::Separator();
    RenderProfilingSettings();
    SettingsUI::Separator();
    RenderCurrentStats();
}

void SettingsMenuComplete::RenderThreadPool() {
    SettingsUI::BeginSettingGroup("Thread Pool");

    RenderSettingRow("Worker Threads:");
    if (ImGui::InputInt("##WorkerThreads", &m_currentSettings.performance.workerThreads)) {
        m_currentSettings.performance.workerThreads = std::clamp(m_currentSettings.performance.workerThreads, 1, 64);
        MarkAsModified();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(detected: %d)", std::thread::hardware_concurrency());

    RenderSettingRow("Job Queue Size:");
    if (ImGui::InputInt("##JobQueueSize", &m_currentSettings.performance.jobQueueSize)) {
        m_currentSettings.performance.jobQueueSize = std::clamp(m_currentSettings.performance.jobQueueSize, 64, 16384);
        MarkAsModified();
    }

    SettingsUI::EndSettingGroup();
}

void SettingsMenuComplete::RenderMemorySettings() {
    SettingsUI::BeginSettingGroup("Memory");

    RenderSettingRow("GPU Memory Limit:");
    if (ImGui::InputInt("##GPUMemLimit", &m_currentSettings.performance.gpuMemoryLimitMB)) {
        m_currentSettings.performance.gpuMemoryLimitMB = std::clamp(m_currentSettings.performance.gpuMemoryLimitMB, 0, 32768);
        MarkAsModified();
    }
    ImGui::SameLine();
    if (m_currentSettings.performance.gpuMemoryLimitMB == 0) {
        ImGui::Text("(Auto)");
    } else {
        ImGui::Text("MB");
    }

    RenderSettingRow("Streaming Budget:");
    if (ImGui::InputInt("##StreamBudget", &m_currentSettings.performance.streamingBudgetMB)) {
        m_currentSettings.performance.streamingBudgetMB = std::clamp(m_currentSettings.performance.streamingBudgetMB, 128, 8192);
        MarkAsModified();
    }
    ImGui::SameLine();
    ImGui::Text("MB");

    SettingsUI::EndSettingGroup();
}

void SettingsMenuComplete::RenderProfilingSettings() {
    SettingsUI::BeginSettingGroup("Profiling");

    RenderSettingRow("Enable Profiler:");
    if (ImGui::Checkbox("##EnableProfiler", &m_currentSettings.performance.enableProfiler)) {
        MarkAsModified();
    }

    RenderSettingRow("Show Overlay:");
    if (ImGui::Checkbox("##ShowOverlay", &m_currentSettings.performance.showProfilerOverlay)) {
        MarkAsModified();
    }

    RenderSettingRow("Export CSV:");
    if (ImGui::Checkbox("##ExportCSV", &m_currentSettings.performance.exportCSV)) {
        MarkAsModified();
    }

    SettingsUI::EndSettingGroup();
}

void SettingsMenuComplete::RenderCurrentStats() {
    SettingsUI::BeginSettingGroup("Current Stats");

    ImGui::Text("FPS:");
    ImGui::SameLine(200);
    ImGui::Text("%.1f", m_stats.fps);

    ImGui::Text("Frame Time:");
    ImGui::SameLine(200);
    ImGui::Text("%.2f ms", m_stats.frameTimeMs);

    ImGui::Text("Culling:");
    ImGui::SameLine(200);
    ImGui::Text("%.2f ms", m_stats.cullingTimeMs);

    ImGui::Text("Lighting:");
    ImGui::SameLine(200);
    ImGui::Text("%.2f ms", m_stats.lightingTimeMs);

    ImGui::Text("Rendering:");
    ImGui::SameLine(200);
    ImGui::Text("%.2f ms", m_stats.renderingTimeMs);

    ImGui::Text("GPU Memory:");
    ImGui::SameLine(200);
    if (m_stats.gpuMemoryTotalMB > 0) {
        ImGui::Text("%d MB / %d MB", m_stats.gpuMemoryUsedMB, m_stats.gpuMemoryTotalMB);
    } else {
        ImGui::Text("%d MB", m_stats.gpuMemoryUsedMB);
    }

    ImGui::Spacing();
    if (ImGui::Button("Export Report")) {
        spdlog::info("Exporting performance report...");
        // TODO: Implement report export
    }

    SettingsUI::EndSettingGroup();
}

// ============================================================================
// Control Buttons
// ============================================================================

void SettingsMenuComplete::RenderControlButtons() {
    float buttonWidth = 120.0f;
    float spacing = 10.0f;
    float totalWidth = buttonWidth * 3 + spacing * 2;
    float offsetX = (ImGui::GetContentRegionAvail().x - totalWidth) * 0.5f;

    if (offsetX > 0) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
    }

    if (ImGui::Button("Apply", ImVec2(buttonWidth, 0))) {
        ApplyChanges();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Apply settings without saving");
    }

    ImGui::SameLine();

    if (ImGui::Button("Save", ImVec2(buttonWidth, 0))) {
        SaveSettings();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Save settings to file and apply");
    }

    ImGui::SameLine();

    if (ImGui::Button("Discard", ImVec2(buttonWidth, 0))) {
        DiscardChanges();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Discard all changes");
    }

    if (m_hasUnsavedChanges) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Unsaved changes");
    }
}

// ============================================================================
// Utility Functions
// ============================================================================

void SettingsMenuComplete::ApplyPreset(QualityPreset preset) {
    m_currentSettings = SettingsManager::GetPresetSettings(preset);
    m_currentSettings.preset = preset;

    // Update UI state
    m_selectedBackend = static_cast<int>(m_currentSettings.rendering.backend);
    m_selectedGIMethod = static_cast<int>(m_currentSettings.lighting.giMethod);
    m_selectedLODQuality = static_cast<int>(m_currentSettings.lod.quality);
    m_selectedUpdateFreq = static_cast<int>(m_currentSettings.caching.lightCacheUpdate);

    MarkAsModified();
    spdlog::info("Applied preset: {}", QualityPresetToString(preset));
}

void SettingsMenuComplete::SaveSettings() {
    if (!ValidateSettings()) {
        m_showValidationErrors = true;
        return;
    }

    auto result = SettingsManager::Instance().Save(m_saveFilepath);
    if (result) {
        ApplyChanges();
        spdlog::info("Settings saved to: {}", m_saveFilepath);
    } else {
        spdlog::error("Failed to save settings: {}", result.error());
    }
}

void SettingsMenuComplete::LoadSettings() {
    auto result = SettingsManager::Instance().Load(m_loadFilepath);
    if (result) {
        Initialize();
        spdlog::info("Settings loaded from: {}", m_loadFilepath);
    } else {
        spdlog::error("Failed to load settings: {}", result.error());
    }
}

void SettingsMenuComplete::ResetToDefault() {
    ApplyPreset(QualityPreset::High);
    spdlog::info("Settings reset to default (High preset)");
}

void SettingsMenuComplete::ApplyChanges() {
    if (!ValidateSettings()) {
        m_showValidationErrors = true;
        return;
    }

    SettingsManager::Instance().GetSettings() = m_currentSettings;
    SettingsManager::Instance().NotifyChanges();

    m_originalSettings = m_currentSettings;
    ClearModifiedFlag();
    spdlog::info("Settings applied");
}

void SettingsMenuComplete::DiscardChanges() {
    m_currentSettings = m_originalSettings;

    // Restore UI state
    m_selectedBackend = static_cast<int>(m_currentSettings.rendering.backend);
    m_selectedGIMethod = static_cast<int>(m_currentSettings.lighting.giMethod);
    m_selectedLODQuality = static_cast<int>(m_currentSettings.lod.quality);
    m_selectedUpdateFreq = static_cast<int>(m_currentSettings.caching.lightCacheUpdate);

    ClearModifiedFlag();
    spdlog::info("Changes discarded");
}

void SettingsMenuComplete::MarkAsModified() {
    m_hasUnsavedChanges = true;
    m_currentSettings.preset = QualityPreset::Custom;
}

void SettingsMenuComplete::ClearModifiedFlag() {
    m_hasUnsavedChanges = false;
}

bool SettingsMenuComplete::ValidateSettings() {
    auto result = SettingsManager::Instance().Validate();
    m_validationErrors = result.errors;
    m_validationErrors.insert(m_validationErrors.end(), result.warnings.begin(), result.warnings.end());
    return result.valid;
}

void SettingsMenuComplete::ShowValidationErrors() {
    ImGui::OpenPopup("Validation Errors");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Validation Errors", &m_showValidationErrors, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("The following issues were found:");
        ImGui::Spacing();

        for (const auto& error : m_validationErrors) {
            ImGui::BulletText("%s", error.c_str());
        }

        ImGui::Spacing();
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            m_showValidationErrors = false;
        }

        ImGui::EndPopup();
    }
}

void SettingsMenuComplete::UpdatePreview() {
    // TODO: Implement preview system
}

void SettingsMenuComplete::RenderPreviewPanel() {
    // TODO: Implement preview rendering
}

PerformanceImpact SettingsMenuComplete::EstimateImpact(const std::string& settingName) {
    // Simple impact estimation
    // TODO: More sophisticated impact analysis
    return PerformanceImpact::Medium;
}

void SettingsMenuComplete::RenderSectionHeader(const char* title) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "%s", title);
    ImGui::Separator();
    ImGui::Spacing();
}

void SettingsMenuComplete::RenderSettingRow(const char* label, float labelWidth) {
    ImGui::Text("%s", label);
    ImGui::SameLine(labelWidth);
}

void SettingsMenuComplete::RenderPerformanceIndicator(PerformanceImpact impact) {
    ImGui::SameLine();
    switch (impact) {
        case PerformanceImpact::None:
            break;
        case PerformanceImpact::Low:
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[Low Impact]");
            break;
        case PerformanceImpact::Medium:
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "[Medium Impact]");
            break;
        case PerformanceImpact::High:
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "[High Impact]");
            break;
    }
}

void SettingsMenuComplete::RenderTooltip(const char* text) {
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void SettingsMenuComplete::RenderProgressBar(float fraction, const char* overlay) {
    ImGui::ProgressBar(fraction, ImVec2(-1.0f, 0.0f), overlay);
}

bool SettingsMenuComplete::RenderButton(const char* label, const char* tooltip) {
    bool clicked = ImGui::Button(label);
    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }
    return clicked;
}

} // namespace Nova
