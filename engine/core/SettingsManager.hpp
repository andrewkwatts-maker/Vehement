#pragma once

#include <string>
#include <vector>
#include <array>
#include <memory>
#include <functional>
#include <optional>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

namespace Nova {

/**
 * @brief Quality preset levels for graphics settings
 */
enum class QualityPreset {
    Low,      // Integrated GPU, 30 FPS target
    Medium,   // GTX 1060, 60 FPS target
    High,     // RTX 2060, 60 FPS target
    Ultra,    // RTX 3080+, 120 FPS target
    Custom    // User-modified
};

/**
 * @brief Rendering backend selection
 */
enum class RenderBackend {
    SDFFirst,         // SDF-first hybrid renderer
    PolygonOnly,      // Traditional polygon rasterizer
    GPUDriven,        // GPU-driven rendering pipeline
    PathTracing       // Path tracing renderer
};

/**
 * @brief Global illumination method
 */
enum class GIMethod {
    None,
    ReSTIR,
    SVGF,
    ReSTIR_SVGF      // Combined ReSTIR + SVGF
};

/**
 * @brief LOD quality level
 */
enum class LODQuality {
    VeryLow,
    Low,
    Medium,
    High,
    VeryHigh
};

/**
 * @brief Rendering settings
 */
struct RenderingSettings {
    // Backend
    RenderBackend backend = RenderBackend::SDFFirst;
    int resolutionScale = 100;  // Percentage
    int targetFPS = 60;
    bool enableAdaptive = true;

    // SDF Rasterizer
    glm::ivec2 sdfTileSize = glm::ivec2(16, 16);
    int maxRaymarchSteps = 128;
    bool enableTemporal = true;
    bool enableCheckerboard = true;
    float raymarchEpsilon = 0.001f;

    // Polygon Rasterizer
    bool enableInstancing = true;
    int shadowCascades = 4;
    int msaaSamples = 4;
    bool enableOcclusionCulling = true;

    // GPU-Driven Rendering
    bool enableGPUCulling = true;
    int gpuCullingJobSize = 256;
    bool enablePersistentBuffers = true;
    bool enableMeshShaders = false;

    // Async Compute
    bool enableAsyncCompute = true;
    int asyncComputeOverlap = 80;  // Percentage

    nlohmann::json ToJson() const;
    static RenderingSettings FromJson(const nlohmann::json& json);
};

/**
 * @brief Lighting settings
 */
struct LightingSettings {
    // Clustered Lighting
    int maxLights = 100000;
    glm::ivec3 clusterGrid = glm::ivec3(32, 18, 48);
    int maxLightsPerCluster = 1024;
    bool enableOverflowHandling = true;

    // Shadow Settings
    glm::ivec2 shadowAtlasSize = glm::ivec2(16384, 16384);
    int maxShadowMaps = 256;
    std::vector<float> cascadeSplits = {0.1f, 0.25f, 0.5f, 1.0f};
    int softShadowSamples = 16;
    float shadowBias = 0.005f;
    bool enableContactShadows = true;

    // Global Illumination
    GIMethod giMethod = GIMethod::ReSTIR_SVGF;
    int giSamplesPerPixel = 1;
    int restirReusePercent = 80;
    int svgfIterations = 5;
    bool enableGICache = true;

    // Light Types
    bool enablePointLights = true;
    bool enableSpotLights = true;
    bool enableDirectionalLights = true;
    bool enableAreaLights = true;
    bool enableEmissiveGeometry = true;

    nlohmann::json ToJson() const;
    static LightingSettings FromJson(const nlohmann::json& json);
};

/**
 * @brief Material settings
 */
struct MaterialSettings {
    // Physical Properties
    bool enableIOR = true;
    bool enableDispersion = true;
    bool enableSubsurfaceScattering = true;
    bool enableBlackbodyEmission = true;
    bool enableClearcoat = true;
    bool enableSheen = true;

    // Texture Quality
    int maxTextureSize = 4096;
    int anisotropicFiltering = 16;
    float mipmapBias = 0.0f;
    bool enableTextureCompression = true;
    bool enableVirtualTexturing = false;

    // Shader Compilation
    bool optimizeShaders = true;
    bool cacheShaders = true;
    bool includeDebugInfo = false;

    nlohmann::json ToJson() const;
    static MaterialSettings FromJson(const nlohmann::json& json);
};

/**
 * @brief LOD settings
 */
struct LODSettings {
    // Global LOD
    LODQuality quality = LODQuality::High;
    float lodBias = 0.0f;

    // Distance Thresholds (in meters)
    std::vector<float> lodDistances = {10.0f, 25.0f, 50.0f, 100.0f};
    float cullingDistance = 200.0f;

    // Transition Settings
    bool enableDithering = true;
    float transitionWidth = 5.0f;
    int hysteresisPercent = 10;

    // Per-Type Settings
    struct TypeSettings {
        bool useCustom = false;
        std::vector<float> customDistances;
        float customCulling = 0.0f;
    };

    TypeSettings buildings;
    TypeSettings units;
    TypeSettings terrain;

