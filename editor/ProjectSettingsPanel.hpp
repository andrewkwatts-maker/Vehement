#pragma once

#include "../engine/core/PropertySystem.hpp"
#include "PropertyOverrideUI.hpp"
#include <imgui.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>

namespace Nova3D {

// Rendering backend options
enum class RenderingBackend {
    Vulkan,
    DirectX12,
    Metal,
    OpenGL
};

// Shadow quality presets
enum class ShadowQuality {
    Low,
    Medium,
    High,
    Ultra
};

// Global illumination technique
enum class GITechnique {
    None,
    SSAO,
    VXGI,
    RTGI,
    ProbeGrid
};

// Project settings panel
class ProjectSettingsPanel {
public:
    ProjectSettingsPanel();
    ~ProjectSettingsPanel();

    void Initialize();
    void Shutdown();

    void RenderUI();

    // Panel state
    bool IsOpen() const { return m_isOpen; }
    void SetOpen(bool open) { m_isOpen = open; }

    // Settings management
    void SaveSettings();
    void LoadSettings();
    void ResetToDefaults();

private:
    // Tab rendering
    void RenderRenderingTab();
    void RenderMaterialSystemTab();
    void RenderLightingTab();
    void RenderLODTab();
    void RenderCachingTab();
    void RenderPerformanceTab();
    void RenderPhysicsTab();
    void RenderAudioTab();

    // Rendering settings
    void RenderBackendSettings();
    void RenderResolutionSettings();
    void RenderQualitySettings();
    void RenderPostProcessingSettings();

    // Material system settings
    void RenderMaterialFeatures();
    void RenderShaderSettings();
    void RenderTextureSettings();

    // Lighting settings
    void RenderLightLimits();
    void RenderShadowSettings();
    void RenderGISettings();
    void RenderClusteringSettings();

    // LOD settings
    void RenderLODDefaults();
    void RenderLODQuality();
    void RenderLODTransitionSettings();

    // Caching settings
    void RenderShaderCacheSettings();
    void RenderAssetCacheSettings();
    void RenderBrickAtlasSettings();

    // Performance settings
    void RenderThreadingSettings();
    void RenderMemorySettings();
    void RenderProfilingSettings();

    // Physics settings
    void RenderPhysicsEngine();
    void RenderPhysicsQuality();
    void RenderPhysicsLimits();

    // Audio settings
    void RenderAudioEngine();
    void RenderAudioQuality();
    void RenderAudioLimits();

    // UI helpers
    void RenderToolbar();
    void RenderStatusBar();

    // Helper functions
    const char* BackendToString(RenderingBackend backend) const;
    const char* ShadowQualityToString(ShadowQuality quality) const;
    const char* GITechniqueToString(GITechnique technique) const;

private:
    bool m_isOpen = true;

    // Global property container
    PropertyContainer* m_globalProperties = nullptr;

    // UI state
    bool m_unsavedChanges = false;

    // Temporary values for editing
    struct {
        // Rendering
        RenderingBackend backend = RenderingBackend::Vulkan;
        int screenWidth = 1920;
        int screenHeight = 1080;
        bool fullscreen = false;
        bool vsync = true;
        int targetFramerate = 60;
        float renderScale = 1.0f;

        // Quality
        int msaaSamples = 4;
        int anisotropicFiltering = 16;
        bool hdr = true;
        float gamma = 2.2f;

        // Post-processing
        bool bloom = true;
        bool motionBlur = true;
        bool dof = true;
        bool chromaticAberration = false;
        bool vignette = true;
        bool filmGrain = false;

        // Material system
        bool enableIOR = true;
        bool enableDispersion = true;
        bool enableSubsurface = true;
        bool enableEmission = true;
        int maxMaterialLayers = 8;
        int maxTexturesPerMaterial = 16;

        // Shader compilation
        bool compileAsync = true;
        bool optimizeShaders = true;
        bool cacheShaders = true;
        std::string shaderCachePath = "Cache/Shaders/";

        // Lighting
        int maxLightsPerCluster = 256;
        int maxLightsGlobal = 4096;
        ShadowQuality shadowQuality = ShadowQuality::High;
        int shadowMapSize = 2048;
        int shadowCascades = 4;
        float shadowDistance = 100.0f;

        // Global illumination
        GITechnique giTechnique = GITechnique::VXGI;
        int vxgiResolution = 128;
        int probeGridResolution = 32;
        bool rtgiEnabled = false;

        // Light clustering
        glm::ivec3 clusterDimensions = glm::ivec3(16, 9, 24);
        float clusterNearPlane = 0.1f;
        float clusterFarPlane = 1000.0f;

        // LOD
        float lodBias = 0.0f;
        std::vector<float> defaultLODDistances = { 10.0f, 25.0f, 50.0f, 100.0f };
        float lodTransitionDuration = 0.5f;
        bool lodFadeTransitions = true;

        // Caching
        int brickAtlasSize = 2048;
        int maxBricksInAtlas = 4096;
        bool compressBricks = true;
        size_t assetCacheSize = 1024 * 1024 * 1024; // 1GB
        bool preloadCommonAssets = true;

        // Performance
        int workerThreadCount = 8;
        bool enableJobSystem = true;
        size_t maxMemoryUsage = 4ULL * 1024 * 1024 * 1024; // 4GB
        bool enableMemoryProfiling = false;
        bool enableGPUProfiling = true;

        // Physics
        int physicsThreads = 2;
        int physicsSubsteps = 1;
        float physicsTimestep = 1.0f / 60.0f;
        int maxRigidBodies = 10000;
        bool enableCCD = true;

        // Audio
        int audioSampleRate = 48000;
        int audioChannels = 2;
        int maxAudioSources = 128;
        float masterVolume = 1.0f;
        bool enable3DAudio = true;
    } m_tempValues;
};

} // namespace Nova3D
