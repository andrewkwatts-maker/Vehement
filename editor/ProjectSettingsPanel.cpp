#include "ProjectSettingsPanel.hpp"
#include <fstream>

namespace Nova3D {

ProjectSettingsPanel::ProjectSettingsPanel() {
}

ProjectSettingsPanel::~ProjectSettingsPanel() {
    Shutdown();
}

void ProjectSettingsPanel::Initialize() {
    // Get global property container
    m_globalProperties = &PropertySystem::Instance().GetGlobalContainer();
}

void ProjectSettingsPanel::Shutdown() {
}

void ProjectSettingsPanel::RenderUI() {
    if (!m_isOpen) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(800, 700), ImGuiCond_FirstUseEver);
    ImGui::Begin("Project Settings", &m_isOpen);

    // Toolbar
    RenderToolbar();

    ImGui::Separator();

    // Tabs for different setting categories
    if (ImGui::BeginTabBar("SettingsTabs")) {
        if (ImGui::BeginTabItem("Rendering")) {
            RenderRenderingTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Material System")) {
            RenderMaterialSystemTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Lighting")) {
            RenderLightingTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("LOD")) {
            RenderLODTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Caching")) {
            RenderCachingTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Performance")) {
            RenderPerformanceTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Physics")) {
            RenderPhysicsTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Audio")) {
            RenderAudioTab();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::Separator();

    // Status bar
    RenderStatusBar();

    ImGui::End();
}

void ProjectSettingsPanel::RenderToolbar() {
    if (ImGui::Button("Save Settings")) {
        SaveSettings();
    }

    ImGui::SameLine();
    if (ImGui::Button("Load Settings")) {
        LoadSettings();
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset to Defaults")) {
        ResetToDefaults();
    }
}

void ProjectSettingsPanel::RenderStatusBar() {
    if (m_unsavedChanges) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Unsaved changes");
    } else {
        ImGui::Text("All changes saved");
    }
}

void ProjectSettingsPanel::RenderRenderingTab() {
    PropertyOverrideUI::BeginCategory("Backend");
    RenderBackendSettings();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Resolution");
    RenderResolutionSettings();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Quality");
    RenderQualitySettings();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Post-Processing");
    RenderPostProcessingSettings();
    PropertyOverrideUI::EndCategory();
}

void ProjectSettingsPanel::RenderBackendSettings() {
    const char* backends[] = { "Vulkan", "DirectX 12", "Metal", "OpenGL" };
    int currentBackend = static_cast<int>(m_tempValues.backend);

    PropertyOverrideUI::RenderCombo(
        "Rendering Backend",
        currentBackend,
        backends,
        4,
        m_globalProperties,
        PropertyLevel::Global,
        [this](int index) {
            m_tempValues.backend = static_cast<RenderingBackend>(index);
            m_unsavedChanges = true;
        },
        "Graphics API to use for rendering"
    );
}

void ProjectSettingsPanel::RenderResolutionSettings() {
    PropertyOverrideUI::RenderInt(
        "Screen Width",
        m_tempValues.screenWidth,
        m_globalProperties,
        PropertyLevel::Global,
        [this](int) { m_unsavedChanges = true; },
        640, 7680,
        "Screen width in pixels"
    );

    PropertyOverrideUI::RenderInt(
        "Screen Height",
        m_tempValues.screenHeight,
        m_globalProperties,
        PropertyLevel::Global,
        [this](int) { m_unsavedChanges = true; },
        480, 4320,
        "Screen height in pixels"
    );

    PropertyOverrideUI::RenderBool(
        "Fullscreen",
        m_tempValues.fullscreen,
        m_globalProperties,
        PropertyLevel::Global,
        [this](bool) { m_unsavedChanges = true; },
        "Run in fullscreen mode"
    );

    PropertyOverrideUI::RenderFloat(
        "Render Scale",
        m_tempValues.renderScale,
        m_globalProperties,
        PropertyLevel::Global,
        [this](float) { m_unsavedChanges = true; },
        0.25f, 2.0f,
        "Internal rendering resolution scale (1.0 = native)"
    );

    PropertyOverrideUI::RenderBool(
        "VSync",
        m_tempValues.vsync,
        m_globalProperties,
        PropertyLevel::Global,
        [this](bool) { m_unsavedChanges = true; },
        "Enable vertical synchronization"
    );

    PropertyOverrideUI::RenderInt(
        "Target Framerate",
        m_tempValues.targetFramerate,
        m_globalProperties,
        PropertyLevel::Global,
        [this](int) { m_unsavedChanges = true; },
        30, 240,
        "Target framerate (0 = unlimited)"
    );
}

void ProjectSettingsPanel::RenderQualitySettings() {
    const char* msaaOptions[] = { "Off", "2x", "4x", "8x" };
    int msaaIndex = 0;
    if (m_tempValues.msaaSamples == 2) msaaIndex = 1;
    else if (m_tempValues.msaaSamples == 4) msaaIndex = 2;
    else if (m_tempValues.msaaSamples == 8) msaaIndex = 3;

    PropertyOverrideUI::RenderCombo(
        "MSAA",
        msaaIndex,
        msaaOptions,
        4,
        m_globalProperties,
        PropertyLevel::Global,
        [this](int index) {
            int samples[] = { 0, 2, 4, 8 };
            m_tempValues.msaaSamples = samples[index];
            m_unsavedChanges = true;
        },
        "Multisample anti-aliasing samples"
    );

    PropertyOverrideUI::RenderInt(
        "Anisotropic Filtering",
        m_tempValues.anisotropicFiltering,
        m_globalProperties,
        PropertyLevel::Global,
        [this](int) { m_unsavedChanges = true; },
        1, 16,
        "Anisotropic filtering level"
    );

    PropertyOverrideUI::RenderBool(
        "HDR",
        m_tempValues.hdr,
        m_globalProperties,
        PropertyLevel::Global,
        [this](bool) { m_unsavedChanges = true; },
        "Enable high dynamic range rendering"
    );

    PropertyOverrideUI::RenderFloat(
        "Gamma",
        m_tempValues.gamma,
        m_globalProperties,
        PropertyLevel::Global,
        [this](float) { m_unsavedChanges = true; },
        1.8f, 2.6f,
        "Gamma correction value"
    );
}

void ProjectSettingsPanel::RenderPostProcessingSettings() {
    PropertyOverrideUI::RenderBool(
        "Bloom",
        m_tempValues.bloom,
        m_globalProperties,
        PropertyLevel::Global,
        [this](bool) { m_unsavedChanges = true; },
        "Enable bloom effect"
    );

    PropertyOverrideUI::RenderBool(
        "Motion Blur",
        m_tempValues.motionBlur,
        m_globalProperties,
        PropertyLevel::Global,
        [this](bool) { m_unsavedChanges = true; },
        "Enable motion blur"
    );

    PropertyOverrideUI::RenderBool(
        "Depth of Field",
        m_tempValues.dof,
        m_globalProperties,
        PropertyLevel::Global,
        [this](bool) { m_unsavedChanges = true; },
        "Enable depth of field"
    );

    PropertyOverrideUI::RenderBool(
        "Chromatic Aberration",
        m_tempValues.chromaticAberration,
        m_globalProperties,
        PropertyLevel::Global,
        [this](bool) { m_unsavedChanges = true; },
        "Enable chromatic aberration"
    );

    PropertyOverrideUI::RenderBool(
        "Vignette",
        m_tempValues.vignette,
        m_globalProperties,
        PropertyLevel::Global,
        [this](bool) { m_unsavedChanges = true; },
        "Enable vignette effect"
    );

    PropertyOverrideUI::RenderBool(
        "Film Grain",
        m_tempValues.filmGrain,
        m_globalProperties,
        PropertyLevel::Global,
        [this](bool) { m_unsavedChanges = true; },
        "Enable film grain"
    );
}

void ProjectSettingsPanel::RenderMaterialSystemTab() {
    PropertyOverrideUI::BeginCategory("Material Features");
    RenderMaterialFeatures();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Shader Settings");
    RenderShaderSettings();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Texture Settings");
    RenderTextureSettings();
    PropertyOverrideUI::EndCategory();
}

void ProjectSettingsPanel::RenderMaterialFeatures() {
    PropertyOverrideUI::RenderBool(
        "Enable IOR",
        m_tempValues.enableIOR,
        m_globalProperties,
        PropertyLevel::Global,
        [this](bool) { m_unsavedChanges = true; },
        "Enable index of refraction calculations"
    );

    PropertyOverrideUI::RenderBool(
        "Enable Dispersion",
        m_tempValues.enableDispersion,
        m_globalProperties,
        PropertyLevel::Global,
        [this](bool) { m_unsavedChanges = true; },
        "Enable chromatic dispersion"
    );

    PropertyOverrideUI::RenderBool(
        "Enable Subsurface Scattering",
        m_tempValues.enableSubsurface,
        m_globalProperties,
        PropertyLevel::Global,
        [this](bool) { m_unsavedChanges = true; },
        "Enable subsurface scattering"
    );

    PropertyOverrideUI::RenderBool(
        "Enable Emission",
        m_tempValues.enableEmission,
        m_globalProperties,
        PropertyLevel::Global,
        [this](bool) { m_unsavedChanges = true; },
        "Enable emissive materials"
    );

    PropertyOverrideUI::RenderInt(
        "Max Material Layers",
        m_tempValues.maxMaterialLayers,
        m_globalProperties,
        PropertyLevel::Global,
        [this](int) { m_unsavedChanges = true; },
        1, 16,
        "Maximum number of material layers"
    );

    PropertyOverrideUI::RenderInt(
        "Max Textures Per Material",
        m_tempValues.maxTexturesPerMaterial,
        m_globalProperties,
        PropertyLevel::Global,
        [this](int) { m_unsavedChanges = true; },
        1, 32,
        "Maximum textures per material"
    );
}

void ProjectSettingsPanel::RenderShaderSettings() {
    PropertyOverrideUI::RenderBool(
        "Compile Async",
        m_tempValues.compileAsync,
        m_globalProperties,
        PropertyLevel::Global,
        [this](bool) { m_unsavedChanges = true; },
        "Compile shaders asynchronously"
    );

    PropertyOverrideUI::RenderBool(
        "Optimize Shaders",
        m_tempValues.optimizeShaders,
        m_globalProperties,
        PropertyLevel::Global,
        [this](bool) { m_unsavedChanges = true; },
        "Enable shader optimization"
    );

    PropertyOverrideUI::RenderBool(
        "Cache Shaders",
        m_tempValues.cacheShaders,
        m_globalProperties,
        PropertyLevel::Global,
        [this](bool) { m_unsavedChanges = true; },
        "Cache compiled shaders"
    );

    PropertyOverrideUI::RenderString(
        "Shader Cache Path",
        m_tempValues.shaderCachePath,
        m_globalProperties,
        PropertyLevel::Global,
        [this](const std::string&) { m_unsavedChanges = true; },
        "Path for shader cache"
    );
}

void ProjectSettingsPanel::RenderTextureSettings() {
    ImGui::Text("Texture settings (compression, streaming, etc.)");
}

void ProjectSettingsPanel::RenderLightingTab() {
    PropertyOverrideUI::BeginCategory("Light Limits");
    RenderLightLimits();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Shadows");
    RenderShadowSettings();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Global Illumination");
    RenderGISettings();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Light Clustering");
    RenderClusteringSettings();
    PropertyOverrideUI::EndCategory();
}

void ProjectSettingsPanel::RenderLightLimits() {
    PropertyOverrideUI::RenderInt(
        "Max Lights Per Cluster",
        m_tempValues.maxLightsPerCluster,
        m_globalProperties,
        PropertyLevel::Global,
        [this](int) { m_unsavedChanges = true; },
        64, 1024,
        "Maximum lights per cluster"
    );

    PropertyOverrideUI::RenderInt(
        "Max Lights Global",
        m_tempValues.maxLightsGlobal,
        m_globalProperties,
        PropertyLevel::Global,
        [this](int) { m_unsavedChanges = true; },
        256, 16384,
        "Maximum total lights in scene"
    );
}

void ProjectSettingsPanel::RenderShadowSettings() {
    const char* qualities[] = { "Low", "Medium", "High", "Ultra" };
    int qualityIndex = static_cast<int>(m_tempValues.shadowQuality);

    PropertyOverrideUI::RenderCombo(
        "Shadow Quality",
        qualityIndex,
        qualities,
        4,
        m_globalProperties,
        PropertyLevel::Global,
        [this](int index) {
            m_tempValues.shadowQuality = static_cast<ShadowQuality>(index);
            m_unsavedChanges = true;
        },
        "Shadow quality preset"
    );

    const char* mapSizes[] = { "512", "1024", "2048", "4096", "8192" };
    int sizeIndex = 2; // Default to 2048
    if (m_tempValues.shadowMapSize == 512) sizeIndex = 0;
    else if (m_tempValues.shadowMapSize == 1024) sizeIndex = 1;
    else if (m_tempValues.shadowMapSize == 2048) sizeIndex = 2;
    else if (m_tempValues.shadowMapSize == 4096) sizeIndex = 3;
    else if (m_tempValues.shadowMapSize == 8192) sizeIndex = 4;

    PropertyOverrideUI::RenderCombo(
        "Shadow Map Size",
        sizeIndex,
        mapSizes,
        5,
        m_globalProperties,
        PropertyLevel::Global,
        [this](int index) {
            int sizes[] = { 512, 1024, 2048, 4096, 8192 };
            m_tempValues.shadowMapSize = sizes[index];
            m_unsavedChanges = true;
        },
        "Default shadow map resolution"
    );

    PropertyOverrideUI::RenderInt(
        "Shadow Cascades",
        m_tempValues.shadowCascades,
        m_globalProperties,
        PropertyLevel::Global,
        [this](int) { m_unsavedChanges = true; },
        1, 8,
        "Number of cascade splits for directional lights"
    );

    PropertyOverrideUI::RenderFloat(
        "Shadow Distance",
        m_tempValues.shadowDistance,
        m_globalProperties,
        PropertyLevel::Global,
        [this](float) { m_unsavedChanges = true; },
        10.0f, 500.0f,
        "Maximum shadow rendering distance"
    );
}

void ProjectSettingsPanel::RenderGISettings() {
    const char* techniques[] = { "None", "SSAO", "VXGI", "RTGI", "Probe Grid" };
    int techniqueIndex = static_cast<int>(m_tempValues.giTechnique);

    PropertyOverrideUI::RenderCombo(
        "GI Technique",
        techniqueIndex,
        techniques,
        5,
        m_globalProperties,
        PropertyLevel::Global,
        [this](int index) {
            m_tempValues.giTechnique = static_cast<GITechnique>(index);
            m_unsavedChanges = true;
        },
        "Global illumination technique"
    );

    if (m_tempValues.giTechnique == GITechnique::VXGI) {
        PropertyOverrideUI::RenderInt(
            "VXGI Resolution",
            m_tempValues.vxgiResolution,
            m_globalProperties,
            PropertyLevel::Global,
            [this](int) { m_unsavedChanges = true; },
            64, 512,
            "Voxel grid resolution"
        );
    }

    if (m_tempValues.giTechnique == GITechnique::ProbeGrid) {
        PropertyOverrideUI::RenderInt(
            "Probe Grid Resolution",
            m_tempValues.probeGridResolution,
            m_globalProperties,
            PropertyLevel::Global,
            [this](int) { m_unsavedChanges = true; },
            16, 128,
            "Light probe grid resolution"
        );
    }

    if (m_tempValues.giTechnique == GITechnique::RTGI) {
        PropertyOverrideUI::RenderBool(
            "Enable RTGI",
            m_tempValues.rtgiEnabled,
            m_globalProperties,
            PropertyLevel::Global,
            [this](bool) { m_unsavedChanges = true; },
            "Enable ray-traced global illumination (requires RT hardware)"
        );
    }
}

void ProjectSettingsPanel::RenderClusteringSettings() {
    ImGui::Text("Cluster Dimensions: %d x %d x %d",
               m_tempValues.clusterDimensions.x,
               m_tempValues.clusterDimensions.y,
               m_tempValues.clusterDimensions.z);

    PropertyOverrideUI::RenderFloat(
        "Cluster Near Plane",
        m_tempValues.clusterNearPlane,
        m_globalProperties,
        PropertyLevel::Global,
        [this](float) { m_unsavedChanges = true; },
        0.01f, 10.0f,
        "Near plane for cluster grid"
    );

    PropertyOverrideUI::RenderFloat(
        "Cluster Far Plane",
        m_tempValues.clusterFarPlane,
        m_globalProperties,
        PropertyLevel::Global,
        [this](float) { m_unsavedChanges = true; },
        100.0f, 10000.0f,
        "Far plane for cluster grid"
    );
}

void ProjectSettingsPanel::RenderLODTab() {
    PropertyOverrideUI::BeginCategory("LOD Defaults");
    RenderLODDefaults();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("LOD Quality");
    RenderLODQuality();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Transitions");
    RenderLODTransitionSettings();
    PropertyOverrideUI::EndCategory();
}

void ProjectSettingsPanel::RenderLODDefaults() {
    PropertyOverrideUI::RenderFloat(
        "LOD Bias",
        m_tempValues.lodBias,
        m_globalProperties,
        PropertyLevel::Global,
        [this](float) { m_unsavedChanges = true; },
        -2.0f, 2.0f,
        "Global LOD bias (negative = higher quality)"
    );

    ImGui::Text("Default LOD Distances:");
    for (size_t i = 0; i < m_tempValues.defaultLODDistances.size(); ++i) {
        char label[64];
        snprintf(label, sizeof(label), "LOD %zu", i);

        PropertyOverrideUI::RenderFloat(
            label,
            m_tempValues.defaultLODDistances[i],
            m_globalProperties,
            PropertyLevel::Global,
            [this](float) { m_unsavedChanges = true; },
            1.0f, 1000.0f
        );
    }
}

void ProjectSettingsPanel::RenderLODQuality() {
    ImGui::Text("LOD quality controls");
}

void ProjectSettingsPanel::RenderLODTransitionSettings() {
    PropertyOverrideUI::RenderBool(
        "Fade Transitions",
        m_tempValues.lodFadeTransitions,
        m_globalProperties,
        PropertyLevel::Global,
        [this](bool) { m_unsavedChanges = true; },
        "Smooth fade between LOD levels"
    );

    if (m_tempValues.lodFadeTransitions) {
        PropertyOverrideUI::RenderFloat(
            "Transition Duration",
            m_tempValues.lodTransitionDuration,
            m_globalProperties,
            PropertyLevel::Global,
            [this](float) { m_unsavedChanges = true; },
            0.0f, 2.0f,
            "Duration of LOD transitions"
        );
    }
}

void ProjectSettingsPanel::RenderCachingTab() {
    PropertyOverrideUI::BeginCategory("Shader Cache");
    RenderShaderCacheSettings();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Asset Cache");
    RenderAssetCacheSettings();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Brick Atlas");
    RenderBrickAtlasSettings();
    PropertyOverrideUI::EndCategory();
}

void ProjectSettingsPanel::RenderShaderCacheSettings() {
    ImGui::Text("Shader cache settings (covered in Material System tab)");
}

void ProjectSettingsPanel::RenderAssetCacheSettings() {
    float cacheSizeGB = static_cast<float>(m_tempValues.assetCacheSize) / (1024.0f * 1024.0f * 1024.0f);
    if (PropertyOverrideUI::RenderFloat(
        "Asset Cache Size (GB)",
        cacheSizeGB,
        m_globalProperties,
        PropertyLevel::Global,
        nullptr,
        0.5f, 16.0f,
        "Maximum asset cache size in gigabytes"
    )) {
        m_tempValues.assetCacheSize = static_cast<size_t>(cacheSizeGB * 1024.0f * 1024.0f * 1024.0f);
        m_unsavedChanges = true;
    }

    PropertyOverrideUI::RenderBool(
        "Preload Common Assets",
        m_tempValues.preloadCommonAssets,
        m_globalProperties,
        PropertyLevel::Global,
        [this](bool) { m_unsavedChanges = true; },
        "Preload frequently used assets at startup"
    );
}

void ProjectSettingsPanel::RenderBrickAtlasSettings() {
    PropertyOverrideUI::RenderInt(
        "Brick Atlas Size",
        m_tempValues.brickAtlasSize,
        m_globalProperties,
        PropertyLevel::Global,
        [this](int) { m_unsavedChanges = true; },
        1024, 8192,
        "Size of brick texture atlas"
    );

    PropertyOverrideUI::RenderInt(
        "Max Bricks",
        m_tempValues.maxBricksInAtlas,
        m_globalProperties,
        PropertyLevel::Global,
        [this](int) { m_unsavedChanges = true; },
        1024, 16384,
        "Maximum number of bricks in atlas"
    );

    PropertyOverrideUI::RenderBool(
        "Compress Bricks",
        m_tempValues.compressBricks,
        m_globalProperties,
        PropertyLevel::Global,
        [this](bool) { m_unsavedChanges = true; },
        "Enable brick compression"
    );
}

void ProjectSettingsPanel::RenderPerformanceTab() {
    PropertyOverrideUI::BeginCategory("Threading");
    RenderThreadingSettings();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Memory");
    RenderMemorySettings();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Profiling");
    RenderProfilingSettings();
    PropertyOverrideUI::EndCategory();
}

void ProjectSettingsPanel::RenderThreadingSettings() {
    PropertyOverrideUI::RenderInt(
        "Worker Threads",
        m_tempValues.workerThreadCount,
        m_globalProperties,
        PropertyLevel::Global,
        [this](int) { m_unsavedChanges = true; },
        1, 32,
        "Number of worker threads"
    );

    PropertyOverrideUI::RenderBool(
        "Enable Job System",
        m_tempValues.enableJobSystem,
        m_globalProperties,
        PropertyLevel::Global,
        [this](bool) { m_unsavedChanges = true; },
        "Enable job-based parallelism"
    );
}

void ProjectSettingsPanel::RenderMemorySettings() {
    float memoryGB = static_cast<float>(m_tempValues.maxMemoryUsage) / (1024.0f * 1024.0f * 1024.0f);
    if (PropertyOverrideUI::RenderFloat(
        "Max Memory Usage (GB)",
        memoryGB,
        m_globalProperties,
        PropertyLevel::Global,
        nullptr,
        1.0f, 64.0f,
        "Maximum memory usage in gigabytes"
    )) {
        m_tempValues.maxMemoryUsage = static_cast<size_t>(memoryGB * 1024.0f * 1024.0f * 1024.0f);
        m_unsavedChanges = true;
    }

    PropertyOverrideUI::RenderBool(
        "Enable Memory Profiling",
        m_tempValues.enableMemoryProfiling,
        m_globalProperties,
        PropertyLevel::Global,
        [this](bool) { m_unsavedChanges = true; },
        "Track memory allocations (impacts performance)"
    );
}

void ProjectSettingsPanel::RenderProfilingSettings() {
    PropertyOverrideUI::RenderBool(
        "Enable GPU Profiling",
        m_tempValues.enableGPUProfiling,
        m_globalProperties,
        PropertyLevel::Global,
        [this](bool) { m_unsavedChanges = true; },
        "Enable GPU performance profiling"
    );
}

void ProjectSettingsPanel::RenderPhysicsTab() {
    PropertyOverrideUI::BeginCategory("Physics Engine");
    RenderPhysicsEngine();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Quality");
    RenderPhysicsQuality();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Limits");
    RenderPhysicsLimits();
    PropertyOverrideUI::EndCategory();
}

void ProjectSettingsPanel::RenderPhysicsEngine() {
    ImGui::Text("Physics Engine: PhysX (default)");
}

void ProjectSettingsPanel::RenderPhysicsQuality() {
    PropertyOverrideUI::RenderInt(
        "Physics Threads",
        m_tempValues.physicsThreads,
        m_globalProperties,
        PropertyLevel::Global,
        [this](int) { m_unsavedChanges = true; },
        1, 8,
        "Number of physics threads"
    );

    PropertyOverrideUI::RenderInt(
        "Substeps",
        m_tempValues.physicsSubsteps,
        m_globalProperties,
        PropertyLevel::Global,
        [this](int) { m_unsavedChanges = true; },
        1, 8,
        "Physics substeps per frame"
    );

    PropertyOverrideUI::RenderFloat(
        "Timestep",
        m_tempValues.physicsTimestep,
        m_globalProperties,
        PropertyLevel::Global,
        [this](float) { m_unsavedChanges = true; },
        1.0f / 120.0f, 1.0f / 30.0f,
        "Fixed physics timestep",
        "%.5f"
    );
}

void ProjectSettingsPanel::RenderPhysicsLimits() {
    PropertyOverrideUI::RenderInt(
        "Max Rigid Bodies",
        m_tempValues.maxRigidBodies,
        m_globalProperties,
        PropertyLevel::Global,
        [this](int) { m_unsavedChanges = true; },
        100, 100000,
        "Maximum number of rigid bodies"
    );

    PropertyOverrideUI::RenderBool(
        "Enable CCD",
        m_tempValues.enableCCD,
        m_globalProperties,
        PropertyLevel::Global,
        [this](bool) { m_unsavedChanges = true; },
        "Enable continuous collision detection"
    );
}

void ProjectSettingsPanel::RenderAudioTab() {
    PropertyOverrideUI::BeginCategory("Audio Engine");
    RenderAudioEngine();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Quality");
    RenderAudioQuality();
    PropertyOverrideUI::EndCategory();

    PropertyOverrideUI::BeginCategory("Limits");
    RenderAudioLimits();
    PropertyOverrideUI::EndCategory();
}

void ProjectSettingsPanel::RenderAudioEngine() {
    ImGui::Text("Audio Engine: OpenAL (default)");
}

void ProjectSettingsPanel::RenderAudioQuality() {
    const char* sampleRates[] = { "22050", "44100", "48000", "96000" };
    int rateIndex = 2; // Default to 48000
    if (m_tempValues.audioSampleRate == 22050) rateIndex = 0;
    else if (m_tempValues.audioSampleRate == 44100) rateIndex = 1;
    else if (m_tempValues.audioSampleRate == 48000) rateIndex = 2;
    else if (m_tempValues.audioSampleRate == 96000) rateIndex = 3;

    PropertyOverrideUI::RenderCombo(
        "Sample Rate",
        rateIndex,
        sampleRates,
        4,
        m_globalProperties,
        PropertyLevel::Global,
        [this](int index) {
            int rates[] = { 22050, 44100, 48000, 96000 };
            m_tempValues.audioSampleRate = rates[index];
            m_unsavedChanges = true;
        },
        "Audio sample rate in Hz"
    );

    PropertyOverrideUI::RenderInt(
        "Channels",
        m_tempValues.audioChannels,
        m_globalProperties,
        PropertyLevel::Global,
        [this](int) { m_unsavedChanges = true; },
        1, 8,
        "Number of audio channels"
    );

    PropertyOverrideUI::RenderFloat(
        "Master Volume",
        m_tempValues.masterVolume,
        m_globalProperties,
        PropertyLevel::Global,
        [this](float) { m_unsavedChanges = true; },
        0.0f, 1.0f,
        "Master volume level"
    );
}

void ProjectSettingsPanel::RenderAudioLimits() {
    PropertyOverrideUI::RenderInt(
        "Max Audio Sources",
        m_tempValues.maxAudioSources,
        m_globalProperties,
        PropertyLevel::Global,
        [this](int) { m_unsavedChanges = true; },
        16, 512,
        "Maximum simultaneous audio sources"
    );

    PropertyOverrideUI::RenderBool(
        "Enable 3D Audio",
        m_tempValues.enable3DAudio,
        m_globalProperties,
        PropertyLevel::Global,
        [this](bool) { m_unsavedChanges = true; },
        "Enable 3D positional audio"
    );
}

void ProjectSettingsPanel::SaveSettings() {
    if (!m_globalProperties) return;

    // Save to JSON file
    PropertySystem::Instance().SaveProject("ProjectSettings.json");

    m_unsavedChanges = false;
}

void ProjectSettingsPanel::LoadSettings() {
    // Load from JSON file
    PropertySystem::Instance().LoadProject("ProjectSettings.json");

    m_unsavedChanges = false;
}

void ProjectSettingsPanel::ResetToDefaults() {
    // Reset all settings to default values
    m_tempValues = decltype(m_tempValues)();

    m_unsavedChanges = true;
}

const char* ProjectSettingsPanel::BackendToString(RenderingBackend backend) const {
    switch (backend) {
        case RenderingBackend::Vulkan: return "Vulkan";
        case RenderingBackend::DirectX12: return "DirectX 12";
        case RenderingBackend::Metal: return "Metal";
        case RenderingBackend::OpenGL: return "OpenGL";
        default: return "Unknown";
    }
}

const char* ProjectSettingsPanel::ShadowQualityToString(ShadowQuality quality) const {
    switch (quality) {
        case ShadowQuality::Low: return "Low";
        case ShadowQuality::Medium: return "Medium";
        case ShadowQuality::High: return "High";
        case ShadowQuality::Ultra: return "Ultra";
        default: return "Unknown";
    }
}

const char* ProjectSettingsPanel::GITechniqueToString(GITechnique technique) const {
    switch (technique) {
        case GITechnique::None: return "None";
        case GITechnique::SSAO: return "SSAO";
        case GITechnique::VXGI: return "VXGI";
        case GITechnique::RTGI: return "RTGI";
        case GITechnique::ProbeGrid: return "Probe Grid";
        default: return "Unknown";
    }
}

} // namespace Nova3D