    nlohmann::json ToJson() const;
    static LODSettings FromJson(const nlohmann::json& json);
};

/**
 * @brief Caching settings
 */
struct CachingSettings {
    // SDF Brick Cache
    bool enableBrickCache = true;
    glm::ivec3 brickAtlasSize = glm::ivec3(32, 32, 32);
    int maxCacheMemoryMB = 512;
    bool enableDeduplication = true;

    // Shader Cache
    bool enableShaderCache = true;
    std::string shaderCachePath = "cache/shaders/";
    int maxCachedShaders = 1000;

    // Light Cache
    bool enableLightCache = true;
    enum class UpdateFrequency {
        PerFrame,
        OnChange,
        Manual
    };
    UpdateFrequency lightCacheUpdate = UpdateFrequency::PerFrame;
    bool enableStaticLightCache = true;

    nlohmann::json ToJson() const;
    static CachingSettings FromJson(const nlohmann::json& json);
};

/**
 * @brief Performance settings
 */
struct PerformanceSettings {
    // Thread Pool
    int workerThreads = 8;  // 0 = auto-detect
    int jobQueueSize = 1024;

    // Memory
    int gpuMemoryLimitMB = 0;  // 0 = auto
    int streamingBudgetMB = 2048;

    // Profiling
    bool enableProfiler = true;
    bool showProfilerOverlay = true;
    bool exportCSV = false;
    std::string profileOutputPath = "profiling/";

    nlohmann::json ToJson() const;
    static PerformanceSettings FromJson(const nlohmann::json& json);
};

/**
 * @brief Complete settings structure
 */
struct CompleteSettings {
    QualityPreset preset = QualityPreset::High;
    RenderingSettings rendering;
    LightingSettings lighting;
    MaterialSettings materials;
    LODSettings lod;
    CachingSettings caching;
    PerformanceSettings performance;

    nlohmann::json ToJson() const;
    static CompleteSettings FromJson(const nlohmann::json& json);
};

/**
 * @brief Settings validation result
 */
struct ValidationResult {
    bool valid = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    void AddError(const std::string& error) {
        errors.push_back(error);
        valid = false;
    }

    void AddWarning(const std::string& warning) {
        warnings.push_back(warning);
    }
};

/**
 * @brief Settings manager for comprehensive engine configuration
 *
 * Manages all rendering, lighting, material, LOD, caching, and performance settings.
 * Provides preset system, JSON save/load, and validation.
 */
class SettingsManager {
public:
    static SettingsManager& Instance();

    // Delete copy/move for singleton
    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;
    SettingsManager(SettingsManager&&) = delete;
    SettingsManager& operator=(SettingsManager&&) = delete;

    /**
     * @brief Initialize with default settings
     */
    void Initialize();

    /**
     * @brief Load settings from JSON file
     */
    [[nodiscard]] std::optional<void> Load(const std::string& filepath);

    /**
     * @brief Save settings to JSON file
     */
    [[nodiscard]] std::optional<void> Save(const std::string& filepath);

    /**
     * @brief Apply a quality preset
     */
    void ApplyPreset(QualityPreset preset);

    /**
     * @brief Get preset settings
     */
    static CompleteSettings GetPresetSettings(QualityPreset preset);

    /**
     * @brief Validate current settings
     */
    ValidationResult Validate() const;

    /**
     * @brief Get current settings
     */
    const CompleteSettings& GetSettings() const { return m_settings; }
    CompleteSettings& GetSettings() { return m_settings; }

    /**
     * @brief Get individual setting groups
     */
    const RenderingSettings& GetRenderingSettings() const { return m_settings.rendering; }
    const LightingSettings& GetLightingSettings() const { return m_settings.lighting; }
    const MaterialSettings& GetMaterialSettings() const { return m_settings.materials; }
    const LODSettings& GetLODSettings() const { return m_settings.lod; }
    const CachingSettings& GetCachingSettings() const { return m_settings.caching; }
    const PerformanceSettings& GetPerformanceSettings() const { return m_settings.performance; }

    /**
     * @brief Set individual setting groups
     */
    void SetRenderingSettings(const RenderingSettings& settings) { m_settings.rendering = settings; }
    void SetLightingSettings(const LightingSettings& settings) { m_settings.lighting = settings; }
    void SetMaterialSettings(const MaterialSettings& settings) { m_settings.materials = settings; }
    void SetLODSettings(const LODSettings& settings) { m_settings.lod = settings; }
    void SetCachingSettings(const CachingSettings& settings) { m_settings.caching = settings; }
    void SetPerformanceSettings(const PerformanceSettings& settings) { m_settings.performance = settings; }

    /**
     * @brief Register change callback
     */
    using ChangeCallback = std::function<void(const CompleteSettings&)>;
    void RegisterChangeCallback(ChangeCallback callback) {
        m_changeCallbacks.push_back(callback);
    }

    /**
     * @brief Notify callbacks of settings change
     */
    void NotifyChanges();

private:
    SettingsManager() = default;
    ~SettingsManager() = default;

    CompleteSettings m_settings;
    std::vector<ChangeCallback> m_changeCallbacks;
};

/**
 * @brief Convert enum to string
 */
const char* QualityPresetToString(QualityPreset preset);
const char* RenderBackendToString(RenderBackend backend);
const char* GIMethodToString(GIMethod method);
const char* LODQualityToString(LODQuality quality);

} // namespace Nova
